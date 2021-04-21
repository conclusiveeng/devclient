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

#include <iostream>
#include <memory>
#include <string>
#include <sysexits.h>
#include <getopt.h>
#include <fmt/format.h>
#include <gtkmm/application.h>
#include <fstream>
#include <stdlib.h>

#include <log.hh>
#include <device.hh>
#include <uart.hh>
#include <i2c.hh>
#include <eeprom/24c.hh>
#include <gpio.hh>
#include <utils.hh>
#include <mainwindow.hh>
#include <application.hh>
#include <nogui.hh>
#include <ucl.h>

using namespace std;

static const struct option long_options[] = {
	{ "baudrate", required_argument, nullptr, 'b' },
	{ "compile-dts", required_argument, nullptr, 'c' },
	{ "device", optional_argument, nullptr, 'd' },
	{ "gpio", optional_argument, nullptr, 'g' },
	{ "help", no_argument, nullptr, 'h' },
	{ "jtag", required_argument, nullptr, 'j' },
	{ "list", no_argument, nullptr, 'l' },
	{ "passthrough", no_argument, nullptr, 'p' },
	{ "read-eeprom", no_argument, nullptr, 'r' },
	{ "script", required_argument, nullptr, 's' },
	{ "decompile-dts", required_argument, nullptr, 't' },
	{ "uart", required_argument, nullptr, 'u' },
	{ "write-eeprom", no_argument, nullptr, 'w' },
	{ "config", required_argument, nullptr, 'x' },
	{ nullptr, 0, nullptr, 0}
};

static void
usage(const std::string &argv0)
{
	fmt::print("usage: {:s}\n", argv0);
	fmt::print("-b:		baud rate for UART port, allowed values: 9600, 19200, 38400, 57600, 115200\n");
	fmt::print("		example: -b 115200\n");
	fmt::print("-c:		compile dts from file and write it to eeprom\n");
	fmt::print("		example: -c board.dts\n");
	fmt::print("-d:		serial string of the selected device\n");
	fmt::print("		example: -d 006/2019\n");
	fmt::print("-g:		set value for gpio pins\n");
	fmt::print("-h:		this help message\n");
	fmt::print("-j:		IP address and two TCP port numbers for listening for JTAG communication\n");
	fmt::print("		cannot be used together with -p option\n");
	fmt::print("		parameter format: <IP_address>:<gdb_port>:<telnet_port>\n");
	fmt::print("		example: -j 0.0.0.0:3333:4444\n");
	fmt::print("-l:		list connected devices\n");
	fmt::print("-p:		enable JTAG pass-through mode, cannot be used together with -j option\n");
	fmt::print("-r:		read raw eeprom contents (binary data) and save it to file\n");
	fmt::print("		example: -r eeprom.img\n");
	fmt::print("-s:		absolute path to script\n");
	fmt::print("-t:		download contents of eeprom, decompile it and write it to dts file\n");
	fmt::print("		example: -t board.dts\n");
	fmt::print("-u:		IP address and TCP port number for listening for serial/uart communication\n");
	fmt::print("		example: -u 0.0.0.0:2222\n");
	fmt::print("-w:		write raw contents of file (binary data) to eeprom\n");
	fmt::print("		example: -w eeprom.img\n");
	fmt::print("-x:		configuration file name\n");
	fmt::print("		example: -w devclient.cfg\n");
	fmt::print("\nInvocation examples:\n");
	fmt::print("{:s} -d 006/2019 -u 0.0.0.0:2222 -b 115200 -j 0.0.0.0:3333:4444 -s /tmp/script\n", argv0);
	fmt::print("{:s} -d 006/2019 -u 0.0.0.0:2222 -b 115200 -p\n", argv0);
	fmt::print("{:s} -d 006/2019 -w eeprom.img\n", argv0);
	fmt::print("{:s} -x devclient.cfg\n", argv0);
}


