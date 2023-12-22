#ifndef VKJS_APPBASE_H
#define VKJS_APPBASE_H

#include <SDL.h>
#include <SDL_vulkan.h>
#include "vkjs.h"
#include "jsrlib/jsr_camera.h"

// We want to keep GPU and CPU busy. To do that we may start building a new command buffer while the previous one is still being executed
// This number defines how many frames may be worked on simultaneously at once
#define MAX_CONCURRENT_FRAMES 2

namespace vkjs {

	class AppBase {
	private:
		std::string window_title() const;
		uint32_t dest_width, dest_height;
		bool resizing{ false };
		void handle_mouse_move(int x, int y);
		void next_frame();
		void create_pipeline_cache();
		void create_command_pool();
		void create_synchronization_primitives();
		void init_swapchain();
		void setup_swapchain();
		void create_command_buffers();
		void destroy_command_buffers();
		void init_window();
		void init_imgui();
		void render_imgui();

		std::string shader_dir = "shaders";
		/** @brief Encapsulated physical and logical vulkan device */
	protected:
		bool minimized = false;
		bool quit = false;
		Device* device_wrapper;

		std::string shader_path() const;
		uint32_t frame_counter;
		uint32_t current_frame;
		uint32_t last_fps;
		VkSurfaceKHR surface;
		VkRenderPass imgui_render_pass = VK_NULL_HANDLE;
		VkDescriptorPool imgui_descriptor_pool = VK_NULL_HANDLE;
		VkFramebuffer imgui_fb[MAX_CONCURRENT_FRAMES]{};

		// Vulkan instance, stores all per-application states
		VkInstance instance;
		vkb::Instance vkb_instance;

		std::vector<std::string> supported_instance_extensions;
		
		// Physical device (GPU) that Vulkan will use
		VkPhysicalDevice physical_device;

		// Stores physical device properties (for e.g. checking device limits)
		VkPhysicalDeviceProperties device_properties;
		// Stores the features available on the selected physical device (for e.g. checking if a feature is available)
		VkPhysicalDeviceFeatures device_features;
		VkPhysicalDeviceFeatures2 device_features12;
		// Stores all available memory (type) properties for the physical device
		VkPhysicalDeviceMemoryProperties device_memory_properties;
		/** @brief Set of physical device features to be enabled for this example (must be set in the derived constructor) */
		VkPhysicalDeviceFeatures enabled_features{};
		VkPhysicalDeviceVulkan12Features enabled_features12{};

		/** @brief Set of device extensions to be enabled for this example (must be set in the derived constructor) */
		std::vector<const char*> enabled_device_extensions;
		std::vector<const char*> enabled_instance_extensions;

		/** @brief Optional pNext structure for passing extension structures to device creation */
		void* device_create_pnext_chain = nullptr;
		// Handle to the device graphics queue that command buffers are submitted to
		VkQueue queue;
		// Depth buffer format (selected during Vulkan initialization)
		VkFormat depth_format;
		// Command buffer pool
		VkCommandPool cmd_pool;
		/** @brief Pipeline stages used to wait at for graphics queue submissions */
		VkPipelineStageFlags submit_pipeline_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		// Contains command buffers and semaphores to be presented to the queue
		VkSubmitInfo submit_info;
		// Command buffers used for rendering
		std::array<VkCommandBuffer, MAX_CONCURRENT_FRAMES> draw_cmd_buffers;
		// Global render pass for frame buffer writes
		VkRenderPass render_pass = VK_NULL_HANDLE;
		// List of available frame buffers (same as number of swap chain images)
		std::vector<VkFramebuffer> framebuffers;
		// Active frame buffer index
		uint32_t current_buffer = 0;
		// Descriptor set pool
		VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
		// List of shader modules created (stored for cleanup)
		std::vector<VkShaderModule> shader_modules;
		// Pipeline cache object
		VkPipelineCache pipeline_cache;
		// Wraps the swap chain to present images (framebuffers) to the windowing system
		SwapChain swapchain;

		// Synchronization semaphores
		struct FrameSemaphores {
			// Swap chain image presentation
			VkSemaphore present_complete;
			// Command buffer submission and execution
			VkSemaphore render_complete;
		};

		std::array<VkFence, MAX_CONCURRENT_FRAMES> wait_fences;
		std::array<FrameSemaphores, MAX_CONCURRENT_FRAMES> semaphores;

