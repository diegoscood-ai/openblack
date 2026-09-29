/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <bgfx/bgfx.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace openblack
{
class LandIslandInterface;
class FoliageBlockedMap;
class FoliageWaterMap;

namespace graphics
{
class ShaderProgram;
class Texture2D;
} // namespace graphics

/// Mod world.foliage: grass, flowers and bushes over the landscape (the original has none).
/// Each plant kind has rules in <executable>/Mods/world.foliage/foliage.cfg: the landscape textures it grows on (by
/// look or LND material type), an altitude range, a slope range, how many per cell and how patchy. Grey texels of the
/// images take the colour of the ground texture under each plant. Plants are placed deterministically, one land block
/// at a time near the camera, and never under buildings, features or fields.
class Foliage
{
public:
	/// Which texels of an image take the ground colour
	enum class Tint : uint8_t
	{
		None, ///< the image's own colours
		Grey, ///< only the grey texels (monochrome blades; coloured petals keep their colour)
		All,  ///< the whole image, turned grey first
	};

	/// One kind of plant (a [section] of foliage.cfg)
	struct Species
	{
		std::string name;
		std::vector<uint16_t> layers;   ///< texture array layers, one picked at random per plant
		std::vector<uint16_t> terrains; ///< TerrainMaterialType values it grows on
		std::vector<uint8_t> looks;     ///< or texture looks it grows on (Foliage::Look), from the average colour
		float perCell {1.0f};           ///< plants per 10 x 10 cell at medium density
		glm::vec2 size {1.0f, 1.5f};    ///< width range, world units
		glm::vec2 altitude {0.0f, 1000.0f};
		glm::vec2 slope {0.0f, 90.0f}; ///< degrees
		float patches {0.0f};          ///< 0: even cover, 1: only in patches
		float sway {1.0f};             ///< wind sway scale
		float lean {0.3f};             ///< random lean up to this much (so the plane also shows from above)
		std::vector<uint8_t> nearWater; ///< only within waterDistance of these (Foliage::Water); empty: anywhere
		glm::vec2 waterDistance {0.0f, 8.0f};
		Tint tint {Tint::Grey};
	};

	/// Kinds of water a plant can be required to grow near
	enum class Water : uint8_t
	{
		Lake,   ///< water cells not connected to the open sea (the edge of the map)
		Stream, ///< the island's rivers (CREATE_STREAM / CREATE_STREAM_POINT)
		Sea,
	};

	/// What a landscape texture looks like, from its average colour
	enum class Look : uint8_t
	{
		Green, ///< grass
		Dry,   ///< brown earth, dry grass
		Sand,
		Rock,
		Snow,
	};
	[[nodiscard]] static Look ClassifyTexture(glm::vec3 colour);

	Foliage();
	~Foliage();

	/// Reads foliage.cfg and the images next to it; false (and nothing drawn) if they are missing
	bool Load(const std::filesystem::path& directory);
	[[nodiscard]] bool IsLoaded() const noexcept { return _texture != nullptr; }
	[[nodiscard]] const std::vector<Species>& GetSpecies() const noexcept { return _species; }

	/// Starts over for another island, density or once the scene's objects exist; then places the plants of the
	/// blocks that came within `distance` of the camera and frees the ones left far behind
	void Update(LandIslandInterface& island, float density, glm::vec3 cameraPosition, float distance);

	struct DrawDesc
	{
		bgfx::ViewId viewId;
		const graphics::ShaderProgram* program;
		glm::vec3 cameraPosition;
		float distance;
		bgfx::TextureHandle landLight;       ///< landscape light table, indexed by luminosity
		const graphics::Texture2D* materials; ///< the island's material array (the ground colour)
		float materialRepeats;                ///< material repeats per block (terrain-x2 mod)
		glm::vec4 haze;
		glm::vec4 hazeColour;
		bool alphaToCoverage;
		float seconds;
	};
	/// Draws the plants within the distance (alpha tested, in the opaque pass)
	void Draw(const DrawDesc& desc) const;

	/// Plants currently placed (for the log)
	[[nodiscard]] size_t GetPlantCount() const noexcept { return _plantCount; }

private:
	/// Per-plant instance data, 5 x vec4 (i_data0..4)
	struct Instance
	{
		glm::vec4 positionWidth;       ///< base x, y, z; width
		glm::vec4 heightLayerLightYaw; ///< height; texture layer; land luminosity 0..1; yaw
		glm::vec4 textureGround;       ///< v of the image's top; sway; ground material; tint mode
		glm::vec4 groundUvLeanPhase;   ///< ground texture uv (one block = 0..1); lean; sway phase
		glm::vec4 groundEnds;          ///< ground height at the plane's left and right ends, relative to the base
	};

	/// The plants of one land block
	struct Chunk
	{
		glm::vec2 centre {0.0f};
		bool built {false};
		bgfx::VertexBufferHandle instances {BGFX_INVALID_HANDLE};
		uint32_t count {0};
	};

	void Clear();
	void BuildChunk(LandIslandInterface& island, size_t blockIndex, float density);

	std::vector<Species> _species;
	std::vector<float> _layerTop;    ///< per layer: v of the image's top edge (images sit on the bottom of the layer)
	std::vector<float> _layerAspect; ///< per layer: image height / width
	std::unique_ptr<graphics::Texture2D> _texture;
	bgfx::VertexBufferHandle _quad {BGFX_INVALID_HANDLE};
	bgfx::IndexBufferHandle _quadIndices {BGFX_INVALID_HANDLE};
	bgfx::VertexLayout _instanceLayout;

	std::vector<Chunk> _chunks; ///< one per land block
	std::unique_ptr<FoliageBlockedMap> _blocked;
	std::unique_ptr<FoliageWaterMap> _water;
	std::vector<uint8_t> _looks; ///< per island material: its Look
	size_t _plantCount {0};
	uint64_t _placementKey {0}; ///< what the chunks were placed for
};

} // namespace openblack
