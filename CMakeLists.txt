cmake_minimum_required(VERSION 3.16)
project(devclient)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-Wall -ggdb3")


set(OPENOCD_URL https://github.com/conclusiveeng/openocd.git)
set(OPENOCD_VER master)

add_subdirectory(contrib/fmt-6.0.0)
add_subdirectory(contrib/filesystem-1.2.10)
add_subdirectory(contrib/libucl)

find_package(PkgConfig)
find_package(Boost)
find_package(yaml-cpp)

pkg_check_modules(GTKMM gtkmm-3.0)
pkg_check_modules(GIOMM giomm-2.4)
pkg_check_modules(LIBFTDI libftdipp1)

link_directories(${GTKMM_LIBRARY_DIRS})
link_directories(${LIBFTDI_LIBRARY_DIRS})
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/contrib/libucl/)
link_libraries(stdc++fs)
include_directories(${GIOMM_INCLUDE_DIRS})
include_directories(${GTKMM_INCLUDE_DIRS})
include_directories(${LIBFTDI_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/contrib/filesystem-1.2.10/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/contrib/libucl/include)
include_directories(${YAML_CPP_INCLUDE_DIRS})
include_directories(include)

add_executable(devclient
        src/utils.cc
        src/uart.cc
        src/jtag.cc
        src/i2c.cc
        src/gpio.cc
        src/device.cc
        src/log.cc
        src/dtb.cc
        src/deviceselect.cc
        src/application.cc
        src/mainwindow.cc
        src/nogui.cc
        src/main.cc
        src/onie_tlv.cc
        src/eeprom.cc)

message("-- Cloning OpenOCD")

execute_process(
        COMMAND git clone --recursive ${OPENOCD_URL} -b ${OPENOCD_VER}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

execute_process(
        COMMAND git pull origin ${OPENOCD_VER}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/openocd)

file(COPY scripts DESTINATION ${CMAKE_BINARY_DIR})
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/tools)
file(COPY icon.png DESTINATION ${CMAKE_BINARY_DIR})

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/openocd/config.log
        COMMAND ./bootstrap nosubmodule
        COMMAND ./configure --enable-target64 --enable-libftdi --prefix="" --disable-werror
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/openocd)

add_custom_target(openocd ALL
        DEPENDS ${CMAKE_BINARY_DIR}/openocd/config.log
        COMMAND make -j4
        COMMAND make install DESTDIR=${CMAKE_BINARY_DIR}/tools
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/openocd)

target_link_libraries(devclient
        ${GIOMM_LIBRARIES}
        ${GTKMM_LIBRARIES}
        ${LIBFTDI_LIBRARIES}
        ${Boost_LIBRARIES}
        ${YAML_CPP_LIBRARIES}
        fmt
        ucl
        z)
target_link_libraries(devclient pthread)
target_link_libraries(devclient ftdipp1)

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_link_libraries(devclient stdc++fs)
endif()

install(TARGETS devclient DESTINATION bin)
install(DIRECTORY ${CMAKE_BINARY_DIR}/tools/ DESTINATION tools USE_SOURCE_PERMISSIONS)
install(DIRECTORY ${CMAKE_BINARY_DIR}/scripts/ DESTINATION scripts USE_SOURCE_PERMISSIONS)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(CPACK_GENERATOR "DragNDrop")
elseif(UNIX)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Jakub Klama")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/conclusive/devclient")
endif()

include(CPack)
