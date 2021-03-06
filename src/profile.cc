
#include <profile.hh>
#include <string>
#include <yaml-cpp/node/node.h>
#include <yaml-cpp/yaml.h>
#include <log.hh>

ProfileConfig::ProfileConfig(const std::string &file_name) 
{
    YAML::Node profile_file;

    Logger::debug("File name: {}", file_name);
	try {
		profile_file = YAML::LoadFile(file_name);
	}
	catch (const YAML::BadFile& badFile) {
		throw ProfileConfigException("Cannot read profile file.");
	}
	catch (const YAML::ParserException& parserException) {
		throw ProfileConfigException("Profile file has bad format.");
	}

    if ((uart = profile_file["devcable-serial"]) == nullptr) {
        throw ProfileConfigException("devcable-serial node is not found in profile file");
    } else {
        devcable_serial = profile_file["devcable-serial"].as<std::string>();
    }

    if ((uart = profile_file["uart"]) == nullptr) {
        throw ProfileConfigException("UART node in profile file is not found.");
    }

    if ((jtag = profile_file["jtag"]) == nullptr) {
        throw ProfileConfigException("JTAG node in profile file is not found.");
    }

    /* GPIO and EEPROM nodes in profile file are optional */
    if ((gpio = profile_file["gpio"]) == nullptr) {
         Logger::warning("GPIO node in profile file is not found.");
    } else {
        for(YAML::const_iterator it=gpio.begin(); it!=gpio.end(); ++it)
            gpio_names.push_back(it->as<std::string>());
    }

    if ((eeprom = profile_file["eeprom"]) == nullptr) {
        Logger::warning("EEPROM node in profile file is not found.");
    }
}

std::string ProfileConfig::get_devcable_serial() 
{
    return devcable_serial;
}

std::uint32_t ProfileConfig::get_uart_baudrate() 
{
    uint32_t baudrate = 0;
    if (uart["baudrate"])
        baudrate = uart["baudrate"].as<uint32_t>();
    else
        throw ProfileConfigException("No 'baudrate' in Uart node");

    return baudrate;
}

std::string ProfileConfig::get_uart_listen_address() 
{
    std::string listen_address;
    if (uart["listen_address"])
        listen_address = uart["listen_address"].as<std::string>();
    else
        throw ProfileConfigException("No 'listen_address' in Uart node");

    return listen_address;
}

std::uint32_t ProfileConfig::get_uart_port() 
{
    uint32_t uart_port = 0;
    if (uart["listen_port"])
        uart_port = uart["listen_port"].as<uint32_t>();
    else
        throw ProfileConfigException("No 'listen_port' in Uart node");

    return uart_port;
}

std::uint32_t ProfileConfig::get_jtag_gdb_port() 
{
    uint32_t gdb_listen_port = 0;
    if (jtag["gdb_listen_port"])
        gdb_listen_port = jtag["gdb_listen_port"].as<uint32_t>();
    else
        throw ProfileConfigException("No 'gdb_listen_port' in JTAG node");

    return gdb_listen_port;
}

std::uint32_t ProfileConfig::get_jtag_telnet_port() 
{
    uint32_t telnet_listen_port = 0;
    if (jtag["telnet_listen_port"])
        telnet_listen_port = jtag["telnet_listen_port"].as<uint32_t>();
    else
        throw ProfileConfigException("No 'telnet_listen_port' in JTAG node");

    return telnet_listen_port;
}

std::string ProfileConfig::get_jtag_listen_address()
{
    std::string listen_address;
    if (jtag["listen_address"])
        listen_address = jtag["listen_address"].as<std::string>();
    else
        throw ProfileConfigException("No 'listen_address' in JTAG node");

    return listen_address;
}

std::string ProfileConfig::get_jtag_script_file() 
{
    std::string script_file;
    if (jtag["script_file"])
        script_file = jtag["script_file"].as<std::string>();
    else
        throw ProfileConfigException("No 'script_file' in JTAG node");

    return script_file;
}

bool ProfileConfig::get_jtag_passtrough() 
{
    bool pass_trough = 0;
    if (jtag["pass_trough"])
        pass_trough = jtag["pass_trough"].as<bool>();
    else
        throw ProfileConfigException("No 'pass_trough' in JTAG node");

    return pass_trough;
}

std::string ProfileConfig::get_gpio_name(int gpio) 
{
    int gpio_labels = gpio_names.size();
    if (gpio >= 0 && gpio < gpio_labels)
        return gpio_names[gpio];
    return "";
}

std::string ProfileConfig::get_eeprom_file() 
{
    std::string eeprom_file;
    if (eeprom && eeprom["eeprom_file"])
        eeprom_file = eeprom["eeprom_file"].as<std::string>();
    else
        throw ProfileConfigException("No 'eeprom_file' in JTAG node");

    return eeprom_file;
}