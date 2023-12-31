cmake_minimum_required (VERSION 3.8)

project ("3d-vulkan")

find_package(Vulkan REQUIRED)

set(CMAKE_CXX_STANDARD 17)

add_compile_definitions(
  DDS_USE_STD_FILESYSTEM
  GLM_FORCE_DEPTH_ZERO_TO_ONE
)

add_library(sdl2 INTERFACE)
set(sdl2_DIR "SDL_PATH" CACHE PATH "Path to SDL2")

if (WIN32)
target_include_directories(sdl2 INTERFACE ${sdl2_DIR}/include ${sdl2_image_DIR}/include)
target_link_directories(sdl2 INTERFACE ${sdl2_DIR}/lib/x64 ${sdl2_image_DIR}/lib/x64)
elseif (UNIX)
target_include_directories(sdl2 INTERFACE ${sdl2_DIR}/include/SDL2 ${sdl2_image_DIR}/include/SDL2)
target_link_directories(sdl2 INTERFACE ${sdl2_DIR}/lib ${sdl2_image_DIR}/lib)
endif()

target_link_libraries(sdl2 INTERFACE SDL2 SDL2main)

include_directories(
    include
    src 
    third_party/glm
    third_party/gli
    third_party/vma
    third_party/vkbootstrap
    third_party/nv_dds
    third_party/dds_image/include
    third_party/stb_image
    third_party/stb_dxt
    .
)

add_subdirectory(third_party)
add_subdirectory(jsrlib)
add_subdirectory(vkjs)
add_subdirectory(src)

find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

## find all the shader files under the shaders folder
file(GLOB_RECURSE GLSL_SOURCE_FILES
  "${PROJECT_SOURCE_DIR}/shaders/*.frag"
  "${PROJECT_SOURCE_DIR}/shaders/*.vert"
  "${PROJECT_SOURCE_DIR}/shaders/*.comp"
)

file(GLOB_RECURSE GLSL_INCLUDE_FILES
  "${PROJECT_SOURCE_DIR}/shaders/*.glsl"
)

## iterate each shader
foreach(GLSL ${GLSL_SOURCE_FILES})
  message(STATUS "BUILDING SHADER")
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/${FILE_NAME}.spv")
  message(STATUS ${GLSL})
  ##execute glslang command to compile that specific shader
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${GLSL_VALIDATOR} -g -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL} ${GLSL_INCLUDE_FILES})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    Shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
    )

