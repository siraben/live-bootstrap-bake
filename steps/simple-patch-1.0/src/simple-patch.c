#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "M2libc/bootstrappable.h"

/*
SPDX-FileCopyrightText: 2023 Richard Masters <grick23@gmail.com>
SPDX-License-Identifier: MIT

Simple Patch program.

This program is written in a subset of C called M2, which is from the
stage0-posix bootstrap project.

Example usage:
./simple-patch -p1 -i patch_file

*/

// function prototypes
void read_file_or_die(char *file_name, char **buffer, int *file_size);
void write_file_or_die(char *file_name, char *buffer, int file_size);
void patch_unified_or_die(char *patch_buffer, int patch_size, int strip);
void apply_hunk_or_die(char **file_buffer, int *file_size, int *offset,
                 char *before_buffer, int before_size,
                 char *after_buffer, int after_size);
void writestr_fd(int fd, char *str);
void usage(void);
int memsame(char *search_buffer, int search_size,
            char *pattern_buffer, int pattern_size);
int starts_with(char *buffer, int size, char *pattern);
int line_size(char *pos, char *end);
int next_line_starts_with(char *line, int size, char *end, char *pattern);
char *copy_patch_path(char *line, int size, int strip);
int parse_strip(char *arg);
int parse_hunk_header(char *line, int size, int *old_start, int *old_count, int *new_count);
int parse_count(char *line, int size, int *pos);
int line_offset(char *buffer, int size, int line_number);
int remove_trailing_newline(char *buffer, int size);


int main(int argc, char **argv) {
    int strip;
    char *patch_name;
    char *patch_buffer;
    int patch_size;
    int i;

    strip = 0;
    patch_name = NULL;
    i = 1;
    while(i < argc) {
        if(strcmp(argv[i], "-i") == 0) {
            i = i + 1;
            if(i >= argc) usage();
            patch_name = argv[i];
        } else if(strcmp(argv[i], "-N") == 0) {
            /* Already-applied patch detection is not needed for bootstrap use. */
        } else if(starts_with(argv[i], strlen(argv[i]), "-p")) {
            strip = parse_strip(argv[i] + 2);
        } else if(starts_with(argv[i], strlen(argv[i]), "-Np")) {
            strip = parse_strip(argv[i] + 3);
        } else {
            patch_name = argv[i];
        }
        i = i + 1;
    }

    if(patch_name == NULL) usage();

    read_file_or_die(patch_name, &patch_buffer, &patch_size);
    patch_unified_or_die(patch_buffer, patch_size, strip);

    return EXIT_SUCCESS;
}


void read_file_or_die(char *file_name, char **buffer, int *file_size) {
    int file_fd;
    int num_bytes_read;

    file_fd = open(file_name, O_RDONLY, 0);
    if (file_fd == -1) {
        writestr_fd(2, "Could not open file: ");
        writestr_fd(2, file_name);
        writestr_fd(2, "\n");
        exit(1);
    }
    // determine file size
    *file_size = lseek(file_fd, 0, SEEK_END);
    // go back to beginning of file
    lseek(file_fd, 0, SEEK_SET);
    // alloc a buffer to read the entire file
    *buffer = calloc(*file_size + 1, sizeof(char));

    // read the entire patch file
    num_bytes_read = read(file_fd, *buffer, *file_size);
    if (num_bytes_read != *file_size) {
        writestr_fd(2, "Could not read file: ");
        writestr_fd(2, file_name);
        writestr_fd(2, "\n");
        exit(1);
    }
    close(file_fd);
}

void write_file_or_die(char *file_name, char *buffer, int file_size) {
    int file_fd;
    int num_bytes_written;

    file_fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, 0);
    if (file_fd == -1) {
        writestr_fd(2, "Could not write file: ");
        writestr_fd(2, file_name);
        writestr_fd(2, "\n");
        exit(1);
    }
    num_bytes_written = write(file_fd, buffer, file_size);
    if (num_bytes_written != file_size) {
        writestr_fd(2, "Could not write file: ");
        writestr_fd(2, file_name);
        writestr_fd(2, "\n");
        exit(1);
    }
    close(file_fd);
}

