/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Foliage.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <tuple>
#include <unordered_map>

#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <glm/gtx/transform.hpp>
#include <spdlog/spdlog.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_GIF
#include <stb_image.h>

#include "3D/L3DMesh.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/MeshTint.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;

namespace
{
constexpr uint16_t k_LayerWidth = 256;
constexpr uint16_t k_LayerHeight = 512;
constexpr float k_BlockSize = 160.0f;
constexpr float k_CellSize = 10.0f;
constexpr float k_BlockedGrid = 5.0f; ///< resolution of the no-plants map around buildings
/// No plants closer than this to a river's line: its water channel (river.l3d alpha < 15) is about 4 units wide and
/// wanders a little inside the footprint
constexpr float k_RiverClearance = 3.0f;

/// TerrainMaterialType names (Enums.h), the values foliage.cfg's "terrain" lists use
constexpr std::array<std::string_view, 32> k_TerrainNames = {
    "None",       "DeepWater",  "ShallowWater", "RiverMud",   "WetEarth",   "Earth",
    "HardEarth",  "Sand",       "WetSand",      "Gravel",     "WetGravel",  "SmallRocks",
    "SmallRocksInShallowWater", "SmallLooseRocksOnSolidRock", "LargeRocks", "SolidRock",
    "WetSolidRock", "UnstableSolidRock", "Grass", "WetGrass", "DryGrass", "Rushes",
    "Ferns",      "Corn",       "Scrub",        "Heather",    "ForestUndergrowth", "Snow",
    "PackIce",    "SolidIce",   "SnowyMountainTops", "Pavement",
};

constexpr std::array<std::string_view, 5> k_LookNames = {"green", "dry", "sand", "rock", "snow"};

/// Ambient sound zones of the LND cells (LNDCell::flags >> 1, as BWLandEditor names them), the values foliage.cfg's
/// "zone" lists use; "meadow" is the birds zone, the one most of the land has
constexpr std::array<std::pair<std::string_view, uint8_t>, 15> k_ZoneNames = {{
    {"none", 0},
    {"splash", 2},
    {"ocean", 3},
    {"slow_waves", 4},
    {"lake", 5},
    {"coast", 6},
    {"fast_waves", 7},
    {"jungle", 8},
    {"wind", 10},
    {"desert", 12},
    {"birds", 14},
    {"meadow", 14},
    {"forest", 16},
    {"river", 18},
    {"swamp", 4}, // the slow waves of the inland ponds
}};

std::string Trim(std::string_view text)
{
	const auto begin = text.find_first_not_of(" \t\r");
	if (begin == std::string_view::npos)
	{
		return {};
	}
	const auto end = text.find_last_not_of(" \t\r");
	return std::string(text.substr(begin, end - begin + 1));
}

std::vector<std::string> SplitList(std::string_view text)
{
	std::vector<std::string> items;
	std::stringstream stream {std::string(text)};
	std::string item;
	while (std::getline(stream, item, ','))
	{
		if (auto trimmed = Trim(item); !trimmed.empty())
		{
			items.push_back(std::move(trimmed));
		}
	}
	return items;
}

/// "a-b" or "a" (a range of one value)
glm::vec2 ParseRange(const std::string& text)
{
	const auto dash = text.find('-', 1);
	const float first = std::stof(text.substr(0, dash));
	const float second = dash == std::string::npos ? first : std::stof(text.substr(dash + 1));
	return {std::min(first, second), std::max(first, second)};
}

/// Deterministic random numbers per placement candidate
struct Random
{
	uint32_t state;

	explicit Random(uint32_t seed)
	    : state(seed * 747796405u + 2891336453u)
	{
	}

	float Next()
	{
		state = state * 747796405u + 2891336453u;
		uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
		word = (word >> 22u) ^ word;
		return static_cast<float>(word >> 8) / 16777216.0f;
	}
};

uint32_t Hash(uint32_t a, uint32_t b, uint32_t c)
{
	uint32_t h = a * 0x8DA6B343u ^ b * 0xD8163841u ^ c * 0xCB1AB31Fu;
	h ^= h >> 16;
	h *= 0x7FEB352Du;
	h ^= h >> 15;
	return h;
}

/// Smooth value noise 0..1 (for the patches)
float ValueNoise(glm::vec2 p, uint32_t seed)
{
	const glm::vec2 cell = glm::floor(p);
	const glm::vec2 f = p - cell;
	const glm::vec2 u = f * f * (3.0f - 2.0f * f);
	const auto corner = [&](int dx, int dz) {
		const auto x = static_cast<uint32_t>(static_cast<int>(cell.x) + dx);
		const auto z = static_cast<uint32_t>(static_cast<int>(cell.y) + dz);
		return static_cast<float>(Hash(x, z, seed) >> 8) / 16777216.0f;
	};
	const float a = corner(0, 0);
	const float b = corner(1, 0);
	const float c = corner(0, 1);
	const float d = corner(1, 1);
	return a + (b - a) * u.x + (c - a) * u.y + (a - b - c + d) * u.x * u.y;
}

/// The normal of the ground as it is drawn (GetDrawnHeightAt: the landscape mesh flattens every altitude of 3 or
/// less to 0, GetHeightAt and GetNormalAt only next to a base corner of 4 or less)
static glm::vec3 GroundNormal(const LandIslandInterface& island, glm::vec2 point)
{
	const float dx = island.GetDrawnHeightAt(point + glm::vec2(1.0f, 0.0f)) - island.GetDrawnHeightAt(point - glm::vec2(1.0f, 0.0f));
	const float dz = island.GetDrawnHeightAt(point + glm::vec2(0.0f, 1.0f)) - island.GetDrawnHeightAt(point - glm::vec2(0.0f, 1.0f));
	return glm::normalize(glm::vec3(-dx, 2.0f, -dz));
}

} // namespace

/// No-plants map: under buildings, features, fields, rocks, piles and the like (not under trees)
class openblack::FoliageBlockedMap
{
public:
	FoliageBlockedMap(glm::vec2 origin, glm::vec2 size)
	    : _origin(origin)
	    , _width(static_cast<int>(size.x / k_BlockedGrid) + 1)
	    , _height(static_cast<int>(size.y / k_BlockedGrid) + 1)
	    , _cells(static_cast<size_t>(_width) * _height, 0)
	{
	}

	void AddCircle(glm::vec2 centre, float radius)
	{
		const glm::ivec2 low = glm::floor((centre - radius - _origin) / k_BlockedGrid);
		const glm::ivec2 high = glm::floor((centre + radius - _origin) / k_BlockedGrid);
		for (int z = std::max(low.y, 0); z <= std::min(high.y, _height - 1); ++z)
		{
			for (int x = std::max(low.x, 0); x <= std::min(high.x, _width - 1); ++x)
			{
				const glm::vec2 point = _origin + (glm::vec2(x, z) + 0.5f) * k_BlockedGrid;
				if (glm::distance(point, centre) <= radius + k_BlockedGrid * 0.5f)
				{
					_cells[static_cast<size_t>(z) * _width + x] = 1;
				}
			}
		}
	}

	[[nodiscard]] bool IsBlocked(glm::vec2 point) const
	{
		const glm::ivec2 cell = glm::floor((point - _origin) / k_BlockedGrid);
		if (cell.x < 0 || cell.y < 0 || cell.x >= _width || cell.y >= _height)
		{
			return false;
		}
		return _cells[static_cast<size_t>(cell.y) * _width + cell.x] != 0;
	}

	[[nodiscard]] size_t CountBlocked() const { return static_cast<size_t>(std::ranges::count(_cells, 1)); }

private:
	glm::vec2 _origin;
	int _width;
	int _height;
	std::vector<uint8_t> _cells;
};

namespace
{
constexpr std::array<std::string_view, 3> k_WaterNames = {"lake", "stream", "sea"};
} // namespace

