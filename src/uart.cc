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
#include <utils.hh>
#include <uart.hh>
#include <gtkmm.h>

#define BUFSIZE		4096

Uart::Uart(const Device &device, const Glib::RefPtr<Gio::SocketAddress> &addr,
    int baudrate)
{
	Glib::RefPtr<Gio::SocketAddress> retaddr;

	m_running = false;
	m_context.set_interface(INTERFACE_C);
	m_device = device;

	if (m_context.open(device.vid, device.pid, device.description,
	    device.serial) != 0) {
		show_centered_dialog("Failed to open device.");
		return;
	}

	if (m_context.reset() != 0) {
		show_centered_dialog("Failed to reset UART channel");
		return;
	}
	
	if (m_context.set_bitmode(0xff, BITMODE_RESET) != 0) {
		show_centered_dialog("Failed to reset bitmode.");
		return;
	}

	if (m_context.bitbang_disable() != 0) {
		show_centered_dialog("Failed to set bitbang_disable.");
		return;
	}

	if (m_context.set_baud_rate(baudrate) != 0) {
		show_centered_dialog("Failed to set the baud rate.");
		return;
	}

	m_context.set_latency(1);

	try {
		m_socket_service = Gio::ThreadedSocketService::create(10);
		m_socket_service->add_address(
		    addr,
		    Gio::SocketType::SOCKET_TYPE_STREAM,
		    Gio::SocketProtocol::SOCKET_PROTOCOL_TCP,
		    retaddr);
	} catch (const Glib::Exception &err) {
		throw std::runtime_error(err.what());
	}

	m_socket_service->signal_run().connect(sigc::mem_fun(*this,
	    &Uart::socket_worker));

	Logger::info("UART: listening on {}", addr->to_string());
}

Uart::~Uart()
{
	stop();
}

void
Uart::start()
{
	if (m_running)
		return;

	try {
		m_socket_service->start();
		m_usb_worker = std::thread(&Uart::usb_worker, this);
	} catch (const std::exception &err) {
		throw std::runtime_error(err.what());
	}

	m_running = true;
	Logger::debug("UART: started");
}

void
Uart::stop()
{
	if (!m_running)
		return;

	for (auto &i: m_connections)
		i.m_cancel->cancel();

	m_running = false;
	m_socket_service->stop();
	m_socket_service->close();
	m_context.close();
	m_usb_worker.join();
	Logger::debug("UART: stopped");
}

void
Uart::usb_worker()
{
	uint8_t buffer[BUFSIZE];
	int ret;

	Logger::debug("UART: USB thread started");

	for (;;) {
		ret = m_context.read(buffer, sizeof(buffer));
		if (ret < 0 || !m_running)
			break;

		if (ret == 0)
			continue;

		Logger::debug("read {} bytes from USB", ret);

		for (auto &i: m_connections) {
			try {
				i.m_ostream->write(buffer, ret);
			} catch (const Gio::Error &err) {
				Logger::warning(
				    "UART: error sending data to {}: {}",
				    i.m_conn->get_remote_address()->to_string(),
				    err.what());
				remove_connection(i);
				continue;
			}
		}
	}

	Logger::debug("UART: USB thread stopped");
}

bool
Uart::socket_worker(const Glib::RefPtr<Gio::SocketConnection> &conn,
    const Glib::RefPtr<Glib::Object> &source)
{
	UartConnection uartconn;
	Glib::RefPtr<Gio::InputStream> istream;
	uint8_t buffer[BUFSIZE];
	ssize_t ret;
	int written;

	Logger::info("UART: accepted connection from {}",
	    conn->get_remote_address()->to_string());

	uartconn.m_address = conn->get_remote_address();
	uartconn.m_address->reference();
	uartconn.m_cancel = Gio::Cancellable::create();
	uartconn.m_conn = conn;
	uartconn.m_ostream = conn->get_output_stream();

	istream = conn->get_input_stream();
	m_connections.push_back(uartconn);
	m_connected.emit(uartconn.m_address);

	/* Disable local echo */
	uartconn.m_ostream->write("\xFF\xFB\x01\xFF\xFB\x03");
	uartconn.m_ostream->write(fmt::format("==> Connected to {} {} <==\r\n",
	    m_device.description, m_device.serial));

	for (;;) {
		try {
			ret = istream->read(buffer, sizeof(buffer),
			    uartconn.m_cancel);
			if (ret <= 0)
				break;
		} catch (const Gio::Error &err) {
			Logger::warning("UART: I/O error: {}", err.what());
			break;
		}

		Logger::debug("UART: read {} bytes from socket", ret);

		written = m_context.write(buffer, ret);
		if (written != ret) {
			Logger::error("UART: read {} bytes, written {} bytes",
			    ret, written);
		}
	}
	
	Logger::info("UART: connection from {} ended",
	    uartconn.m_address->to_string());

	remove_connection(uartconn);
	return (false);
}

void
Uart::remove_connection(const UartConnection &conn)
{
	auto it = std::find(m_connections.begin(),
	    m_connections.end(), conn);

	if (it != m_connections.end()) {
		m_disconnected.emit(conn.m_conn->get_remote_address());
		m_connections.erase(it);
	}
}
