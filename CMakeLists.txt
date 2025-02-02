# root CMake

cmake_minimum_required(VERSION 3.22)

project(rapidioproject VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if (MSVC)
	set(CMAKE_CXX_FLAGS "/W4 /WX /EHsc")
	set(CMAKE_CXX_FLAGS_DEBUG "/Od")
	set(CMAKE_CXX_FLAGS_RELEASE "/O2")
else()
	set(CMAKE_CXX_FLAGS "-Wall -Wextra")
	set(CMAKE_CXX_FLAGS_DEBUG "-g")
	set(CMAKE_CXX_FLAGS_RELEASE "-O3")
endif()

set(DATA_FILES "${CMAKE_CURRENT_SOURCE_DIR}/rapidio/data")
set(DESTINATION_COPY "${CMAKE_BINARY_DIR}/data")

add_subdirectory("rapidio")