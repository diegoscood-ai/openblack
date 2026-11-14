/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The player's hand picking up and letting go of the things of the testbed scene objects.pickup_kinds (its rock and its
// birch) and of a pile of food, in a small world of the game's own systems and data (support/WorldHarness.h): the real
// hand, driven by the packets its own presses and releases send, over the real physics and map cells. What is picked up
// leaves the physics and the map, a tree pulled up creaks and counts as an evil act, a pile gives a handful (poisoned if
// it was), what is let go of leaves the hand as the hand held it, only lifted out of the ground, is offered to the
// creatures to catch while it flies, and is kept as the last thing the player let go of; the help profile counts the
// pick-ups and throws. Every expected value is the original's (docs/wiki/hand/grab-hold-and-throw.md,
// docs/wiki/hand/states.md, docs/wiki/project/progress/hand/).

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/L3DMesh.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Audio/Game/BankTables.h"
#include "Common/EventManager.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureDeeds.h"
#include "Creature/CreatureObjectActions.h"
#include "ECS/Archetypes/MobileObjectArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/PlayerArchetype.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/TreeRoots.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Events/CreatureMimicEvents.h"
#include "ECS/LastDropped.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectFlags.h"
#include "ECS/ObjectResources.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/PhysicsGameHooks.h"
#include "ECS/Systems/Implementations/CreatureObjectActionSystem.h"
#include "ECS/Systems/Implementations/HandSystem.h"
#include "ECS/Systems/Implementations/RewardSystem.h"
#include "ECS/ToBeDeleted.h"
#include "GameClock.h"
#include "Help/HelpProfile.h"
#include "Input/GamePackets.h"
#include "Physics/Body.h"
#include "Physics/ObjectRules.h"
#include "support/RecordingSoundTags.h"
#include "support/RestoreService.h"
#include "support/WorldHarness.h"

using namespace openblack;
using ecs::components::Pot;
using ecs::components::Transform;
using HelpEvent = openblack::help_profile::Event;

namespace
{
constexpr std::string_view k_Scene = "objects.pickup_kinds";
/// The scene's objects, by their place in its list
constexpr size_t k_SceneRock = 1;
constexpr size_t k_SceneBirch = 3;
/// Where the scene's objects stand, about this point
const glm::vec2 k_Over {1000.0f, 1200.0f};
/// A throw: faster across the land than a let-go (more than 2 metres a second)
const glm::vec3 k_Throw {-12.0f, 3.0f, 2.0f};
/// The hand's angles as it lets go of a thing (x, y, z)
const glm::vec3 k_HandAngles {0.3f, 1.2f, -0.4f};
/// The change of a tree pulled up or put down: the player info row of info.dat
constexpr float k_TreePullPut = 0.005f;
/// The first handful from a pile: the hand pot's row of info.dat
constexpr uint32_t k_FirstHandful = 25;
/// A local random seed, and the tree-break sample the original draws from it
constexpr uint32_t k_CreakSeed = 0xE6F4C8D6u;
constexpr int k_CreakSample = 33;

/// The game's own kinds of thing in the physics, with the things offered to the creatures to catch kept in order
class CatchOffers final: public ecs::PhysicsGameHooks
{
public:
	void OfferToCatchingCreatures(ecs::systems::DynamicsSystemInterface& dynamics, entt::entity object,
	                              ecs::PhysicsEntry& entry) override
	{
		offers.push_back(object);
		PhysicsGameHooks::OfferToCatchingCreatures(dynamics, object, entry);
	}

	std::vector<entt::entity> offers;
};

class HandPickUpWorld: public test::world::WorldHarness
{
protected:
	[[nodiscard]] static const testbed_scenarios::ObjectSetup& SceneObject(size_t index)
	{
		return Scene(k_Scene).objects.at(index);
	}
	[[nodiscard]] static MobileStaticInfo RockType() { return std::get<MobileStaticInfo>(SceneObject(k_SceneRock).type); }
	[[nodiscard]] static TreeInfo BirchType() { return std::get<TreeInfo>(SceneObject(k_SceneBirch).type); }
	/// Where a scene object stands, moved along the land by `shift`
	[[nodiscard]] static glm::vec3 Place(size_t index, glm::vec2 shift = glm::vec2(0.0f))
	{
		const auto at = k_Over + SceneObject(index).offset + shift;
		return {at.x, k_Ground, at.y};
	}

	void SetUp() override
	{
		WorldHarness::SetUp();
		if (IsSkipped() || HasFatalFailure())
		{
			return;
		}
		if (!GameData())
		{
			FillSyntheticRows();
		}
		_tags = &static_cast<test::RecordingSoundTags&>(Locator::soundTagSystem::emplace<test::RecordingSoundTags>());
		auto hooks = std::make_unique<CatchOffers>();
		_offers = &hooks->offers;
		Dynamics().SetClassHooks(std::move(hooks));
		help_profile::Reset();
		static_cast<void>(ecs::archetypes::PlayerArchetype::Create(PlayerNames::PLAYER_ONE));
		ecs::effects::alignment::Of(PlayerNames::PLAYER_ONE) = {};
		game_packets::Reset();
		game_clock::SetTurn(1);
		// the hand as the game starts it: its packets' handlers, its taps and the physics' hand parts; in the locator, as
		// the game has it, so the map's cells leave out what it holds when a reaction's spread rebuilds them
		_hand = &static_cast<ecs::systems::HandSystem&>(Locator::handSystem::emplace<ecs::systems::HandSystem>());
		static_cast<void>(_hand->Initialize());
		_handReady = true;
	}

	void TearDown() override
	{
		if (_handReady)
		{
			_hand = nullptr;
			Locator::handSystem::reset();
			game_packets::Reset();
			game_clock::SetTurn(0);
			help_profile::Reset();
			ecs::effects::alignment::Of(PlayerNames::PLAYER_ONE) = {};
		}
		WorldHarness::TearDown();
	}

	[[nodiscard]] ecs::systems::HandSystem& Hand() const { return *_hand; }
	[[nodiscard]] test::RecordingSoundTags& Tags() const { return *_tags; }
	[[nodiscard]] std::vector<entt::entity>& Offers() const { return *_offers; }

