function(gkey_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE gkey-core Qt6::Test)
    target_compile_definitions(${name} PRIVATE
        GKEY_SOURCE_MODEL_DIR="${CMAKE_SOURCE_DIR}/data/models"
    )
    add_test(NAME ${name} COMMAND ${name})
    # Keep the tests away from the user's real configuration and state.
    set_tests_properties(${name} PROPERTIES ENVIRONMENT
        "XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/xdg/config;XDG_STATE_HOME=${CMAKE_CURRENT_BINARY_DIR}/xdg/state;QT_QPA_PLATFORM=offscreen"
    )
endfunction()

gkey_add_test(tst_devicemodel)
gkey_add_test(tst_profiles)
gkey_add_test(tst_engine)
