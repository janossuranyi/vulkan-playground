
set(DEMO_BACKEND
    backend/vk.h
    backend/vk_barrier.h
    backend/vk_barrier.cpp
    backend/vk_device.h
    backend/vk_device.cpp
    backend/vk_backend.h
    backend/vk_backend.cpp
    backend/vk_command_pool.h
    backend/vk_command_pool.cpp
    backend/vk_command_buffer.h
    backend/vk_command_buffer.cpp
    backend/vk_resources.h
    backend/vk_pipeline.h
    backend/vk_pipeline.cpp
    backend/vk_check.h
    backend/vk_vertex.h
    backend/vk_vertex.cpp
    backend/vk_pipeline.h
    backend/vk_pipeline.cpp
    backend/vk_initializers.h
    backend/vk_initializers.cpp
    backend/vk_resources.cpp
    backend/vk_buffer.h
    backend/vk_buffer.cpp
    backend/vk_shader_filemgr.h
    backend/vk_shader_filemgr.cpp
)

set(DEMO_RENDERER
    renderer/Window.h
    renderer/Window.cpp
    renderer/Resources.h
    renderer/Vulkan.h
    renderer/VulkanDevice.h
    renderer/VulkanDevice.cpp
    renderer/VulkanCheck.h
    renderer/VulkanSwapchain.h
    renderer/VulkanSwapchain.cpp
    renderer/VulkanRenderer.h
    renderer/VulkanRenderer.cpp
    renderer/VulkanInitializers.h
    renderer/VulkanInitializers.cpp
    renderer/VulkanBuffer.h
    renderer/VulkanBuffer.cpp
    renderer/VulkanImage.h
    renderer/VulkanImage.cpp
    renderer/VulkanBarrier.h
    renderer/VulkanBarrier.cpp
    renderer/VulkanPipeline.h
    renderer/VulkanPipeline.cpp
    renderer/FrameCounter.h
    renderer/FrameCounter.cpp
    renderer/Vertex.h
    renderer/Vertex.cpp
    renderer/ResourceDescriptions.h
    renderer/ResourceHandles.h
    renderer/gli_utils.h
    renderer/gli_utils.cpp
    renderer/RenderFrontend.h
    renderer/RenderFrontend.cpp
    renderer/Camera.h
    renderer/Camera.cpp
    renderer/Frustum.h
    renderer/Frustum.cpp
    renderer/Bounds.h
    renderer/Bounds.cpp
    renderer/Material.h
    renderer/Light.h
    renderer/ConstPool.h
    renderer/Pipeline.h
)

set(DEMO_PIPELINES
    renderer/pipelines/DepthPrePass.h
    renderer/pipelines/DepthPrePass.cpp
    renderer/pipelines/ForwardLightingPass.h
    renderer/pipelines/ForwardLightingPass.cpp
    renderer/pipelines/CompImageGen.h
    renderer/pipelines/CompImageGen.cpp
)

set(DEMO_COMMON
    common/common.h
)

add_executable(demo
    ${DEMO_COMMON}
    ${DEMO_RENDERER}
    ${DEMO_PIPELINES}
    pch.h
    demo.h
    demo.cpp
    demo_utils.cpp
    jobsys.cpp
    jobsys.h
    world.h
    world.cpp
    mesh_data.h
    gltf_loader.h
    gltf_loader.cpp
)

source_group("Backend" FILES ${DEMO_BACKEND} )
source_group("Common" FILES ${DEMO_COMMON} )
source_group("Renderer" FILES ${DEMO_RENDERER} )
source_group("Renderer\\Pipelines" FILES ${DEMO_PIPELINES} )

add_compile_definitions(
   DDS_USE_STD_FILESYSTEM
   VKS_USE_VOLK
   TINYGLTF_NO_EXTERNAL_IMAGE
)

#    Vulkan::Vulkan
target_link_libraries(demo
    jsrlib
    vkjs
    sdl2
    vkbootstrap
    vma
    stb_image
    tiny_gltf
    spirv-reflect
    imgui
#    Vulkan::Vulkan
    volk
)
