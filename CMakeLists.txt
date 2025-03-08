cmake_minimum_required(VERSION 3.5)

project(smbc)

# Force the compiler to be clang
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

add_subdirectory(codegen)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wno-c++11-narrowing")

# if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
#     add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
#     add_link_options(-fprofile-instr-generate)
# endif()


# Add coverage flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wno-c++11-narrowing -fprofile-instr-generate -fcoverage-mapping")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-instr-generate -fcoverage-mapping")

# Add sanitizers
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined,leak")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address,undefined,leak")



set(SOURCE_FILES
    source/Configuration.cpp
    source/Main.cpp
    source/Emulation/APU.cpp
    source/Emulation/Controller.cpp
    source/Emulation/MemoryAccess.cpp
    source/Emulation/PPU.cpp
    source/SMB/SMB.cpp
    source/SMB/SMBData.cpp
    source/SMB/SMBEngine.cpp
    source/Util/Video.cpp)

find_package(SDL2 REQUIRED)
find_package(Boost REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})

add_executable(smbc ${SOURCE_FILES})
target_link_libraries(smbc ${SDL2_LIBRARY} ${SDL2_LIBRARIES})

add_dependencies(smbc codegen)

add_custom_target(coverage
    COMMAND llvm-profdata merge -sparse *.profraw -o default.profdata
    COMMAND llvm-cov export ./smbc -instr-profile=default.profdata -format=text -summary-only > coverage.json
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating coverage report"
)
