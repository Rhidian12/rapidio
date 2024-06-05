# main.cpp 

project(rapidio)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
add_library(rapidio INTERFACE)
target_include_directories(rapidio INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(rapidioTests main.cpp)

target_link_libraries(rapidioTests PRIVATE rapidio)
