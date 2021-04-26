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

#include <onie_tlv.hh>
#include <string>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstring>
#include <netinet/in.h>
#include <ctime>
#include <zlib.h>
#include <log.hh>
#include <yaml-cpp/yaml.h>

#define HEADER_SIZE sizeof (struct tlv_header_raw)
#define RECORD_SIZE sizeof (struct tlv_record_raw)

OnieTLV::OnieTLV()
{
	yaml_map["product-name"] = TLV_CODE_PRODUCT_NAME;
	yaml_map["part-number"] = TLV_CODE_PART_NUMBER;
	yaml_map["serial-number"] = TLV_CODE_SERIAL_NUMBER;
	yaml_map["mac-address"] = TLV_CODE_MAC_BASE;
	yaml_map["manufacture-date"] = TLV_CODE_MANUF_DATE;
	yaml_map["device-version"] = TLV_CODE_DEV_VERSION;
	yaml_map["label-revision"] = TLV_CODE_LABEL_REVISION;
	yaml_map["platform-name"] = TLV_CODE_PLATFORM_NAME;
	yaml_map["onie-version"] = TLV_CODE_ONIE_VERSION;
	yaml_map["number-mac"] = TLV_CODE_NUM_MACs;
	yaml_map["manufacturer"] = TLV_CODE_MANUF_NAME;
	yaml_map["country-code"] = TLV_CODE_COUNTRY_CODE;
	yaml_map["vendor-name"] = TLV_CODE_VENDOR_NAME;
	yaml_map["diag-version"] = TLV_CODE_DIAG_VERSION;
	yaml_map["service-tag"] = TLV_CODE_SERVICE_TAG;

	board_name = "Not set";
	revision = "Not set";
};

OnieTLV::~OnieTLV()
{

}

void OnieTLV::validate_date(std::string date_value)
{
	struct tm tm;

	if (date_value.length() != 19)
		throw OnieTLVException("Bad date format. Should be MM/DD/YYYY hh:mm:ss");

	if (!strptime(date_value.c_str(), "%m/%d/%Y %H:%M:%S", &tm))
		throw OnieTLVException("Bad date. Check if date is valid.");
}

void OnieTLV::validate_text(std::string text, size_t len)
{
	if (text.length() > len)
		throw OnieTLVException(fmt::format("Field value cannot be longer than {}", len));
}

bool OnieTLV::validate_mac_address(std::string mac_address)
{
	int ret;
	int mac_bytes[6];

	if (mac_address.length() != 17) {
		Logger::error("Bad MAC address format. Should be xx:xx:xx:xx:xx");
		return false;
	}
	ret = std::sscanf(mac_address.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x",
			&mac_bytes[0], &mac_bytes[1], &mac_bytes[2], &mac_bytes[3], &mac_bytes[4], &mac_bytes[5]);
	if (ret != 6) {
		Logger::error("Bad MAC address format. sscanf failed.");
		return false;
	}
	if ((mac_bytes[0] | mac_bytes[1] | mac_bytes[2] | mac_bytes[3] | mac_bytes[4] | mac_bytes[5]) == 0) {
		Logger::error("MAC address is all 0.");
		return false;  // MAC address cannot be 00:00:00:00:00:00
	}
	if (0x01 & mac_bytes[0]) {
		Logger::error("MAC address is multicast.");
		return false;  // MAC address cannot be multicast
	}
	return true;
}

void OnieTLV::parse_mac_address(std::string mac_text, uint8_t *mac_address)
{
	int mac_bytes[6];

	if (!validate_mac_address(mac_text)) {
		throw OnieTLVException("Invalid MAC address. Required format is: xx:xx:xx:xx:xx.");
	}

	std::sscanf(mac_text.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x",
			&mac_bytes[0], &mac_bytes[1], &mac_bytes[2], &mac_bytes[3], &mac_bytes[4], &mac_bytes[5]);

	for (int i=0; i < 6; i++)
		mac_address[i] = mac_bytes[i];
}

int OnieTLV::parse_number(std::string text_number, int min, int max)
{
	int parsed_number = -1;
	std::string::const_iterator it = text_number.begin();
	while (it != text_number.end() && std::isdigit(*it)) ++it;
	bool is_ok = !text_number.empty() && it == text_number.end();

	if (is_ok) {
		try {
			parsed_number = std::stoi(text_number);
		} catch (std::invalid_argument& invalidArgument) {
			throw OnieTLVException("Cannot convert number. Check if number is positive integer.");
		}
		catch (std::out_of_range& ofRange) {
			throw OnieTLVException("Cannot convert number. Number is out of range.");
		}
	}

	if (!is_ok || parsed_number < min || parsed_number > max) {
		throw OnieTLVException(fmt::format("Number value cannot be smaller than {} or higher than {}.", min, max));
	}

	return parsed_number;
}

