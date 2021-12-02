cmake_minimum_required(VERSION 3.2)

project(bhd_vdf)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "-flto -fvisibility=hidden")

set(CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_LIST_DIR}/src/chiavdf/cmake
    ${CMAKE_MODULE_PATH}
)


enable_language(ASM)

find_package(GMP REQUIRED)
find_package(GMPXX REQUIRED)

set(asm_compiled_src build/avx2_asm_compiled.s)
add_custom_target(asm_compiled DEPENDS build/avx2_asm_compiled.s)

file(GLOB vdf_src
    src/bhd_vdf.cpp
    src/chiavdf/compile_asm.c
    ${ASM_COMPILED_SRC}
    )

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/chiavdf
    ${INCLUDE_DIRECTORIES}
    ${GMP_INCLUDE_DIR}
    ${GMPXX_INCLUDE_DIR}
    )

add_library(bhd_vdf SHARED ${vdf_src})
target_link_libraries(bhd_vdf PRIVATE ${GMP_LIBRARIES} ${GMPXX_LIBRARIES} -pthread)

file(GLOB compile_asm_src
    src/chiavdf/compile_asm.cpp
    )

add_executable(compile_asm ${compile_asm_src})
target_link_libraries(compile_asm PRIVATE ${GMP_LIBRARIES} ${GMPXX_LIBRARIES} -pthread)

add_custom_command(
    OUTPUT asm_compiled
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/compile_asm avx2
    DEPENDS compile_asm
    COMMENT "Compiling asm..."
    )
