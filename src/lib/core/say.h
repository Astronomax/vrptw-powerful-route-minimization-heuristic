#ifndef TARANTOOL_LIB_CORE_SAY_H_INCLUDED
#define TARANTOOL_LIB_CORE_SAY_H_INCLUDED
/*
 * Copyright 2010-2016, Tarantool AUTHORS, please see AUTHORS file.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <../../../utils.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <errno.h>
#include "small/rlist.h"

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

/** \cond public */
typedef void (*sayfunc_t)(const char *, int, const char *, const char *, ...);

/** Internal function used to implement say() macros */
CFORMAT(printf, 4, 5) extern sayfunc_t _say;

/**
 * Internal function that implements MT-Safe strerror().
 * It is used by say_syserror() macro.
 */
const char *
_say_strerror(int errnum);

/**
 * \param file (const char * ) - file name to print
 * \param line (int) - line number to print
 * \param error (const char * ) - error description, may be NULL
 * \param format (const char * ) - printf()-like format string
 * \param ... - format arguments
 */
#define say_file_line(file, line, error, format, ...) ({ \
	_say(file, line, error, format, ##__VA_ARGS__); })

/**
 * \param error (const char * ) - error description, may be NULL
 * \param format (const char * ) - printf()-like format string
 * \param ... - format arguments
 */
#define say(error, format, ...) ({ \
	say_file_line(__FILE__, __LINE__, error, format, ##__VA_ARGS__); })

/**
 * \param format (const char * ) - printf()-like format string
 * \param ... - format arguments
 * \sa printf()
 * \sa enum say_level
 * Example:
 * \code
 * say_info("Some useful information: %s", status);
 * \endcode
 */
#define say_error(format, ...) say(NULL, format, ##__VA_ARGS__)
/** \copydoc say_error() */
#define say_crit(format, ...) say(NULL, format, ##__VA_ARGS__)
/** \copydoc say_error() */
#define say_warn(format, ...) say(NULL, format, ##__VA_ARGS__)
/** \copydoc say_error() */
#define say_info(format, ...) say(NULL, format, ##__VA_ARGS__)
/** \copydoc say_error() */
#define say_verbose(format, ...) say(NULL, format, ##__VA_ARGS__)
/** \copydoc say_error() */
#define say_debug(format, ...) say(NULL, format, ##__VA_ARGS__)
/** \copydoc say_error(). */
#define say_syserror(format, ...) say(_say_strerror(errno), \
				      format, ##__VA_ARGS__)
/** \endcond public */

#define panic_status(status, ...) ({\
	say(NULL, __VA_ARGS__); \
	exit(status); \
})
#define panic(...)			panic_status(EXIT_FAILURE, __VA_ARGS__)

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */

#endif /* TARANTOOL_LIB_CORE_SAY_H_INCLUDED */
