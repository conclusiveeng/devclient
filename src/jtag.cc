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

#include <vector>
#include <giomm.h>
#include <gtkmm.h>
#include <ftdi.hpp>
#include <log.hh>
#include <jtag.hh>
#include <utils.hh>
#include <filesystem.hh>
#if defined(__linux__)
#include <sys/prctl.h>
#endif

#define RESET_MASK	0x20
#define BUFFER_SIZE	1024

JtagServer::JtagServer(const Device &device,
    Glib::RefPtr<Gio::InetAddress> address, uint16_t gdb_port,
    uint16_t ocd_port, const std::string &board_script):
    m_device(device),
    m_address(address),
    m_ocd_port(ocd_port),
    m_gdb_port(gdb_port),
    m_board_script(board_script),
    m_running(false)
{
}

JtagServer::~JtagServer()
{
	stop();
}

void
JtagServer::start()
{
	int stdout_fd;
	int stderr_fd;
	std::vector<std::string> argv {
		executable_dir() + "/tools/bin/openocd",
		"-c", fmt::format("bindto {}", m_address->to_string()),
		"-c", fmt::format("gdb_port {}", m_gdb_port),
		"-c", fmt::format("telnet_port {}", m_ocd_port),
		"-c", "tcl_port disabled",
		"-c", "adapter driver ftdi",
		"-c", "transport select jtag",
		"-c", "adapter speed 1000",
		"-c", "ftdi_channel 1",
		"-c", "ftdi_layout_init 0x0030 0x000b",
		"-c", "ftdi_layout_signal nTRST -noe 0x10 -ndata 0x10",
		"-c", "ftdi_layout_signal nSRST -noe 0x20 -ndata 0x20",
		"-c", fmt::format("ftdi_serial \"{}\"", m_device.serial),
		"-c", fmt::format("ftdi_vid_pid {:#04x} {:#04x}",
		    m_device.vid, m_device.pid),
		"-f", m_board_script
	};

	if (m_running)
		return;

	try {
		Glib::spawn_async_with_pipes("/tmp", argv,
		    Glib::SpawnFlags::SPAWN_DO_NOT_REAP_CHILD,
		    sigc::mem_fun(*this, &JtagServer::prepare_child), &m_pid,
		    nullptr, &stdout_fd, &stderr_fd);
	} catch (const Glib::Error &err) {
		show_centered_dialog(
		    "Failed to start JTAG server.", err.what());
		return;
	}

	Glib::signal_child_watch().connect(
	    sigc::mem_fun(*this, &JtagServer::child_exited),
	    m_pid);

	m_out = Gio::UnixInputStream::create(stdout_fd, true);
	m_err = Gio::UnixInputStream::create(stderr_fd, true);

	m_out->read_bytes_async(BUFFER_SIZE, sigc::bind(sigc::mem_fun(
	    *this, &JtagServer::output_ready), m_out));

	m_err->read_bytes_async(BUFFER_SIZE, sigc::bind(sigc::mem_fun(
	    *this, &JtagServer::output_ready), m_err));

	setpgid(m_pid, getpid());

	m_running = true;

	on_server_start.emit();
}

void
JtagServer::stop()
{
	if (!m_running)
		return;

	kill(m_pid, SIGTERM);

	/* Crappy, there should be a better way to do this */
	while (m_running)
		Gtk::Main::iteration();
}


void
JtagServer::bypass(const Device &device)
{
	Ftdi::Context context;

	context.set_interface(INTERFACE_B);

	if (context.open(device.vid, device.pid, device.description,
	    device.serial) != 0) {
		show_centered_dialog("Failed to open device.");
		return;
	}

	if (context.reset() != 0) {
		show_centered_dialog("Failed to reset channel");
		return;
	}

	if (context.set_bitmode(0xff, BITMODE_RESET) != 0) {
		show_centered_dialog("Failed to set BITMODE_RESET");
		return;
	}

	if (context.set_bitmode(0, BITMODE_BITBANG) != 0)
	{
		show_centered_dialog("Failed to set BITMODE_BITBANG");
		return;
	}

	Logger::info("Bypass mode enabled.");
	show_centered_dialog("Bypass mode enabled.");

	context.close();
}

void
JtagServer::reset(const Device &device)
{
	Ftdi::Context context;
	uint8_t data;

	context.set_interface(INTERFACE_B);

	if (context.open(device.vid, device.pid, device.description,
	    device.serial) != 0) {
		show_centered_dialog("Failed to open device");
		return;
	}

	if (context.reset() != 0) {
		show_centered_dialog("Failed to reset channel");
		return;
	}

	if (context.set_bitmode(0x0, BITMODE_RESET) != 0) {
		show_centered_dialog("Failed to set bitmode");
		return;
	}

	if (context.set_bitmode(0x20, BITMODE_BITBANG) != 0) {
		show_centered_dialog("Failed to set bitmode");
		return;
	}

	data = RESET_MASK;
	if (context.write(&data, sizeof(data)) != sizeof(data)) {
		show_centered_dialog("Failed to write reset mask");
		return;
	}

	data = 0x00;
	if (context.write(&data, sizeof(data)) != sizeof(data)) {
		show_centered_dialog("Failed to write reset mask");
		return;
	}

	usleep(1000 * 100);

	data = RESET_MASK;
	if (context.write(&data, sizeof(data)) != sizeof(data)) {
		show_centered_dialog("Failed to write reset mask");
		return;
	}

	if (context.set_bitmode(0, BITMODE_BITBANG) != 0) {
		show_centered_dialog("Failed to set bitmode");
		return;
	}

	Logger::info("Reset done");
	context.close();
}

void
JtagServer::prepare_child()
{
#if defined(__linux__)
	prctl(PR_SET_PDEATHSIG, SIGTERM);
#endif
}

void
JtagServer::output_ready(Glib::RefPtr<Gio::AsyncResult> &result,
    Glib::RefPtr<Gio::UnixInputStream> stream)
{
	Glib::RefPtr<Glib::Bytes> buffer;
	const char *ptr;
	size_t nread;

	buffer = stream->read_bytes_finish(result);
	if (buffer.get() == nullptr)
		return;

	ptr = static_cast<const char *>(buffer->get_data(nread));
	if (nread == 0)
		return;

	on_output_produced.emit(std::string(ptr, nread));
	stream->read_bytes_async(BUFFER_SIZE, sigc::bind(sigc::mem_fun(
	    *this, &JtagServer::output_ready), stream));
}

void
JtagServer::child_exited(Glib::Pid pid, int code)
{
	Logger::info("OpenOCD exited with code {} (pid {})", code, pid);
	on_server_exit.emit();
	m_running = false;
}
