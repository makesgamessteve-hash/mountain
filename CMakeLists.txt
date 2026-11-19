# Mountain - a relaxing retro 3D hiking game.
# Code: MIT License. Art/sound assets: CC BY 4.0. See LICENSE and assets/LICENSE.md.
cmake_minimum_required(VERSION 3.20)
project(mountain VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MTN_BUILD_TESTS "Build unit tests" ON)
option(MTN_BUILD_GAME "Build the game executable (needs SDL3 + OpenGL)" ON)
option(MTN_COVERAGE "Instrument engine code for coverage (gcc/clang)" OFF)
option(MTN_FETCH_SDL "Download and build SDL3 if it is not installed" ON)
set(MTN_SDL_TAG "release-3.4.20" CACHE STRING "SDL3 git tag used when fetching")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

# Output everything into one folder so that the executable finds assets/ beside it.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER ${cfg} CFG)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG} ${CMAKE_BINARY_DIR}/bin)
endforeach()

if(EMSCRIPTEN)
  set(MTN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
endif()

# Deterministic float math across compilers (no fused multiply-add contraction).
set(MTN_WARN_FLAGS)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  set(MTN_WARN_FLAGS -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
  add_compile_options(-ffp-contract=off)
elseif(MSVC)
  set(MTN_WARN_FLAGS /W3)
  add_compile_options(/fp:precise)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# ---------------------------------------------------------------- third party
file(GLOB LUA_SOURCES ${CMAKE_SOURCE_DIR}/third_party/lua/*.c)
add_library(mtn_lua STATIC ${LUA_SOURCES})
target_include_directories(mtn_lua PUBLIC ${CMAKE_SOURCE_DIR}/third_party/lua)
if(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
  target_compile_definitions(mtn_lua PRIVATE LUA_USE_POSIX)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(mtn_lua PRIVATE -w)
endif()

add_library(mtn_stb STATIC third_party/stb/stb_impl.c)
target_include_directories(mtn_stb PUBLIC ${CMAKE_SOURCE_DIR}/third_party/stb)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(mtn_stb PRIVATE -w)
endif()

# ---------------------------------------------------------------- engine core
# Platform independent: no SDL, no OpenGL. Fully unit tested.
file(GLOB MTN_CORE_SOURCES ${CMAKE_SOURCE_DIR}/engine/src/core/*.c
                           ${CMAKE_SOURCE_DIR}/engine/src/script/*.c)
add_library(mtn_core STATIC ${MTN_CORE_SOURCES})
target_include_directories(mtn_core PUBLIC ${CMAKE_SOURCE_DIR}/engine/include)
target_link_libraries(mtn_core PUBLIC mtn_lua mtn_stb)
target_compile_options(mtn_core PRIVATE ${MTN_WARN_FLAGS})
if(NOT MSVC)
  target_link_libraries(mtn_core PUBLIC m)
endif()

if(MTN_COVERAGE)
  set(MTN_COV_FLAGS --coverage -O0 -fno-inline)
  target_compile_options(mtn_core PRIVATE ${MTN_COV_FLAGS})
  target_link_options(mtn_core PUBLIC --coverage)
endif()

# ---------------------------------------------------------------- game
if(MTN_BUILD_GAME)
  if(EMSCRIPTEN)
    # emscripten's find_package only searches the sysroot unless told otherwise
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
  endif()
  if(TRUE)
    find_package(SDL3 CONFIG QUIET)
    if(NOT SDL3_FOUND)
      if(NOT MTN_FETCH_SDL)
        message(FATAL_ERROR "SDL3 not found. Set SDL3_DIR or enable MTN_FETCH_SDL.")
      endif()
      message(STATUS "SDL3 not found - fetching ${MTN_SDL_TAG}")
      include(FetchContent)
      set(SDL_TESTS OFF CACHE BOOL "" FORCE)
      set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
      set(SDL_SHARED OFF CACHE BOOL "" FORCE)
      set(SDL_STATIC ON CACHE BOOL "" FORCE)
      FetchContent_Declare(SDL3 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
                                GIT_TAG ${MTN_SDL_TAG} GIT_SHALLOW TRUE)
      FetchContent_MakeAvailable(SDL3)
    endif()
  endif()

  file(GLOB MTN_RENDER_SOURCES ${CMAKE_SOURCE_DIR}/engine/src/render/*.c)
  file(GLOB MTN_PLATFORM_SOURCES ${CMAKE_SOURCE_DIR}/engine/src/platform/*.c)
  add_executable(mountain WIN32 engine/src/main.c ${MTN_RENDER_SOURCES} ${MTN_PLATFORM_SOURCES})
  target_include_directories(mountain PRIVATE ${CMAKE_SOURCE_DIR}/engine/src)
  target_link_libraries(mountain PRIVATE mtn_core SDL3::SDL3)
  if(NOT EMSCRIPTEN)
    set(OpenGL_GL_PREFERENCE GLVND)
    find_package(OpenGL REQUIRED)
    target_link_libraries(mountain PRIVATE OpenGL::GL)
  endif()
  target_compile_options(mountain PRIVATE ${MTN_WARN_FLAGS})
  if(MTN_COVERAGE)
    target_compile_options(mountain PRIVATE ${MTN_COV_FLAGS})
  endif()

  if(EMSCRIPTEN)
    set_target_properties(mountain PROPERTIES SUFFIX ".html")
    target_link_options(mountain PRIVATE
      -sUSE_WEBGL2=1 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2
      -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=134217728 -sSTACK_SIZE=1048576
      -sEXPORTED_FUNCTIONS=_main,_mtn_web_xr_frame,_mtn_web_xr_ended,_mtn_web_touch_hint,_malloc,_free
      -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,HEAPF32,HEAPU8
      --js-library ${CMAKE_SOURCE_DIR}/web/mountain_lib.js
      --pre-js ${CMAKE_SOURCE_DIR}/web/pre.js
      --shell-file ${CMAKE_SOURCE_DIR}/web/shell.html
      --preload-file ${CMAKE_SOURCE_DIR}/assets@/assets
      --exclude-file "*/README.md"
      -lidbfs.js
      -O2)
  elseif(WIN32)
    if(TARGET SDL3::SDL3-shared)
      add_custom_command(TARGET mountain POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3-shared> $<TARGET_FILE_DIR:mountain>)
    endif()
    if(MINGW)
      target_link_options(mountain PRIVATE -static-libgcc)
    endif()
  endif()

  if(NOT EMSCRIPTEN)
    # Assets live in a folder beside the executable (never baked in).
    add_custom_target(mtn_assets ALL
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets ${CMAKE_BINARY_DIR}/bin/assets
      COMMENT "Copying assets beside the executable")
    add_dependencies(mountain mtn_assets)
    install(TARGETS mountain RUNTIME DESTINATION .)
    install(DIRECTORY assets DESTINATION .)
    install(FILES LICENSE README.md DESTINATION .)
  endif()
endif()

# ---------------------------------------------------------------- tests
if(MTN_BUILD_TESTS)
  enable_testing()
  file(GLOB MTN_TEST_SOURCES ${CMAKE_SOURCE_DIR}/tests/unit/*.c)
  add_executable(mtn_unit_tests ${MTN_TEST_SOURCES})
  target_link_libraries(mtn_unit_tests PRIVATE mtn_core)
  target_compile_definitions(mtn_unit_tests PRIVATE MTN_SOURCE_DIR="${CMAKE_SOURCE_DIR}" MTN_BINARY_DIR="${CMAKE_BINARY_DIR}")
  target_compile_options(mtn_unit_tests PRIVATE ${MTN_WARN_FLAGS})
  add_test(NAME unit COMMAND mtn_unit_tests WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_FOUND)
    add_test(NAME script_api_docs COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/check_api_docs.py)
  endif()
  if(MTN_BUILD_GAME AND UNIX)
    file(GLOB MTN_GAME_TESTS ${CMAKE_SOURCE_DIR}/tests/game/*.lua)
    foreach(t ${MTN_GAME_TESTS})
      get_filename_component(tn ${t} NAME_WE)
      add_test(NAME game_${tn}
        COMMAND ${CMAKE_SOURCE_DIR}/tools/run_game_test.sh $<TARGET_FILE:mountain> ${t})
    endforeach()
    add_test(NAME game_failures COMMAND ${CMAKE_SOURCE_DIR}/tests/game/failures.sh $<TARGET_FILE:mountain>)
  endif()
endif()