	/// The scene's rock where it stands, or moved along the land
	[[nodiscard]] entt::entity Rock(glm::vec2 shift = glm::vec2(0.0f)) const
	{
		const auto type = RockType();
		// on synthetic data a box 2 m wide and 1.5 m tall, standing on its base
		LoadMesh(Info().mobileStatic.at(static_cast<size_t>(type)).meshId, glm::vec3(-1.0f, 0.0f, -1.0f),
		         glm::vec3(1.0f, 1.5f, 1.0f));
		return ecs::archetypes::MobileStaticArchetype::CreateFromInfo(Place(k_SceneRock, shift), type, 0.0f, 0.0f,
		                                                              SceneObject(k_SceneRock).scale);
	}

	/// The scene's birch, grown, rooted where it stands (or at `at`)
	[[nodiscard]] entt::entity Birch(std::optional<glm::vec3> at = std::nullopt) const
	{
		const auto type = BirchType();
		// on synthetic data a trunk 1 m across and 8 m tall
		LoadMesh(Info().tree.at(static_cast<size_t>(type)).normal, glm::vec3(-0.5f, 0.0f, -0.5f), glm::vec3(0.5f, 8.0f, 0.5f));
		const float scale = SceneObject(k_SceneBirch).scale;
		return ecs::archetypes::TreeArchetype::Create(0, at.value_or(Place(k_SceneBirch)), type, false, 0.0f, scale, scale);
	}

	/// A pile of food on the land in front of the scene's rock
	[[nodiscard]] entt::entity FoodPile(int32_t amount, bool poisoned) const
	{
		for (const auto type : {PotInfo::FoodPile, PotInfo::HandFood})
		{
			LoadMesh(Info().pot.at(static_cast<size_t>(type)).meshId, glm::vec3(-1.0f, 0.0f, -1.0f),
			         glm::vec3(1.0f, 0.5f, 1.0f));
		}
		const auto pile =
		    ecs::archetypes::PotArchetype::Create(Place(k_SceneRock, glm::vec2(0.0f, -8.0f)), 0.0f, PotInfo::FoodPile, amount);
		Registry().Get<Pot>(pile).poisoned = poisoned;
		return pile;
	}

	/// Lifted above the land and flying at the velocity
	void Hurl(entt::entity thing, float height, glm::vec3 velocity) const
	{
		auto& transform = Registry().Get<Transform>(thing);
		transform.position.y = k_Ground + height;
		ASSERT_NE(Dynamics().AddObject(thing, velocity, glm::vec3(0.0f)), nullptr);
	}

	/// The turn's packets applied, as the next turn applies them first thing
	static void Dispatch()
	{
		game_packets::Flush();
		game_packets::DispatchQueuedPackets();
	}

	/// The hand's press on a thing: its place-in-hand packet (the one the tug sends too, once a tree comes free)
	static void PickUp(entt::entity thing)
	{
		game_packets::Push({game_packets::Type::PlaceInHand, thing});
		Dispatch();
	}

	/// The hand's press on a pile: the synced hand over it, then the locked select's start
	static void Scoop(entt::entity pile)
	{
		game_packets::Packet hand {game_packets::Type::Hand};
		const auto at = Registry().Get<const Transform>(pile).position;
		hand.coords = map_coords::FromWorld(at);
		hand.data = {at.x, at.y, at.z};
		game_packets::Push(hand);
		game_packets::Push({game_packets::Type::StartLockedSelect, pile});
		Dispatch();
	}

	/// The hand's release: the throw data (the velocity, no spin, where the hand holds the thing and its angles), then
	/// the apply to the map point under it
	static void LetGo(glm::vec3 velocity, glm::vec3 from, glm::vec3 angles)
	{
		game_packets::Packet data {game_packets::Type::ThrowData};
		data.data = {velocity.x, velocity.y, velocity.z, 0.0f,     0.0f,     0.0f,
		             from.x,     from.y,     from.z,     angles.x, angles.y, angles.z};
		game_packets::Push(data);
		game_packets::Push({game_packets::Type::ApplyToMapCoord, entt::null, glm::vec3(from.x, k_Ground, from.z)});
		Dispatch();
	}

	/// A body's lowest point, its points placed by its axes about its centre
	[[nodiscard]] static float Lowest(const physics::Body& body)
	{
		const auto& axes = body.Axes();
		float lowest = 1000.0f;
		for (const auto& point : body.Points())
		{
			const float y =
			    ((axes[1].y * point.local.y + point.local.x * axes[0].y) + axes[2].y * point.local.z) + body.Centre().y;
			lowest = std::min(lowest, y);
		}
		return lowest;
	}

	/// A reaction type whose info says it ends when its initiator is put in a hand, and one whose info says it does not
	[[nodiscard]] std::pair<Reaction, Reaction> EndingAndLastingReactions() const
	{
		std::optional<Reaction> ending;
		std::optional<Reaction> lasting;
		for (size_t type = 0; type < Info().reaction.size(); ++type)
		{
			// not a flying thing's: a catch out of the air ends those on its own
			if (static_cast<Reaction>(type) == Reaction::ReactToFlyingObject)
			{
				continue;
			}
			auto& kept = Info().reaction.at(type).whetherReactionFinishesIfInitiatorInHand != 0 ? ending : lasting;
			if (!kept.has_value())
			{
				kept = static_cast<Reaction>(type);
			}
		}
		EXPECT_TRUE(ending.has_value() && lasting.has_value());
		return {ending.value_or(Reaction::LookAtObject), lasting.value_or(Reaction::FollowObject)};
	}

private:
	/// The rows the synthetic world's things read: a rock of the rock kind, a birch, a pile of food and the hand's
	/// handful of food, the player's tree change and one reaction that ends in the hand
	void FillSyntheticRows() const
	{
		auto& rock = Info().mobileStatic.at(static_cast<size_t>(RockType()));
		rock.mobileType = static_cast<MobileStaticInfo>(2);
		rock.meshId = MeshId::ObjectRockChalk;
		rock.weight = 400.0f;
		Info().tree.at(static_cast<size_t>(BirchType())).normal = MeshId::TreeBirch;
		auto& pile = Info().pot.at(static_cast<size_t>(PotInfo::FoodPile));
		pile.meshId = MeshId::I_Food;
		pile.potType = PotType::PileFood;
		pile.resourceType = ResourceType::Food;
		pile.maxAmountInPot = 20000;
		auto& handful = Info().pot.at(static_cast<size_t>(PotInfo::HandFood));
		handful.meshId = MeshId::I_OfferingFood;
		handful.potType = PotType::PileFood;
		handful.resourceType = ResourceType::Food;
		handful.maxAmountInPot = 20000;
		handful.canBecomeAPhysicsObject = 1;
		handful.amountPickedUpInitially = k_FirstHandful;
		Info().player.treePullPutAlignmentChange = k_TreePullPut;
		Info().reaction.at(static_cast<size_t>(Reaction::LookAtObject)).whetherReactionFinishesIfInitiatorInHand = 1;
	}

