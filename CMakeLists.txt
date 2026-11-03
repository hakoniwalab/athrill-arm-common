cmake_minimum_required(VERSION 3.16)
project(athrill_arm_common C)

# The Athrill core (for std_types.h), next to this checkout as for the targets.
set(ATHRILL_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../athrill" CACHE PATH "Athrill core source tree")

# Header-only: targets add include/ and the core's src/inc to their include path.
add_library(athrill_arm_common INTERFACE)
target_include_directories(athrill_arm_common INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${ATHRILL_SOURCE_DIR}/src/inc")

include(CTest)
if(BUILD_TESTING)
  add_executable(test_helpers test/test_helpers.c)
  target_link_libraries(test_helpers PRIVATE athrill_arm_common)
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_helpers PRIVATE -Wall -Wextra -Werror)
  endif()
  add_test(NAME helpers COMMAND test_helpers)
endif()
