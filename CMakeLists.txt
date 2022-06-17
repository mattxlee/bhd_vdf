cmake_minimum_required(VERSION 3.0)

project(bhd_vdf)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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

option(BUILD_SRV "Build bhd_srv" OFF)
option(BUILD_TEST "Build test" OFF)

include(FetchContent)
FetchContent_Declare(
    chiavdf
    GIT_REPOSITORY https://github.com/Chia-Network/chiavdf
    GIT_TAG 1.0.6
    PATCH_COMMAND git apply --reject ${CMAKE_CURRENT_SOURCE_DIR}/chiavdf.patch || true
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

set(CMAKE_MODULE_PATH
    ${chiavdf_SOURCE_DIR}/src/cmake
    ${CMAKE_MODULE_PATH}
    )

set(GMP_PREFIX_DIR "${CMAKE_CURRENT_BINARY_DIR}/gmp")
set(GMP_SOURCE_DIR "${GMP_PREFIX_DIR}/src")

set(GMP_INCLUDE_DIR ${GMP_PREFIX_DIR}/include)
set(GMPXX_INCLUDE_DIR ${GMP_PREFIX_DIR}/include)

include(ExternalProject)
ExternalProject_Add(GMP
    URL https://gmplib.org/download/gmp/gmp-6.2.1.tar.lz
    URL_HASH SHA256=2c7f4f0d370801b2849c48c9ef3f59553b5f1d3791d070cffb04599f9fc67b41
    SOURCE_DIR "${GMP_SOURCE_DIR}"
    CONFIGURE_COMMAND ./configure --enable-cxx --enable-static --enable-fat --prefix=${GMP_PREFIX_DIR} --host=${TOOLCHAIN_PREFIX} CC_FOR_BUILD=gcc
    TEST_COMMAND ""
    BUILD_IN_SOURCE 1
    )
set(GMP_LIBRARIES ${GMP_PREFIX_DIR}/lib/libgmp.a)
set(GMPXX_LIBRARIES ${GMP_PREFIX_DIR}/lib/libgmpxx.a)

find_package(Threads)

message(STATUS ${plog_SOURCE_DIR})

include_directories(
    ${GMP_INCLUDE_DIR}
    ${GMPXX_INCLUDE_DIR}
    ${chiavdf_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
    ${plog_SOURCE_DIR}/include
    )

set(COMPILE_ASM_SRC
    ${chiavdf_SOURCE_DIR}/src/xgcd_partial.c
    ${chiavdf_SOURCE_DIR}/src/compile_asm.cpp
    )

set(CHIAVDF_SRC
    ${chiavdf_SOURCE_DIR}/src/refcode/lzcnt.c
    )

add_executable(compile_asm ${COMPILE_ASM_SRC})
target_compile_definitions(compile_asm PRIVATE ${COMPILE_ASM_DEFS})
target_link_libraries(compile_asm PRIVATE ${COMPILE_ASM_LDFLAGS} ${GMP_LIBRARIES} ${GMPXX_LIBRARIES})

add_custom_command(OUTPUT ./asm_compiled.s COMMAND ./compile_asm DEPENDS compile_asm)
add_custom_command(OUTPUT ./avx2_asm_compiled.s COMMAND ./compile_asm avx2 DEPENDS compile_asm)
add_custom_command(OUTPUT ./avx512_asm_compiled.s COMMAND ./compile_asm avx512 DEPENDS compile_asm)

set(VDF_LIB_SRCS
    src/vdf_computer.cpp
    ${CHIAVDF_SRC}
    ./asm_compiled.s
    ./avx2_asm_compiled.s
    ./avx512_asm_compiled.s
    )
add_library(bhd_vdf ${VDF_LIB_SRCS})
add_dependencies(bhd_vdf GMP)
target_link_libraries(bhd_vdf PRIVATE ${COMPILE_ASM_LDFLAGS} ${GMP_LIBRARIES} ${GMPXX_LIBRARIES})

set(INSTALL_INCLUDE_FILES
    src/vdf_types.h
    src/vdf_computer.h
    )

install(FILES ${INSTALL_INCLUDE_FILES} DESTINATION include)
install(TARGETS bhd_vdf DESTINATION lib)

if (BUILD_SRV)
    find_package(Protobuf REQUIRED)
    find_package(Boost REQUIRED COMPONENTS program_options)
    protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ./src/vdf_msgs.proto)
    set(VDF_SRV_SRCS
        src/vdf_server.cpp
        src/vdf_computer.cpp
        src/vdf_net.cpp
        src/vdf_utils.cpp
        ${CHIAVDF_SRC}
        ./asm_compiled.s
        ./avx2_asm_compiled.s
        ./avx512_asm_compiled.s
        )
    add_executable(vdf_srv
        ${VDF_SRV_SRCS}
        ${PROTO_SRCS}
        )
    add_dependencies(vdf_srv GMP)
    target_link_libraries(vdf_srv PRIVATE
        ${COMPILE_ASM_LDFLAGS}
        ${GMP_LIBRARIES}
        ${GMPXX_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        Boost::program_options
        protobuf::libprotobuf
        )
endif(BUILD_SRV)

if (BUILD_TEST)
    find_package(Protobuf REQUIRED)
    find_package(GTest REQUIRED)
    protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ./src/vdf_msgs.proto)
    enable_testing()
    # VDF test target
    set(VDF_TEST_SRCS
        src/vdf_test.cpp
        src/vdf_computer.cpp
        ${CHIAVDF_SRC}
        ./asm_compiled.s
        ./avx2_asm_compiled.s
        ./avx512_asm_compiled.s
        )
    add_executable(vdf_test ${VDF_TEST_SRCS})
    add_dependencies(vdf_test GMP)
    target_link_libraries(vdf_test PRIVATE ${COMPILE_ASM_LDFLAGS}
        GTest::gtest GTest::gtest_main ${GMP_LIBRARIES} ${GMPXX_LIBRARIES})
    add_test(vdf_test COMMAND vdf_test)
    # VDF server test target
    set(VDF_SRV_TEST_SRCS
        src/vdf_net.cpp
        src/vdf_computer.cpp
        src/vdf_utils.cpp
        src/vdf_srv_test.cpp
        ${CHIAVDF_SRC}
        ${PROTO_SRCS}
        ./asm_compiled.s
        ./avx2_asm_compiled.s
        ./avx512_asm_compiled.s
        )
    add_executable(vdf_srv_test ${VDF_SRV_TEST_SRCS})
    add_dependencies(vdf_srv_test GMP)
    target_link_libraries(vdf_srv_test PRIVATE
        ${COMPILE_ASM_LDFLAGS}
        GTest::gtest GTest::gtest_main ${GMP_LIBRARIES} ${GMPXX_LIBRARIES}
        protobuf::libprotobuf
        )
    add_test(vdf_srv_test COMMAND vdf_srv_test)
endif(BUILD_TEST)
