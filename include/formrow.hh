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
	FormRow(const Glib::ustring &label):
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




template <class T>
class FormRowGpio: public Gtk::Box
{
public:
	Gtk::RadioButton m_radio_in;
	Gtk::RadioButton m_radio_out;
	Gtk::Image image;

	FormRowGpio(const Glib::ustring &label):
	    Gtk::Box(Gtk::Orientation::ORIENTATION_HORIZONTAL, 10),
	    m_label(label)
	{
		m_label.set_justify(Gtk::Justification::JUSTIFY_LEFT);
		m_label.set_size_request(250, -1);
		pack_start(m_label, false, true);
		m_radio_in.set_label("input");
		m_radio_out.set_label("output");
		m_radio_out.join_group(m_radio_in);

		pack_start(m_radio_in, true, true);
		pack_start(m_radio_out, true, true);
		pack_start(m_widget, true, true);
		pack_start(image, false, false);
		show_all_children();
	}

	T &get_widget()
	{
		return (m_widget);
	}

	void set_gpio_name(std::string name)
	{
		m_label.set_label(name);
		
	}
	
	
protected:
	T m_widget;
	Gtk::Label m_label;

	void clicked();
};



#endif //DEVCLIENT_FORMROW_HH
