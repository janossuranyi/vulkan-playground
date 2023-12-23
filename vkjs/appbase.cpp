#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#include <SDL_vulkan.h>

#include "appbase.h"
#include "vkcheck.h"

#include "VulkanInitializers.hpp"
#include "jsrlib/jsr_camera.h"
#include "jsrlib/jsr_logger.h"

namespace vkjs
{
	static void check_vk_result(VkResult err)
	{
		if (err == 0)
			return;
		fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
		if (err < 0)
			abort();
	}

	std::string AppBase::window_title() const
	{
		return title;
	}
	void AppBase::handle_mouse_move(int x, int y)
	{
		bool handled = false;
		on_mouse_moved(float(x), float(y), handled);

		if (!handled)
		{
			camera.ProcessMouseMovement(float(x), float(-y));
		}
	}
	void AppBase::next_frame()
	{
		if (minimized || paused) {
			return;
		}

		time = (float)SDL_GetTicks64();
		frame_time = time - prev_frame_time;
		prev_frame_time = time;

		current_frame = frame_counter % MAX_CONCURRENT_FRAMES;
		
		vkWaitForFences(*device_wrapper, 1, &wait_fences[current_frame], VK_TRUE, UINT64_MAX);

		VkResult result = swapchain.acquire_next_image(semaphores[current_frame].present_complete, &current_buffer);
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			window_resize();
			return;
		}
		else if ((result != VK_SUCCESS) && (result != VK_SUBOPTIMAL_KHR)) {
			throw "Could not acquire the next swap chain image!";
		}

		VK_CHECK(vkResetFences(*device_wrapper, 1, &wait_fences[current_frame]));
		
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame();

		ImGui::NewFrame();

		const VkCommandBuffer cmd = draw_cmd_buffers[current_frame];

		VK_CHECK(vkResetCommandBuffer(cmd, 0));
		device()->begin_command_buffer(cmd);
		
		render();

		on_update_gui();
		render_imgui();
		VK_CHECK(vkEndCommandBuffer(cmd));

		VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo submit = vks::initializers::submitInfo();
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &cmd;
		submit.pSignalSemaphores = &semaphores[current_frame].render_complete;
		submit.signalSemaphoreCount = 1;
		submit.waitSemaphoreCount = 1;
		submit.pWaitSemaphores = &semaphores[current_frame].present_complete;
		submit.pWaitDstStageMask = &stageMask;
		VK_CHECK(vkQueueSubmit(queue, 1, &submit, wait_fences[current_frame]));

		result = swapchain.present_image(queue, current_buffer, semaphores[current_frame].render_complete);

