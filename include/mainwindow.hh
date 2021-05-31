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

#ifndef DEVCLIENT_MAINWINDOW_HH
#define DEVCLIENT_MAINWINDOW_HH

#include <gtkmm.h>
#include <formrow.hh>
#include <uart.hh>
#include <jtag.hh>
#include <gpio.hh>
#include <i2c.hh>
#include <dtb.hh>
//#include <profile.hh>
#include <onie_tlv.hh>
#include <profile.hh>

class MainWindow;


class ProfileTab: public Gtk::Box
{
public:
	ProfileTab(MainWindow *parent, const Device &dev);
	std::shared_ptr<Gpio> m_gpio;

protected:
	void clicked();

	Gtk::HBox m_h;
	Gtk::Button m_load;
	FormRow<Gtk::Entry> m_entry;
	
	uint8_t state;
	MainWindow *m_parent;
	const Device &m_device;
};

class SerialTab: public Gtk::Box
{
public:
	SerialTab(MainWindow *parent, const Device &dev);

	void set_address(std::string addr);
	void set_port(std::string port);
	void set_baud(std::string baud);
	
protected:
	void start_clicked();
	void stop_clicked();
	void launch_terminal_clicked();
	void client_connected(Glib::RefPtr<Gio::SocketAddress> addr);
	void client_disconnected(Glib::RefPtr<Gio::SocketAddress> addr);
	void on_address_changed();
	void on_port_changed();

	FormRow<Gtk::Entry> m_address_row;
	FormRow<Gtk::Entry> m_port_row;
	FormRow<Gtk::ComboBoxText> m_baud_row;
	FormRow<Gtk::Entry> m_status_row;
	Gtk::Separator m_separator;
	Gtk::Label m_label;
	Gtk::ScrolledWindow m_scroll;
	Gtk::ListViewText m_clients;
	Gtk::ButtonBox m_buttons;
	Gtk::Button m_start;
	Gtk::Button m_stop;
	Gtk::Button m_terminal;
	
	sigc::connection m_addr_changed_conn;
	sigc::connection m_port_changed_conn;
	
	std::shared_ptr<Uart> m_uart;
	
	MainWindow *m_parent;
	
	const Device &m_device;
};

class JtagTab: public Gtk::Box
{
public:
	JtagTab(MainWindow *parent, const Device &dev);

	void set_address(std::string addr);
	void set_ocd_port(std::string port);
	void set_gdb_port(std::string port);
	void set_script(std::string script);
	
	
protected:
	void start_clicked();
	void stop_clicked();
	void reset_clicked();
	void bypass_clicked();
	
	void on_output_ready(const std::string &output);
	void on_server_start();
	void on_server_exit();
	void on_address_changed();
	void on_ocd_port_changed();
	void on_gdb_port_changed();
	
	FormRow<Gtk::Entry> m_address_row;
	FormRow<Gtk::Entry> m_gdb_port_row;
	FormRow<Gtk::Entry> m_ocd_port_row;
	FormRow<Gtk::FileChooserButton> m_board_row;
	FormRow<Gtk::Entry> m_status_row;
	Glib::RefPtr<Gtk::TextBuffer> m_textbuffer;
	Gtk::ScrolledWindow m_scroll;
	Gtk::TextView m_textview;
	Gtk::ButtonBox m_buttons;
	Gtk::Button m_start;
	Gtk::Button m_stop;
	Gtk::Button m_reset;
	Gtk::Button m_bypass;
	
	sigc::connection m_addr_changed_conn;
	sigc::connection m_ocd_port_changed_conn;
	sigc::connection m_gdb_port_changed_conn;
	
	std::shared_ptr<JtagServer> m_server;
	
	MainWindow *m_parent;
	
	const Device &m_device;
};

class EepromTab: public Gtk::Box
{
public:
	EepromTab(MainWindow *parent, const Device &dev);

protected:
	void read_clicked();
	void write_clicked();
	void compile_done(bool ok, int size, const std::string &errors);
	void decompile_done(bool ok, int size, const std::string &errors);

	Glib::RefPtr<Gtk::TextBuffer> m_textbuffer;
	Gtk::ScrolledWindow m_scroll;
	Gtk::TextView m_textview;
	Gtk::ButtonBox m_buttons;
	Gtk::Button m_read;
	Gtk::Button m_write;
	Gtk::Button m_save;
	std::shared_ptr<DTB> m_dtb;
	std::shared_ptr<std::string> m_textual;
	std::shared_ptr<std::vector<uint8_t>> m_blob;
	MainWindow *m_parent;
	const Device &m_device;
};

class EepromTLVTab: public Gtk::Box
{
public:
	EepromTLVTab(MainWindow *parent);

protected:
	class ModelColumns : public Gtk::TreeModel::ColumnRecord
	{
	public:
		ModelColumns() {add(m_id); add(m_name); add(m_value);}

		Gtk::TreeModelColumn<tlv_code_t> m_id;
		Gtk::TreeModelColumn<std::string> m_name;
		Gtk::TreeModelColumn<std::string> m_value;
	};

	Gtk::Label m_addr_label;
	Gtk::HPaned m_paned;
	Gtk::ScrolledWindow m_scroll;
	Gtk::ButtonBox m_buttons;
	Gtk::Button m_load;
	Gtk::Button m_read;
	Gtk::Button m_write;
	Gtk::Button m_clear;
	MainWindow *m_parent;
	Glib::RefPtr<Gtk::ListStore> m_list_store_ref;
	Gtk::TreeView m_tlv_records;
	std::shared_ptr<std::vector<uint8_t>> m_blob;
	OnieTLV otlv;
	ModelColumns m_model_columns;
	Gtk::ComboBoxText m_combo_addr;

	void load_clicked();
	void read_clicked();
	void write_clicked();
	void clear_clicked();
	void add_tlv_row(tlv_code_t id, std::string name, std::string value);
	void update_tlv_row(tlv_code_t id, std::string value);

};

class GpioTab: public Gtk::Box
{
public:
	GpioTab(MainWindow *parent, const Device &dev);
	virtual ~GpioTab() noexcept;
	std::shared_ptr<Gpio> m_gpio;

	void set_gpio_name(int no, const std::string &name);
	
protected:
	void state_changed(bool state, uint8_t mask);
	void direction_changed(bool state, uint8_t mask);

	MainWindow *m_parent;
	const Device &m_device;
	FormRowGpio m_gpio_row[4];
	sigc::connection m_timer;
	bool timer();
};

class MainWindow: public Gtk::Window
{
public:
	MainWindow();
	virtual ~MainWindow();

	Gpio *m_gpio;
	I2C *m_i2c;
	ProfileConfig *m_pc;
	
	void set_gpio_name(int no, std::string name);
	void set_uart_addr(std::string addr);
	void set_uart_port(std::string port);
	void set_uart_baud(std::string baud);
	void set_jtag_addr(std::string addr);
	void set_jtag_gdb_port(std::string port);
	void set_jtag_ocd_port(std::string port);
	void set_jtag_script(std::string script);
	
protected:
	Gtk::Notebook m_notebook;
	ProfileTab m_profile;
	SerialTab m_uart_tab;
	JtagTab m_jtag_tab;
//	EepromTab m_eeprom_tab;
	EepromTLVTab m_eeprom_tlv_tab;
	GpioTab m_gpio_tab;
	Device m_device;
	
	void show_deviceselect_dialog();
	void configure_devices(const Device &device);
};

#endif /* DEVCLIENT_MAINWINDOW_HH */