/// Distance from every point of the island to each kind of water (Foliage::Water), on a 5-unit grid
class openblack::FoliageWaterMap
{
public:
	explicit FoliageWaterMap(LandIslandInterface& island)
	{
		const auto extent = island.GetExtent();
		_origin = extent.minimum;
		_width = static_cast<int>((extent.maximum.x - extent.minimum.x) / k_BlockedGrid) + 1;
		_height = static_cast<int>((extent.maximum.y - extent.minimum.y) / k_BlockedGrid) + 1;
		for (auto& distances : _distance)
		{
			distances.assign(static_cast<size_t>(_width) * _height, k_Far);
		}

		// Water bodies: 4-connected water cells; the ones reaching the edge of the map (or a missing block) are the sea
		const int cells = island.GetCellsPerSide();
		std::vector<int8_t> kind(static_cast<size_t>(cells) * cells, -1); // -1 land, 0 lake, 2 sea, 3 unvisited
		// the water bit through the single source (MapCoords::IsWater 0x6035B0: a cell without a block is water too, so
		// the sea beyond the landscape blocks counts) plus the LND fullWater flag the mod has always added
		const auto isWater = [&island](int x, int z) {
			return ecs::sea_cells::IsWater(island, glm::ivec2(x, z)) ||
			       island.GetCell(glm::u16vec2(x, z)).properties.fullWater;
		};
		for (int x = 0; x < cells; ++x)
		{
			for (int z = 0; z < cells; ++z)
			{
				if (isWater(x, z))
				{
					kind[static_cast<size_t>(x) * cells + z] = 3;
				}
			}
		}
		std::vector<glm::ivec2> body;
		for (int x = 0; x < cells; ++x)
		{
			for (int z = 0; z < cells; ++z)
			{
				if (kind[static_cast<size_t>(x) * cells + z] != 3)
				{
					continue;
				}
				body.clear();
				body.emplace_back(x, z);
				kind[static_cast<size_t>(x) * cells + z] = 0;
				bool sea = false;
				for (size_t i = 0; i < body.size(); ++i)
				{
					const auto cell = body[i];
					sea = sea || cell.x == 0 || cell.y == 0 || cell.x == cells - 1 || cell.y == cells - 1;
					for (const auto step : {glm::ivec2(1, 0), glm::ivec2(-1, 0), glm::ivec2(0, 1), glm::ivec2(0, -1)})
					{
						const auto next = cell + step;
						if (next.x >= 0 && next.y >= 0 && next.x < cells && next.y < cells &&
						    kind[static_cast<size_t>(next.x) * cells + next.y] == 3)
						{
							kind[static_cast<size_t>(next.x) * cells + next.y] = 0;
							body.push_back(next);
						}
					}
				}
				const auto water = sea ? Foliage::Water::Sea : Foliage::Water::Lake;
				for (const auto cell : body)
				{
					kind[static_cast<size_t>(cell.x) * cells + cell.y] = static_cast<int8_t>(water);
					Seed(water, glm::vec2(cell) * k_CellSize);
				}
				_bodies[static_cast<size_t>(water)] += 1;
			}
		}

		// Rivers: every segment between a stream point and the points it links to
		if (Locator::entitiesRegistry::has_value())
		{
			Locator::entitiesRegistry::value().Each<const ecs::components::Stream>(
			    [this](const ecs::components::Stream& stream) {
				    for (size_t p = 0; p < stream.points.size(); ++p)
				    {
					    const glm::vec2 from(stream.points[p].x, stream.points[p].z);
					    Seed(Foliage::Water::Stream, from);
					    if (p + 1 < stream.points.size())
					    {
						    const glm::vec2 to(stream.points[p + 1].x, stream.points[p + 1].z);
						    AddSegment(from, to);
						    const int steps = static_cast<int>(glm::distance(from, to) / (k_BlockedGrid * 0.5f)) + 1;
						    for (int i = 0; i <= steps; ++i)
						    {
							    Seed(Foliage::Water::Stream, glm::mix(from, to, static_cast<float>(i) / static_cast<float>(steps)));
						    }
					    }
					    _bodies[static_cast<size_t>(Foliage::Water::Stream)] += 1;
				    }
			    });
		}

		for (auto& distances : _distance)
		{
			Propagate(distances);
		}
	}

	/// Distance in world units from the point to the nearest water of that kind (large if there is none); exact to
	/// the river segments up to k_SegmentCell units, from the 5-unit distance map beyond
	[[nodiscard]] float DistanceTo(Foliage::Water water, glm::vec2 point) const
	{
		if (water == Foliage::Water::Stream)
		{
			const glm::ivec2 bucket = glm::floor(point / k_SegmentCell);
			float nearest = 1e9f;
			for (int dz = -1; dz <= 1; ++dz)
			{
				for (int dx = -1; dx <= 1; ++dx)
				{
					const auto found = _segments.find(Key(bucket + glm::ivec2(dx, dz)));
					if (found == _segments.end())
					{
						continue;
					}
					for (const auto& [a, b] : found->second)
					{
						const glm::vec2 ab = b - a;
						const float t = std::clamp(glm::dot(point - a, ab) / std::max(glm::dot(ab, ab), 1e-6f), 0.0f, 1.0f);
						nearest = std::min(nearest, glm::distance(point, a + ab * t));
					}
				}
			}
			if (nearest <= k_SegmentCell)
			{
				return nearest;
			}
		}
		const glm::ivec2 cell = glm::floor((point - _origin) / k_BlockedGrid + 0.5f);
		if (cell.x < 0 || cell.y < 0 || cell.x >= _width || cell.y >= _height)
		{
			return 1e9f;
		}
		const auto value = _distance.at(static_cast<size_t>(water))[static_cast<size_t>(cell.y) * _width + cell.x];
		return static_cast<float>(value) * k_BlockedGrid / 3.0f;
	}

	[[nodiscard]] std::string Describe() const
	{
		return fmt::format("{} lake cells groups, {} sea, {} stream points", _bodies[0], _bodies[2], _bodies[1]);
	}

private:
	static constexpr uint16_t k_Far = 60000;
	static constexpr float k_SegmentCell = 20.0f;

	static uint64_t Key(glm::ivec2 bucket)
	{
		return (static_cast<uint64_t>(static_cast<uint32_t>(bucket.x)) << 32u) | static_cast<uint32_t>(bucket.y);
	}

	/// A river segment, in every 20-unit bucket its bounding box touches
	void AddSegment(glm::vec2 from, glm::vec2 to)
	{
		const glm::ivec2 low = glm::floor(glm::min(from, to) / k_SegmentCell);
		const glm::ivec2 high = glm::floor(glm::max(from, to) / k_SegmentCell);
		for (int z = low.y; z <= high.y; ++z)
		{
			for (int x = low.x; x <= high.x; ++x)
			{
				_segments[Key({x, z})].emplace_back(from, to);
			}
		}
	}

	std::unordered_map<uint64_t, std::vector<std::pair<glm::vec2, glm::vec2>>> _segments;

	/// A cell of the land (its 10 x 10 square) or a point on a river is water of that kind
	void Seed(Foliage::Water water, glm::vec2 corner)
	{
		auto& distances = _distance.at(static_cast<size_t>(water));
		const bool square = water != Foliage::Water::Stream;
		const glm::ivec2 low = glm::floor((corner - _origin) / k_BlockedGrid + (square ? 0.0f : 0.5f));
		const int size = square ? static_cast<int>(k_CellSize / k_BlockedGrid) : 1;
		for (int z = low.y; z < low.y + size; ++z)
		{
			for (int x = low.x; x < low.x + size; ++x)
			{
				if (x >= 0 && z >= 0 && x < _width && z < _height)
				{
					distances[static_cast<size_t>(z) * _width + x] = 0;
				}
			}
		}
	}

	/// Two-pass 3-4 chamfer distance (in thirds of a grid step)
	void Propagate(std::vector<uint16_t>& d) const
	{
		const auto at = [&](int x, int z) -> uint16_t& { return d[static_cast<size_t>(z) * _width + x]; };
		const auto relax = [&](int x, int z, int nx, int nz, int cost) {
			if (nx >= 0 && nz >= 0 && nx < _width && nz < _height)
			{
				at(x, z) = static_cast<uint16_t>(std::min<int>(at(x, z), at(nx, nz) + cost));
			}
		};
		for (int z = 0; z < _height; ++z)
		{
			for (int x = 0; x < _width; ++x)
			{
				relax(x, z, x - 1, z, 3);
				relax(x, z, x, z - 1, 3);
				relax(x, z, x - 1, z - 1, 4);
				relax(x, z, x + 1, z - 1, 4);
			}
		}
		for (int z = _height - 1; z >= 0; --z)
		{
			for (int x = _width - 1; x >= 0; --x)
			{
				relax(x, z, x + 1, z, 3);
				relax(x, z, x, z + 1, 3);
				relax(x, z, x + 1, z + 1, 4);
				relax(x, z, x - 1, z + 1, 4);
			}
		}
	}

	glm::vec2 _origin {0.0f};
	int _width {0};
	int _height {0};
	std::array<std::vector<uint16_t>, 3> _distance;
	std::array<int, 3> _bodies {};
};

namespace
{
std::unique_ptr<FoliageBlockedMap> BuildBlockedMap(const LandIslandInterface& island)
{
	const auto extent = island.GetExtent();
	auto map = std::make_unique<FoliageBlockedMap>(extent.minimum, extent.maximum - extent.minimum);
	auto& blocked = *map;
	if (!Locator::entitiesRegistry::has_value())
	{
		return map;
	}
	using namespace ecs::components;
	const auto& registry = Locator::entitiesRegistry::value();
	constexpr float k_Margin = 1.0f;
	registry.Each<const Fixed>([&registry, &blocked](entt::entity entity, const Fixed& fixed) {
		if (!registry.AnyOf<Tree, BigForest, Forest>(entity))
		{
			blocked.AddCircle(fixed.boundingCenter, fixed.boundingRadius + k_Margin);
		}
	});
	const auto& meshes = Locator::resources::value().GetMeshes();
	registry.Each<const Mesh, const Transform>(
	    [&registry, &blocked, &meshes](entt::entity entity, const Mesh& mesh, const Transform& transform) {
		    if (registry.AnyOf<Fixed>(entity) ||
		        !registry.AnyOf<Field, MobileStatic, Pot, StoragePit, Temple, TempleInteriorPart, FishFarm>(entity))
		    {
			    return;
		    }
		    if (!meshes.Contains(mesh.id))
		    {
			    return;
		    }
		    const auto box = meshes.Handle(mesh.id)->GetBoundingBox();
		    const auto size = box.Size() * transform.scale;
		    const float radius = 0.5f * std::max(size.x, size.z);
		    const glm::vec3 centre = transform.position + transform.rotation * (box.Center() * transform.scale);
		    blocked.AddCircle({centre.x, centre.z}, radius + k_Margin);
	    });
	return map;
}
} // namespace

