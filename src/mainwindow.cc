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

#include <string>
#include <cctype>
#include <gtkmm/dialog.h>
#include <fmt/format.h>
#include <formrow.hh>
#include <utils.hh>
#include <dtb.hh>
#include <deviceselect.hh>
#include <eeprom/24c.hh>
#include <mainwindow.hh>
#include <application.hh>
#include <ucl.h>


#if __cplusplus <= 201703L
#include <experimental/filesystem>
using namespace std::experimental;
#else
#include <filesystem>
using namespace std;
#endif


MainWindow::MainWindow():
    m_profile(this, m_device),
    m_uart_tab(this, m_device),
    m_jtag_tab(this, m_device),
    m_eeprom_tab(this, m_device),
    m_eeprom_tlv_tab(this),
    m_gpio_tab(this, m_device)
{
	set_title("Conclusive developer cable client");
	set_size_request(640, 480);
	set_position(Gtk::WIN_POS_CENTER_ALWAYS);
	m_notebook.append_page(m_profile, "Profile");
	m_notebook.append_page(m_uart_tab, "Serial console");
	m_notebook.append_page(m_jtag_tab, "JTAG");
	m_notebook.append_page(m_eeprom_tab, "EEPROM");
	m_notebook.append_page(m_eeprom_tlv_tab, "EEPROM TLV");
	m_notebook.append_page(m_gpio_tab, "GPIO");
	add(m_notebook);

	set_icon_from_file(fmt::format("{}/icon.png", executable_dir()));
	show_all_children();
	show_deviceselect_dialog();
	
	m_gpio_tab.m_gpio = (std::shared_ptr<Gpio>) m_gpio;
}

MainWindow::~MainWindow() {}


void
MainWindow::set_gpio_name(int no, std::string name)
{
	m_gpio_tab.set_gpio_name(no, name);
}

void
MainWindow::show_deviceselect_dialog()
{
	DeviceSelectDialog dialog;
	dialog.set_position(Gtk::WIN_POS_CENTER_ALWAYS);
	dialog.run();
	if (dialog.get_selected_device().has_value())
		configure_devices(dialog.get_selected_device().value());
}

void
MainWindow::configure_devices(const Device &device)
{
	set_title(fmt::format("{0} {1}", device.description, device.serial));
	m_device = device;

	try {
		m_i2c = new I2C(m_device, 100000);
		m_gpio = new Gpio(m_device);
		m_gpio->set(0);
	} catch (const std::runtime_error &err) {
		show_centered_dialog("Error", err.what());
		exit(1);
	}
}

ProfileTab::ProfileTab(MainWindow *parent, const Device &dev):
    Gtk::Box(Gtk::Orientation::ORIENTATION_VERTICAL),
    m_load("Load profile"),
    m_entry("Loaded profile"),
    m_parent(parent),
    m_device(dev)
{
	m_load.signal_clicked().connect(sigc::mem_fun(*this, &ProfileTab::clicked));
	m_entry.get_widget().set_editable(false);
	m_entry.get_widget().set_text("<none>");
	pack_start(m_entry, false, false);
	pack_end(m_load, false, false);
}



