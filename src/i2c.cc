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

#include <ftdi.hpp>
#include <log.hh>
#include <device.hh>
#include <i2c.hh>

I2C::I2C(const Device &device, int clock)
{
	const uint8_t sync[] = { 0xaa };
	uint8_t rd[2];
	const uint8_t cmd[] = {
	    DIS_DIV_5,
	    DIS_ADAPTIVE,
	    DIS_3_PHASE,
	    SET_BITS_LOW, SDA_OUT | SCL, OUT_PINS,
	    TCK_DIVISOR, 0xc8, 0x00,
	};
	const uint8_t cmd2[] = {
	    LOOPBACK_END,
	    /* Tristate SDA and SCL pins */
	    SET_BITS_LOW, 0, WP
	};

	m_context.set_interface(INTERFACE_A);

	if (m_context.open(device.vid, device.pid, device.description,
	    device.serial) != 0) {
		throw std::runtime_error(fmt::format(
		    "Failed to open device: {}",
		    m_context.error_string()));
	}

	if (m_context.set_bitmode(0xff, BITMODE_RESET) != 0)
		throw std::runtime_error("Failed to set bitmode");

	if (m_context.set_bitmode(0xff, BITMODE_MPSSE) != 0)
		throw std::runtime_error("Failed to set bitmode");

	m_context.write(sync, sizeof(sync));

	for (;;) {
		if (m_context.read(rd, sizeof(rd)) != sizeof(rd))
			throw std::runtime_error("Failed to synchronize");

		if (rd[0] == 0xfa && rd[1] == 0xaa)
			break;
	}

	m_context.write(cmd, sizeof(cmd));
	m_context.write(cmd2, sizeof(cmd2));
}

I2C::I2C(const Ftdi::Context &context, int clock)
{
	const uint8_t sync[] = { 0xaa };
	uint8_t rd[2];
	const uint8_t cmd[] = {
	    DIS_DIV_5,
	    DIS_ADAPTIVE,
	    DIS_3_PHASE,
	    SET_BITS_LOW, SDA_OUT | SCL, OUT_PINS,
	    TCK_DIVISOR, 0xc8, 0x00,
	};
	const uint8_t cmd2[] = {
	    LOOPBACK_END,
	    /* Tristate SDA and SCL pins */
	    SET_BITS_LOW, 0, WP
	};

	m_context = context;
	m_context.set_interface(INTERFACE_A);
	// m_context.reset();

	if (m_context.set_bitmode(0xff, BITMODE_RESET) != 0) 
		throw std::runtime_error("Failed to set bitmode");

	if (m_context.set_bitmode(0xff, BITMODE_MPSSE) != 0)
		throw std::runtime_error("Failed to set bitmode");

	m_context.write(sync, sizeof(sync));

	for (;;) {
		if (m_context.read(rd, sizeof(rd)) != sizeof(rd))
			throw std::runtime_error("Failed to synchronize");

		if (rd[0] == 0xfa && rd[1] == 0xaa)
			break;
	}

	m_context.write(cmd, sizeof(cmd));
	m_context.write(cmd2, sizeof(cmd2));
}

I2C::~I2C()
{

}

void
I2C::read(size_t nbytes, std::vector<uint8_t> &result)
{
	size_t i;
	uint8_t rd_data;


	/*
	 * Don't know why this is needed, but we're ending up with one
	 * extra byte in front of the read buffer after read
	 */
	m_context.set_interface(INTERFACE_A);
	m_context.read(&rd_data, 1);

	for (i = 0; i < nbytes; i++)
		result.push_back(read_byte(i != nbytes - 1));
}

void
I2C::write(const std::vector<uint8_t> &data)
{
	for (const auto &i: data)
		write_byte(i);
}

void
I2C::start()
{
	const uint8_t cmd[] = {
	    /* SCL high, SDA high, repeat 4 times */
	    SET_BITS_LOW, SCL | SDA_OUT, OUT_PINS,
	    SET_BITS_LOW, SCL | SDA_OUT, OUT_PINS,
	    SET_BITS_LOW, SCL | SDA_OUT, OUT_PINS,
	    SET_BITS_LOW, SCL | SDA_OUT, OUT_PINS,
	    /* SCL high, SDA low, repeat 4 times */
	    SET_BITS_LOW, SCL, OUT_PINS,
	    SET_BITS_LOW, SCL, OUT_PINS,
	    SET_BITS_LOW, SCL, OUT_PINS,
	    SET_BITS_LOW, SCL, OUT_PINS,
	    /* SCL low, SDA low */
	    SET_BITS_LOW, 0, OUT_PINS,
	    SEND_IMMEDIATE
	};

	Logger::debug("I2C: start");
	m_context.set_interface(INTERFACE_A);
	m_context.write(cmd, sizeof(cmd));
}

void
I2C::stop()
{
	const uint8_t cmd[] = {
	    /* SCL high, SDA low, repeat 4 times */
	    SET_BITS_LOW, SCL, OUT_PINS,
	    SET_BITS_LOW, SCL, OUT_PINS,
	    SET_BITS_LOW, SCL, OUT_PINS,
	    SET_BITS_LOW, SCL, OUT_PINS,
	    /* SCL high, SDA high, repeat 4 times */
	    SET_BITS_LOW, SCL | SDA_OUT, OUT_PINS,
	    SET_BITS_LOW, SCL | SDA_OUT, OUT_PINS,
	    SET_BITS_LOW, SCL | SDA_OUT, OUT_PINS,
	    SET_BITS_LOW, SCL | SDA_OUT, OUT_PINS,
	    /* Tristate SDA and SCL pins */
	    SEND_IMMEDIATE,
	};

	const uint8_t cmd2[] = {
	    SET_BITS_LOW, 0, WP,
	    SEND_IMMEDIATE
	};

	Logger::debug("I2C: stop");
	m_context.set_interface(INTERFACE_A);
	m_context.write(cmd, sizeof(cmd));
	m_context.write(cmd2, sizeof(cmd2));
}

uint8_t
I2C::read_byte(bool ack)
{
	uint8_t rd;
	uint8_t ackbyte = static_cast<uint8_t>(ack ? 0 : 0xff);
	const uint8_t cmd[] = {
	    SET_BITS_LOW, 0, SCL | WP,
	    MPSSE_DO_READ | MPSSE_READ_NEG, 0, 0,
	    SET_BITS_LOW, 0, OUT_PINS,
	    MPSSE_DO_WRITE | MPSSE_WRITE_NEG | MPSSE_BITMODE, 0, ackbyte,
	    SET_BITS_LOW, 0, OUT_PINS,
	    SEND_IMMEDIATE
	};

	m_context.set_interface(INTERFACE_A);
	m_context.write(cmd, sizeof(cmd));
	m_context.read(&rd, 1);
	return (rd);
}

void
I2C::write_byte(uint8_t byte)
{
	uint8_t rd;
	const uint8_t cmd[] = {
	    MPSSE_DO_WRITE | MPSSE_WRITE_NEG, 0, 0, byte,
	    SET_BITS_LOW, 0, SCL | WP,
	    MPSSE_DO_READ | MPSSE_BITMODE, 0,
	    SEND_IMMEDIATE,
	};
	const uint8_t cmd2[] = {
	    SET_BITS_LOW, 0, OUT_PINS
	};

	m_context.set_interface(INTERFACE_A);
	m_context.write(cmd, sizeof(cmd));
	m_context.read(&rd, 1);
	m_context.write(cmd2, sizeof(cmd2));
}
