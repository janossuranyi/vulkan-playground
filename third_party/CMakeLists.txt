
add_library(vkbootstrap)
target_include_directories(vkbootstrap PUBLIC vkbootstrap)
target_link_libraries(vkbootstrap Vulkan::Vulkan)
target_sources(vkbootstrap PRIVATE
    vkbootstrap/VkBootstrap.h
    vkbootstrap/VkBootstrapDispatch.h
    vkbootstrap/VkBootstrap.cpp
)

add_library(vma)
target_include_directories(vma PUBLIC vma)
target_link_libraries(vma Vulkan::Vulkan)
target_sources(vma PRIVATE
    vma/vk_mem_alloc.h
    vma/vk_mem_alloc.cpp
)

#add_subdirectory(gli)

#add_library(glm INTERFACE)
#target_include_directories(glm INTERFACE glm)

add_library(nv_dds)
target_compile_definitions(nv_dds PUBLIC NV_DDS_NO_GL_SUPPORT)
target_include_directories(nv_dds PUBLIC nv_dds)
target_sources(nv_dds PRIVATE
    nv_dds/nv_dds.h
    nv_dds/nv_dds.cpp
)

add_subdirectory(dds_image)

add_library(stb_image)
target_include_directories(stb_image PUBLIC stb_image)
target_sources(stb_image PRIVATE
    stb_image/stb_image.h
    stb_image/stb_image.cpp
)

add_compile_definitions(
   TINYGLTF_NO_EXTERNAL_IMAGE
)

add_library(tiny_gltf)
target_include_directories(tiny_gltf PUBLIC tinygltf)
target_sources(tiny_gltf PRIVATE
    tinygltf/json.hpp
    tinygltf/stb_image_write.h
    tinygltf/tiny_gltf.h
    tinygltf/tiny_gltf.cpp
)

#if (WIN32)
#   set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
#endif()

add_subdirectory(volk)

add_library(spirv-reflect)
target_include_directories(spirv-reflect PUBLIC SPIRV-Reflect)
target_sources(spirv-reflect PRIVATE
    "SPIRV-Reflect/spirv_reflect.h"
    "SPIRV-Reflect/spirv_reflect.c"
    "SPIRV-Reflect/include/spirv/unified1/spirv.h"
)

add_library(mikktspace)
target_include_directories(mikktspace PUBLIC MikkTSpace)
target_sources(mikktspace PRIVATE
    MikkTSpace/mikktspace.h
    MikkTSpace/mikktspace.c
)

add_library(imgui)
target_include_directories(imgui PUBLIC 
    imgui
    imgui/backends
)

target_link_libraries(imgui Vulkan::Vulkan sdl2)
target_compile_definitions(imgui PUBLIC
    VK_NO_PROTOTYPES
)
target_sources(imgui PRIVATE 
    imgui/imgui.h
    imgui/imgui.cpp
    imgui/imconfig.h
    imgui/imgui_draw.cpp
    imgui/imgui_internal.h
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imstb_rectpack.h
    imgui/imstb_textedit.h
    imgui/imstb_truetype.h
    imgui/backends/imgui_impl_sdl2.h
    imgui/backends/imgui_impl_sdl2.cpp
    imgui/backends/imgui_impl_vulkan.h
    imgui/backends/imgui_impl_vulkan.cpp
)