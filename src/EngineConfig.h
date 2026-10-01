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
#include <vector>

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
	/// With the foliage: crop fields drawn as growing plants (foliage.cfg [field_stage] sections) instead of their mesh.
	bool foliageFields {false};
	/// Mod world.crops: fields sow themselves and are sown again once harvested (the original needs the town's farmers,
	/// and openblack has no villager jobs yet, so its fields would stay empty forever), and grow this much faster.
	bool fieldsWithoutFarmers {false};
	float fieldGrowthMultiplier {1.0f};
	/// Mod game.skip-intro: the answer given at each new game to the original's skip-tutorial requester (SkipBox, its
	/// callback 0x544480), which openblack does not draw: 0 = play everything (the box's default answer, SkipBox::Init
	/// 0x544206), 1 = skip the tutorial (bit 23 of g_game+0x14), 2 = also skip the creature training (bits 23 and 24),
	/// 3 = the box's fourth answer, keep the old creature (bits 23 to 25). For 3 the mod also answers the
	/// CURRENT_PROFILE_HAS_CREATURE that SetupLand1 ands with bit 25 (openblack has no player profiles), because that
	/// is the only answer whose script path leaves the opening to the player: CreaturesInGlade does not run.
	int skipTutorialChoice {0};
	/// Mod game.skip-intro, option "free start" (**not original**): the first script task that takes the camera at a
	/// new game is the land's opening; while it holds the camera the engine ignores what it does to the player (its
	/// camera, wide screen, fades and music) and answers its waits at once, so nothing moves the camera, locks the
	/// interface or hangs on a camera command openblack does not implement. Everything after that task is normal.
	bool skipIntroFreeStart {false};
	/// Mod test.miracle-dispensers (Worship/TestDispensers.h): a miracle dispenser of each player miracle around the
	/// human player's temple, at that power-up level (0 base, 1, 2, 3 = every level), making an orb every so many
	/// seconds once the last was taken, plus one empty dispenser that never makes one; with testDispensersSeed a fire
	/// seed is put into the human player's hand once they are placed.
	bool testDispensers {false};
	int testDispensersLevel {0};
	float testDispensersSeconds {10.0f};
	bool testDispensersSeed {true};
	/// Living water: the sea reflects models and sprites too (the original only mirrors the sky and the land) and the
	/// reflection ripples with moving waves in a loop (the original's reflection is static).
	bool livingWater {false};
	/// Mod graphics.hd-tweaks: the villagers' textures replaced by the HD images of Mods/graphics.hd-tweaks (4x the
	/// original's 256x256 atlases, Resources/HdTextures.h).
	bool hdTweaksTextures {false};
	/// Its smooth option: the villagers' meshes as curved PN triangles split into level^2 triangles (0 = off,
	/// 3D/PnTessellation.h). They are the boned meshes whose textures are all in hdTweaksSkins (the mod's list, made
	/// from the pack's own MSH_P_ meshes: openblack's mesh names don't follow every pack).
	int hdTweaksSmoothLevel {0};
	/// Mod graphics.hd-tweaks (light): 0 the original's vertex lighting, 1 the same light per pixel on the smooth normals.
	/// Applied by the shader every frame.
	int hdTweaksLighting {0};
	/// Mod graphics.hd-tweaks (sharp): texture mip bias of the villagers (negative: sharper far away)
	float hdTweaksMipBias {0.0f};
	/// Mod graphics.hd-tweaks (detail): villagers and animals with their high detail mesh instead of the std one, the
	/// only LOD the original draws (ECS/DetailMeshes.h)
	bool hdTweaksHighDetail {false};
	std::vector<uint32_t> hdTweaksSkins;

	float timeOfDay {12.0f};
	float skyAlignment {0.0f};
	float bumpMapStrength {1.0f};
	float smallBumpMapStrength {1.0f};

	float cameraXFov {70.0f};
	float cameraNearClip {1.0f};
	float cameraFarClip {static_cast<float>(0x10000)};

	float guiScale {1.0f};

	/// Music master volume 0..127: AudioMusicMasterVolume of the original's BWSetup registry key (GAudio fn_00428250 /
	/// fn_004282B0), applied with LHMusicSetMasterVolume 0x1000E890; 127 without the key ([0x10056280] = 0x7F at
	/// 0x1000DE08). Not saved yet (openblack has no settings file for it).
	uint32_t audioMusicMasterVolume {0x7F};
	/// Sample master volume 0..127 (every effect and voice): AudioSampleMasterVolume of BWSetup (fn_00428250 /
	/// fn_004282B0), applied with LHSampleSetMasterVolume 0x100150E0; 127 without the key (LH_AudioSystem+0x3C, ctor
	/// 0x10015290). Not saved yet (openblack has no settings file for it).
	uint32_t audioSampleMasterVolume {0x7F};

	GraphicsBackend graphicsBackend {GraphicsBackend::Noop};
	glm::u16vec2 resolution {256, 256};
	windowing::DisplayMode displayMode {windowing::DisplayMode::Windowed};

	uint32_t numFramesToSimulate {0};
};
} // namespace openblack
