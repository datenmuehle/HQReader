cmake_minimum_required (VERSION 2.6)
project (433MHzThermometerReader)

set(SYSROOTPATH "~/sysroot" CACHE STRING "give pat h to sysroot directory")
#option(FCGIDBG "set to wait for debugger connection" FALSE)

#set(CMAKE_C_FLAGS "-pthread")

# The version number.
set (433MHzThermometerReader_VERSION_MAJOR 1)
set (433MHzThermometerReader_VERSION_MINOR 0)

set (PROJECT_INCLUDE_DIR "Code/inc")
set (PROJECT_SOURCE_DIR "Code/src")

#if (${FCGIDBG} EQUAL TRUE)
#    add_definitions( -DFCGIDEBUG=TRUE )
#endif(${FCGIDBG} EQUAL TRUE)

# configure a header file to pass some of the CMake settings
# to the source code
#configure_file (
#  "${PROJECT_INCLUDE_DIR}/spwrest_srv_version.h.in"
#  "${PROJECT_INCLUDE_DIR}/spwrest_srv_version.h"
#  )

include_directories("${SYSROOTPATH}/usr/include")
include_directories("${SYSROOTPATH}/home/pi/wiringPi/wiringPi/wiringPi")
include_directories("${CMAKE_SOURCE_DIR}/Code/inc")

FILE(GLOB_RECURSE TARGET_SRC  "${PROJECT_SOURCE_DIR}/*.c")
FILE(GLOB_RECURSE TARGET_INC  "${PROJECT_INCLUDE_DIR}/*.h")

add_executable(433MHzThermometerReader ${TARGET_SRC} ${TARGET_INC})

#LINK_DIRECTORIES(${SYSROOTPATH}/usr/lib/)

#MESSAGE(${SYSROOTPATH})

add_library( wiringPi SHARED IMPORTED )

set_target_properties( wiringPi PROPERTIES IMPORTED_LOCATION ${SYSROOTPATH}/usr/lib/libwiringPi.so )

TARGET_LINK_LIBRARIES(433MHzThermometerReader wiringPi)
