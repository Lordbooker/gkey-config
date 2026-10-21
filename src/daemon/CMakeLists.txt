# gkeyd: user session daemon (Qt6 Core + DBus only, no GUI modules).
add_executable(gkeyd
    main.cpp
)

target_link_libraries(gkeyd PRIVATE
    gkey-core
    Qt6::Core
    Qt6::DBus
    PkgConfig::LIBEVDEV
    PkgConfig::LIBUDEV
)

install(TARGETS gkeyd RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
