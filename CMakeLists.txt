cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(Fahrenheit C CXX)
include("${VITASDK}/share/vita.cmake" REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FAHRENHEIT_MEDIA REQUIRED SDL2_mixer SDL2_image sndfile vorbisfile)
find_program(VITA_LIBS_GEN_EXECUTABLE NAMES vita-libs-gen HINTS "${VITASDK}/bin")
if(NOT VITA_LIBS_GEN_EXECUTABLE)
  message(FATAL_ERROR "vita-libs-gen is required to build the libc bridge")
endif()
set(LIBC_BRIDGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/libc_bridge")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/libc_bridge/nids.yml")
execute_process(
  COMMAND "${VITA_LIBS_GEN_EXECUTABLE}" -c "${CMAKE_CURRENT_SOURCE_DIR}/libc_bridge/nids.yml" "${LIBC_BRIDGE_DIR}"
  RESULT_VARIABLE LIBC_BRIDGE_RESULT
)
if(NOT LIBC_BRIDGE_RESULT EQUAL 0)
  message(FATAL_ERROR "libc bridge stub generation failed: ${LIBC_BRIDGE_RESULT}")
endif()
add_subdirectory("${LIBC_BRIDGE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/libc_bridge-build")

set(VITA_APP_NAME "Fahrenheit")
set(VITA_TITLEID  "FAHRENEIT")
set(VITA_MKSFOEX_FLAGS "-d ATTRIBUTE2=12")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wl,-q,--wrap,memcpy,--wrap,memmove,--wrap,memset,--allow-multiple-definition -Wall -O3 -fdiagnostics-color=always -fno-optimize-sibling-calls -mfloat-abi=softfp")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++11 -Wno-write-strings")

add_executable(Fahrenheit
  loader/main.c
  loader/dialog.c
  loader/so_util.c
  loader/ogg_patch.c
  loader/vorbis_patch.c
  loader/sha1.c
  loader/ctype_patch.c
  loader/fnmatch.c
  loader/fios.c
  loader/trophies.c
)

set_target_properties(Fahrenheit PROPERTIES LINKER_LANGUAGE CXX C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS YES)
target_compile_options(Fahrenheit PRIVATE ${FAHRENHEIT_MEDIA_STATIC_CFLAGS})

target_link_libraries(Fahrenheit
  -pthread
  -Wl,--start-group
  ${FAHRENHEIT_MEDIA_STATIC_LDFLAGS}
  SDL2_mixer
  SDL2_image
  SDL2
  z
  m
  stdc++
  OpenSLES
  sndfile
  vorbis
  vorbisenc
  vorbisfile
  ogg
  FLAC
  openal
  imgui
  vitaGL
  vitashark
  SceShaccCgExt
  mathneon
  mpg123
  taihen_stub
  kubridge_stub
  SceIme_stub
  SceAppMgr_stub
  SceAppUtil_stub
  SceAudio_stub
  SceAudioIn_stub
  SceCtrl_stub
  SceCommonDialog_stub
  SceDisplay_stub
  SceFios2_stub
  SceGxm_stub
  SceLibcBridge_stub
  SceShaccCg_stub
  SceSysmodule_stub
  ScePower_stub
  SceTouch_stub
  SceMotion_stub
  SceHid_stub
  SceVshBridge_stub
  SceKernelDmacMgr_stub
  SceNpTrophy_stub
  -Wl,--end-group
)

vita_create_self(eboot.bin Fahrenheit UNSAFE)
vita_create_vpk(Fahrenheit.vpk ${VITA_TITLEID} eboot.bin
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE ${CMAKE_SOURCE_DIR}/sce_sys/icon0.png sce_sys/icon0.png
       ${CMAKE_SOURCE_DIR}/sce_sys/pic0.png sce_sys/pic0.png
       ${CMAKE_SOURCE_DIR}/sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
       ${CMAKE_SOURCE_DIR}/sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
       ${CMAKE_SOURCE_DIR}/sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
       ${CMAKE_SOURCE_DIR}/sce_sys/trophy/FAHR00001_00/TROPHY.TRP sce_sys/trophy/FAHR00001_00/TROPHY.TRP
)

add_custom_target(copy
  COMMAND cp eboot.bin D:/app/${VITA_TITLEID}/eboot.bin
  DEPENDS eboot.bin
)

# Keep legacy invalid returns diagnostic-only instead of inventing results.
set_property(SOURCE loader/so_util.c APPEND PROPERTY COMPILE_OPTIONS -Wno-error=return-mismatch -Wno-error=incompatible-pointer-types)
