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

#include <fmt/format.h>
#include <ftdi.hpp>
#include <device.hh>
#include <gpio.hh>
#include <gtkmm.h>

Gpio::Gpio(const Device &device)
{
	/* set all the GPIO to input - clear all the bits */
	io_state = 0x00;

	m_context.set_interface(INTERFACE_D);

	if (m_context.open(device.vid, device.pid, device.description,
	    device.serial) != 0) {
		throw std::runtime_error(fmt::format(
		    "Failed to open device: {}",
		    m_context.error_string()));
	}

	configure();
}

Gpio::~Gpio()
{
	m_context.close();
}

uint8_t
Gpio::get()
{
	uint8_t rd;

	m_context.read(&rd, 1);
	return (rd);
}

void
Gpio::set(uint8_t mask)
{
	m_context.write(&mask, 1);
}


void
Gpio::configure()
{
	if (m_context.set_bitmode(0xff, BITMODE_RESET) != 0)
		throw std::runtime_error("Failed to set bitmode");

	if (m_context.set_bitmode(io_state, BITMODE_BITBANG) != 0)
		throw std::runtime_error("Failed to set bitmode");
}

