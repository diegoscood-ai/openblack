/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A creature struck by a thrown thing (creature_struck::HurtCreature), against a fake world that records what it is
// asked to do: nothing for a toy; otherwise first the sway, kicked by the turn's force where the striking thing is, for
// any strike, however soft, and even for the local player's creature under a script's control, which takes nothing
// else; then the crush by how hard it was struck for its weight (at most 1, and 1 for a strike that compares with
// nothing), when above 0.005; the feedback only for its own player's throw; then its fear and its anger, in that order.
// No game data.

#include <cmath>

#include <array>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/AllMeshes.h"
#include "Creature/CreatureDesires.h"
#include "ECS/CreatureStruck.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/PhysicsHooksWorld.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "Magic/MagicWorldInterface.h"
#include "Physics/Body.h"
#include "Physics/LivingRules.h"

using namespace openblack;
namespace creature_struck = openblack::ecs::creature_struck;
namespace effects = openblack::ecs::effects;
namespace living = openblack::physics::living;
namespace sources = openblack::creature_desires::sources;

namespace
{
constexpr auto k_Crush = static_cast<size_t>(effects::EffectValues::Number::Crush);

/// One thing the creature was made to take, in the order it took them
struct Taken
{
	std::string what;
	float amount {0.0f};
	uint32_t source {0};
};

class FakeWorld final: public ecs::physics_hooks::World
{
public:
	[[nodiscard]] ecs::Registry& Entities() override { return registry; }
	[[nodiscard]] ecs::systems::HandSystemInterface& HandPart() override
	{
		throw std::logic_error("a struck creature does not need the hand");
	}
	[[nodiscard]] ecs::systems::BuildingDamageSystemInterface& BuildingDamage() override
	{
		throw std::logic_error("a struck creature does not need the broken buildings");
	}
	[[nodiscard]] PlayerNames LocalPlayer() const override { return local; }
	[[nodiscard]] bool IsToy(entt::entity object) const override
	{
		askedIfToy = true;
		return toys.contains(object);
	}
	[[nodiscard]] std::optional<effects::EffectValues> Crush() const override
	{
		if (!hasData)
		{
			return std::nullopt;
		}
		effects::EffectValues values;
		values.numbers.at(k_Crush) = 1.0f;
		return values;
	}
	void ApplyEffect(entt::entity object, const effects::EffectValues& values, const magic::EffectSource& source) override
	{
		EXPECT_EQ(object, k_Creature);
		taken.push_back({.what = "crush", .amount = values.numbers.at(k_Crush)});
		sourceGiven = source;
	}
	void UpdateAttitudeFromFeedback(entt::entity creature, float feedback) override
	{
		EXPECT_EQ(creature, k_Creature);
		taken.push_back({.what = "feedback", .amount = feedback});
	}
	void ChangeDesireSource(entt::entity creature, uint32_t source, float amount) override
	{
		EXPECT_EQ(creature, k_Creature);
		taken.push_back({.what = "source", .amount = amount, .source = source});
	}
	void KickSway(entt::entity creature, glm::vec3 force, glm::vec3 point) override
	{
		EXPECT_EQ(creature, k_Creature);
		kicks.push_back({.force = force, .point = point, .takenBefore = taken.size()});
	}

	/// A kick of the creature's sway, and how many things it had taken before it
	struct Kick
	{
		glm::vec3 force;
		glm::vec3 point;
		size_t takenBefore;
	};

	static constexpr auto k_Creature = static_cast<entt::entity>(7);

	ecs::Registry registry;
	PlayerNames local {PlayerNames::PLAYER_ONE};
	std::set<entt::entity> toys;
	bool hasData {true};
	mutable bool askedIfToy {false};
	std::vector<Taken> taken;
	std::vector<Kick> kicks;
	std::optional<magic::EffectSource> sourceGiven;
};

constexpr auto k_Rock = static_cast<entt::entity>(3);

/// The weight of a creature of size 1 with neither thin nor fat, weak nor strong a body: its mass times g
float Weight()
{
	return living::CreatureMass(1.0f, 0.0f, 0.0f) * openblack::physics::k_Gravity;
}

/// A rock the player's hand threw strikes the player's own creature, standing free, as hard as so many times its weight
creature_struck::Strike HandThrownRock(float timesItsWeight)
{
	return {
	    .struck = true,
	    .hitter = k_Rock,
	    .hitterCentre = {10.0f, 2.0f, 20.0f},
	    .hitterByPlayer = true,
	    .ownByPlayer = true,
	    .force = {300.0f, -50.0f, 0.0f},
	    .impact = timesItsWeight * Weight(),
	    .owner = PlayerNames::PLAYER_ONE,
	};
}
} // namespace

