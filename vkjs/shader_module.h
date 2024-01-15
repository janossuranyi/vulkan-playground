#ifndef VKJS_SHADER_MODULE_H_
#define VKJS_SHADER_MODULE_H_

#include "vkjs.h"

namespace vkjs {
	class ShaderModule {
	public:
		ShaderModule(VkDevice device) : _device(device), _module(VK_NULL_HANDLE) {}
		~ShaderModule();
		VkResult create(std::filesystem::path filename);
		VkShaderModule module() const { return _module; }
		uint32_t size() const;
		const void* data() const;
	private:
		VkDevice _device;
		VkShaderModule _module;
		uint32_t _size;
		std::unique_ptr<uint32_t[]> _data;
	};
}
#endif // !VKJS_SHADER_MODULE_H_