void patch_unified_or_die(char *patch_buffer, int patch_size, int strip) {
    char *pos;
    char *end;
    int size;
    char *file_name;
    char *file_buffer;
    int file_size;
    int file_offset;
    int file_dirty;
    char *before_buffer;
    char *after_buffer;
    int before_size;
    int after_size;
    int old_start;
    int old_count;
    int old_seen;
    int new_count;
    int new_seen;
    int line_delta;
    char prefix;
    char previous_prefix;
    int content_size;

    pos = patch_buffer;
    end = patch_buffer + patch_size;
    file_name = NULL;
    file_buffer = NULL;
    file_size = 0;
    file_offset = 0;
    line_delta = 0;
    file_dirty = FALSE;

    while(pos < end) {
        size = line_size(pos, end);

        if(starts_with(pos, size, "--- ")) {
            if(file_dirty) {
                write_file_or_die(file_name, file_buffer, file_size);
                file_dirty = FALSE;
            }

            pos = pos + size;
            if(pos >= end) {
                writestr_fd(2, "Missing +++ line\n");
                exit(1);
            }
            size = line_size(pos, end);
            if(!starts_with(pos, size, "+++ ")) {
                writestr_fd(2, "Missing +++ line\n");
                exit(1);
            }
            file_name = copy_patch_path(pos + 4, size - 4, strip);
            read_file_or_die(file_name, &file_buffer, &file_size);
            file_offset = 0;
            line_delta = 0;
            pos = pos + size;
        } else if(starts_with(pos, size, "@@ ")) {
            if(file_name == NULL) {
                writestr_fd(2, "Patch hunk has no file header\n");
                exit(1);
            }

            if(!parse_hunk_header(pos, size, &old_start, &old_count, &new_count)) {
                writestr_fd(2, "Malformed patch hunk header\n");
                exit(1);
            }
            file_offset = line_offset(file_buffer, file_size, old_start + line_delta);
            pos = pos + size;
            before_buffer = calloc(patch_size + 1, sizeof(char));
            after_buffer = calloc(patch_size + 1, sizeof(char));
            before_size = 0;
            after_size = 0;
            old_seen = 0;
            new_seen = 0;
            previous_prefix = 0;

            while(pos < end) {
                size = line_size(pos, end);
                if(starts_with(pos, size, "@@ ")) {
                    break;
                }
                if(starts_with(pos, size, "diff ")) {
                    break;
                }
                if(starts_with(pos, size, "--- ") &&
                   next_line_starts_with(pos, size, end, "+++ ")) {
                    break;
                }
                if(starts_with(pos, size, "\\ No newline")) {
                    if(previous_prefix == ' ') {
                        before_size = remove_trailing_newline(before_buffer, before_size);
                        after_size = remove_trailing_newline(after_buffer, after_size);
                    } else if(previous_prefix == '-') {
                        before_size = remove_trailing_newline(before_buffer, before_size);
                    } else if(previous_prefix == '+') {
                        after_size = remove_trailing_newline(after_buffer, after_size);
                    } else {
                        writestr_fd(2, "No newline marker has no previous hunk line\n");
                        exit(1);
                    }
                    pos = pos + size;
                    continue;
                }

                prefix = pos[0];
                content_size = size - 1;
                if(prefix == ' ') {
                    memcpy(before_buffer + before_size, pos + 1, content_size);
                    memcpy(after_buffer + after_size, pos + 1, content_size);
                    before_size = before_size + content_size;
                    after_size = after_size + content_size;
                    old_seen = old_seen + 1;
                    new_seen = new_seen + 1;
                } else if(prefix == '-') {
                    memcpy(before_buffer + before_size, pos + 1, content_size);
                    before_size = before_size + content_size;
                    old_seen = old_seen + 1;
                } else if(prefix == '+') {
                    memcpy(after_buffer + after_size, pos + 1, content_size);
                    after_size = after_size + content_size;
                    new_seen = new_seen + 1;
                } else {
                    writestr_fd(2, "Unsupported patch hunk line\n");
                    exit(1);
                }
                previous_prefix = prefix;
                pos = pos + size;
            }

            if((old_seen != old_count) || (new_seen != new_count)) {
                writestr_fd(2, "Patch hunk line count mismatch\n");
                exit(1);
            }

            apply_hunk_or_die(&file_buffer, &file_size, &file_offset,
                         before_buffer, before_size,
                         after_buffer, after_size);
            line_delta = line_delta + new_count - old_count;
            file_dirty = TRUE;
        } else {
            pos = pos + size;
        }
    }

    if(file_dirty) {
        write_file_or_die(file_name, file_buffer, file_size);
    }
}

void apply_hunk_or_die(char **file_buffer, int *file_size, int *offset,
                 char *before_buffer, int before_size,
                 char *after_buffer, int after_size) {
    int pos;
    int new_size;
    char *new_buffer;

    pos = *offset;
    if(!memsame(*file_buffer + pos, *file_size - pos,
                before_buffer, before_size)) {
        writestr_fd(2, "Patch hunk did not match\n");
        exit(1);
    }

    new_size = *file_size - before_size + after_size;
    new_buffer = calloc(new_size + 1, sizeof(char));
    memcpy(new_buffer, *file_buffer, pos);
    memcpy(new_buffer + pos, after_buffer, after_size);
    memcpy(new_buffer + pos + after_size,
           *file_buffer + pos + before_size,
           *file_size - pos - before_size);

    *file_buffer = new_buffer;
    *file_size = new_size;
    *offset = pos + after_size;
}

/*
    Write the string to the given file descriptor.
*/
void writestr_fd(int fd, char *str) {
    write(fd, str, strlen(str));
}

