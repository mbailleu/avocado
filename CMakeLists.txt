cmake_minimum_required(VERSION 3.10)

project(Avocado VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(DPDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SpeicherDPDK" CACHE STRING "Path to dpdk source")
set(DPDK_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/dpdk/build" CACHE STRING "Path to dpdk build dir")
set(DPDK_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/dpdk/bin" CACHE STRING "Path to the dpdk installation")
set(CITYHASH_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cityhash" CACHE STRING "Path to the cityhash sources")
set(CITYHASH_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/cityhash" CACHE STRING "Path to the cityhash build directory")

option(SCONE "Enable or disable scone support" OFF)

if(SCONE)
  set(SCONE_FLAG "-DSCONE=1")
endif()

include_directories(SYSTEM "${DPDK_INSTALL_DIR}/usr/local/include/dpdk")
link_directories("${DPDK_INSTALL_DIR}/usr/local/lib")


add_custom_command(
  OUTPUT ${CITYHASH_BINARY_DIR}/Makefile
  COMMAND mkdir -p ${CITYHASH_BINARY_DIR}
  COMMAND cd ${CITYHASH_BINARY_DIR} && ${CITYHASH_DIR}/./configure
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_command(
  OUTPUT ${CITYHASH_BINARY_DIR}/src/libcityhash.la
  COMMAND make
  WORKING_DIRECTORY ${CITYHASH_BINARY_DIR}
  DEPENDS ${CITYHASH_BINARY_DIR}/Makefile
)

add_custom_target(cityhash_lib
  DEPENDS ${CITYHASH_BINARY_DIR}/src/libcityhash.la
)

configure_file(dpdk-build.sh.in ${CMAKE_CURRENT_BINARY_DIR}/dpdk-build.sh)

add_custom_command(
  OUTPUT ${DPDK_BINARY_DIR}/lib/libdpdk.a
  COMMAND ./dpdk-build.sh
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/dpdk-build.sh
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_command(
  OUTPUT ${DPDK_INSTALL_DIR}/usr/local/lib/libdpdk.a
  COMMAND make install DESTDIR=${DPDK_INSTALL_DIR}
  DEPENDS ${DPDK_BINARY_DIR}/lib/libdpdk.a
  WORKING_DIRECTORY ${DPDK_BINARY_DIR}
)

add_custom_target(dpdk_lib
  DEPENDS ${DPDK_INSTALL_DIR}/usr/local/lib/libdpdk.a
)

set(LTO OFF)
set(TRANSPORT dpdk)
set(PERF ON)

add_subdirectory(eRPC)
add_dependencies(erpc dpdk_lib)

include_directories("${CMAKE_CURRENT_BINARY_DIR}/eRPC/src" "${CMAKE_CURRENT_SOURCE_DIR}/eRPC/src")

add_subdirectory(src)
add_dependencies(server cityhash_lib dpdk_lib)
target_link_directories(server PRIVATE ${CITYHASH_BINARY_DIR}/src/.libs ${DPDK_INSTALL_DIR})
target_include_directories(server PRIVATE ${CITYHASH_DIR}/src ${DPDK_INSTALL_DIR})

add_dependencies(client dpdk_lib)
target_link_directories(client PRIVATE ${CITYHASH_BINARY_DIR}/src/.libs)

add_dependencies(hello-server cityhash_lib dpdk_lib)
target_link_directories(hello-server PRIVATE ${CITYHASH_BINARY_DIR}/src/.libs ${DPDK_INSTALL_DIR})
target_include_directories(hello-server PRIVATE ${CITYHASH_DIR}/src ${DPDK_INSTALL_DIR})

add_dependencies(memtable cityhash_lib)
target_link_directories(memtable_test PRIVATE ${CITYHASH_BINARY_DIR}/src/.libs)

add_dependencies(city_hash_test cityhash_lib)
target_link_directories(city_hash_test PRIVATE ${CITYHASH_BINARY_DIR}/src/.libs)