/* "load profile" button maintenance */
void
ProfileTab::clicked()
{
	int res;
	std::string fname;
	ucl_parser *parser;
	const ucl_object_t *root, *uart, *jtag, *device;
	const ucl_object_t *baud, *uart_ip, *uart_port;
	const ucl_object_t *jtag_ip, *gdb_port, *telnet_port, *jtag_script;
	const ucl_object_t *gpio, *name0, *name1, *name2, *name3;
	std::string uart_listen_addr;
//	uint32_t baudrate_value;


	Gtk::FileChooserDialog d_file("Choose profile");

	d_file.add_button("Select", Gtk::RESPONSE_OK);
	d_file.add_button("Cancel", Gtk::RESPONSE_CANCEL);

	res = d_file.run();

	switch (res) {
		case Gtk::RESPONSE_OK:
			fname = d_file.get_filename();
			break;
		case Gtk::RESPONSE_CANCEL:
			fname.clear();
			break;
	}

	parser = ucl_parser_new(0);
	if (!ucl_parser_add_file(parser, fname.c_str())) {
		Gtk::MessageDialog *error_dialog = new Gtk::MessageDialog("Error loading profile file", false);
		error_dialog->set_title("Error");
		error_dialog->set_secondary_text(ucl_parser_get_error(parser));
		error_dialog->run();
		delete error_dialog;
		return;
	}

	root = ucl_parser_get_object(parser);
	device =  ucl_object_lookup(root, "device");

	/* parse UART */
	uart = ucl_object_lookup(device, "uart");
	baud = ucl_object_lookup(uart, "baudrate");
//	baudrate_value = ucl_object_toint(baud);
	uart_ip = ucl_object_lookup(uart, "listen_ip");
	uart_port = ucl_object_lookup(uart, "listen_port");

	/* parse JTAG */
	jtag = ucl_object_lookup(device, "jtag");
	jtag_ip = ucl_object_lookup(jtag, "listen_ip");
	gdb_port = ucl_object_lookup(jtag, "gdb_port");
	telnet_port = ucl_object_lookup(jtag, "telnet_port");
//	pass_through = ucl_object_lookup(jtag, "pass_through");
	jtag_script = ucl_object_lookup(jtag, "script");

	/* parse GPIO */
	gpio = ucl_object_lookup(device, "gpio");
	name0 = ucl_object_lookup(gpio, "gpio0");
	name1 = ucl_object_lookup(gpio, "gpio1");
	name2 = ucl_object_lookup(gpio, "gpio2");
	name3 = ucl_object_lookup(gpio, "gpio3");

	/* set UART parameters */
	m_parent->set_uart_addr(ucl_object_tostring(uart_ip));
	m_parent->set_uart_port(std::to_string(ucl_object_toint(uart_port)));
	m_parent->set_uart_baud(std::to_string(ucl_object_toint(baud)));

	/* set JTAG parameters */
	m_parent->set_jtag_addr(ucl_object_tostring(jtag_ip));
	m_parent->set_jtag_ocd_port(std::to_string(ucl_object_toint(telnet_port)));
	m_parent->set_jtag_gdb_port(std::to_string(ucl_object_toint(gdb_port)));
	m_parent->set_jtag_script(ucl_object_tostring(jtag_script));

	/* set GPIO parameters */
	m_parent->set_gpio_name(0, ucl_object_tostring(name0));
	m_parent->set_gpio_name(1, ucl_object_tostring(name1));
	m_parent->set_gpio_name(2, ucl_object_tostring(name2));
	m_parent->set_gpio_name(3, ucl_object_tostring(name3));

	/* show file name in profile tab */
	m_entry.get_widget().set_text(filesystem::path(fname).filename().c_str());
}

SerialTab::SerialTab(MainWindow *parent, const Device &dev):
    Gtk::Box(Gtk::Orientation::ORIENTATION_VERTICAL),
    m_address_row("Listen address"),
    m_port_row("Listen port"),
    m_baud_row("Port baud rate"),
    m_status_row("Status"),
    m_label("Connected clients:"),
    m_clients(1),
    m_start("Start"),
    m_stop("Stop"),
    m_terminal("Launch terminal"),
    m_parent(parent),
    m_device(dev)
{
	m_address_row.get_widget().set_text("127.0.0.1");
	m_addr_changed_conn = m_address_row
		.get_widget()
		.signal_changed()
		.connect(sigc::mem_fun(*this, &SerialTab::on_address_changed));
	m_port_row.get_widget().set_text("2222");
	m_port_changed_conn = m_port_row
		.get_widget()
		.signal_changed()
		.connect(sigc::mem_fun(*this, &SerialTab::on_port_changed));
	
	m_status_row.get_widget().set_text("Stopped");
	m_baud_row.get_widget().append("9600");
	m_baud_row.get_widget().append("19200");
	m_baud_row.get_widget().append("38400");
	m_baud_row.get_widget().append("57600");
	m_baud_row.get_widget().append("115200");
	m_baud_row.get_widget().set_active_text("115200");
	m_status_row.get_widget().set_editable(false);
	m_clients.set_column_title(0, "Client address");
	m_scroll.add(m_clients);

	m_start.signal_clicked().connect(sigc::mem_fun(*this,
	    &SerialTab::start_clicked));

	m_stop.signal_clicked().connect(sigc::mem_fun(*this,
	    &SerialTab::stop_clicked));

	m_terminal.signal_clicked().connect(sigc::mem_fun(*this,
	    &SerialTab::launch_terminal_clicked));

	m_buttons.set_border_width(5);
	m_buttons.set_layout(Gtk::ButtonBoxStyle::BUTTONBOX_END);
	m_buttons.pack_start(m_start);
	m_buttons.pack_start(m_stop);
	m_buttons.pack_start(m_terminal);

	set_border_width(5);
	pack_start(m_address_row, false, true);
	pack_start(m_port_row, false, true);
	pack_start(m_baud_row, false, true);
	pack_start(m_status_row, false, true);
	pack_start(m_separator, false, true);
	pack_start(m_label, false, true);
	pack_start(m_scroll, true, true);
	pack_start(m_buttons, false, true);
}