Foliage::Look Foliage::ClassifyTexture(glm::vec3 colour)
{
	// hue / saturation / value of the average colour; measured on Land1-5: grass hue 58-70 with saturation >= 0.55,
	// rock the same hues but saturation 0.29-0.45, sand value >= 0.6, snow and ice saturation < 0.1
	const float high = std::max({colour.r, colour.g, colour.b});
	const float low = std::min({colour.r, colour.g, colour.b});
	const float saturation = high > 0.0f ? (high - low) / high : 0.0f;
	float hue = 0.0f;
	if (high > low)
	{
		if (high == colour.r)
		{
			hue = 60.0f * std::fmod((colour.g - colour.b) / (high - low), 6.0f);
		}
		else if (high == colour.g)
		{
			hue = 60.0f * ((colour.b - colour.r) / (high - low) + 2.0f);
		}
		else
		{
			hue = 60.0f * ((colour.r - colour.g) / (high - low) + 4.0f);
		}
	}
	if (saturation < 0.15f && high > 0.55f)
	{
		return Look::Snow;
	}
	if (hue >= 50.0f && hue <= 100.0f && saturation >= 0.55f)
	{
		return Look::Green;
	}
	if (high >= 0.6f)
	{
		return Look::Sand;
	}
	if (hue < 50.0f && saturation >= 0.5f)
	{
		return Look::Dry;
	}
	return Look::Rock;
}

Foliage::Foliage()
{
	_instanceLayout.begin()
	    .add(bgfx::Attrib::TexCoord7, 4, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord6, 4, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord5, 4, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord4, 4, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord3, 4, bgfx::AttribType::Float)
	    .end();
}

Foliage::~Foliage()
{
	Clear();
	if (bgfx::isValid(_quad))
	{
		bgfx::destroy(_quad);
	}
	if (bgfx::isValid(_soilQuad))
	{
		bgfx::destroy(_soilQuad);
	}
	if (bgfx::isValid(_quadIndices))
	{
		bgfx::destroy(_quadIndices);
	}
	if (bgfx::isValid(_foldQuad))
	{
		bgfx::destroy(_foldQuad);
	}
	if (bgfx::isValid(_foldIndices))
	{
		bgfx::destroy(_foldIndices);
	}
}

void Foliage::Clear()
{
	for (auto& chunk : _chunks)
	{
		if (bgfx::isValid(chunk.instances))
		{
			bgfx::destroy(chunk.instances);
		}
	}
	_chunks.clear();
	_blocked.reset();
	_water.reset();
	_looks.clear();
	_plantCount = 0;
	_fieldPlants.clear();
	_fieldInstances.clear();
	_fieldGround.clear();
	_flyerInstances.clear();
}

glm::vec3 Foliage::GroundColourAt(LandIslandInterface& island, glm::vec2 point)
{
	// the cell's material pair for its altitude (as the terrain shader and BuildChunk pick it), mixed by the
	// coefficient
	const auto& materials = island.GetMaterialInfo();
	const auto& countries = island.GetCountries();
	const int last = island.GetCellsPerSide() - 1;
	const auto cellCoordinates = glm::clamp(glm::ivec2(glm::floor(point / k_CellSize)), 0, last);
	const auto& cell = island.GetCell(glm::u16vec2(cellCoordinates));
	const auto& country = countries.at(cell.properties.country);
	const auto altitude = island.GetCellAltitude(cell);
	const auto& mapMaterial =
	    altitude > 255 ? country.materials.back()
	                   : country.materials.at((altitude + island.GetNoise(glm::u8vec2(cellCoordinates))) % country.materials.size());
	const auto colourOf = [&materials](uint32_t index) {
		return index < materials.size() ? materials[index].colour : glm::vec3(0.5f);
	};
	return glm::mix(colourOf(mapMaterial.indices[0]), colourOf(mapMaterial.indices[1]),
	                static_cast<float>(mapMaterial.coefficient) / 255.0f);
}

uint8_t Foliage::ZoneOf(uint8_t cellFlags)
{
	const auto zone = static_cast<uint8_t>(cellFlags >> 1u);
	return zone > 8 && (zone & 1u) != 0 ? static_cast<uint8_t>(zone - 1) : zone;
}

