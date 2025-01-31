# main.cpp 

project(rapidio)

##################################
# GTest
# https://github.com/google/googletest/tree/main/googletest#incorporating-into-an-existing-cmake-project

include(FetchContent)
FetchContent_Declare(
  googletest
  # Specify the commit you depend on and update it regularly.
  URL https://github.com/google/googletest/archive/504ea69cf7e9947be54f808a09b7b08988e84b5f.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Now simply link against gtest or gtest_main as needed. Eg
add_executable(rapidioTests "test_main.cpp")
target_link_libraries(rapidioTests PRIVATE gtest_main)
add_test(NAME rapidio_test COMMAND example)

##################################
# RapidIO

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
add_library(rapidio INTERFACE)
target_include_directories(rapidio INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(rapidioTests PRIVATE rapidio)

##################################
# RapidIO Performance Tests

add_executable(rapidioPerformance "profile_main.cpp")

##################################
# Copying /data files to build

add_custom_command(TARGET rapidioTests PRE_LINK
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${DATA_FILES} ${DESTINATION_COPY}
	COMMENT "Copying /data files...")