TEST(CreatureStruck, TheToysAreTheFiveToyModels)
{
	constexpr std::array k_Toys {MeshId::ObjectToyBall, MeshId::ObjectToyBowlingBall, MeshId::ObjectToyCuddly,
	                             MeshId::ObjectToyDice, MeshId::ObjectToySkittle};
	for (const auto mesh : k_Toys)
	{
		EXPECT_TRUE(creature_struck::IsToyModel(mesh)) << static_cast<uint32_t>(mesh);
	}
	EXPECT_FALSE(creature_struck::IsToyModel(static_cast<MeshId>(static_cast<uint32_t>(MeshId::ObjectToyBall) - 1)));
	EXPECT_FALSE(creature_struck::IsToyModel(static_cast<MeshId>(static_cast<uint32_t>(MeshId::ObjectToySkittle) + 1)));
}

TEST(CreatureStruck, ItTakesTheCrushItsPlayersFeedbackThenFearThenAnger)
{
	FakeWorld world;
	creature_struck::HurtCreature(world, FakeWorld::k_Creature, HandThrownRock(50.0f));
	// 50 times its weight, a hundredth of that: half the crush
	ASSERT_EQ(world.taken.size(), 4u);
	EXPECT_EQ(world.taken[0].what, "crush");
	EXPECT_FLOAT_EQ(world.taken[0].amount, 0.5f);
	EXPECT_EQ(world.taken[1].what, "feedback");
	EXPECT_FLOAT_EQ(world.taken[1].amount, -0.5f);
	EXPECT_EQ(world.taken[2].what, "source");
	EXPECT_EQ(world.taken[2].source, sources::k_FearFromDamage);
	EXPECT_EQ(world.taken[2].source, 18u);
	EXPECT_FLOAT_EQ(world.taken[2].amount, 0.5f);
	EXPECT_EQ(world.taken[3].what, "source");
	EXPECT_EQ(world.taken[3].source, sources::k_AngerFromDamage);
	EXPECT_EQ(world.taken[3].source, 9u);
	EXPECT_FLOAT_EQ(world.taken[3].amount, 0.5f);
	// The crush is a blow from the rock, put down to the player whose hand threw it, coming from the rock's centre
	ASSERT_TRUE(world.sourceGiven.has_value());
	EXPECT_EQ(world.sourceGiven->player, PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(world.sourceGiven->playerless);
	ASSERT_TRUE(world.sourceGiven->appliedBy.has_value());
	EXPECT_EQ(*world.sourceGiven->appliedBy, k_Rock);
	EXPECT_TRUE(world.sourceGiven->blow);
	ASSERT_TRUE(world.sourceGiven->point.has_value());
	EXPECT_EQ(*world.sourceGiven->point, glm::vec3(10.0f, 2.0f, 20.0f));
}

TEST(CreatureStruck, NothingStrikingItOrAToyDoesNothing)
{
	FakeWorld world;
	auto nothing = HandThrownRock(50.0f);
	nothing.struck = false;
	creature_struck::HurtCreature(world, FakeWorld::k_Creature, nothing);
	EXPECT_TRUE(world.taken.empty());
	EXPECT_TRUE(world.kicks.empty());

	world.toys.insert(k_Rock);
	creature_struck::HurtCreature(world, FakeWorld::k_Creature, HandThrownRock(50.0f));
	EXPECT_TRUE(world.askedIfToy);
	EXPECT_TRUE(world.taken.empty());
	EXPECT_TRUE(world.kicks.empty());
}

TEST(CreatureStruck, TheTurnsForceSwaysItWhereTheStrikingThingIsBeforeAnythingElse)
{
	FakeWorld world;
	creature_struck::HurtCreature(world, FakeWorld::k_Creature, HandThrownRock(50.0f));
	ASSERT_EQ(world.kicks.size(), 1u);
	EXPECT_EQ(world.kicks[0].force, glm::vec3(300.0f, -50.0f, 0.0f));
	EXPECT_EQ(world.kicks[0].point, glm::vec3(10.0f, 2.0f, 20.0f));
	EXPECT_EQ(world.kicks[0].takenBefore, 0u);
	EXPECT_EQ(world.taken.size(), 4u);

	// A strike too soft to hurt still sways it
	FakeWorld soft;
	creature_struck::HurtCreature(soft, FakeWorld::k_Creature, HandThrownRock(0.4f));
	EXPECT_EQ(soft.kicks.size(), 1u);
	EXPECT_TRUE(soft.taken.empty());

	// A thing that has gone sways it from where its body was
	FakeWorld gone;
	auto fromGone = HandThrownRock(50.0f);
	fromGone.hitter = entt::null;
	creature_struck::HurtCreature(gone, FakeWorld::k_Creature, fromGone);
	ASSERT_EQ(gone.kicks.size(), 1u);
	EXPECT_EQ(gone.kicks[0].point, glm::vec3(10.0f, 2.0f, 20.0f));
}

TEST(CreatureStruck, AThingThatHasGoneIsNotAskedAboutAndStillHurts)
{
	FakeWorld world;
	auto gone = HandThrownRock(50.0f);
	gone.hitter = entt::null;
	creature_struck::HurtCreature(world, FakeWorld::k_Creature, gone);
	EXPECT_FALSE(world.askedIfToy);
	ASSERT_EQ(world.taken.size(), 4u);
	ASSERT_TRUE(world.sourceGiven.has_value());
	ASSERT_TRUE(world.sourceGiven->appliedBy.has_value());
	EXPECT_EQ(*world.sourceGiven->appliedBy, entt::entity {entt::null});
}

TEST(CreatureStruck, TheLocalPlayersCreatureUnderAScriptIsNotHurtAtAll)
{
	FakeWorld world;
	auto scripted = HandThrownRock(50.0f);
	scripted.scriptControlled = true;
	creature_struck::HurtCreature(world, FakeWorld::k_Creature, scripted);
	EXPECT_TRUE(world.taken.empty());
	// It still sways from the blow
	EXPECT_EQ(world.kicks.size(), 1u);

	// Another player's creature under a script is hurt, frightened and angered, and gives no feedback
	scripted.owner = PlayerNames::PLAYER_TWO;
	creature_struck::HurtCreature(world, FakeWorld::k_Creature, scripted);
	ASSERT_EQ(world.taken.size(), 3u);
	EXPECT_EQ(world.taken[0].what, "crush");
	EXPECT_EQ(world.taken[1].source, sources::k_FearFromDamage);
	EXPECT_EQ(world.taken[2].source, sources::k_AngerFromDamage);

	// The player at this machine is the one asked about
	FakeWorld other;
	other.local = PlayerNames::PLAYER_TWO;
	creature_struck::HurtCreature(other, FakeWorld::k_Creature, scripted);
	EXPECT_TRUE(other.taken.empty());
}

TEST(CreatureStruck, OnlyItsOwnPlayersThrowGivesFeedback)
{
	// A throw that came to it through something nobody threw
	FakeWorld world;
	auto notThrown = HandThrownRock(50.0f);
	notThrown.hitterByPlayer = false;
	notThrown.ownByPlayer = false;
	creature_struck::HurtCreature(world, FakeWorld::k_Creature, notThrown);
	ASSERT_EQ(world.taken.size(), 3u);
	EXPECT_EQ(world.taken[0].what, "crush");
	EXPECT_EQ(world.taken[1].what, "source");
	EXPECT_EQ(world.taken[2].what, "source");
	// put down to no player
	ASSERT_TRUE(world.sourceGiven.has_value());
	EXPECT_TRUE(world.sourceGiven->playerless);
	EXPECT_EQ(world.sourceGiven->player, PlayerNames::NEUTRAL);

	// The player's hand threw it at another player's creature
	FakeWorld another;
	auto atAnother = HandThrownRock(50.0f);
	atAnother.owner = PlayerNames::PLAYER_TWO;
	creature_struck::HurtCreature(another, FakeWorld::k_Creature, atAnother);
	ASSERT_EQ(another.taken.size(), 3u);
	EXPECT_EQ(another.taken[1].what, "source");
	ASSERT_TRUE(another.sourceGiven.has_value());
	EXPECT_EQ(another.sourceGiven->player, PlayerNames::PLAYER_ONE);
}

TEST(CreatureStruck, TheCrushIsAHundredthOfTheKnockAndAtMostOne)
{
	FakeWorld world;
	creature_struck::HurtCreature(world, FakeWorld::k_Creature, HandThrownRock(100.0f));
	ASSERT_EQ(world.taken.size(), 4u);
	EXPECT_FLOAT_EQ(world.taken[0].amount, 1.0f);

	FakeWorld harder;
	creature_struck::HurtCreature(harder, FakeWorld::k_Creature, HandThrownRock(1000.0f));
	ASSERT_EQ(harder.taken.size(), 4u);
	for (const auto& taken : harder.taken)
	{
		EXPECT_FLOAT_EQ(std::abs(taken.amount), 1.0f) << taken.what;
	}

	// A bigger body weighs more: by its drawn size cubed, and by the shape it is to become
	FakeWorld bigger;
	auto onBigger = HandThrownRock(50.0f);
	onBigger.drawnSize = 2.0f;
	creature_struck::HurtCreature(bigger, FakeWorld::k_Creature, onBigger);
	ASSERT_EQ(bigger.taken.size(), 4u);
	EXPECT_FLOAT_EQ(bigger.taken[0].amount, 0.5f / 8.0f);

	FakeWorld fatter;
	auto onFatter = HandThrownRock(50.0f);
	onFatter.thinFat = 1.0f;
	onFatter.weakStrong = 1.0f;
	creature_struck::HurtCreature(fatter, FakeWorld::k_Creature, onFatter);
	ASSERT_EQ(fatter.taken.size(), 4u);
	EXPECT_NEAR(fatter.taken[0].amount, 0.5f / 1.3f, 1e-6f);
}

TEST(CreatureStruck, AStrikeOfHalfItsWeightOrLessDoesNothing)
{
	// At exactly half its weight the crush is 0.005, which does not count
	FakeWorld edge;
	auto atEdge = HandThrownRock(0.0f);
	atEdge.impact = 0.5f * Weight();
	creature_struck::HurtCreature(edge, FakeWorld::k_Creature, atEdge);
	EXPECT_TRUE(edge.taken.empty());

	FakeWorld below;
	creature_struck::HurtCreature(below, FakeWorld::k_Creature, HandThrownRock(0.4f));
	EXPECT_TRUE(below.taken.empty());

	FakeWorld above;
	creature_struck::HurtCreature(above, FakeWorld::k_Creature, HandThrownRock(0.6f));
	ASSERT_EQ(above.taken.size(), 4u);
	EXPECT_NEAR(above.taken[0].amount, 0.006f, 1e-6f);
}

TEST(CreatureStruck, AStrikeThatComparesWithNothingCountsAsTheMost)
{
	FakeWorld world;
	auto strange = HandThrownRock(0.0f);
	strange.impact = std::numeric_limits<float>::quiet_NaN();
	creature_struck::HurtCreature(world, FakeWorld::k_Creature, strange);
	ASSERT_EQ(world.taken.size(), 4u);
	EXPECT_FLOAT_EQ(world.taken[0].amount, 1.0f);
	EXPECT_FLOAT_EQ(world.taken[1].amount, -1.0f);
	EXPECT_FLOAT_EQ(world.taken[2].amount, 1.0f);
	EXPECT_FLOAT_EQ(world.taken[3].amount, 1.0f);
}

TEST(CreatureStruck, WithoutTheGamesDataItIsStillFrightenedAndAngered)
{
	FakeWorld world;
	world.hasData = false;
	creature_struck::HurtCreature(world, FakeWorld::k_Creature, HandThrownRock(50.0f));
	ASSERT_EQ(world.taken.size(), 3u);
	EXPECT_EQ(world.taken[0].what, "feedback");
	EXPECT_EQ(world.taken[1].source, sources::k_FearFromDamage);
	EXPECT_EQ(world.taken[2].source, sources::k_AngerFromDamage);
	EXPECT_FALSE(world.sourceGiven.has_value());
}
