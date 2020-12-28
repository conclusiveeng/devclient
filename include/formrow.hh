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

#ifndef DEVCLIENT_FORMROW_HH
#define DEVCLIENT_FORMROW_HH

#include <gtkmm.h>

template <class T>
class FormRow: public Gtk::Box
{
public:
	explicit FormRow(const Glib::ustring &label):
	    Gtk::Box(Gtk::Orientation::ORIENTATION_HORIZONTAL, 10),
	    m_label(label)
	{
		m_label.set_justify(Gtk::Justification::JUSTIFY_LEFT);
		m_label.set_size_request(250, -1);
		pack_start(m_label, false, true);
		pack_start(m_widget, true, true);
		show_all_children();
	}

	T &get_widget()
	{
		return (m_widget);
	}

protected:
	T m_widget;
	Gtk::Label m_label;
};

class FormRowGpio: public Gtk::Box
{
public:
	explicit FormRowGpio(const Glib::ustring &label):
	    Gtk::Box(Gtk::Orientation::ORIENTATION_HORIZONTAL, 10),
	    m_toggle("off"),
	    m_image("gtk-no", Gtk::ICON_SIZE_BUTTON),
	    m_label(label)
	{
		m_label.set_justify(Gtk::Justification::JUSTIFY_LEFT);
		m_label.set_size_request(250, -1);
		pack_start(m_label, false, true);
		m_radio_in.set_label("input");
		m_radio_out.set_label("output");
		m_radio_out.join_group(m_radio_in);
		m_toggle.set_sensitive(false);
		m_image.set_sensitive(false);

		pack_start(m_radio_in, true, true);
		pack_start(m_radio_out, true, true);
		pack_start(m_toggle, true, true);
		pack_start(m_image, false, false);

		m_toggle.signal_toggled().connect(sigc::mem_fun(*this, &FormRowGpio::toggled));
		m_radio_in.signal_toggled().connect(sigc::mem_fun(*this, &FormRowGpio::in_toggled));
		m_radio_out.signal_toggled().connect(sigc::mem_fun(*this, &FormRowGpio::out_toggled));
		show_all_children();
	}

	void toggled()
	{
		bool active = m_toggle.get_active();

		if (active) {
			m_state_changed.emit(true);
			m_toggle.set_label("on");
			m_image.set_from_icon_name("gtk-yes", Gtk::ICON_SIZE_BUTTON);
		} else {
			m_state_changed.emit(false);
			m_toggle.set_label("off");
			m_image.set_from_icon_name("gtk-no", Gtk::ICON_SIZE_BUTTON);
		}
	}

	void in_toggled()
	{
		m_toggle.set_sensitive(false);
		m_image.set_sensitive(false);
		m_direction_changed.emit(false);
	}

	void out_toggled()
	{
		m_toggle.set_sensitive(true);
		m_image.set_sensitive(true);
		m_direction_changed.emit(true);
	}

	bool get_direction()
	{
		return (m_radio_out.get_active());
	}

	void set_direction(bool output)
	{
		m_radio_in.set_active(!output);
		m_radio_out.set_active(output);
		m_direction_changed.emit(output);
	}

	bool get_state()
	{
		return (m_toggle.get_active());
	}

	void set_state(bool state)
	{
		m_toggle.set_active(state);
		m_state_changed.emit(state);
	}

	void set_gpio_name(const std::string &name)
	{
		m_label.set_label(name);
	}

	sigc::signal<void(bool)> direction_changed()
	{
		return m_direction_changed;
	}

	sigc::signal<void(bool)> state_changed()
	{
		return m_state_changed;
	}

protected:
	Gtk::ToggleButton m_toggle;
	Gtk::RadioButton m_radio_in;
	Gtk::RadioButton m_radio_out;
	Gtk::Image m_image;
	Gtk::Label m_label;
	sigc::signal<void(bool)> m_direction_changed;
	sigc::signal<void(bool)> m_state_changed;
};



#endif //DEVCLIENT_FORMROW_HH
