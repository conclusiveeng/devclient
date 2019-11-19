//
// Created by jakub on 26.10.2019.
//

#include <wait.h>
#include <vector>
#include <experimental/filesystem>
#include <giomm.h>
#include <gtkmm/main.h>
#include <ftdi.hpp>
#include <log.hh>
#include <jtag.hh>
#include <utils.hh>


#define BUFFER_SIZE	1024

JtagServer::JtagServer(const Device &device, Glib::RefPtr<Gio::InetAddress>,
    uint16_t gdb_port, uint16_t ocd_port, const std::string &board_type):
    m_device(device),
    m_ocd_port(ocd_port),
    m_gdb_port(gdb_port),
    m_board_type(board_type),
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
	std::string scripts_path = executable_dir() / "scripts";
	std::vector<std::string> argv {
		executable_dir() / "tools/bin/openocd",
		"-c", fmt::format("gdb_port {}", m_gdb_port),
		"-c", fmt::format("telnet_port {}", m_ocd_port),
		"-c", "tcl_port disabled",
		"-c", "interface ftdi",
		"-c", "transport select jtag",
		"-c", "adapter_khz 8000",
		"-c", "ftdi_channel 1",
		"-c", "ftdi_layout_init 0x0008 0x000b",
		"-c", "ftdi_layout_signal nTRST -data 0x10",
		"-c", "ftdi_layout_signal nSRST -oe 0x20 -data 0x20",
		"-c", "adapter_nsrst_delay 500",
		"-c", fmt::format("ftdi_serial \"{}\"", m_device.serial),
		"-c", fmt::format("ftdi_vid_pid {:#04x} {:#04x}", m_device.vid, m_device.pid),
		"-f", fmt::format("{}/{}.tcl", scripts_path, m_board_type),
	};

	if (m_running)
		return;

	Glib::spawn_async_with_pipes("/tmp", argv,
	    Glib::SpawnFlags::SPAWN_DO_NOT_REAP_CHILD,
	    Glib::SlotSpawnChildSetup(), &m_pid, nullptr,
	    &stdout_fd, &stderr_fd);

	m_out = Gio::UnixInputStream::create(stdout_fd, true);
	m_err = Gio::UnixInputStream::create(stderr_fd, true);

	m_out->read_bytes_async(BUFFER_SIZE, sigc::bind(sigc::mem_fun(
	    *this, &JtagServer::output_ready), m_out));

	m_err->read_bytes_async(BUFFER_SIZE, sigc::bind(sigc::mem_fun(
	    *this, &JtagServer::output_ready), m_err));

	Glib::signal_child_watch().connect(
	    sigc::mem_fun(*this, &JtagServer::child_exited),
	    m_pid);

	setpgid(m_pid, getpid());
	m_running = true;
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
	    device.serial) != 0)
		throw std::runtime_error("Failed to open device");

	if (context.set_bitmode(0xff, BITMODE_RESET) != 0)
		throw std::runtime_error("Failed to set bitmode");

	if (context.set_bitmode(0, BITMODE_BITBANG) != 0)
		throw std::runtime_error("Failed to set bitmode");

	Logger::info("Bypass mode enabled");
	context.close();
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

	output_produced.emit(std::string(ptr, nread));
	stream->read_bytes_async(BUFFER_SIZE, sigc::bind(sigc::mem_fun(
	    *this, &JtagServer::output_ready), stream));
}

void
JtagServer::child_exited(Glib::Pid pid, int code)
{
	Logger::info("OpenOCD exited with code {} (pid {})", code, pid);
	m_running = false;
}