void
SerialTab::start_clicked()
{
	Glib::RefPtr<Gio::SocketAddress> addr;
	int baud;

	if (m_uart)
		return;

	baud = std::stoi(m_baud_row.get_widget().get_active_text());
	addr = Gio::InetSocketAddress::create(
	    Gio::InetAddress::create(m_address_row.get_widget().get_text()),
	    std::stoi(m_port_row.get_widget().get_text()));

	try {
		m_uart = std::make_shared<Uart>(m_device, addr, baud);
		m_uart->m_connected.connect(sigc::mem_fun(*this,
		    &SerialTab::client_connected));
		m_uart->m_disconnected.connect(sigc::mem_fun(*this,
		    &SerialTab::client_disconnected));
		m_uart->start();
		m_status_row.get_widget().set_text("Running");
	} catch (const std::runtime_error &err) {
		show_centered_dialog("Error", err.what());
	}
}

void
SerialTab::stop_clicked()
{
	if (!m_uart)
		return;

	m_uart.reset();
	m_status_row.get_widget().set_text("Stopped");
}

void
SerialTab::launch_terminal_clicked()
{
#if defined(__APPLE__) && defined(__MACH__)
	std::vector<std::string> argv {
		"osascript",
		"-e",
		fmt::format(
			"tell app \"Terminal\" to do script "
   			"\"telnet 127.0.0.1 {}\"",
   			m_port_row.get_widget().get_text())
	};
#elif defined(__unix__)
	std::vector<std::string> argv {
		"x-terminal-emulator",
		"-e",
		fmt::format(
			"telnet 127.0.0.1 {}",
			m_port_row.get_widget().get_text())
	};
#endif

#if !defined(__APPLE__) && !defined(__unix__)
	Gtk::MessageDialog dialog(
		*this, "Error", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK);
	dialog.set_secondary_text(
		"Launching a terminal is unimplemented for your platform.");
	dialog.run();
#else
	Glib::spawn_async("/", argv, Glib::SpawnFlags::SPAWN_SEARCH_PATH);
#endif
}

void
SerialTab::client_connected(Glib::RefPtr<Gio::SocketAddress> addr)
{
	m_clients.append(addr->to_string());
}

void
SerialTab::client_disconnected(Glib::RefPtr<Gio::SocketAddress> addr)
{
	Glib::RefPtr<Gtk::TreeModel> model = m_clients.get_model();
	Glib::ustring addr_s = addr->to_string();
	Gtk::ListStore *store;

	store = dynamic_cast<Gtk::ListStore *>(model.get());
	store->foreach_iter([=] (const Gtk::TreeIter &it) -> bool {
		Glib::ustring result;

		it->get_value(0, result);
		if (result == addr_s)
			store->erase(it);
		return true;
	});
}

void SerialTab::on_address_changed()
{
	Glib::ustring output;
	for (const unsigned int &c : m_address_row.get_widget().get_text())
		if (isdigit((char)c) || c == 0x2E) output += c;
	m_addr_changed_conn.block();
	m_address_row.get_widget().set_text(output);
	m_addr_changed_conn.unblock();
}

