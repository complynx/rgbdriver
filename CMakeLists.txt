# set minimum cmake version
cmake_minimum_required(VERSION 3.5 FATAL_ERROR)


# project name and language
project(rgbdriver)

set(CMAKE_C_STANDARD 99)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


include(GNUInstallDirs)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})

# define executable and its source file
add_subdirectory(color2duty)
add_subdirectory(fasti2c)
add_subdirectory(pcadriver)
add_subdirectory(rgbdriver)
# add_subdirectory(smartthingsd)
