cmake_minimum_required(VERSION 3.0)

project(compile_asm)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

enable_language(ASM)

if (${CMAKE_SYSTEM_NAME} MATCHES Darwin)
  add_compile_definitions(CHIAOSX=1)
endif()
add_compile_definitions(
  VDF_MODE=0
  FAST_MACHINE=1
  )

if (${CMAKE_CXX_COMPILER_ID} MATCHES ^.*Clang.*$)
  set(NOPIE "-fno-PIE")
else()
  set(NOPIE "-no-pie")
endif()

string(TOLOWER "${CMAKE_BUILD_TYPE}" CONF)
if (NOT CONF MATCHES "debug")
  set(LTOFLAG " -flto")
  message(STATUS "LTO is on")
endif()

set(COMPILE_ASM_LDFLAGS "${NOPIE}${LTOFLAG}")

set(CXX_FLAGS "-pthread ${NOPIE}${LTOFLAG} -fvisibility=hidden")
set(CMAKE_CXX_FLAGS_DEBUG "${CXX_FLAGS} -g2 -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CXX_FLAGS} -O3")

include(FetchContent)
FetchContent_Declare(
  chiavdf
  GIT_REPOSITORY https://github.com/Chia-Network/chiavdf
  GIT_TAG 1.0.3
  )
FetchContent_Declare(
  plog
  GIT_REPOSITORY https://github.com/sergiusthebest/plog
  GIT_TAG 1.1.6
  )
FetchContent_Declare(
  gtest
  GIT_REPOSITORY https://github.com/google/googletest
  GIT_TAG release-1.11.0
  )
FetchContent_MakeAvailable(chiavdf plog gtest)

set(CHIAVDF_DIR ${CMAKE_BINARY_DIR}/_deps/chiavdf-src)

set(CMAKE_MODULE_PATH
  ${CHIAVDF_DIR}/src/cmake
  ${CMAKE_MODULE_PATH}
  )

find_package(GMP REQUIRED)
find_package(GMPXX REQUIRED)
find_package(Boost 1.6 REQUIRED COMPONENTS program_options)
find_package(Threads)
find_package(Protobuf REQUIRED)

message(STATUS ${plog_SOURCE_DIR})

include_directories(
  ${GMP_INCLUDE_DIR}
  ${GMPXX_INCLUDE_DIR}
  ${CHIAVDF_DIR}/src
  ${CMAKE_BINARY_DIR}
  ${plog_SOURCE_DIR}/include
  )

set(COMPILE_ASM_SRC
  ${CHIAVDF_DIR}/src/xgcd_partial.c
  ${CHIAVDF_DIR}/src/compile_asm.cpp
  )

set(CHIAVDF_SRC
  ${CHIAVDF_DIR}/src/refcode/lzcnt.c
  )

add_executable(compile_asm ${COMPILE_ASM_SRC})
target_compile_definitions(compile_asm PRIVATE ${COMPILE_ASM_DEFS})
target_link_libraries(compile_asm PRIVATE ${COMPILE_ASM_LDFLAGS} ${GMP_LIBRARIES} ${GMPXX_LIBRARIES})

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/asm_compiled.s COMMAND ${CMAKE_BINARY_DIR}/compile_asm DEPENDS compile_asm)
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/avx2_asm_compiled.s COMMAND ${CMAKE_BINARY_DIR}/compile_asm avx2 DEPENDS compile_asm)
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/avx512_asm_compiled.s COMMAND ${CMAKE_BINARY_DIR}/compile_asm avx512 DEPENDS compile_asm)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ./src/vdf_msgs.proto)

set(VDF_LIB_SRCS
  src/vdf_computer.cpp
  ${CHIAVDF_SRC}
  ${CMAKE_BINARY_DIR}/asm_compiled.s
  ${CMAKE_BINARY_DIR}/avx2_asm_compiled.s
  ${CMAKE_BINARY_DIR}/avx512_asm_compiled.s
  )
add_library(vdf ${VDF_LIB_SRCS})
target_link_libraries(vdf PRIVATE ${COMPILE_ASM_LDFLAGS} ${GMP_LIBRARIES} ${GMPXX_LIBRARIES})

install(FILES src/vdf_computer.h DESTINATION include)
install(TARGETS vdf DESTINATION lib)

set(VDF_SRV_SRCS
  src/vdf_server.cpp
  src/vdf_computer.cpp
  src/vdf_net.cpp
  ${CHIAVDF_SRC}
  ${CMAKE_BINARY_DIR}/asm_compiled.s
  ${CMAKE_BINARY_DIR}/avx2_asm_compiled.s
  ${CMAKE_BINARY_DIR}/avx512_asm_compiled.s
  )
add_executable(vdf_srv
  ${VDF_SRV_SRCS}
  ${PROTO_SRCS}
  )
target_link_libraries(vdf_srv PRIVATE
  ${COMPILE_ASM_LDFLAGS}
  ${GMP_LIBRARIES}
  ${GMPXX_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
  Boost::program_options
  protobuf::libprotobuf
  )

find_package(GTest REQUIRED)
enable_testing()
# VDF test target
set(VDF_TEST_SRCS
  src/vdf_test.cpp
  src/vdf_computer.cpp
  ${CHIAVDF_SRC}
  ${CMAKE_BINARY_DIR}/asm_compiled.s
  ${CMAKE_BINARY_DIR}/avx2_asm_compiled.s
  ${CMAKE_BINARY_DIR}/avx512_asm_compiled.s
  )
add_executable(vdf_test ${VDF_TEST_SRCS})
target_link_libraries(vdf_test PRIVATE ${COMPILE_ASM_LDFLAGS}
  GTest::gtest GTest::gtest_main ${GMP_LIBRARIES} ${GMPXX_LIBRARIES})
add_test(vdf_test COMMAND vdf_test)
# VDF server test target
set(VDF_SRV_TEST_SRCS
  src/vdf_net.cpp
  src/vdf_srv_test.cpp
  )
add_executable(vdf_srv_test ${VDF_SRV_TEST_SRCS})
target_link_libraries(vdf_srv_test PRIVATE GTest::gtest GTest::gtest_main)
add_test(vdf_srv_test COMMAND vdf_srv_test)