bool Foliage::Load(const std::filesystem::path& directory, const std::vector<std::filesystem::path>& modules,
                   const std::vector<float>& moduleDensities)
{
	if (!std::filesystem::exists(directory / "foliage.cfg"))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Foliage: no {}", (directory / "foliage.cfg").string());
		return false;
	}

	std::vector<std::vector<uint8_t>> layers;
	const auto addLayer = [&](const uint8_t* pixels, int width, int height) {
		// Scaled to the layer width (bilinear), sitting on the bottom of the layer. The colour of the transparent
		// texels is the average of the opaque ones, so the mip levels don't get dark fringes.
		const float scale = static_cast<float>(k_LayerWidth) / static_cast<float>(width);
		const int scaledHeight = std::min(static_cast<int>(std::lround(static_cast<float>(height) * scale)),
		                                  static_cast<int>(k_LayerHeight));
		std::vector<uint8_t> layer(static_cast<size_t>(k_LayerWidth) * k_LayerHeight * 4, 0);
		const auto source = [&](int x, int y, int c) {
			x = std::clamp(x, 0, width - 1);
			y = std::clamp(y, 0, height - 1);
			return static_cast<float>(pixels[(static_cast<size_t>(y) * width + x) * 4 + c]);
		};
		std::array<double, 3> sum {};
		double sumWeight = 0.0;
		for (int y = 0; y < scaledHeight; ++y)
		{
			const float sy = (static_cast<float>(y) + 0.5f) / scale - 0.5f;
			const int y0 = static_cast<int>(std::floor(sy));
			const float fy = sy - static_cast<float>(y0);
			auto* row = &layer[(static_cast<size_t>(k_LayerHeight - scaledHeight + y) * k_LayerWidth) * 4];
			for (int x = 0; x < k_LayerWidth; ++x)
			{
				const float sx = (static_cast<float>(x) + 0.5f) / scale - 0.5f;
				const int x0 = static_cast<int>(std::floor(sx));
				const float fx = sx - static_cast<float>(x0);
				// alpha-weighted so transparent texels don't bleed their colour into the edges
				std::array<float, 4> texel {};
				const std::array<std::array<float, 3>, 4> taps = {{{0, 0, (1 - fx) * (1 - fy)},
				                                                    {1, 0, fx * (1 - fy)},
				                                                    {0, 1, (1 - fx) * fy},
				                                                    {1, 1, fx * fy}}};
				for (const auto& tap : taps)
				{
					const int tx = x0 + static_cast<int>(tap[0]);
					const int ty = y0 + static_cast<int>(tap[1]);
					const float a = source(tx, ty, 3) * tap[2];
					for (int c = 0; c < 3; ++c)
					{
						texel[c] += source(tx, ty, c) * a;
					}
					texel[3] += a;
				}
				auto* out = &row[static_cast<size_t>(x) * 4];
				if (texel[3] > 0.0f)
				{
					for (int c = 0; c < 3; ++c)
					{
						out[c] = static_cast<uint8_t>(std::clamp(texel[c] / texel[3], 0.0f, 255.0f));
						sum[c] += out[c] * texel[3];
					}
					sumWeight += texel[3];
				}
				out[3] = static_cast<uint8_t>(std::clamp(texel[3], 0.0f, 255.0f));
			}
		}
		if (sumWeight > 0.0)
		{
			for (size_t i = 0; i < layer.size(); i += 4)
			{
				if (layer[i + 3] == 0)
				{
					for (int c = 0; c < 3; ++c)
					{
						layer[i + c] = static_cast<uint8_t>(sum[c] / sumWeight);
					}
				}
			}
		}
		layers.push_back(std::move(layer));
		_layerTop.push_back(1.0f - static_cast<float>(scaledHeight) / k_LayerHeight);
		_layerAspect.push_back(static_cast<float>(height) / static_cast<float>(width));
	};
	// the images by path: a .png is one layer, an animated .gif one layer per frame (an entry of _animations)
	std::vector<std::pair<std::string, int>> layerFiles;
	std::vector<std::pair<std::string, int>> animationFiles;
	const auto animationOf = [&](const std::filesystem::path& path) -> int {
		const auto key = path.generic_string();
		if (const auto found = std::ranges::find(animationFiles, key, &std::pair<std::string, int>::first);
		    found != animationFiles.end())
		{
			return found->second;
		}
		std::ifstream stream(path, std::ios::binary);
		const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
		int* delays = nullptr;
		int width = 0;
		int height = 0;
		int frames = 0;
		int channels = 0;
		auto* pixels = bytes.empty() ? nullptr
		                             : stbi_load_gif_from_memory(bytes.data(), static_cast<int>(bytes.size()), &delays, &width,
		                                                         &height, &frames, &channels, 4);
		if (pixels == nullptr || frames <= 0)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Foliage: cannot read {}", path.string());
			animationFiles.emplace_back(key, -1);
			return -1;
		}
		Animation animation {static_cast<uint16_t>(layers.size()), {}};
		float time = 0.0f;
		// each frame's half width: the furthest opaque pixel from the middle column (the body)
		std::vector<float> halfWidths;
		for (int frame = 0; frame < frames; ++frame)
		{
			const auto* image = pixels + static_cast<size_t>(frame) * width * height * 4;
			float halfWidth = 0.0f;
			for (int y = 0; y < height; ++y)
			{
				for (int x = 0; x < width; ++x)
				{
					if (image[(static_cast<size_t>(y) * width + x) * 4 + 3] > 128)
					{
						halfWidth = std::max(halfWidth, std::abs(static_cast<float>(x) + 0.5f - 0.5f * static_cast<float>(width)));
					}
				}
			}
			halfWidths.push_back(halfWidth);
			addLayer(image, width, height);
			// browsers draw delays under 20 ms as 100 ms
			const int delay = delays != nullptr && delays[frame] >= 20 ? delays[frame] : 100;
			time += static_cast<float>(delay) / 1000.0f;
			animation.ends.push_back(time);
		}
		stbi_image_free(pixels);
		STBI_FREE(delays);
		const auto widest = std::ranges::max_element(halfWidths);
		for (const float halfWidth : halfWidths)
		{
			animation.folds.push_back(*widest > 0.0f ? std::acos(std::clamp(halfWidth / *widest, 0.0f, 1.0f)) : 0.0f);
		}
		_animations.push_back(std::move(animation));
		animationFiles.emplace_back(key, static_cast<int>(_animations.size() - 1));
		return animationFiles.back().second;
	};
	const auto layerOf = [&](const std::filesystem::path& path) -> int {
		if (path.extension() == ".gif")
		{
			const int animation = animationOf(path); // a plant shows the first frame
			return animation < 0 ? -1 : _animations[static_cast<size_t>(animation)].first;
		}
		const auto key = path.generic_string();
		if (const auto found = std::ranges::find(layerFiles, key, &std::pair<std::string, int>::first); found != layerFiles.end())
		{
			return found->second;
		}
		int width = 0;
		int height = 0;
		int channels = 0;
		auto* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
		if (pixels == nullptr)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Foliage: cannot read {}", path.string());
			layerFiles.emplace_back(key, -1);
			return -1;
		}
		addLayer(pixels, width, height);
		stbi_image_free(pixels);
		layerFiles.emplace_back(key, static_cast<int>(layers.size() - 1));
		return layerFiles.back().second;
	};

	_species.clear();
	_fieldStages.clear();
	_flyers.clear();
	_animations.clear();
	_layerTop.clear();
	_layerAspect.clear();
	// what the key = value lines belong to: a plant kind, the [field] settings, a [field_stage ...] or a [flyer ...]
	enum class Section : uint8_t
	{
		None,
		Species,
		Field,
		FieldStage,
		Flyer,
	};
	const auto parseColour = [](const std::string& text) {
		const auto parts = SplitList(text);
		if (parts.size() != 3)
		{
			throw std::invalid_argument("colour");
		}
		return glm::vec3(std::stof(parts[0]), std::stof(parts[1]), std::stof(parts[2])) / 255.0f;
	};
	// the mod's own foliage.cfg, then each module's, with its images next to it
	std::vector<std::filesystem::path> sources = {directory};
	sources.insert(sources.end(), modules.begin(), modules.end());
	for (size_t sourceIndex = 0; sourceIndex < sources.size(); ++sourceIndex)
	{
	const auto& source = sources[sourceIndex];
	// a module's own density (its option in the Mods menu) on top of the mod's
	const float sourceDensity = sourceIndex > 0 && sourceIndex - 1 < moduleDensities.size() ? moduleDensities[sourceIndex - 1] : 1.0f;
	std::ifstream file(source / "foliage.cfg");
	if (!file)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Foliage: no {}", (source / "foliage.cfg").string());
		continue;
	}
	std::string line;
	int lineNumber = 0;
	auto section = Section::None;
	while (std::getline(file, line))
	{
		++lineNumber;
		if (const auto comment = line.find_first_of("#;"); comment != std::string::npos)
		{
			line.resize(comment);
		}
		line = Trim(line);
		if (line.empty())
		{
			continue;
		}
		if (line.front() == '[' && line.back() == ']')
		{
			const auto name = Trim(std::string_view(line).substr(1, line.size() - 2));
			if (name == "field")
			{
				section = Section::Field;
			}
			else if (name.starts_with("field_stage"))
			{
				section = Section::FieldStage;
				_fieldStages.push_back({});
				_fieldStages.back().name = Trim(std::string_view(name).substr(11));
			}
			else if (name.starts_with("flyer"))
			{
				section = Section::Flyer;
				_flyers.push_back({});
				_flyers.back().name = Trim(std::string_view(name).substr(5));
			}
			else
			{
				section = Section::Species;
				_species.push_back({});
				_species.back().name = name;
			}
			continue;
		}
		const auto equals = line.find('=');
		if (equals == std::string::npos || section == Section::None)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Foliage: foliage.cfg line {} ignored", lineNumber);
			continue;
		}
		const auto key = Trim(std::string_view(line).substr(0, equals));
		const auto value = Trim(std::string_view(line).substr(equals + 1));
		if (section == Section::Field)
		{
			try
			{
				if (key == "spacing")
				{
					_fieldSpacing = std::max(std::stof(value), 0.2f);
				}
				else if (key == "stagger")
				{
					_fieldStagger = std::max(std::stof(value), 0.0f);
				}
				else if (key == "soil")
				{
					int width = 0;
					int height = 0;
					int channels = 0;
					auto* pixels = stbi_load((source / value).string().c_str(), &width, &height, &channels, 4);
					if (pixels == nullptr)
					{
						SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Foliage: cannot read {}", value);
					}
					else
					{
						_fieldSoil = std::make_unique<graphics::Texture2D>("FieldSoil");
						_fieldSoil->Create(static_cast<uint16_t>(width), static_cast<uint16_t>(height), 1,
						                   graphics::TextureFormat::RGBA8, graphics::Wrapping::ClampEdge, graphics::Filter::Linear,
						                   bgfx::copy(pixels, static_cast<uint32_t>(width * height * 4)));
						stbi_image_free(pixels);
					}
				}
				else if (key == "ripening")
				{
					_fieldRipening = ParseRange(value);
				}
				else if (key == "soil_margin")
				{
					_fieldSoilMargin = std::clamp(std::stof(value), 0.0f, 1.0f);
				}
				else
				{
					SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Foliage: unknown key {} (line {})", key, lineNumber);
				}
			}
			catch (const std::exception&)
			{
				SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Foliage: bad value {} (line {})", value, lineNumber);
			}
			continue;
		}
		if (section == Section::FieldStage)
		{
			auto& stage = _fieldStages.back();
			try
			{
				if (key == "images")
				{
					for (const auto& image : SplitList(value))
					{
						if (const int layer = layerOf(source / image); layer >= 0)
						{
							stage.layers.push_back(static_cast<uint16_t>(layer));
						}
					}
				}
				else if (key == "growth")
				{
					stage.growth = ParseRange(value);
				}
				else if (key == "size")
				{
					// start-end, in this order (not sorted like the other ranges: a stage may shrink)
					const auto dash = value.find('-', 1);
					stage.size.x = std::stof(value.substr(0, dash));
					stage.size.y = dash == std::string::npos ? stage.size.x : std::stof(value.substr(dash + 1));
				}
				else if (key == "colour")
				{
					const auto dash = value.find('-');
					stage.colourFrom = parseColour(value.substr(0, dash));
					stage.colourTo = dash == std::string::npos ? stage.colourFrom : parseColour(value.substr(dash + 1));
				}
				else if (key == "sway")
				{
					stage.sway = std::stof(value);
				}
				else if (key == "lean")
				{
					stage.lean = std::stof(value);
				}
				else
				{
					SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Foliage: unknown key {} (line {})", key, lineNumber);
				}
			}
			catch (const std::exception&)
			{
				SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Foliage: bad value {} (line {})", value, lineNumber);
			}
			continue;
		}
		if (section == Section::Flyer)
		{
			auto& flyer = _flyers.back();
			try
			{
				if (key == "images")
				{
					for (const auto& image : SplitList(value))
					{
						if (const int animation = animationOf(source / image); animation >= 0)
						{
							flyer.animations.push_back(static_cast<uint16_t>(animation));
						}
					}
				}
				else if (key == "over")
				{
					flyer.over = SplitList(value);
				}
				else if (key == "per_plant")
				{
					flyer.perPlant = std::max(std::stof(value), 0.0f);
				}
				else if (key == "size")
				{
					flyer.size = ParseRange(value);
				}
				else if (key == "height")
				{
					flyer.height = ParseRange(value);
				}
				else if (key == "range")
				{
					flyer.range = ParseRange(value);
				}
				else if (key == "flight")
				{
					flyer.flight = glm::max(ParseRange(value), 0.5f);
				}
				else if (key == "rest")
				{
					flyer.rest = glm::max(ParseRange(value), 0.0f);
				}
				else if (key == "speed")
				{
					flyer.speed = std::max(std::stof(value), 0.01f);
				}
				else if (key == "flee")
				{
					flyer.flee = std::max(std::stof(value), 0.0f);
				}
				else if (key == "fold")
				{
					flyer.fold = value == "on" || value == "yes" || value == "true" ? 1.0f
					             : value == "off" || value == "no" || value == "false"
					                 ? 0.0f
					                 : std::clamp(std::stof(value), 0.0f, 1.5f);
				}
				else if (key == "night")
				{
					flyer.night = value == "on" || value == "yes" || value == "true" || value == "1";
				}
				else
				{
					SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Foliage: unknown key {} (line {})", key, lineNumber);
				}
			}
			catch (const std::exception&)
			{
				SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Foliage: bad value {} (line {})", value, lineNumber);
			}
			continue;
		}
		auto& species = _species.back();
		try
		{
			if (key == "images")
			{
				for (const auto& image : SplitList(value))
				{
					if (const int layer = layerOf(source / image); layer >= 0)
					{
						species.layers.push_back(static_cast<uint16_t>(layer));
					}
				}
			}
			else if (key == "terrain")
			{
				for (const auto& name : SplitList(value))
				{
					const auto found = std::ranges::find(k_TerrainNames, name);
					if (found != k_TerrainNames.end())
					{
						species.terrains.push_back(static_cast<uint16_t>(found - k_TerrainNames.begin()));
					}
					else if (std::isdigit(static_cast<unsigned char>(name.front())) != 0)
					{
						species.terrains.push_back(static_cast<uint16_t>(std::stoi(name)));
					}
					else
					{
						SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Foliage: unknown terrain {} (line {})", name,
						                   lineNumber);
					}
				}
			}
			else if (key == "texture")
			{
				for (const auto& name : SplitList(value))
				{
					const auto found = std::ranges::find(k_LookNames, name);
					if (found != k_LookNames.end())
					{
						species.looks.push_back(static_cast<uint8_t>(found - k_LookNames.begin()));
					}
					else
					{
						SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Foliage: unknown texture look {} (line {})", name,
						                   lineNumber);
					}
				}
			}
			else if (key == "per_cell")
			{
				species.perCell = std::stof(value) * sourceDensity;
			}
			else if (key == "size")
			{
				species.size = ParseRange(value);
			}
			else if (key == "altitude")
			{
				species.altitude = ParseRange(value);
			}
			else if (key == "slope")
			{
				species.slope = ParseRange(value);
			}
			else if (key == "patches")
			{
				species.patches = std::clamp(std::stof(value), 0.0f, 1.0f);
			}
			else if (key == "sway")
			{
				species.sway = std::stof(value);
			}
			else if (key == "near")
			{
				for (const auto& name : SplitList(value))
				{
					const auto found = std::ranges::find(k_WaterNames, name);
					if (found != k_WaterNames.end())
					{
						species.nearWater.push_back(static_cast<uint8_t>(found - k_WaterNames.begin()));
					}
					else
					{
						SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Foliage: unknown water {} (line {})", name, lineNumber);
					}
				}
			}
			else if (key == "zone" || key == "not_zone")
			{
				auto& list = key == "zone" ? species.zones : species.notZones;
				for (const auto& name : SplitList(value))
				{
					const auto found = std::ranges::find(k_ZoneNames, name, &std::pair<std::string_view, uint8_t>::first);
					if (found != k_ZoneNames.end())
					{
						list.push_back(found->second);
					}
					else if (std::isdigit(static_cast<unsigned char>(name.front())) != 0)
					{
						list.push_back(ZoneOf(static_cast<uint8_t>(std::stoi(name) << 1)));
					}
					else
					{
						SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Foliage: unknown zone {} (line {})", name, lineNumber);
					}
				}
			}
			else if (key == "water_distance")
			{
				species.waterDistance = ParseRange(value);
			}
			else if (key == "lean")
			{
				species.lean = std::stof(value);
			}
			else if (key == "tint")
			{
				species.tint = value == "none" ? Tint::None : value == "all" ? Tint::All : Tint::Grey;
			}
			else if (key == "ground_value")
			{
				species.groundValue = ParseRange(value);
			}
			else if (key == "ground_saturation")
			{
				species.groundSaturation = ParseRange(value);
			}
			else if (key == "cross" || key == "flat" || key == "coast")
			{
				(key == "cross" ? species.cross : key == "flat" ? species.flat : species.coast) =
				    value == "on" || value == "yes" || value == "true" || value == "1";
			}
			else if (key == "lift")
			{
				species.lift = std::stof(value);
			}
			else if (key == "opacity")
			{
				species.opacity = std::clamp(std::stof(value), 0.01f, 1.0f);
			}
			else if (key == "shade")
			{
				species.shade = std::max(std::stof(value), 0.0f);
			}
			else if (key == "share")
			{
				species.share = std::clamp(std::stof(value), 0.0f, 1.0f);
			}
			else
			{
				SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Foliage: unknown key {} (line {})", key, lineNumber);
			}
		}
		catch (const std::exception&)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Foliage: bad value {} (line {})", value, lineNumber);
		}
	}
	}
	std::erase_if(_species, [](const Species& species) {
		return species.layers.empty() || (species.terrains.empty() && species.looks.empty());
	});
	// the flyers find their plants by name, in any foliage.cfg
	_flyersOver.assign(_species.size(), {});
	for (auto& flyer : _flyers)
	{
		for (const auto& name : flyer.over)
		{
			const auto found = std::ranges::find(_species, name, &Species::name);
			if (found != _species.end())
			{
				flyer.overSpecies.push_back(static_cast<uint16_t>(found - _species.begin()));
			}
			else
			{
				SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Foliage: flyer {} over unknown plant {}", flyer.name, name);
			}
		}
	}
	std::erase_if(_flyers, [](const Flyer& flyer) { return flyer.animations.empty() || flyer.overSpecies.empty(); });
	for (size_t f = 0; f < _flyers.size(); ++f)
	{
		for (const auto species : _flyers[f].overSpecies)
		{
			_flyersOver[species].push_back(static_cast<uint16_t>(f));
		}
	}
	std::erase_if(_fieldStages, [](const FieldStage& stage) { return stage.layers.empty(); });
	std::ranges::sort(_fieldStages, {}, [](const FieldStage& stage) { return stage.growth.x; });
	if (layers.empty() || (_species.empty() && _fieldStages.empty()))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Foliage: nothing to draw in {}", directory.string());
		return false;
	}

	std::vector<uint8_t> texels;
	texels.reserve(layers.size() * layers.front().size());
	for (const auto& layer : layers)
	{
		texels.insert(texels.end(), layer.begin(), layer.end());
	}
	_texture = std::make_unique<graphics::Texture2D>("Foliage");
	_texture->Create(k_LayerWidth, k_LayerHeight, static_cast<uint16_t>(layers.size()), graphics::TextureFormat::RGBA8,
	                 graphics::Wrapping::ClampEdge, graphics::Filter::LinearMipmapLinear,
	                 bgfx::copy(texels.data(), static_cast<uint32_t>(texels.size())));

	// two crossed planes: x -0.5..0.5 across, y 0..1 up from the base, z = which plane
	bgfx::VertexLayout quadLayout;
	quadLayout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();
	static constexpr std::array<float, 24> k_Quad = {-0.5f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.5f, 1.0f, 0.0f, -0.5f, 1.0f, 0.0f,
	                                                 -0.5f, 0.0f, 1.0f, 0.5f, 0.0f, 1.0f, 0.5f, 1.0f, 1.0f, -0.5f, 1.0f, 1.0f};
	static constexpr std::array<uint16_t, 12> k_Indices = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
	_quad = bgfx::createVertexBuffer(bgfx::makeRef(k_Quad.data(), sizeof(k_Quad)), quadLayout);
	// the field soil: x, z -0.5..0.5 and uv, like the footprints of the meshes (vs_footprint_instanced)
	bgfx::VertexLayout soilLayout;
	soilLayout.begin()
	    .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .end();
	static constexpr std::array<float, 24> k_Soil = {-0.5f, -0.5f, 0.0f, 0.0f, 0.5f, -0.5f, 1.0f, 0.0f, 0.5f, 0.5f, 1.0f, 1.0f,
	                                                 -0.5f, -0.5f, 0.0f, 0.0f, 0.5f, 0.5f,  1.0f, 1.0f, -0.5f, 0.5f, 0.0f, 1.0f};
	_soilQuad = bgfx::createVertexBuffer(bgfx::makeRef(k_Soil.data(), sizeof(k_Soil)), soilLayout);
	_quadIndices = bgfx::createIndexBuffer(bgfx::makeRef(k_Indices.data(), sizeof(k_Indices)));
	static constexpr std::array<float, 18> k_Fold = {-0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f,
	                                                 -0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f, 0.0f};
	static constexpr std::array<uint16_t, 12> k_FoldIndices = {0, 1, 4, 0, 4, 3, 1, 2, 5, 1, 5, 4};
	_foldQuad = bgfx::createVertexBuffer(bgfx::makeRef(k_Fold.data(), sizeof(k_Fold)), quadLayout);
	_foldIndices = bgfx::createIndexBuffer(bgfx::makeRef(k_FoldIndices.data(), sizeof(k_FoldIndices)));

	SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Foliage: {} plant kinds, {} field stages, {} flyers, {} images ({} modules)",
	                   _species.size(), _fieldStages.size(), _flyers.size(), layers.size(), modules.size());
	return true;
}

