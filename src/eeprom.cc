#include <eeprom.hh>
#include <eeprom/24c.hh>
#include <log.hh>

#define RD_BIT 0x01
/* First = eeprom address without R/W = 8th bit, Second = eeprom address extended to 8 bits */
std::map<std::string, uint8_t> Eeprom::eeprom_addrs = { {"0x50", 0xa0}, {"0x56", 0xac} };

void Eeprom24c::read(uint16_t offset, size_t length, std::vector<uint8_t> &data)
{
    if (!address.valid) {
        Logger::error("EEPROM adddress is not valid");
        return;
    }

    m_i2c.start();
    m_i2c.write({
        address.write,
        static_cast<unsigned char>((offset >> 8) & 0xff),
        static_cast<unsigned char>(offset & 0xff)
    });

    m_i2c.start();
    m_i2c.write({ address.read });
    m_i2c.read(length, data);
    m_i2c.stop();
}

void Eeprom24c::write(uint16_t offset, const std::vector<uint8_t> &data)
{
    std::vector<uint8_t> slice;
    std::vector<uint8_t>::size_type i;

    if (!address.valid) {
        Logger::error("EEPROM adddress is not valid");
        return;
    }

    for (i = 0; i < data.size(); i += 32) {
        Logger::debug("Writing to AT24C at offset {}", offset);
        m_i2c.start();
        m_i2c.write({
            address.write,
            static_cast<unsigned char>((offset >> 8) & 0xff),
            static_cast<unsigned char>(offset & 0xff)
        });

        slice = std::vector<uint8_t>(data.begin() + i, data.begin() + i + 32);
        m_i2c.write(slice);
        m_i2c.stop();
        offset += 32;
        usleep(50000);
    }
}

void Eeprom24c::erase()
{
}

void Eeprom24c::set_address(std::string addr) 
{
    auto it = eeprom_addrs.find(addr);
    if (it != eeprom_addrs.end()) {
        address.write = it->second;
        address.read = (it->second | RD_BIT);
        address.valid = true;
        Logger::debug("Read address 0x{:x} Write addr 0x{:x}", address.read, address.write);
    } else {
        Logger::error("Wrong EEPROM address provided: {}", addr);
        address.valid = false;
    }
}
