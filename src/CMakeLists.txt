

set(DEMO_COMMON
    common/common.h
)

add_executable(demo
    ${DEMO_COMMON}
    pch.h
    vertex.h
    material.h
    bounds.h
    bounds.cpp
    frustum.h
    frustum.cpp
    light.h
    sample1.h
    sample1.cpp
    sample2.h
    sample2.cpp
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

source_group("Common" FILES ${DEMO_COMMON} )

add_compile_definitions(
   DDS_USE_STD_FILESYSTEM
   VKJS_USE_VOLK
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
    ktx2
#    Vulkan::Vulkan
    volk
)
