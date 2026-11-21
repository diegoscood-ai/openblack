/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The editor's miracles: the palette's dispensers are made by the miracles' service with the editor's turn, its bubbles
// float over the land, and every miracle a dispenser can give has a name

#define LOCATOR_IMPLEMENTATIONS

#include <cstring>

#include <memory>
#include <optional>

#include <fmt/format.h>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Registry.h"
#include "Editor/EditorEntities.h"
#include "Editor/EditorPalette.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/OneOffSpellSeed.h"
#include "Magic/DispenserRules.h"
#include "support/MagicFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using openblack::editor::PlaceItem;
using openblack::editor::PlaceKind;

namespace
{
/// The miracles' service as the editor sees it: it records the dispensers and bubbles it is asked for and hands back
/// an entity of its own for each
class RecordingMagicSystem final: public test::InertMagicSystem
{
public:
	struct DispenserCall
	{
		glm::vec3 position;
		MagicType type;
		float yAngleRadians;
	};
	struct BubbleCall
	{
		glm::vec3 position;
		MagicType type;
	};

	entt::entity CreateDispenser(glm::vec3 position, MagicType type, float yAngleRadians) override
	{
		dispenser = DispenserCall {.position = position, .type = type, .yAngleRadians = yAngleRadians};
		return made;
	}
	entt::entity CreateOneOffSeedFor(glm::vec3 position, MagicType type) override
	{
		bubble = BubbleCall {.position = position, .type = type};
		return made;
	}

	entt::entity made {static_cast<entt::entity>(7)};
	std::optional<DispenserCall> dispenser;
	std::optional<BubbleCall> bubble;
};

/// The miracles' service making a bubble as the game does: the first seed with the miracle, at the miracle's level in
/// it, in a registry of its own
class BubbleMakingMagicSystem final: public test::InertMagicSystem
{
public:
	BubbleMakingMagicSystem(ecs::Registry& land, const InfoConstants& info)
	    : _land(land)
	    , _info(info)
	{
	}

	entt::entity CreateOneOffSeedFor(glm::vec3 /*position*/, MagicType type) override
	{
		const auto seed = magic::one_off::SeedFor(_info, type);
		if (!seed.has_value())
		{
			return entt::null;
		}
		const auto orb = _land.Create();
		auto& bubble = _land.Assign<ecs::components::OneOffSpellSeed>(orb);
		bubble.seedType = seed->seed;
		bubble.powerUp = seed->powerUp;
		return orb;
	}

private:
	ecs::Registry& _land;
	const InfoConstants& _info;
};

/// Tables with two seeds: fire with its plain miracle and two power-ups, heal with one power-up and nothing past it
std::unique_ptr<InfoConstants> TwoSeedTables()
{
	// The tables are large: made on the heap, every miracle none
	auto info = std::make_unique<InfoConstants>();
	info->spellSeed.at(static_cast<size_t>(SpellSeedType::Fire)).magicTypes = {
	    MagicType::Fireball, MagicType::FireballPowerUpOne, MagicType::FireballPowerUpTwo, MagicType::None};
	info->spellSeed.at(static_cast<size_t>(SpellSeedType::Heal)).magicTypes = {MagicType::Heal, MagicType::HealPowerUpOne,
	                                                                           MagicType::None, MagicType::None};
	return info;
}

class EditorMiraclesTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_magic = &static_cast<RecordingMagicSystem&>(Locator::magicSystem::emplace<RecordingMagicSystem>());
	}

	RecordingMagicSystem* _magic {nullptr};

private:
	test::RestoreService<Locator::magicSystem> _restoreMagic;
};

TEST_F(EditorMiraclesTest, ADispenserIsMadeOfItsMiracleWithTheEditorsTurn)
{
	const glm::vec3 point {120.0f, 14.0f, 80.0f};
	const auto placed =
	    editor::Place(PlaceItem {.kind = PlaceKind::Dispenser, .type = static_cast<int32_t>(MagicType::Heal)}, point, 1.25f);

	EXPECT_EQ(placed, _magic->made);
	ASSERT_TRUE(_magic->dispenser.has_value());
	EXPECT_EQ(_magic->dispenser->type, MagicType::Heal);
	EXPECT_FLOAT_EQ(_magic->dispenser->yAngleRadians, 1.25f);
	// It stands where the editor puts it, on the land
	EXPECT_EQ(_magic->dispenser->position, point);
	EXPECT_FALSE(_magic->bubble.has_value());
}

TEST_F(EditorMiraclesTest, ABubbleOfItsMiracleFloatsThreeUnitsOverTheLand)
{
	const glm::vec3 point {120.0f, 14.0f, 80.0f};
	const auto placed = editor::Place(
	    PlaceItem {.kind = PlaceKind::MiracleBubble, .type = static_cast<int32_t>(MagicType::LightningBolt)}, point, 1.25f);

	EXPECT_EQ(placed, _magic->made);
	ASSERT_TRUE(_magic->bubble.has_value());
	EXPECT_EQ(_magic->bubble->type, MagicType::LightningBolt);
	EXPECT_EQ(_magic->bubble->position, glm::vec3(120.0f, 17.0f, 80.0f));
	EXPECT_FALSE(_magic->dispenser.has_value());
}

TEST(EditorMiracles, NothingIsPlacedWithoutTheMiraclesService)
{
	const test::RestoreService<Locator::magicSystem> restoreMagic;
	Locator::magicSystem::reset();
	const PlaceItem dispenser {.kind = PlaceKind::Dispenser, .type = static_cast<int32_t>(MagicType::Fireball)};
	const PlaceItem bubble {.kind = PlaceKind::MiracleBubble, .type = static_cast<int32_t>(MagicType::Fireball)};

	EXPECT_EQ(editor::Place(dispenser, glm::vec3(0.0f), 0.0f), entt::entity {entt::null});
	EXPECT_EQ(editor::Place(bubble, glm::vec3(0.0f), 0.0f), entt::entity {entt::null});
}

