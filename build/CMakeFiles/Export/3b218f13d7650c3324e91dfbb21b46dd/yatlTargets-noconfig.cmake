#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "yatl::yatl" for configuration ""
set_property(TARGET yatl::yatl APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(yatl::yatl PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "C"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libyatl.a"
  )

list(APPEND _cmake_import_check_targets yatl::yatl )
list(APPEND _cmake_import_check_files_for_yatl::yatl "${_IMPORT_PREFIX}/lib/libyatl.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
