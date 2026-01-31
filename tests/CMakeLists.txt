cmake_minimum_required(VERSION 3.14)

# Tests can be built standalone or as part of yatl
if(NOT TARGET yatl)
    project(yatl-tests LANGUAGES C)
    find_package(yatl REQUIRED)
    set(YATL_LIB yatl::yatl)
else()
    set(YATL_LIB yatl)
endif()

# Add test executables here
# Example:
# add_executable(test_parse test_parse.c)
# target_link_libraries(test_parse PRIVATE ${YATL_LIB})
# add_test(NAME test_parse COMMAND test_parse)
