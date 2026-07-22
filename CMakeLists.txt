cmake_minimum_required(VERSION 3.25)
project(atomic_vk VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -Werror  -Wno-missing-field-initializers)
endif()

# Locate Vulkan SDK
if(DEFINED ENV{VULKAN_SDK})
  set(VULKAN_HEADERS_DIR "$ENV{VULKAN_SDK}/include")
else()
  find_package(Vulkan REQUIRED)
  set(VULKAN_HEADERS_DIR ${Vulkan_INCLUDE_DIRS})
endif()

# Find Offline Shader Compiler glslc
find_program(GLSLC_EXECUTABLE glslc REQUIRED)
message(STATUS "Using offline glslc compiler: ${GLSLC_EXECUTABLE}")

# -----------------------------------------------------------------
# 1. Shader Compile Commands
# -----------------------------------------------------------------
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

# Create dynamic target for shaders
add_custom_target(avk_compiled_shaders DEPENDS ${SPV_OUTPUTS})

# -----------------------------------------------------------------
# 2. Fetch GLM
# -----------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE
)
set(GLM_TEST_ENABLE OFF CACHE BOOL "Disable GLM tests" FORCE)
FetchContent_MakeAvailable(glm)

# -----------------------------------------------------------------
# 3. Volk (We apply and propagate Vulkan platform definitions here)
# -----------------------------------------------------------------
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

# -----------------------------------------------------------------
# 4. Vera
# -----------------------------------------------------------------
add_subdirectory(thirdparty/Vera)

# -----------------------------------------------------------------
# 5. atomic_vk Engine Core
# -----------------------------------------------------------------
file(GLOB_RECURSE AVK_SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
    "include/*.h"
)

add_library(avk_engine STATIC ${AVK_SOURCES})
add_dependencies(avk_engine avk_compiled_shaders) # Compiles shaders BEFORE compiling engine code

target_include_directories(avk_engine PUBLIC
    include
    thirdparty/volk
    thirdparty/Vera/src/
    thirdparty/
)
target_include_directories(avk_engine SYSTEM PUBLIC
    thirdparty/vma_allocator
    thirdparty/clay

)
target_link_libraries(avk_engine PUBLIC volk vera glm)

# Pass the compiled shader folder path to our C++ compilation unit
target_compile_definitions(avk_engine PRIVATE AVK_SHADER_BINARY_DIR="${SHADER_OUTPUT_DIR}")

# -----------------------------------------------------------------
# 6. Create Sandbox Example
# -----------------------------------------------------------------
add_executable(avk_example example/basic.cpp)
target_link_libraries(avk_example PRIVATE avk_engine)

if(MSVC)
  target_compile_options(avk_example PRIVATE
        /wd4100 # unreferenced formal parameter
        /wd4189 # local variable is initialized but not referenced
        /wd4324 # structure was padded due to alignment specifier
        /wd26495 # C++ Core Guidelines: Member variable is uninitialized (VMA false positives)
        /wd26813 # C++ Core Guidelines: Use bitwise and to check if a flag is set (VMA)
        /wd6386  # MSVC: Buffer overrun false positives inside VMA structure allocations
        /wd28251 # MSVC: Inconsistent annotations on API boundaries
        /wd26115 # MSVC: Failing to release lock tracking false positives in STL mutexes
    )
endif()

if(WIN32)
  target_compile_definitions(avk_example PRIVATE VERA_PLATFORM_WIN32 UNICODE _UNICODE)
  target_compile_definitions(avk_engine PUBLIC VERA_PLATFORM_WIN32 UNICODE _UNICODE)
elseif(UNIX AND NOT APPLE)
  target_compile_definitions(avk_example PRIVATE VERA_PLATFORM_LINUX)
  target_compile_definitions(avk_engine PUBLIC VERA_PLATFORM_LINUX)
endif()
