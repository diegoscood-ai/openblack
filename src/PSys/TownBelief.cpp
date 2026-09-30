/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownBelief.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <chrono>
#include <numbers>
#include <random>
#include <set>
#include <string>
#include <unordered_map>

#include <glm/gtc/matrix_transform.hpp>

#include "3D/L3DMesh.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/TotemStatue.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
// SF_TownBelief's UR_TownCentreBelief0
constexpr float k_HeightAt1 = 2.0f;
constexpr float k_HeightPerLevel = 2.5f;
constexpr float k_RadiusAt0 = 0.3f, k_RadiusAt1 = 0.6f;
constexpr float k_ScaleAt0 = 0.5f, k_ScaleAt1 = 1.0f;
constexpr float k_SpeedAt0 = 1.0f, k_SpeedAt1 = 2.0f;
constexpr float k_PhaseSpeed = 0.2f;
constexpr float k_SpeedUpDuringFight = 2.0f;
constexpr float k_FightAt0 = 1.5f, k_FightAt1 = 3.0f;
constexpr float k_BetweenFightsAt0 = 10.0f, k_BetweenFightsAt1 = 3.0f;
constexpr float k_Step = 0.1f;

/// the player colours (0xBFF0B8, by GetRemapedPlayer; the remap comes from the profile, identity here)
constexpr std::array<uint32_t, 7> k_PlayerColours = {0xFF4646, 0x47FF54, 0xE347FF, 0x47F9FF, 0xFFFD47, 0x4777FF, 0xFFA247};

struct Symbol
{
	float a1 {0.0f}, a2 {0.0f}, phase {0.0f};
	float fightTimer {0.0f}, fightLength {0.0f}, waitLength {-1.0f};
	bool fighting {false};
};

struct Centre
{
	std::unordered_map<int, Symbol> symbols;
};

std::unordered_map<entt::entity, Centre> g_Centres;
std::mt19937 g_Random(4242);

float Rand(float a, float b)
{
	return a + std::uniform_real_distribution<float>(0.0f, 1.0f)(g_Random) * (b - a);
}

int PlayerIndex(const std::string& name)
{
	static const std::array<std::string_view, 8> k_Names = {"PLAYER_ONE", "PLAYER_TWO", "PLAYER_THREE", "PLAYER_FOUR",
	                                                        "PLAYER_FIVE", "PLAYER_SIX", "PLAYER_SEVEN", "PLAYER_EIGHT"};
	for (size_t i = 0; i < k_Names.size(); ++i)
	{
		if (name == k_Names[i])
		{
			return static_cast<int>(i);
		}
	}
	return -1;
}

/// PlayersSymbols (PlayerSymbol::CreateFinalTextureSymbols 0x5DEF00): the local human's cell is a copy of the
/// ChooseSymbol cell of the profile's "player symbol" (registry, 0 when missing, as on this install), so it is drawn
/// straight from ChooseSymbol (4 x 4 cells of 64 x 64). Computer players get Lethis/Kazarr/Nemesis .cps greyscale
/// images there (not done: they use ChooseSymbol cell = player number).
const Creator& SymbolCreator(int player)
{
	static std::array<Creator, 16> creators = [] {
		std::array<Creator, 16> c;
		for (int i = 0; i < 16; ++i)
		{
			c[static_cast<size_t>(i)].kind = Creator::Kind::Sprite;
			c[static_cast<size_t>(i)].texture = "ChooseSymbol";
			c[static_cast<size_t>(i)].spritesPerRow = 4;
			c[static_cast<size_t>(i)].fileOffset = i;
			c[static_cast<size_t>(i)].numFrames = 1;
		}
		return c;
	}();
	constexpr int k_ProfileSymbol = 0;
	return creators[static_cast<size_t>(player == 0 ? k_ProfileSymbol : std::clamp(player, 0, 15))];
}

const Creator& GlowCreator()
{
	static const Creator creator = [] {
		Creator c;
		c.kind = Creator::Kind::Sprite;
		c.texture = "S_SpriteSheet3";
		c.spritesPerRow = 8;
		c.numFrames = 32;
		return c;
	}();
	return creator;
}

bool IsTownCentre(const ecs::components::Abode& abode)
{
	static const std::set<AbodeNumber> k_Centres = [] {
		std::set<AbodeNumber> result;
		for (const auto& info : Locator::infoConstants::value().abode)
		{
			if (info.abodeType == AbodeType::TownCentre)
			{
				result.insert(info.abodeNumber);
			}
		}
		return result;
	}();
	return k_Centres.contains(abode.type);
}
} // namespace

void town_belief::Clear()
{
	g_Centres.clear();
}

