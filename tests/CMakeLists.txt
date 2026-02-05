cmake_minimum_required(VERSION 3.14)

include(FetchContent)

# Tests can be built standalone or as part of yatl
if(NOT TARGET yatl)
    project(yatl-tests LANGUAGES C)
    find_package(yatl REQUIRED)
    set(YATL_LIB yatl::yatl)
else()
    set(YATL_LIB yatl)
endif()

# Fetch munit
FetchContent_Declare(
    munit
    GIT_REPOSITORY https://github.com/nemequ/munit.git
    GIT_TAG master
)
FetchContent_MakeAvailable(munit)

# Build munit as a library
add_library(munit STATIC ${munit_SOURCE_DIR}/munit.c)
target_include_directories(munit PUBLIC ${munit_SOURCE_DIR})

# Combined test suite
add_executable(test_yatl test_yatl.c)
target_link_libraries(test_yatl PRIVATE ${YATL_LIB} munit)
target_include_directories(test_yatl PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_yatl COMMAND test_yatl WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
