#include "shader_module.h"
#include "jsrlib/jsr_resources.h"

namespace vkjs {

	ShaderModule::~ShaderModule() {
		if (_module != VK_NULL_HANDLE) {
			vkDestroyShaderModule(_device, _module, nullptr);
		}
	}
	VkResult ShaderModule::create(std::filesystem::path filename)
	{
		const std::vector<uint8_t> spirv = jsrlib::Filesystem::root.ReadFile(filename.u8string());
		if (spirv.empty())
		{
			return VK_ERROR_UNKNOWN;
		}

		VkShaderModuleCreateInfo smci = {};
		smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		smci.codeSize = spirv.size();
		smci.pCode = (uint32_t*)spirv.data();
		return(vkCreateShaderModule(_device, &smci, nullptr, &_module));
	}
}