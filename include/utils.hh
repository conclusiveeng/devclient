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

#ifndef DEVCLIENT_UTILS_HH
#define DEVCLIENT_UTILS_HH

#include <string>
#include <iostream>
#include <fstream>
#include <glibmm.h>
#include <gtkmm.h>
#include <fmt/format.h>

template <>
struct fmt::formatter<Glib::ustring>
{
	template <typename ParseContext>
	constexpr auto parse(ParseContext &ctx)
	{
		return ctx.begin();
	}
	
	template <typename FormatContext>
	auto format(const Glib::ustring &s, FormatContext &ctx)
	{
		return format_to(ctx.out(), "{}", s.c_str());
	}
};

void show_centered_dialog(const std::string &title, const std::string &secondary = "");

template<class Elem, class Traits>
void hex_dump(
	const void *aData,
	std::size_t aLength,
    	std::basic_ostream<Elem, Traits> &stream,
    	std::size_t width = 16)
{
	const char* const start = static_cast<const char*>(aData);
	const char* const end = start + aLength;
	const char* line = start;
	
	while (line != end)
	{
		stream.width(4);
		stream.fill('0');
		stream << std::hex << line - start << " ";
		std::size_t lineLength = std::min(
			width,
			static_cast<std::size_t>(end - line));
		for (std::size_t pass = 1; pass <= 2; ++pass)
		{
			for (
				const char *next = line;
				next != end && next != line + width;
				++next
				){
				char ch = *next;
				switch (pass)
				{
					case 1:
						stream << (ch < 32 ? '.' : ch);
						break;
					case 2:
						if (next != line)
							stream << " ";
						
						stream.width(2);
						stream.fill('0');
						stream << std::hex <<
						       std::uppercase <<
						       static_cast<int>(static_cast<unsigned char>(ch));
						break;
				}
			}
			if (pass == 1 && lineLength != width)
				stream << std::string(width - lineLength, ' ');
			
			stream << " ";
		}
		
		stream << std::endl;
		line = line + lineLength;
	}
}

std::string executable_dir();

#endif //DEVCLIENT_UTILS_HH
