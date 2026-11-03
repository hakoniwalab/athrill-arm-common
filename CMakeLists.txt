cmake_minimum_required(VERSION 3.16)
project(athrill_arm_common C)

# The Athrill core (for std_types.h), next to this checkout as for the targets.
set(ATHRILL_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../athrill" CACHE PATH "Athrill core source tree")

# Header-only: targets add include/ and the core's src/inc to their include path.
add_library(athrill_arm_common INTERFACE)
target_include_directories(athrill_arm_common INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${ATHRILL_SOURCE_DIR}/src/inc")

# Source-based code coverage of the tests (clang), see tools/coverage.sh.
option(ATHRILL_ARM_COMMON_COVERAGE "Build the tests with code coverage" OFF)

include(CTest)
if(BUILD_TESTING)
  add_executable(test_helpers test/test_helpers.c)
  target_link_libraries(test_helpers PRIVATE athrill_arm_common)
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_helpers PRIVATE -Wall -Wextra -Werror)
  endif()
  add_test(NAME helpers COMMAND test_helpers)

  # Pseudo-code through a test profile (test/profile/arm_target_profile.h).
  add_executable(test_pseudo test/test_pseudo.c)
  target_link_libraries(test_pseudo PRIVATE athrill_arm_common)
  target_include_directories(test_pseudo PRIVATE test/profile)
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_pseudo PRIVATE -Wall -Wextra -Werror)
  endif()
  add_test(NAME pseudo COMMAND test_pseudo)

  # Load and store pseudo-code.
  add_executable(test_pseudo_ls test/test_pseudo_ls.c)
  target_link_libraries(test_pseudo_ls PRIVATE athrill_arm_common)
  target_include_directories(test_pseudo_ls PRIVATE test/profile)
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_pseudo_ls PRIVATE -Wall -Wextra -Werror)
  endif()
  add_test(NAME pseudo-ls COMMAND test_pseudo_ls)

  # Extend, bitfield, bit-operation, multiply and divide pseudo-code.
  add_executable(test_pseudo_misc test/test_pseudo_misc.c)
  target_link_libraries(test_pseudo_misc PRIVATE athrill_arm_common)
  target_include_directories(test_pseudo_misc PRIVATE test/profile)
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_pseudo_misc PRIVATE -Wall -Wextra -Werror)
  endif()
  add_test(NAME pseudo-misc COMMAND test_pseudo_misc)

  # IT, MRS and MSR pseudo-code.
  add_executable(test_pseudo_system test/test_pseudo_system.c)
  target_link_libraries(test_pseudo_system PRIVATE athrill_arm_common)
  target_include_directories(test_pseudo_system PRIVATE test/profile)
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_pseudo_system PRIVATE -Wall -Wextra -Werror)
  endif()
  add_test(NAME pseudo-system COMMAND test_pseudo_system)

  if(ATHRILL_ARM_COMMON_COVERAGE)
    foreach(t test_helpers test_pseudo test_pseudo_ls test_pseudo_misc test_pseudo_system)
      target_compile_options(${t} PRIVATE -fprofile-instr-generate -fcoverage-mapping -O0)
      target_link_options(${t} PRIVATE -fprofile-instr-generate)
    endforeach()
  endif()
endif()
