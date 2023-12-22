#pragma once

#include "pch.h"
#include "vk.h"
#include "vk_pipeline.h"
#include "common/vertex.h"

namespace vks {

	struct Vertex : public common::Vertex {

		static VertexInputDescription Vertex::vertex_input_description();
		static VertexInputDescription Vertex::vertex_input_description_position_only();
	};

}