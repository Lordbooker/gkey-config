# SPDX-FileCopyrightText: 2026 Lordbooker
# SPDX-License-Identifier: GPL-2.0-only

# Shared code of the daemon, the GUI and the tests (no GUI modules).
add_library(gkey-core STATIC
    Action.cpp
    Action.h
    Autostart.cpp
    Autostart.h
    BindingEngine.cpp
    BindingEngine.h
    ConfigPaths.cpp
    ConfigPaths.h
    DaemonState.cpp
    DaemonState.h
    DeviceModel.cpp
    DeviceModel.h
    DirectoryWatcher.cpp
    DirectoryWatcher.h
    EventCodes.cpp
    EventCodes.h
    GroupAccess.cpp
    GroupAccess.h
    KeyOutput.cpp
    KeyOutput.h
    LcdFont.cpp
    LcdFont.h
    LcdFrame.cpp
    LcdFrame.h
    LcdPages.cpp
    LcdPages.h
    Macro.cpp
    Macro.h
    MacroRun.cpp
    MacroRun.h
    MacroStore.cpp
    MacroStore.h
    ModelRegistry.cpp
    ModelRegistry.h
    OutputCodes.cpp
    OutputCodes.h
    Profile.cpp
    Profile.h
    ProfileStore.cpp
    ProfileStore.h
    Settings.cpp
    Settings.h
    StickProcessor.cpp
    StickProcessor.h
    StickSettings.cpp
    StickSettings.h
)

target_include_directories(gkey-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(gkey-core
    PUBLIC  Qt6::Core
    PRIVATE PkgConfig::LIBEVDEV
)

target_compile_definitions(gkey-core PUBLIC
    GKEY_VERSION="${GKEY_VERSION}"
    GKEY_DATA_DIR="${GKEY_DATA_DIR}"
)
