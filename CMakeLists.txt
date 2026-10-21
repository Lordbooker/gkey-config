cmake_minimum_required(VERSION 3.20)

project(gkey-config VERSION 1.0.0 LANGUAGES CXX)

# --- Version ------------------------------------------------------------------
# GKEY_VERSION: displayed version. Default: "git describe" without the leading
# "v" (e.g. 0.1.0 or 0.1.0-3-gd1568bb), else PROJECT_VERSION.
if(NOT DEFINED GKEY_VERSION)
    find_package(Git QUIET)
    if(GIT_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --match "v[0-9]*"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE GKEY_GIT_DESCRIBE
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
    endif()
    if(GKEY_GIT_DESCRIBE)
        string(REGEX REPLACE "^v" "" GKEY_VERSION "${GKEY_GIT_DESCRIBE}")
    else()
        set(GKEY_VERSION "${PROJECT_VERSION}")
    endif()
endif()
message(STATUS "gkey-config version: ${GKEY_VERSION}")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Options --------------------------------------------------------------------
option(BUILD_GUI         "Build the Qt6 configuration tool gkey-config" ON)
option(BUILD_TESTS       "Build the unit tests (Qt Test)"               OFF)
option(ENABLE_SANITIZERS "Enable ASan/UBSan in debug builds"             OFF)

if(ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

# --- Dependencies -------------------------------------------------------------
include(GNUInstallDirs)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVDEV REQUIRED IMPORTED_TARGET libevdev)
pkg_check_modules(LIBUDEV  REQUIRED IMPORTED_TARGET libudev)

set(GKEY_QT_COMPONENTS Core DBus)
if(BUILD_GUI)
    list(APPEND GKEY_QT_COMPONENTS Gui Widgets Network LinguistTools)
endif()
if(BUILD_TESTS)
    list(APPEND GKEY_QT_COMPONENTS Test)
endif()
find_package(Qt6 6.4 REQUIRED COMPONENTS ${GKEY_QT_COMPONENTS})
qt_standard_project_setup()

# Directory of the installed model files, compiled into the binaries.
set(GKEY_DATA_DIR "${CMAKE_INSTALL_FULL_DATADIR}/gkey-config")

# --- Targets --------------------------------------------------------------------
add_subdirectory(src/core)
add_subdirectory(src/daemon)
if(BUILD_GUI)
    add_subdirectory(src/gui)
endif()
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# --- Data files -------------------------------------------------------------------
install(DIRECTORY data/models/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/gkey-config/models
    FILES_MATCHING PATTERN "*.json"
)
# Relative destinations (= <prefix>/lib/...): CPack/RPM would otherwise mark
# absolute paths as %config, which is wrong for files below /usr/lib.
install(FILES data/udev/70-gkey-config.rules
    DESTINATION lib/udev/rules.d
)
install(FILES data/sysusers/gkey-config.conf
    DESTINATION lib/sysusers.d
)
configure_file(data/systemd/gkeyd.service.in ${CMAKE_CURRENT_BINARY_DIR}/gkeyd.service @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/gkeyd.service
    DESTINATION lib/systemd/user
)
configure_file(data/dbus/io.github.lordbooker.GKeyConfig.service.in
    ${CMAKE_CURRENT_BINARY_DIR}/io.github.lordbooker.GKeyConfig.service @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/io.github.lordbooker.GKeyConfig.service
    DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services
)
if(BUILD_GUI)
    install(FILES data/desktop/io.github.lordbooker.GKeyConfig.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )
    # Tray icon at login (XDG autostart; with prefix /usr -> /etc/xdg/autostart)
    install(FILES data/desktop/gkey-config-tray.desktop
        DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/xdg/autostart
    )
    install(FILES data/icons/gkey-config.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
    )
endif()
install(FILES README.md LICENSE
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

include(cmake/Packaging.cmake)
