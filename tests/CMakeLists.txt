# SPDX-FileCopyrightText: 2026 Lordbooker
# SPDX-License-Identifier: GPL-2.0-only

function(gkey_add_test name)
    cmake_parse_arguments(ARG "" "" "LIBS;WRAPPER" ${ARGN})
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE gkey-core Qt6::Test ${ARG_LIBS})
    target_compile_definitions(${name} PRIVATE
        GKEY_SOURCE_MODEL_DIR="${CMAKE_SOURCE_DIR}/data/models"
    )
    add_test(NAME ${name} COMMAND ${ARG_WRAPPER} $<TARGET_FILE:${name}>)
    # Keep the tests away from the user's real configuration and state.
    set_tests_properties(${name} PROPERTIES ENVIRONMENT
        "XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/xdg/config;XDG_STATE_HOME=${CMAKE_CURRENT_BINARY_DIR}/xdg/state;QT_QPA_PLATFORM=offscreen"
    )
endfunction()

gkey_add_test(tst_devicemodel)
gkey_add_test(tst_profiles)
gkey_add_test(tst_engine)
gkey_add_test(tst_macros)
gkey_add_test(tst_leds LIBS gkeyd-lib)
gkey_add_test(tst_stick)
gkey_add_test(tst_lcd)
gkey_add_test(tst_lcdwriter LIBS gkeyd-lib)
gkey_add_test(tst_session)
gkey_add_test(tst_diagnosis)
gkey_add_test(tst_systemstats)

# The D-Bus test needs a private session bus.
find_program(DBUS_RUN_SESSION dbus-run-session)
if(DBUS_RUN_SESSION)
    gkey_add_test(tst_dbus LIBS gkeyd-lib Qt6::DBus WRAPPER ${DBUS_RUN_SESSION} --)
else()
    message(STATUS "dbus-run-session not found: D-Bus test disabled")
endif()

# Completeness of the GUI translations (needs lupdate from Qt Linguist).
if(BUILD_GUI)
    gkey_add_test(tst_translations)
    target_compile_definitions(tst_translations PRIVATE
        GKEY_LUPDATE="$<TARGET_FILE:Qt6::lupdate>"
        GKEY_GUI_SOURCE_DIR="${CMAKE_SOURCE_DIR}/src/gui"
        GKEY_TS_DIR="${CMAKE_SOURCE_DIR}/translations"
    )
endif()