void Foliage::Update(LandIslandInterface& island, float density, glm::vec3 cameraPosition, float distance, bool fields)
{
	if (!IsLoaded())
	{
		return;
	}
	const auto& materials = island.GetMaterialInfo();
	const auto& blocks = island.GetBlocks();
	if (materials.empty() || blocks.empty() || density <= 0.0f)
	{
		Clear();
		_placementKey = 0;
		return;
	}
	// Placed again for another island, another density, or once the scene's objects exist (the island loads first)
	bool hasObjects = false;
	if (Locator::entitiesRegistry::has_value())
	{
		Locator::entitiesRegistry::value().Each<const ecs::components::Fixed>(
		    [&hasObjects](const ecs::components::Fixed& /*unused*/) { hasObjects = true; });
	}
	uint32_t densityBits = 0;
	std::memcpy(&densityBits, &density, sizeof(densityBits));
	const uint64_t key = (reinterpret_cast<uintptr_t>(blocks.data()) * 31u + blocks.size()) ^
	                     (static_cast<uint64_t>(densityBits) << 1u) ^ (hasObjects ? 1u : 0u);
	if (key != _placementKey)
	{
		Clear();
		_placementKey = key;
		_blocked = BuildBlockedMap(island);
		_water = std::make_unique<FoliageWaterMap>(island);
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Foliage: water {}", _water->Describe());
		for (const auto& material : materials)
		{
			_looks.push_back(static_cast<uint8_t>(ClassifyTexture(material.colour)));
		}
		_chunks.resize(blocks.size());
		for (size_t i = 0; i < blocks.size(); ++i)
		{
			_chunks[i].centre = blocks[i].GetMapPosition() + glm::vec2(k_BlockSize * 0.5f);
		}
		std::string lookList;
		for (size_t i = 0; i < _looks.size(); ++i)
		{
			lookList += fmt::format("{}{}:{}", i == 0 ? "" : " ", i, k_LookNames.at(_looks[i]));
		}
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Foliage: {} no-plant cells around objects; textures {}",
		                   _blocked->CountBlocked(), lookList);
	}

	// A block is placed when it comes within reach and freed well past it; a few per frame so moving is smooth
	const glm::vec2 eye(cameraPosition.x, cameraPosition.z);
	const float reach = distance + k_BlockSize * 0.75f;
	constexpr int k_BuildsPerFrame = 6;
	int builds = 0;
	for (size_t i = 0; i < _chunks.size(); ++i)
	{
		auto& chunk = _chunks[i];
		const float away = glm::distance(chunk.centre, eye);
		if (!chunk.built && away <= reach && builds < k_BuildsPerFrame)
		{
			BuildChunk(island, i, density);
			++builds;
		}
		else if (chunk.built && away > reach + k_BlockSize)
		{
			if (bgfx::isValid(chunk.instances))
			{
				bgfx::destroy(chunk.instances);
				chunk.instances = BGFX_INVALID_HANDLE;
			}
			_plantCount -= chunk.count;
			chunk.count = 0;
			chunk.built = false;
		}
	}

	_fieldInstances.clear();
	if (fields && !_fieldStages.empty())
	{
		UpdateFields(island, cameraPosition, distance);
	}
	else if (Locator::entitiesRegistry::has_value())
	{
		// the option turned off: the field meshes back to their own colour
		auto& registry = Locator::entitiesRegistry::value();
		std::vector<entt::entity> tinted;
		registry.Each<const ecs::components::MeshTint, const ecs::components::Field>(
		    [&tinted](entt::entity entity, const ecs::components::MeshTint& /*unused*/,
		               const ecs::components::Field& /*unused*/) { tinted.push_back(entity); });
		for (const auto entity : tinted)
		{
			registry.Remove<ecs::components::MeshTint>(entity);
		}
	}
}


