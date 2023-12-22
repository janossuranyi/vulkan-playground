#pragma once

#include "renderer/ResourceDescriptions.h"
#include "world.h"

namespace jsr {

	bool gltfLoadWorld(std::filesystem::path filename, World& world);

}