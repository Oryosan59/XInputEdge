cmake_minimum_required(VERSION 3.16)

find_package(Threads REQUIRED)

# ---------------------------------------------------------------
# Test: ring_buffer
# ---------------------------------------------------------------
add_executable(test_ring_buffer test_ring_buffer.c)
target_link_libraries(test_ring_buffer PRIVATE xinputedge Threads::Threads)
target_compile_options(test_ring_buffer PRIVATE -Wall -Wextra -Werror -pedantic)
add_test(NAME ring_buffer COMMAND test_ring_buffer)

# ---------------------------------------------------------------
# Test: packet
# ---------------------------------------------------------------
add_executable(test_packet test_packet.c)
target_link_libraries(test_packet PRIVATE xinputedge Threads::Threads)
target_compile_options(test_packet PRIVATE -Wall -Wextra -Werror -pedantic)
add_test(NAME packet COMMAND test_packet)
