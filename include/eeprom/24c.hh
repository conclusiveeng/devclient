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

#ifndef DEVCLIENT_24C_HH
#define DEVCLIENT_24C_HH

#include <vector>
#include <log.hh>
#include <eeprom.hh>

#define EEPROM_24C_ADDRESS_RD	0xa1
#define EEPROM_24C_ADDRESS_WR	0xa0

class Eeprom24c: public Eeprom
{
public:
	using Eeprom::Eeprom;

	void read(uint16_t offset, size_t length, std::vector<uint8_t> &data)
	{
		m_i2c.start();
		m_i2c.write({
		    EEPROM_24C_ADDRESS_WR,
		    static_cast<unsigned char>((offset >> 8) & 0xff),
		    static_cast<unsigned char>(offset & 0xff)
		});

		m_i2c.start();
		m_i2c.write({ EEPROM_24C_ADDRESS_RD });
		m_i2c.read(length, data);
		m_i2c.stop();
	}

	void write(uint16_t offset, const std::vector<uint8_t> &data)
	{
		std::vector<uint8_t> slice;
		std::vector<uint8_t>::size_type i;

		for (i = 0; i < data.size(); i += 32) {
			Logger::debug("Writing to AT24C at offset {}", offset);
			m_i2c.start();
			m_i2c.write({
			    EEPROM_24C_ADDRESS_WR,
			    static_cast<unsigned char>((offset >> 8) & 0xff),
			    static_cast<unsigned char>(offset & 0xff)
			});

			slice = std::vector<uint8_t>(data.begin() + i, data.begin() + i + 32);
			m_i2c.write(slice);
			m_i2c.stop();
			offset += 32;
			Glib::usleep(50000);
		}
	}

	void erase()
	{

	}
};

#endif /* DEVCLIENT_24C_HH */
