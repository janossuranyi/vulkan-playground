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
	private:
		VkDevice _device;
		VkShaderModule _module;
	};
}
#endif // !VKJS_SHADER_MODULE_H_
