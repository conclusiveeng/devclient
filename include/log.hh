/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Conclusive Engineering
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef DEVCLIENT_LOG_HH
#define DEVCLIENT_LOG_HH

#include <fmt/format.h>

class Logger
{
public:
	template <typename... Args>
	static void debug(const char *fmt, const Args &... args)
	{
		Logger::log("DEBUG", fmt, fmt::make_format_args(args...));
	}

	template <typename... Args>
	static void info(const char *fmt, const Args &... args)
	{
		Logger::log("INFO", fmt, fmt::make_format_args(args...));
	}

	template <typename... Args>
	static void warning(const char *fmt, const Args &... args)
	{
		Logger::log("WARNING", fmt, fmt::make_format_args(args...));
	}

	template <typename... Args>
	static void error(const char *fmt, const Args &... args)
	{
		Logger::log("ERROR", fmt, fmt::make_format_args(args...));
	}

	static void log(const char *level, const char *fmt,
	    fmt::format_args args);
};

#endif //DEVCLIENT_LOG_HH
