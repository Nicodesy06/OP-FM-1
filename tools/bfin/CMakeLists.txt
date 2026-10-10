# Host harness around bcore (github.com/op1emu/bcore, MIT): disassemble and
# execute *original* OP-1 Blackfin code from a flat memory image.
#   cmake -S tools/bfin -B build/bfin -DBCORE_DIR=/path/to/op1emu/ext/bcore \
#         -DLLVM_DIR=/usr/lib/llvm-15/lib/cmake/llvm
cmake_minimum_required(VERSION 3.10)
project(op1-bftool CXX C)
set(CMAKE_CXX_STANDARD 17)
set(BCORE_DIR "" CACHE PATH "op1emu/ext/bcore checkout")
add_subdirectory(${BCORE_DIR} bcore EXCLUDE_FROM_ALL)
add_executable(bftool bftool.cpp)
target_include_directories(bftool PRIVATE ${BCORE_DIR}/tools ${BCORE_DIR}/src)
target_link_libraries(bftool PRIVATE bfin-core)
