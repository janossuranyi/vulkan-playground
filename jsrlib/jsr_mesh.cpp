#include "jsr_mesh.h"

namespace jsrlib {

	void MeshGenerator::set_transform(const glm::mat4& matrix)
	{
		m_transform = matrix;
	}

	void CubeGenerator::generate(std::vector<glm::vec3>* positions, std::vector<glm::vec2>* texcoords, std::vector<glm::vec3>* normals, std::vector<glm::vec4>* tangents, std::vector<uint16_t>* indices)
	{
		using namespace glm;

		const int numVerts = 8;
		const int numIndices = 36;

		assert(positions);
		assert(indices);
		indices->resize(numIndices);
		positions->resize(numVerts);

		const float low =  0.0f;
		const float high = 1.0f;

		vec3 center(0.0f);
		vec3 mx(low, 0.0f, 0.0f);
		vec3 px(high, 0.0f, 0.0f);
		vec3 my(0.0f, low, 0.0f);
		vec3 py(0.0f, high, 0.0f);
		vec3 mz(0.0f, 0.0f, low);
		vec3 pz(0.0f, 0.0f, high);

		(*positions)[0] = center + mx + my + mz;
		(*positions)[1] = center + px + my + mz;
		(*positions)[2] = center + px + py + mz;
		(*positions)[3] = center + mx + py + mz;
		(*positions)[4] = center + mx + my + pz;
		(*positions)[5] = center + px + my + pz;
		(*positions)[6] = center + px + py + pz;
		(*positions)[7] = center + mx + py + pz;

		if (texcoords) {
			texcoords->resize(numVerts);
			for (int i = 0; i < numVerts; ++i) {
				(*texcoords)[i] = vec2((*positions)[i]);
			}
		}

		// bottom
		(*indices)[0 * 3 + 0] = 2;
		(*indices)[0 * 3 + 1] = 3;
		(*indices)[0 * 3 + 2] = 0;
		(*indices)[1 * 3 + 0] = 1;
		(*indices)[1 * 3 + 1] = 2;
		(*indices)[1 * 3 + 2] = 0;
		// back
		(*indices)[2 * 3 + 0] = 5;
		(*indices)[2 * 3 + 1] = 1;
		(*indices)[2 * 3 + 2] = 0;
		(*indices)[3 * 3 + 0] = 4;
		(*indices)[3 * 3 + 1] = 5;
		(*indices)[3 * 3 + 2] = 0;
		// left
		(*indices)[4 * 3 + 0] = 7;
		(*indices)[4 * 3 + 1] = 4;
		(*indices)[4 * 3 + 2] = 0;
		(*indices)[5 * 3 + 0] = 3;
		(*indices)[5 * 3 + 1] = 7;
		(*indices)[5 * 3 + 2] = 0;
		// right
		(*indices)[6 * 3 + 0] = 1;
		(*indices)[6 * 3 + 1] = 5;
		(*indices)[6 * 3 + 2] = 6;
		(*indices)[7 * 3 + 0] = 2;
		(*indices)[7 * 3 + 1] = 1;
		(*indices)[7 * 3 + 2] = 6;
		// front
		(*indices)[8 * 3 + 0] = 3;
		(*indices)[8 * 3 + 1] = 2;
		(*indices)[8 * 3 + 2] = 6;
		(*indices)[9 * 3 + 0] = 7;
		(*indices)[9 * 3 + 1] = 3;
		(*indices)[9 * 3 + 2] = 6;
		// top
		(*indices)[10 * 3 + 0] = 4;
		(*indices)[10 * 3 + 1] = 7;
		(*indices)[10 * 3 + 2] = 6;
		(*indices)[11 * 3 + 0] = 5;
		(*indices)[11 * 3 + 1] = 4;
		(*indices)[11 * 3 + 2] = 6;
	}

}
