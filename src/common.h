/*
 * Copyright (C) 2009, 2010, Gregory P. Ward and contributors.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef VCPROMPT_H
#define VCPROMPT_H

/* What the user asked for (environment + command-line).
 */
typedef struct {
    int debug;
    char* format;                       /* e.g. "[%b%u%m]" */
    int show_branch;                    /* show current branch? */
    int show_revision;                  /* show current revision? */
    int show_unknown;                   /* show ? if unknown files? */
    int show_modified;                  /* show ! if local changes? */
    int show_staged;                    /* show * if local staged changes? */
} options_t;

/* What we figured out by analyzing the working dir: info that
 * will be printed to stdout for the shell to incorporate into
 * the user's prompt.
 */
typedef struct {
    char* branch;                       /* name of current branch */
    char* revision;                     /* current revision */
    int unknown;                        /* any unknown files? */
    int modified;                       /* any local changes? */
    int staged;                         /* any local staged changes? */
} result_t;

int result_set_revision(result_t* result, const char *revision, int len);
int result_set_branch(result_t* result, const char *branch);

void
set_options(options_t*);

result_t*
init_result();

void
free_result(result_t*);

/* printf()-style output of fmt and other args to stdout, but only if
 * debug mode is on (e.g. from the command line -d).
 */
void
debug(char* fmt, ...);

/* stat() the specified file and return true if it is a directory, false
 * if stat() failed or it is not a directory.
 */
int
isdir(char* name);

/* stat() the specified file and return true if it is a regular file,
 * false if stat() failed or it is not a regular file.
 */
int
isfile(char* name);

/* Open the specified file, read the first line (up to size-1 chars) to
 * buf, and close the file.  buf will not contain a newline.  Caller
 * must allocate at least size chars for buf.  Return 1 on successful
 * read, 0 on any errors.  Error messages will be written with debug(),
 * i.e. only visible if running in debug mode.
 */
int
read_first_line(char* filename, char* buf, int size);

#endif