void SerialTab::set_address(std::string addr)
{
	m_address_row.get_widget().set_text(addr);
}

void SerialTab::set_port(std::string port)
{
	m_port_row.get_widget().set_text(port);
}

void SerialTab::set_baud(std::string baud)
{
	m_baud_row.get_widget().set_active_text(baud);
}

void SerialTab::on_port_changed()
{
	Glib::ustring output;
	for (const unsigned int &c : m_port_row.get_widget().get_text())
		if (isdigit((char)c)) output += c;
	m_port_changed_conn.block();
	m_port_row.get_widget().set_text(output);
	m_port_changed_conn.unblock();
}

JtagTab::JtagTab(MainWindow *parent, const Device &dev):
    Gtk::Box(Gtk::Orientation::ORIENTATION_VERTICAL),
    m_address_row("Listen address"),
    m_gdb_port_row("GDB server listen port"),
    m_ocd_port_row("OpenOCD listen port"),
    m_board_row("Board init script"),
    m_status_row("Status"),
    m_start("Start"),
    m_stop("Stop"),
    m_reset("Reset target"),
    m_bypass("J-Link bypass mode"),
    m_parent(parent),
    m_device(dev)
{
	Pango::FontDescription font("Monospace 9");

	m_address_row.get_widget().set_text("127.0.0.1");
	m_addr_changed_conn = m_address_row
	    .get_widget()
	    .signal_changed()
	    .connect(sigc::mem_fun(*this, &JtagTab::on_address_changed));
	
	m_ocd_port_row.get_widget().set_text("4444");
	m_ocd_port_changed_conn = m_ocd_port_row
	    .get_widget()
	    .signal_changed()
	    .connect(sigc::mem_fun(*this, &JtagTab::on_ocd_port_changed));
	
	m_gdb_port_row.get_widget().set_text("3333");
	m_gdb_port_changed_conn = m_gdb_port_row
	    .get_widget()
	    .signal_changed()
	    .connect(sigc::mem_fun(*this, &JtagTab::on_gdb_port_changed));
	
	m_status_row.get_widget().set_text("Stopped");

	m_textbuffer = Gtk::TextBuffer::create();
	m_textview.set_editable(false);
	m_textview.set_buffer(m_textbuffer);
	m_textview.set_wrap_mode(Gtk::WrapMode::WRAP_WORD);
	m_textview.override_font(font);
	m_scroll.add(m_textview);

	m_buttons.set_border_width(5);
	m_buttons.set_layout(Gtk::ButtonBoxStyle::BUTTONBOX_END);
	m_buttons.pack_start(m_start);
	m_buttons.pack_start(m_stop);
	m_buttons.pack_start(m_reset);
	m_buttons.pack_start(m_bypass);

	m_start.signal_clicked().connect(sigc::mem_fun(*this,
	    &JtagTab::start_clicked));
	m_stop.signal_clicked().connect(sigc::mem_fun(*this,
	    &JtagTab::stop_clicked));
	m_reset.signal_clicked().connect(sigc::mem_fun(*this,
	    &JtagTab::reset_clicked));
	m_bypass.signal_clicked().connect(sigc::mem_fun(*this,
	    &JtagTab::bypass_clicked));

	set_border_width(5);
	pack_start(m_address_row, false, true);
	pack_start(m_gdb_port_row, false, true);
	pack_start(m_ocd_port_row, false, true);
	pack_start(m_board_row, false, true);
	pack_start(m_status_row, false, true);
	pack_start(m_scroll, true, true);
	pack_start(m_buttons, false, true);
}

