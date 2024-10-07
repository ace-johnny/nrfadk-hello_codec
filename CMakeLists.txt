# nrfadk-hello_codec

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(nrfadk-hello_codec)

target_sources(app PRIVATE src/main.c)

zephyr_library_include_directories(src/drivers)
add_subdirectory(src/drivers)
add_subdirectory(${ZEPHYR_CIRRUS_LOGIC_MODULE_DIR} cirrus_logic_bin_dir)
