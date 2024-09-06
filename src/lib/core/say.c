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
#include "say.h"
#include "tt_static.h"
#include "tt_strerror.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void
say_default(const char *filename, int line, const char *error, const char *format, ...);

sayfunc_t _say = say_default;

const char *
_say_strerror(int errnum)
{
	return tt_strerror(errnum);
}

/*
 * From pipe(7):
 * POSIX.1 says that write(2)s of less than PIPE_BUF bytes must be atomic:
 * the output data is written to the pipe as a contiguous sequence. Writes
 * of more than PIPE_BUF bytes may be nonatomic: the kernel may interleave
 * the data with data written by other processes. PIPE_BUF is 4k on Linux.
 *
 * Nevertheless, let's ignore the fact that messages can be interleaved in
 * some situations and set SAY_BUF_LEN_MAX to 16k for now.
 */
enum { SAY_BUF_LEN_MAX = 16 * 1024 };
static __thread char say_buf[SAY_BUF_LEN_MAX];

/**
 * Wrapper over write which ensures, that writes not more than buffer size.
 */
static ssize_t
safe_write(int fd, const char *buf, int size)
{
	assert(size >= 0);
	/* Writes at most SAY_BUF_LEN_MAX - 1
	 * (1 byte was taken for 0 byte in vsnprintf).
	 */
	return write(fd, buf, MIN(size, SAY_BUF_LEN_MAX - 1));
}

int
say_format_plain(char *buf, int len, const char *filename, int line, const char *error,
		 const char *format, va_list ap)
{
	int total = 0;

	/* Primitive basename(filename) */
	if (filename) {
		for (const char *f = filename; *f; f++)
			if (*f == '/' && *(f + 1) != '\0')
				filename = f + 1;
		SNPRINT(total, snprintf, buf, len, " %s:%i", filename,
			line);
	}

	SNPRINT(total, vsnprintf, buf, len, format, ap);
	if (error != NULL)
		SNPRINT(total, snprintf, buf, len, ": %s", error);

	SNPRINT(total, snprintf, buf, len, "\n");
	return total;
}

/** Wrapper around log->format_func to be used with SNPRINT. */
static int
format_func_adapter(char *buf, int len, const char *filename, int line,
		    const char *error, const char *format, va_list ap)
{
	return say_format_plain(buf, len, filename, line, error, format, ap);
}

/**
 * Format a line of log.
 */
static int
format_log_entry(const char *filename, int line, const char *error,
		 const char *format, va_list ap)
{
	int total = 0;
	char *buf = say_buf;
	int len = SAY_BUF_LEN_MAX;

	SNPRINT(total, format_func_adapter, buf, len, filename, line, error,
		format, ap);

	return total;
}

/**
 * Default say function.
 */
static void
say_default(const char *filename, int line, const char *error, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);

	int errsv = errno;

	int total = format_log_entry(filename, line, error, format, ap);
	if (total > 0) {
		ssize_t r = safe_write(STDERR_FILENO, say_buf, total);
		(void) r;                       /* silence gcc warning */
	}
	errno = errsv; /* Preserve the errno. */

	va_end(ap);
}