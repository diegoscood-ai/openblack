/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <map>
#include <string_view>

#include "Windowing/WindowingInterface.h"

namespace openblack
{

enum class GraphicsBackend : uint8_t
{
	Noop,
	Direct3D12,
	Metal,
	Vulkan,
};

static const std::map<std::string_view, GraphicsBackend> k_GraphicsBackendStringLookup {
    std::pair {"Noop", GraphicsBackend::Noop},
    std::pair {"Direct3D12", GraphicsBackend::Direct3D12},
    std::pair {"Metal", GraphicsBackend::Metal},
    std::pair {"Vulkan", GraphicsBackend::Vulkan},
};

struct EngineConfig
{
	bool wireframe {false};
	bool showVillagerNames {false};
	bool debugVillagerNames {false};
	bool debugVillagerStates {false};

	bool viewDetailOverlay {false};
	bool drawSky {true};
	bool drawWater {true};
	bool drawIsland {true};
	bool drawEntities {true};
	bool drawSprites {true};
	bool drawBoundingBoxes {false};
	bool drawFootpaths {false};
	bool drawStreams {false};

	bool vsync {false};
	/// The original's graphics detail level 0..6 (Graphics/DetailLevel.h); 4 is the original's default
	uint8_t detailLevel {4};
	bool running {false};

	// Mods: changes to the original behaviour, all off by default
	/// Lower floating rocks and other mobile statics onto the landscape (StaticGrounding).
	bool groundStaticObjects {false};
	/// Multisample anti-aliasing of the backbuffer: 0 (off, as the original), 2, 4, 8 or 16 samples. With MSAA on, alpha
	/// cut-outs (leaves, fences) use alpha to coverage for smooth edges.
	uint8_t msaa {0};
	/// Mip levels and trilinear filtering for model, landscape and water textures (the original had no mip levels).
	bool textureMipmaps {false};
	/// Anisotropic filtering of those textures (implies textureMipmaps).
	bool anisotropicFiltering {false};
	/// Landscape material textures upscaled 2x (Lanczos-3) when the island loads (the original's are 256x256).
	bool terrainTexturesX2 {false};
	/// Landscape material textures repeated this many times per block (the original: once).
	float terrainTextureDensity {1.0f};
	/// Steep landscape faces take the materials from the side (triplanar) instead of stretching the top-down projection.
	bool terrainTriplanar {false};
	/// Grass, flowers and bushes over the landscape (3D/Foliage, rules in Mods/world.foliage/foliage.cfg): plants per
	/// cell multiplier, 0 = none, and the distance they are drawn to.
	float foliageDensity {0.0f};
	float foliageDistance {200.0f};
	/// Living water: the sea reflects models and sprites too (the original only mirrors the sky and the land) and the
	/// reflection ripples with moving waves in a loop (the original's reflection is static).
	bool livingWater {false};

	float timeOfDay {12.0f};
	float skyAlignment {0.0f};
	float bumpMapStrength {1.0f};
	float smallBumpMapStrength {1.0f};

	float cameraXFov {70.0f};
	float cameraNearClip {1.0f};
	float cameraFarClip {static_cast<float>(0x10000)};

	float guiScale {1.0f};

	GraphicsBackend graphicsBackend {GraphicsBackend::Noop};
	glm::u16vec2 resolution {256, 256};
	windowing::DisplayMode displayMode {windowing::DisplayMode::Windowed};

	uint32_t numFramesToSimulate {0};
};
} // namespace openblack