void
JtagTab::start_clicked()
{
	Glib::RefPtr<Gio::InetAddress> addr;

	addr = Gio::InetAddress::create(m_address_row.get_widget().get_text());

	m_textbuffer->set_text("");
	m_server = std::make_shared<JtagServer>(m_device, addr,
	    std::stoi(m_gdb_port_row.get_widget().get_text()),
	    std::stoi(m_ocd_port_row.get_widget().get_text()),
	    m_board_row.get_widget().get_filename());
	
	m_server->on_output_produced.connect(
	    sigc::mem_fun(*this, &JtagTab::on_output_ready));
	m_server->on_server_start.connect(
	    sigc::mem_fun(*this, &JtagTab::on_server_start));
	m_server->on_server_exit.connect(
	    sigc::mem_fun(*this, &JtagTab::on_server_exit));

	try {
		m_server->start();
	} catch (const std::runtime_error &err) {
		show_centered_dialog("Failed to start JTAG server.",
		    err.what());
	}
}

void
JtagTab::stop_clicked()
{
	if (m_server)
	{
		m_server->stop();
		m_server.reset();
	}
}

void
JtagTab::reset_clicked()
{
	JtagServer::reset(m_device);
}

void
JtagTab::bypass_clicked()
{
	JtagServer::bypass(m_device);
}

void
JtagTab::on_output_ready(const std::string &output)
{
	Glib::ustring text = m_textbuffer->get_text();
	
	text += output;
	m_textbuffer->set_text(text);
	m_textview.scroll_to(m_textbuffer->get_insert());
}

void
JtagTab::on_server_start()
{
	m_status_row.get_widget().set_text("Running");
	m_reset.set_sensitive(false);
	m_bypass.set_sensitive(false);
}

void
JtagTab::on_server_exit()
{
	m_status_row.get_widget().set_text("Stopped");
	m_reset.set_sensitive(true);
	m_bypass.set_sensitive(true);
}

void JtagTab::on_address_changed()
{
	Glib::ustring output;

	for (const unsigned int &c: m_address_row.get_widget().get_text()) {
		if (isdigit((char) c) || c == 0x2E)
			output += c;
	}

	m_addr_changed_conn.block();
	m_address_row.get_widget().set_text(output);
	m_addr_changed_conn.unblock();
}

void JtagTab::on_ocd_port_changed()
{
	Glib::ustring output;

	for (const unsigned int &c: m_ocd_port_row.get_widget().get_text()) {
		if (isdigit((char) c))
			output += c;
	}

	m_ocd_port_changed_conn.block();
	m_ocd_port_row.get_widget().set_text(output);
	m_ocd_port_changed_conn.unblock();
}

void JtagTab::on_gdb_port_changed()
{
	Glib::ustring output;

	for (const unsigned int &c: m_gdb_port_row.get_widget().get_text()) {
		if (isdigit((char)c))
			output += c;
	}
	m_gdb_port_changed_conn.block();
	m_gdb_port_row.get_widget().set_text(output);
	m_gdb_port_changed_conn.unblock();
}

void JtagTab::set_address(std::string addr)
{
	m_address_row.get_widget().set_text(addr);
}

void JtagTab::set_ocd_port(std::string port)
{
	m_ocd_port_row.get_widget().set_text(port);
}

void JtagTab::set_gdb_port(std::string port)
{
	m_gdb_port_row.get_widget().set_text(port);
}

void JtagTab::set_script(std::string script)
{
	m_board_row.get_widget().set_filename(script);
}

EepromTab::EepromTab(MainWindow *parent, const Device &dev):
	Gtk::Box(Gtk::Orientation::ORIENTATION_VERTICAL),
	m_read("Read"),
	m_write("Write"),
	m_save("Save buffer to file"),
	m_parent(parent),
	m_device(dev)
{
	Pango::FontDescription font("Monospace 9");

	m_textbuffer = Gtk::TextBuffer::create();
	m_textview.set_buffer(m_textbuffer);
	m_textview.override_font(font);
	m_scroll.add(m_textview);

	m_buttons.set_border_width(5);
	m_buttons.set_layout(Gtk::ButtonBoxStyle::BUTTONBOX_END);
	m_buttons.pack_start(m_read);
	m_buttons.pack_start(m_write);
	m_buttons.pack_start(m_save);

	m_textbuffer->set_text(
	    "/dts-v1/;\n"
	    "/ {\n"
	    "\tmodel = \"UNNAMED\";\n"
	    "\tserial = \"INVALID\";\n"
	    "\tethaddr-eth0 = [00 00 00 00 00 00];\n"
	    "};");

	m_read.signal_clicked().connect(sigc::mem_fun(*this,
	    &EepromTab::read_clicked));
	m_write.signal_clicked().connect(sigc::mem_fun(*this,
	    &EepromTab::write_clicked));

	set_border_width(5);
	pack_start(m_scroll, true, true);
	pack_start(m_buttons, false, true);
}

