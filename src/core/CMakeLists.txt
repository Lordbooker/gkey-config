# Shared code of the daemon, the GUI and the tests (no GUI modules).
add_library(gkey-core STATIC
    DeviceModel.cpp
    DeviceModel.h
    ModelRegistry.cpp
    ModelRegistry.h
    EventCodes.cpp
    EventCodes.h
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