		bool requires_stencil{ false };
		std::vector<char> keystate;

	public:
		bool prepared = false;
		bool resized = false;
		bool view_updated = false;
		uint32_t width = 1280;
		uint32_t height = 720;

		/** @brief Example settings that can be changed e.g. by command line arguments */
		struct Settings {
			/** @brief Activates validation layers (and message output) when set to true */
			bool validation = false;
			/** @brief Set to true if fullscreen mode has been requested via command line */
			bool fullscreen = false;
			/** @brief Set to true if v-sync will be forced for the swapchain */
			bool vsync = false;
			/** @brief Enable UI overlay */
			bool overlay = true;
		} settings;

		VkClearColorValue default_clear_color = { { 0.025f, 0.025f, 0.025f, 1.0f } };
		static std::vector<const char*> args;

		// Defines a frame rate independent timer value clamped from -1.0...1.0
		// For use in animations, rotations, etc.
		float timer = 0.0f;
		// Multiplier for speeding up (or slowing down) the global timer
		float timer_speed = 0.25f;
		float frame_time = 0.0f, prev_frame_time = 0.0f;

		bool paused = false;

		jsrlib::Camera camera;
		glm::vec2 mouse_pos;

		std::string title = "Vulkan Engine";
		std::string name = "vulkanEngine";
		
		uint32_t api_version = VK_API_VERSION_1_2;
		
		VmaAllocator allocator;

		struct {
			VkImage image;
			VmaAllocation mem;
			VkImageView view;
		} depth_stencil;

		struct {
			bool left = false;
			bool right = false;
			bool middle = false;
		} mouse_buttons;

		bool mouse_capture = false;
		SDL_Window* window;
		AppBase(bool enable_validation = false);
		virtual ~AppBase() noexcept;
		
		constexpr Device* device() {
			return device_wrapper;
		}

		bool init();
		void handle_events();

		virtual void update_imgui();

		/** @brief (Virtual) Creates the application wide Vulkan instance */
		virtual VkResult create_instance(bool enable_validation);
		/** @brief (Pure virtual) Render function to be implemented by the application */
		virtual void render() = 0;
		/** @brief (Virtual) Called when the camera view has changed */
		virtual void on_view_changed();
		/** @brief (Virtual) Called after a key was pressed, can be used to do custom key handling */
		virtual void on_key_pressed(uint32_t);
		/** @brief (Virtual) Called after the mouse cursor moved and before internal events (like camera rotation) is handled */
		virtual void on_mouse_moved(float x, float y, bool& handled);
		/** @brief (Virtual) Called when the window has been resized, can be used by the sample application to recreate resources */
		virtual void on_window_resized();
		/** @brief (Virtual) Called when resources have been recreated that require a rebuild of the command buffers (e.g. frame buffer), to be implemented by the sample application */
		virtual void build_command_buffers();
		/** @brief (Virtual) Setup default depth and stencil views */
		virtual void setup_depth_stencil();
		/** @brief (Virtual) Setup default framebuffers for all requested swapchain images */
		virtual void setup_framebuffer();
		/** @brief (Virtual) Setup a default renderpass */
		virtual void setup_render_pass();
		/** @brief (Virtual) Called after the physical device features have been read, can be used to set features to enable on the device */
		virtual void get_enabled_features();
		/** @brief (Virtual) Called after the physical device extensions have been read, can be used to enable extensions based on the supported extension listing*/
		virtual void get_enabled_extensions();
		/** @brief Prepares all Vulkan resources and functions required to run the sample */
		virtual void prepare();
		/** @brief Loads a SPIR-V shader file for the given shader stage */
		VkPipelineShaderStageCreateInfo load_shader(const std::string& fileName, VkShaderStageFlagBits stage);

		void window_resize();

		/** @brief Entry point for the main render loop */
		void run();

		/** Prepare the next frame for workload submission by acquiring the next swap chain image */
		void prepare_frame();
		/** @brief Presents the current image to the swap chain */
		void submit_frame();
		/** @brief (Virtual) Default image acquire + submission and command buffer submission function */
		virtual void render_frame();

		/** @brief (Virtual) Called when the UI overlay is updating, can be used to add custom elements to the overlay */
		virtual void on_update_gui();

	};

}
#endif // !VKJS_APPBASE_H
