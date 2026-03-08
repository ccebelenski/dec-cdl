include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

add_executable(cdl_tests
    test_cli_tables.cpp
    test_cli_parse.cpp
    test_cli_present.cpp
    test_cli_get_value.cpp
    test_cli_dispatch.cpp
)

target_link_libraries(cdl_tests PRIVATE cdl GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(cdl_tests)