	test::RestoreService<Locator::soundTagSystem> _restoreTags;
	test::RecordingSoundTags* _tags {nullptr};
	std::vector<entt::entity>* _offers {nullptr};
	ecs::systems::HandSystem* _hand {nullptr};
	bool _handReady {false};
};
} // namespace

// The hand's pick-up counts as a pick-up in the help profile, never as a catch, even of a thing flying through the air:
// the catch is told by the thing being in the physics, which it has already left when it is told
TEST_P(HandPickUpWorld, APickUpCountsAsAPickUpNeverAsACatchEvenOutOfTheAir)
{
	const auto rock = Rock();
	Hurl(rock, 10.0f, glm::vec3(-5.0f, 2.0f, 0.0f));
	ASSERT_NE(Dynamics().Find(rock), nullptr);
	PickUp(rock);
	ASSERT_EQ(Hand().GetHeldObject(), std::optional(rock));
	EXPECT_EQ(help_profile::Get(HelpEvent::PickUp).Total(), 1u);
	EXPECT_EQ(help_profile::Get(HelpEvent::Catch).Total(), 0u);
}

// Each thing that goes into the hand makes the hand's pick-up reaction, for the hand's player and started at once,
// placed at the place the last hand sync gave rather than where the hand is drawn; a press that puts nothing in the
// hand makes none
TEST_P(HandPickUpWorld, APickUpMakesTheHandsPickUpReactionAtItsSyncedPlace)
{
	ecs::systems::RewardSystem rewards;
	const auto chest = rewards.Create(Place(k_SceneRock, glm::vec2(0.0f, 8.0f)), RewardObjectInfo::SmallFood,
	                                  PlayerNames::PLAYER_ONE, entt::null, false);
	PickUp(chest);
	ASSERT_FALSE(Hand().GetHeldObject().has_value());
	EXPECT_EQ(Hand().GetPickUpReaction(), 0u);
	// the synced place 30 m along the land from the rock, a rock standing there
	const auto far = Rock(glm::vec2(30.0f, 0.0f));
	const auto syncedAt = Registry().Get<const Transform>(far).position;
	game_packets::Packet hand {game_packets::Type::Hand};
	hand.coords = map_coords::FromWorld(syncedAt);
	hand.data = {syncedAt.x, syncedAt.y, syncedAt.z};
	game_packets::Push(hand);
	Dispatch();
	game_clock::SetTurn(7);
	const auto rock = Rock();
	PickUp(rock);
	ASSERT_EQ(Hand().GetHeldObject(), std::optional(rock));
	const auto id = Hand().GetPickUpReaction();
	const auto* reaction = ecs::effects::reactions::Find(id);
	ASSERT_NE(reaction, nullptr);
	EXPECT_EQ(reaction->type, Reaction::ReactToHandPickUp);
	EXPECT_EQ(reaction->initiator, Hand().GetPlayerHands()[0]);
	EXPECT_EQ(reaction->player, std::optional(PlayerNames::PLAYER_ONE));
	EXPECT_EQ(reaction->turnCreated, 7u);
	// placed at the synced place: the rock standing there is in its cell, the one held 30 m away is not
	EXPECT_LT(ecs::effects::reactions::DistanceToReactionCell(far, *reaction), 10.0f);
	EXPECT_GT(ecs::effects::reactions::DistanceToReactionCell(rock, *reaction), 20.0f);
	// one reaction for each thing put in the hand
	LetGo(k_Throw, Place(k_SceneRock) + glm::vec3(0.0f, 6.0f, 0.0f), k_HandAngles);
	PickUp(far);
	EXPECT_EQ(Hand().GetHeldObject(), std::optional(far));
	EXPECT_NE(Hand().GetPickUpReaction(), id);
	EXPECT_NE(ecs::effects::reactions::Find(Hand().GetPickUpReaction()), nullptr);
}

// A rooted tree pulled up into the hand creaks once, one of the three tree-break samples drawn from the local random
// stream, a point sound in the game's bank of the 3D kind that plays at once; and it counts as an evil act of the
// player's: minus the player info's tree change, weighed by the alignment, waits to be taken in
TEST_P(HandPickUpWorld, PullingUpATreeCreaksAndCountsAsAnEvilAct)
{
	EXPECT_EQ(Info().player.treePullPutAlignmentChange, k_TreePullPut);
	const auto birch = Birch();
	Locator::gameRandom::value().Load({.synced = game_random::k_InitialSeed, .local = k_CreakSeed});
	PickUp(birch);
	ASSERT_EQ(Hand().GetHeldObject(), std::optional(birch));
	ASSERT_EQ(Tags().points.size(), 1u);
	const auto& creak = Tags().points.front();
	EXPECT_EQ(creak.sample, k_CreakSample);
	EXPECT_EQ(creak.mode, 3);
	EXPECT_EQ(creak.loops, 0);
	EXPECT_FALSE(creak.extra3DFlag);
	EXPECT_TRUE(creak.is3D);
	EXPECT_EQ(creak.bank, audio::SfxBank::InGame);
	EXPECT_EQ(creak.delay, 0);
	const auto& alignment = ecs::effects::alignment::Of(PlayerNames::PLAYER_ONE);
	EXPECT_EQ(alignment.pending, -k_TreePullPut);
	EXPECT_EQ(alignment.value, 0.0f);
}

