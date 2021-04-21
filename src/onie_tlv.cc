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

#define HEADER_SIZE sizeof (struct tlv_header_raw)
#define RECORD_SIZE sizeof (struct tlv_record_raw)

OnieTLV::OnieTLV()
{

};

OnieTLV::~OnieTLV()
{

}

bool OnieTLV::validate_date(const char *value)
{
	struct tm tm;

	if (!value || strlen(value) != 19) {
		Logger::error("Bad date format. Should be MM/DD/YYYY hh:mm:ss");
		return false;
	}

	if (!strptime(value, "%m/%d/%Y %H:%M:%S", &tm))
		return false;

	return true;
}

bool OnieTLV::validate_mac_address(const char *mac_address)
{
	int ret;
	int mac_bytes[6];

	if (!mac_address || (strlen(mac_address) != 17)) {
		Logger::error("Bad MAC address format. Should be xx:xx:xx:xx:xx");
		return -1;
	}

	ret = std::sscanf(mac_address, "%02x:%02x:%02x:%02x:%02x:%02x",
			&mac_bytes[0], &mac_bytes[1], &mac_bytes[2], &mac_bytes[3], &mac_bytes[4], &mac_bytes[5]);
	if (ret != 6)
		return false;

	if ((mac_bytes[0] | mac_bytes[1] | mac_bytes[2] | mac_bytes[3] | mac_bytes[4] | mac_bytes[5]) == 0)
		return false;  // MAC address cannot be 00:00:00:00:00:00

	if (0x01 & mac_bytes[0])
		return false;  // MAC address cannot be multicast

	return true;
}

int OnieTLV::set_mac_address(const char *value, uint8_t *mac_address)
{
	int mac_bytes[6];

	if (!validate_mac_address(value))
		return -1;

	std::sscanf(value, "%02x:%02x:%02x:%02x:%02x:%02x",
			&mac_bytes[0], &mac_bytes[1], &mac_bytes[2], &mac_bytes[3], &mac_bytes[4], &mac_bytes[5]);

	for (int i=0; i < 6; i++)
		mac_address[i] = mac_bytes[i];

	return 0;
}

bool OnieTLV::is_eeprom_valid(uint32_t crc32)
{
	if (crc32 == eeprom_tlv_crc32_generated)
		return true;
	return false;
}

