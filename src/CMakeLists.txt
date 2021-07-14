cmake_minimum_required(VERSION 3.05)

project(Avocado VERSION 1.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")

#set(CMAKE_CXX_STANDARD 14)
#set(CMAKE_CXX_STANDARD_REQUIRED True)

set(WFLAGS "-Wall -Wextra -Wno-deprecated-declarations -Wno-register -Wno-address-of-packed-member -Wno-vla -Wno-unused-function -Wno-maybe-uninitialized -Wno-unused-parameter -Wno-unused-variable")

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAGS} -O0 -g -std=c++17 -fsanitize=address")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAGS} -O3 -march=native -std=c++17")

configure_file(version.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/version.cpp)

find_package(folly REQUIRED)
find_package(glog REQUIRED)

add_compile_options(-DERPC_DPDK=1 -DTRANSPORT=DPDK)

include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/../cityhash/src/)
link_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../cityhash/src/.libs/)

if(SCONE)
  add_compile_options(-DSCONE=1)
  message(STATUS "Build secure version with scone")
else()
  message(STATUS "Build non-secure version for native host")
endif()

include_directories(PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/include
  COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/include
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/include/ptr_info.h
  COMMAND ./shared_ptr_size > include/ptr_info.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS shared_ptr_size
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/include
)

add_custom_target(ptr_info
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/include/ptr_info.h
)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/debug_print/include/map.h
  COMMAND make include/map.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/debug_print
)

add_custom_target(map_header
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/debug_print/include/map.h 
)

add_executable(shared_ptr_size
  ${CMAKE_CURRENT_SOURCE_DIR}/vars/shared_ptr_size.cpp
)

add_library(common
  common.cpp
  copy_utility.c
  debug_print/src/debug_print.cpp
  generate_traces.cpp
)

add_library(memtable
  memtables/memtable.cpp
  memtables/create_allocator.cpp
)

add_dependencies(common
  map_header
)

add_executable(client
  client.cpp
)

add_executable(server
  ${WRAPPER}
  server.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/version.cpp
)

add_executable(hello-server
  hello-server.cpp
  memtables/memtable.cpp
)

target_include_directories(hello-server PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/stats/release")

add_executable(city_hash_test
  tests/city_hash_test.cpp
)

add_executable(package_test
  tests/package_test.cpp
)

add_executable(package_stats_test
  tests/package_stats_test.cpp
  stats/debug/core.cpp
  stats/debug/crypt_stats.cpp
  stats/debug/crystal_clock.cpp
  stats/rdtsc.cpp
)

target_include_directories(package_stats_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/stats/debug")
target_compile_options(package_stats_test PRIVATE -DCOLLECT_STATS=1)
target_link_libraries(package_stats_test PRIVATE crypto)

add_executable(skiplist_test
  tests/skiplist_test.cpp
  memtables/memtable.cpp
)

add_executable(memtable_test
  tests/memtable_test.cpp
)


add_dependencies(server ptr_info)
add_dependencies(skiplist_test ptr_info)
if (SCONE)
  target_include_directories(hello-server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  add_library(scone
    scone.cpp
  )
else()
  target_include_directories(hello-server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  add_library(scone INTERFACE)
endif()
target_include_directories(common PUBLIC debug_print/include)
target_include_directories(server PUBLIC debug_print/include/ ${CMAKE_CURRENT_BINARY_DIR}/include)
target_include_directories(skiplist_test PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/include)
target_include_directories(memtable PUBLIC debug_print/include/ ${CMAKE_CURRENT_BINARY_DIR}/include)

if(STATS)
  add_library(stats
    stats/debug/core.cpp
    stats/debug/crypt_stats.cpp
    stats/debug/crystal_clock.cpp
    stats/debug/packet_stats.cpp
    stats/rdtsc.cpp
  )
  target_include_directories(stats PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/stats/debug")
  target_include_directories(server PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/stats/debug")
  target_include_directories(memtable PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/stats/debug")
  target_compile_options(server PRIVATE -DCOLLECT_STATS=1)
  target_compile_options(stats PRIVATE -DCOLLECT_STATS=1)
  target_compile_options(memtable PRIVATE -DCOLLECT_STATS=1)
  target_link_libraries(memtable PRIVATE stats)
else()
  add_library(stats
    stats/release/core.cpp
    stats/release/crystal_clock.cpp
  )
  target_include_directories(stats PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/stats/release")
  target_include_directories(server PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/stats/release")
endif()

target_link_libraries(server PRIVATE -Wl,--whole-archive stats -Wl,--no-whole-archive)


target_link_libraries(package_test PUBLIC crypto)
target_link_libraries(city_hash_test PUBLIC libcityhash.a)
target_link_libraries(client PUBLIC common erpc pthread numa -Wl,--whole-archive dpdk -Wl,--no-whole-archive dl crypto scone)

target_link_libraries(common
  PRIVATE
    libcityhash.a
)

target_link_libraries(memtable
  PRIVATE
    dl
    Folly::folly
    glog::glog
    double-conversion
    fmt
    crypto
)

target_link_libraries(hello-server 
  PUBLIC 
    common 
    erpc 
    -Wl,--whole-archive dpdk -Wl,--no-whole-archive 
    #    rte_ethdev
    #-Wl,-lrte_port
    scone
    numa 
    dl 
    crypto 
    Folly::folly
    glog::glog
    gflags 
    double-conversion
    pthread 
)

target_link_libraries(server 
  PRIVATE
    common
    scone
    memtable
    docopt
    fmt
    Folly::folly
    glog::glog
    double-conversion
    numa
    crypto
    erpc
    dl
    -Wl,--whole-archive dpdk -Wl,--no-whole-archive
    pthread
)

target_link_libraries(memtable_test
  PUBLIC
    common
    pthread
    memtable
    gflags
)


target_link_libraries(skiplist_test
  PUBLIC
    pthread
    dl
    Folly::folly
    glog::glog
    gflags
    double-conversion
    crypto
)

