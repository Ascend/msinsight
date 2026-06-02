project(third_party)

set(CMAKE_CXX_STANDARD 17)

if (UNIT_TEST_ENABLE)
add_subdirectory(googletest)
endif()

add_subdirectory(libuv)
add_subdirectory(sqlite)

# json_modern_c++ source introduction
# uWebSockets & uSockets source introduction
