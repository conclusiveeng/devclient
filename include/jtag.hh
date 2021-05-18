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

#ifndef DEVCLIENT_JTAG_HH
#define DEVCLIENT_JTAG_HH

#include <giomm.h>
#include <device.hh>

class JtagServer
{
public:
	JtagServer(const Device &device, Glib::RefPtr<Gio::InetAddress>,
	    uint16_t gdb_port, uint16_t ocd_port,
	    const std::string &board_script);
	virtual ~JtagServer();
	void start();
	void stop();
	static void bypass(const Device &device);
	static void reset(const Device &device);

	sigc::signal<void, const std::string &> on_output_produced;
	sigc::signal<void> on_server_start;
	sigc::signal<void> on_server_exit;

protected:
	void prepare_child();
	void child_exited(Glib::Pid pid, int code);
	void output_ready(Glib::RefPtr<Gio::AsyncResult> &result/*,
	    Glib::RefPtr<Gio::UnixInputStream> stream*/);

	const Device &m_device;
	Glib::RefPtr<Gio::InetAddress> m_address;
	uint16_t m_ocd_port;
	uint16_t m_gdb_port;
	std::string m_board_script;
	Glib::Pid m_pid;
	// Glib::RefPtr<Gio::UnixInputStream> m_out;
	// Glib::RefPtr<Gio::UnixInputStream> m_err;
	bool m_running;
};

#endif /* DEVCLIENT_JTAG_HH */
