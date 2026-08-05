cmake_minimum_required(VERSION 3.25)
project(atomic_vk VERSION 1.0.0 LANGUAGES CXX C)

# Silence legacy GLM policy warnings
if(POLICY CMP0077)
  cmake_policy(SET CMP0077 NEW)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Vulkan SDK
find_package(Vulkan REQUIRED)
if(DEFINED ENV{VULKAN_SDK})
  set(VULKAN_HEADERS_DIR "$ENV{VULKAN_SDK}/include")
else()
  set(VULKAN_HEADERS_DIR ${Vulkan_INCLUDE_DIRS})
endif()

# =============================================================================
# 1. SHADER COMPILATION (glslc)
# =============================================================================
find_program(GLSLC_EXECUTABLE glslc REQUIRED)

set(SHADER_SOURCES
    shaders/ui.vert
    shaders/ui_shape.frag
    shaders/ui_text.frag
)

set(SPV_OUTPUTS)
set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")

foreach(SHADER_FILE ${SHADER_SOURCES})
  get_filename_component(SHADER_NAME ${SHADER_FILE} NAME)
  set(SPV_FILE "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")

  add_custom_command(
      OUTPUT ${SPV_FILE}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUTPUT_DIR}
      COMMAND ${GLSLC_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_FILE} -o ${SPV_FILE}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_FILE}
      COMMENT "Compiling shader: ${SHADER_NAME} -> ${SHADER_NAME}.spv"
  )
  list(APPEND SPV_OUTPUTS ${SPV_FILE})
endforeach()

add_custom_target(avk_compiled_shaders DEPENDS ${SPV_OUTPUTS})

# =============================================================================
# 2. THIRDPARTY DEPENDENCIES
# =============================================================================
include(FetchContent)

# Tracy Profiler
option(TRACY_ENABLE "Enable Tracy Profiler" ON)
set(TRACY_ENABLE ON CACHE BOOL "Enable Tracy Profiler" FORCE)
set(TRACY_ON_DEMAND OFF CACHE BOOL "Disable on-demand" FORCE)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/tracy/CMakeLists.txt")
  add_subdirectory(thirdparty/tracy)
else()
  message(STATUS "thirdparty/tracy not found. Fetching via FetchContent...")
  FetchContent_Declare(
      tracy
      GIT_REPOSITORY https://github.com/wolfpld/tracy.git
      GIT_TAG master
      GIT_SHALLOW FALSE
    )
  FetchContent_MakeAvailable(tracy)
endif()

# GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE
)
set(GLM_TEST_ENABLE OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glm)

# Volk
add_library(volk STATIC
    thirdparty/volk/volk.c
    thirdparty/volk/volk.h
)
target_include_directories(volk PUBLIC
    thirdparty/volk
    ${VULKAN_HEADERS_DIR}
)

if(WIN32)
  target_compile_definitions(volk PUBLIC VK_USE_PLATFORM_WIN32_KHR)
elseif(UNIX AND NOT APPLE)
  target_compile_definitions(volk PUBLIC VK_USE_PLATFORM_WAYLAND_KHR VK_USE_PLATFORM_XLIB_KHR VK_USE_PLATFORM_XCB_KHR)
endif()

# HarfBuzz
set(HB_HAVE_FREETYPE ON CACHE BOOL "" FORCE)
set(HB_BUILD_SUBSET OFF CACHE BOOL "" FORCE)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/harfbuzz/CMakeLists.txt")
  add_subdirectory(thirdparty/harfbuzz)
