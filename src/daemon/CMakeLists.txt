# SPDX-FileCopyrightText: 2026 Lordbooker
# SPDX-License-Identifier: GPL-2.0-only

# gkeyd: user session daemon (Qt6 Core + DBus only, no GUI modules).
add_library(gkeyd-lib STATIC
    DBusService.cpp
    DBusService.h
    Daemon.cpp
    Daemon.h
    Device.cpp
    Device.h
    DeviceMonitor.cpp
    DeviceMonitor.h
    InputDevice.cpp
    InputDevice.h
    LcdController.cpp
    LcdController.h
    LcdWriter.cpp
    LcdWriter.h
    LedController.cpp
    LedController.h
    VirtualInput.cpp
    VirtualInput.h
)
target_include_directories(gkeyd-lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
find_package(Threads REQUIRED)
target_link_libraries(gkeyd-lib PUBLIC
    gkey-core
    Threads::Threads
    Qt6::Core
    Qt6::DBus
    PkgConfig::LIBEVDEV
    PkgConfig::LIBUDEV
)

add_executable(gkeyd main.cpp)
target_link_libraries(gkeyd PRIVATE gkeyd-lib)

install(TARGETS gkeyd RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
