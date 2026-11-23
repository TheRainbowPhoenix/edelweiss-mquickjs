cmake_minimum_required(VERSION 3.24)
project(ExwordMQuickJS LANGUAGES C ASM)
if(NOT FXSDK_PLATFORM_LONG STREQUAL "exword")
  message(FATAL_ERROR "Use the fxSDK EXWORD.cmake toolchain")
endif()

# Keep this branch to follow the latest EX-word runtime, or pin a commit/tag.
set(EDELWEISS_REF "edelweiss" CACHE STRING "Upstream Edelweiss branch, tag or commit")
set(EXWORD_APP_ID "QJSJS" CACHE STRING "Five-character dictionary installation ID")
set(EXWORD_MODULE "qjs" CACHE STRING "D01 filename without its extension")
set(EXWORD_HEAP_SIZE "393216" CACHE STRING "Payload-local gint heap in bytes")
set(EXWORD_APP_TITLE "MicroQuickJS" CACHE STRING "Installation title")
if(NOT EXWORD_APP_ID MATCHES "^[A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9]$")
  message(FATAL_ERROR "EXWORD_APP_ID must be five uppercase letters/digits")
endif()
string(LENGTH "${EXWORD_MODULE}" MODULE_LENGTH)
if(NOT EXWORD_MODULE MATCHES "^[a-z][a-z0-9_]*$" OR MODULE_LENGTH GREATER 8)
  message(FATAL_ERROR "EXWORD_MODULE must be a lowercase identifier of at most eight characters")
endif()

include(FetchContent)
set(EDL_BUILD_PAYLOADS OFF CACHE BOOL "Build upstream bundled payloads" FORCE)
set(EDL_BUILD_PACKAGES OFF CACHE BOOL "Build upstream installation packages" FORCE)
set(EDL_APPID "${EXWORD_APP_ID}" CACHE STRING "Embedded bootstrap fallback ID" FORCE)
FetchContent_Declare(edelweiss
  GIT_REPOSITORY https://github.com/TheRainbowPhoenix/Edelweiss.git
  GIT_TAG "${EDELWEISS_REF}")
set(MQUICKJS_REF "main" CACHE STRING "MicroQuickJS revision to build")
FetchContent_Declare(mquickjs
  GIT_REPOSITORY https://github.com/bellard/mquickjs.git
  GIT_TAG "${MQUICKJS_REF}")
FetchContent_MakeAvailable(edelweiss mquickjs)
include("${edelweiss_SOURCE_DIR}/sdk/GenerateEDL.cmake")
include(GenerateEXWORDPackage)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Toybox's ELF manifest reader requires this INFO, non-SHF_ALLOC section.
# The stock Edelweiss linker leaves all unknown sections out of the image.
set(EDL_LINKER "${CMAKE_CURRENT_BINARY_DIR}/edl.ld")
file(READ "${EDL_LINKER}" edl_script)
set(marker "    .edl.meta 0 (INFO) : { KEEP(*(.edl.meta)) } :NONE")
string(FIND "${edl_script}" "${marker}" marker_index)
if(marker_index EQUAL -1)
  message(FATAL_ERROR "Unsupported Edelweiss linker script: .edl.meta missing")
endif()
string(REPLACE "${marker}" "${marker}\n    .edl.cmd 0 (INFO) : { KEEP(*(.edl.cmd)) } :NONE"
  edl_script "${edl_script}")
file(WRITE "${EDL_LINKER}" "${edl_script}")

