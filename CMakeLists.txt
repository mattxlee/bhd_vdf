cmake_minimum_required(VERSION 3.0)

project(bhd_vdf)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

enable_language(ASM)

option(BUILD_COMPILE_ASM "Build compile_asm and use it to generate .s files" OFF)
option(GENERATE_ASM_FILES "Generate .s files by using compile_asm, affect only when BUILD_COMPILE_ASM is ON" OFF)
option(BUILD_TEST "Build test" OFF)

if (${CMAKE_SYSTEM_NAME} MATCHES Darwin)
    add_compile_definitions(CHIAOSX=1)
endif()
add_compile_definitions(
    VDF_MODE=0
    FAST_MACHINE=1
    )

if (${CMAKE_CXX_COMPILER_ID} MATCHES ^.*Clang.*$)
    set(PIEFLAG "-fno-PIE")
    set(PIELINKOPTS "-no-pie")
else()
    set(PIEFLAG "-no-pie")
    set(PIELINKOPTS "-no-pie")
endif()

# LTO flag will only apply to release version
if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_compile_definitions(DEBUG=1)
    set(OPTIMIZEFLAGS "-O0")
else()
    add_compile_definitions(NDEBUG=1)
    set(LTOFLAG "-flto")
    message(STATUS "LTO is on")
    set(OPTIMIZEFLAGS "-O2")
endif()
set(VISIBILITY_HIDDEN_FLAG "-fvisibility=hidden")

include(FetchContent)
FetchContent_Declare(
    chiavdf
    GIT_REPOSITORY https://github.com/Chia-Network/chiavdf
    GIT_TAG 1.0.6
    )
FetchContent_MakeAvailable(chiavdf)

# Setup the way to retrieve GMP source
set(GMP_PREFIX_DIR ${CMAKE_CURRENT_BINARY_DIR}/gmp)
set(GMP_SOURCE_DIR ${GMP_PREFIX_DIR}/src)
include(ExternalProject)
ExternalProject_Add(GMP
    URL https://gmplib.org/download/gmp/gmp-6.2.1.tar.lz
    URL_HASH SHA256=2c7f4f0d370801b2849c48c9ef3f59553b5f1d3791d070cffb04599f9fc67b41
    SOURCE_DIR "${GMP_SOURCE_DIR}"
    CONFIGURE_COMMAND CPPFLAGS=-D__USE_MINGW_ANSI_STDIO=1 ./configure --enable-cxx --enable-fat --enable-static --prefix=${GMP_PREFIX_DIR} --host=${TOOLCHAIN_PREFIX} CC_FOR_BUILD=gcc
    TEST_COMMAND ""
    BUILD_IN_SOURCE 1
    PATCH_COMMAND git apply --reject ${CMAKE_CURRENT_SOURCE_DIR}/chiavdf-remove_boost.patch || true
    )
set(GMP_INCLUDE_DIR ${GMP_PREFIX_DIR}/include)
set(GMPXX_INCLUDE_DIR ${GMP_PREFIX_DIR}/include)
set(GMP_LIBRARIES ${GMP_PREFIX_DIR}/lib/libgmp.a)
set(GMPXX_LIBRARIES ${GMP_PREFIX_DIR}/lib/libgmpxx.a)

find_package(Threads)

