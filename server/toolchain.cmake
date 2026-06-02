set(CMAKE_SYSTEM_NAME "Windows")
message(STATUS "System name = ${CMAKE_SYSTEM_NAME}")
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER /usr/bin/x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/x86_64-w64-mingw32-g++)
set(CMAKE_INCLUDE_PATH /usr/x86_64-w64-mingw32/include)
set(HUGETLBFS_LIBRARY /usr/x86_64-w64-mingw32/lib)

set(CMAKE_PREFIX_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_INSTALL_PREFIX /usr/x86_64-w64-mingw32)

SET(CMAKE_FIND_ROOT_PATH  /usr/x86_64-w64-mingw32)
set(CMAKE_LIBRARY_PATH ${CMAKE_PREFIX_PATH}/lib)

# for libraries and headers in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