// The creak sounds at the tree's map coordinates, as the original keeps them: a land script's tree stands on the metres
// of its map units, a replanted tree on its grid point, so its place is the original's point bit for bit. The
// original's numbers: a Land 1 tree's map units (8757182, 15746990) give (1336.24, 2402.80) as 0x44A707AE, 0x45162CCD;
// (8771142, 15700000) give 0x44A74BD8, 0x4515BA14. Read back from those metres the map units would come out one more
// (8757183, 8771143), so the creak is not worked out again from the metres
TEST_P(HandPickUpWorld, ATreeCreaksAtItsMapCoordinates)
{
	struct Case
	{
		int32_t x;
		int32_t z;
		uint32_t pointX;
		uint32_t pointZ;
	};
	constexpr std::array<Case, 2> k_Cases {
	    {{8757182, 15746990, 0x44A707AEu, 0x45162CCDu}, {8771142, 15700000, 0x44A74BD8u, 0x4515BA14u}}};
	for (const auto& c : k_Cases)
	{
		ASSERT_NE(map_coords::ToFixed(map_coords::ToMetres(c.x)), c.x);
		// a land script's tree: the metres of its map units
		const auto birch = Birch(glm::vec3(map_coords::ToMetres(c.x), k_Ground, map_coords::ToMetres(c.z)));
		Tags().points.clear();
		PickUp(birch);
		ASSERT_EQ(Tags().points.size(), 1u);
		EXPECT_EQ(std::bit_cast<uint32_t>(Tags().points.front().point.x), c.pointX);
		EXPECT_EQ(std::bit_cast<uint32_t>(Tags().points.front().point.z), c.pointZ);
		EXPECT_EQ(Tags().points.front().point.y, k_Ground);
		LetGo(k_Throw, Place(k_SceneBirch) + glm::vec3(0.0f, 6.0f, 0.0f), glm::vec3(0.0f));
	}
	// a replanted tree: where it comes to rest, put on the map's grid
	const auto rest = glm::vec3(map_coords::ToMetres(k_Cases[0].x) + 0.0001f, k_Ground, map_coords::ToMetres(k_Cases[0].z));
	const auto planted = physics::objects::PlacedOnTheMap(&Locator::terrainSystem::value(), rest);
	const auto birch = Birch(planted);
	Tags().points.clear();
	PickUp(birch);
	ASSERT_EQ(Tags().points.size(), 1u);
	const auto units = map_coords::ToFixed(rest.x);
	EXPECT_EQ(Tags().points.front().point.x, map_coords::ToMetres(units));
	EXPECT_EQ(Tags().points.front().point.z, map_coords::ToMetres(map_coords::ToFixed(rest.z)));
}

// A pile gives the hand a handful of what it holds: a hand pot of food of the hand pot's first amount, taken from the
// pile, which stays on the land with the rest
TEST_P(HandPickUpWorld, APileGivesAHandfulOfWhatItHolds)
{
	EXPECT_EQ(Info().pot.at(static_cast<size_t>(PotInfo::HandFood)).amountPickedUpInitially, k_FirstHandful);
	const auto pile = FoodPile(100, false);
	Scoop(pile);
	const auto held = Hand().GetHeldObject();
	ASSERT_TRUE(held.has_value());
	ASSERT_NE(*held, pile);
	const auto& handful = Registry().Get<const Pot>(*held);
	EXPECT_EQ(handful.type, PotInfo::HandFood);
	EXPECT_EQ(handful.amount, k_FirstHandful);
	EXPECT_FALSE(handful.poisoned);
	EXPECT_EQ(Registry().Get<const Pot>(pile).amount, 100u - k_FirstHandful);
}

// The hand's record of who owned what it last picked up lasts from one land to the next and goes only with a new game,
// as the interface status it belongs to is made once a game
TEST_P(HandPickUpWorld, WhoOwnedWhatTheHandLastPickedUpIsForgottenOnlyAtANewGame)
{
	EXPECT_EQ(Hand().GetSourceOwner(), std::nullopt);
	Scoop(FoodPile(100, false));
	ASSERT_TRUE(Hand().GetHeldObject().has_value());
	// a loose pile is the neutral player's
	EXPECT_EQ(Hand().GetSourceOwner(), std::optional(PlayerNames::NEUTRAL));
	Hand().ResetTurnState(false);
	EXPECT_EQ(Hand().GetSourceOwner(), std::optional(PlayerNames::NEUTRAL));
	Hand().ResetTurnState(true);
	EXPECT_EQ(Hand().GetSourceOwner(), std::nullopt);
}

// A handful taken from a poisoned pile is poisoned too
TEST_P(HandPickUpWorld, AHandfulFromAPoisonedPileIsPoisoned)
{
	const auto pile = FoodPile(100, true);
	Scoop(pile);
	const auto held = Hand().GetHeldObject();
	ASSERT_TRUE(held.has_value());
	ASSERT_NE(*held, pile);
	EXPECT_EQ(Registry().Get<const Pot>(*held).amount, k_FirstHandful);
	EXPECT_TRUE(Registry().Get<const Pot>(*held).poisoned);
}

