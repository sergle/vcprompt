/*
 * Copyright (C) 2009, 2010, Gregory P. Ward and contributors.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef GIT_H
#define GIT_H

#include "common.h"

/* The name reported by the "%n" format specifier. */
#define GIT_NAME "git"

/* Return 1 if the current directory is the top of a git working copy,
 * 0 otherwise.  Cheap: it only reads .git.
 */
int git_probe(void);

/* Analyze the git working copy in the current directory and return what
 * the requested format specifiers need.  Returns NULL if this turns out
 * not to be a usable git repository after all.  Caller owns the result.
 */
result_t* git_get_info(options_t* options);

#endif
