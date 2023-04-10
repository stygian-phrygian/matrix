# project meta data
cmake_minimum_required(VERSION 3.15)
project(matrix)

# add the cmake directory to the cmake module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# dependencies
# sudo apt install libncurses-dev
find_package(Curses REQUIRED)

# add imported target libraries
# sudo apt install libncurses-dev
find_package(LIBCURSES REQUIRED)

add_subdirectory(apps)