bool OnieTLV::load_eeprom_file(const uint8_t *eeprom)
{
	struct tlv_header_raw *record_header;
	uint32_t crc_eeprom;
	uint32_t eeprom_crc32_host;
	uint16_t total_bytes_eeprom;
	uint8_t eeprom_generated[2048];
	uint32_t read_bytes = 0;
	uint8_t *eeprom_read_ptr = const_cast<uint8_t *>(eeprom);

	if (!eeprom)
		return false;

	record_header = reinterpret_cast<tlv_header_raw *> (eeprom_read_ptr);
	eeprom_read_ptr += HEADER_SIZE;
	read_bytes += HEADER_SIZE;

	// Convert total length from big endian
	total_bytes_eeprom = ntohs(record_header->total_length) + HEADER_SIZE;
	Logger::info("load_eeprom_file, length : [{}]", total_bytes_eeprom);

	while (read_bytes < total_bytes_eeprom) {
		TLVRecord record;
		struct tlv_record_raw *record_raw = reinterpret_cast<tlv_record_raw *> (eeprom_read_ptr);

		record.type = record_raw->type;
		record.data_length = record_raw->length;
		record.data.assign(reinterpret_cast<char *>(record_raw->value), record.data_length);
		Logger::debug("Type {} Len: {}", record.type, record.data_length);
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
		Logger::error("\"EEPROM TLV is not valid! CRC mismatch!");
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

bool OnieTLV::save_user_tlv(uint8_t tlv_code, const char *value)
{
	std::pair<std::string, size_t> tlv_value;
	TLVRecord new_record;
	switch (tlv_code) {
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
			// For all text based fields just copy the text to EEPROM
			new_record.type = tlv_code;
			new_record.data_length = strlen(value);
			new_record.data.assign(value, new_record.data_length);

			update_records(new_record);
			break;
		case TLV_CODE_DEV_VERSION:
			// Device version is just single byte
			new_record.type = tlv_code;
			new_record.data_length = 1;
			new_record.data.assign(value, new_record.data_length);

			update_records(new_record);
			break;
		case TLV_CODE_NUM_MACs:
			// Number on following MAC addresses (2 bytes)
			uint32_t num_mac;
			uint16_t num_mac_eeprom;
			new_record.type = tlv_code;
			new_record.data_length = 2;
			num_mac = strtol(value, NULL, 0);
			num_mac_eeprom = htons(num_mac);
			new_record.data.assign(reinterpret_cast<char *>(&num_mac_eeprom), new_record.data_length);
			update_records(new_record);
			break;
		case TLV_CODE_COUNTRY_CODE:
			// Country code is just a string limited to 2 bytes
			new_record.type = tlv_code;
			new_record.data_length = 2;
			new_record.data.assign(value, new_record.data_length);

			update_records(new_record);
			break;
		case TLV_CODE_CRC_32:
			// This field is computed before write to EEPROM cannot be set.
			return false;
		case TLV_CODE_MAC_BASE:
			// MAC address is saved as 6 bytes without any additional characters. MAC address must be checked
			// if it's not all zeros or if it's not broadcast address.
			uint8_t mac_address[6];
			if (set_mac_address(value, mac_address) < 0) {
				std::cout << "Bad mac address format!\n";
				break;
			}
			new_record.type = tlv_code;
			new_record.data_length = 6;
			new_record.data.assign(reinterpret_cast<char *>(mac_address), new_record.data_length);

			update_records(new_record);
			break;
		case TLV_CODE_MANUF_DATE:
			// Manufacture date must be in format MM/DD/YYYY hh:mm:ss it's stored in this format in EEPROM
			if (!validate_date(value)) {
				std::cout << "Bad date format!\n";
				break;
			}
			new_record.type = tlv_code;
			new_record.data_length = 19;
			new_record.data.assign(value, new_record.data_length);

			update_records(new_record);
			break;
		default:
			// Any other type is wrong type
			return false;
	}

	return true;
}

bool OnieTLV::get_string_record(const uint8_t id, char *tlv_string)
{
	TLVRecord *record;
	if (!tlv_string)
		return false;

	switch (id) {
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
			record = find_record_or_nullptr(id);
			if (!record) {
				Logger::error("Field id {} was not found!", id);
				return false;
			}
			strncpy(tlv_string, record->data.c_str(), record->data_length);
			tlv_string[record->data_length] = '\0';
	}
	return true;
}

bool OnieTLV::get_numeric_record(const uint8_t id, uint32_t *tlv_value)
{
	TLVRecord *record;
	if (!tlv_value)
		return false;

	if ((id == TLV_CODE_NUM_MACs || id == TLV_CODE_DEV_VERSION) == 0)
		return false;

	record = find_record_or_nullptr(id);
	if (!record) {
		Logger::error("Field id {} was not found!", id);
		return false;
	}

	if (record->type == TLV_CODE_DEV_VERSION) {
		*tlv_value = record->data.c_str()[0];
	} else {
		uint16_t eeprom_num_mac;
		memcpy(&eeprom_num_mac, record->data.c_str(), record->data_length);
		*tlv_value = ntohs(eeprom_num_mac);
	}

	return true;
}

size_t OnieTLV::get_usage()
{
	return usage;
}

bool OnieTLV::get_mac_record(char *tlv_mac)
{
	TLVRecord *record;
	if (!tlv_mac)
		return false;

	record = find_record_or_nullptr(TLV_CODE_MAC_BASE);
	if (!record) {
		Logger::error("Field id {} was not found!", TLV_CODE_MAC_BASE);
		return false;
	}

	const char *mac = record->data.c_str();
	sprintf(tlv_mac, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0]&0xFF, mac[1]&0xFF,
			mac[2]&0xFF, mac[3]&0xFF, mac[4]&0xFF, mac[5]&0xFF);
	return true;
}

TLVRecord *OnieTLV::find_record_or_nullptr(uint8_t id)
{
	auto is_selected_id = [id](TLVRecord rec){ return rec.type == id; };

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