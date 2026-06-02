message(STATUS "===============================third party cmake begin==============================")

set(THIRD_PARTY_DIR ${HOME_DIR}/third_party)

message(STATUS "THIRD_PARTY_DIR: ${THIRD_PARTY_DIR}")

# googletest
set(GTEST_INCLUDE_DIR ${THIRD_PARTY_DIR}/googletest/googletest/include)
message(STATUS "GTEST_INCLUDE_DIR: ${GTEST_INCLUDE_DIR}")

# googlemock
set(GMOCK_INCLUDE_DIR ${THIRD_PARTY_DIR}/googletest/googlemock/include)
message(STATUS "GMOCK_INCLUDE_DIR: ${GMOCK_INCLUDE_DIR}")

# sqlite
set(SQLITE_INCLUDE_DIR ${THIRD_PARTY_DIR}/sqlite/include)
message(STATUS "SQLITE_INCLUDE_DIR: ${SQLITE_INCLUDE_DIR}")

# libuv
set(LIBUV_INCLUDE_DIR ${THIRD_PARTY_DIR}/libuv/include)
message(STATUS "LIBUV_INCLUDE_DIR: ${LIBUV_INCLUDE_DIR}")

# rapidjson
set(RAPIDJSON_INCLUDE_DIR ${THIRD_PARTY_DIR}/rapidjson/include/rapidjson)
message(STATUS "RAPIDJSON_INCLUDE_DIR: ${RAPIDJSON_INCLUDE_DIR}")

# uSockets
set(U_SOCKETS_INCLUDE_DIR ${THIRD_PARTY_DIR}/uSockets/src)
message(STATUS "U_SOCKETS_INCLUDE_DIR: ${U_SOCKETS_INCLUDE_DIR}")

# uSockets source code
set(U_SOCKETS_SRC_DIR ${THIRD_PARTY_DIR}/uSockets/src)
message(STATUS "U_SOCKETS_SRC_DIR: ${U_SOCKETS_SRC_DIR}")

# uWebSockets
set(U_WEBSOCKETS_INCLUDE_DIR ${THIRD_PARTY_DIR}/uWebSockets/src)
message(STATUS "U_WEBSOCKETS_INCLUDE_DIR: ${U_WEBSOCKETS_INCLUDE_DIR}")

# third party include source
include_directories(${RAPIDJSON_INCLUDE_DIR})
include_directories(${LIBUV_INCLUDE_DIR})
include_directories(${U_SOCKETS_INCLUDE_DIR})
include_directories(${U_WEBSOCKETS_INCLUDE_DIR})
include_directories(${SQLITE_INCLUDE_DIR})

# uWebSockets & uSockets source
aux_source_directory(${U_SOCKETS_SRC_DIR} U_SOCKETS_SRC)
aux_source_directory(${U_SOCKETS_SRC_DIR}/crypto U_SOCKETS_SRC_CRYPTO)
aux_source_directory(${U_SOCKETS_SRC_DIR}/eventing U_SOCKETS_SRC_EVENTING)
aux_source_directory(${U_SOCKETS_SRC_DIR}/internal U_SOCKETS_SRC_INTERNAL)

list(APPEND U_SOCKETS_SRC_LIST
        ${U_SOCKETS_SRC}
        ${U_SOCKETS_SRC_CRYPTO}
        ${U_SOCKETS_SRC_EVENTING}
        ${U_SOCKETS_SRC_INTERNAL})

add_subdirectory(${THIRD_PARTY_DIR})

message(STATUS "===============================third party cmake end==============================")