// What the hand picks up leaves the map's cells, and the reactions it started whose info says so end while it is in the
// hand; a thing caught out of the air leaves the physics as it is, where it was caught, with no landing
TEST_P(HandPickUpWorld, WhatIsPickedUpLeavesThePhysicsAndTheMapAndWhatItStartedEnds)
{
	const auto [ends, lasts] = EndingAndLastingReactions();
	const auto rock = Rock();
	ASSERT_TRUE(ecs::map_cells::IsObjectInMap(rock));
	const auto ending = ecs::effects::reactions::CreateReaction(rock, ends, PlayerNames::PLAYER_ONE, false);
	const auto lasting = ecs::effects::reactions::CreateReaction(rock, lasts, PlayerNames::PLAYER_ONE, false);
	// a flying thing's reaction lasts when the thing is picked up from the land
	const auto flew =
	    ecs::effects::reactions::CreateReaction(rock, Reaction::ReactToFlyingObject, PlayerNames::PLAYER_ONE, false);
	PickUp(rock);
	ASSERT_EQ(Hand().GetHeldObject(), std::optional(rock));
	EXPECT_FALSE(ecs::map_cells::IsObjectInMap(rock));
	ASSERT_NE(ecs::effects::reactions::Find(ending), nullptr);
	ASSERT_NE(ecs::effects::reactions::Find(lasting), nullptr);
	EXPECT_FALSE(ecs::effects::reactions::Find(ending)->available);
	EXPECT_TRUE(ecs::effects::reactions::Find(lasting)->available);
	EXPECT_NE(ecs::effects::reactions::Find(flew), nullptr);
	// the hand lets it go, and catches another rock in flight
	LetGo(k_Throw, Place(k_SceneRock) + glm::vec3(0.0f, 6.0f, 0.0f), k_HandAngles);
	ASSERT_FALSE(Hand().GetHeldObject().has_value());
	const auto flying = Rock(glm::vec2(20.0f, 0.0f));
	Hurl(flying, 10.0f, glm::vec3(-5.0f, 2.0f, 0.0f));
	Turn();
	ASSERT_NE(Dynamics().Find(flying), nullptr);
	const auto caughtAt = Registry().Get<const Transform>(flying).position;
	ASSERT_GT(caughtAt.y, k_Ground + 1.0f);
	const auto flying9 =
	    ecs::effects::reactions::CreateReaction(flying, Reaction::ReactToFlyingObject, PlayerNames::PLAYER_ONE, false);
	const auto flyingLasting = ecs::effects::reactions::CreateReaction(flying, lasts, PlayerNames::PLAYER_ONE, false);
	ASSERT_NE(ecs::effects::reactions::Find(flying9), nullptr);
	PickUp(flying);
	ASSERT_EQ(Hand().GetHeldObject(), std::optional(flying));
	EXPECT_EQ(Dynamics().Find(flying), nullptr);
	EXPECT_FALSE(ecs::map_cells::IsObjectInMap(flying));
	EXPECT_EQ(Registry().Get<const Transform>(flying).position, caughtAt);
	// caught out of the air, its flying-object reactions end at once; its others stay
	EXPECT_EQ(ecs::effects::reactions::Find(flying9), nullptr);
	EXPECT_NE(ecs::effects::reactions::Find(flyingLasting), nullptr);
	// a computer player's hand takes things out of the physics the same way
	const auto other = Rock(glm::vec2(-20.0f, 0.0f));
	Hurl(other, 10.0f, glm::vec3(5.0f, 2.0f, 0.0f));
	Turn();
	ASSERT_NE(Dynamics().Find(other), nullptr);
	const auto other9 =
	    ecs::effects::reactions::CreateReaction(other, Reaction::ReactToFlyingObject, PlayerNames::PLAYER_ONE, false);
	ecs::physics::from_hand::LeavePhysicsForHand(other);
	EXPECT_EQ(Dynamics().Find(other), nullptr);
	EXPECT_EQ(ecs::effects::reactions::Find(other9), nullptr);
}

// A tree freed from the ground is held as anything else is: the hand's held and drawn object, out of the map, and let go
// of as any other thing, kept as the last thing the player let go of
TEST_P(HandPickUpWorld, ATreePulledFreeIsHeldAndLetGoAsAnythingElse)
{
	const auto birch = Birch();
	PickUp(birch);
	ASSERT_EQ(Hand().GetHeldObject(), std::optional(birch));
	EXPECT_EQ(Hand().GetRenderHandObject(), std::optional(birch));
	EXPECT_TRUE(Hand().IsInMagicHand(birch));
	EXPECT_FALSE(ecs::map_cells::IsObjectInMap(birch));
	LetGo(k_Throw, Place(k_SceneBirch) + glm::vec3(0.0f, 6.0f, 0.0f), glm::vec3(0.0f));
	EXPECT_FALSE(Hand().GetHeldObject().has_value());
	EXPECT_FALSE(Hand().GetRenderHandObject().has_value());
	EXPECT_EQ(ecs::last_dropped::WhoLastDropped(Registry(), birch), std::optional(PlayerNames::PLAYER_ONE));
	EXPECT_EQ(help_profile::Get(HelpEvent::Throw).Total(), 1u);
}

// A thing the hand is made to let go of leaves it as the hand's own let-go with no speed would leave it from where the
// hand draws it, turned as it is drawn there: the forced drop sends the throw data of that pose with no velocity and
// then the throw of the held thing, which the next turn's packets apply
TEST_P(HandPickUpWorld, AThingTheHandIsMadeToLetGoOfLeavesItAsALetGoWithNoSpeedFromWhereItIsDrawn)
{
	const auto from = Place(k_SceneRock) + glm::vec3(0.0f, 6.0f, 0.0f);
	const auto turned = affine::RotationYXZ(k_HandAngles.y, k_HandAngles.x, k_HandAngles.z);
	const auto forced = Rock();
	PickUp(forced);
	ASSERT_EQ(Hand().GetHeldObject(), std::optional(forced));
	auto& drawn = Registry().AssignOrReplace<ecs::components::HandDrawPose>(forced);
	drawn.position = from;
	drawn.rotation = turned;
	Hand().ForceDropHeld();
	Dispatch();
	ASSERT_FALSE(Hand().GetHeldObject().has_value());
	// the same rock let go of by the hand with no speed, 30 m along, from the same pose
	const glm::vec3 along {30.0f, 0.0f, 0.0f};
	const auto gentle = Rock(glm::vec2(along.x, along.z));
	PickUp(gentle);
	ASSERT_EQ(Hand().GetHeldObject(), std::optional(gentle));
	LetGo(glm::vec3(0.0f), from + along, k_HandAngles);
	ASSERT_FALSE(Hand().GetHeldObject().has_value());
	const auto& left = Registry().Get<const Transform>(forced);
	const auto& put = Registry().Get<const Transform>(gentle);
	for (int i = 0; i < 3; ++i)
	{
		EXPECT_NEAR(left.position[i] + along[i], put.position[i], 1e-3f) << i;
		for (int j = 0; j < 3; ++j)
		{
			EXPECT_NEAR(left.rotation[i][j], put.rotation[i][j], 1e-5f) << i << ", " << j;
		}
	}
	const auto* forcedBody = Dynamics().Find(forced);
	EXPECT_EQ(forcedBody == nullptr, Dynamics().Find(gentle) == nullptr);
	if (forcedBody != nullptr)
	{
		EXPECT_EQ(forcedBody->body.velocity, glm::vec3(0.0f));
	}
}

