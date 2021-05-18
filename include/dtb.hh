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

#ifndef DEVCLIENT_DTB_HH
#define DEVCLIENT_DTB_HH

#include <vector>
#include <string>
#include <condition_variable>

class DTB
{
public:
	using SlotDone = sigc::slot<void, bool, int, std::string>;

	DTB(std::shared_ptr<std::string> &dts, std::shared_ptr<std::vector<uint8_t>> &dtb);
	virtual ~DTB();
	void compile(const SlotDone &done);
	void decompile(const SlotDone &done);

protected:
	void run_dtc(bool compile, const SlotDone &done);
	void output_ready(Glib::RefPtr<Gio::AsyncResult> &result);
	void error_ready(Glib::RefPtr<Gio::AsyncResult> &result);
	void child_exited(Glib::Pid pid, int code);

	// Glib::RefPtr<Gio::UnixOutputStream> m_in;
	// Glib::RefPtr<Gio::UnixInputStream> m_out;
	// Glib::RefPtr<Gio::UnixInputStream> m_err;
	std::shared_ptr<std::vector<uint8_t>> m_dtb;
	std::shared_ptr<std::string> m_dts;
	std::string m_errors;
	SlotDone m_done;
	bool m_compile;
};

#endif //DEVCLIENT_DTB_HH
