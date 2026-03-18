cmake_minimum_required(VERSION 3.16)

option(ENABLE_VALGRIND "Run tests with Valgrind memcheck" OFF)

find_package(Threads REQUIRED)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
FetchContent_MakeAvailable(googletest)

# ---------------------------------------------------------------
# Valgrind Wrapper Macro
# ---------------------------------------------------------------
macro(add_valgrind_test TARGET_NAME)
  if(ENABLE_VALGRIND)
    add_test(NAME ${TARGET_NAME}
             COMMAND valgrind --leak-check=full --error-exitcode=1 $<TARGET_FILE:${TARGET_NAME}>)
  else()
    add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
  endif()
endmacro()

# ---------------------------------------------------------------
# Test: ring_buffer
# ---------------------------------------------------------------
add_executable(test_ring_buffer test_ring_buffer.c)
target_link_libraries(test_ring_buffer PRIVATE xinputedge Threads::Threads)
target_compile_options(test_ring_buffer PRIVATE -Wall -Wextra -Werror -pedantic)
add_valgrind_test(test_ring_buffer)

# ---------------------------------------------------------------
# Test: packet
# ---------------------------------------------------------------
add_executable(test_packet test_packet.c)
target_link_libraries(test_packet PRIVATE xinputedge Threads::Threads)
target_compile_options(test_packet PRIVATE -Wall -Wextra -Werror -pedantic)
add_valgrind_test(test_packet)

# ---------------------------------------------------------------
# Test: boundary (GoogleTest)
# ---------------------------------------------------------------
add_executable(test_boundary_gtest test_boundary_gtest.cpp)
target_link_libraries(test_boundary_gtest PRIVATE xinputedge gtest_main Threads::Threads)
target_compile_options(test_boundary_gtest PRIVATE -Wall -Wextra -pedantic)
add_valgrind_test(test_boundary_gtest)
