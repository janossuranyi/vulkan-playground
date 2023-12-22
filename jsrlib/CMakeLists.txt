add_library(jsrlib STATIC)

target_sources(jsrlib PRIVATE
    jsr_math.h
    jsr_math.cpp
    jsr_mesh.h
    jsr_mesh.cpp
    jsr_ringbuffer.h
    jsr_semaphore.h
    jsr_semaphore.cpp
    jsr_jobsystem.h
    jsr_jobsystem.cpp
    jsr_jobsystem2.h
    jsr_jobsystem2.cpp
    jsr_joblist.h
    jsr_joblist.cpp
    jsr_worker.h
    jsr_worker.cpp
    jsr_joblist_thread.h
    jsr_joblist_thread.cpp
    jsr_transient_buffer.h
    jsr_transient_buffer.cpp
    jsr_resources.h
    jsr_resources.cpp
    jsr_logger.h
    jsr_logger.cpp
    jsr_filesystem.cpp
    jsr_memblock.h
    jsr_memblock.cpp
    jsr_bounds.h
    jsr_bounds.cpp
    jsr_camera.h
    jsr_camera.cpp
    jsr_frustum.h
    jsr_frustum.cpp
)

target_link_libraries(jsrlib sdl2)
