cmake_minimum_required(VERSION 3.16)
project(insight_server_test VERSION 0.1.0)

include(cmake/third_party.cmake)

set(SRC_HOME_DIR ${HOME_DIR}/src)
set(TEST_HOME_DIR ${HOME_DIR}/src/test)

aux_source_directory(${TEST_HOME_DIR} TEST)
aux_source_directory(${TEST_HOME_DIR}/utils TEST)
aux_source_directory(${TEST_HOME_DIR}/server TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/communication/handler TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/communication/protocol TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/communication/handler TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/communication/core TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/communication/advisor TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/memory TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/memory/database TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/memory/handler TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/memory/service TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/memory/parser TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/memory/protocol TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/memory/table TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/memscope TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/memsnapshot TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/summary TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/summary/core TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/summary/protocol TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/summary/handler TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/summary/core TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/timeline TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/timeline/core/analyze TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/timeline/core/parser TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/timeline/core/kernelE2E TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/timeline/core/systemViewOverall TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/timeline/handler TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/timeline/protocol TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/timeline/database TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/timeline/analyzer TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/timeline/dataEngine TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/timeline/table TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/timeline/parser TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/timeline/cache TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/communication TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/communication/parser TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/advisor TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/advisor/process TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/advisor/protocol TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/advisor/handler TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/operator TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/operator/database TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/operator/parser TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/operator/protocol TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/operator/handler TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/common TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/defs TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/source TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/source/mockUtils TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/source/handler TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/source/parser TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/global TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/global/core TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/global/protocol TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/global/handler TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/servitization TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/servitization/protocol TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/servitization/repo TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/servitization/handler TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/servitization/context TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/servitization/api TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/full_db TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/full_db/parserAlloc TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/base/core TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/jupyter/protocol TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/jupyter/parser TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/rl/core TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/rl/protocol TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/rl/handler TEST)
aux_source_directory(${TEST_HOME_DIR}/performance PERFORMANCE_TEST)
aux_source_directory(${HOME_DIR}/src/protocol TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/rl TEST)
aux_source_directory(${TEST_HOME_DIR}/framework TEST)
aux_source_directory(${TEST_HOME_DIR}/modules/triton TEST)

# 增加头文件include目录
include_directories(
        ${TEST_HOME_DIR}/modules/memory
        ${TEST_HOME_DIR}/mock
        ${TEST_HOME_DIR}
        ${TEST_HOME_DIR}/framework
)

list(APPEND DIC_TEST_LIST
        ${TEST}
        ${U_SOCKETS_SRC_LIST}
        ${DIC_UTILS_SRC}
        ${DIC_PROTOCOL_SRC}
        ${DIC_MODULES_SRC_LIST}
        ${DIC_SERVER_SRC}
        ${DIC_CHANNEL_SRC})

list(APPEND DIC_PERFORMANCE_TEST_LIST
        ${PERFORMANCE_TEST}
        ${U_SOCKETS_SRC_LIST}
        ${DIC_UTILS_SRC}
        ${DIC_PROTOCOL_SRC}
        ${DIC_MODULES_SRC_LIST}
        ${DIC_SERVER_SRC}
        ${DIC_CHANNEL_SRC})

# mockcpp
set(MOCKCPP_INCLUDE_DIR ${HOME_DIR}/output/mockcpp/include)
message(STATUS "MOCKCPP_INCLUDE_DIR: ${MOCKCPP_INCLUDE_DIR}")

add_executable(insight_test ${DIC_TEST_LIST} modules/summary/SummaryTest.cpp modules/summary/MockTraceDatabase.h TestSuit.cpp
        FullDbTestSuit.cpp modules/timeline/handler/HandlerTest.cpp modules/timeline/protocol/ProtocolTest.cpp
        ../../msinsight/include/utils/NumberSafeUtil.h
        utils/NumberSafeUtilTest.cpp)
add_dependencies(insight_test mockcpp_build)
target_include_directories(insight_test PUBLIC ${HOME_DIR}/src/protocol)
target_include_directories(insight_test PRIVATE ${MOCKCPP_INCLUDE_DIR})
target_link_directories(insight_test PRIVATE ${HOME_DIR}/output/mockcpp/lib)
target_link_libraries(insight_test msinsight)

add_executable(insight_performance ${DIC_PERFORMANCE_TEST_LIST})
add_dependencies(insight_performance mockcpp_build)
target_include_directories(insight_performance PRIVATE ${MOCKCPP_INCLUDE_DIR})
target_link_directories(insight_performance PRIVATE ${HOME_DIR}/output/mockcpp/lib)
target_link_libraries(insight_performance msinsight)

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_link_libraries(insight_test
            gtest_main gmock_main mockcpp
            uv_a userenv ws2_32 iphlpapi shlwapi -lssp z
            ${EXECUTABLE_OUTPUT_PATH}/libsqlite.dll)
    target_link_libraries(insight_performance
            gtest_main gmock_main mockcpp
            uv_a userenv ws2_32 iphlpapi shlwapi -lssp z
            ${EXECUTABLE_OUTPUT_PATH}/libsqlite.dll)
else()
    target_link_libraries(insight_test
            gtest_main gmock_main mockcpp
            uv_a pthread dl sqlite z)
    target_link_libraries(insight_performance
            gtest_main gmock_main mockcpp
            uv_a pthread dl sqlite z)
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_link_libraries(insight_performance stdc++fs)
    target_link_libraries(insight_test stdc++fs)
endif()

if (${_PROJECT_TYPE} MATCHES "fuzz")
    add_subdirectory(fuzz)
endif ()

# Split debug info from insight_test.exe in Debug build on Windows to avoid
# PE loader failure caused by excessively large executable (>1.9GB).
if (CMAKE_BUILD_TYPE MATCHES "Debug" AND CMAKE_SYSTEM_NAME MATCHES "Windows")
    add_custom_command(TARGET insight_test POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} --only-keep-debug $<TARGET_FILE:insight_test> $<TARGET_FILE:insight_test>.debug
        COMMAND ${CMAKE_STRIP} --strip-debug $<TARGET_FILE:insight_test>
        COMMAND ${CMAKE_OBJCOPY} --add-gnu-debuglink=$<TARGET_FILE:insight_test>.debug $<TARGET_FILE:insight_test>
        COMMENT "Splitting debug info from insight_test.exe"
    )
    add_custom_command(TARGET insight_performance POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} --only-keep-debug $<TARGET_FILE:insight_performance> $<TARGET_FILE:insight_performance>.debug
        COMMAND ${CMAKE_STRIP} --strip-debug $<TARGET_FILE:insight_performance>
        COMMAND ${CMAKE_OBJCOPY} --add-gnu-debuglink=$<TARGET_FILE:insight_performance>.debug $<TARGET_FILE:insight_performance>
        COMMENT "Splitting debug info from insight_performance.exe"
    )
endif()
