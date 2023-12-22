#pragma once
#include "pch.h"
#include "renderer/ResourceHandles.h"

namespace jsr {

    enum MaterialType { MATERIAL_TYPE_DIFFUSE, MATERIAL_TYPE_PHONG, MATERIAL_TYPE_METALLIC_ROUGHNESS };
	enum AlphaMode { ALPHA_MODE_OPAQUE, ALPHA_MODE_BLEND, ALPHA_MODE_MASK };

	inline int encodeAlphaMode(int flags, AlphaMode mode) {
		return flags | (((int)mode & 3) << 1);
	}

	inline int encodeDoubleSided(int flags, bool doubleSided) {
		return flags | int(doubleSided);
	}

	struct TexturePaths {
		std::filesystem::path albedoTexturePath;
		std::filesystem::path normalTexturePath;
		std::filesystem::path specularTexturePath;
		std::filesystem::path emissiveTexturePath;
	};

	struct Material {
		MaterialType type;
		std::string name;
		handle::Image baseColorTexture;
		handle::Image normalTexture;
		handle::Image specularTexture;
		handle::Image occlusionTexture;
		handle::Image emissiveTexture;
		handle::DescriptorSet shaderSet;
		glm::vec4 baseColorFactor = glm::vec4(1.0f);
		glm::vec3 emissiveFactor = glm::vec3(1.0f);
		glm::vec3 normalScale = glm::vec3(1.0f, -1.0f, 1.0f);
		float alphaCutoff = 0.5;
		float occlusionStrenght = 1.0f;
		float metallicFactor = 1.0f;
		float roughnessFactor = 1.0f;
		AlphaMode alphaMode = ALPHA_MODE_OPAQUE;
		bool doubleSided = false;
		TexturePaths texturePaths;
	};
}