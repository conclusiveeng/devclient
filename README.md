# Conclusive Developer Cable Client

## Building and running on Debian and Ubuntu

1. Install the needed prerequisites:

```
sudo apt-get install build-essential cmake libgtkmm-3.0-dev libftdipp1-dev libtool libyaml-cpp-dev
```

2. Build:

```
$ cd devclient
$ mkdir build
$ cd build
$ cmake ..
$ make
$ make package
```

3. Install and run:

```
sudo dpkg -i devclient-0.1.1-Linux.deb
sudo /opt/conclusive/devclient/bin/devclient
```

## Building and running on macOS

`brew install --HEAD conclusiveeng/formulas/devclient`

If you encounter an error like this: `Failed to open device: unable to claim usb device. Make sure the default FTDI driver is not in use`

Run `sudo kextunload -b com.FTDI.driver.FTDIUSBSerialDriver`

## Usage

### Device selection

### Serial console tab

### JTAG tab

### EEPROM tab
