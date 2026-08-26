cmake_minimum_required(VERSION 3.24)

project(UTool
  VERSION 0.2.0
  LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC: consistent runtime + UTF-8 source
if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8 /Zc:__cplusplus)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

include(FetchContent)

# --- Dependencies -----------------------------------------------------------

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# Lua 5.4 sources (no upstream CMakeLists — populate then build lua_static)
FetchContent_Declare(
  lua
  URL      https://www.lua.org/ftp/lua-5.4.7.tar.gz
  URL_HASH SHA256=9fbf5e28ef86c69858f6d3d34eccc32e911c1a28b4120ff3e84aaa70cfbf1e30
)
# FetchContent_Populate is intentional: Lua ships without a CMake project.
cmake_policy(SET CMP0169 OLD)
FetchContent_GetProperties(lua)
if(NOT lua_POPULATED)
  FetchContent_Populate(lua)
endif()

add_library(lua_static STATIC
  ${lua_SOURCE_DIR}/src/lapi.c
  ${lua_SOURCE_DIR}/src/lauxlib.c
  ${lua_SOURCE_DIR}/src/lbaselib.c
  ${lua_SOURCE_DIR}/src/lcode.c
  ${lua_SOURCE_DIR}/src/lcorolib.c
  ${lua_SOURCE_DIR}/src/lctype.c
  ${lua_SOURCE_DIR}/src/ldblib.c
  ${lua_SOURCE_DIR}/src/ldebug.c
  ${lua_SOURCE_DIR}/src/ldo.c
  ${lua_SOURCE_DIR}/src/ldump.c
  ${lua_SOURCE_DIR}/src/lfunc.c
  ${lua_SOURCE_DIR}/src/lgc.c
  ${lua_SOURCE_DIR}/src/linit.c
  ${lua_SOURCE_DIR}/src/liolib.c
  ${lua_SOURCE_DIR}/src/llex.c
  ${lua_SOURCE_DIR}/src/lmathlib.c
  ${lua_SOURCE_DIR}/src/lmem.c
  ${lua_SOURCE_DIR}/src/loadlib.c
  ${lua_SOURCE_DIR}/src/lobject.c
  ${lua_SOURCE_DIR}/src/lopcodes.c
  ${lua_SOURCE_DIR}/src/loslib.c
  ${lua_SOURCE_DIR}/src/lparser.c
  ${lua_SOURCE_DIR}/src/lstate.c
  ${lua_SOURCE_DIR}/src/lstring.c
  ${lua_SOURCE_DIR}/src/lstrlib.c
  ${lua_SOURCE_DIR}/src/ltable.c
  ${lua_SOURCE_DIR}/src/ltablib.c
  ${lua_SOURCE_DIR}/src/ltm.c
  ${lua_SOURCE_DIR}/src/lundump.c
  ${lua_SOURCE_DIR}/src/lutf8lib.c
  ${lua_SOURCE_DIR}/src/lvm.c
  ${lua_SOURCE_DIR}/src/lzio.c
)
target_include_directories(lua_static PUBLIC ${lua_SOURCE_DIR}/src)
set_target_properties(lua_static PROPERTIES
  C_STANDARD 99
  C_STANDARD_REQUIRED ON
  LINKER_LANGUAGE C
)
if(MSVC)
  target_compile_options(lua_static PRIVATE /W0)
endif()
# lua.c / luac.c are the interpreters — not part of the library

FetchContent_Declare(
  sol2
  GIT_REPOSITORY https://github.com/ThePhD/sol2.git
  GIT_TAG        v3.3.1
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(sol2)

# --- utool_lib --------------------------------------------------------------

add_library(utool_lib STATIC
  src/Core/Config.cpp
  src/Pak/UnrealPak.cpp
  src/Mod/Prepare.cpp
  src/Mod/CurveFloat.cpp
  src/Mod/JsonEditor.cpp
  src/Lua/Host.cpp
  src/Lua/SolJson.cpp
  src/Lua/Modules/Registry.cpp
  src/Lua/Modules/Curve.cpp
  src/Lua/Modules/Asset.cpp
  src/Cli/Commands.cpp
)

target_include_directories(utool_lib
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(utool_lib
  PUBLIC
    nlohmann_json::nlohmann_json
    sol2::sol2
    lua_static
)

# --- utool (CLI executable) -------------------------------------------------

add_executable(utool
  src/main.cpp
)

target_link_libraries(utool
  PRIVATE
    utool_lib
)

# --- utool_tests ------------------------------------------------------------

enable_testing()
add_subdirectory(tests)
