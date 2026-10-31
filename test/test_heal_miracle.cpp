/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The heal miracle's search for the people it heals, pinned to the original game: the map cells are walked along the
// map's spiral from the cast cell, ceil(2R / 10) squared of them (a 2 by 2 block for the plain heal's 10 m); each cell's
// people are measured from the cast point moved into that cell, on the flat and strictly inside the reach; within a
// cell the newest arrivals come first, up to the most it heals; trees are never healed. Healing a villager is a good
// deed for the caster. The rules are the original's (docs/wiki/magic/food-wood-water-heal.md#heal,
// docs/wiki/magic/spell-core.md, the heal's search; docs/wiki/magic/influence-and-alignment.md#player-alignment)

// The test builds its own world lists
#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "ECS/Components/Alignment.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/CastRules.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/SpellLink.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using ecs::components::Transform;

namespace
{
using Entities = std::vector<entt::entity>;

/// The plain heal's reach (its row's 10 m)
constexpr float k_PlainReach = 10.0f;
/// The cast point, in the cell (10, 10), half a cell in from its corner
constexpr glm::vec3 k_Cast {105.0f, 0.0f, 105.0f};

/// Living things in the map's cells, on no land, with the world lists the cells need
class HealMiracle: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		// the map's side is the default 512 cells without a land
		Locator::terrainSystem::reset();
		test::EmplaceMapAndVillagerDefaults();
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		test::EmplaceWorldSystems();
		ecs::map_cells::Clear();
		ecs::object_index::OnLoadMap();
	}
	void TearDown() override
	{
		ecs::map_cells::Clear();
		Locator::entitiesRegistry::reset();
		test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// An object standing at a point, not in the map's cells
	static entt::entity Make(const glm::vec3& position)
	{
		const auto entity = Reg().Create();
		ecs::object_index::Assign(entity);
		Reg().Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
		return entity;
	}
	/// A lion (a living thing the heal heals) at a point, put at the head of its cell's mobile list
	static entt::entity LionAt(const glm::vec3& position)
	{
		const auto entity = Make(position);
		Reg().Assign<ecs::components::Animal>(entity).type = AnimalInfo::Lion;
		ecs::map_cells::InsertMapObject(entity);
		return entity;
	}

	/// Whom the heal finds round the cast point, no more than `most`
	static Entities Found(float reach, size_t most = 100) { return magic::cast_rules::HealTargetsAround(k_Cast, reach, most); }

private:
	test::RestoreService<Locator::terrainSystem> _restoreTerrain;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
};
} // namespace

// The spiral starts at the cast cell (10, 10) and steps west, south, east, east, north, north...: with the plain heal's
// ceil(20 / 10) squared = 4 cells it looks only in (10, 10), (9, 10), (9, 9) and (10, 9), so a lion 7 m east of the
// point is not healed; with 12 m, ceil(2.4) squared = 9 cells, the walk goes on through (11, 9), (11, 10), (11, 11),
// (10, 11) and (9, 11)
TEST_F(HealMiracle, ThePlainHealLooksOnlyInATwoByTwoBlockOfCellsWalkedOutwardFromTheCastCell)
{
	const auto inCell = LionAt({106.0f, 0.0f, 106.0f});
	const auto west = LionAt({97.0f, 0.0f, 105.0f});
	const auto southWest = LionAt({97.0f, 0.0f, 97.0f});
	const auto south = LionAt({105.0f, 0.0f, 97.0f});
	const auto east = LionAt({112.0f, 0.0f, 105.0f});
	const auto north = LionAt({105.0f, 0.0f, 112.0f});
	EXPECT_EQ(Found(k_PlainReach), (Entities {inCell, west, southWest, south}));
	EXPECT_EQ(Found(12.0f), (Entities {inCell, west, southWest, south, east, north}));
}

// Each cell's people are measured from the cast point moved into that cell along the spiral: a lion 14 m west of the
// cast point is 4 m from the point moved one cell west, and is healed by the plain heal's 10 m; so is one 14.5 m south
TEST_F(HealMiracle, EachCellsPeopleAreMeasuredFromTheCastPointMovedIntoThatCell)
{
	const auto farWest = LionAt({91.0f, 0.0f, 105.0f});
	const auto farSouth = LionAt({105.0f, 0.0f, 90.5f});
	EXPECT_EQ(Found(k_PlainReach), (Entities {farWest, farSouth}));
}

// The distance is on the flat, height ignored, and must be strictly less than the reach: with a 5 m reach (one cell),
// a lion 5 m away is not healed, one 4 m away is, and so is one 2 m away and 400 m up
TEST_F(HealMiracle, TheDistanceIsFlatAndStrictlyInsideTheReach)
{
	LionAt({100.0f, 0.0f, 105.0f});
	const auto inside = LionAt({101.0f, 0.0f, 105.0f});
	const auto high = LionAt({103.0f, 400.0f, 105.0f});
	EXPECT_EQ(Found(5.0f), (Entities {high, inside}));
}

// Within a cell the list is walked from its head, where the latest arrival goes, and the walk stops at the most
TEST_F(HealMiracle, WithinACellTheLatestArrivalsAreTakenFirstUpToTheMost)
{
	const auto first = LionAt({104.0f, 0.0f, 104.0f});
	const auto second = LionAt({106.0f, 0.0f, 104.0f});
	const auto third = LionAt({104.0f, 0.0f, 106.0f});
	EXPECT_EQ(Found(k_PlainReach, 2), (Entities {third, second}));
	EXPECT_EQ(Found(k_PlainReach, 3), (Entities {third, second, first}));
	EXPECT_EQ(Found(k_PlainReach, 1), (Entities {third}));
	EXPECT_TRUE(Found(k_PlainReach, 0).empty());
}