void
EepromTab::write_clicked()
{
	m_textual = std::make_shared<std::string>(m_textbuffer->get_text());
	m_blob = std::make_shared<std::vector<uint8_t>>();
	m_dtb = std::make_shared<DTB>(m_textual, m_blob);

	try {
		m_dtb->compile(sigc::mem_fun(*this, &EepromTab::compile_done));
	} catch (const std::runtime_error &err) {
		Gtk::MessageDialog msg("Write error");

		msg.set_secondary_text(err.what());
		msg.run();
	}
}

void
EepromTab::read_clicked()
{
	Eeprom24c eeprom(*m_parent->m_i2c);

	m_textual = std::make_shared<std::string>();
	m_blob = std::make_shared<std::vector<uint8_t>>();
	m_dtb = std::make_shared<DTB>(m_textual, m_blob);

	try {
		eeprom.read(0, 4096, *m_blob);
		m_dtb->decompile(sigc::mem_fun(*this,
		    &EepromTab::decompile_done));
	} catch (const std::runtime_error &err) {
		Gtk::MessageDialog msg("Read error");

		msg.set_secondary_text(err.what());
		msg.run();
	}
}


void
EepromTab::compile_done(bool ok, int size, const std::string &errors)
{
	if (ok) {
		Gtk::MessageDialog dlg(*m_parent, fmt::format(
		    "Compilation and flashing done (size: {} bytes)", size));

		Eeprom24c eeprom(*m_parent->m_i2c);
		eeprom.write(0, *m_blob);
		dlg.run();
	} else {
		Gtk::MessageDialog dlg(*m_parent, "Compile errors!");

		dlg.set_secondary_text(fmt::format("<tt>{}</tt>",
		    Glib::Markup::escape_text(errors)), true);
		dlg.run();
	}
}

void
EepromTab::decompile_done(bool ok, int size, const std::string &errors)
{
	if (ok) {
		Gtk::MessageDialog dlg(*m_parent, fmt::format(
		    "Reading done (size: {} bytes)", size));

		dlg.run();
		m_textbuffer->set_text(*m_textual);
	} else {
		Gtk::MessageDialog dlg(*m_parent, "Read errors!");

		dlg.set_secondary_text(fmt::format("<tt>{}</tt>",
		    Glib::Markup::escape_text(errors)), true);
		dlg.run();
	}
}
EepromTLVTab::EepromTLVTab(MainWindow *parent):
		Gtk::Box(Gtk::Orientation::ORIENTATION_VERTICAL),
		m_read("Read"),
		m_write("Write"),
		m_clear("Clear EEPROM"),
		m_parent(parent)
{
	Pango::FontDescription font("Monospace 9");

	m_list_store_ref = Gtk::ListStore::create(m_model_columns);
	m_tlv_records.set_model(m_list_store_ref);

	m_tlv_records.append_column("Id", m_model_columns.m_id);
	m_tlv_records.append_column("Name", m_model_columns.m_name);
	m_tlv_records.append_column_editable("Value", m_model_columns.m_value);

	m_add_row(TLV_CODE_PRODUCT_NAME, "Product name", "set-me-sample-name");
	m_add_row(TLV_CODE_PART_NUMBER, "Part number", "");
	m_add_row(TLV_CODE_SERIAL_NUMBER, "Serial number", "000000");
	m_add_row(TLV_CODE_MAC_BASE, "MAC", "70:B3:D5:B9:D0:00");
	m_add_row(TLV_CODE_MANUF_DATE, "Manufacture date", "01/01/2021 12:00:01");
	m_add_row(TLV_CODE_DEV_VERSION, "Device version", "1");
	m_add_row(TLV_CODE_LABEL_REVISION, "Label revision", "");
	m_add_row(TLV_CODE_PLATFORM_NAME, "Platform name", "");
	m_add_row(TLV_CODE_ONIE_VERSION, "ONIE version", "1");
	m_add_row(TLV_CODE_NUM_MACs, "Number MACs", "1");
	m_add_row(TLV_CODE_MANUF_NAME, "Manufacturer", "Conclusive Engineering");
	m_add_row(TLV_CODE_COUNTRY_CODE, "Country code", "PL");
	m_add_row(TLV_CODE_VENDOR_NAME, "Vendor", "");
	m_add_row(TLV_CODE_DIAG_VERSION, "Diag Version", "");
	m_add_row(TLV_CODE_SERVICE_TAG, "Service tag", "");

	m_scroll.add(m_tlv_records);

	m_buttons.set_border_width(5);
	m_buttons.set_layout(Gtk::ButtonBoxStyle::BUTTONBOX_END);
	m_buttons.pack_start(m_read);
	m_buttons.pack_start(m_write);
	m_buttons.pack_start(m_clear);

	set_border_width(5);
	pack_start(m_scroll, true, true);
	pack_start(m_buttons, false, true);

	m_read.signal_clicked().connect(sigc::mem_fun(*this, &EepromTLVTab::read_clicked));
	m_write.signal_clicked().connect(sigc::mem_fun(*this, &EepromTLVTab::write_clicked));
	m_clear.signal_clicked().connect(sigc::mem_fun(*this, &EepromTLVTab::clear_clicked));
}