else()
  message(STATUS "thirdparty/harfbuzz not found. Fetching via FetchContent...")
  FetchContent_Declare(
      harfbuzz
      GIT_REPOSITORY https://github.com/harfbuzz/harfbuzz.git
      GIT_TAG 8.3.0
      GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(harfbuzz)
endif()

# SheenBidi (BiDi Engine)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/SheenBidi/CMakeLists.txt")
  add_subdirectory(thirdparty/SheenBidi)
else()
  message(STATUS "thirdparty/SheenBidi not found. Fetching via FetchContent...")
  FetchContent_Declare(
      SheenBidi
      GIT_REPOSITORY https://github.com/Tehreer/SheenBidi.git
      GIT_TAG v2.6
      GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(SheenBidi)
endif()

# FreeType
set(FT_DISABLE_ZLIB OFF CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG OFF CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ OFF CACHE BOOL "" FORCE)
add_subdirectory(thirdparty/freetype)

# msdfgen
set(MSDFGEN_USE_VCPKG OFF CACHE BOOL "" FORCE)
set(MSDFGEN_CORE_ONLY OFF CACHE BOOL "" FORCE)
set(MSDFGEN_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
set(MSDFGEN_USE_SKIA OFF CACHE BOOL "" FORCE)
set(MSDFGEN_USE_OPENGL OFF CACHE BOOL "" FORCE)
set(MSDFGEN_DISABLE_PNG OFF CACHE BOOL "" FORCE)
set(MSDFGEN_DISABLE_SVG ON CACHE BOOL "" FORCE)
set(MSDFGEN_INSTALL OFF CACHE BOOL "" FORCE)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/msdfgen/CMakeLists.txt")
  add_subdirectory(thirdparty/msdfgen)
else()
  message(STATUS "thirdparty/msdfgen not found. Fetching via FetchContent...")
  FetchContent_Declare(
      msdfgen
      GIT_REPOSITORY https://github.com/Chlumsky/msdfgen.git
      GIT_TAG v1.10
      GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(msdfgen)
endif()

# msdf-atlas-gen
set(MSDF_ATLAS_USE_VCPKG OFF CACHE BOOL "" FORCE)
set(MSDF_ATLAS_MSDFGEN_EXTERNAL ON CACHE BOOL "" FORCE)
set(MSDF_ATLAS_USE_SKIA OFF CACHE BOOL "" FORCE)
set(MSDF_ATLAS_NO_ARGH ON CACHE BOOL "" FORCE)
set(MSDF_ATLAS_NO_ARTERY_FONT ON CACHE BOOL "" FORCE)
set(MSDF_ATLAS_BUILD_STANDALONE ON CACHE BOOL "" FORCE)
set(MSDF_ATLAS_INSTALL OFF CACHE BOOL "" FORCE)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/msdf-atlas-gen/CMakeLists.txt")
  add_subdirectory(thirdparty/msdf-atlas-gen)
else()
  message(STATUS "thirdparty/msdf-atlas-gen not found. Fetching via FetchContent...")
  FetchContent_Declare(
      msdf_atlas_gen
      GIT_REPOSITORY https://github.com/Chlumsky/msdf-atlas-gen.git
      GIT_TAG v1.2.2
      GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(msdf_atlas_gen)
endif()

# Vera Windowing
add_subdirectory(thirdparty/Vera)

# =============================================================================
# 3. COMPILE-TIME MSDF FONT GENERATOR FUNCTION
# =============================================================================
set(GENERATED_FONTS_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated_fonts")

function(atomic_add_font)
  cmake_parse_arguments(ARG "" "FONT;ATLAS_SIZE" "" ${ARGN})
  if(NOT ARG_FONT)
    message(FATAL_ERROR "atomic_add_font: FONT argument is required!")
  endif()
  if(NOT ARG_ATLAS_SIZE)
    set(ARG_ATLAS_SIZE 2048)
  endif()
  get_filename_component(FONT_NAME ${ARG_FONT} NAME_WE)
  set(PNG_OUTPUT "${GENERATED_FONTS_DIR}/${FONT_NAME}_atlas.png")
  set(CSV_OUTPUT "${GENERATED_FONTS_DIR}/${FONT_NAME}_metrics.csv")
  if("${FONT_NAME}" STREQUAL "lucide")
    set(LUCIDE_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/ui/generated/lucideIcons.generated.h")
    set(SCRIPT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/include/ui/generated/genUnicodeForMsdf.py")
    set(CHARSET_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/lucide_charset.txt")
    add_custom_command(
        OUTPUT ${CHARSET_OUTPUT}
        COMMAND python3 "${SCRIPT_PATH}" "${LUCIDE_HEADER}" "${CHARSET_OUTPUT}"
        DEPENDS "${LUCIDE_HEADER}" "${SCRIPT_PATH}"
        COMMENT "Generating Lucide charset file for MSDF generator"
    )
    set(CHARSET_ARG -charset ${CHARSET_OUTPUT})
    set(CHARSET_DEPENDENCY ${CHARSET_OUTPUT})
    set(CURRENT_ATLAS_SIZE 4096)
  else()
    set(CHARSET_FILE "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/charset.txt")
    set(CHARSET_ARG -charset ${CHARSET_FILE})
    set(CHARSET_DEPENDENCY ${CHARSET_FILE})
    set(CURRENT_ATLAS_SIZE ${ARG_ATLAS_SIZE})
  endif()
  add_custom_command(
    OUTPUT ${PNG_OUTPUT} ${CSV_OUTPUT}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GENERATED_FONTS_DIR}
    COMMAND msdf-atlas-gen-standalone
          -font ${ARG_FONT}
          -type mtsdf
          -overlap
          -scanline
          -errorcorrection indiscriminate
          ${CHARSET_ARG}
          -pxrange 8
          -size 32
          -pots
          -imageout ${PNG_OUTPUT}
          -csv ${CSV_OUTPUT}
    DEPENDS ${ARG_FONT} msdf-atlas-gen-standalone ${CHARSET_DEPENDENCY}
    COMMENT "Generating MTSDF Vector Font Atlas: ${FONT_NAME}"
  )
  set(TARGET_NAME "gen_font_${FONT_NAME}")
  add_custom_target(${TARGET_NAME} DEPENDS ${PNG_OUTPUT} ${CSV_OUTPUT})
  add_dependencies(avk_engine ${TARGET_NAME})
endfunction()

# =============================================================================
# 4. ENGINE TARGET
# =============================================================================
file(GLOB_RECURSE AVK_SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
    "include/*.h"
)

add_library(avk_engine STATIC ${AVK_SOURCES})
add_dependencies(avk_engine avk_compiled_shaders)

if(MSVC)
  target_compile_options(avk_engine PRIVATE /W4 /WX)
else()
  target_compile_options(avk_engine PRIVATE
    -Wall -Wextra -Werror
    -Wno-missing-field-initializers
  )
endif()

target_include_directories(avk_engine PUBLIC
    include
    ${GENERATED_FONTS_DIR}
    thirdparty/
    thirdparty/volk
    thirdparty/harfbuzz/src
    thirdparty/SheenBidi/Headers
    thirdparty/msdfgen
    thirdparty/msdf-atlas-gen
    thirdparty/Vera/src/vera_windowing/
)
target_include_directories(avk_engine SYSTEM PUBLIC
    thirdparty/vma_allocator
    thirdparty/clay
   ${tracy_SOURCE_DIR}/public
)

target_compile_definitions(avk_engine PUBLIC
    TRACY_ENABLE
)
set_target_properties(avk_engine PROPERTIES
    UNITY_BUILD ON
    UNITY_BUILD_BATCH_SIZE 16
)
set_source_files_properties(
    src/avk/avk_stb.cpp
    src/avk/avk_vma.cpp
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE
)

# ⚡ LINK VULKAN SO SYMBOLS RESOLVE FOR WINDOWING / SURFACE CALLS
target_link_libraries(avk_engine PUBLIC
    Vulkan::Vulkan
    volk
    vera_windowing
    glm
    freetype
    harfbuzz
    SheenBidi
    msdfgen-core
    msdfgen-ext
    msdf-atlas-gen
    Tracy::TracyClient
)

# Path definitions
target_compile_definitions(avk_engine PRIVATE
    AVK_SHADER_BINARY_DIR="${SHADER_OUTPUT_DIR}"
    AVK_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    AVK_GENERATED_FONTS_DIR="${GENERATED_FONTS_DIR}"
)

# Platform Flags & Vulkan Surface Extensions
if(WIN32)
  target_compile_definitions(avk_engine PUBLIC VERA_PLATFORM_WIN32 VK_USE_PLATFORM_WIN32_KHR UNICODE _UNICODE)
elseif(UNIX AND NOT APPLE)
  target_compile_definitions(avk_engine PUBLIC VERA_PLATFORM_LINUX VK_USE_PLATFORM_WAYLAND_KHR VK_USE_PLATFORM_XLIB_KHR VK_USE_PLATFORM_XCB_KHR)
endif()

# =============================================================================
# 5. REGISTER FONTS AT COMPILE TIME
# =============================================================================
atomic_add_font(
    FONT "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/Roboto-Regular.ttf"
    ATLAS_SIZE 512
)

atomic_add_font(
    FONT "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/lucide.ttf"
    ATLAS_SIZE 512
)

# =============================================================================
# 6. EXAMPLE EXECUTABLE
# =============================================================================
add_executable(avk_example example/basic.cpp)
target_link_libraries(avk_example PRIVATE avk_engine Vulkan::Vulkan)

if(MSVC)
  target_compile_options(avk_example PRIVATE
      /wd4100 /wd4189 /wd4324 /wd26495 /wd26813 /wd6386 /wd28251 /wd26115
  )
else()
  target_compile_options(avk_example PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
endif()

target_compile_definitions(avk_example PRIVATE
    TRACY_ENABLE
)

if(WIN32)
  target_compile_definitions(avk_example PRIVATE VERA_PLATFORM_WIN32 VK_USE_PLATFORM_WIN32_KHR UNICODE _UNICODE)
elseif(UNIX AND NOT APPLE)
  target_compile_definitions(avk_example PRIVATE VERA_PLATFORM_LINUX VK_USE_PLATFORM_WAYLAND_KHR VK_USE_PLATFORM_XLIB_KHR VK_USE_PLATFORM_XCB_KHR)
endif()

# Copy assets folder
add_custom_command(
    TARGET avk_example POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "${CMAKE_CURRENT_BINARY_DIR}/assets"
    COMMENT "Syncing assets folder to target build directory..."
)
