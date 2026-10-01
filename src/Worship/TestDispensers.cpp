/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Mod test.miracle-dispensers: NOT in the original (a test aid, off by default). Everything here is openblack's own
// choice (mod); only the dispensers themselves are the original's (Worship/SpellDispenser.cpp).

#include "TestDispensers.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "Citadel.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TotemStatue.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Influence/Influence.h"
#include "ECS/MapCollide.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "EngineConfig.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/OneOffSpellSeed.h"
#include "Magic/Core/Players.h"
#include "Magic/MagicTables.h"
#include "Resources/ResourcesInterface.h"
#include "SpecialPoints.h"
#include "SpellDispenser.h"

using namespace openblack;
using namespace openblack::worship;
using namespace openblack::ecs::components;

namespace
{
/// The player miracle seeds (GSpellSeedInfo selectionGesture 1 = SPIRAL; the creature's M8 seeds 12..27 are left out),
/// in the seed table's order
constexpr std::array k_Seeds = {
    SpellSeedType::Storm,         SpellSeedType::Nature,         SpellSeedType::Fire,        SpellSeedType::Food,
    SpellSeedType::Shield,        SpellSeedType::PhysicalShield, SpellSeedType::LightningBolt, SpellSeedType::Heal,
    SpellSeedType::Wood,          SpellSeedType::Water,          SpellSeedType::FlockFlying, SpellSeedType::FlockGround,
    SpellSeedType::Teleport,      SpellSeedType::BeamExplosion,
};
/// The dispenser of the Land 1 challenge script (GiveSpellDispenserReward)
constexpr const char* k_DispenserAbode = "NORSE_ABODE_SPELL_DISPENSER";
/// Mod choices (openblack's): metres between two dispensers, the first ring's gap from the temple, the room kept
/// around buildings, trees and other objects, the half size of the patch that must be dry and how flat it must be
constexpr float k_Spacing = 13.0f;
constexpr float k_FirstGap = 12.0f;
constexpr float k_Clearance = 4.0f;
constexpr float k_PatchHalf = 4.0f;
constexpr float k_MaxRise = 2.5f;
constexpr int k_Rings = 8;
/// Without a mesh: the temple's radius
constexpr float k_DefaultTempleRadius = 20.0f;

/// Mod choice (openblack's): turns after the placement before the fire seed is tried, so the land script's intro
/// (Land 1 takes the hand and the camera) has started; it is tried again every turn while the hand is busy
constexpr uint32_t k_SeedDelayTurns = 10;
constexpr uint32_t k_SeedTries = 600;

bool g_Placed = false;
float g_Seconds = 0.0f;
std::vector<entt::entity> g_Dispensers;
entt::entity g_Empty = entt::null;
uint32_t g_SeedTurns = 0; ///< turns since the placement while the fire seed is still to be given (0: done or off)

auto& Registry()
{
	return Locator::entitiesRegistry::value();
}

PlayerNames HumanPlayer()
{
	for (int p = 0; p < static_cast<int>(PlayerNames::_COUNT); ++p)
	{
		if (magic::players::IsHuman(static_cast<PlayerNames>(p)))
		{
			return static_cast<PlayerNames>(p);
		}
	}
	return PlayerNames::PLAYER_ONE;
}

/// The larger half extent in x or z of the entity's mesh x its scale (Object::Get2DRadius 0x638180's formula), 0
/// without a mesh
float RadiusOf(entt::entity entity)
{
	const auto* mesh = Registry().TryGet<const Mesh>(entity);
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (mesh == nullptr || !meshes.Contains(mesh->id))
	{
		return 0.0f;
	}
	const auto half = meshes.Handle(mesh->id)->GetBoundingBox().Size() * 0.5f;
	const auto& scale = Registry().Get<const Transform>(entity).scale;
	return std::max(half.x * scale.x, half.z * scale.z);
}

struct Obstacle
{
	glm::vec2 centre;
	float radius;
};

std::vector<Obstacle> Obstacles()
{
	auto& registry = Registry();
	std::vector<Obstacle> obstacles;
	registry.Each<const Fixed>([&](entt::entity, const Fixed& fixed) {
		obstacles.push_back({fixed.boundingCenter, fixed.boundingRadius});
	});
	registry.Each<const Transform>([&](entt::entity entity, const Transform& transform) {
		if (registry.AnyOf<Abode, Temple, WorshipSite, Tree, Feature, AnimatedStatic, MobileStatic, MobileObject, Pot, Field,
		                   StreetLantern, TotemStatue>(entity))
		{
			obstacles.push_back({glm::vec2(transform.position.x, transform.position.z), std::max(RadiusOf(entity), 0.5f)});
		}
	});
	return obstacles;
}

bool IsOpenFlatDryLand(const glm::vec3& point, PlayerNames player, const std::vector<Obstacle>& obstacles)
{
	if (!ecs::sea_cells::InBounds(point))
	{
		return false;
	}
	float low = point.y;
	float high = point.y;
	for (const float dx : {-k_PatchHalf, 0.0f, k_PatchHalf})
	{
		for (const float dz : {-k_PatchHalf, 0.0f, k_PatchHalf})
		{
			glm::vec3 at(point.x + dx, 0.0f, point.z + dz);
			if (ecs::sea_cells::IsWater(at) || !ecs::sea_cells::IsDryLand(at))
			{
				return false;
			}
			at.y = GroundAt(at);
			low = std::min(low, at.y);
			high = std::max(high, at.y);
		}
	}
	if (high - low > k_MaxRise)
	{
		return false;
	}
	// cast rules (fn_005FB5D0): the player's influence > 0
	if (influence::CalculatePlayerInfluence(player, point) <= 0.0f)
	{
		return false;
	}
	const glm::vec2 xz(point.x, point.z);
	for (const auto& obstacle : obstacles)
	{
		if (glm::distance(xz, obstacle.centre) < obstacle.radius + k_Clearance)
		{
			return false;
		}
	}
	return ecs::map_collide::IsOkToCreateAtPos(point, "test.miracle-dispensers");
}

/// The magic types to place for a seed: its level (0 base, 1 PU one, 2 PU two, the highest it has when it has fewer),
/// or every level with level 3
std::vector<MagicType> MagicsOf(const GSpellSeedInfo& seed, int level)
{
	std::vector<MagicType> magics;
	if (level >= 3)
	{
		for (const auto magic : seed.magicTypes)
		{
			if (magic != MagicType::None && std::ranges::find(magics, magic) == magics.end())
			{
				magics.push_back(magic);
			}
		}
		return magics;
	}
	for (int l = std::clamp(level, 0, 3); l >= 0; --l)
	{
		const auto magic = seed.magicTypes.at(static_cast<size_t>(l));
		if (magic != MagicType::None)
		{
			magics.push_back(magic);
			break;
		}
	}
	return magics;
}

std::string Text(const std::array<char, 0x30>& text)
{
	return {text.data(), strnlen(text.data(), text.size())};
}

void Place(entt::entity temple, PlayerNames player)
{
	auto& registry = Registry();
	const auto& config = Locator::config::value();
	const auto& tables = Locator::infoConstants::value();
	const auto abode = GAbodeInfo::Find(k_DispenserAbode);

	std::vector<MagicType> magics;
	for (const auto seed : k_Seeds)
	{
		const auto& info = magic::GetSpellSeedInfo(tables, seed);
		if (info.exists == 0)
		{
			continue;
		}
		for (const auto magic : MagicsOf(info, config.testDispensersLevel))
		{
			magics.push_back(magic);
		}
	}

	const auto centre = registry.Get<const Transform>(temple).position;
	const float templeRadius = RadiusOf(temple) > 0.0f ? RadiusOf(temple) : k_DefaultTempleRadius;
	const auto obstacles = Obstacles();
	// one more place for the empty dispenser
	const size_t wanted = magics.size() + 1;
	std::vector<glm::vec3> spots;
	for (int ring = 0; ring < k_Rings && spots.size() < wanted; ++ring)
	{
		const float radius = templeRadius + k_FirstGap + static_cast<float>(ring) * k_Spacing;
		const int count = std::max(6, static_cast<int>(glm::two_pi<float>() * radius / k_Spacing));
		const float offset = static_cast<float>(ring) * 0.5f * glm::two_pi<float>() / static_cast<float>(count);
		for (int i = 0; i < count && spots.size() < wanted; ++i)
		{
			const float angle = offset + static_cast<float>(i) * glm::two_pi<float>() / static_cast<float>(count);
			glm::vec3 point(centre.x + radius * std::sin(angle), 0.0f, centre.z + radius * std::cos(angle));
			point.y = GroundAt(point);
			const bool spaced = std::ranges::none_of(spots, [&](const glm::vec3& spot) {
				return glm::distance(glm::vec2(spot.x, spot.z), glm::vec2(point.x, point.z)) < k_Spacing - 0.5f;
			});
			if (spaced && IsOpenFlatDryLand(point, player, obstacles))
			{
				spots.push_back(point);
			}
		}
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"),
	                   "Mod test.miracle-dispensers: temple at ({:.1f}, {:.1f}) radius {:.1f}, {} miracles, {} places, "
	                   "recharge {:.0f} s",
	                   centre.x, centre.z, templeRadius, magics.size(), spots.size(), config.testDispensersSeconds);

	// openblack: their creation indices from the mods' range (object_index::ModScope), so the original's keep theirs
	const ecs::object_index::ModScope scope;
	for (size_t i = 0; i < magics.size() && i < spots.size(); ++i)
	{
		const auto magic = magics[i];
		const auto& spot = spots[i];
		// as GiveSpellDispenserReward: CREATE_WITH_ANGLE_AND_SCALE(SPELL_DISPENSER, abode, pos, 1.0, angle),
		// SET_MAGIC_PROPERTIES(d, magic, seconds), SET_ACTIVE(d, 1); facing the temple
		const float yAngle = std::atan2(centre.x - spot.x, centre.z - spot.z);
		const auto dispenser = dispenser::Create(spot, abode, -1, yAngle, 1.0f);
		if (dispenser == entt::null)
		{
			continue;
		}
		dispenser::SetMagicProperties(dispenser, magic, config.testDispensersSeconds);
		dispenser::SetActive(dispenser, true);
		g_Dispensers.push_back(dispenser);
		const auto seed = magic::GetFirstSpellSeedForMagicType(tables, magic);
		int powerUp = -1;
		(void)magic::GetPowerUpGesture(magic::GetSpellSeedInfo(tables, seed), magic, &powerUp);
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Mod test.miracle-dispensers: dispenser {} seed {} ({}) pu {} magic {} ({}) at ({:.1f}, {:.1f})",
		                   static_cast<uint32_t>(dispenser), static_cast<int>(seed),
		                   Text(magic::GetSpellSeedInfo(tables, seed).debugString), powerUp, static_cast<int>(magic),
		                   Text(magic::GetMagicEffectInfo(tables, magic).debugString), spot.x, spot.z);
	}
	// the empty machine: created as CREATE(SPELL_DISPENSER) without SET_MAGIC_PROPERTIES nor SET_ACTIVE, so it is
	// inactive with no magic and never makes an orb (SpellDispenser::Process 0x722A70 needs it active)
	if (spots.size() > magics.size())
	{
		const auto& spot = spots[magics.size()];
		const float yAngle = std::atan2(centre.x - spot.x, centre.z - spot.z);
		g_Empty = dispenser::Create(spot, abode, -1, yAngle, 1.0f);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Mod test.miracle-dispensers: empty dispenser {} at ({:.1f}, {:.1f})",
		                   static_cast<uint32_t>(g_Empty), spot.x, spot.z);
	}
	if (spots.size() < magics.size())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Mod test.miracle-dispensers: room for only {} of the {} dispensers",
		                   spots.size(), magics.size());
	}
}
} // namespace