void
EepromTLVTab::write_clicked()
{
	uint8_t eeprom_file[2048];

	for (auto row: m_list_store_ref->children())
	{
		int parsed_number;
		uint8_t field_id = row.get_value(m_model_columns.m_id);
		Glib::ustring field_value = row.get_value(m_model_columns.m_value);
		switch (field_id) {
			case TLV_CODE_DEV_VERSION:
				parsed_number = m_parse_user_number(field_value, 0, 255, "Device version");
				if (parsed_number == -1)
					return;
				otlv.save_user_tlv(field_id, reinterpret_cast<char*>(&parsed_number));
			break;
			case TLV_CODE_NUM_MACs:
				parsed_number = m_parse_user_number(field_value, 1, 65535, "MAC number");
				if (parsed_number == -1)
					return;
				otlv.save_user_tlv(field_id, std::to_string(parsed_number).c_str());
			break;
			case TLV_CODE_COUNTRY_CODE:
				if (field_value.length() != 2) {
					m_show_error_dialog("ERROR: Country code must 2 characters only. Example: PL.\n"
						 "Data will not be saved.");
					return;
				}
				otlv.save_user_tlv(field_id, field_value.substr(0,2).c_str());
				break;
			case TLV_CODE_MANUF_DATE:
				field_value = field_value.substr(0,19);
				if (!otlv.validate_date(field_value.c_str())) {
					m_show_error_dialog("ERROR: Invalid date vale. Required format is: MM/DD/YYYY hh:mm:ss.\n"
						 "Data will not be saved.");
					return;
				}
				otlv.save_user_tlv(field_id, field_value.c_str());
				break;
			case TLV_CODE_MAC_BASE:
				field_value = field_value.substr(0,17);
				if (!otlv.validate_mac_address(field_value.c_str())) {
					m_show_error_dialog("ERROR: Invalid MAC address. Required format is: xx:xx:xx:xx:xx.\n"
						 "Data will not be saved.");
					return;
				}
				otlv.save_user_tlv(field_id, field_value.c_str());
				break;
			default:
				if (!field_value.empty())
					otlv.save_user_tlv(field_id, field_value.substr(0,TLV_EEPROM_VALUE_MAX_SIZE-1).c_str());
		}
//		Logger::debug("Filed id {} value: {}", field_id, field_value);
	}

	otlv.generate_eeprom_file(eeprom_file);
	m_blob = std::make_shared<std::vector<uint8_t>>(eeprom_file, eeprom_file+otlv.get_usage());
	Eeprom24c eeprom(*m_parent->m_i2c);
	eeprom.write(0, *m_blob);
}

