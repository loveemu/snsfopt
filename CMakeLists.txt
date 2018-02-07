cmake_minimum_required(VERSION 2.8)

project(snsfopt C CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CheckIncludeFiles)

include_directories(src/snsf9x/snes9x)

check_include_files(stdint.h HAVE_STDINT_H)
if(HAVE_STDINT_H)
    add_definitions(-DHAVE_STDINT_H)
endif()

check_include_files(inttypes.h HAVE_INTTYPES_H)
if(HAVE_INTTYPES_H)
    add_definitions(-DHAVE_INTTYPES_H)
endif()

check_include_files(stdbool.h HAVE_STDBOOL_H)
if(HAVE_STDBOOL_H)
    add_definitions(-DHAVE_STDBOOL_H)
endif()

if(MSVC)
    if(CMAKE_CL_64)
        set(MSVC_ARCHITECTURE_NAME x64)
    else()
        set(MSVC_ARCHITECTURE_NAME win32)
    endif()

    # Use bundled dependencies
    set(ZLIB_ROOT ${CMAKE_SOURCE_DIR}/dependencies/zlib)
    set(ZLIB_LIBRARY ${CMAKE_SOURCE_DIR}/dependencies/zlib/lib/${MSVC_ARCHITECTURE_NAME}/zlibstat.lib)
    add_definitions(-DZLIB_WINAPI)
endif()

find_package(ZLIB REQUIRED)

if(MSVC)
    option(STATIC_CRT "Use static CRT libraries" ON)

    # Rewrite command line flags to use /MT if necessary
    if(STATIC_CRT)
        foreach(flag_var
                CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
                CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
            if(${flag_var} MATCHES "/MD")
                string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
            endif(${flag_var} MATCHES "/MD")
        endforeach(flag_var)
    endif()

    # Allow for wildcards in command-line path arguments
    # http://msdn.microsoft.com/en-us/library/8bch7bkk.aspx
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} setargv.obj")

    # Disable MSVC specific secure error
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

#============================================================================
# snsfopt
#============================================================================

set(SRCS
    src/snsfopt.cpp
    src/SPCFile.cpp
    src/PSFFile.cpp
    src/ZlibReader.cpp
    src/ZlibWriter.cpp
)

set(SNSF9X_SRCS
    src/snsf9x/SNESSystem.cpp
    src/snsf9x/snes9x/cpu.cpp
    src/snsf9x/snes9x/cpuexec.cpp
    src/snsf9x/snes9x/cpuops.cpp
    src/snsf9x/snes9x/dma.cpp
    src/snsf9x/snes9x/globals.cpp
    src/snsf9x/snes9x/memmap.cpp
    src/snsf9x/snes9x/ppu.cpp
    src/snsf9x/snes9x/sa1.cpp
    src/snsf9x/snes9x/sdd1.cpp
    src/snsf9x/snes9x/apu/apu.cpp
    src/snsf9x/snes9x/apu/SNES_SPC.cpp
    src/snsf9x/snes9x/apu/SNES_SPC_misc.cpp
    src/snsf9x/snes9x/apu/SNES_SPC_state.cpp
    src/snsf9x/snes9x/apu/SPC_DSP.cpp
    src/snsf9x/snes9x/apu/SPC_Filter.cpp
)

add_executable(snsfopt ${SRCS} ${SNSF9X_SRCS})

if(ZLIB_FOUND)
    include_directories(${ZLIB_INCLUDE_DIRS})
    target_link_libraries(snsfopt ${ZLIB_LIBRARIES})
endif(ZLIB_FOUND)