find_program(HOST_CC NAMES cc gcc clang REQUIRED)
set(GENERATED "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(JS_STDLIB_HEADER "${GENERATED}/mqjs_stdlib.h")
set(JS_ATOM_HEADER "${GENERATED}/mquickjs_atom.h")
add_custom_command(
  OUTPUT "${JS_STDLIB_HEADER}" "${JS_ATOM_HEADER}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${GENERATED}"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/make_stdlib.py"
    prepare "${mquickjs_SOURCE_DIR}/mqjs_stdlib.c" "${GENERATED}/port_stdlib.c"
  COMMAND "${HOST_CC}" -O2 -D_GNU_SOURCE "-I${mquickjs_SOURCE_DIR}"
    "${GENERATED}/port_stdlib.c" "${mquickjs_SOURCE_DIR}/mquickjs_build.c"
    -lm -o "${GENERATED}/mqjs_stdlib_host"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/make_stdlib.py"
    generate "${GENERATED}/mqjs_stdlib_host" "${JS_STDLIB_HEADER}" "${JS_ATOM_HEADER}"
  DEPENDS "${mquickjs_SOURCE_DIR}/mqjs_stdlib.c"
    "${mquickjs_SOURCE_DIR}/mquickjs_build.c"
    "${mquickjs_SOURCE_DIR}/mquickjs_build.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/make_stdlib.py" VERBATIM)
add_custom_target(qjs_codegen DEPENDS "${JS_STDLIB_HEADER}" "${JS_ATOM_HEADER}")

add_gint_edl(qjs
  src/main.c
  src/browser.c
  src/gint_draw.c
  "${mquickjs_SOURCE_DIR}/mquickjs.c"
  "${mquickjs_SOURCE_DIR}/dtoa.c"
  "${mquickjs_SOURCE_DIR}/libm.c"
  "${mquickjs_SOURCE_DIR}/cutils.c")
add_dependencies(qjs qjs_codegen)
target_include_directories(qjs PRIVATE "${GENERATED}" "${mquickjs_SOURCE_DIR}" include)
# Capture the host API without modifying Edelweiss's ABI or startup.
target_link_options(qjs PRIVATE "-Wl,--wrap=edl_gint_set_api")
if(NOT EXWORD_HEAP_SIZE MATCHES "^[0-9]+$" OR EXWORD_HEAP_SIZE LESS 262144 OR
   EXWORD_HEAP_SIZE GREATER 786432)
  message(FATAL_ERROR "EXWORD_HEAP_SIZE must be 262144..786432")
endif()
target_compile_definitions(qjs PRIVATE EDL_HEAP_SIZE=${EXWORD_HEAP_SIZE})
target_compile_options(qjs PRIVATE -Wno-unused-parameter)

# Edelweiss boots main.edl, while Toybox discovers qjs.edl by its .edl.cmd.
add_custom_target(qjs_home_payload ALL
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "$<TARGET_FILE:qjs>" "${CMAKE_BINARY_DIR}/main.edl"
  DEPENDS qjs VERBATIM)

add_custom_target(embedded_bootstrap ALL
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_BINARY_DIR}/edelweiss.d01"
    "${CMAKE_BINARY_DIR}/${EXWORD_MODULE}.d01"
  DEPENDS edelweiss VERBATIM)
if(NOT EXWORD_METADATA_SOURCE)
  message(FATAL_ERROR "Set EXWORD_METADATA_SOURCE to the EX-word metadata repository (CI does this)")
endif()
generate_exword_package(TARGET embedded_bootstrap
  D01 "${CMAKE_BINARY_DIR}/${EXWORD_MODULE}.d01" APPID "${EXWORD_APP_ID}"
  TITLE "${EXWORD_APP_TITLE}" HTML_DIR "${EXWORD_METADATA_SOURCE}/html"
  OUTPUT_DIR "${CMAKE_BINARY_DIR}/packages" LOCALES ja cn)
foreach(LOCALE IN ITEMS ja cn)
  add_custom_target(qjs_payload_${LOCALE} ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:qjs>"
      "${CMAKE_BINARY_DIR}/packages/${LOCALE}/${EXWORD_APP_ID}/main.edl"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/examples/DEMO.JS"
      "${CMAKE_BINARY_DIR}/packages/${LOCALE}/${EXWORD_APP_ID}/DEMO.JS"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/examples/ARGS.JS"
      "${CMAKE_BINARY_DIR}/packages/${LOCALE}/${EXWORD_APP_ID}/ARGS.JS"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/examples/HELLO.JS"
      "${CMAKE_BINARY_DIR}/packages/${LOCALE}/${EXWORD_APP_ID}/HELLO.JS"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/examples/SMOKE.JS"
      "${CMAKE_BINARY_DIR}/packages/${LOCALE}/${EXWORD_APP_ID}/SMOKE.JS"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/examples/SCRIPTS.TXT"
      "${CMAKE_BINARY_DIR}/packages/${LOCALE}/${EXWORD_APP_ID}/SCRIPTS.TXT"
    DEPENDS embedded_bootstrap_package_${LOCALE} qjs VERBATIM)
endforeach()
execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY "${edelweiss_SOURCE_DIR}"
  OUTPUT_VARIABLE EDELWEISS_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
file(WRITE "${CMAKE_BINARY_DIR}/edelweiss-version.txt" "${EDELWEISS_COMMIT}\n")
message(STATUS "Building with Edelweiss ${EDELWEISS_COMMIT}")