bool OnieTLV::is_eeprom_valid(uint32_t crc32)
{
	if (crc32 == eeprom_tlv_crc32_generated)
		return true;
	Logger::error("CRC mismatch EEPROM value {} != expected value {}", crc32, eeprom_tlv_crc32_generated);
	return false;
}

bool OnieTLV::load_eeprom_file(const uint8_t *eeprom)
{
	struct tlv_header_raw *record_header;
	uint32_t crc_eeprom;
	uint32_t eeprom_crc32_host;
	uint16_t total_bytes_eeprom;
	uint8_t eeprom_generated[TLV_EEPROM_MAX_SIZE];
	uint32_t read_bytes = 0;
	uint8_t *eeprom_read_ptr = const_cast<uint8_t *>(eeprom);

	if (!eeprom)
		return false;

	record_header = reinterpret_cast<tlv_header_raw *> (eeprom_read_ptr);
	if (strncmp(record_header->signature, TLV_EEPROM_ID_STRING, strlen(TLV_EEPROM_ID_STRING)) != 0) {
		Logger::error("EEPROM TLV signature is invalid. Skipping loading values.");
		return false;
	}
	eeprom_read_ptr += HEADER_SIZE;
	read_bytes += HEADER_SIZE;

	// Convert total length from big endian
	total_bytes_eeprom = ntohs(record_header->total_length) + HEADER_SIZE;
	total_bytes_eeprom = (total_bytes_eeprom > TLV_EEPROM_MAX_SIZE)?TLV_EEPROM_MAX_SIZE:total_bytes_eeprom;
	Logger::info("load_eeprom_file, length : [{}]", total_bytes_eeprom);

	while (read_bytes < total_bytes_eeprom) {
		TLVRecord record;
		struct tlv_record_raw *record_raw = reinterpret_cast<tlv_record_raw *> (eeprom_read_ptr);

		record.type = record_raw->type;
		record.data_length = record_raw->length;
		record.data.assign(reinterpret_cast<char *>(record_raw->value), record.data_length);
		Logger::debug("Type 0x{:x} Len: {}", record.type, record.data_length);
		update_records(record);

		eeprom_read_ptr += RECORD_SIZE + record.data_length;
		read_bytes += RECORD_SIZE + record.data_length;
	}

	TLVRecord *crc32_record = find_record_or_nullptr(TLV_CODE_CRC_32);
	if (!crc32_record) {
		Logger::error("CRC32 NOT FOUND! Start from scratch!");
		tlv_records.clear();
		return false;
	}

	// CRC in EEPROM is saved as big endian
	memcpy(&crc_eeprom, crc32_record->data.c_str(), crc32_record->data_length);
	eeprom_crc32_host = ntohl(crc_eeprom);

	generate_eeprom_file(eeprom_generated);
	if (!is_eeprom_valid(eeprom_crc32_host)) {
		Logger::error("EEPROM TLV is not valid! CRC mismatch!");
		return false;
	}

	Logger::debug("EEPROM TLV is valid.");
	return true;
}

bool OnieTLV::generate_eeprom_file(uint8_t *eeprom)
{
	uint32_t crc_to_eeprom;
	uint8_t *eeprom_write_ptr = eeprom;

	if (!eeprom)
		return false;

	std::sort(tlv_records.begin(), tlv_records.end(),
		   [](auto const &a, auto const &b) {return a.type < b.type; });

	// Prepare some space for header that will be written later.
	usage = HEADER_SIZE;
	eeprom_write_ptr += HEADER_SIZE;

	for (auto const &record: tlv_records) {
		// CRC record is written separately
		if (record.type == TLV_CODE_CRC_32)
			continue;
		struct tlv_record_raw *tlv_record = reinterpret_cast<struct tlv_record_raw *>(eeprom_write_ptr);
		tlv_record->type = record.type;
		tlv_record->length = record.data_length;
		memcpy(tlv_record->value, record.data.c_str(), tlv_record->length);
		usage += RECORD_SIZE + tlv_record->length;
		eeprom_write_ptr += RECORD_SIZE + tlv_record->length;
	}
	// Header is saved at the beginning of the file, with length as big endian.
	struct tlv_header_raw record_header;
	strcpy(record_header.signature, TLV_EEPROM_ID_STRING);
	record_header.version = (uint8_t )TLV_EEPROM_VERSION;
	// Total length = all data records without header
	record_header.total_length = htons(usage - HEADER_SIZE + TLV_EEPROM_LEN_CRC);
	memcpy(eeprom, &record_header, sizeof (struct tlv_header_raw));

	struct tlv_record_raw *crc_record = reinterpret_cast<struct tlv_record_raw *>(eeprom_write_ptr);
	crc_record->type = TLV_CODE_CRC_32;
	crc_record->length = 4;
	// CRC32 is calculated from 'T' in header to length in
	usage += RECORD_SIZE;
	eeprom_tlv_crc32_generated = crc32(0, eeprom, usage);

	// CRC must be saved in big endian in EEPROM
	crc_to_eeprom = htonl(eeprom_tlv_crc32_generated);
	memcpy(crc_record->value, &crc_to_eeprom, crc_record->length);
	usage += crc_record->length;

	Logger::debug("EEPROM usage [{}] bytes.", usage);
	return true;
}

