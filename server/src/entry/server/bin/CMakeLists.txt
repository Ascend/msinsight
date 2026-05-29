project(profiler_server)

aux_source_directory(./ SERVER_BIN_SRC)
list(APPEND SERVER_SRC_LIST
        ${SERVER_BIN_SRC}
        ${U_SOCKETS_SRC_LIST}
        ${DIC_UTILS_SRC}
        ${DIC_PROTOCOL_SRC}
        ${DIC_MODULES_SRC_LIST}
        ${DIC_SERVER_SRC}
        ${DIC_CHANNEL_SRC})

add_executable(${PROJECT_NAME} ${SERVER_SRC_LIST})

set_target_properties(uv PROPERTIES EXCLUDE_FROM_ALL ON)
if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    if ((NOT CMAKE_BUILD_TYPE MATCHES "Debug") AND (NOT CMAKE_BUILD_TYPE MATCHES "PROFILE"))
        target_link_libraries(${PROJECT_NAME} -lwinpthread -lgcc_eh)
    endif()
    target_link_libraries(${PROJECT_NAME} uv_a msinsight sqlite)
else()
    target_link_libraries(${PROJECT_NAME} uv_a msinsight sqlite)
endif()
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_link_libraries(${PROJECT_NAME} stdc++fs)
endif()
