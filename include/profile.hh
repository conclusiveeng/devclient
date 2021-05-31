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

#ifndef DEVCLIENT_PROFILE_HH
#define DEVCLIENT_PROFILE_HH

#include "uart.hh"
#include <bits/stdint-uintn.h>
#include <cstdint>
#include <exception>
#include <string>
#include <vector>
#include <yaml-cpp/node/node.h>
#include <yaml-cpp/yaml.h>

class ProfileValueException : public std::exception {
  std::string error_msg;

public:
  ProfileValueException(const std::string &msg) : std::exception() {
    error_msg = msg;
  }

  const char *what() const throw() { return "ProfileValueException"; }

  std::string get_info() const { return error_msg; }
};

class Profile {

public:
  Profile(const std::string &file_name);
  std::string get_devcable_serial();
  std::uint32_t get_uart_baudrate();
  std::string get_uart_listen_address();
  std::uint32_t get_uart_port();
  std::uint32_t get_jtag_gdb_port();
  std::uint32_t get_jtag_telnet_port();
  std::string get_jtag_listen_address();
  std::string get_jtag_script_file();
  bool get_jtag_passtrough();
  std::string get_gpio_name(int gpio);
  std::string get_eeprom_file();

private:
   YAML::Node uart;
   YAML::Node jtag;
   YAML::Node gpio;
   YAML::Node eeprom;
   std::string devcable_serial;
   std::vector<std::string> gpio_names;
};

#endif /* DEVCLIENT_PROFILE_HH */
