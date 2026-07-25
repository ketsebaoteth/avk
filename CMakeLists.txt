cmake_minimum_required(VERSION 3.25)
project(atomic_vk VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -Werror -Wno-missing-field-initializers)
endif()

# Vulkan SDK
if(DEFINED ENV{VULKAN_SDK})
  set(VULKAN_HEADERS_DIR "$ENV{VULKAN_SDK}/include")
else()
  find_package(Vulkan REQUIRED)
  set(VULKAN_HEADERS_DIR ${Vulkan_INCLUDE_DIRS})
endif()

# Shader Compilation (glslc)
find_program(GLSLC_EXECUTABLE glslc REQUIRED)

set(SHADER_SOURCES
    shaders/ui.vert
    shaders/ui.frag
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

# Thirdparty Dependencies
include(FetchContent)
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
  target_compile_definitions(volk PUBLIC VK_USE_PLATFORM_WAYLAND_KHR VK_USE_PLATFORM_XLIB_KHR)
endif()

# FreeType
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)

add_subdirectory(thirdparty/Vera)
add_subdirectory(thirdparty/freetype)

# Engine Target
file(GLOB_RECURSE AVK_SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
    "include/*.h"
)

add_library(avk_engine STATIC ${AVK_SOURCES})
add_dependencies(avk_engine avk_compiled_shaders)

target_include_directories(avk_engine PUBLIC
    include
    thirdparty/
    thirdparty/volk
    thirdparty/Vera/src/vera_windowing/
)
target_include_directories(avk_engine SYSTEM PUBLIC
    thirdparty/vma_allocator
    thirdparty/clay
)

target_link_libraries(avk_engine PUBLIC volk vera_windowing glm freetype)

# Path definitions injected for engine asset & shader loading
target_compile_definitions(avk_engine PRIVATE
    AVK_SHADER_BINARY_DIR="${SHADER_OUTPUT_DIR}"
    AVK_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

# Platform Flags
if(WIN32)
  target_compile_definitions(avk_engine PUBLIC VERA_PLATFORM_WIN32 UNICODE _UNICODE)
elseif(UNIX AND NOT APPLE)
  target_compile_definitions(avk_engine PUBLIC VERA_PLATFORM_LINUX)
endif()

# Example Executable
add_executable(avk_example example/basic.cpp)
target_link_libraries(avk_example PRIVATE avk_engine)

if(MSVC)
  target_compile_options(avk_example PRIVATE
      /wd4100 /wd4189 /wd4324 /wd26495 /wd26813 /wd6386 /wd28251 /wd26115
  )
endif()

if(WIN32)
  target_compile_definitions(avk_example PRIVATE VERA_PLATFORM_WIN32 UNICODE _UNICODE)
elseif(UNIX AND NOT APPLE)
  target_compile_definitions(avk_example PRIVATE VERA_PLATFORM_LINUX)
endif()

# Copy assets next to output binary as build post-step fallback
add_custom_command(
    TARGET avk_example POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:avk_example>/assets"
    COMMENT "Syncing assets folder to target build directory..."
)
