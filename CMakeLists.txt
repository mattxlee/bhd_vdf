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

if (${CMAKE_CXX_COMPILER_ID} MATCHES ^.*Clang$)
    set(NOPIE "-fno-PIE")
else()
    set(NOPIE "-no-pie")
endif()
set(COMPILE_ASM_LDFLAGS "-flto ${NOPIE}")

set(CMAKE_CXX_FLAGS "-O3 -flto -pthread ${NOPIE} -fvisibility=hidden")

set(CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_LIST_DIR}/src/chiavdf/cmake
    ${CMAKE_MODULE_PATH}
    )

set(CHIAVDF_SRC
    src/chiavdf/refcode/lzcnt.c
    src/chiavdf/generic.cpp
    src/chiavdf/proof_common.cpp
    src/chiavdf/util.cpp
    src/chiavdf/vdf_new.cpp
    src/chiavdf/integer_common.cpp
    src/chiavdf/integer.cpp
    src/chiavdf/nucomp.cpp
    src/chiavdf/bqfc.c
    src/chiavdf/xgcd_partial.c
    )

find_package(GMP REQUIRED)
find_package(GMPXX REQUIRED)
find_package(spdlog REQUIRED)

include_directories(
    ${GMP_INCLUDE_DIR}
    ${GMPXX_INCLUDE_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/src/chiavdf
    )

set(COMPILE_ASM_SRC
    src/chiavdf/compile_asm.cpp
    ${CHIAVDF_SRC}
    )

add_executable(compile_asm ${COMPILE_ASM_SRC})
target_compile_definitions(compile_asm PRIVATE ${COMPILE_ASM_DEFS})
target_link_libraries(compile_asm PRIVATE ${COMPILE_ASM_LDFLAGS} ${GMP_LIBRARIES} ${GMPXX_LIBRARIES})

enable_testing()
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest
    GIT_TAG release-1.11.0
    )
FetchContent_MakeAvailable(googletest)
find_package(GTest REQUIRED)

add_custom_command(OUTPUT asm_compiled.s COMMAND compile_asm)
add_custom_command(OUTPUT asm_compiled_avx2.s COMMAND compile_asm avx2)
add_custom_command(OUTPUT asm_compiled_avx512.s COMMAND compile_asm avx512)

set(VDF_TEST_SRCS
    src/vdf_test.cpp
    src/vdf_computer.cpp
    ${CHIAVDF_SRC}
    asm_compiled.s
    avx2_asm_compiled.s
    avx512_asm_compiled.s
    )
add_executable(vdf_test ${VDF_TEST_SRCS})
target_link_libraries(vdf_test PRIVATE ${COMPILE_ASM_LDFLAGS} GTest::gtest GTest::gtest_main ${GMP_LIBRARIES} ${GMPXX_LIBRARIES} spdlog::spdlog)
