#ifndef JSR_LIB_MESH_H_
#define JSR_LIB_MESH_H_

#include <glm/glm.hpp>
#include <vector>

namespace jsrlib {

	class MeshGenerator {
	public:
		void set_transform(const glm::mat4& matrix);
		virtual void generate(
			std::vector<glm::vec3>* positions,
			std::vector<glm::vec2>* texcoords,
			std::vector<glm::vec3>* normals,
			std::vector<glm::vec4>* tangents,
			std::vector<uint16_t>* indices) = 0;
	protected:
		glm::mat4 m_transform{ 1.0f };
	};

	class CubeGenerator :public MeshGenerator {
		// Inherited via MeshGenerator
		virtual void generate(std::vector<glm::vec3>* positions, std::vector<glm::vec2>* texcoords, std::vector<glm::vec3>* normals, std::vector<glm::vec4>* tangents, std::vector<uint16_t>* indices) override;
	};
}

#endif // !JSR_LIB_MESH_H_