bool OnieTLV::save_user_tlv(tlv_code_t tlv_id, std::string value)
{
	switch (tlv_id) {
		case TLV_CODE_PRODUCT_NAME:
		case TLV_CODE_PART_NUMBER:
		case TLV_CODE_SERIAL_NUMBER:
		case TLV_CODE_LABEL_REVISION:
		case TLV_CODE_PLATFORM_NAME:
		case TLV_CODE_ONIE_VERSION:
		case TLV_CODE_MANUF_NAME:
		case TLV_CODE_VENDOR_NAME:
		case TLV_CODE_DIAG_VERSION:
		case TLV_CODE_SERVICE_TAG:
		case TLV_CODE_VENDOR_EXT: {
			TLVRecord new_record;
			// For all text based fields just copy the text to EEPROM
			validate_text(value, TLV_EEPROM_VALUE_MAX_SIZE);
			new_record.type = tlv_id;
			new_record.data_length = value.length();
			new_record.data = value;
			update_records(new_record);
			break;
		}
		case TLV_CODE_DEV_VERSION: {
			TLVRecord new_record;
			// Device version is just single byte
			int num_version = parse_number(value, 0, 255);
			new_record.type = tlv_id;
			new_record.data_length = 1;
			new_record.data.assign(reinterpret_cast<char *>(&num_version), new_record.data_length);
			update_records(new_record);
			break;
		}
		case TLV_CODE_NUM_MACs: {
			TLVRecord new_record;
			// Number num_mac following MAC addresses (2 bytes)
			uint32_t num_mac = parse_number(value, 0, 65535);
			uint16_t num_mac_eeprom = htons(num_mac);
			new_record.type = tlv_id;
			new_record.data_length = 2;
			new_record.data.assign(reinterpret_cast<char *>(&num_mac_eeprom), new_record.data_length);
			update_records(new_record);
			break;
		}
		case TLV_CODE_COUNTRY_CODE: {
			TLVRecord new_record;
			// Country code is just a string limited to 2 bytes
			validate_text(value, 2);
			new_record.type = tlv_id;
			new_record.data_length = 2;
			new_record.data = value;
			update_records(new_record);
			break;
		}
		case TLV_CODE_MAC_BASE: {
			TLVRecord new_record;
			// MAC address is saved as 6 bytes without any additional characters. MAC address must be checked
			// if it's not all zeros or if it's not broadcast address.
			uint8_t mac_address[6];
			parse_mac_address(value, mac_address);
			new_record.type = tlv_id;
			new_record.data_length = 6;
			new_record.data.assign(reinterpret_cast<const char *>(mac_address), new_record.data_length);
			update_records(new_record);
			break;
		}
		case TLV_CODE_MANUF_DATE: {
			TLVRecord new_record;
			// Manufacture date must be in format MM/DD/YYYY hh:mm:ss it's stored in this format in EEPROM
			validate_date(value);
			new_record.type = tlv_id;
			new_record.data_length = 19;
			new_record.data = value;
			update_records(new_record);
			break;
		}
		case TLV_CODE_CRC_32:
			// This field is computed before write to EEPROM cannot be set.
			throw OnieTLVException("CRC field cannot be set!");
		default:
			// Any other type is wrong type
			throw OnieTLVException(fmt::format("Invalid field set 0x{:x} = {}", tlv_id, value));
	}

	return true;
}

