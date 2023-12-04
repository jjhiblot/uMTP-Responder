cmake_minimum_required(VERSION 3.5)

project(utmpresponder LANGUAGES C)


option(OLD_FUNCTIONFS_DESCRIPTORS "build with old-style FunctionFS descriptors support for old 3.15 kernels" OFF)
option(USE_SYSLOG "syslog support" ON)
option(USE_SYSTEMD "systemd support (if available)" ON)


FILE(GLOB SRC src/*.c src/mtp_operations/*.c)
add_executable(utmpresponder ${SRC})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

if (USE_SYSTEMD)
    pkg_check_modules(SYSTEMD "systemd")
    if(SYSTEMD_FOUND)
        target_compile_definitions(utmpresponder PUBLIC SYSTEMD_NOTIFY)
        if (NOT SYSTEMD_LIBRARIES)
            set(SYSTEMD_LIBRARIES -lsystemd)
        endif()
    endif()
endif()

if( USE_SYSLOG)
    target_compile_definitions(utmpresponder PUBLIC USE_SYSLOG)
endif()

target_compile_definitions(utmpresponder PRIVATE $<$<CONFIG:Debug>: DEBUG>)
target_include_directories(utmpresponder PUBLIC inc/ ${SYSTEMD_INCLUDE_DIRS})
target_compile_options(utmpresponder PUBLIC ${SYSTEMD_CFLAGS_OTHER})
target_link_libraries(utmpresponder PUBLIC Threads::Threads ${SYSTEMD_LIBRARIES})

install(TARGETS utmpresponder RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
