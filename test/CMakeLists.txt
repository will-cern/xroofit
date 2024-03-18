
## get googletest
include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
)
FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
    FetchContent_Populate(googletest)
    add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})
endif()
enable_testing()
include(GoogleTest)

add_executable(TestxRooFit_oneChannel oneChannelTests.cxx)
target_link_libraries(TestxRooFit_oneChannel xRooFit gtest gtest_main)

add_executable(TestxRooFit_twoChannel twoChannelTests.cxx)
target_link_libraries(TestxRooFit_twoChannel xRooFit gtest gtest_main)