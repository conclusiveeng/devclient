//
// Created by jakub on 24.10.2019.
//
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

#ifndef DEVCLIENT_UART_HH
#define DEVCLIENT_UART_HH

#include <thread>
#include <vector>
#include <giomm.h>
#include <ftdi.hpp>
#include <device.hh>

class UartConnection
{
public:
	Glib::RefPtr<Gio::SocketAddress> m_address;
	Glib::RefPtr<Gio::SocketConnection> m_conn;
	Glib::RefPtr<Gio::OutputStream> m_ostream;
	Glib::RefPtr<Gio::Cancellable> m_cancel;

	inline bool operator==(const UartConnection &other)
	{
		return (m_address.get() == other.m_address.get());
	}
};

class Uart
{
public:
	Uart(const Device &device, const Glib::RefPtr<Gio::SocketAddress> &addr,
	    int baudrate);
	Uart(const Ftdi::Context &context, const Glib::RefPtr<Gio::SocketAddress> &addr,
    int baudrate);
	virtual ~Uart();
	void start();
	void stop();

	sigc::signal<void, Glib::RefPtr<Gio::SocketAddress>> m_connected;
	sigc::signal<void, Glib::RefPtr<Gio::SocketAddress>> m_disconnected;

protected:
	void remove_connection(const UartConnection &conn);
	void usb_worker();
	bool socket_worker(
	    const Glib::RefPtr<Gio::SocketConnection> &conn,
	    const Glib::RefPtr<Glib::Object> &source);

	Ftdi::Context m_context;
	Glib::RefPtr<Gio::ThreadedSocketService> m_socket_service;
	std::vector<UartConnection> m_connections;
	std::thread m_usb_worker;
	Device m_device;
	bool m_running;
};

#endif //DEVCLIENT_UART_HH
