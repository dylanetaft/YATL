cmake_minimum_required(VERSION 3.14)

# Examples can be built standalone or as part of yatl
if(NOT TARGET yatl)
    project(yatl-examples LANGUAGES C)
    find_package(yatl REQUIRED)
    set(YATL_LIB yatl::yatl)
else()
    set(YATL_LIB yatl)
endif()

add_executable(fuzz_iter fuzzer_iter.c)
target_include_directories(fuzz_iter PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(fuzz_iter PRIVATE ${YATL_LIB})
target_compile_options(fuzz_iter PRIVATE -fsanitize=fuzzer,address -O1 -fno-omit-frame-pointer)
target_link_libraries(fuzz_iter PRIVATE -fsanitize=fuzzer,address)