if (BUILD_COMPILE_ASM)
    set(COMPILE_ASM_SRC
        ${chiavdf_SOURCE_DIR}/src/xgcd_partial.c
        ${chiavdf_SOURCE_DIR}/src/compile_asm.cpp
        ${UINT128_SRCS}
        )
    add_executable(compile_asm ${COMPILE_ASM_SRC})
    add_dependencies(compile_asm GMP)
    target_compile_options(compile_asm PRIVATE ${PIEFLAG} ${OPTIMIZEFLAGS} ${VISIBILITY_HIDDEN_FLAG} -g)
    target_link_options(compile_asm PRIVATE ${PIELINKOPTS} ${LTOFLAG})
    target_link_libraries(compile_asm PRIVATE ${GMP_LIBRARIES} ${GMPXX_LIBRARIES})
    target_include_directories(compile_asm PRIVATE ${GMP_INCLUDE_DIR} ${GMPXX_INCLUDE_DIR})
    if (GENERATE_ASM_FILES)
        set(COMPILE_ASM_EXE "compile_asm")
        add_custom_command(OUTPUT ./asm_compiled.s COMMAND ./${COMPILE_ASM_EXE} DEPENDS compile_asm)
        add_custom_command(OUTPUT ./avx2_asm_compiled.s COMMAND ./${COMPILE_ASM_EXE} avx2 DEPENDS compile_asm)
        add_custom_command(OUTPUT ./avx512_asm_compiled.s COMMAND ./${COMPILE_ASM_EXE} avx512 DEPENDS compile_asm)
    endif()
endif()

if (NOT WIN32)
    add_definitions(-DUSE_VDF_COMPUTER=1)
    set(ASM_FILES
        src/unix/asm_compiled.s
        src/unix/avx2_asm_compiled.s
        src/unix/avx512_asm_compiled.s
        )
endif()
set(CHIAVDF_SRC
    ${chiavdf_SOURCE_DIR}/src/refcode/lzcnt.c
    ${ASM_FILES}
    )
set(VDF_LIB_SRCS
    src/vdf_computer.cpp
    ${CHIAVDF_SRC}
    )
add_library(bhd_vdf STATIC ${VDF_LIB_SRCS})
add_dependencies(bhd_vdf GMP)
if (NOT WIN32)
    target_compile_options(bhd_vdf PRIVATE ${PIEFLAG} ${OPTIMIZEFLAGS} ${VISIBILITY_HIDDEN_FLAG} -g)
    target_link_options(bhd_vdf PRIVATE ${PIELINKOPTS} ${LTOFLAG})
endif()
target_include_directories(bhd_vdf PRIVATE
    ${GMP_INCLUDE_DIR}
    ${GMPXX_INCLUDE_DIR}
    ${chiavdf_SOURCE_DIR}/src
    )
target_link_libraries(bhd_vdf PRIVATE ${GMP_LIBRARIES} ${GMPXX_LIBRARIES})

set(INSTALL_INCLUDE_FILES
    src/vdf_types.h
    src/vdf_computer.h
    ${chiavdf_SOURCE_DIR}/src/verifier.h
    )
install(FILES ${INSTALL_INCLUDE_FILES} DESTINATION include)
install(TARGETS bhd_vdf DESTINATION lib)

if (BUILD_TEST)
    set(BUILD_GMOCK OFF CACHE INTERNAL "Do not compile GMOCK")
    set(INSTALL_GTEST OFF CACHE INTERNAL "Do not install GTEST")
    FetchContent_Declare(
        gtest
        GIT_REPOSITORY https://github.com/google/googletest
        GIT_TAG release-1.11.0
        )
    FetchContent_MakeAvailable(gtest)
    enable_testing()
    set(VDF_TEST_SRCS
        src/vdf_test.cpp
        src/vdf_computer.cpp
        ${CHIAVDF_SRC}
        )
    add_executable(vdf_test ${VDF_TEST_SRCS})
    add_dependencies(vdf_test GMP)
    if (NOT WIN32)
        target_compile_options(vdf_test PRIVATE ${PIEFLAG} ${OPTIMIZEFLAGS} ${VISIBILITY_HIDDEN_FLAG} -g)
        target_link_options(vdf_test PRIVATE ${PIELINKOPTS} ${LTOFLAG})
    endif()
    target_include_directories(vdf_test PRIVATE
        ${GMP_INCLUDE_DIR}
        ${GMPXX_INCLUDE_DIR}
        ${chiavdf_SOURCE_DIR}/src
        )
    target_link_libraries(vdf_test PRIVATE GTest::gtest GTest::gtest_main ${GMP_LIBRARIES} ${GMPXX_LIBRARIES})
    add_test(vdf_test COMMAND vdf_test)
endif(BUILD_TEST)