// A tree at the cast point, filed in its cell, is not healed; the lion beside it is
TEST_F(HealMiracle, ATreeIsNotHealed)
{
	const auto tree = Make({105.0f, 0.0f, 105.0f});
	Reg().Assign<ecs::components::Tree>(tree);
	ecs::map_cells::InsertMapObject(tree);
	const auto lion = LionAt({104.0f, 0.0f, 105.0f});
	EXPECT_EQ(Found(k_PlainReach), (Entities {lion}));
}

namespace
{
/// The tables the alignment change reads, put back when the test ends
class HealMiracleAlignment: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto info = std::make_unique<InfoConstants>();
		info->player.applyEffectAlignmentChangeAddition = 0.1f;
		info->alignment.at(k_HealRow).villager = 1.0f;
		Locator::infoConstants::reset(info.release());
	}

	/// The alignment table's row of the heal
	static constexpr size_t k_HealRow = 3;

private:
	test::RestoreService<Locator::infoConstants> _restoreInfo;
};
} // namespace

// A heal event's values heal by 1: a villager healed from 0.3 to full adds heal x the heal row's villager column x K
// to the caster's pending change, K = the life changed + the player info's addition; the good deed is boosted by half
// an evil caster's alignment and damped by half a good one's
TEST_F(HealMiracleAlignment, HealingAVillagerIsAGoodDeedForTheCaster)
{
	ecs::effects::EffectValues heal;
	heal.numbers.at(static_cast<size_t>(ecs::effects::EffectValues::Number::Heal)) = 1.0f;
	const ecs::effects::alignment::EffectTarget healed {.life = 1.0f, .type = AlignmentType::Villager, .burnDamage = 0.0f};
	const float k = (1.0f - 0.3f) + 0.1f;

	ecs::components::Alignment neutral {};
	ecs::effects::alignment::Update(neutral, healed, heal, 0.3f);
	EXPECT_FLOAT_EQ(neutral.pending, 1.0f * 1.0f * k);
	EXPECT_EQ(neutral.value, 0.0f);

	ecs::components::Alignment evil {.value = -0.5f, .pending = 0.0f};
	ecs::effects::alignment::Update(evil, healed, heal, 0.3f);
	EXPECT_FLOAT_EQ(evil.pending, (1.0f + 0.25f) * k);

	ecs::components::Alignment good {.value = 0.5f, .pending = 0.0f};
	ecs::effects::alignment::Update(good, healed, heal, 0.3f);
	EXPECT_FLOAT_EQ(good.pending, (1.0f - 0.25f) * k);

	// healing someone already at full life changes nothing
	ecs::components::Alignment unchanged {};
	ecs::effects::alignment::Update(unchanged, healed, heal, 1.0f);
	EXPECT_EQ(unchanged.pending, 0.0f);
}

namespace
{
/// The heal's chakra rule alone over a point creator, its chakras making bursts of a group the file does not have
const std::string k_ChakraFile =
    "BEGINPROPERTIES\n"
    "PROPERTY DeleteOnCloseDown BOOL 0\n"
    "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
    "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
    "ENDPROPERTIES\n"
    "BEGINCLASS ParticlePointCreator P0\nBEGINPROPERTIES\nPROPERTY InitialScale FLOAT 1\nENDPROPERTIES\nENDCLASS\n"
    "BEGINCLASS UR_HealSpellChakra C0\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
    "PROPERTY PCreator PERSIS_PNTR P0\nPROPERTY NextGroups ARRAY SIZE 1 1\n"
    "ENDPROPERTIES\nENDCLASS\n";

/// The heal spell behind an effect, as its rules see it: it keeps the heal events it is sent
class HealEvents final: public particles::SpellSink
{
public:
	int SpellEvent(const particles::SpellEventInfo& event) override
	{
		if (event.type == particles::SpellEventInfo::Type::Object)
		{
			healed.push_back(event.target);
		}
		return 1;
	}
	[[nodiscard]] int PowerUpLevel() const override { return -1; }

	Entities healed;
};

/// A registry with no hand, put back when the test ends
class HealMiracleChakras: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::handSystem::reset();
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		auto file = particles::File::Parse(k_ChakraFile, "test");
		ASSERT_TRUE(file.has_value());
		_file = std::make_shared<const particles::File>(std::move(*file));
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// A lion standing at a point
	static entt::entity LionAt(const glm::vec3& position)
	{
		const auto entity = Reg().Create();
		Reg().Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<ecs::components::Animal>(entity).type = AnimalInfo::Lion;
		return entity;
	}

	std::shared_ptr<const particles::File> _file;

private:
	test::RestoreService<Locator::handSystem> _restoreHand;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
};
} // namespace

// A heal's chakra heals each target it is given once, and a target that already has a chakra of another heal is passed
// over: two heals never work on the same person at once
TEST_F(HealMiracleChakras, TwoHealsNeverWorkOnTheSamePersonAtOnce)
{
	const auto both = LionAt({100.0f, 0.0f, 100.0f});
	const auto other = LionAt({104.0f, 0.0f, 100.0f});
	HealEvents firstSpell;
	HealEvents secondSpell;
	particles::Effect first(_file, k_Cast, 1.0f);
	particles::Effect second(_file, k_Cast, 1.0f);
	first.SetSink(&firstSpell);
	second.SetSink(&secondSpell);

	first.AddTarget(both);
	first.Step(0.1f);
	EXPECT_EQ(firstSpell.healed, (Entities {both}));

	// the second heal is given the same lion and another: only the other is healed by it
	second.AddTarget(both);
	second.AddTarget(other);
	second.Step(0.1f);
	EXPECT_EQ(secondSpell.healed, (Entities {other}));
	EXPECT_EQ(firstSpell.healed, (Entities {both}));
}