// A thing let go of leaves the hand where the hand holds it, turned by the hand's angles and at its own size; the
// physics only lifts it out of the ground: thrown from above the land it is not pulled down to it, and thrown from
// below it is raised until its lowest point is on the land
TEST_P(HandPickUpWorld, AThrownThingLeavesTheHandAsTheHandHeldItOnlyLiftedOutOfTheGround)
{
	const auto rock = Rock();
	const auto scale = Registry().Get<const Transform>(rock).scale;
	PickUp(rock);
	const auto from = Place(k_SceneRock) + glm::vec3(0.0f, 6.0f, 0.0f);
	LetGo(k_Throw, from, k_HandAngles);
	ASSERT_FALSE(Hand().GetHeldObject().has_value());
	const auto& transform = Registry().Get<const Transform>(rock);
	EXPECT_EQ(transform.position, from);
	EXPECT_EQ(transform.rotation, affine::RotationYXZ(k_HandAngles.y, k_HandAngles.x, k_HandAngles.z));
	EXPECT_EQ(transform.scale, scale);
	const auto* above = Dynamics().Find(rock);
	ASSERT_NE(above, nullptr);
	EXPECT_GT(Lowest(above->body), k_Ground);
	// from below the land
	const auto low = Rock(glm::vec2(20.0f, 0.0f));
	PickUp(low);
	LetGo(k_Throw, Place(k_SceneRock, glm::vec2(20.0f, 0.0f)) - glm::vec3(0.0f, 0.5f, 0.0f), k_HandAngles);
	const auto* below = Dynamics().Find(low);
	ASSERT_NE(below, nullptr);
	EXPECT_FLOAT_EQ(Lowest(below->body), k_Ground);
}

// A thing thrown, which does not land where it is let go of, is offered to the creatures to catch; one put down gently on
// the land lands there and is not
TEST_P(HandPickUpWorld, AThingLetGoThatDoesNotLandIsOfferedToTheCreaturesToCatch)
{
	const auto rock = Rock();
	PickUp(rock);
	Offers().clear();
	LetGo(k_Throw, Place(k_SceneRock) + glm::vec3(0.0f, 6.0f, 0.0f), k_HandAngles);
	EXPECT_EQ(Offers(), std::vector {rock});
	const auto other = Rock(glm::vec2(20.0f, 0.0f));
	PickUp(other);
	Offers().clear();
	LetGo(glm::vec3(0.0f), Place(k_SceneRock, glm::vec2(20.0f, 0.0f)), glm::vec3(0.0f));
	EXPECT_TRUE(Offers().empty());
}

// The player's first throw counts as a throw in the help profile
TEST_P(HandPickUpWorld, TheFirstThrowCountsAsAThrow)
{
	const auto rock = Rock();
	PickUp(rock);
	EXPECT_EQ(help_profile::Get(HelpEvent::Throw).Total(), 0u);
	LetGo(k_Throw, Place(k_SceneRock) + glm::vec3(0.0f, 6.0f, 0.0f), k_HandAngles);
	EXPECT_EQ(help_profile::Get(HelpEvent::Throw).Total(), 1u);
}

// The player keeps the last thing their hand let go of, one thing at a time: letting go of another takes the mark off the
// first
TEST_P(HandPickUpWorld, ThePlayerKeepsTheLastThingTheHandLetGoOf)
{
	const auto first = Rock();
	PickUp(first);
	LetGo(k_Throw, Place(k_SceneRock) + glm::vec3(0.0f, 6.0f, 0.0f), k_HandAngles);
	EXPECT_EQ(ecs::last_dropped::WhoLastDropped(Registry(), first), std::optional(PlayerNames::PLAYER_ONE));
	const auto second = Rock(glm::vec2(20.0f, 0.0f));
	PickUp(second);
	LetGo(k_Throw, Place(k_SceneRock, glm::vec2(20.0f, 0.0f)) + glm::vec3(0.0f, 6.0f, 0.0f), k_HandAngles);
	EXPECT_EQ(ecs::last_dropped::WhoLastDropped(Registry(), second), std::optional(PlayerNames::PLAYER_ONE));
	EXPECT_EQ(ecs::last_dropped::WhoLastDropped(Registry(), first), std::nullopt);
}

// A reward chest on the land never goes into the hand, as the original's chest says it may not be placed in a hand
// (docs/wiki/scripts/map-script-objects.md#reward-chests-create_reward-239-create_reward_in_town-240); the hand still
// picks up the rock beside it
TEST_P(HandPickUpWorld, AChestOnTheLandNeverGoesInTheHand)
{
	ecs::systems::RewardSystem rewards;
	const auto chest = rewards.Create(Place(k_SceneRock, glm::vec2(0.0f, 8.0f)), RewardObjectInfo::SmallFood,
	                                  PlayerNames::PLAYER_ONE, entt::null, false);
	// every other check of the press lets it through: it is on the land and may be picked up
	ASSERT_TRUE(ecs::IsAvailable(chest));
	ASSERT_FALSE(ecs::object_flags::IsCannotBePickedUp(chest));
	PickUp(chest);
	EXPECT_FALSE(Hand().GetHeldObject().has_value());
	EXPECT_FALSE(Hand().IsInMagicHand(chest));
	EXPECT_EQ(help_profile::Get(HelpEvent::PickUp).Total(), 0u);
	const auto rock = Rock();
	PickUp(rock);
	EXPECT_EQ(Hand().GetHeldObject(), std::optional(rock));
}

// The three mushrooms are loose things to the hand and to a creature: the hand's press picks each up, and a creature
// takes one as a whole thing, by the test of things in general, never by a kind rule of its own
// (docs/wiki/project/progress/resources/poison_and_mushrooms.md)
TEST_P(HandPickUpWorld, EachMushroomIsALooseThingTheHandAndACreatureTakeWhole)
{
	using creature_object_actions::KindRule;
	using creature_object_actions::Thing;
	float shift = 0.0f;
	for (const auto& [type, weight] :
	     {std::pair {MobileObjectInfo::Champi, 70.0f}, std::pair {MobileObjectInfo::MagicMushroom, 70.0f},
	      std::pair {MobileObjectInfo::Toadstool, 200.0f}})
	{
		auto& row = Info().mobileObject.at(static_cast<size_t>(type));
		if (!GameData())
		{
			// on synthetic data its info.dat row: its own kind and its weight
			row.mobileType = type;
			row.weight = weight;
		}
		LoadMesh(row.meshId, glm::vec3(-0.2f, 0.0f, -0.2f), glm::vec3(0.2f, 0.3f, 0.2f));
		const auto at = Place(k_SceneRock, glm::vec2(shift, 6.0f));
		const auto mushroom = ecs::archetypes::MobileObjectArchetype::Create(at, type, 0.0f, 1.0f);
		shift += 3.0f;
		const auto facts = ecs::systems::CreatureObjectActionSystem::KindFactsOf(mushroom);
		EXPECT_EQ(facts.thing, Thing::MobileObject);
		EXPECT_EQ(facts.rule, KindRule::Whole);
		EXPECT_TRUE(facts.holdable);
		EXPECT_FALSE(creature_object_actions::DecidedByKind(facts, true).has_value());
		PickUp(mushroom);
		EXPECT_EQ(Hand().GetHeldObject(), std::optional(mushroom));
		LetGo(k_Throw, at + glm::vec3(0.0f, 6.0f, 0.0f), k_HandAngles);
		EXPECT_FALSE(Hand().GetHeldObject().has_value());
	}
}