void
EepromTLVTab::read_clicked()
{
	Eeprom24c eeprom(*m_parent->m_i2c);

	m_blob = std::make_shared<std::vector<uint8_t>>();
	try {
		eeprom.read(0, 2048, *m_blob);
		otlv.load_eeprom_file(m_blob->data());
	} catch (const std::runtime_error &err) {
		m_show_error_dialog("Error while trying to read EEPROM.");
		return;
	}

	for (auto tlv: otlv.TEXT_TLV) {
		char otlv_string[2048];
		if (otlv.get_string_record(tlv, otlv_string))
			m_update_row(tlv, otlv_string);
	}

	for (auto tlv: otlv.NUMERIC_TLV) {
		uint32_t otlv_number;
		if (otlv.get_numeric_record(tlv, &otlv_number))
			m_update_row(tlv, std::to_string(otlv_number));
	}

	char otlv_mac[255];
	if (otlv.get_mac_record(otlv_mac))
		m_update_row(TLV_CODE_MAC_BASE, otlv_mac);
}

void
EepromTLVTab::clear_clicked()
{
	uint8_t eeprom_file[2048];
	memset(eeprom_file, '0', 2048);
	m_blob = std::make_shared<std::vector<uint8_t>>(eeprom_file, eeprom_file+2048);
	Eeprom24c eeprom(*m_parent->m_i2c);
	eeprom.write(0, *m_blob);
}

GpioTab::GpioTab(MainWindow *parent, const Device &dev):
    Gtk::Box(Gtk::Orientation::ORIENTATION_VERTICAL),
    m_device(dev),
    m_gpio_row {
	FormRowGpio("GPIO 0"),
	FormRowGpio("GPIO 1"),
	FormRowGpio("GPIO 2"),
	FormRowGpio("GPIO 3")
    }
{
	set_border_width(10);

	for (int i = 0; i < 4; i++) {
		m_gpio_row[i].direction_changed().connect(sigc::bind(sigc::mem_fun(
		    *this, &GpioTab::direction_changed), 1 << i));
		m_gpio_row[i].state_changed().connect(sigc::bind(sigc::mem_fun(
		    *this, &GpioTab::state_changed), 1 << i));
		pack_start(m_gpio_row[i], false, true);
	}

	m_timer = Glib::signal_timeout().connect(sigc::mem_fun(*this, &GpioTab::timer), 500);
}

GpioTab::~GpioTab() noexcept
{
	m_timer.disconnect();
}

void
GpioTab::state_changed(bool state, uint8_t mask)
{
	uint8_t val;

	val = state
	    ? m_gpio->get() | mask
	    : m_gpio->get() & ~mask;

	m_gpio->set(val);
}

void
GpioTab::direction_changed(bool output, uint8_t mask)
{
	uint8_t val;

	val = output
	      ? m_gpio->get_direction() | mask
	      : m_gpio->get_direction() & ~mask;

	m_gpio->configure(val);
}

bool
GpioTab::timer()
{
	uint8_t val;
	bool state;

	if (m_gpio == nullptr)
		return (true);

	val = m_gpio->get();
	for (int i = 0; i < 4; i++) {
		state = val & (1 << i);
		if (m_gpio_row[i].get_state() != state)
			m_gpio_row[i].set_state(state);
	}

	return (true);
}

void
GpioTab::set_gpio_name(int no, const std::string &name)
{

}

void MainWindow::set_uart_addr(std::string addr)
{
	m_uart_tab.set_address(addr);
}

void MainWindow::set_uart_port(std::string port)
{
	m_uart_tab.set_port(port);
}

void MainWindow::set_uart_baud(std::string baud)
{
	m_uart_tab.set_baud(baud);
}

void MainWindow::set_jtag_addr(std::string addr)
{
	m_jtag_tab.set_address(addr);
}

void MainWindow::set_jtag_ocd_port(std::string port)
{
	m_jtag_tab.set_ocd_port(port);
}

void MainWindow::set_jtag_gdb_port(std::string port)
{
	m_jtag_tab.set_gdb_port(port);
}

void MainWindow::set_jtag_script(std::string script)
{
	m_jtag_tab.set_script(script);
}
