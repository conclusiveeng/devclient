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
#include <string>
#include <giomm.h>
#include <log.hh>
#include <utils.hh>
#include <dtb.hh>

#define BUFFER_SIZE	1024

DTB::DTB( std::shared_ptr<std::string> &dts,
    std::shared_ptr<std::vector<uint8_t>> &dtb):
    m_dtb(dtb),
    m_dts(dts)
{
}

DTB::~DTB()
{
}

void DTB::run_dtc(bool compile, const SlotDone &done)
{
	int stdin_fd;
	int stdout_fd;
	int stderr_fd;
	Glib::Pid pid;

	m_done = done;
	m_compile = compile;

	std::string errors;
	std::vector<std::string> argv {
		"dtc",
		"-I", compile ? "dts" : "dtb",
		"-O", compile ? "dtb" : "dts"
	};

	try {
		Glib::spawn_async_with_pipes("/tmp", argv,
		    Glib::SpawnFlags::SPAWN_DO_NOT_REAP_CHILD |
		    Glib::SpawnFlags::SPAWN_SEARCH_PATH,
		    Glib::SlotSpawnChildSetup(),
		    &pid, &stdin_fd, &stdout_fd, &stderr_fd);
	} catch (const Glib::Error &err) {
		throw std::runtime_error(err.what());
	}

	// m_in = Gio::UnixOutputStream::create(stdin_fd, true);
	// m_out = Gio::UnixInputStream::create(stdout_fd, true);
	// m_err = Gio::UnixInputStream::create(stderr_fd, true);

	// m_out->read_bytes_async(BUFFER_SIZE,
	//     sigc::mem_fun(*this, &DTB::output_ready));

	// m_err->read_bytes_async(BUFFER_SIZE,
	//     sigc::mem_fun(*this, &DTB::error_ready));

	// Glib::signal_child_watch().connect(
	//     sigc::mem_fun(*this, &DTB::child_exited),
	//     pid);

	// if (compile)
	// 	m_in->write(*m_dts);
	// else
	// 	m_in->write(m_dtb->data(), m_dtb->size());

	// m_in->flush();
	// m_in->close();
}

void
DTB::compile(const DTB::SlotDone &done)
{
	run_dtc(true, done);
}

void
DTB::decompile(const DTB::SlotDone &done)
{
	run_dtc(false, done);
}

void
DTB::output_ready(Glib::RefPtr<Gio::AsyncResult> &result)
{
	// Glib::RefPtr<Glib::Bytes> data;
	// const uint8_t *buffer;
	// size_t len;

	// data = m_out->read_bytes_finish(result);
	// buffer = static_cast<const uint8_t *>(data->get_data(len));

	// if (len == 0)
	// 	return;

	// if (m_compile)
	// 	m_dtb->insert(m_dtb->end(), &buffer[0], &buffer[len]);
	// else
	// 	m_dts->insert(m_dts->end(), &buffer[0], &buffer[len]);

	// m_out->read_bytes_async(BUFFER_SIZE,
	//     sigc::mem_fun(*this, &DTB::output_ready));
}

void
DTB::error_ready(Glib::RefPtr<Gio::AsyncResult> &result)
{
	// Glib::RefPtr<Glib::Bytes> data;
	// const uint8_t *buffer;
	// size_t len;

	// data = m_err->read_bytes_finish(result);
	// buffer = static_cast<const uint8_t *>(data->get_data(len));

	// if (len == 0)
	// 	return;

	// m_errors.insert(m_errors.end(), &buffer[0], &buffer[len]);
	// m_err->read_bytes_async(BUFFER_SIZE,
	//     sigc::mem_fun(*this, &DTB::error_ready));
}

void
DTB::child_exited(Glib::Pid pid, int code)
{
	Logger::debug("Child exited, status: {}", code);
	m_done(code == 0, m_compile ? m_dtb->size() : m_dts->size(), m_errors);
}
