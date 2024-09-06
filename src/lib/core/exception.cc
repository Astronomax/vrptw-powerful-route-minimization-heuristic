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
#include "exception.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "fiber.h"
#include "reflection.h"
#include "tt_strerror.h"

extern "C" {

static void
exception_destroy(struct error *e)
{
	delete (Exception *) e;
}

static void
exception_raise(struct error *error)
{
	Exception *e = (Exception *) error;
	e->raise();
}

static void
exception_log(struct error *error)
{
	Exception *e = (Exception *) error;
	e->log();
}

} /* extern "C" */

const struct type_info type_Exception = make_type("Exception", NULL);

void *
Exception::operator new(size_t size)
{
	return xmalloc(size);
}

void
Exception::operator delete(void *ptr)
{
	free(ptr);
}

Exception::~Exception()
{
	assert(refs == 0);
	TRASH((struct error *) this);
}

Exception::Exception(const struct type_info *type_arg, const char *file,
		     unsigned line)
{
	error_create(this, exception_destroy, exception_raise,
		     exception_log, type_arg, file, line);
}

void
Exception::log() const
{
	say_file_line(file, line, errmsg, "%s", type->name);
}

const struct type_info type_SystemError =
	make_type("SystemError", &type_Exception);

SystemError::SystemError(const struct type_info *type,
			 const char *file, unsigned line)
	:Exception(type, file, line)
{
	saved_errno = errno;
}

SystemError::SystemError(const char *file, unsigned line,
			 const char *format, ...)
	: SystemError(&type_SystemError, file, line)
{
	va_list ap;
	va_start(ap, format);
	error_vformat_msg(this, format, ap);
	va_end(ap);
	error_append_msg(this, ": %s", tt_strerror(saved_errno));
}

const struct type_info type_OutOfMemory =
	make_type("OutOfMemory", &type_SystemError);

OutOfMemory::OutOfMemory(const char *file, unsigned line,
			 size_t amount, const char *allocator,
			 const char *object)
	: SystemError(&type_OutOfMemory, file, line)
{
	saved_errno = ENOMEM;
	error_format_msg(this, "Failed to allocate %u bytes in %s for %s",
			 (unsigned) amount, allocator, object);
}

const struct type_info type_TimedOut =
	make_type("TimedOut", &type_SystemError);

TimedOut::TimedOut(const char *file, unsigned line)
	: SystemError(&type_TimedOut, file, line)
{
	saved_errno = ETIMEDOUT;
	error_format_msg(this, "timed out");
}

const struct type_info type_FiberIsCancelled =
	make_type("FiberIsCancelled", &type_Exception);

FiberIsCancelled::FiberIsCancelled(const char *file, unsigned line)
	: Exception(&type_FiberIsCancelled, file, line)
{
	error_format_msg(this, "fiber is cancelled");
}

void
FiberIsCancelled::log() const
{
	say_info(nullptr, "fiber has been cancelled");
	say_info("fiber: exiting");
}

const struct type_info type_IllegalParams =
	make_type("IllegalParams", &type_Exception);

IllegalParams::IllegalParams(const char *file, unsigned line,
				     const char *format, ...)
	: Exception(&type_IllegalParams, file, line)
{
	va_list ap;
	va_start(ap, format);
	error_vformat_msg(this, format, ap);
	va_end(ap);
}

struct error *
BuildOutOfMemory(const char *file, unsigned line,
		 size_t amount, const char *allocator,
		 const char *object)
{
	return new OutOfMemory(file, line, amount, allocator, object);
}

struct error *
BuildTimedOut(const char *file, unsigned line)
{
	return new TimedOut(file, line);
}

struct error *
BuildFiberIsCancelled(const char *file, unsigned line)
{
	return new FiberIsCancelled(file, line);
}

struct error *
BuildIllegalParams(const char *file, unsigned line, const char *format, ...)
{
	IllegalParams *e = new IllegalParams(file, line, "");
	va_list ap;
	va_start(ap, format);
	error_vformat_msg(e, format, ap);
	va_end(ap);
	return e;
}

struct error *
BuildSystemError(const char *file, unsigned line, const char *format, ...)
{
	SystemError *e = new SystemError(file, line, "");
	va_list ap;
	va_start(ap, format);
	error_vformat_msg(e, format, ap);
	va_end(ap);
	error_append_msg(e, ": %s", tt_strerror(e->saved_errno));
	return e;
}