		if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR)) {
			window_resize();
		}
		else if (result != VK_SUCCESS) {
			throw "Could not present the image to the swap chain!";
		}

		frame_counter++;
	}
	void AppBase::create_pipeline_cache()
	{
	}
	void AppBase::create_command_pool()
	{
	}
	void AppBase::create_synchronization_primitives()
	{
		using namespace vks;

		for (size_t i(0); i < MAX_CONCURRENT_FRAMES; ++i)
		{
			// create wait/signal semphores
			VkSemaphoreCreateInfo sci = initializers::semaphoreCreateInfo();
			VK_CHECK(vkCreateSemaphore(*device_wrapper, &sci, nullptr, &semaphores[i].present_complete));
			VK_CHECK(vkCreateSemaphore(*device_wrapper, &sci, nullptr, &semaphores[i].render_complete));

			// create wait fences
			VkFenceCreateInfo fci = initializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
			vkCreateFence(*device_wrapper, &fci, nullptr, &wait_fences[i]);
		}
	}
	void AppBase::init_swapchain()
	{
		VkSurfaceFormatKHR format{};
		format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
		format.format = VK_FORMAT_B8G8R8A8_SRGB;

		uint32_t sfc{ 0 };
		std::vector<VkSurfaceFormatKHR> formats;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device_wrapper->vkb_physical_device, surface, &sfc, nullptr);	formats.resize(sfc);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device_wrapper->vkb_physical_device, surface, &sfc, formats.data());

		for (const auto& it : formats) {
			if (it.format == VK_FORMAT_B8G8R8A8_UNORM || it.format == VK_FORMAT_R8G8B8A8_UNORM) {
				format = it;
				break;
			}
		}

		vkb::SwapchainBuilder builder{device()->vkb_device};
		auto swap_ret = builder
			//.set_desired_format(format)
			.set_old_swapchain(swapchain.vkb_swapchain)
			.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
			.set_old_swapchain(swapchain.vkb_swapchain).build();

		if (!swap_ret) {
			swapchain.vkb_swapchain.swapchain = VK_NULL_HANDLE;
			swap_ret = builder.build();
		}

		if (swap_ret) {
			if (swapchain.vkb_swapchain)
			{
				swapchain.vkb_swapchain.destroy_image_views(swapchain.views);
				vkb::destroy_swapchain(swapchain.vkb_swapchain);
			}
			swapchain.vkb_swapchain = swap_ret.value();
			jsrlib::Info("Swapchain created [%dx%d]", swapchain.vkb_swapchain.extent.width, swapchain.vkb_swapchain.extent.height);
			width = swapchain.vkb_swapchain.extent.width;
			height = swapchain.vkb_swapchain.extent.height;
			swapchain.images = swapchain.vkb_swapchain.get_images().value();
			swapchain.views = swapchain.vkb_swapchain.get_image_views().value();
		}
	}
	void AppBase::setup_swapchain()
	{
	}
	void AppBase::create_command_buffers()
	{
		VkCommandPoolCreateInfo cpci = vks::initializers::commandPoolCreateInfo();
		cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		cpci.queueFamilyIndex = device()->queue_family_indices.graphics;
		VK_CHECK(vkCreateCommandPool(*device_wrapper, &cpci, nullptr, &cmd_pool));

		VkCommandBufferAllocateInfo cbai = vks::initializers::commandBufferAllocateInfo(cmd_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 2);

		vkAllocateCommandBuffers(*device_wrapper, &cbai, draw_cmd_buffers.data());
	}
	void AppBase::destroy_command_buffers()
	{
		vkDestroyCommandPool(*device_wrapper, cmd_pool, nullptr);
	}
	void AppBase::init_window()
	{
		SDL_Init(SDL_INIT_VIDEO);

		SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
		if (settings.fullscreen)
		{
			if (width == 0 && height == 0)
			{
				// borderless desktop resolution
				window_flags = (SDL_WindowFlags)(window_flags | SDL_WINDOW_FULLSCREEN_DESKTOP);
			}
			else
			{
				window_flags = (SDL_WindowFlags)(window_flags | SDL_WINDOW_FULLSCREEN);
			}
		}

		window = SDL_CreateWindow(
			title.c_str(),
			SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED,
			width,
			height,
			window_flags
		);

		if (window == nullptr) {
			throw std::runtime_error("SDL cannot create window !");
		}

		int x, y;
		
		SDL_GetWindowSize(window, &x, &y);
		width = uint32_t(x);
		height = uint32_t(y);

		keystate.resize(65535, false);
	}

	void AppBase::init_imgui()
	{
		const std::vector<VkDescriptorPoolSize> poolsize = {
				{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
				{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
				{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
		};

		VkDescriptorPoolCreateInfo dpci = vks::initializers::descriptorPoolCreateInfo(poolsize, 1000);

		VK_CHECK(vkCreateDescriptorPool(*device_wrapper, &dpci, nullptr, &imgui_descriptor_pool));

		VkAttachmentDescription color = {};
		color.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		color.format = swapchain.vkb_swapchain.image_format;
		color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color.samples = VK_SAMPLE_COUNT_1_BIT;
		VkAttachmentReference colorRef = {};
		colorRef.attachment = 0;
		colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		VkSubpassDescription subpass = {};
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef;
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.flags = 0;
		VkSubpassDependency dep = {};
		dep.dependencyFlags = 0;
		dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
		dep.srcSubpass = VK_SUBPASS_EXTERNAL;
		dep.dstSubpass = 0;

		VkRenderPassCreateInfo imguiRenderPassCI = {};
		imguiRenderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		imguiRenderPassCI.subpassCount = 1;
		imguiRenderPassCI.attachmentCount = 1;
		imguiRenderPassCI.dependencyCount = 1;
		imguiRenderPassCI.pAttachments = &color;
		imguiRenderPassCI.pDependencies = &dep;
		imguiRenderPassCI.pSubpasses = &subpass;

		VK_CHECK(vkCreateRenderPass(device_wrapper->logical_device, &imguiRenderPassCI, nullptr, &imgui_render_pass));
		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		ImGuiStyle* style = &ImGui::GetStyle();
		style->FrameRounding = 3.0f;
		style->Colors[ImGuiCol_WindowBg].w = 240.f / 255.f;

#ifdef VKJS_USE_VOLK
		ImGui_ImplVulkan_LoadFunctions([](const char* function_name, void* vulkan_instance)
			{
				return vkGetInstanceProcAddr(*(reinterpret_cast<VkInstance*>(vulkan_instance)), function_name);
			}, &instance);
#endif
		// Setup Platform/Renderer backends
		ImGui_ImplSDL2_InitForVulkan(window);
		ImGui_ImplVulkan_InitInfo init_info = {};
		init_info.Instance = instance;
		init_info.PhysicalDevice = device()->vkb_physical_device;
		init_info.Device = device()->logical_device;
		init_info.QueueFamily = device()->queue_family_indices.graphics;
		init_info.Queue = device()->graphics_queue;
		init_info.PipelineCache = VK_NULL_HANDLE;
		init_info.DescriptorPool = imgui_descriptor_pool;
		init_info.Subpass = 0;
		init_info.MinImageCount = 2;
		init_info.ImageCount = swapchain.vkb_swapchain.image_count;
		init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		init_info.Allocator = nullptr;
		init_info.CheckVkResultFn = check_vk_result;
		ImGui_ImplVulkan_Init(&init_info, imgui_render_pass);

		device()->execute_commands([&](VkCommandBuffer cmd)
			{
				ImGui_ImplVulkan_CreateFontsTexture(cmd);
			});

		ImGui_ImplVulkan_DestroyFontUploadObjects();

		//ImGui_ImplVulkan_NewFrame();
		//ImGui_ImplSDL2_NewFrame();
	}

	void AppBase::render_imgui()
	{
		ImGui::Render();
		ImDrawData* draw_data = ImGui::GetDrawData();

		VkFramebufferCreateInfo fbCI = vks::initializers::framebufferCreateInfo();
		fbCI.attachmentCount = 1;
		fbCI.pAttachments = &swapchain.views[current_buffer];
		fbCI.layers = 1;
		fbCI.width = width;
		fbCI.height = height;
		fbCI.renderPass = imgui_render_pass;
		
		if (imgui_fb[current_frame]) {
			vkDestroyFramebuffer(*device_wrapper, imgui_fb[current_frame], nullptr);
		}
		VK_CHECK(vkCreateFramebuffer(*device_wrapper, &fbCI, nullptr, &imgui_fb[current_frame]));

		VkRenderPassBeginInfo renderPassInfo = vks::initializers::renderPassBeginInfo();
		renderPassInfo.clearValueCount = 0;
		renderPassInfo.framebuffer = imgui_fb[current_frame];
		renderPassInfo.renderArea.extent = swapchain.vkb_swapchain.extent;
		renderPassInfo.renderPass = imgui_render_pass;

		const auto cmd = draw_cmd_buffers[current_frame];
		vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		ImGui_ImplVulkan_RenderDrawData(draw_data, cmd);
		vkCmdEndRenderPass(cmd);

	}

	std::string AppBase::shader_path() const
	{
		return shader_dir;
	}
	AppBase::AppBase(bool enable_validation)
	{
		settings.validation = enable_validation;
	}
	AppBase::~AppBase() noexcept
	{
		vkDeviceWaitIdle(*device_wrapper);

		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();

		vkDestroyDescriptorPool(*device_wrapper, imgui_descriptor_pool, nullptr);
		vkDestroyRenderPass(*device_wrapper, imgui_render_pass, nullptr);
		destroy_command_buffers();

		for (size_t i(0); i < MAX_CONCURRENT_FRAMES; ++i)
		{
			// create wait/signal semphores
			vkDestroySemaphore(*device_wrapper, semaphores[i].present_complete, 0);
			vkDestroySemaphore(*device_wrapper, semaphores[i].render_complete, 0);
			vkDestroyFence(*device_wrapper, wait_fences[i], 0);
			vkDestroyFramebuffer(*device_wrapper, imgui_fb[i], nullptr);
		}

		swapchain.vkb_swapchain.destroy_image_views(swapchain.views);
		vkb::destroy_swapchain(swapchain.vkb_swapchain);

		if (device_wrapper) { delete device_wrapper; }
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkb::destroy_instance(vkb_instance);
		if (window) SDL_DestroyWindow(window);
		SDL_Quit();
	}
	bool AppBase::init()
	{
		VkResult res;

		init_window();

		res = create_instance(settings.validation);
		if (res != VK_SUCCESS) {
			jsrlib::Error("Could not create Vulkan instance :%d", res);
			return false;
		}

		// Choose physical device

#ifdef VKJS_USE_VOLK
		volkLoadInstanceOnly(instance);
#endif
		get_enabled_features();
		get_enabled_extensions();

		VkPhysicalDeviceShaderDrawParametersFeatures shader_draw_parameters_features = {};
		shader_draw_parameters_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
		shader_draw_parameters_features.pNext = nullptr;
		shader_draw_parameters_features.shaderDrawParameters = VK_TRUE;

		SDL_bool surf_res = SDL_Vulkan_CreateSurface(window, instance, &surface);
		// get the surface of the window we opened with SDL
		if (SDL_TRUE != surf_res) {
			jsrlib::Error("SDL failed to create Vulkan surface !");
			return false;
		}

		//use vkbootstrap to select a GPU.
		//We want a GPU that can write to the SDL surface and supports Vulkan 1.2
		vkb::PhysicalDeviceSelector selector(vkb_instance);
		selector
			.set_minimum_version(1, 2)
			.set_surface(surface)
			.set_required_features(enabled_features)
			.set_required_features_12(enabled_features12)
			.add_required_extension_features(shader_draw_parameters_features);

		for (const auto* extension_name : enabled_device_extensions) {
			selector.add_required_extension(extension_name);
		}
		
		auto selector_result = selector.select();

		if (!selector_result) {
			return false;
		}

		vkb::PhysicalDevice physicalDevice = selector_result.value();

		std::string gpuType;
		switch (physicalDevice.properties.deviceType) {
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: gpuType = "Discrete GPU"; break;
		case VK_PHYSICAL_DEVICE_TYPE_CPU: gpuType = "CPU"; break;
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: gpuType = "Integrated GPU"; break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: gpuType = "Virtual GPU"; break;
		case VK_PHYSICAL_DEVICE_TYPE_OTHER: gpuType = "Other GPU"; break;
		default: gpuType = "unknown";
		}

		jsrlib::Info("Selected GPU: %s, %s", physicalDevice.name.c_str(), gpuType.c_str());
		jsrlib::Info("API version: %d.%d.%d",
			VK_API_VERSION_MAJOR(physicalDevice.properties.apiVersion),
			VK_API_VERSION_MINOR(physicalDevice.properties.apiVersion),
			VK_API_VERSION_PATCH(physicalDevice.properties.apiVersion));

		device_wrapper = new Device(physicalDevice, instance);
		queue = device_wrapper->get_graphics_queue();
		
		return true;
	}

	void AppBase::handle_events()
	{
		SDL_Event e;
		ImGuiIO& io = ImGui::GetIO();

		while (SDL_PollEvent(&e) != SDL_FALSE) {

			if (e.type == SDL_QUIT) {
				quit = true;
				break;
			}


			if (e.type == SDL_WINDOWEVENT) {
				switch (e.window.event) {
				case SDL_WINDOWEVENT_MINIMIZED:
					minimized = true;
					break;
				case SDL_WINDOWEVENT_RESTORED:
					minimized = false;
					break;
				case SDL_WINDOWEVENT_RESIZED:
					SDL_GetWindowSize(window, (int*)&dest_width, (int*)&dest_height);
					window_resize();
					break;
				}
			}
			
			ImGui_ImplSDL2_ProcessEvent(&e);
			if (io.WantCaptureMouse || io.WantCaptureKeyboard) {
				continue;
			}

			if (e.type == SDL_KEYDOWN) {
				if (e.key.keysym.sym == SDLK_ESCAPE) {
					quit = true;
					break;
				}
				const uint32_t keycode = (uint32_t) (e.key.keysym.sym & 0xffff);
				keystate[keycode] = true;

				on_key_pressed(e.key.keysym.sym);
			}
			else if (e.type == SDL_KEYUP) {
				const uint32_t keycode = (uint32_t) (e.key.keysym.sym & 0xffff);
				keystate[keycode] = false;
			}
			else if (e.type == SDL_MOUSEBUTTONDOWN)
			{
				switch (e.button.button) {
				case SDL_BUTTON_RIGHT: mouse_buttons.right = true; break;
				case SDL_BUTTON_LEFT: mouse_buttons.left = true; break;
				case SDL_BUTTON_MIDDLE: mouse_buttons.middle = true; break;
				}
			}
			else if (e.type == SDL_MOUSEBUTTONUP)
			{
				switch (e.button.button) {
				case SDL_BUTTON_RIGHT: mouse_buttons.right = false; break;
				case SDL_BUTTON_LEFT: mouse_buttons.left = false; break;
				case SDL_BUTTON_MIDDLE: mouse_buttons.middle = false; break;
				}
			}
			else if (e.type == SDL_MOUSEWHEEL)
			{
				float r = (float)e.wheel.y;
				if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) r = -r;
				camera.ProcessMouseScroll((float)e.wheel.y);
			}

		}

		if (mouse_buttons.right && !mouse_capture) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			mouse_capture = true;
		}
		if (!mouse_buttons.right && mouse_capture) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			mouse_capture = false;
		}

		if (keystate[SDLK_w]) {
			camera.ProcessKeyboard(jsrlib::FORWARD, frame_time);
			//jsrlib::Info("Camera.Pos[%.3f, %.3f, %.3f]", view.Position.x, view.Position.y, view.Position.z);
		}
		if (keystate[SDLK_s]) {
			camera.ProcessKeyboard(jsrlib::BACKWARD, frame_time);
			//jsrlib::Info("Camera.Pos[%.3f, %.3f, %.3f]", view.Position.x, view.Position.y, view.Position.z);
		}
		if (keystate[SDLK_a]) {
			camera.ProcessKeyboard(jsrlib::LEFT, frame_time);
			//jsrlib::Info("Camera.Pos[%.3f, %.3f, %.3f]", view.Position.x, view.Position.y, view.Position.z);
		}
		if (keystate[SDLK_d]) {
			camera.ProcessKeyboard(jsrlib::RIGHT, frame_time);
			//jsrlib::Info("Camera.Pos[%.3f, %.3f, %.3f]", view.Position.x, view.Position.y, view.Position.z);
		}
		int x, y;
		SDL_GetRelativeMouseState(&x, &y);
		if (mouse_capture)
		{
			camera.ProcessMouseMovement(float(x), float(-y), true);
			//jsrlib::Info("Camera.: %.3f, Camera.Pitch: %.3f", view.Yaw, view.Pitch);
		}
		SDL_GetMouseState(&x, &y);
		mouse_pos = glm::vec2(x, y);

	}

	VkResult AppBase::create_instance(bool enable_validation)
	{

#ifdef VKJS_USE_VOLK
		VK_CHECK(volkInitialize());
#endif

		vkb::InstanceBuilder builder;
		// init vulkan instance with debug features
		builder
			.set_app_name("VKJS_Engine")
			.set_app_version(VK_MAKE_VERSION(1, 0, 0))
			.require_api_version(1, 2, 0);


		auto system_info_ret = vkb::SystemInfo::get_system_info();
		if (!system_info_ret) {
			jsrlib::Error("VkBootStrap error, cannot get system info: %s", system_info_ret.error().message().c_str());
			return system_info_ret.full_error().vk_result;
		}

		auto sysinf = system_info_ret.value();
		if (enable_validation && sysinf.validation_layers_available)
		{
			builder
				.enable_validation_layers()
				.use_default_debug_messenger();
		}

		if (sysinf.debug_utils_available)
		{
		}

		auto instance_builder_result = builder.build();

		if (!instance_builder_result) {
			return instance_builder_result.full_error().vk_result;
		}

		vkb_instance = instance_builder_result.value();
		instance = vkb_instance.instance;

		return VK_SUCCESS;
	}
	void AppBase::on_view_changed()
	{
	}
	void AppBase::on_key_pressed(uint32_t)
	{
	}
	void AppBase::on_mouse_moved(float x, float y, bool& handled)
	{
	}
	void AppBase::on_window_resized()
	{
	}
	void AppBase::build_command_buffers()
	{
	}
	void AppBase::setup_depth_stencil()
	{
	}
	void AppBase::setup_framebuffer()
	{
	}
	void AppBase::setup_render_pass()
	{
	}
	void AppBase::get_enabled_features()
	{
		/*
		enabled_features.samplerAnisotropy = VK_TRUE;
		enabled_features.fragmentStoresAndAtomics = VK_TRUE;
		enabled_features.fillModeNonSolid = VK_TRUE;
		enabled_features.depthClamp = VK_TRUE;
		enabled_features.geometryShader = VK_TRUE;
		enabled_features.textureCompressionBC = VK_TRUE;
		enabled_features.shaderSampledImageArrayDynamicIndexing = VK_TRUE;

		enabled_features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		enabled_features12.pNext = nullptr;
		enabled_features12.hostQueryReset = VK_TRUE;
		enabled_features12.runtimeDescriptorArray = VK_TRUE;
		enabled_features12.descriptorBindingPartiallyBound = VK_TRUE;
		enabled_features12.descriptorBindingVariableDescriptorCount = VK_TRUE;
		enabled_features12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
		enabled_features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
		enabled_features12.shaderFloat16 = VK_TRUE;
		enabled_features12.descriptorIndexing = VK_TRUE;
		*/
	}
	void AppBase::get_enabled_extensions()
	{
		/*
		enabled_device_extensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
		enabled_device_extensions.push_back(VK_EXT_SHADER_SUBGROUP_BALLOT_EXTENSION_NAME);
		enabled_device_extensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
		enabled_device_extensions.push_back(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME);
		*/

	}
	void AppBase::prepare()
	{
		create_synchronization_primitives();
		init_swapchain();
		create_command_buffers();
		init_imgui();
		setup_render_pass();
	}
	void AppBase::window_resize()
	{
		if (!prepared || minimized)
		{
			return;
		}
		resized = true;
		prepared = false;

		// Ensure all operations on the device have been finished before destroying resources
		vkDeviceWaitIdle(device()->logical_device);

		width = dest_width;
		height = dest_height;
		init_swapchain();
		on_window_resized();

		prepared = true;

	}
	void AppBase::run()
	{
		while (!quit) {
			
			handle_events();
			next_frame();

		}
	}
	void AppBase::prepare_frame()
	{
	}
	void AppBase::submit_frame()
	{
	}
	void AppBase::render_frame()
	{
	}
	void AppBase::on_update_gui()
	{
		ImGui::ShowDemoWindow();
	}
}