std::optional<std::string> OnieTLV::get_tlv_record(const tlv_code_t tlv_id) {
	TLVRecord *record = find_record_or_nullptr(tlv_id);
	if (!record) {
		Logger::error("Field tlv_id 0x{:x} was not found!", tlv_id);
		return {};
	}

	switch (tlv_id) {
		case TLV_CODE_PRODUCT_NAME:
		case TLV_CODE_PART_NUMBER:
		case TLV_CODE_SERIAL_NUMBER:
		case TLV_CODE_LABEL_REVISION:
		case TLV_CODE_PLATFORM_NAME:
		case TLV_CODE_ONIE_VERSION:
		case TLV_CODE_MANUF_NAME:
		case TLV_CODE_VENDOR_NAME:
		case TLV_CODE_DIAG_VERSION:
		case TLV_CODE_SERVICE_TAG:
		case TLV_CODE_VENDOR_EXT:
		case TLV_CODE_COUNTRY_CODE:
		case TLV_CODE_MANUF_DATE:
			return record->data;
		case TLV_CODE_DEV_VERSION:
			return std::to_string((uint8_t)record->data.c_str()[0]);
		case TLV_CODE_NUM_MACs: {
			uint16_t eeprom_num_mac;
			memcpy(&eeprom_num_mac, record->data.c_str(), record->data_length);
			return std::to_string(ntohs(eeprom_num_mac));
		}
		case TLV_CODE_MAC_BASE: {
			char mac_text[20];
			const char *mac = record->data.c_str();
			/* libfmt formatter cannot be used here, as it skips leading zeros while printing hex */
			sprintf(mac_text, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0]&0xFF, mac[1]&0xFF, mac[2]&0xFF,
					mac[3]&0xFF, mac[4]&0xFF, mac[5]&0xFF);
			return std::string(mac_text);
		}
		case TLV_CODE_RESERVED:
		case TLV_CODE_RESERVED_1:
			Logger::warning("Reading reserved field 0x{:x} is not allowed!", tlv_id);
			return {};
		case TLV_CODE_CRC_32:
			Logger::warning("Reading crc field 0x{:x} should not be done.", tlv_id);
			return {};
	}

	return {};
}

size_t OnieTLV::get_usage()
{
	return usage;
}

TLVRecord *OnieTLV::find_record_or_nullptr(tlv_code_t tlv_id)
{
	auto is_selected_id = [tlv_id](TLVRecord rec){ return rec.type == tlv_id; };

	auto found_record = std::find_if(std::begin(tlv_records), std::end(tlv_records), is_selected_id);
	if (found_record != std::end(tlv_records))
		return &(*found_record);
	else
		return nullptr;
}

void OnieTLV::update_records(TLVRecord& rec)
{
	for (auto &record : tlv_records) {
		if (record.type == rec.type) {
			record.data = rec.data;
			record.data_length = rec.data_length;
			return;
		}
	}
	tlv_records.push_back(rec);
};

bool OnieTLV::load_from_yaml(std::string filename) {
	YAML::Node config;
	try {
		config = YAML::LoadFile(filename);
	}
	catch (const YAML::BadFile& badFile) {
		Logger::error("Error while reading file.");
	}

	if (config["board-name"]) {
		board_name = config["board-name"].as<std::string>();
		Logger::debug("Reading YAML config. Board name: {}", board_name);
	} else {
		Logger::error("EEPROM configuration file doesn't have board name. Abort reading.");
		return false;
	}
	if (config["rev"]) {
		revision = config["rev"].as<std::string>();
		Logger::debug("Reading YAML config. Revision: {}", revision);
	} else {
		Logger::error("EEPROM configuration file doesn't have revision. Abort reading.");
		return false;
	}

	if (!config["eeprom"]){
		Logger::error("EEPROM configuration file doesn't have eeeprom section. Abort reading.");
		return false;
	}

	tlv_code_t tlv_id;
	for (YAML::const_iterator it=config["eeprom"].begin();it!=config["eeprom"].end();++it) {
		YAML::Node node = *it;
		if (!node["name"] || !node["value"])
			continue;

		try {
			tlv_id = yaml_map.at(node["name"].as<std::string>());
		} catch (const std::out_of_range& oor) {
			continue;
		}

		try {
			save_user_tlv(tlv_id, node["value"].as<std::string>());
		} catch (OnieTLVException& exception) {
			Logger::error("Error while parsing field id 0x{:x} = '{}'. Info: {}", tlv_id,
					node["name"].as<std::string>(), exception.get_info());
		}
	}
	return true;
}