TEST(EditorMiracles, ADispenserOnTheLandIsAPaletteItemOfItsMiracle)
{
	ecs::Registry registry;
	const auto entity = registry.Create();
	registry.Assign<ecs::components::SpellDispenser>(entity).magicType = MagicType::Teleport;

	EXPECT_EQ(editor::KindOf(registry, entity), editor::EntityKind::Miracle);
	EXPECT_EQ(editor::ItemOf(registry, entity),
	          std::optional(PlaceItem {.kind = PlaceKind::Dispenser, .type = static_cast<int32_t>(MagicType::Teleport)}));
}

TEST(EditorMiracles, EveryDispensableMiracleIsNamedFromTheMagicTables)
{
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	// The tables are large: made on the heap, every name empty
	auto info = std::make_unique<InfoConstants>();
	// Each miracle a dispenser can give is spelled in the tables as the game's are
	for (const auto type : magic::DispensableMiracles())
	{
		const auto text = fmt::format("MIRACLE_{}", static_cast<uint32_t>(type));
		std::memcpy(info->magicEffect.at(static_cast<size_t>(type)).debugString.data(), text.data(), text.size());
	}
	Locator::infoConstants::emplace(*info);

	const auto names = editor::NameTables::Load();
	ASSERT_TRUE(names.has_value());
	ASSERT_FALSE(magic::DispensableMiracles().empty());
	for (const auto type : magic::DispensableMiracles())
	{
		const auto expected = fmt::format("Miracle {}", static_cast<uint32_t>(type));
		// Dispensers and bubbles share the miracles' names
		EXPECT_EQ(names->NameOf(PlaceKind::Dispenser, static_cast<int32_t>(type)), expected);
		EXPECT_EQ(names->NameOf(PlaceKind::MiracleBubble, static_cast<int32_t>(type)), expected);
	}
}

TEST(EditorMiracles, ABubbleGivesItsSeedsMiracleAtItsLevel)
{
	const auto info = TwoSeedTables();

	EXPECT_EQ(editor::MiracleOfBubble(*info, SpellSeedType::Fire, -1), std::optional(MagicType::Fireball));
	EXPECT_EQ(editor::MiracleOfBubble(*info, SpellSeedType::Fire, 1), std::optional(MagicType::FireballPowerUpTwo));
	EXPECT_EQ(editor::MiracleOfBubble(*info, SpellSeedType::Heal, 0), std::optional(MagicType::HealPowerUpOne));
	// A level the seed has no miracle at gives its plain one, as the seed casts it
	EXPECT_EQ(editor::MiracleOfBubble(*info, SpellSeedType::Heal, 1), std::optional(MagicType::Heal));
}

TEST(EditorMiracles, ASeedOfNoMiracleIsNotPlaceable)
{
	const auto info = TwoSeedTables();

	EXPECT_EQ(editor::MiracleOfBubble(*info, SpellSeedType::None, -1), std::nullopt);
	// A seed whose every level is none
	EXPECT_EQ(editor::MiracleOfBubble(*info, SpellSeedType::Water, -1), std::nullopt);
	// Levels a seed can't have
	EXPECT_EQ(editor::MiracleOfBubble(*info, SpellSeedType::Fire, -2), std::nullopt);
	EXPECT_EQ(editor::MiracleOfBubble(*info, SpellSeedType::Fire, 3), std::nullopt);
}

TEST(EditorMiracles, ABubbleOnTheLandIsAPaletteItemOfItsMiracle)
{
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	const auto info = TwoSeedTables();
	Locator::infoConstants::emplace(*info);
	ecs::Registry registry;
	const auto fire = registry.Create();
	auto& fireBubble = registry.Assign<ecs::components::OneOffSpellSeed>(fire);
	fireBubble.seedType = SpellSeedType::Fire;
	fireBubble.powerUp = 0;
	const auto none = registry.Create();
	registry.Assign<ecs::components::OneOffSpellSeed>(none);

	EXPECT_EQ(editor::KindOf(registry, fire), editor::EntityKind::Miracle);
	EXPECT_EQ(editor::ItemOf(registry, fire),
	          std::optional(
	              PlaceItem {.kind = PlaceKind::MiracleBubble, .type = static_cast<int32_t>(MagicType::FireballPowerUpOne)}));
	// A bubble of no seed is a miracle on the land, but not one the palette can place
	EXPECT_EQ(editor::KindOf(registry, none), editor::EntityKind::Miracle);
	EXPECT_EQ(editor::ItemOf(registry, none), std::nullopt);
}

TEST(EditorMiracles, ABubbleTheEditorPlacedComesBackAsTheSameItem)
{
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	const test::RestoreService<Locator::magicSystem> restoreMagic;
	const auto info = TwoSeedTables();
	Locator::infoConstants::emplace(*info);
	ecs::Registry land;
	Locator::magicSystem::emplace<BubbleMakingMagicSystem>(land, *info);

	for (const auto type : {MagicType::Fireball, MagicType::FireballPowerUpOne, MagicType::FireballPowerUpTwo, MagicType::Heal,
	                        MagicType::HealPowerUpOne})
	{
		const PlaceItem item {.kind = PlaceKind::MiracleBubble, .type = static_cast<int32_t>(type)};
		const auto placed = editor::Place(item, glm::vec3(0.0f), 0.0f);

		ASSERT_NE(placed, entt::entity {entt::null});
		EXPECT_EQ(editor::ItemOf(land, placed), std::optional(item)) << static_cast<uint32_t>(type);
	}
}
} // namespace
