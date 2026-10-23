# SPDX-FileCopyrightText: 2026 Lordbooker
# SPDX-License-Identifier: GPL-2.0-only

# gkey-config: Qt6 configuration tool.
add_executable(gkey-config
    main.cpp
    BacklightPage.cpp
    BacklightPage.h
    BindingEditor.cpp
    BindingEditor.h
    DaemonClient.cpp
    DaemonClient.h
    DeviceView.cpp
    DeviceView.h
    KeyCaptureEdit.cpp
    KeyCaptureEdit.h
    KeyNames.cpp
    KeyNames.h
    LcdPage.cpp
    LcdPage.h
    MacroEditor.cpp
    MacroEditor.h
    MainWindow.cpp
    MainWindow.h
    SessionSetup.cpp
    SessionSetup.h
    StickPage.cpp
    StickPage.h
)

target_link_libraries(gkey-config PRIVATE
    gkey-core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::DBus
    Qt6::Network   # QLocalServer/QLocalSocket for the single instance
)

# Translations: English is the source language and the fallback. The .qm
# files are built into the binary under :/i18n. After text changes:
#   cmake --build build --target update_translations
set(GKEY_GUI_LANGUAGES de es fr it pt)
set(GKEY_GUI_TS_FILES)
foreach(lang IN LISTS GKEY_GUI_LANGUAGES)
    list(APPEND GKEY_GUI_TS_FILES ${CMAKE_SOURCE_DIR}/translations/gkey-config_${lang}.ts)
endforeach()
qt_add_translations(gkey-config
    TS_FILES ${GKEY_GUI_TS_FILES}
    RESOURCE_PREFIX "/i18n"
    LUPDATE_OPTIONS -no-obsolete -locations none
)

install(TARGETS gkey-config RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
