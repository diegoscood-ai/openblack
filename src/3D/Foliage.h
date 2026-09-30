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
#include <unordered_map>
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
/// Modules of the mod (Mods/<name>/ with "module_of = world.foliage" in its mod.cfg) add their own foliage.cfg, read
/// with the same rules, e.g. things lying on the beach or butterflies over the flowers.
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
		std::vector<uint8_t> zones;     ///< only in cells with these ambient sound zones (Foliage::ZoneOf); empty: any
		std::vector<uint8_t> notZones;  ///< never in cells with these zones
		glm::vec2 waterDistance {0.0f, 8.0f};
		Tint tint {Tint::Grey};
		glm::vec2 groundValue {0.0f, 1.0f};      ///< brightness range of the ground colour under the plant (0..1)
		glm::vec2 groundSaturation {0.0f, 1.0f}; ///< saturation range of that colour
		bool cross {false};                      ///< two crossed planes instead of one
		bool flat {false};  ///< lying on the ground (shells, seaweed, tracks), blended by its alpha instead of upright
		bool coast {false}; ///< may also grow in the coast cells and next to water (above `altitude`)
		float lift {0.04f}; ///< flat: height over the ground
		float shade {1.0f}; ///< flat and tinted: scale of the ground colour it takes (below 1 darker)
		float share {0.0f}; ///< least share of the ground drawn at the point made of its textures (0..1)
	};

	/// Something flying around a kind of plant (a [flyer ...] section): butterflies over the flowers
	struct Flyer
	{
		std::string name;
		std::vector<uint16_t> animations; ///< indices into _animations (animated .gif images), one picked per flyer
		std::vector<std::string> over;    ///< species it lives on, by name (from any foliage.cfg)
		std::vector<uint16_t> overSpecies;
		float perPlant {0.02f};         ///< flyers per plant of those species
		glm::vec2 size {0.4f, 0.6f};    ///< width range
		glm::vec2 height {0.5f, 1.5f};  ///< flight height over its plant
		glm::vec2 range {2.0f, 6.0f};   ///< how far it wanders from its plant
		glm::vec2 flight {6.0f, 14.0f}; ///< seconds in the air, then
		glm::vec2 rest {2.0f, 6.0f};    ///< seconds sitting on its plant (slow wing beats)
		float speed {1.0f};             ///< flight and wing beat speed scale
		float flee {5.0f};              ///< flies away from the hand closer than this (0: never)
		bool night {false};             ///< also at night (otherwise they go one by one at dusk)
		float fold {0.7f};              ///< how much the wings fold along the body with the image's beats (0: flat)
	};

	/// One growth stage of the plants of a crop field (a [field_stage ...] section of foliage.cfg)
	struct FieldStage
	{
		std::string name;
		std::vector<uint16_t> layers;
		glm::vec2 growth {0.0f, 1200.0f}; ///< field growth (0..1200, Field::k_AgeRecolt = ripe) it covers
		glm::vec2 size {1.0f, 1.0f};      ///< width at the start and at the end of the stage
		glm::vec3 colourFrom {0.5f};      ///< tint (0..1) at the start and at the end: grey texels take it
		glm::vec3 colourTo {0.5f};
		float sway {1.0f};
		float lean {0.2f};
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

	/// The cell's ambient sound zone, painted by the map's designer (LNDCell::flags >> 1): 8 jungle, 10 wind, 14 birds
	/// (most land), 16 forest, 5 lake... The odd codes above 8 are variants of the even one below and count as it.
	[[nodiscard]] static uint8_t ZoneOf(uint8_t cellFlags);

	Foliage();
	~Foliage();

	/// Reads foliage.cfg and the images next to it, then the foliage.cfg of each module folder (its images next to it);
	/// false (and nothing drawn) if there is nothing to draw
	bool Load(const std::filesystem::path& directory, const std::vector<std::filesystem::path>& modules = {});
	[[nodiscard]] bool IsLoaded() const noexcept { return _texture != nullptr; }
	[[nodiscard]] const std::vector<Species>& GetSpecies() const noexcept { return _species; }
	[[nodiscard]] const std::vector<FieldStage>& GetFieldStages() const noexcept { return _fieldStages; }
	[[nodiscard]] const std::vector<Flyer>& GetFlyers() const noexcept { return _flyers; }

	/// Starts over for another island, density or once the scene's objects exist; then places the plants of the
	/// blocks that came within `distance` of the camera and frees the ones left far behind. With `fields`, the crop
	/// fields within the distance get their plants for their current growth and food.
	void Update(LandIslandInterface& island, float density, glm::vec3 cameraPosition, float distance, bool fields);
	/// Moves the flyers of the placed blocks within the distance ([flyer] sections; FoliageFlyers.cpp)
	void UpdateFlyers(LandIslandInterface& island, glm::vec3 cameraPosition, float distance, float seconds);

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

	/// Draws the crop fields' soil ([field] soil image) into the island's footprint texture (view already bound)
	void DrawFieldFootprints(bgfx::ViewId viewId, const graphics::ShaderProgram& program) const;

	/// Plants currently placed (for the log)
	[[nodiscard]] size_t GetPlantCount() const noexcept { return _plantCount; }
	/// Flyers drawn this frame
	[[nodiscard]] size_t GetFlyerCount() const noexcept { return _flyerInstances.size(); }

private:
	/// Per-plant instance data, 5 x vec4 (i_data0..4)
	struct Instance
	{
		glm::vec4 positionWidth;       ///< base x, y, z; width
		glm::vec4 heightLayerLightYaw; ///< height; texture layer; land luminosity 0..1; yaw
		glm::vec4 textureGround;       ///< v of the image's top; sway; ground material; tint mode
		glm::vec4 groundUvLeanPhase;   ///< ground texture uv (one block = 0..1); lean; sway phase
		glm::vec4 groundEnds;          ///< ground height at the plane's left and right ends, relative to the base;
		                               ///< colour source; colour. Flat: the ground's slope across and along, 2, blended
	};

	/// Where a flyer lives: on one plant of the species it is over
	struct FlyerHome
	{
		glm::vec3 top; ///< the plant's top
		float luminosity;
		uint16_t flyer;     ///< index into _flyers
		uint16_t animation; ///< index into _animations
		float width;
		float seed;          ///< 0..1
		float fleeTime {0.0f};       ///< seconds left darting away from the hand (like the fish from a splash)
		glm::vec2 away {1.0f, 0.0f}; ///< the way it darts: away from the hand when it came close
		glm::vec3 offset {0.0f};     ///< how far it is from its path: grows while it flees, then it flies back
	};

	/// The frames of an animated image: consecutive texture layers
	struct Animation
	{
		uint16_t first;
		std::vector<float> ends; ///< seconds at the end of each frame, from the image's frame delays
		/// folding flyers, per frame: the fold (radians) that would make the widest frame look as wide as this one
		/// (acos of its width over the widest): the wings' beat, in time with the image
		std::vector<float> folds;
	};

	/// The plants of one land block
	struct Chunk
	{
		glm::vec2 centre {0.0f};
		bool built {false};
		bgfx::VertexBufferHandle instances {BGFX_INVALID_HANDLE};
		uint32_t count {0};
		uint32_t crossStart {0}; ///< the instances from here on draw two crossed planes
		uint32_t flatStart {0};  ///< and from here on lie on the ground
		std::vector<FlyerHome> homes;
	};

	/// One plant of a crop field, placed once per field
	struct FieldPlant
	{
		glm::vec3 position;
		float luminosity;
		float yaw;
		float lean;
		float phase;
		float groundSlope; ///< height change per unit along the plane
		float stagger;     ///< -1..1: grows a little ahead of or behind the field
		float keep;        ///< 0..1: still there while the field keeps more than this share of its food
		float pickImage;
		float pickSize;
	};

	void Clear();
	void BuildChunk(LandIslandInterface& island, size_t blockIndex, float density);
	void UpdateFields(LandIslandInterface& island, glm::vec3 cameraPosition, float distance);
	/// The average colour of the landscape texture at a point (its cell's materials for the altitude)
	[[nodiscard]] static glm::vec3 GroundColourAt(LandIslandInterface& island, glm::vec2 point);

	std::vector<Species> _species;
	std::vector<Flyer> _flyers;
	std::vector<Animation> _animations;
	std::vector<std::vector<uint16_t>> _flyersOver; ///< per species: the flyers living on it
	std::vector<Instance> _flyerInstances;         ///< this frame's flyers
	float _flyerSeconds {0.0f};                    ///< the time of the last UpdateFlyers
	std::vector<FieldStage> _fieldStages;
	float _fieldSpacing {1.0f};  ///< [field] spacing: units between the plants of a field
	float _fieldStagger {60.0f}; ///< [field] stagger: growth units a plant may be ahead of or behind its field
	/// [field] ripening: the growth over which the far field mesh goes from the ground colour to its own
	glm::vec2 _fieldRipening {350.0f, 1200.0f};
	std::unordered_map<uint32_t, glm::vec3> _fieldGround; ///< per field entity: the average ground colour under it
	std::unordered_map<uint32_t, std::vector<FieldPlant>> _fieldPlants; ///< per field entity
	std::vector<Instance> _fieldInstances;                             ///< this frame's field plants
	std::vector<float> _layerTop;    ///< per layer: v of the image's top edge (images sit on the bottom of the layer)
	std::vector<float> _layerAspect; ///< per layer: image height / width
	std::unique_ptr<graphics::Texture2D> _texture;
	std::unique_ptr<graphics::Texture2D> _fieldSoil;    ///< [field] soil: drawn under the fields like a footprint
	float _fieldSoilMargin {0.1f};                      ///< [field] soil_margin: share of the field added on each side
	bgfx::VertexBufferHandle _soilQuad {BGFX_INVALID_HANDLE};
	bgfx::VertexBufferHandle _quad {BGFX_INVALID_HANDLE};
	bgfx::IndexBufferHandle _quadIndices {BGFX_INVALID_HANDLE};
	/// the flyers: a quad split down the middle (x -0.5, 0, 0.5), each half folding up about the centre line
	bgfx::VertexBufferHandle _foldQuad {BGFX_INVALID_HANDLE};
	bgfx::IndexBufferHandle _foldIndices {BGFX_INVALID_HANDLE};
	bgfx::VertexLayout _instanceLayout;

	std::vector<Chunk> _chunks; ///< one per land block
	std::unique_ptr<FoliageBlockedMap> _blocked;
	std::unique_ptr<FoliageWaterMap> _water;
	std::vector<uint8_t> _looks; ///< per island material: its Look
	size_t _plantCount {0};
	uint64_t _placementKey {0}; ///< what the chunks were placed for
};

} // namespace openblack
