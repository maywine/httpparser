PROJECT(httparser)
cmake_minimum_required(VERSION 3.10...3.15)

if(NOT CMAKE_C_STANDARD)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
endif()

if(NOT CMAKE_CXX_STANDARD)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
endif()

option(BUILD_SHARED_LIBS "Build shared library" ON)
option(HTTPARSER_BUILD_TESTS "Build httparser's unit tests" OFF)
option(HTTPARSER_INSTALL "Install httparser's header" ON)

# default release
if(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
        FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(HTTPARSER_BUILD_TESTS)
FIND_PACKAGE(Boost REQUIRED COMPONENTS 
            unit_test_framework 
            system)
enable_testing()
add_subdirectory(tests)
endif(HTTPARSER_BUILD_TESTS)

if(HTTPARSER_INSTALL)
include(GNUInstallDirs)
INSTALL(FILES "${CMAKE_SOURCE_DIR}/single_include/httpparser/httpparser.h"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/httpparser")
endif(HTTPARSER_INSTALL)
