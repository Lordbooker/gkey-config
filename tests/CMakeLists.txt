add_executable(tst_devicemodel tst_devicemodel.cpp)
target_link_libraries(tst_devicemodel PRIVATE gkey-core Qt6::Test)
target_compile_definitions(tst_devicemodel PRIVATE
    GKEY_SOURCE_MODEL_DIR="${CMAKE_SOURCE_DIR}/data/models"
)
add_test(NAME devicemodel COMMAND tst_devicemodel)
