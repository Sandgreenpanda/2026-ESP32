cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(Esp32-DIGI)
add_compile_options("-Wno-error=sizeof-pointer-memaccess")