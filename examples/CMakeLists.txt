cmake_minimum_required(VERSION 3.14)

# Examples can be built standalone or as part of yatl
if(NOT TARGET yatl)
    project(yatl-examples LANGUAGES C)
    find_package(yatl REQUIRED)
    set(YATL_LIB yatl::yatl)
else()
    set(YATL_LIB yatl)
endif()

add_executable(example1 example1.c)
add_executable(replace_array replace_array.c)
target_link_libraries(example1 PRIVATE ${YATL_LIB})
target_link_libraries(replace_array PRIVATE ${YATL_LIB})
