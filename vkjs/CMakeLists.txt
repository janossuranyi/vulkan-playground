add_library(vkjs STATIC)

add_compile_definitions(
   VKJS_USE_VOLK
)

target_sources(vkjs PRIVATE
    vkjs.h
    span.h
    types.h
    vkcheck.h 
    vertex.h
    device.h
    buffer.h
    image.h
    keycodes.h
    input.h
    swapchain.h
    appbase.h
    vk_descriptors.h
    vk_types.h
    shader_module.h
    pipeline.h
    spirv_utils.h
    device.cpp
    buffer.cpp
    image.cpp
    input.cpp
    swapchain.cpp
    appbase.cpp
    pipeline.cpp
    gli_utils.h
    gli_utils.cpp
    VulkanInitializers.hpp
    vk_descriptors.cpp
    shader_module.cpp
    spirv_utils.cpp
)

target_link_libraries(vkjs 
    jsrlib 
    sdl2
    vkbootstrap
    vma
    stb_image
    imgui
    #    Vulkan::Vulkan
    volk
    spirv-reflect
)