int
uart_maintenance(std::string serial, std::string uart_listen_addr, uint32_t baudrate_value, std::shared_ptr<SerialCmdLine> &serial_cmd)
{
	Device dev;

	if (!uart_listen_addr.empty()) {
		uint16_t port;
		std::string addr;

		if ((baudrate_value != 9600) &&
			(baudrate_value != 19200) &&
			(baudrate_value != 38400) &&
			(baudrate_value != 57600) &&
			(baudrate_value != 115200)) {
			fmt::print("Improper baud rate value: {:d}\n", baudrate_value);
			exit(0);
		}

		addr = uart_listen_addr.substr(0, uart_listen_addr.find(':'));
		port = std::stoi(uart_listen_addr.substr(
			uart_listen_addr.find(':') + 1,
			uart_listen_addr.size()), 0, 10);

		auto saddr = Gio::InetSocketAddress::create(
			Gio::InetAddress::create(addr),
			port);

		dev = *DeviceEnumerator::find_by_serial(serial);
		serial_cmd = std::shared_ptr<SerialCmdLine>(new SerialCmdLine(
			dev,
			saddr,
			baudrate_value));
		serial_cmd->start();
	}

	return 0;
}


int jtag_maintenance(std::string serial, std::string jtag, std::string script, std::shared_ptr<JtagCmdLine> &jtag_cmd)
{
	Device dev;
	if (!jtag.empty()) {
		uint16_t port_gdb, port_ocd;
		std::string addr;
		Glib::RefPtr<Gio::InetAddress> saddr;

		addr = jtag.substr(0, jtag.find(':'));
		port_gdb = std::stoi(jtag.substr(
			jtag.find(':') + 1,
			jtag.size()), 0, 10);
		port_ocd = std::stoi(jtag.substr(
			jtag.rfind(':') + 1,
			jtag.size()), 0, 10);

		dev = *DeviceEnumerator::find_by_serial(serial);
		saddr = Gio::InetAddress::create(addr);

		jtag_cmd = std::unique_ptr<JtagCmdLine>(new JtagCmdLine(
			dev,
			saddr,
			port_gdb,
			port_ocd,
			script));
		jtag_cmd->m_server->start();
	}

	return 0;
}


int
parse_config_file(std::string file_read, std::shared_ptr<SerialCmdLine> &serial_cmd, std::shared_ptr<JtagCmdLine> &jtag_cmd)
{
	ucl_parser *parser;
	const ucl_object_t *root, *uart, *jtag, *device, *serial;
	const ucl_object_t *baud, *uart_ip, *uart_port;
	const ucl_object_t *jtag_ip, *gdb_port, *telnet_port, *pass_through, *jtag_script;
	std::string uart_listen_addr;
	uint32_t baudrate_value;

	parser = ucl_parser_new(0);
	if (!ucl_parser_add_file(parser, file_read.c_str()))
		printf("error loading config file\n");

	root = ucl_parser_get_object(parser);
	device =  ucl_object_lookup(root, "device");

	serial = ucl_object_lookup(device, "serial");

	/* parse UART */
	uart = ucl_object_lookup(device, "uart");
	baud = ucl_object_lookup(uart, "baudrate");
	baudrate_value = ucl_object_toint(baud);
	uart_ip = ucl_object_lookup(uart, "listen_ip");
	uart_port = ucl_object_lookup(uart, "listen_port");

	/* parse JTAG */
	jtag = ucl_object_lookup(device, "jtag");
	jtag_ip = ucl_object_lookup(jtag, "listen_ip");
	gdb_port = ucl_object_lookup(jtag, "gdb_port");
	telnet_port = ucl_object_lookup(jtag, "telnet_port");
	pass_through = ucl_object_lookup(jtag, "pass_through");
	jtag_script = ucl_object_lookup(jtag, "script");

	if ((pass_through != NULL) && (ucl_object_toint(pass_through)) && (jtag_ip != NULL)) {
		Logger::error("JTAG server and pass through mode cannot be used together");
		exit(0);
	}

	if (uart != NULL) {
		char addr[128];
		std::sprintf(addr, "%s:%lu", ucl_object_tostring(uart_ip), ucl_object_toint(uart_port));
		uart_listen_addr.assign(addr, strlen(addr));
		uart_maintenance(ucl_object_tostring(serial), uart_listen_addr, baudrate_value, serial_cmd);
	}

	if ((jtag != NULL) && (!(ucl_object_toint(pass_through)))) {
		char jtag2[128];
		std::string jtag3;
		std::sprintf(jtag2, "%s:%lu:%lu",
			     ucl_object_tostring(jtag_ip),
			     ucl_object_toint(gdb_port), ucl_object_toint(telnet_port));
		jtag3.assign(jtag2, strlen(jtag2));
		jtag_maintenance(ucl_object_tostring(serial), jtag2, ucl_object_tostring(jtag_script), jtag_cmd);
	}

	return 0;
}