void usage(void) {
    writestr_fd(2, "Usage: simple-patch [-pN] [-N] -i patch\n");
    exit(1);
}

/*
    Is the pattern located at the start of the search buffer
    (and not exceeding the length of the search buffer)?
*/

int memsame(char *search_buffer, int search_size,
            char *pattern_buffer, int pattern_size) {
    int check_offset = 0;

    if (pattern_size > search_size) {
        return FALSE;
    }
    while (check_offset < pattern_size) {
        if (search_buffer[check_offset] != pattern_buffer[check_offset]) {
             return FALSE;
        }
        check_offset = check_offset + 1;
    }
    return TRUE;
}

int starts_with(char *buffer, int size, char *pattern) {
    return memsame(buffer, size, pattern, strlen(pattern));
}

int line_size(char *pos, char *end) {
    int size;

    size = 0;
    while(pos + size < end) {
        size = size + 1;
        if(pos[size - 1] == '\n') {
            return size;
        }
    }
    return size;
}

int next_line_starts_with(char *line, int size, char *end, char *pattern) {
    char *next;
    int next_size;

    next = line + size;
    if(next >= end) {
        return FALSE;
    }
    next_size = line_size(next, end);
    return starts_with(next, next_size, pattern);
}

char *copy_patch_path(char *line, int size, int strip) {
    int start;
    int stop;
    int components;
    int out_size;
    char *path;

    start = 0;
    while(start < size &&
          (line[start] == ' ' || line[start] == '\t')) {
        start = start + 1;
    }

    components = 0;
    while(start < size && components < strip) {
        if(line[start] == '/') {
            components = components + 1;
        }
        start = start + 1;
    }

    stop = start;
    while(stop < size &&
          line[stop] != ' ' &&
          line[stop] != '\t' &&
          line[stop] != '\n') {
        stop = stop + 1;
    }

    out_size = stop - start;
    if(out_size <= 0) {
        writestr_fd(2, "Empty patch path\n");
        exit(1);
    }
    if((9 == out_size) && memsame(line + start, out_size, "/dev/null", 9)) {
        writestr_fd(2, "Creating or deleting files is not supported\n");
        exit(1);
    }
    path = calloc(out_size + 1, sizeof(char));
    memcpy(path, line + start, out_size);
    return path;
}

int parse_strip(char *arg) {
    int i;
    if(arg[0] == 0) {
        return 0;
    }
    i = 0;
    while(arg[i] != 0) {
        if(arg[i] < '0' || arg[i] > '9') {
            usage();
        }
        i = i + 1;
    }
    return atoi(arg);
}

int parse_count(char *line, int size, int *pos) {
    int count;
    count = 0;
    if(*pos >= size || line[*pos] < '0' || line[*pos] > '9') {
        return -1;
    }
    while(*pos < size && line[*pos] >= '0' && line[*pos] <= '9') {
        count = (count * 10) + line[*pos] - '0';
        *pos = *pos + 1;
    }
    return count;
}

int parse_hunk_header(char *line, int size, int *old_start, int *old_count, int *new_count) {
    int pos;
    int new_start;
    if(!starts_with(line, size, "@@ -")) {
        return FALSE;
    }
    pos = 4;
    *old_start = parse_count(line, size, &pos);
    if(*old_start < 0) return FALSE;
    *old_count = 1;
    if(pos < size && line[pos] == ',') {
        pos = pos + 1;
        *old_count = parse_count(line, size, &pos);
        if(*old_count < 0) return FALSE;
    }
    if(pos >= size || line[pos] != ' ') return FALSE;
    pos = pos + 1;
    if(pos >= size || line[pos] != '+') return FALSE;
    pos = pos + 1;
    new_start = parse_count(line, size, &pos);
    if(new_start < 0) return FALSE;
    *new_count = 1;
    if(pos < size && line[pos] == ',') {
        pos = pos + 1;
        *new_count = parse_count(line, size, &pos);
        if(*new_count < 0) return FALSE;
    }
    if(pos >= size || line[pos] != ' ') return FALSE;
    pos = pos + 1;
    if(pos >= size || line[pos] != '@') return FALSE;
    pos = pos + 1;
    if(pos >= size || line[pos] != '@') return FALSE;
    pos = pos + 1;
    if(pos < size && line[pos] != ' ' && line[pos] != '\t' &&
       line[pos] != '\n' && line[pos] != '\r') return FALSE;
    return TRUE;
}

int line_offset(char *buffer, int size, int line_number) {
    int pos;
    int line;
    if(line_number <= 1) {
        return 0;
    }
    pos = 0;
    line = 1;
    while(pos < size && line < line_number) {
        if(buffer[pos] == '\n') {
            line = line + 1;
        }
        pos = pos + 1;
    }
    return pos;
}

int remove_trailing_newline(char *buffer, int size) {
    if(size <= 0) {
        return size;
    }
    if(buffer[size - 1] == '\n') {
        buffer[size - 1] = 0;
        return size - 1;
    }
    return size;
}