// A handful scooped from a store's food pile whose food is poisoned is poisoned too; the store gives it from its total
// (docs/wiki/hand/states.md#the-start)
TEST_P(HandPickUpWorld, AHandfulFromAPoisonedStoreIsPoisoned)
{
	auto& row = Info().pot.at(static_cast<size_t>(PotInfo::StoragePitFoodPile));
	if (!GameData())
	{
		row.meshId = MeshId::I_Food;
		row.potType = PotType::PileFood;
		row.resourceType = ResourceType::Food;
		row.maxAmountInPot = 20000;
	}
	for (const auto type : {PotInfo::StoragePitFoodPile, PotInfo::HandFood})
	{
		LoadMesh(Info().pot.at(static_cast<size_t>(type)).meshId, glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(1.0f, 0.5f, 1.0f));
	}
	const auto pile = ecs::archetypes::PotArchetype::Create(Place(k_SceneRock, glm::vec2(8.0f, -8.0f)), 0.0f,
	                                                        PotInfo::StoragePitFoodPile, 100);
	const auto pit = Registry().Create();
	Registry().Assign<ecs::components::Abode>(pit).townId = ecs::components::Abode::k_NoTown;
	auto& store = Registry().Assign<ecs::components::StoragePit>(pit);
	store.woodPiles.fill(entt::null);
	store.foodPile = pile;
	Registry().Get<Pot>(pile).poisoned = true;
	ASSERT_TRUE(ecs::object_resources::IsPoisoned(pit));
	Scoop(pile);
	const auto held = Hand().GetHeldObject();
	ASSERT_TRUE(held.has_value());
	ASSERT_NE(*held, pile);
	EXPECT_EQ(Registry().Get<const Pot>(*held).type, PotInfo::HandFood);
	EXPECT_EQ(Registry().Get<const Pot>(*held).amount, k_FirstHandful);
	EXPECT_TRUE(Registry().Get<const Pot>(*held).poisoned);
	EXPECT_EQ(Registry().Get<const Pot>(pile).amount, 100u - k_FirstHandful);
}

// A held thing that is destroyed leaves the hand at the hand's next turn: until then the hand counts as full and takes
// nothing else, though the destroyed thing no longer reads as held; after it the next press takes another thing
TEST_P(HandPickUpWorld, AHeldThingThatIsDestroyedLeavesTheHandAtItsNextTurn)
{
	const auto rock = Rock();
	PickUp(rock);
	ASSERT_EQ(Hand().GetHeldObject(), std::optional(rock));
	ecs::ToBeDeleted(rock);
	EXPECT_FALSE(Hand().GetHeldObject().has_value());
	const auto other = Rock(glm::vec2(20.0f, 0.0f));
	PickUp(other);
	EXPECT_FALSE(Hand().GetHeldObject().has_value());
	Hand().ProcessTurn();
	PickUp(other);
	EXPECT_EQ(Hand().GetHeldObject(), std::optional(other));
}