void Foliage::UpdateFields(LandIslandInterface& island, glm::vec3 cameraPosition, float distance)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	using namespace ecs::components;
	auto& registry = Locator::entitiesRegistry::value();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const glm::vec2 eye(cameraPosition.x, cameraPosition.z);
	const float firstGrowth = _fieldStages.front().growth.x;
	const float lastGrowth = _fieldStages.back().growth.y;
	registry.Each<const Field, const Transform, const Mesh>([&](entt::entity entity, const Field& field,
	                                                            const Transform& transform, const Mesh& mesh) {
		if (!meshes.Contains(mesh.id))
		{
			return;
		}
		const auto box = meshes.Handle(mesh.id)->GetBoundingBox();
		// the field mesh (seen far away) is tinted like the plants: the average ground colour under it while young,
		// its texture's own colour once ripe
		const auto key = static_cast<uint32_t>(entt::to_integral(entity));
		auto ground = _fieldGround.find(key);
		if (ground == _fieldGround.end())
		{
			glm::vec3 sum(0.0f);
			for (int i = 0; i < 3; ++i)
			{
				for (int j = 0; j < 3; ++j)
				{
					const glm::vec3 local(glm::mix(box.minima.x, box.maxima.x, 0.2f + 0.3f * static_cast<float>(i)), 0.0f,
					                      glm::mix(box.minima.z, box.maxima.z, 0.2f + 0.3f * static_cast<float>(j)));
					const glm::vec3 world = transform.position + transform.rotation * (local * transform.scale);
					sum += GroundColourAt(island, glm::vec2(world.x, world.z));
				}
			}
			ground = _fieldGround.emplace(key, sum / 9.0f).first;
		}
		const float own = std::clamp((field.growth - _fieldRipening.x) / std::max(_fieldRipening.y - _fieldRipening.x, 1.0f),
		                             0.0f, 1.0f);
		if (auto* tint = registry.TryGet<MeshTint>(entity); tint != nullptr)
		{
			tint->ground = ground->second;
			tint->own = own;
		}
		else
		{
			registry.Assign<MeshTint>(entity, ground->second, own, true);
		}
		const glm::vec3 centre = transform.position + transform.rotation * (box.Center() * transform.scale);
		const float radius = 0.5f * glm::length(glm::vec2(box.Size().x * transform.scale.x, box.Size().z * transform.scale.z));
		if (glm::distance(glm::vec2(centre.x, centre.z), eye) > distance + radius)
		{
			return;
		}
		// placed once per field: a jittered grid over the mesh's footprint, on the ground
		auto& plants = _fieldPlants[static_cast<uint32_t>(entt::to_integral(entity))];
		if (plants.empty())
		{
			const auto seed = static_cast<uint32_t>(entt::to_integral(entity)) * 7919u + 13u;
			const glm::vec2 step = glm::vec2(_fieldSpacing) / glm::max(glm::vec2(transform.scale.x, transform.scale.z), 0.01f);
			const auto columns = static_cast<int>(std::max(1.0f, std::floor(box.Size().x / step.x)));
			const auto rows = static_cast<int>(std::max(1.0f, std::floor(box.Size().z / step.y)));
			for (int i = 0; i < columns; ++i)
			{
				for (int j = 0; j < rows; ++j)
				{
					Random random(Hash(static_cast<uint32_t>(i), static_cast<uint32_t>(j), seed));
					const glm::vec3 local(box.minima.x + (static_cast<float>(i) + 0.15f + 0.7f * random.Next()) * step.x,
					                      box.Center().y,
					                      box.minima.z + (static_cast<float>(j) + 0.15f + 0.7f * random.Next()) * step.y);
					const glm::vec3 world = transform.position + transform.rotation * (local * transform.scale);
					const glm::vec2 point(world.x, world.z);
					FieldPlant plant {};
					plant.yaw = random.Next() * 3.1415927f;
					const glm::vec2 along(std::cos(plant.yaw), std::sin(plant.yaw));
					plant.position = glm::vec3(point.x, island.GetDrawnHeightAt(point), point.y);
					plant.groundSlope = 0.5f * (island.GetDrawnHeightAt(point + along) - island.GetDrawnHeightAt(point - along));
					const int last = island.GetCellsPerSide() - 1;
					const auto cell = glm::clamp(glm::ivec2(glm::floor(point / k_CellSize)), 0, last);
					plant.luminosity = static_cast<float>(island.GetCell(glm::u16vec2(cell)).luminosity) / 255.0f;
					plant.lean = random.Next() * 2.0f - 1.0f;
					plant.phase = random.Next() * 6.2831853f;
					plant.stagger = random.Next() * 2.0f - 1.0f;
					plant.keep = random.Next();
					plant.pickImage = random.Next();
					plant.pickSize = random.Next();
					plants.push_back(plant);
				}
			}
		}
		// nothing until it is sown; the harvest thins it: the share of the food it would have at this growth
		if (field.crops < Field::k_TimesToSow)
		{
			return;
		}
		const float expectedFood = std::min(field.growth, Field::k_AgeRecolt) * Field::k_TotalFood / Field::k_AgeRecolt;
		const float kept = expectedFood > 0.0f ? std::clamp(field.food / expectedFood, 0.0f, 1.0f) : 0.0f;
		for (const auto& plant : plants)
		{
			const float growth = std::min(field.growth, lastGrowth) + plant.stagger * _fieldStagger;
			if (plant.keep >= kept || growth < firstGrowth)
			{
				continue;
			}
			const auto found = std::ranges::find_if(_fieldStages, [growth](const FieldStage& stage) { return growth < stage.growth.y; });
			const auto& stage = found != _fieldStages.end() ? *found : _fieldStages.back();
			if (growth < stage.growth.x)
			{
				continue; // between two stages that don't meet
			}
			const float t = std::clamp((growth - stage.growth.x) / std::max(stage.growth.y - stage.growth.x, 1.0f), 0.0f, 1.0f);
			const auto layer =
			    stage.layers[std::min(static_cast<size_t>(plant.pickImage * stage.layers.size()), stage.layers.size() - 1)];
			const float width = glm::mix(stage.size.x, stage.size.y, t) * (0.85f + 0.3f * plant.pickSize);
			const float plantHeight = width * _layerAspect[layer];
			const auto colour = glm::clamp(glm::mix(stage.colourFrom, stage.colourTo, t), 0.0f, 1.0f);
			const auto packed = static_cast<float>(static_cast<uint32_t>(colour.r * 255.0f + 0.5f) * 65536u +
			                                       static_cast<uint32_t>(colour.g * 255.0f + 0.5f) * 256u +
			                                       static_cast<uint32_t>(colour.b * 255.0f + 0.5f));
			const float end = plant.groundSlope * 0.5f * width;
			_fieldInstances.push_back({{plant.position.x, plant.position.y - 0.06f * plantHeight, plant.position.z, width},
			                           {plantHeight, static_cast<float>(layer), plant.luminosity, plant.yaw},
			                           {_layerTop[layer], stage.sway, 0.0f, static_cast<float>(Tint::Grey)},
			                           {0.0f, 0.0f, stage.lean * plant.lean, plant.phase},
			                           {-end, end, 1.0f, packed}});
		}
	});
}

