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

#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <stb_image.h>

#include "3D/L3DMesh.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Registry.h"
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
	    .end();
}

Foliage::~Foliage()
{
	Clear();
	if (bgfx::isValid(_quad))
	{
		bgfx::destroy(_quad);
	}
	if (bgfx::isValid(_quadIndices))
	{
		bgfx::destroy(_quadIndices);
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
	_looks.clear();
	_plantCount = 0;
}

bool Foliage::Load(const std::filesystem::path& directory)
{
	std::ifstream file(directory / "foliage.cfg");
	if (!file)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Foliage: no {}", (directory / "foliage.cfg").string());
		return false;
	}

	std::vector<std::vector<uint8_t>> layers;
	std::vector<std::string> layerFiles;
	const auto layerOf = [&](const std::string& image) -> int {
		if (const auto found = std::ranges::find(layerFiles, image); found != layerFiles.end())
		{
			return static_cast<int>(found - layerFiles.begin());
		}
		int width = 0;
		int height = 0;
		int channels = 0;
		auto* pixels = stbi_load((directory / image).string().c_str(), &width, &height, &channels, 4);
		if (pixels == nullptr)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Foliage: cannot read {}", image);
			return -1;
		}
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
		stbi_image_free(pixels);
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
		layerFiles.push_back(image);
		_layerTop.push_back(1.0f - static_cast<float>(scaledHeight) / k_LayerHeight);
		_layerAspect.push_back(static_cast<float>(height) / static_cast<float>(width));
		return static_cast<int>(layers.size() - 1);
	};

	_species.clear();
	_layerTop.clear();
	_layerAspect.clear();
	std::string line;
	int lineNumber = 0;
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
			_species.push_back({});
			_species.back().name = Trim(std::string_view(line).substr(1, line.size() - 2));
			continue;
		}
		const auto equals = line.find('=');
		if (equals == std::string::npos || _species.empty())
		{
			SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Foliage: foliage.cfg line {} ignored", lineNumber);
			continue;
		}
		const auto key = Trim(std::string_view(line).substr(0, equals));
		const auto value = Trim(std::string_view(line).substr(equals + 1));
		auto& species = _species.back();
		try
		{
			if (key == "images")
			{
				for (const auto& image : SplitList(value))
				{
					if (const int layer = layerOf(image); layer >= 0)
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
				species.perCell = std::stof(value);
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
			else if (key == "lean")
			{
				species.lean = std::stof(value);
			}
			else if (key == "tint")
			{
				species.tint = value == "none" ? Tint::None : value == "all" ? Tint::All : Tint::Grey;
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
	std::erase_if(_species, [](const Species& species) {
		return species.layers.empty() || (species.terrains.empty() && species.looks.empty());
	});
	if (layers.empty() || _species.empty())
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
	_quadIndices = bgfx::createIndexBuffer(bgfx::makeRef(k_Indices.data(), sizeof(k_Indices)));

	SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Foliage: {} plant kinds, {} images", _species.size(), layers.size());
	return true;
}

void Foliage::Update(LandIslandInterface& island, float density, glm::vec3 cameraPosition, float distance)
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
}

void Foliage::BuildChunk(LandIslandInterface& island, size_t blockIndex, float density)
{
	const auto& materials = island.GetMaterialInfo();
	const auto& countries = island.GetCountries();
	const auto& block = island.GetBlocks()[blockIndex];
	auto& chunk = _chunks[blockIndex];
	chunk.built = true;

	std::vector<Instance> instances;
	const glm::vec2 mapPosition = block.GetMapPosition();
	const auto blockOffset = glm::ivec2(block.GetBlockPosition() * 16);
	for (int x = 0; x < 16; ++x)
	{
		for (int z = 0; z < 16; ++z)
		{
			const glm::ivec2 cellCoordinates = blockOffset + glm::ivec2(x, z);
			const auto cellAt = [&island, cellCoordinates](int dx, int dz) -> const lnd::LNDCell& {
				return island.GetCell(glm::u16vec2(std::clamp(cellCoordinates.x + dx, 0, 511),
				                                   std::clamp(cellCoordinates.y + dz, 0, 511)));
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
					if (cell.properties.hasWater || cell.properties.fullWater)
					{
						continue;
					}
					const auto& country = countries.at(cell.properties.country);
					const auto noiseCoordinates =
					    glm::u8vec2(cellCoordinates + glm::ivec2(static_cast<int>(corner & 1), static_cast<int>(corner >> 1)));
					const auto& mapMaterial =
					    country.materials.at((cell.altitude + island.GetNoise(noiseCoordinates)) % country.materials.size());
					const auto materialIndex = pickMaterial * 255.0f < static_cast<float>(mapMaterial.coefficient)
					                               ? mapMaterial.indices[1]
					                               : mapMaterial.indices[0];
					if (materialIndex >= materials.size() || materials[materialIndex].picture ||
					    (std::ranges::find(species.terrains, materials[materialIndex].type) == species.terrains.end() &&
					     std::ranges::find(species.looks, _looks[materialIndex]) == species.looks.end()))
					{
						continue;
					}
					if (species.patches > 0.0f &&
					    ValueNoise(point / 45.0f, static_cast<uint32_t>(s) + 17u) < species.patches * 0.6f)
					{
						continue;
					}
					const float height = island.GetHeightAt(point);
					if (height < species.altitude.x || height > species.altitude.y)
					{
						continue;
					}
					const float slope = glm::degrees(std::acos(std::clamp(island.GetNormalAt(point).y, -1.0f, 1.0f)));
					if (slope < species.slope.x || slope > species.slope.y || _blocked->IsBlocked(point))
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
					// sunk a little so the image's flat bottom edge stays under the ground when it leans
					const float plantHeight = width * _layerAspect[layer];
					instances.push_back({{point.x, height - 0.12f * plantHeight, point.y, width},
					                     {plantHeight, static_cast<float>(layer), luminosity, yaw},
					                     {_layerTop[layer], species.sway, static_cast<float>(materialIndex),
					                      static_cast<float>(species.tint)},
					                     {local.y / k_BlockSize, local.x / k_BlockSize, lean, phase}});
				}
			}
		}
	}
	chunk.count = static_cast<uint32_t>(instances.size());
	if (!instances.empty())
	{
		chunk.instances = bgfx::createVertexBuffer(
		    bgfx::copy(instances.data(), static_cast<uint32_t>(instances.size() * sizeof(Instance))), _instanceLayout);
	}
	_plantCount += instances.size();
}

void Foliage::Draw(const DrawDesc& desc) const
{
	if (!IsLoaded() || _chunks.empty() || !bgfx::isValid(desc.landLight) || desc.materials == nullptr)
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
	const float reach = desc.distance + k_BlockSize * 0.75f;
	const glm::vec2 eye(desc.cameraPosition.x, desc.cameraPosition.z);
	for (const auto& chunk : _chunks)
	{
		if (chunk.count == 0 || glm::distance(chunk.centre, eye) > reach)
		{
			continue;
		}
		bgfx::setVertexBuffer(0, _quad);
		bgfx::setIndexBuffer(_quadIndices, 0, 6); // one plane: crossed planes show as little crosses from above
		bgfx::setInstanceDataBuffer(chunk.instances, 0, chunk.count);
		bgfx::setState(state);
		bgfx::submit(desc.viewId, graphics::toBgfx(program.GetRawHandle()), 0, BGFX_DISCARD_ALL & ~BGFX_DISCARD_BINDINGS);
	}
	bgfx::discard(BGFX_DISCARD_ALL);
}