// A tree whose physics ends landed on land is planted again, and the player whose body it was shows its creature the
// deed of planting a tree (deed 11) on the tree, with no magic, before the player's alignment takes the planting; a
// tree with no player shows nothing, nor does one that did not land, which becomes a dead tree
// (docs/wiki/creature/mimicry.md#deed-table, row 11)
TEST_P(HandPickUpWorld, ATreeReplantedShowsItsPlayersCreatureTheDeedOfPlantingATree)
{
	const test::RestoreService<Locator::events> restoreEvents;
	Locator::events::emplace<EventManager>();
	std::vector<ecs::events::PlayerDeedForMimic> deeds;
	std::vector<float> pendingAtTheDeed;
	Locator::events::value().AddHandler<ecs::events::PlayerDeedForMimic>(
	    [&deeds, &pendingAtTheDeed](const ecs::events::PlayerDeedForMimic& event) {
		    deeds.push_back(event);
		    pendingAtTheDeed.push_back(ecs::effects::alignment::Of(PlayerNames::PLAYER_ONE).pending);
	    });
	const auto birch = Birch();
	ecs::PhysicsEntry po;
	po.entity = birch;
	po.flags = ecs::PhysicsEntry::k_Landed;
	po.throwerPlayer = PlayerNames::PLAYER_ONE;
	EXPECT_EQ(Hand().TreeEndPhysics(birch, po), birch);
	ASSERT_EQ(deeds.size(), 1u);
	EXPECT_EQ(deeds.front().player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(deeds.front().deed, creature_watching::Deed::PlantTree);
	EXPECT_EQ(static_cast<int>(deeds.front().deed), 11);
	EXPECT_EQ(deeds.front().object, birch);
	EXPECT_EQ(deeds.front().point, Registry().Get<const Transform>(birch).position);
	EXPECT_FALSE(deeds.front().magic.has_value());
	// the deed is told before the alignment takes the planting
	EXPECT_EQ(pendingAtTheDeed.front(), 0.0f);
	EXPECT_GT(ecs::effects::alignment::Of(PlayerNames::PLAYER_ONE).pending, 0.0f);
	// with no player: planted, and no deed
	const auto other = Birch(Place(k_SceneBirch) + glm::vec3(20.0f, 0.0f, 0.0f));
	po.entity = other;
	po.throwerPlayer.reset();
	EXPECT_EQ(Hand().TreeEndPhysics(other, po), other);
	EXPECT_EQ(deeds.size(), 1u);
	// not landed: a dead tree, and no deed
	const auto fallen = Birch(Place(k_SceneBirch) + glm::vec3(-20.0f, 0.0f, 0.0f));
	po.entity = fallen;
	po.flags = 0;
	po.throwerPlayer = PlayerNames::PLAYER_ONE;
	static_cast<void>(Hand().TreeEndPhysics(fallen, po));
	EXPECT_EQ(deeds.size(), 1u);
}

// A hand's food that ends its flight on land is put down as a new pile, which starts its row's reaction, the food one,
// for the player whose hand let it go: the nearby people come and look (docs/wiki/world/objects.md#pots-and-piles)
TEST_P(HandPickUpWorld, AHandsFoodPutDownOnLandMakesAPileThatStartsTheFoodReaction)
{
	if (!GameData())
	{
		auto& row = Info().pot.at(static_cast<size_t>(PotInfo::MagicFood));
		row.meshId = MeshId::I_Food;
		row.potType = PotType::PileFood;
		row.resourceType = ResourceType::Food;
		row.maxAmountInPot = 20000;
		row.associatedReaction = Reaction::ReactToFood;
	}
	ASSERT_EQ(Info().pot.at(static_cast<size_t>(PotInfo::MagicFood)).associatedReaction, Reaction::ReactToFood);
	for (const auto type : {PotInfo::HandFood, PotInfo::MagicFood})
	{
		LoadMesh(Info().pot.at(static_cast<size_t>(type)).meshId, glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(1.0f, 0.5f, 1.0f));
	}
	const auto handful =
	    ecs::archetypes::PotArchetype::Create(Place(k_SceneRock, glm::vec2(12.0f, -12.0f)), 0.0f, PotInfo::HandFood, 30);
	ASSERT_NE(handful, entt::entity {entt::null});
	// the handful goes, a pile takes its food
	EXPECT_EQ(Hand().PotEndPhysics(handful, PlayerNames::PLAYER_ONE), entt::entity {entt::null});
	std::vector<entt::entity> piles;
	Registry().Each<const Pot>([&piles](entt::entity entity, const Pot& pot) {
		if (pot.type == PotInfo::MagicFood)
		{
			piles.push_back(entity);
		}
	});
	ASSERT_EQ(piles.size(), 1u);
	EXPECT_EQ(Registry().Get<const Pot>(piles.front()).amount, 30u);
	const auto id = ecs::effects::reactions::GetReactionOfTypeInitiatedBy(piles.front(), Reaction::ReactToFood);
	ASSERT_NE(id, 0u);
	ASSERT_NE(ecs::effects::reactions::Find(id), nullptr);
	EXPECT_EQ(ecs::effects::reactions::Find(id)->player, std::optional(PlayerNames::PLAYER_ONE));
}

// An uprooted tree's roots follow it, its matrix with the rotation scaled by 0.15 times its mesh's extent, and when its
// flight ends with it a dead tree they break off from the dead tree's matrix and fall by themselves
// (docs/wiki/hand/states.md#a-trees-roots)
TEST_P(HandPickUpWorld, AnUprootedTreesRootsFollowItAndBreakOffWhenItBecomesADeadTree)
{
	// on synthetic data a box 2 m wide and half a metre tall
	LoadMesh(MeshId::TreeRoots, glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(1.0f, 0.5f, 1.0f));
	const auto rootsMesh = resources::HashIdentifier(MeshId::TreeRoots);
	const auto birch = Birch();
	auto& meshes = Locator::resources::value().GetMeshes();
	const auto treeMesh = Registry().Get<const ecs::components::Mesh>(birch).id;
	const float extent = 0.5f * meshes.Handle(treeMesh)->GetBoundingBox().Size().x;
	const auto rootsEntities = [rootsMesh]() {
		std::vector<entt::entity> found;
		Registry().Each<const ecs::components::Mesh>(
		    [&found, rootsMesh](entt::entity entity, const ecs::components::Mesh& mesh) {
			    if (mesh.id == rootsMesh)
			    {
				    found.push_back(entity);
			    }
		    });
		return found;
	};
	ASSERT_TRUE(rootsEntities().empty());
	// moved out of the ground: the roots, on the tree
	Registry().Get<Transform>(birch).position.y = k_Ground + 3.0f;
	Hand().TreeMoved(birch);
	const auto found = rootsEntities();
	ASSERT_EQ(found.size(), 1u);
	const auto roots = found.front();
	const auto* attached = Registry().TryGet<const ecs::components::TreeRoots>(roots);
	ASSERT_NE(attached, nullptr);
	EXPECT_EQ(attached->tree, birch);
	EXPECT_EQ(attached->factor, 0.15f * extent);
	const auto& tree = Registry().Get<const Transform>(birch);
	EXPECT_EQ(Registry().Get<const Transform>(roots).position, tree.position);
	EXPECT_EQ(Registry().Get<const Transform>(roots).scale, tree.scale * (0.15f * extent));
	// a flight that did not land: a dead tree, its roots the same, off it and falling from where it lies
	ecs::PhysicsEntry po;
	po.entity = birch;
	po.flags = 0;
	po.throwerPlayer = PlayerNames::PLAYER_ONE;
	static_cast<void>(Hand().TreeEndPhysics(birch, po));
	ASSERT_TRUE(Registry().AllOf<ecs::components::DeadTree>(birch));
	EXPECT_EQ(rootsEntities(), (std::vector<entt::entity> {roots}));
	EXPECT_FALSE(Registry().AllOf<ecs::components::TreeRoots>(roots));
	const auto& dead = Registry().Get<const Transform>(birch);
	EXPECT_EQ(Registry().Get<const Transform>(roots).position, dead.position);
	EXPECT_EQ(Registry().Get<const Transform>(roots).rotation, dead.rotation);
	EXPECT_EQ(Registry().Get<const Transform>(roots).scale, dead.scale * (0.15f * extent));
}

// A tree a script marked non-scenic keeps the mark until the hand replants it; the replanting sets it again to whether
// the tree landed in a town, so one replanted away from every town is scenic again, in a new forest of its own
TEST_P(HandPickUpWorld, AReplantedTreeIsNonScenicOnlyInATown)
{
	const auto birch = Birch();
	auto& tree = Registry().Get<ecs::components::Tree>(birch);
	tree.isNonScenic = true;
	ASSERT_EQ(tree.forestId, 0u);
	ecs::PhysicsEntry po;
	po.entity = birch;
	po.flags = ecs::PhysicsEntry::k_Landed;
	EXPECT_EQ(Hand().TreeEndPhysics(birch, po), birch);
	const auto& replanted = Registry().Get<const ecs::components::Tree>(birch);
	EXPECT_FALSE(replanted.isNonScenic);
	EXPECT_NE(replanted.forestId, 0u);
}

INSTANTIATE_TEST_SUITE_P(Data, HandPickUpWorld,
                         ::testing::Values(test::world::WorldData::Synthetic, test::world::WorldData::Game),
                         test::world::DataName);
