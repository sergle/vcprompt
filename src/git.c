/*
 * Copyright (C) 2009, 2010, Gregory P. Ward and contributors.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "git.h"

/* Build a path in buf with printf() semantics.  Returns 0 (and complains in
   debug mode) if the result would not fit, since a silently truncated path
   just turns into a confusing "not a git repo" further down. */
static int
build_path(char* buf, int size, char* fmt, ...)
{
    va_list args;
    int n;

    va_start(args, fmt);
    n = vsnprintf(buf, size, fmt, args);
    va_end(args);

    if (n < 0 || n >= size) {
        debug("path too long (max %d chars): '%s...'", size - 1, buf);
        return 0;
    }
    return 1;
}

/* Trim trailing whitespace in place: .git pointer files written by Windows
   tooling end with CRLF, and read_first_line() only chops the LF. */
static void
chop_trailing_space(char* buf)
{
    int len = strlen(buf);
    while (len > 0 && isspace((unsigned char) buf[len-1]))
        buf[--len] = '\0';
}

/* Resolve the git directory into buf.  In an ordinary checkout that is
   ".git", but a linked worktree (and a submodule) has .git as a *file*
   holding a line "gitdir: /path/to/repo/.git/worktrees/<name>".  Returns 0
   if this is not a git repository at all -- including when the pointer is
   stale, so that the caller keeps looking in the parent directories. */
static int
git_dir(char* buf, int size)
{
    char line[1024];
    char* prefix = "gitdir: ";
    int prefixlen = strlen(prefix);
    char* gitdir;

    if (isdir(".git")) {
        snprintf(buf, size, ".git");
        return 1;
    }
    if (!isfile(".git") || !read_first_line(".git", line, sizeof(line)))
        return 0;
    if (strncmp(prefix, line, prefixlen) != 0) {
        debug(".git is a file but has no gitdir line: not a git repo");
        return 0;
    }

    gitdir = line + prefixlen;
    chop_trailing_space(gitdir);
    if (gitdir[0] == '\0') {
        debug(".git has an empty gitdir line: not a git repo");
        return 0;
    }
    if (!isdir(gitdir)) {
        debug("gitdir '%s' does not exist: not a git repo", gitdir);
        return 0;
    }
    if (!build_path(buf, size, "%s", gitdir)) {
        debug("gitdir path too long: not a git repo");
        return 0;
    }
    debug(".git points at gitdir '%s'", buf);
    return 1;
}

/* Branch refs live in the main repository, which a worktree's gitdir points
   at through its "commondir" file.  Falls back to the gitdir itself. */
static int
git_common_dir(char* gitdir, char* buf, int size)
{
    char filename[1024];
    char common[1024];

    /* An ordinary checkout is its own common dir; only a gitdir reached
       through a .git pointer file can have a commondir file. */
    if (strcmp(gitdir, ".git") != 0
        && build_path(filename, sizeof(filename), "%s/commondir", gitdir)
        && read_first_line(filename, common, sizeof(common))) {
        chop_trailing_space(common);
        if (common[0] == '/')
            return build_path(buf, size, "%s", common);
        if (common[0] != '\0')
            return build_path(buf, size, "%s/%s", gitdir, common);
    }
    return build_path(buf, size, "%s", gitdir);
}

static int
git_probe(vccontext_t* context)
{
    char gitdir[1024];

    /* Resolve rather than just stat(): a stray or stale .git file must not
       stop the search, or we would claim the directory and then print
       nothing at all. */
    return git_dir(gitdir, sizeof(gitdir));
}

static result_t*
git_get_info(vccontext_t* context)
{
    result_t* result = init_result();
    char buf[1024];
    char gitdir[1024];
    char headfile[1024];

    if (!git_dir(gitdir, sizeof(gitdir))) {
        debug("unable to locate the git directory: assuming not a git repo");
        return NULL;
    }

    if (!build_path(headfile, sizeof(headfile), "%s/HEAD", gitdir))
        return NULL;
    if (!read_first_line(headfile, buf, 1024)) {
        debug("unable to read %s: assuming not a git repo", headfile);
        return NULL;
    }
    else {
        char* prefix = "ref: refs/heads/";
        int prefixlen = strlen(prefix);

        if (context->options->show_branch || context->options->show_revision) {
            int found_branch = 0;
            if (strncmp(prefix, buf, prefixlen) == 0) {
                /* yep, we're on a known branch */
                debug("read a head ref from %s: '%s'", headfile, buf);
                if (result_set_branch(result, buf + prefixlen))
                    found_branch = 1;
            }
            else {
                /* if it's not a branch name, assume it is a commit ID */
                debug("%s doesn't look like a head ref: unknown branch",
                      headfile);
                result_set_branch(result, "(unknown)");
                result_set_revision(result, buf, 12);
            }
            if (context->options->show_revision && found_branch) {
                char buf[1024];
                char commondir[1024];
                char filename[1024];

                if (git_common_dir(gitdir, commondir, sizeof(commondir))
                    && build_path(filename, sizeof(filename),
                                  "%s/refs/heads/%s",
                                  commondir, result->branch)
                    && read_first_line(filename, buf, 1024)) {
                    result_set_revision(result, buf, 12);
                }
            }
        }
        if (context->options->show_modified) {
            int status = system("git diff --no-ext-diff --quiet --exit-code");
            if (WEXITSTATUS(status) == 1)       /* files modified */
                result->modified = 1;
            /* any other outcome (including failure to fork/exec,
               failure to run git, or diff error): assume no
               modifications */
        }
        if (context->options->show_staged) {
          //git diff --name-only --cached
            int status = system("git diff --cached --no-ext-diff --quiet --exit-code");
            if (WEXITSTATUS(status) == 1)       /* files modified */
              result->staged = 1;
            /* any other outcome (including failure to fork/exec,
               failure to run git, or diff error): assume no
               modifications */
        }
        if (context->options->show_unknown) {
            int status = system("test -n \"$(git ls-files --others --exclude-standard)\"");
            if (WEXITSTATUS(status) == 0)
                result->unknown = 1;
            /* again, ignore other errors and assume no unknown files */
        }
    }

    return result;
}

vccontext_t* get_git_context(options_t* options)
{
    return init_context("git", options, git_probe, git_get_info);
}
