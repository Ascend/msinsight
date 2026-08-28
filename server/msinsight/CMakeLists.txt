project(msinsight)

set(SOURCE_DIR ${CMAKE_SOURCE_DIR}/${PROJECT_NAME})
aux_source_directory(${SOURCE_DIR}/src CORE_SRC)
aux_source_directory(${SOURCE_DIR}/src/base CORE_SRC)
aux_source_directory(${SOURCE_DIR}/src/utils CORE_SRC)
list(APPEND CORE_INCLUDE
        ${SOURCE_DIR}/include
        ${SOURCE_DIR}/include/utils
        ${SOURCE_DIR}/include/base)
set(CMAKE_CXX_VISIBILITY_PRESET default)
add_library(${PROJECT_NAME} SHARED ${CORE_SRC})
target_include_directories(${PROJECT_NAME} PUBLIC ${CORE_INCLUDE})
if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    # MinGW auto-export must not expose the statically linked stack-protector runtime.
    target_link_options(${PROJECT_NAME} PRIVATE
        "-Wl,--exclude-symbols=__stack_chk_fail"
        "-Wl,--exclude-symbols=__stack_chk_fail_local"
        "-Wl,--exclude-symbols=__stack_chk_guard")
    target_link_libraries(${PROJECT_NAME} shlwapi )
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_link_libraries(${PROJECT_NAME} pthread dl stdc++fs)
else()
    target_link_libraries(${PROJECT_NAME} pthread dl)
endif()
