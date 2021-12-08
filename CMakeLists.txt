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
set(COMPILE_ASM_LDFLAGS "${NOPIE}")

set(CMAKE_CXX_FLAGS_DEBUG "-pthread ${NOPIE} -fvisibility=hidden -g2 -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-pthread ${NOPIE} -fvisibility=hidden -O3")

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest
    GIT_TAG release-1.11.0
    )
FetchContent_Declare(
    chiavdf
    GIT_REPOSITORY https://github.com/Chia-Network/chiavdf
    GIT_TAG 1.0.3
    )
FetchContent_MakeAvailable(googletest chiavdf)

set(CHIAVDF_DIR ${CMAKE_BINARY_DIR}/_deps/chiavdf-src)

set(CMAKE_MODULE_PATH
    ${CHIAVDF_DIR}/src/cmake
    ${CMAKE_MODULE_PATH}
    )

find_package(GMP REQUIRED)
find_package(GMPXX REQUIRED)
find_package(spdlog REQUIRED)

include_directories(
    ${GMP_INCLUDE_DIR}
    ${GMPXX_INCLUDE_DIR}
    ${CHIAVDF_DIR}/src
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

enable_testing()
find_package(GTest REQUIRED)

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/asm_compiled.s COMMAND ${CMAKE_BINARY_DIR}/compile_asm DEPENDS compile_asm)
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/avx2_asm_compiled.s COMMAND ${CMAKE_BINARY_DIR}/compile_asm avx2 DEPENDS compile_asm)
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/avx512_asm_compiled.s COMMAND ${CMAKE_BINARY_DIR}/compile_asm avx512 DEPENDS compile_asm)

set(VDF_TEST_SRCS
    src/vdf_test.cpp
    src/vdf_computer.cpp
    ${CHIAVDF_SRC}
    ${CMAKE_BINARY_DIR}/asm_compiled.s
    ${CMAKE_BINARY_DIR}/avx2_asm_compiled.s
    ${CMAKE_BINARY_DIR}/avx512_asm_compiled.s
    )
add_executable(vdf_test ${VDF_TEST_SRCS} ${ASM_S})
target_link_libraries(vdf_test PRIVATE ${COMPILE_ASM_LDFLAGS} GTest::gtest GTest::gtest_main ${GMP_LIBRARIES} ${GMPXX_LIBRARIES} spdlog::spdlog)
