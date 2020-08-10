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
    m_uart_tab(this, m_device),
    m_jtag_tab(this, m_device),
    m_eeprom_tab(this, m_device),
    m_gpio_tab(this, m_device),
    m_profile(this, m_device)
{
	set_title("Conclusive developer cable client");
	set_size_request(640, 480);
	set_position(Gtk::WIN_POS_CENTER_ALWAYS);
	m_notebook.append_page(m_profile, "Profile");
	m_notebook.append_page(m_uart_tab, "Serial console");
	m_notebook.append_page(m_jtag_tab, "JTAG");
	m_notebook.append_page(m_eeprom_tab, "EEPROM");
	m_notebook.append_page(m_gpio_tab, "GPIO");
	add(m_notebook);
	
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
    m_device(dev),
    m_load("Load profile"),
    m_parent(parent),
    m_entry("Loaded profile")
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
	const ucl_object_t *root, *uart, *jtag, *device, *serial;
	const ucl_object_t *baud, *uart_ip, *uart_port;
	const ucl_object_t *jtag_ip, *gdb_port, *telnet_port, *pass_through, *jtag_script;
	const ucl_object_t *gpio, *name0, *name1, *name2, *name3;
	std::string uart_listen_addr;
	uint32_t baudrate_value;


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
    m_device(dev),
    m_address_row("Listen address"),
    m_port_row("Listen port"),
    m_baud_row("Port baud rate"),
    m_status_row("Status"),
    m_clients(1),
    m_start("Start"),
    m_stop("Stop"),
    m_terminal("Launch terminal"),
    m_label("Connected clients:"),
    m_parent(parent)
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
    m_device(dev),
    m_address_row("Listen address"),
    m_gdb_port_row("GDB server listen port"),
    m_ocd_port_row("OpenOCD listen port"),
    m_board_row("Board init script"),
    m_status_row("Status"),
    m_start("Start"),
    m_stop("Stop"),
    m_reset("Reset target"),
    m_bypass("J-Link bypass mode"),
    m_parent(parent)
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
	m_device(dev),
	m_read("Read"),
	m_write("Write"),
	m_save("Save buffer to file"),
	m_parent(parent)
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

GpioTab::GpioTab(MainWindow *parent, const Device &dev):
    Gtk::Box(Gtk::Orientation::ORIENTATION_VERTICAL),
    m_gpio0_row("GPIO 0"),
    m_gpio1_row("GPIO 1"),
    m_gpio2_row("GPIO 2"),
    m_gpio3_row("GPIO 3"),
    m_device(dev)
{
	set_border_width(10);
	m_gpio0_row.get_widget().set_label("off");
	m_gpio1_row.get_widget().set_label("off");
	m_gpio2_row.get_widget().set_label("off");
	m_gpio3_row.get_widget().set_label("off");

	m_gpio0_row.image.set_from_icon_name("gtk-no", Gtk::ICON_SIZE_BUTTON);
	m_gpio1_row.image.set_from_icon_name("gtk-no", Gtk::ICON_SIZE_BUTTON);
	m_gpio2_row.image.set_from_icon_name("gtk-no", Gtk::ICON_SIZE_BUTTON);
	m_gpio3_row.image.set_from_icon_name("gtk-no", Gtk::ICON_SIZE_BUTTON);

	/* connect signal to radio buttons */
	m_gpio0_row.m_radio_in.signal_toggled().connect(sigc::mem_fun(*this, &GpioTab::radio_clicked));
	m_gpio1_row.m_radio_in.signal_toggled().connect(sigc::mem_fun(*this, &GpioTab::radio_clicked));
	m_gpio2_row.m_radio_in.signal_toggled().connect(sigc::mem_fun(*this, &GpioTab::radio_clicked));
	m_gpio3_row.m_radio_in.signal_toggled().connect(sigc::mem_fun(*this, &GpioTab::radio_clicked));

	/* connect signal to on/off buttons */
	m_gpio0_row.get_widget().signal_toggled().connect(sigc::mem_fun(*this, &GpioTab::button_clicked));
	m_gpio1_row.get_widget().signal_toggled().connect(sigc::mem_fun(*this, &GpioTab::button_clicked));
	m_gpio2_row.get_widget().signal_toggled().connect(sigc::mem_fun(*this, &GpioTab::button_clicked));
	m_gpio3_row.get_widget().signal_toggled().connect(sigc::mem_fun(*this, &GpioTab::button_clicked));

	m_gpio0_row.get_widget().set_sensitive(0);
	m_gpio1_row.get_widget().set_sensitive(0);
	m_gpio2_row.get_widget().set_sensitive(0);
	m_gpio3_row.get_widget().set_sensitive(0);

	pack_start(m_gpio0_row, false, true);
	pack_start(m_gpio1_row, false, true);
	pack_start(m_gpio2_row, false, true);
	pack_start(m_gpio3_row, false, true);

	/* set up timer for controlling all the input gpios */
	sigc::slot<bool> my_slot = sigc::bind(sigc::mem_fun(*this, &GpioTab::on_timeout), reference);
	sigc::connection conn = Glib::signal_timeout().connect(my_slot, 100);
}


/* on/off button clicked */
void
GpioTab::button_clicked()
{
	uint8_t val;

	if (m_gpio->io_state & (1 << 0)) {
		if (m_gpio0_row.get_widget().get_active()) {
			m_gpio0_row.get_widget().set_label("on");
			m_gpio0_row.image.set_from_icon_name("gtk-yes", Gtk::ICON_SIZE_BUTTON);
			m_gpio->io_value |= (1 << 0);
		} else {
			m_gpio0_row.get_widget().set_label("off");
			m_gpio0_row.image.set_from_icon_name("gtk-no", Gtk::ICON_SIZE_BUTTON);
			m_gpio->io_value &= ~(1 << 0);
		}
	}

	if (m_gpio->io_state & (1 << 1)) {
		if (m_gpio1_row.get_widget().get_active()) {
			m_gpio1_row.get_widget().set_label("on");
			m_gpio1_row.image.set_from_icon_name("gtk-yes", Gtk::ICON_SIZE_BUTTON);
			m_gpio->io_value |= (1 << 1);
		} else {
			m_gpio1_row.get_widget().set_label("off");
			m_gpio1_row.image.set_from_icon_name("gtk-no", Gtk::ICON_SIZE_BUTTON);
			m_gpio->io_value &= ~(1 << 1);
		}
	}

	if (m_gpio->io_state & (1 << 2)) {
		if (m_gpio2_row.get_widget().get_active()) {
			m_gpio2_row.get_widget().set_label("on");
			m_gpio2_row.image.set_from_icon_name("gtk-yes", Gtk::ICON_SIZE_BUTTON);
			m_gpio->io_value |= (1 << 2);
		} else {
			m_gpio2_row.get_widget().set_label("off");
			m_gpio2_row.image.set_from_icon_name("gtk-no", Gtk::ICON_SIZE_BUTTON);
			m_gpio->io_value &= ~(1 << 2);
		}
	}

	if (m_gpio->io_state & (1 << 3)) {
		if (m_gpio3_row.get_widget().get_active()) {
			m_gpio3_row.get_widget().set_label("on");
			m_gpio3_row.image.set_from_icon_name("gtk-yes", Gtk::ICON_SIZE_BUTTON);
			m_gpio->io_value |= (1 << 3);
		} else {
			m_gpio3_row.get_widget().set_label("off");
			m_gpio3_row.image.set_from_icon_name("gtk-no", Gtk::ICON_SIZE_BUTTON);
			m_gpio->io_value &= ~(1 << 3);
		}
	}

	/* clear those bits of output value "io_value" where there are input pins in "io_state" */
	val = m_gpio->io_value & m_gpio->io_state;

	m_gpio->set(val);
}

/* radio button for selection input or output clicked */
void
GpioTab::radio_clicked()
{
	uint8_t val;

	m_gpio->io_state = 0;
	
	/* check radiobutton0 state - if it is input or output */
	if (m_gpio0_row.m_radio_out.get_active()) {
		m_gpio->io_state |= 1 << 0;			/* set as output */
		m_gpio0_row.get_widget().set_label("off");
		m_gpio0_row.image.set_from_icon_name("gtk-no", Gtk::ICON_SIZE_BUTTON);
	}
	if (m_gpio1_row.m_radio_out.get_active()) {
		m_gpio->io_state |= 1 << 1;
		m_gpio1_row.get_widget().set_label("off");
		m_gpio1_row.image.set_from_icon_name("gtk-no", Gtk::ICON_SIZE_BUTTON);
	}

	if (m_gpio2_row.m_radio_out.get_active()) {
		m_gpio->io_state |= 1 << 2;
		m_gpio2_row.get_widget().set_label("off");
		m_gpio2_row.image.set_from_icon_name("gtk-no", Gtk::ICON_SIZE_BUTTON);
	}

	if (m_gpio3_row.m_radio_out.get_active()) {
		m_gpio->io_state |= 1 << 3;
		m_gpio3_row.get_widget().set_label("off");
		m_gpio3_row.image.set_from_icon_name("gtk-no", Gtk::ICON_SIZE_BUTTON);
	}

	/* configure GPIO basing on io_state */
	m_gpio->configure();

	/* clear those bits of output value "io_value" where there are input pins in "io_state" */
	val = m_gpio->io_value & m_gpio->io_state;

	m_gpio->set(val);

	/* if in given row there is INPUT radiobutton selected - disable it (it will be grayed) */
	m_gpio0_row.get_widget().set_sensitive(m_gpio->io_state & (1 << 0));
	m_gpio1_row.get_widget().set_sensitive(m_gpio->io_state & (1 << 1));
	m_gpio2_row.get_widget().set_sensitive(m_gpio->io_state & (1 << 2));
	m_gpio3_row.get_widget().set_sensitive(m_gpio->io_state & (1 << 3));
}

bool
GpioTab::on_timeout(int param)
{
	unsigned char rxbuf[16];
	int st;


	if (m_gpio == NULL) 
		return true;

	/* read all the GPIO pins */
	st = m_gpio->m_context.read_pins(rxbuf);
	if (st != 0) {
		printf("fail to read st: %d\n", st);
	}

	if (!!(m_gpio->io_state & (1 << 0)) == 0) { /* check if bit0 is configured as input */
		if (rxbuf[0] & (1 << 0)) {
			m_gpio0_row.get_widget().set_label("on");
		} else  {
			m_gpio0_row.get_widget().set_label("off");
		}
	}

	if (!!(m_gpio->io_state & (1 << 1)) == 0) {
		if (rxbuf[0] & (1 << 1)) {
			m_gpio1_row.get_widget().set_label("on");
		} else {
			m_gpio1_row.get_widget().set_label("off");
		}
	}

	if (!!(m_gpio->io_state & (1 << 2)) == 0) {
		if (rxbuf[0] & (1 << 2)) {
			m_gpio2_row.get_widget().set_label("on");
		} else {
			m_gpio2_row.get_widget().set_label("off");
		}
	}

	if (!!(m_gpio->io_state & (1 << 3)) == 0) {
		if (rxbuf[0] & (1 << 3)) {
			m_gpio3_row.get_widget().set_label("on");
		} else {
			m_gpio3_row.get_widget().set_label("off");
		}
	}

	/* set red/green GPIO level state indicators */
	if (rxbuf[0] & (1 << 0)) {
		m_gpio0_row.image.set_from_icon_name("gtk-yes", Gtk::ICON_SIZE_BUTTON);
	} else  {
		m_gpio0_row.image.set_from_icon_name("gtk-no", Gtk::ICON_SIZE_BUTTON);
	}

	if (rxbuf[0] & (1 << 1)) {
		m_gpio1_row.image.set_from_icon_name("gtk-yes", Gtk::ICON_SIZE_BUTTON);
	} else {
		m_gpio1_row.image.set_from_icon_name("gtk-no", Gtk::ICON_SIZE_BUTTON);
	}

	if (rxbuf[0] & (1 << 2)) {
		m_gpio2_row.image.set_from_icon_name("gtk-yes", Gtk::ICON_SIZE_BUTTON);
	} else {
		m_gpio2_row.image.set_from_icon_name("gtk-no", Gtk::ICON_SIZE_BUTTON);
	}

	if (rxbuf[0] & (1 << 3)) {
		m_gpio3_row.image.set_from_icon_name("gtk-yes", Gtk::ICON_SIZE_BUTTON);
	} else {
		m_gpio3_row.image.set_from_icon_name("gtk-no", Gtk::ICON_SIZE_BUTTON);
	}

	return true;
}

void
GpioTab::set_gpio_name(int no, std::string name)
{
	if (no == 0) {
		m_gpio0_row.set_gpio_name(name);
	}

	if (no == 1) {
		m_gpio1_row.set_gpio_name(name);
	}

	if (no == 2) {
		m_gpio2_row.set_gpio_name(name);
	}

	if (no == 3) {
		m_gpio3_row.set_gpio_name(name);
	}
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