void Foliage::BuildChunk(LandIslandInterface& island, size_t blockIndex, float density)
{
	const auto& materials = island.GetMaterialInfo();
	const auto& countries = island.GetCountries();
	const auto& block = island.GetBlocks()[blockIndex];
	auto& chunk = _chunks[blockIndex];
	chunk.built = true;

	std::vector<Instance> instances;
	std::vector<Instance> crossInstances;
	std::vector<Instance> flatInstances;
	chunk.homes.clear();
	const float repeats = Locator::config::value().terrainTextureDensity;
	const glm::vec2 mapPosition = block.GetMapPosition();
	const auto blockOffset = glm::ivec2(block.GetBlockPosition() * 16);
	for (int x = 0; x < 16; ++x)
	{
		for (int z = 0; z < 16; ++z)
		{
			const glm::ivec2 cellCoordinates = blockOffset + glm::ivec2(x, z);
			const auto cellAt = [&island, cellCoordinates](int dx, int dz) -> const lnd::LNDCell& {
				const int last = island.GetCellsPerSide() - 1;
				return island.GetCell(glm::u16vec2(std::clamp(cellCoordinates.x + dx, 0, last),
				                                   std::clamp(cellCoordinates.y + dz, 0, last)));
			};
			const std::array<const lnd::LNDCell*, 4> corners = {&cellAt(0, 0), &cellAt(1, 0), &cellAt(0, 1), &cellAt(1, 1)};
			const glm::vec2 cellOrigin = mapPosition + glm::vec2(x, z) * k_CellSize;
			for (size_t s = 0; s < _species.size(); ++s)
			{
				const auto& species = _species[s];
				Random random(Hash(static_cast<uint32_t>(cellCoordinates.x), static_cast<uint32_t>(cellCoordinates.y),
				                   static_cast<uint32_t>(s)));
				const float expected = species.perCell * density;
				const int count = static_cast<int>(expected + random.Next());
				for (int i = 0; i < count; ++i)
				{
					const glm::vec2 fraction(random.Next(), random.Next());
					const glm::vec2 point = cellOrigin + fraction * k_CellSize;
					const float pickCorner = random.Next();
					const float pickMaterial = random.Next();
					const float pickSize = random.Next();
					const float pickImage = random.Next();
					const float yaw = random.Next() * 3.1415927f;
					const float lean = species.lean * (random.Next() * 2.0f - 1.0f);
					const float phase = random.Next() * 6.2831853f;

					// the landscape material under the point: a corner picked by its bilinear weight, then one of the
					// corner's two materials by the blend coefficient, like the terrain shader mixes them
					const std::array<float, 4> weights = {(1 - fraction.x) * (1 - fraction.y), fraction.x * (1 - fraction.y),
					                                      (1 - fraction.x) * fraction.y, fraction.x * fraction.y};
					size_t corner = 0;
					for (float accumulated = weights[0]; corner < 3 && pickCorner > accumulated;)
					{
						accumulated += weights[++corner];
					}
					const auto& cell = *corners.at(corner);
					// nothing on the coast or next to water: those cells are drawn see-through over the sea (except for
					// the beach's things, coast = on, kept above the water by their altitude)
					if (!species.coast && std::ranges::any_of(corners, [](const lnd::LNDCell* c) {
						    return c->properties.hasWater || c->properties.fullWater || c->properties.coastLine;
					    }))
					{
						continue;
					}
					const auto zone = ZoneOf(cell.flags);
					if ((!species.zones.empty() && std::ranges::find(species.zones, zone) == species.zones.end()) ||
					    std::ranges::find(species.notZones, zone) != species.notZones.end())
					{
						continue;
					}
					const auto& country = countries.at(cell.properties.country);
					const auto noiseCoordinates =
					    glm::u8vec2(cellCoordinates + glm::ivec2(static_cast<int>(corner & 1), static_cast<int>(corner >> 1)));
					const auto altitude = island.GetCellAltitude(cell);
					const auto& mapMaterial =
					    altitude > 255
					        ? country.materials.back()
					        : country.materials.at((altitude + island.GetNoise(noiseCoordinates)) % country.materials.size());
					const auto materialIndex = pickMaterial * 255.0f < static_cast<float>(mapMaterial.coefficient)
					                               ? mapMaterial.indices[1]
					                               : mapMaterial.indices[0];
					const auto grows = [&](uint32_t index) {
						return index < materials.size() && !materials[index].picture &&
						       (std::ranges::find(species.terrains, materials[index].type) != species.terrains.end() ||
						        std::ranges::find(species.looks, _looks[index]) != species.looks.end());
					};
					if (!grows(materialIndex))
					{
						continue;
					}
					// share: how much of the ground drawn at the point is made of its materials (the four corners by their
					// bilinear weight, each corner's two materials by its blend), so that it doesn't show where another
					// texture covers most of it
					if (species.share > 0.0f)
					{
						float share = 0.0f;
						for (size_t c = 0; c < corners.size(); ++c)
						{
							const auto& cornerCell = *corners.at(c);
							const auto cornerAltitude = island.GetCellAltitude(cornerCell);
							const auto& cornerCountry = countries.at(cornerCell.properties.country);
							const auto& blend =
							    cornerAltitude > 255
							        ? cornerCountry.materials.back()
							        : cornerCountry.materials.at(
							              (cornerAltitude + island.GetNoise(glm::u8vec2(cellCoordinates + glm::ivec2(
							                                                    static_cast<int>(c & 1), static_cast<int>(c >> 1))))) %
							              cornerCountry.materials.size());
							const float second = static_cast<float>(blend.coefficient) / 255.0f;
							share += weights[c] * ((grows(blend.indices[0]) ? 1.0f - second : 0.0f) +
							                       (grows(blend.indices[1]) ? second : 0.0f));
						}
						if (share < species.share)
						{
							continue;
						}
					}
					if (species.patches > 0.0f &&
					    ValueNoise(point / 45.0f, static_cast<uint32_t>(s) + 17u) < species.patches * 0.6f)
					{
						continue;
					}
					// the species' altitude range is on the unflattened ground (the same plants as before the mesh was
					// flattened at the coast); they stand on the ground as it is drawn
					const float unflattened = island.GetUnflattenedHeightAt(point);
					if (unflattened < species.altitude.x || unflattened > species.altitude.y)
					{
						continue;
					}
					const float height = island.GetDrawnHeightAt(point);
					const float slope = glm::degrees(std::acos(std::clamp(GroundNormal(island, point).y, -1.0f, 1.0f)));
					if (slope < species.slope.x || slope > species.slope.y || _blocked->IsBlocked(point))
					{
						continue;
					}
						if (_water->DistanceTo(Water::Stream, point) < k_RiverClearance)
					{
						continue;
					}
					// the ground colour the tint takes (the material a few mip levels down at the terrain's uv):
					// very dark or colourless spots would give grey plants
					if (species.groundValue != glm::vec2(0.0f, 1.0f) || species.groundSaturation != glm::vec2(0.0f, 1.0f))
					{
						const auto& small = materials[materialIndex].small;
						if (small.empty())
						{
							continue;
						}
						constexpr int k_Small = LandMaterialInfo::k_SmallSize;
						const glm::vec2 local = (point - mapPosition) / k_BlockSize * repeats;
						const auto wrap = [](float t) {
							return static_cast<int>(std::floor((t - std::floor(t)) * k_Small)) % k_Small;
						};
						// texture u = local z, v = local x (vs_terrain), rows are v
						const auto* texel = &small[(static_cast<size_t>(wrap(local.x)) * k_Small + wrap(local.y)) * 3];
						const float high = std::max({texel[0], texel[1], texel[2]}) / 255.0f;
						const float low = std::min({texel[0], texel[1], texel[2]}) / 255.0f;
						const float saturation = high > 0.0f ? (high - low) / high : 0.0f;
						if (high < species.groundValue.x || high > species.groundValue.y ||
						    saturation < species.groundSaturation.x || saturation > species.groundSaturation.y)
						{
							continue;
						}
					}
					if (!species.nearWater.empty() && std::ranges::none_of(species.nearWater, [&](uint8_t water) {
						    const float away = _water->DistanceTo(static_cast<Water>(water), point);
						    return away >= species.waterDistance.x && away <= species.waterDistance.y;
					    }))
					{
						continue;
					}
					const float luminosity = (corners[0]->luminosity * weights[0] + corners[1]->luminosity * weights[1] +
					                          corners[2]->luminosity * weights[2] + corners[3]->luminosity * weights[3]) /
					                         255.0f;
					const auto layer =
					    species.layers[std::min(static_cast<size_t>(pickImage * species.layers.size()), species.layers.size() - 1)];
					const float width = species.size.x + (species.size.y - species.size.x) * pickSize;
					// the terrain's texture coordinates: (local z, local x) / block size (vs_terrain)
					const glm::vec2 local = point - mapPosition;
					// the bottom edge follows the ground under the plane's two ends (on a slope one end would be buried),
					// sunk a little so the image's flat bottom edge doesn't show when it leans
					const float plantHeight = width * _layerAspect[layer];
					if (species.flat)
					{
						// lying on the ground: centred on the point, tilted like the ground (its slope across the
						// image and along it, from the normal), the image's top pointing along the yaw's side
						const auto normal = GroundNormal(island, point);
						const glm::vec2 gradient = -glm::vec2(normal.x, normal.z) / std::max(normal.y, 0.2f);
						const glm::vec2 across(std::cos(yaw), std::sin(yaw));
						const glm::vec2 along(-across.y, across.x);
						flatInstances.push_back({{point.x, height + species.lift, point.y, width},
						                         {plantHeight, static_cast<float>(layer), luminosity, yaw},
						                         {_layerTop[layer], 0.0f, static_cast<float>(materialIndex),
						                          static_cast<float>(species.tint)},
						                         {local.y / k_BlockSize, local.x / k_BlockSize, species.shade, phase},
						                         {glm::dot(gradient, across), glm::dot(gradient, along), 2.0f,
						                          species.opacity}});
						continue;
					}
					const glm::vec2 across = glm::vec2(std::cos(yaw), std::sin(yaw)) * (0.5f * width);
					const float left = island.GetDrawnHeightAt(point - across) - height;
					const float right = island.GetDrawnHeightAt(point + across) - height;
					(species.cross ? crossInstances : instances).push_back({{point.x, height - 0.06f * plantHeight, point.y, width},
					                     {plantHeight, static_cast<float>(layer), luminosity, yaw},
					                     {_layerTop[layer], species.sway, static_cast<float>(materialIndex),
					                      static_cast<float>(species.tint)},
					                     {local.y / k_BlockSize, local.x / k_BlockSize, lean, phase},
					                     {left, right, 0.0f, 0.0f}});
					// the flyers that live on this kind of plant: a few of its plants get one
					for (const auto f : _flyersOver[s])
					{
						const auto& flyer = _flyers[f];
						Random pick(Hash(static_cast<uint32_t>(i) * 131u + f, static_cast<uint32_t>(cellCoordinates.x) * 977u,
						                 static_cast<uint32_t>(cellCoordinates.y) * 613u + static_cast<uint32_t>(s)));
						if (pick.Next() >= flyer.perPlant)
						{
							continue;
						}
						const auto animation = flyer.animations[std::min(static_cast<size_t>(pick.Next() * flyer.animations.size()),
						                                                 flyer.animations.size() - 1)];
						chunk.homes.push_back({{point.x, height + 0.8f * plantHeight, point.y},
						                       luminosity,
						                       f,
						                       animation,
						                       glm::mix(flyer.size.x, flyer.size.y, pick.Next()),
						                       pick.Next()});
					}
				}
			}
		}
	}
	chunk.crossStart = static_cast<uint32_t>(instances.size());
	instances.insert(instances.end(), crossInstances.begin(), crossInstances.end());
	chunk.flatStart = static_cast<uint32_t>(instances.size());
	instances.insert(instances.end(), flatInstances.begin(), flatInstances.end());
	chunk.count = static_cast<uint32_t>(instances.size());
	if (!instances.empty())
	{
		chunk.instances = bgfx::createVertexBuffer(
		    bgfx::copy(instances.data(), static_cast<uint32_t>(instances.size() * sizeof(Instance))), _instanceLayout);
	}
	_plantCount += instances.size();
}