void test_dispensers::Reset()
{
	g_Placed = false;
	g_Dispensers.clear();
	g_Empty = entt::null;
	g_SeedTurns = 0;
}

namespace
{
/// The "seed" option: a fire seed into the human player's hand through the one-shot path
/// (OneOffSpellSeed::CreateSpellIntoHand 0x72A730, as OPENBLACK_TEST_SEED), once the dispensers are placed
void GiveFireSeed()
{
	if (g_SeedTurns == 0)
	{
		return;
	}
	if (!Locator::config::value().testDispensersSeed)
	{
		g_SeedTurns = 0;
		return;
	}
	if (++g_SeedTurns < k_SeedDelayTurns)
	{
		return;
	}
	const auto seed = magic::one_off::CreateSpellIntoHand(HumanPlayer(), SpellSeedType::Fire, -1, 1.0f);
	if (seed != entt::null || g_SeedTurns > k_SeedTries)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Mod test.miracle-dispensers: fire seed into the hand -> {}",
		                   seed == entt::null ? -1 : static_cast<int>(seed));
		g_SeedTurns = 0;
	}
}
} // namespace

void test_dispensers::ProcessTurn([[maybe_unused]] uint32_t turn)
{
	const auto& config = Locator::config::value();
	if (!config.testDispensers || !Locator::terrainSystem::has_value())
	{
		return;
	}
	if (!g_Placed)
	{
		const auto player = HumanPlayer();
		const auto temple = citadel::Of(player);
		if (temple == entt::null || !Registry().AllOf<Transform>(temple))
		{
			return;
		}
		g_Placed = true;
		g_Seconds = config.testDispensersSeconds;
		Place(temple, player);
		g_SeedTurns = 1;
		return;
	}
	GiveFireSeed();
	// the recharge option changed in the Mods menu: the new period on the placed ones
	if (g_Seconds != config.testDispensersSeconds)
	{
		g_Seconds = config.testDispensersSeconds;
		for (const auto entity : g_Dispensers)
		{
			dispenser::SetTimerTime(entity, g_Seconds);
		}
	}
}
