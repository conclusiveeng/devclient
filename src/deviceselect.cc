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

#include <fmt/format.h>
#include <gtkmm.h>
#include <device.hh>
#include <deviceselect.hh>
#include <application.hh>
#include <utils.hh>

DeviceSelectDialog::DeviceSelectDialog():
    Gtk::Dialog("Select device", true),
    m_ok(Gtk::Stock::OK)
{
	set_position(Gtk::WIN_POS_CENTER);
	
	m_store = Gtk::ListStore::create(m_columns);
	m_treeview.set_model(m_store);
	m_treeview.append_column("VID", m_columns.m_vid);
	m_treeview.append_column("PID", m_columns.m_pid);
	m_treeview.append_column("Description", m_columns.m_description);
	m_treeview.append_column("Serial", m_columns.m_serial);

	for (const auto &i: DeviceEnumerator::enumerate()) {
		auto row = *(m_store->append());
		row[m_columns.m_vid] = fmt::format("{:#04x}", i.vid);
		row[m_columns.m_pid] = fmt::format("{:#04x}", i.pid);
		row[m_columns.m_description] = i.description;
		row[m_columns.m_serial] = i.serial;
		row[m_columns.m_device] = i;
	}

	m_ok.signal_clicked().connect(sigc::mem_fun(*this,
	    &DeviceSelectDialog::ok_clicked));
	
	get_content_area()->add(m_treeview);
	get_action_area()->add(m_ok);
	set_size_request(400, 300);
	set_icon_from_file(fmt::format("{}/icon.png", executable_dir()));
	show_all_children();
}

std::optional<Device>
DeviceSelectDialog::get_selected_device()
{
	Glib::RefPtr<Gtk::TreeSelection> sel;
	Gtk::TreeModel::iterator it;

	sel = m_treeview.get_selection();
	if (!sel)
		return (std::nullopt);

	it = sel->get_selected();
	if (!it)
		return (std::nullopt);

	return (std::make_optional((*it)[m_columns.m_device]));
}

void
DeviceSelectDialog::ok_clicked()
{
	response(Gtk::ResponseType::RESPONSE_OK);
}

DeviceSelectDialog::ModelColumns::ModelColumns()
{
	add(m_vid);
	add(m_pid);
	add(m_description);
	add(m_serial);
	add(m_device);
}
