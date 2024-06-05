# main.cpp 

project(rapidio)

if (WIN32)
    set(EXECUTABLE_TYPE WIN32)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
add_library(rapidio INTERFACE)
target_include_directories(rapidio INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(rapidioTests ${EXECUTABLE_TYPE} main.cpp)

target_link_libraries(rapidioTests PRIVATE rapidio)