void town_belief::Collect(const glm::vec3& camera, std::vector<manager::Drawable>& out)
{
	using namespace ecs::components;
	auto& registry = Locator::entitiesRegistry::value();
	const auto& towns = registry.Context().towns;
	const auto& meshes = Locator::resources::value().GetMeshes();
	const float seconds = std::chrono::duration<float>(std::chrono::steady_clock::now().time_since_epoch()).count();

	registry.Each<const Abode, const Transform, const Mesh>([&](entt::entity entity, const Abode& abode,
	                                                            const Transform& transform, const Mesh& mesh) {
		if (!IsTownCentre(abode))
		{
			return;
		}
		const auto town = towns.find(abode.townId);
		if (town == towns.end() || !registry.Valid(town->second))
		{
			return;
		}
		const auto& beliefs = registry.Get<const Town>(town->second).beliefs;
		// the totem (TotemStatue, TownCentre::GetTotemPos 0x743F20) + its height (Object::GetHeight 0x638120 of the icon
		// on the plinth, GBelief::DrawBelief 0x438800) + HeightAt1; without a totem, the top of the town centre's mesh
		glm::vec3 base = transform.position;
		bool found = false;
		registry.Each<const TotemStatue, const Transform>([&](const TotemStatue& statue, const Transform& plinth) {
			if (found || statue.townCentre != entity)
			{
				return;
			}
			float height = 0.0f;
			if (registry.Valid(statue.top))
			{
				const auto* topMesh = registry.TryGet<const Mesh>(statue.top);
				const auto* topTransform = registry.TryGet<const Transform>(statue.top);
				if (topMesh != nullptr && topTransform != nullptr && meshes.Contains(topMesh->id))
				{
					height = meshes.Handle(topMesh->id)->GetBoundingBox().Size().y * topTransform->scale.y;
				}
			}
			base = glm::vec3(plinth.position.x, statue.baseY + height + k_HeightAt1, plinth.position.z);
			found = true;
		});
		if (!found && meshes.Contains(mesh.id))
		{
			base.y += meshes.Handle(mesh.id)->GetBoundingBox().maxima.y * transform.scale.y + k_HeightAt1;
		}
		const float s = std::clamp(glm::distance(camera, base) * 0.01f, 1.0f, 10.0f);
		auto& centre = g_Centres[entity];

		manager::Drawable drawable {base, {}};
		for (const auto& [name, value] : beliefs)
		{
			const int player = PlayerIndex(name);
			const float b = std::clamp(value, 0.0f, 1.0f);
			if (player < 0 || b <= 0.0f)
			{
				continue;
			}
			// rank (fn_0073BB10): the players with more belief, or as much and a higher number
			int rank = 0;
			for (const auto& [otherName, otherValue] : beliefs)
			{
				const int other = PlayerIndex(otherName);
				const float ob = std::clamp(otherValue, 0.0f, 1.0f);
				if (other >= 0 && other != player && (ob > b || (ob == b && other > player)))
				{
					++rank;
				}
			}
			auto& symbol = centre.symbols[player];
			const float radius = rank == 0 ? 0.0f : k_RadiusAt0 + (k_RadiusAt1 - k_RadiusAt0) * b;
			float speed = rank == 0 ? 0.0f : (k_SpeedAt0 + (k_SpeedAt1 - k_SpeedAt0) * b) / std::max(radius, 1e-3f);
			const float scale = k_ScaleAt0 + (k_ScaleAt1 - k_ScaleAt0) * b;
			float fight = 0.0f;
			if (rank == 1)
			{
				if (symbol.waitLength < 0.0f)
				{
					symbol.waitLength = (k_BetweenFightsAt0 + (k_BetweenFightsAt1 - k_BetweenFightsAt0) * b) * Rand(0.5f, 1.5f);
				}
				symbol.fightTimer += k_Step;
				if (!symbol.fighting && symbol.fightTimer > symbol.waitLength)
				{
					symbol.fighting = true;
					symbol.fightTimer = 0.0f;
					symbol.fightLength = (k_FightAt0 + (k_FightAt1 - k_FightAt0) * b) * Rand(0.5f, 1.5f);
				}
				if (symbol.fighting)
				{
					const float f = symbol.fightTimer / std::max(symbol.fightLength, 1e-3f);
					if (f >= 1.0f)
					{
						symbol.fighting = false;
						symbol.fightTimer = 0.0f;
						symbol.waitLength = -1.0f;
					}
					else
					{
						fight = 1.0f - (2.0f * f - 1.0f) * (2.0f * f - 1.0f);
					}
				}
				speed *= 1.0f + (k_SpeedUpDuringFight - 1.0f) * fight;
			}
			symbol.phase += k_PhaseSpeed * k_Step;
			symbol.a1 += (0.5f + 0.25f * (1.0f + std::cos(symbol.phase))) * speed * k_Step * 0.846f;
			symbol.a2 += speed * k_Step;
			const glm::vec3 position =
			    base + s * glm::vec3(radius * std::cos(symbol.a2) * std::cos(symbol.a1),
			                         radius * std::sin(symbol.a1) + (1.0f - fight) * static_cast<float>(rank) * k_HeightPerLevel,
			                         radius * std::sin(symbol.a2) * std::cos(symbol.a1));

			// PlayerSymbolSprite::Draw 0x69D7E0: two glows then the symbol, all additive billboards
			const float size = 1.5f * scale;
			const uint32_t rgb = k_PlayerColours[static_cast<size_t>(player) % k_PlayerColours.size()];
			const std::array<uint8_t, 3> colour = {static_cast<uint8_t>(rgb >> 16), static_cast<uint8_t>(rgb >> 8),
			                                       static_cast<uint8_t>(rgb)};
			const glm::mat3 still(1.0f);
			const float spin = seconds * 2.0f;
			const glm::mat3 spun = glm::mat3(glm::rotate(glm::mat4(1.0f), -spin, glm::vec3(0.0f, 1.0f, 0.0f)));
			drawable.atoms.push_back({&GlowCreator(), position, still, 1.5f * size, 1.0f, 99.0f, -seconds * 20.0f, colour});
			drawable.atoms.push_back({&GlowCreator(), position, spun, 1.5f * size, 1.0f, 99.0f, -seconds * 23.0f, {255, 255, 255}});
			drawable.atoms.push_back({&SymbolCreator(player), position, still, size, 1.0f, 255.0f, 0.0f, colour});
		}
		if (!drawable.atoms.empty())
		{
			out.push_back(std::move(drawable));
		}
	});
}
