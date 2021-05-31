#include <utils.hh>
#include <filesystem.hh>

#if defined(__APPLE__) && defined(__MACH__)
#include <mach-o/dyld.h>
#endif

void show_centered_dialog(const std::string &title, const std::string &secondary)
{
	Gtk::MessageDialog dialog(title);
	dialog.set_position(Gtk::WIN_POS_CENTER_ALWAYS);
	dialog.set_secondary_text(secondary);
	dialog.run();
}

#if defined(__APPLE__) && defined(__MACH__)

std::string macos_executable_dir()
{
	uint32_t sz = 256;
	char *exe_path = static_cast<char*>(malloc(sz * sizeof(char)));
	
	while (_NSGetExecutablePath(exe_path, &sz) != 0)
	{
		sz *= 2;
		exe_path = static_cast<char*>(
			realloc(exe_path, sz * sizeof(char)));
	}
	
	char *canon_path = realpath(exe_path, nullptr);
	if (canon_path != nullptr) {
		std::strncpy(exe_path, canon_path, sz);
		free(canon_path);
	}
	
	std::string exe_path_ccstr(exe_path);
	exe_path_ccstr.erase(exe_path_ccstr.rfind("/"));
	exe_path_ccstr.erase(exe_path_ccstr.rfind("/"));
	
	free(exe_path);
	
	return exe_path_ccstr;
}

#elif defined(__unix__)

std::string unix_executable_dir()
{
	filesystem::path executable_dir;

	executable_dir = filesystem::read_symlink("/proc/self/exe").parent_path();

	if (executable_dir.string().rfind("/opt/", 0) != std::string::npos)
		executable_dir = executable_dir.parent_path();

	return executable_dir.native();
}

#else

std::string unimpl_executable_dir()
{
	show_centered_dialog(
		"Unimplemented executable_dir() for your platform.");
}

#endif

std::string executable_dir()
{
#if defined(__APPLE__) && defined(__MACH__)
	return macos_executable_dir();
#elif defined(__unix__)
	return unix_executable_dir();
#else
	return unimpl_executable_dir();
#endif
}