# gkey-config: Qt6 configuration tool.
add_executable(gkey-config
    main.cpp
    DeviceView.cpp
    DeviceView.h
)

target_link_libraries(gkey-config PRIVATE
    gkey-core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::DBus
)

install(TARGETS gkey-config RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
