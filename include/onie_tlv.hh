/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Conclusive Engineering
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
#ifndef DEVCLIENT_ONIE_TLV_H
#define DEVCLIENT_ONIE_TLV_H

#include <stdint.h>
#include <string>
#include <vector>
#include <map>

/*
 * Here comes structure of the ONIE TLV EEPROM format
 * For more details check on following website:
 *  https://opencomputeproject.github.io/onie/design-spec/hw_requirements.html#board-eeprom-information-format
 *
 * Current implementation states as for 2021.02
 * */
struct __attribute__ ((__packed__)) tlv_header_raw {
	char        signature[8];       /* TlvInfo */
	uint8_t     version;            /* Version */
	uint16_t    total_length;       /* Length of all data */
};

struct __attribute__ ((__packed__)) tlv_record_raw {
	uint8_t      type;
	uint8_t      length;
	uint8_t      value[0];
};

struct TLVRecord {
	uint8_t type;
	std::string data;
	size_t data_length;
};

#define TLV_EEPROM_ID_STRING    "TlvInfo"
#define TLV_EEPROM_VERSION      0x1
#define TLV_EEPROM_MAX_SIZE     2048
#define TLV_EEPROM_LEN_MAX      (TLV_EEPROM_MAX_SIZE - sizeof(struct tlv_header_raw))
#define TLV_EEPROM_LEN_CRC      (sizeof (tlv_record_raw) + 4)
#define TLV_EEPROM_VALUE_MAX_SIZE   255

/*
 * TLV types
 * List of TLV code identifiers with length.
 */
typedef enum TLVCode {
	TLV_CODE_RESERVED = 0x00,           /* None */
	TLV_CODE_PRODUCT_NAME = 0x21,       /* Variable */
	TLV_CODE_PART_NUMBER = 0x22,        /* Variable */
	TLV_CODE_SERIAL_NUMBER = 0x23,      /* Variable */
	TLV_CODE_MAC_BASE = 0x24,           /* 6 bytes */
	TLV_CODE_MANUF_DATE = 0x25,         /* 19 bytes */
	TLV_CODE_DEV_VERSION = 0x26,        /* 1 byte */
	TLV_CODE_LABEL_REVISION = 0x27,     /* Variable */
	TLV_CODE_PLATFORM_NAME = 0x28,      /* Variable */
	TLV_CODE_ONIE_VERSION  = 0x29,      /* Variable */
	TLV_CODE_NUM_MACs = 0x2A,           /* 2 bytes */
	TLV_CODE_MANUF_NAME = 0x2B,         /* Variable */
	TLV_CODE_COUNTRY_CODE = 0x2C,       /* 2 bytes */
	TLV_CODE_VENDOR_NAME =  0x2D,       /* Variable */
	TLV_CODE_DIAG_VERSION = 0x2E,       /* Variable */
	TLV_CODE_SERVICE_TAG = 0x2F,        /* Variable */
	TLV_CODE_VENDOR_EXT = 0xFD,         /* Variable */
	TLV_CODE_CRC_32 = 0xFE,             /* 4 bytes */
	TLV_CODE_RESERVED_1 = 0xFF,         /* None */
} tlv_code_t;

class OnieTLVException : public std::exception {
	std::string error_msg;
public:
	OnieTLVException(std::string msg) : std::exception(), error_msg(msg)
	{
	}

	const char* what() const throw () {
		return "OnieTLVException";
	}

	std::string get_info() const {
		return error_msg;
	}
};


class OnieTLV {
public:
	OnieTLV();
	~OnieTLV();

	bool save_user_tlv(tlv_code_t tlv_id, std::string value) noexcept(false);
	bool generate_eeprom_file(uint8_t eeprom[2048]);
	bool load_eeprom_file(const uint8_t *eeprom);
	std::optional<std::string> get_tlv_record(const tlv_code_t tlv_id);
	
	size_t get_usage();
	bool load_from_yaml(std::string filename);

	static constexpr tlv_code_t ALL_TLV_ID[] =  {TLV_CODE_PRODUCT_NAME, TLV_CODE_PART_NUMBER, TLV_CODE_SERIAL_NUMBER,
	                                             TLV_CODE_MAC_BASE, TLV_CODE_MANUF_DATE, TLV_CODE_DEV_VERSION,
	                                             TLV_CODE_LABEL_REVISION, TLV_CODE_PLATFORM_NAME,
	                                             TLV_CODE_ONIE_VERSION, TLV_CODE_NUM_MACs, TLV_CODE_MANUF_NAME,
	                                             TLV_CODE_COUNTRY_CODE, TLV_CODE_VENDOR_NAME, TLV_CODE_DIAG_VERSION,
	                                             TLV_CODE_SERVICE_TAG, TLV_CODE_VENDOR_EXT};
private:
	std::vector<TLVRecord> tlv_records;
	std::map<std::string, tlv_code_t> yaml_map;
	uint32_t eeprom_tlv_crc32_generated;
	uint16_t usage;
	std::string board_name;
	std::string revision;

	TLVRecord * find_record_or_nullptr(tlv_code_t tlv_id);
	void update_records(TLVRecord& rec);
	bool is_eeprom_valid(uint32_t crc32);
	int parse_number(std::string text_number, int min, int max) noexcept(false);
	void parse_mac_address(std::string mac_text, uint8_t *mac_address) noexcept(false);
	bool validate_mac_address(std::string mac_address)noexcept(false);
	void validate_date(std::string date_value) noexcept(false);
	void validate_text(std::string text, size_t len) noexcept(false);
};

#endif //DEVCLIENT_ONIE_TLV_H
