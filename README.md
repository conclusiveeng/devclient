# Conclusive Developer Cable Client

## Building and running on Debian and Ubuntu

1. Install the needed prerequisites:

```
sudo apt-get install build-essential cmake libgtkmm-3.0-dev libftdipp1-dev libtool
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

## Building and running on Windows

1. Install MSYS2 according to the guide https://www.msys2.org/ . Follow steps from 1 to 8 (installation on mingw-w64-x86-64 compilers).
2. Run MSYS2-MinGW64 terminal and install following packages:  pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-boost  mingw-w64-x86_64-gtkmm mingw-w64-x86_64-libftdi  mingw-w64-x86_64-gtkmm3 mingw-w64-x86_64-toolchain base-devel
3. Download libftdi and build libftdipp package:
   
    ```bash
    cd libftdi1-1.5
    mkdir build
    cd build
    cmake .. -G"MinGW Makefiles" -DCMAKE_INSTALL_PREFIX="../../libftdi" -DEXAMPLES=OFF -DPYTHON_BINDINGS=OFF -DLINK_PYTHON_LIBRARY=OFF -DDOCUMENTATION=OFF -DFTDIPP=ON -DCMAKE_BUILD_TYPE=Release
    mingw32-make.exe
    mingw32-make.exe install
    ```

4. Copy install directory content into 'C:\msys64\mingw64'
5. Build devclient package same way as for Linux
6. Download "Zadig" from https://zadig.akeo.ie/
7. Configure USB Driver for "Developer Cable"
   1. In "Options" menu check "List all devices"
   2. In "Options" menu uncheck "Ignore Hubs or Composite Parents"
   3. From the list select "Developer Cable (Composite Parent)"
   4. Select new driver "WinUSB" and press "Replace Driver" button
   5. Unplug the developer cable after the change

### Note

Windows version currently only support serial port functionality.

## Usage

### Device selection

### Serial console tab

### JTAG tab

### EEPROM tab