void Foliage::DrawFieldFootprints(bgfx::ViewId viewId, const graphics::ShaderProgram& program) const
{
	if (!_fieldSoil || !bgfx::isValid(_soilQuad) || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	using namespace ecs::components;
	const auto& meshes = Locator::resources::value().GetMeshes();
	std::vector<glm::mat4> matrices;
	Locator::entitiesRegistry::value().Each<const Field, const Transform, const Mesh>(
	    [&](const Field& /*unused*/, const Transform& transform, const Mesh& mesh) {
		    if (!meshes.Contains(mesh.id))
		    {
			    return;
		    }
		    const auto box = meshes.Handle(mesh.id)->GetBoundingBox();
		    const auto size = glm::vec3(box.Size().x, 1.0f, box.Size().z) * (1.0f + 2.0f * _fieldSoilMargin);
		    matrices.push_back(glm::translate(glm::vec3(transform.position.x, 0.0f, transform.position.z)) *
		                       glm::mat4(transform.rotation) * glm::scale(transform.scale) *
		                       glm::translate(glm::vec3(box.Center().x, 0.0f, box.Center().z)) * glm::scale(size));
	    });
	const auto count = static_cast<uint32_t>(matrices.size());
	constexpr uint16_t k_Stride = sizeof(glm::mat4);
	if (count == 0 || bgfx::getAvailInstanceDataBuffer(count, k_Stride) < count)
	{
		return;
	}
	bgfx::InstanceDataBuffer buffer;
	bgfx::allocInstanceDataBuffer(&buffer, count, k_Stride);
	std::memcpy(buffer.data, matrices.data(), matrices.size() * sizeof(glm::mat4));
	program.SetTextureSampler("s_footprint", 0, *_fieldSoil);
	bgfx::setVertexBuffer(0, _soilQuad);
	bgfx::setInstanceDataBuffer(&buffer);
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA);
	bgfx::submit(viewId, graphics::toBgfx(program.GetRawHandle()));
}

void Foliage::Draw(const DrawDesc& desc) const
{
	if (!IsLoaded() || (_chunks.empty() && _fieldInstances.empty() && _flyerInstances.empty()) || !bgfx::isValid(desc.landLight) ||
	    desc.materials == nullptr)
	{
		return;
	}
	const glm::vec4 u_foliageEye(desc.cameraPosition, desc.distance);
	const glm::vec4 u_foliageParams(desc.seconds, desc.materialRepeats, 0.0f, 0.0f);
	const auto& program = *desc.program;
	program.SetTextureSampler("s0_foliage", 0, *_texture);
	program.SetTextureSampler("s1_landLight", 1, graphics::fromBgfx(desc.landLight));
	program.SetTextureSampler("s2_materials", 2, *desc.materials);
	program.SetUniformValue("u_foliageEye", &u_foliageEye);
	program.SetUniformValue("u_foliageParams", &u_foliageParams);
	program.SetUniformValue("u_haze", &desc.haze);
	program.SetUniformValue("u_hazeColour", &desc.hazeColour);

	// two-sided: the crossed planes are seen from both faces
	const uint64_t state = BGFX_STATE_WRITE_MASK | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA |
	                       (desc.alphaToCoverage ? BGFX_STATE_BLEND_ALPHA_TO_COVERAGE : 0);
	// the flat ones blend over the ground by their alpha, without writing depth (the plants still hide them)
	const uint64_t flatState = BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA;
	const float reach = desc.distance + k_BlockSize * 0.75f;
	const glm::vec2 eye(desc.cameraPosition.x, desc.cameraPosition.z);
	for (const auto& chunk : _chunks)
	{
		if (chunk.count == 0 || glm::distance(chunk.centre, eye) > reach)
		{
			continue;
		}
		// one plane (crossed planes show as little crosses from above), both for the species with cross = on
		for (const auto& [first, count, indices, drawState] :
		     {std::tuple {0u, chunk.crossStart, 6u, state},
		      std::tuple {chunk.crossStart, chunk.flatStart - chunk.crossStart, 12u, state},
		      std::tuple {chunk.flatStart, chunk.count - chunk.flatStart, 6u, flatState}})
		{
			if (count == 0)
			{
				continue;
			}
			bgfx::setVertexBuffer(0, _quad);
			bgfx::setIndexBuffer(_quadIndices, 0, indices);
			bgfx::setInstanceDataBuffer(chunk.instances, first, count);
			bgfx::setState(drawState);
			bgfx::submit(desc.viewId, graphics::toBgfx(program.GetRawHandle()), 0, BGFX_DISCARD_ALL & ~BGFX_DISCARD_BINDINGS);
		}
	}
	// the crop fields' plants change with the fields, the flyers move: rebuilt every frame (a few thousand at most)
	for (const auto* transient : {&_fieldInstances, &_flyerInstances})
	{
		const auto count = static_cast<uint32_t>(transient->size());
		if (count == 0 || bgfx::getAvailInstanceDataBuffer(count, sizeof(Instance)) != count)
		{
			continue;
		}
		bgfx::InstanceDataBuffer buffer;
		bgfx::allocInstanceDataBuffer(&buffer, count, sizeof(Instance));
		std::memcpy(buffer.data, transient->data(), count * sizeof(Instance));
		// the flyers on the folding quad (flat with no fold for those with fold = off)
		const bool flyers = transient == &_flyerInstances;
		bgfx::setVertexBuffer(0, flyers ? _foldQuad : _quad);
		bgfx::setIndexBuffer(flyers ? _foldIndices : _quadIndices, 0, flyers ? 12 : 6);
		bgfx::setInstanceDataBuffer(&buffer);
		bgfx::setState(state);
		bgfx::submit(desc.viewId, graphics::toBgfx(program.GetRawHandle()), 0, BGFX_DISCARD_ALL & ~BGFX_DISCARD_BINDINGS);
	}
	bgfx::discard(BGFX_DISCARD_ALL);
}