int
parse_cmdline(int argc, char *const argv[], std::shared_ptr<SerialCmdLine> &serial_cmd, std::shared_ptr<JtagCmdLine> &jtag_cmd)
{
	Glib::RefPtr<Gtk::Application> app;
	Device dev;
	std::unique_ptr<Uart> uart;
	std::string uart_listen_addr;
	std::string serial;
	std::string jtag;
	std::string script;
	std::string file_read;
	std::string file_write;
	uint8_t gpio_value;
	uint32_t baudrate_value;
	std::ofstream f_out;
	std::ifstream f_in;
	bool cmdline = false;
	bool list = false;
	bool eeprom_read = false;
	bool eeprom_write = false;
	bool eeprom_compile = false;
	bool eeprom_decompile = false;
	bool gpio = false;
	bool pass_through = false;
	bool config = false;
	int ch;

	for (;;) {
		ch = getopt_long(argc, argv, "b:c:d:g:hj:lpr:s:t:u:w:x:", long_options, nullptr);
		if (ch == -1)
			break;

		switch (ch) {
		case 'b':
			baudrate_value = std::stoi(optarg, 0, 10);
			cmdline = true;
			break;
		case 'c':
			eeprom_compile = true;
			file_read = optarg;
			break;
		case 'd':
			serial = optarg;
			cmdline = true;
			break;
		case 'g':
			gpio = true;
			gpio_value = std::stoi(optarg, 0, 16);
			break;
		case 'h':
			usage(argv[0]);
			exit(0);
		case 'j':
			jtag = optarg;
			cmdline = true;
			break;
		case 'l':
			list = true;
			break;
		case 'p':
			pass_through = true;
			cmdline = true;
			break;
		case 'r':
			eeprom_read = true;
			file_write = optarg;
			break;
		case 's':
			script = optarg;
			cmdline = true;
			break;
		case 't':
			eeprom_decompile = true;
			file_write = optarg;
			break;
		case 'u':
			uart_listen_addr = optarg;
			cmdline = true;
			break;
		case 'w':
			eeprom_write = true;
			file_read = optarg;
			break;
		case 'x':
			config = true;
			file_read = optarg;
			cmdline = true;
			break;
		default:
			usage(argv[0]);
			exit(EX_USAGE);
		}
	}

	Gio::init();

	if (config) {
		parse_config_file(file_read, serial_cmd, jtag_cmd);
	}

	if (!jtag.empty() && pass_through) {
		Logger::error("JTAG options -j and -p cannot be used together");
		exit(0);
	}

	if (list) {
		fmt::print("Available devices:\n");
		for (const auto &i: DeviceEnumerator::enumerate()) {
			fmt::print("{:#04x}:{:#04x} - {} ({})\n", i.vid, i.pid,
			    i.description, i.serial);
		}
		exit(0);
	}

	if (gpio) {
		dev = *DeviceEnumerator::find_by_serial(serial);
		Gpio gpio(dev);
		gpio.set(gpio_value);
		exit(0);
	}

	if (eeprom_read) {
		dev = *DeviceEnumerator::find_by_serial(serial);
		I2C i2c(dev, 300000);
		Eeprom24c eeprom(i2c);
		std::vector<uint8_t> data;
		char rdata[4096];

		eeprom.read(0, 4096, data);
		f_out.open(file_write, ios::out | ios::binary | ios::trunc);
		std::copy(data.begin(), data.end(), rdata);
		f_out.write(rdata, 4096);
		f_out.close();
		exit(0);
	}

	if (eeprom_write) {
		dev = *DeviceEnumerator::find_by_serial(serial);
		I2C i2c(dev, 300000);
		Eeprom24c eeprom(i2c);
		std::vector<uint8_t> data;
		char rdata[4096];

		f_in.open(file_read, ios::in | ios::binary);
		f_in.read(rdata, 4096);
		f_in.close();
		for (unsigned int x = 0; x < sizeof(rdata); x++)
			data.push_back(rdata[x]);
		eeprom.write(0, data);
		exit(0);
	}

	if (eeprom_decompile) {
		dev = *DeviceEnumerator::find_by_serial(serial);
		I2C i2c(dev, 300000);
		Eeprom24c eeprom(i2c);
		std::vector<uint8_t> data;
		char rdata[4096], fname[256], cmd[256 + 128 + 32];

		eeprom.read(0, 4096, data);

		// save contents of eeprom to temporary file
		std::sprintf(fname, "%s_tmp", file_write.c_str());
		f_out.open(fname, ios::out | ios::binary | ios::trunc);
		std::copy(data.begin(), data.end(), rdata);
		f_out.write(rdata, 4096);
		f_out.close();

		// dtb decompilation
		std::sprintf(cmd, "dtc -I dtb -O dts %s -o %s", fname, file_write.c_str());
		std::system(cmd);
		std::remove(fname);
		exit(0);
	}

	if (eeprom_compile) {
		dev = *DeviceEnumerator::find_by_serial(serial);
		I2C i2c(dev, 300000);
		Eeprom24c eeprom(i2c);
		std::vector<uint8_t> data;
		char rdata[4096], fname[256], cmd[256 + 128 + 32];

		std::sprintf(fname, "%s_tmp", file_read.c_str());
		// dts compilation
		std::sprintf(cmd, "dtc -I dts -O dtb %s -o %s", file_read.c_str(), fname);
		std::system(cmd);

		f_in.open(fname, ios::in | ios::binary);
		f_in.read(rdata, 4096);
		f_in.close();
		for (unsigned int x = 0; x < sizeof(rdata); x++)
			data.push_back(rdata[x]);
		std::remove(fname);

		eeprom.write(0, data);
		exit(0);
	}

	if (!uart_listen_addr.empty())
		uart_maintenance(serial, uart_listen_addr, baudrate_value, serial_cmd);

	if (!jtag.empty())
		jtag_maintenance(serial, jtag, script, jtag_cmd);

	if (pass_through) {
		jtag_cmd = std::unique_ptr<JtagCmdLine>(new JtagCmdLine(dev));
		jtag_cmd->bypass(dev);
	}

	return cmdline;
}


int
main(int argc, char *const argv[])
{
	Glib::RefPtr<Gtk::Application> app;
	std::shared_ptr<SerialCmdLine> serial_cmd;
	bool cmdline = false;
	std::shared_ptr<JtagCmdLine> jtag_cmd;

	Gio::init();
	Glib::init();

	cmdline = parse_cmdline(argc, argv, serial_cmd, jtag_cmd);

	if (cmdline == true) {
		serial_cmd->main_loop->run();
	} else {
		return Devclient::Application::instance()->run();
	}
}
