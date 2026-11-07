/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <array>
#include <bit>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Audio/Services/Guidance.h"
#include "Common/GameAngle.h"
#include "Common/GameDistance.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerPlayerMind.h"
#include "ECS/ComputerPlayerMindTree.h"
#include "InfoConstants.h"
#include "support/ComputerScoreFakes.h"

using namespace openblack;
using namespace openblack::computer_hand;
using openblack::ecs::components::ComputerHand;
using openblack::ecs::components::HandArgument;
using openblack::ecs::components::HandStep;
using openblack::ecs::components::HandSubAction;

namespace
{

/// Stages that give the results the test lines up, and a record of what ran
class FakeRunner final: public StageRunner
{
public:
	StageResult Run(ComputerHand& /*hand*/, HandStep& step, uint8_t stage) override
	{
		ran.emplace_back(step.type, stage);
		if (results.empty())
		{
			return StageResult::Done;
		}
		const auto result = results.front();
		results.pop_front();
		return result;
	}
	[[nodiscard]] bool IsAngryMiracle(int32_t miracle) const override { return angry.contains(miracle); }
	[[nodiscard]] glm::vec3 InteractPoint(entt::entity thing) const override
	{
		const auto found = interactPoints.find(thing);
		return found != interactPoints.end() ? found->second : glm::vec3(0.0f);
	}
	[[nodiscard]] float InfluenceAt(const map_coords::MapCoords& place) const override
	{
		influenceAsked.push_back(place);
		return influence;
	}
	void ThrowHeld(ComputerHand& hand, const glm::vec3& velocity) override { thrown.emplace_back(hand.point, velocity); }
	void LetGo(ComputerHand& hand) override
	{
		++letGo;
		hand.heldObject = entt::null;
	}
	[[nodiscard]] bool IsGone(entt::entity thing) const override { return gone.contains(thing); }
	[[nodiscard]] bool CanBePickedUp(entt::entity thing) const override { return !refused.contains(thing); }
	bool TakeIntoHand(ComputerHand& /*hand*/, entt::entity thing) override
	{
		taken.push_back(thing);
		return goesIn;
	}
	void GiveHeldTo(ComputerHand& hand, entt::entity thing) override
	{
		givenTo.push_back(thing);
		if (takesWhatIsGiven)
		{
			hand.heldObject = entt::null;
		}
	}
	[[nodiscard]] int32_t ResourceOf(entt::entity thing, ResourceType /*type*/) const override
	{
		const auto found = resource.find(thing);
		return found != resource.end() ? found->second : 0;
	}
	[[nodiscard]] bool IsPotOf(entt::entity thing, ResourceType type) const override
	{
		const auto found = pots.find(thing);
		return found != pots.end() && found->second == type;
	}
	void RemoveResource(entt::entity thing, ResourceType /*type*/, int32_t amount) override
	{
		removed.emplace_back(thing, amount);
		resource[thing] -= amount;
	}
	void AddToPot(entt::entity pot, ResourceType /*type*/, int32_t amount) override
	{
		added.emplace_back(pot, amount);
		resource[pot] += amount;
	}
	entt::entity MakeHandPot(entt::entity at, ResourceType type, int32_t amount) override
	{
		made.emplace_back(at, type, amount);
		if (!makesPots)
		{
			return entt::null;
		}
		const auto pot = static_cast<entt::entity>(100 + made.size());
		pots[pot] = type;
		resource[pot] = amount;
		return pot;
	}

	[[nodiscard]] uint8_t GestureOf(int32_t miracle, int32_t which) const override
	{
		gesturesAsked.emplace_back(miracle, which);
		return gesture;
	}
	[[nodiscard]] const std::vector<glm::vec3>& GestureShape(uint8_t /*gesture*/) const override { return shape; }
	[[nodiscard]] float GestureLengthRatio(uint8_t /*gesture*/) const override { return lengthRatio; }
	[[nodiscard]] bool HasWorshipSite(entt::entity icon) const override { return withSite.contains(icon); }
	[[nodiscard]] bool IsWorshipIcon(entt::entity thing) const override { return worshipIcons.contains(thing); }
	[[nodiscard]] bool IsIconCharging(entt::entity icon) const override { return charging.contains(icon); }
	[[nodiscard]] bool ValidForStartCharge(entt::entity /*icon*/, int32_t /*powerUp*/) const override
	{
		return mayStartCharging;
	}
	void StartCharge(entt::entity icon, int32_t powerUp) override { started.emplace_back(icon, powerUp); }
	[[nodiscard]] float ChantNeeded(entt::entity icon) const override
	{
		const auto found = chantsNeeded.find(icon);
		return found != chantsNeeded.end() ? found->second : 0.0f;
	}
	[[nodiscard]] int32_t PowerUpOf(int32_t /*miracle*/) const override { return powerUp; }
	[[nodiscard]] float SecondsWithTheThingAfterACast(int32_t /*miracle*/) const override { return secondsAfterCast; }
	[[nodiscard]] std::optional<FollowedPlace> PlaceToFollow(entt::entity thing) const override
	{
		const auto found = followed.find(thing);
		return found != followed.end() ? std::optional<FollowedPlace>(found->second) : std::nullopt;
	}
	entt::entity CastOnObject(entt::entity thing, int32_t miracle, const glm::vec3& from, entt::entity icon) override
	{
		casts.emplace_back(thing, miracle, from, icon);
		return castGives;
	}
	void KeepCast(entt::entity spell, entt::entity thing) override { kept.emplace_back(spell, thing); }
	entt::entity CastAtPlace(int32_t miracle, const map_coords::MapCoords& to, const map_coords::MapCoords& from,
	                         entt::entity icon, float radius) override
	{
		castsAt.push_back({.miracle = miracle, .to = to, .from = from, .icon = icon, .radius = radius});
		return castGives;
	}
	void KeepCastAt(entt::entity spell, const map_coords::MapCoords& place) override { keptAt.emplace_back(spell, place); }
	void MakeDisciple(entt::entity villager, int32_t disciple) override { disciples.emplace_back(villager, disciple); }
	[[nodiscard]] bool IsCreatureWithPlayer(entt::entity thing) const override { return owned.contains(thing); }
	[[nodiscard]] bool IsLeashableObject(entt::entity thing) const override { return leashable.contains(thing); }
	void SetLeashType(entt::entity creature, int32_t type) override { leashTypes.emplace_back(creature, type); }
	[[nodiscard]] bool IsLeashTied(entt::entity creature) const override { return tied.contains(creature); }
	void TakeLeashOff(entt::entity creature) override
	{
		tied.erase(creature);
		takenOff.push_back(creature);
	}
	void TieLeash(entt::entity creature, entt::entity thing) override
	{
		tied.insert(creature);
		ties.emplace_back(creature, thing);
	}
	void ForceDominantDesire(entt::entity creature, int32_t desire, float seconds) override
	{
		dominant.emplace_back(creature, desire, seconds);
	}
	void OrderCreature(entt::entity creature, uint32_t action, entt::entity thing) override
	{
		orders.emplace_back(creature, action, thing);
	}
	[[nodiscard]] bool IsCommanded(entt::entity creature) const override { return commanded.contains(creature); }
	void TeachCreature(entt::entity creature, uint32_t kind, uint32_t action) override
	{
		taught.emplace_back(creature, kind, action);
	}
	void RaiseTotem(entt::entity totem, float share) override { raised.emplace_back(totem, share); }
	[[nodiscard]] std::optional<JourneyBody> JourneyBodyOf(entt::entity thing) const override
	{
		const auto found = bodies.find(thing);
		return found != bodies.end() ? std::optional<JourneyBody>(found->second) : std::nullopt;
	}
	void DrawThrownAway(float most) override { draws.push_back(most); }
	void SetJourneySpeed(entt::entity creature, float fraction) override { speeds.emplace_back(creature, fraction); }

	std::deque<StageResult> results;
	std::vector<std::tuple<HandSubAction, uint8_t>> ran;
	int letGo {0};
	std::set<entt::entity> gone;
	std::set<entt::entity> refused;
	std::vector<entt::entity> taken;
	bool goesIn {true};
	std::vector<entt::entity> givenTo;
	bool takesWhatIsGiven {true};
	std::map<entt::entity, int32_t> resource;
	std::map<entt::entity, ResourceType> pots;
	std::vector<std::tuple<entt::entity, int32_t>> removed;
	std::vector<std::tuple<entt::entity, int32_t>> added;
	std::vector<std::tuple<entt::entity, ResourceType, int32_t>> made;
	bool makesPots {true};
	uint8_t gesture {1};
	mutable std::vector<std::tuple<int32_t, int32_t>> gesturesAsked;
	std::vector<glm::vec3> shape {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}};
	float lengthRatio {1.0f};
	std::set<entt::entity> withSite;
	std::set<entt::entity> worshipIcons;
	std::set<entt::entity> charging;
	bool mayStartCharging {false};
	std::vector<std::tuple<entt::entity, int32_t>> started;
	std::map<entt::entity, float> chantsNeeded;
	int32_t powerUp {-1};
	float secondsAfterCast {0.0f};
	std::map<entt::entity, FollowedPlace> followed;
	std::vector<std::tuple<entt::entity, int32_t, glm::vec3, entt::entity>> casts;
	entt::entity castGives {entt::null};
	std::vector<std::tuple<entt::entity, entt::entity>> kept;
	struct CastAtPlaceCall
	{
		int32_t miracle;
		map_coords::MapCoords to;
		map_coords::MapCoords from;
		entt::entity icon;
		float radius;
	};
	std::vector<CastAtPlaceCall> castsAt;
	std::vector<std::tuple<entt::entity, map_coords::MapCoords>> keptAt;
	std::vector<std::tuple<glm::vec3, glm::vec3>> thrown;
	float influence {1.0f};
	std::set<int32_t> angry;
	std::map<entt::entity, glm::vec3> interactPoints;
	mutable std::vector<map_coords::MapCoords> influenceAsked;
	std::vector<std::tuple<entt::entity, int32_t>> disciples;
	std::set<entt::entity> owned;
	std::set<entt::entity> leashable;
	std::vector<std::tuple<entt::entity, int32_t>> leashTypes;
	std::set<entt::entity> tied;
	std::vector<std::tuple<entt::entity, int32_t, float>> dominant;
	std::vector<std::tuple<entt::entity, uint32_t, entt::entity>> orders;
	std::set<entt::entity> commanded;
	std::vector<std::tuple<entt::entity, uint32_t, uint32_t>> taught;
	std::vector<std::tuple<entt::entity, float>> raised;
	std::map<entt::entity, JourneyBody> bodies;
	std::vector<float> draws;
	std::vector<std::tuple<entt::entity, float>> speeds;
	std::vector<entt::entity> takenOff;
	std::vector<std::tuple<entt::entity, entt::entity>> ties;
};

/// What the test says of the things: which are objects and where the markers are
class FakeFacts final: public ThingFacts
{
public:
	[[nodiscard]] bool IsObject(entt::entity thing) const override { return objects.contains(thing); }
	[[nodiscard]] bool HasPlace(entt::entity thing) const override { return thing != entt::null && !placeless.contains(thing); }
	[[nodiscard]] std::optional<glm::vec3> MarkerPoint(entt::entity thing) const override
	{
		const auto found = markers.find(thing);
		return found != markers.end() ? std::optional<glm::vec3>(found->second) : std::nullopt;
	}
	[[nodiscard]] bool IsVillager(entt::entity thing) const override { return villagers.contains(thing); }
	[[nodiscard]] bool IsCreature(entt::entity thing) const override { return creatures.contains(thing); }
	[[nodiscard]] bool IsScaffold(entt::entity thing) const override { return scaffolds.contains(thing); }
	[[nodiscard]] bool IsAvailableBuildingSite(entt::entity thing) const override { return siteBuildings.contains(thing); }
	[[nodiscard]] VillagerHome HomeOf(entt::entity villager) const override
	{
		const auto found = homes.find(villager);
		return found != homes.end() ? found->second : VillagerHome {};
	}
	[[nodiscard]] map_coords::MapCoords ArrivePlace(entt::entity thing) const override
	{
		const auto found = arrivePlaces.find(thing);
		return found != arrivePlaces.end() ? found->second : map_coords::MapCoords {};
	}
	[[nodiscard]] bool IsWorshipSite(entt::entity thing) const override { return sites.contains(thing); }
	[[nodiscard]] std::optional<glm::vec3> GatePoint(entt::entity site) const override
	{
		const auto found = gates.find(site);
		return found != gates.end() ? std::optional<glm::vec3>(found->second) : std::nullopt;
	}
	[[nodiscard]] glm::vec3 MapPoint(entt::entity thing) const override
	{
		const auto found = places.find(thing);
		return found != places.end() ? found->second : glm::vec3(0.0f);
	}
	[[nodiscard]] entt::entity SiteBuilding(entt::entity thing) const override
	{
		const auto found = siteBuildings.find(thing);
		return found != siteBuildings.end() ? found->second : entt::entity {entt::null};
	}
	[[nodiscard]] uint32_t ResourceOf(entt::entity thing, ResourceType /*type*/) const override
	{
		const auto found = resource.find(thing);
		return found != resource.end() ? found->second : 0;
	}
	[[nodiscard]] bool IsTown(entt::entity thing) const override { return towns.contains(thing); }
	[[nodiscard]] ResourceType ResourceTypeOf(entt::entity thing) const override
	{
		const auto found = types.find(thing);
		return found != types.end() ? found->second : ResourceType::None;
	}
	[[nodiscard]] entt::entity ResourceDropObject(entt::entity town, ResourceType type) const override
	{
		asked.emplace_back(town, type);
		const auto found = drops.find(town);
		return found != drops.end() ? found->second : entt::entity {entt::null};
	}
	[[nodiscard]] std::optional<MagicType> IconMagicType(entt::entity thing) const override
	{
		const auto found = icons.find(thing);
		return found != icons.end() ? std::optional<MagicType>(found->second) : std::nullopt;
	}
	[[nodiscard]] map_coords::MapCoords MapPlace(entt::entity thing) const override
	{
		const auto found = mapPlaces.find(thing);
		return found != mapPlaces.end() ? found->second : map_coords::MapCoords {};
	}
	[[nodiscard]] std::vector<Worshipper> SiteWorshippers(entt::entity site) const override
	{
		const auto found = worshippers.find(site);
		return found != worshippers.end() ? found->second : std::vector<Worshipper> {};
	}
	[[nodiscard]] std::optional<map_coords::MapCoords> WorshipFoodPlace(entt::entity site) const override
	{
		const auto found = foodPlaces.find(site);
		return found != foodPlaces.end() ? std::optional(found->second) : std::nullopt;
	}
	[[nodiscard]] map_coords::MapCoords NearestEdgeTo(entt::entity thing, const map_coords::MapCoords& /*place*/) const override
	{
		const auto found = mapPlaces.find(thing);
		return found != mapPlaces.end() ? found->second : map_coords::MapCoords {};
	}
	[[nodiscard]] glm::vec3 LandPoint(const map_coords::MapCoords& place) const override
	{
		// flat land at 5 m
		return {map_coords::ToMetres(place.x), 5.0f + place.altitude, map_coords::ToMetres(place.z)};
	}

	std::set<entt::entity> objects;
	std::set<entt::entity> placeless;
	std::map<entt::entity, glm::vec3> markers;
	std::set<entt::entity> villagers;
	std::set<entt::entity> creatures;
	std::set<entt::entity> scaffolds;
	std::map<entt::entity, VillagerHome> homes;
	std::map<entt::entity, map_coords::MapCoords> arrivePlaces;
	std::set<entt::entity> sites;
	std::map<entt::entity, glm::vec3> gates;
	std::map<entt::entity, glm::vec3> places;
	std::map<entt::entity, entt::entity> siteBuildings;
	std::map<entt::entity, uint32_t> resource;
	std::set<entt::entity> towns;
	std::map<entt::entity, ResourceType> types;
	std::map<entt::entity, entt::entity> drops;
	mutable std::vector<std::tuple<entt::entity, ResourceType>> asked;
	std::map<entt::entity, MagicType> icons;
	std::map<entt::entity, map_coords::MapCoords> mapPlaces;
	std::map<entt::entity, map_coords::MapCoords> foodPlaces;
	std::map<entt::entity, std::vector<Worshipper>> worshippers;
};

/// The node of the mind that a name names first, and its parent
std::pair<uint16_t, uint16_t> NodeAndParent(std::string_view name)
{
	const auto node = computer_player::FindNode(name);
	EXPECT_TRUE(node.has_value());
	const auto parent = computer_player::GameTree().nodes[*node].parent;
	EXPECT_TRUE(parent.has_value());
	return {*node, *parent};
}

/// The first node of the row named, its number and the space after it included, and its parent
std::pair<uint16_t, uint16_t> NodeAndParentOfRow(std::string_view rowName)
{
	const auto& nodes = computer_player::MindNodes();
	for (uint16_t node = 0; node < nodes.size(); ++node)
	{
		if (computer_player::k_MindRows.at(nodes.at(node).row).name == rowName && nodes.at(node).parent.has_value())
		{
			return {node, *nodes.at(node).parent};
		}
	}
	ADD_FAILURE() << rowName;
	return {0, 0};
}

/// Flat land at 5 m, wherever the hand asks
class FlatGround final: public Ground
{
public:
	[[nodiscard]] float AltitudeAt(int32_t /*mapX*/, int32_t /*mapZ*/) const override { return 5.0f; }
};

/// The float of the bits the original gave
float Bits(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}

/// A hand at the point going to the target at the speed
ComputerHand HandGoing(glm::vec3 point, glm::vec3 target, float speed)
{
	ComputerHand hand;
	hand.point = point;
	hand.target = target;
	hand.speed = speed;
	return hand;
}

HandArgument PointAt(float x, float y, float z)
{
	return {.kind = HandArgument::Kind::Point, .point = {x, y, z}};
}

HandArgument ThingOf(entt::entity thing)
{
	return {.kind = HandArgument::Kind::Thing, .thing = thing};
}

} // namespace

TEST(ComputerHand, ReadyOnlyWithoutSteps)
{
	ComputerHand hand;
	EXPECT_TRUE(IsReady(hand));
	Append(hand, {.type = HandSubAction::MoveHandToPos, .arguments = {PointAt(1, 2, 3)}});
	EXPECT_FALSE(IsReady(hand));
}

TEST(ComputerHand, PrerequisitesFromTheGamesTable)
{
	EXPECT_EQ(Prerequisite(HandSubAction::PickUpObject), HandSubAction::MoveHandToObject);
	EXPECT_EQ(Prerequisite(HandSubAction::DropObjectInHandAtPos), HandSubAction::MoveHandToPos);
	EXPECT_EQ(Prerequisite(HandSubAction::ThrowObjectInHandAtPos), HandSubAction::MoveHandNearPos);
	EXPECT_EQ(Prerequisite(HandSubAction::MoveHandToPos), HandSubAction::None);
	EXPECT_EQ(Prerequisite(HandSubAction::MoveHandToObject), HandSubAction::None);
	EXPECT_EQ(Prerequisite(HandSubAction::SetHandToPos), HandSubAction::None);
	EXPECT_EQ(Prerequisite(HandSubAction::SetHandToPosDirectY), HandSubAction::None);
	EXPECT_EQ(Prerequisite(HandSubAction::None), HandSubAction::None);
}

TEST(ComputerHand, AStepWithoutAMoveTakesThreeTurns)
{
	ComputerHand hand;
	FakeRunner runner;
	Append(hand, {.type = HandSubAction::MoveHandToPos, .arguments = {PointAt(1, 2, 3)}});
	EXPECT_TRUE(Step(hand, runner));
	EXPECT_TRUE(Step(hand, runner));
	EXPECT_FALSE(IsReady(hand));
	EXPECT_TRUE(Step(hand, runner));
	EXPECT_TRUE(IsReady(hand));
	const std::vector<std::tuple<HandSubAction, uint8_t>> expected {
	    {HandSubAction::MoveHandToPos, 0}, {HandSubAction::MoveHandToPos, 1}, {HandSubAction::MoveHandToPos, 2}};
	EXPECT_EQ(runner.ran, expected);
}

TEST(ComputerHand, AStageGoingOnStaysOnItsStage)
{
	ComputerHand hand;
	FakeRunner runner;
	runner.results = {StageResult::Done, StageResult::Going, StageResult::Going, StageResult::Done};
	Append(hand, {.type = HandSubAction::MoveHandToPos, .arguments = {PointAt(1, 2, 3)}});
	for (int turn = 0; turn < 5; ++turn)
	{
		Step(hand, runner);
	}
	EXPECT_TRUE(IsReady(hand));
	ASSERT_EQ(runner.ran.size(), 5u);
	EXPECT_EQ(std::get<1>(runner.ran.at(3)), 1);
	EXPECT_EQ(std::get<1>(runner.ran.at(4)), 2);
}

TEST(ComputerHand, TheMoveGoesInFrontWithACopyOfTheFirstArgument)
{
	ComputerHand hand;
	FakeRunner runner;
	const auto villager = static_cast<entt::entity>(7);
	Append(hand, {.type = HandSubAction::PickUpObject,
	              .arguments = {ThingOf(villager), {.kind = HandArgument::Kind::Float, .floatValue = 150.0f}}});
	// The first turn only puts the move in front
	EXPECT_TRUE(Step(hand, runner));
	EXPECT_TRUE(runner.ran.empty());
	ASSERT_EQ(hand.steps.size(), 2u);
	EXPECT_EQ(hand.steps.front().type, HandSubAction::MoveHandToObject);
	ASSERT_EQ(hand.steps.front().arguments.size(), 1u);
	EXPECT_EQ(hand.steps.front().arguments.front().thing, villager);
	// The move runs its three stages, then the pick up its three without a second move in front
	for (int turn = 0; turn < 6; ++turn)
	{
		Step(hand, runner);
	}
	EXPECT_TRUE(IsReady(hand));
	const std::vector<std::tuple<HandSubAction, uint8_t>> expected {
	    {HandSubAction::MoveHandToObject, 0}, {HandSubAction::MoveHandToObject, 1}, {HandSubAction::MoveHandToObject, 2},
	    {HandSubAction::PickUpObject, 0},     {HandSubAction::PickUpObject, 1},     {HandSubAction::PickUpObject, 2}};
	EXPECT_EQ(runner.ran, expected);
}

TEST(ComputerHand, ItsCameraIsReportedWhereEachTurnOfStepsLeavesIt)
{
	ComputerHand hand;
	FakeRunner runner;
	Append(hand, {.type = HandSubAction::PickUpObject, .arguments = {ThingOf(static_cast<entt::entity>(7))}});
	EXPECT_FALSE(hand.reportedCamera.has_value());
	// a turn that only puts the move in front reports nothing
	hand.point = {1.0f, 2.0f, 3.0f};
	Step(hand, runner);
	EXPECT_FALSE(hand.reportedCamera.has_value());
	// the move's turns report where they leave the hand
	hand.point = {4.0f, 5.0f, 6.0f};
	Step(hand, runner);
	EXPECT_EQ(hand.reportedCamera, std::optional(glm::vec3(4.0f, 5.0f, 6.0f)));
	// and so does a turn with no steps left
	for (int turn = 0; turn < 5; ++turn)
	{
		Step(hand, runner);
	}
	ASSERT_TRUE(IsReady(hand));
	hand.point = {7.0f, 8.0f, 9.0f};
	Step(hand, runner);
	EXPECT_EQ(hand.reportedCamera, std::optional(glm::vec3(7.0f, 8.0f, 9.0f)));
}

TEST(ComputerHand, TheNextStepIsLookedAtForItsOwnMove)
{
	ComputerHand hand;
	FakeRunner runner;
	Append(hand, {.type = HandSubAction::MoveHandToPos, .arguments = {PointAt(1, 2, 3)}});
	Append(hand, {.type = HandSubAction::DropObjectInHandAtPos, .arguments = {PointAt(4, 5, 6)}});
	for (int turn = 0; turn < 3; ++turn)
	{
		Step(hand, runner);
	}
	ASSERT_EQ(hand.steps.size(), 1u);
	Step(hand, runner);
	ASSERT_EQ(hand.steps.size(), 2u);
	EXPECT_EQ(hand.steps.front().type, HandSubAction::MoveHandToPos);
	EXPECT_EQ(hand.steps.front().arguments.front().point.x, 4.0f);
}

TEST(ComputerHand, AFailedStageGivesUpEveryStepAndLetsGo)
{
	ComputerHand hand;
	FakeRunner runner;
	runner.results = {StageResult::Done, StageResult::Failed};
	Append(hand, {.type = HandSubAction::MoveHandToPos, .arguments = {PointAt(1, 2, 3)}});
	Append(hand, {.type = HandSubAction::MoveHandToPos, .arguments = {PointAt(4, 5, 6)}});
	hand.busy = true;
	EXPECT_TRUE(Step(hand, runner));
	EXPECT_FALSE(Step(hand, runner));
	EXPECT_TRUE(IsReady(hand));
	EXPECT_EQ(runner.letGo, 1);
	EXPECT_EQ(hand.stage, 0);
	EXPECT_TRUE(hand.checkPrerequisite);
	EXPECT_FALSE(hand.busy);
}

TEST(ComputerHand, AGoneThingClearsTheSteps)
{
	ComputerHand hand;
	FakeRunner runner;
	const auto scaffold = static_cast<entt::entity>(3);
	Append(hand, {.type = HandSubAction::PickUpObject, .arguments = {ThingOf(scaffold)}});
	SweepGone(hand, runner);
	EXPECT_FALSE(IsReady(hand));
	runner.gone.insert(scaffold);
	SweepGone(hand, runner);
	EXPECT_TRUE(IsReady(hand));
	EXPECT_EQ(runner.letGo, 1);
}

TEST(ComputerHand, AThingArgumentWithoutAThingClearsTheSteps)
{
	ComputerHand hand;
	FakeRunner runner;
	Append(hand, {.type = HandSubAction::PickUpObject, .arguments = {ThingOf(entt::null)}});
	SweepGone(hand, runner);
	EXPECT_TRUE(IsReady(hand));
}

TEST(ComputerHand, TheMapPlaceIsCutTowardZero)
{
	ComputerHand hand;
	hand.point = {1.0f, 50.0f, -1.0f};
	KeepMapPlace(hand);
	EXPECT_EQ(hand.mapX, 6553);
	EXPECT_EQ(hand.mapZ, -6553);
}

// The values below are the original's own, from its move functions run on the same hand (dev/tools/oracle), with the
// land at 5 m and the turn at 100 ms

TEST(ComputerHandMoves, AStepIsTheSpeedOverTheTurnsInASecond)
{
	EXPECT_EQ(StepLength(150.0f), 15.0f);
	EXPECT_EQ(StepLength(90.0f), 9.0f);
}

TEST(ComputerHandMoves, OverTheGroundAcrossTheLandHeldAboveIt)
{
	const FlatGround ground;
	auto hand = HandGoing({10.0f, 50.0f, 20.0f}, {100.0f, 7.0f, 60.0f}, 150.0f);
	EXPECT_EQ(MoveOverGround(hand, ground), StageResult::Going);
	EXPECT_EQ(hand.point.x, Bits(0x41BDA84Au));
	EXPECT_EQ(hand.point.y, 15.0f);
	EXPECT_EQ(hand.point.z, Bits(0x41D0BC93u));
}

TEST(ComputerHandMoves, OverTheGroundArrivesWithinAStep)
{
	const FlatGround ground;
	auto hand = HandGoing({10.0f, 50.0f, 20.0f}, {12.0f, 7.0f, 21.0f}, 150.0f);
	EXPECT_EQ(MoveOverGround(hand, ground), StageResult::Done);
	EXPECT_EQ(hand.point, glm::vec3(12.0f, 15.0f, 21.0f));
}

TEST(ComputerHandMoves, OverTheGroundRightAboveTheTargetArrives)
{
	const FlatGround ground;
	auto hand = HandGoing({10.0f, 50.0f, 20.0f}, {10.0f, 7.0f, 20.0f}, 150.0f);
	EXPECT_EQ(MoveOverGround(hand, ground), StageResult::Done);
	EXPECT_EQ(hand.point, glm::vec3(10.0f, 15.0f, 20.0f));
}

TEST(ComputerHandMoves, OverTheGroundAStepAwayStillSteps)
{
	// The game's distance across the land comes out a little over 15 here, so the hand steps onto the target and
	// arrives only on the next turn
	const FlatGround ground;
	auto hand = HandGoing({0.0f, 5.0f, 0.0f}, {15.0f, 15.0f, 0.0f}, 150.0f);
	EXPECT_EQ(MoveOverGround(hand, ground), StageResult::Going);
	EXPECT_EQ(hand.point, glm::vec3(15.0f, 15.0f, 0.0f));
}

TEST(ComputerHandMoves, StraightGoesUpAndDownToo)
{
	auto hand = HandGoing({10.0f, 50.0f, 20.0f}, {100.0f, 7.0f, 60.0f}, 150.0f);
	EXPECT_EQ(MoveStraight(hand), StageResult::Going);
	EXPECT_EQ(hand.point.x, Bits(0x41B47F24u));
	EXPECT_EQ(hand.point.y, Bits(0x422FFE12u));
	EXPECT_EQ(hand.point.z, Bits(0x41CCAA49u));
}

TEST(ComputerHandMoves, StraightArrivesAtThePointsOwnHeight)
{
	auto hand = HandGoing({10.0f, 50.0f, 20.0f}, {12.0f, 7.0f, 21.0f}, 150.0f);
	EXPECT_EQ(MoveStraight(hand), StageResult::Done);
	EXPECT_EQ(hand.point, glm::vec3(12.0f, 7.0f, 21.0f));
}

TEST(ComputerHandMoves, ToAThingFarAwayInLongStrides)
{
	const FlatGround ground;
	auto hand = HandGoing({0.0f, 0.0f, 0.0f}, {400.0f, 7.0f, 30.0f}, 150.0f);
	EXPECT_EQ(MoveToThing(hand, ground), StageResult::Going);
	EXPECT_EQ(hand.point.x, Bits(0x42C7709Bu));
	EXPECT_EQ(hand.point.y, 15.0f);
	EXPECT_EQ(hand.point.z, Bits(0x40EF53EDu));
}

TEST(ComputerHandMoves, ToAThingNearerInShortStrides)
{
	const FlatGround ground;
	auto hand = HandGoing({0.0f, 0.0f, 0.0f}, {200.0f, 7.0f, 30.0f}, 150.0f);
	EXPECT_EQ(MoveToThing(hand, ground), StageResult::Going);
	EXPECT_EQ(hand.point.x, Bits(0x411E3AD5u));
	EXPECT_EQ(hand.point.z, Bits(0x3FBDE033u));
}

TEST(ComputerHandMoves, ToAThingCloseAtTheHandsSpeed)
{
	const FlatGround ground;
	auto hand = HandGoing({0.0f, 0.0f, 0.0f}, {50.0f, 7.0f, 30.0f}, 150.0f);
	EXPECT_EQ(MoveToThing(hand, ground), StageResult::Going);
	EXPECT_EQ(hand.point.x, Bits(0x414DCC5Eu));
	EXPECT_EQ(hand.point.z, Bits(0x40F6F53Cu));
}

TEST(ComputerHandMoves, ToAThingWithinAStepArrives)
{
	const FlatGround ground;
	auto hand = HandGoing({0.0f, 0.0f, 0.0f}, {10.0f, 7.0f, 3.0f}, 150.0f);
	EXPECT_EQ(MoveToThing(hand, ground), StageResult::Done);
	EXPECT_EQ(hand.point, glm::vec3(10.0f, 15.0f, 3.0f));
	auto touching = HandGoing({0.0f, 0.0f, 0.0f}, {0.5f, 7.0f, 0.3f}, 150.0f);
	EXPECT_EQ(MoveToThing(touching, ground), StageResult::Done);
	EXPECT_EQ(touching.point, glm::vec3(0.5f, 15.0f, 0.3f));
}

TEST(ComputerHandMoves, PutAtHoversOrKeepsThePointsHeight)
{
	const FlatGround ground;
	ComputerHand hand;
	PutAt(hand, {1.0f, 40.0f, 2.0f}, &ground);
	EXPECT_EQ(hand.point, glm::vec3(1.0f, 15.0f, 2.0f));
	PutAt(hand, {1.0f, 40.0f, 2.0f}, nullptr);
	EXPECT_EQ(hand.point, glm::vec3(1.0f, 40.0f, 2.0f));
}

TEST(ComputerHandMoves, APointFromTheMapGrid)
{
	const FlatGround ground;
	EXPECT_EQ(PointFromMap(65536, -65536, 2.0f, ground), glm::vec3(10.0f, 7.0f, -10.0f));
}

TEST(ComputerHandScript, TakingTheHandLeavesOnlyTheScriptsStep)
{
	ComputerHand hand;
	FakeRunner runner;
	Append(hand, {.type = HandSubAction::MoveHandToObject, .arguments = {ThingOf(static_cast<entt::entity>(4))}});
	hand.stage = 1;
	TakeForScript(hand, StepToPoint(HandSubAction::MoveHandToPos, {1.0f, 2.0f, 3.0f}), 200.0f, runner);
	ASSERT_EQ(hand.steps.size(), 1u);
	EXPECT_EQ(hand.steps.front().type, HandSubAction::MoveHandToPos);
	EXPECT_EQ(hand.steps.front().arguments.front().kind, HandArgument::Kind::Point);
	EXPECT_EQ(hand.steps.front().arguments.front().point, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_EQ(hand.stage, 0);
	EXPECT_EQ(hand.speed, 200.0f);
	EXPECT_EQ(hand.playerSpeed, ComputerHand::k_StartingSpeed);
	EXPECT_TRUE(hand.held);
	EXPECT_TRUE(hand.busy);
	// The hand lets go as it gives up its steps, and again once it has the speed
	EXPECT_EQ(runner.letGo, 2);
	EXPECT_TRUE(MovedByScript(hand));
	EXPECT_TRUE(TakesItsTurn(hand));
}

TEST(ComputerHandScript, APauseHoldsTheHandAndGivesThePlayersSpeed)
{
	ComputerHand hand;
	SetSpeed(hand, 150.0f);
	hand.speed = 40.0f;
	Pause(hand, true);
	EXPECT_TRUE(hand.held);
	EXPECT_EQ(hand.speed, 150.0f);
	EXPECT_TRUE(TakesItsTurn(hand));
	Pause(hand, false);
	EXPECT_FALSE(hand.held);
	EXPECT_FALSE(TakesItsTurn(hand));
}

TEST(ComputerHandScript, TheSpeedIsThePlayersAndTheSteps)
{
	ComputerHand hand;
	SetSpeed(hand, 350.0f);
	EXPECT_EQ(hand.playerSpeed, 350.0f);
	EXPECT_EQ(hand.speed, 350.0f);
}

TEST(ComputerHandScript, AFreshHandIsAboveTheMiddleOfTheMap)
{
	const FlatGround ground;
	const ComputerHand hand;
	EXPECT_EQ(OnGround(hand, ground), glm::vec3(1285.0f, 5.0f, 1285.0f));
	EXPECT_FALSE(MovedByScript(hand));
	EXPECT_FALSE(TakesItsTurn(hand));
}

TEST(ComputerHandScript, TheLandUnderTheHandAsOfItsLastTurn)
{
	const FlatGround ground;
	ComputerHand hand;
	FakeRunner runner;
	hand.point = {100.0f, 60.0f, 200.0f};
	hand.held = true;
	Step(hand, runner);
	hand.point = {300.0f, 60.0f, 400.0f};
	EXPECT_EQ(OnGround(hand, ground), glm::vec3(100.0f, 5.0f, 200.0f));
}

TEST(ComputerHandHolding, APickUpTakesTheThingIntoTheHand)
{
	ComputerHand hand;
	FakeRunner runner;
	const auto villager = static_cast<entt::entity>(9);
	EXPECT_EQ(PickUpTurn(hand, villager, runner), StageResult::Done);
	EXPECT_EQ(hand.heldObject, villager);
	ASSERT_EQ(runner.taken.size(), 1u);
	EXPECT_EQ(runner.taken.front(), villager);
}

TEST(ComputerHandHolding, AFullHandLetsGoFirstAndTriesAgain)
{
	ComputerHand hand;
	FakeRunner runner;
	hand.heldObject = static_cast<entt::entity>(2);
	const auto scaffold = static_cast<entt::entity>(5);
	EXPECT_EQ(PickUpTurn(hand, scaffold, runner), StageResult::Going);
	EXPECT_EQ(runner.letGo, 1);
	EXPECT_TRUE(runner.taken.empty());
	EXPECT_EQ(PickUpTurn(hand, scaffold, runner), StageResult::Done);
	EXPECT_EQ(hand.heldObject, scaffold);
}

TEST(ComputerHandHolding, AThingThatMayNotBePickedUpFailsTheStep)
{
	ComputerHand hand;
	FakeRunner runner;
	const auto rock = static_cast<entt::entity>(6);
	runner.refused.insert(rock);
	EXPECT_EQ(PickUpTurn(hand, rock, runner), StageResult::Failed);
	EXPECT_EQ(hand.heldObject, entt::entity {entt::null});
}

TEST(ComputerHandHolding, AThingThatDoesNotGoInEndsTheStepEmptyHanded)
{
	ComputerHand hand;
	FakeRunner runner;
	runner.goesIn = false;
	EXPECT_EQ(PickUpTurn(hand, static_cast<entt::entity>(8), runner), StageResult::Done);
	EXPECT_EQ(hand.heldObject, entt::entity {entt::null});
}

TEST(ComputerHandHolding, AThingInAHandMayNotBePickedUp)
{
	const auto villager = static_cast<entt::entity>(9);
	const auto scaffold = static_cast<entt::entity>(5);
	const std::vector<entt::entity> computerHands {scaffold};
	EXPECT_TRUE(MayBePickedUp(villager, std::nullopt, computerHands));
	EXPECT_FALSE(MayBePickedUp(villager, villager, computerHands));
	EXPECT_FALSE(MayBePickedUp(scaffold, std::nullopt, computerHands));
	EXPECT_FALSE(MayBePickedUp(entt::null, std::nullopt, {}));
}

TEST(ComputerHandHolding, WhatTheHandLetsGoOfIsItsPlayersThrow)
{
	// the hand lets go through its own player's interface, a computer player's or any other's
	EXPECT_EQ(LetGoCreditedTo(PlayerNames::PLAYER_TWO), std::optional(PlayerNames::PLAYER_TWO));
	EXPECT_EQ(LetGoCreditedTo(PlayerNames::PLAYER_ONE), std::optional(PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(LetGoCreditedTo(PlayerNames::NEUTRAL).has_value());
}

TEST(ComputerHandHolding, TheDropLetsGo)
{
	ComputerHand hand;
	FakeRunner runner;
	hand.heldObject = static_cast<entt::entity>(2);
	EXPECT_EQ(DropTurn(hand, runner), StageResult::Done);
	EXPECT_EQ(runner.letGo, 1);
	EXPECT_EQ(hand.heldObject, entt::entity {entt::null});
}

TEST(ComputerHandHolding, WhatALetGoLeavesOverGoesBackIntoTheHand)
{
	// the rest of a scaffold put down gently goes into the hand that let go, as a pick-up's thing does
	ComputerHand hand;
	FakeRunner runner;
	const auto leftOver = static_cast<entt::entity>(6);
	EXPECT_TRUE(TakeLeftOver(hand, leftOver, runner));
	EXPECT_EQ(runner.taken, std::vector<entt::entity> {leftOver});
	EXPECT_EQ(hand.heldObject, leftOver);
	// nothing left over: nothing is taken
	ComputerHand empty;
	EXPECT_FALSE(TakeLeftOver(empty, entt::null, runner));
	EXPECT_EQ(runner.taken.size(), 1u);
	EXPECT_EQ(empty.heldObject, entt::entity {entt::null});
	// a thing that does not go in leaves the hand empty
	runner.goesIn = false;
	EXPECT_FALSE(TakeLeftOver(empty, leftOver, runner));
	EXPECT_EQ(empty.heldObject, entt::entity {entt::null});
}

TEST(ComputerHandHolding, GivingTheHeldThingIsOverInATurnTakenOrNot)
{
	ComputerHand hand;
	FakeRunner runner;
	const auto pot = static_cast<entt::entity>(4);
	const auto site = static_cast<entt::entity>(7);
	hand.heldObject = pot;
	runner.takesWhatIsGiven = false;
	EXPECT_EQ(GiveHeldTurn(hand, site, runner), StageResult::Done);
	EXPECT_EQ(runner.givenTo, std::vector<entt::entity> {site});
	EXPECT_EQ(hand.heldObject, pot);
	runner.takesWhatIsGiven = true;
	EXPECT_EQ(GiveHeldTurn(hand, site, runner), StageResult::Done);
	EXPECT_EQ(hand.heldObject, entt::entity {entt::null});
}

TEST(ComputerHandHolding, TheGiveWaitsForTheMoveToTheThing)
{
	// the give needs the move to its thing in front of it, as the game's table of steps gives it, and so do the takes
	// out of a container
	EXPECT_EQ(Prerequisite(HandSubAction::DropObjectInHandOnObject), HandSubAction::MoveHandToObject);
	EXPECT_EQ(Prerequisite(HandSubAction::CreateFoodFromContainer), HandSubAction::MoveHandToObject);
	EXPECT_EQ(Prerequisite(HandSubAction::CreateWoodFromContainer), HandSubAction::MoveHandToObject);
	EXPECT_EQ(Prerequisite(HandSubAction::Wait), HandSubAction::None);
}

TEST(ComputerHandContainer, AnEmptyHandTakesWhatIsAskedInANewPot)
{
	ComputerHand hand;
	FakeRunner runner;
	const auto pile = static_cast<entt::entity>(3);
	runner.resource[pile] = 500;
	EXPECT_EQ(TakeFromContainerTurn(hand, pile, 120, ResourceType::Food, runner), StageResult::Done);
	ASSERT_EQ(runner.made.size(), 1u);
	EXPECT_EQ(runner.made.front(), std::make_tuple(pile, ResourceType::Food, 120));
	EXPECT_EQ(runner.removed, (std::vector<std::tuple<entt::entity, int32_t>> {{pile, 120}}));
	EXPECT_EQ(runner.resource[pile], 380);
	EXPECT_EQ(hand.heldObject, static_cast<entt::entity>(101));
}

TEST(ComputerHandContainer, NoMoreThanTheContainerHas)
{
	ComputerHand hand;
	FakeRunner runner;
	const auto pile = static_cast<entt::entity>(3);
	runner.resource[pile] = 80;
	EXPECT_EQ(TakeFromContainerTurn(hand, pile, 120, ResourceType::Wood, runner), StageResult::Done);
	ASSERT_EQ(runner.made.size(), 1u);
	EXPECT_EQ(runner.made.front(), std::make_tuple(pile, ResourceType::Wood, 80));
	EXPECT_EQ(runner.resource[pile], 0);
}

TEST(ComputerHandContainer, APotOfItInTheHandIsFilledUpToTheAmount)
{
	ComputerHand hand;
	FakeRunner runner;
	const auto pile = static_cast<entt::entity>(3);
	const auto pot = static_cast<entt::entity>(4);
	runner.resource[pile] = 500;
	runner.resource[pot] = 50;
	runner.pots[pot] = ResourceType::Food;
	hand.heldObject = pot;
	EXPECT_EQ(TakeFromContainerTurn(hand, pile, 120, ResourceType::Food, runner), StageResult::Done);
	EXPECT_TRUE(runner.made.empty());
	EXPECT_EQ(runner.added, (std::vector<std::tuple<entt::entity, int32_t>> {{pot, 70}}));
	EXPECT_EQ(runner.resource[pot], 120);
	EXPECT_EQ(hand.heldObject, pot);
	// a pot that holds the amount already takes nothing, and the step is done
	EXPECT_EQ(TakeFromContainerTurn(hand, pile, 120, ResourceType::Food, runner), StageResult::Done);
	EXPECT_EQ(runner.added.size(), 1u);
	EXPECT_EQ(runner.removed.size(), 1u);
}

TEST(ComputerHandContainer, SomethingElseInTheHandMakesItWait)
{
	ComputerHand hand;
	FakeRunner runner;
	const auto pile = static_cast<entt::entity>(3);
	runner.resource[pile] = 500;
	// a villager, or a pot of the other resource
	hand.heldObject = static_cast<entt::entity>(9);
	EXPECT_EQ(TakeFromContainerTurn(hand, pile, 120, ResourceType::Food, runner), StageResult::Going);
	const auto wood = static_cast<entt::entity>(4);
	runner.pots[wood] = ResourceType::Wood;
	hand.heldObject = wood;
	EXPECT_EQ(TakeFromContainerTurn(hand, pile, 120, ResourceType::Food, runner), StageResult::Going);
	EXPECT_TRUE(runner.removed.empty());
	EXPECT_TRUE(runner.made.empty());
}

TEST(ComputerHandContainer, AnEmptyContainerFailsTheStepOfAnEmptyHand)
{
	ComputerHand hand;
	FakeRunner runner;
	const auto pile = static_cast<entt::entity>(3);
	EXPECT_EQ(TakeFromContainerTurn(hand, pile, 120, ResourceType::Food, runner), StageResult::Failed);
	EXPECT_TRUE(runner.removed.empty());
	// nothing asked for fails too, and so does a pot that could not be made
	runner.resource[pile] = 500;
	EXPECT_EQ(TakeFromContainerTurn(hand, pile, 0, ResourceType::Food, runner), StageResult::Failed);
	runner.makesPots = false;
	EXPECT_EQ(TakeFromContainerTurn(hand, pile, 120, ResourceType::Food, runner), StageResult::Failed);
	EXPECT_EQ(runner.removed.size(), 1u);
}

TEST(ComputerHandContainer, ThePotThatDoesNotGoInIsStillMade)
{
	ComputerHand hand;
	FakeRunner runner;
	const auto pile = static_cast<entt::entity>(3);
	runner.resource[pile] = 500;
	runner.goesIn = false;
	EXPECT_EQ(TakeFromContainerTurn(hand, pile, 120, ResourceType::Food, runner), StageResult::Done);
	EXPECT_EQ(hand.heldObject, entt::entity {entt::null});
}

TEST(ComputerHandWait, TheTurnsOfTheSecondsCutTowardZero)
{
	// the original's wait (oracle): 1 s 10 turns, 0.5 s 5, 2.5 s 25, 3.7 s 37, 0.33 s 3, 0.25 s 2, none 0
	EXPECT_EQ(WaitTurns(1.0f), 10);
	EXPECT_EQ(WaitTurns(0.5f), 5);
	EXPECT_EQ(WaitTurns(2.5f), 25);
	EXPECT_EQ(WaitTurns(3.7f), 37);
	EXPECT_EQ(WaitTurns(0.33f), 3);
	EXPECT_EQ(WaitTurns(0.25f), 2);
	EXPECT_EQ(WaitTurns(0.0f), 0);
}

TEST(ComputerHandWait, ASecondGoesOnForTenTurns)
{
	ComputerHand hand;
	StartWait(hand, 1.0f);
	EXPECT_EQ(hand.waitTurns, 10);
	for (int turn = 1; turn < 10; ++turn)
	{
		EXPECT_EQ(WaitTurn(hand), StageResult::Going) << turn;
	}
	EXPECT_EQ(WaitTurn(hand), StageResult::Done);
	EXPECT_EQ(hand.waitTurns, 0);
}

TEST(ComputerHandWait, AWaitOfNoTurnsGoesOn)
{
	// the original's wait of 0 s counts below zero and goes on (oracle: still going after 40 turns, at -40)
	ComputerHand hand;
	StartWait(hand, 0.0f);
	for (int turn = 0; turn < 40; ++turn)
	{
		EXPECT_EQ(WaitTurn(hand), StageResult::Going);
	}
	EXPECT_EQ(hand.waitTurns, -40);
}

TEST(ComputerHandHeldDraw, ATurnOfStepsKeepsWhereItBegan)
{
	ComputerHand hand;
	FakeRunner runner;
	hand.point = {1.0f, 2.0f, 3.0f};
	hand.target = {9.0f, 2.0f, 3.0f};
	Append(hand, StepToPoint(HandSubAction::MoveHandToPos, {9.0f, 2.0f, 3.0f}));
	Step(hand, runner);
	EXPECT_EQ(hand.turnStartPoint, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_EQ(hand.stepTurns, 1u);
}

TEST(ComputerHandHeldDraw, WithoutTimeTheThingIsAtTheHand)
{
	EXPECT_EQ(SlideOfHeld({0.0f, 0.0f, 0.0f}, {10.0f, 20.0f, 30.0f}, 50, 0), glm::vec3(10.0f, 20.0f, 30.0f));
}

TEST(ComputerHandHeldDraw, ItSlidesOverTheTimeAndStopsAtTheHand)
{
	const glm::vec3 start {0.0f, 0.0f, 0.0f};
	const glm::vec3 hand {10.0f, 20.0f, 30.0f};
	EXPECT_EQ(SlideOfHeld(start, hand, 25, 100), glm::vec3(2.5f, 5.0f, 7.5f));
	EXPECT_EQ(SlideOfHeld(start, hand, 150, 100), hand);
}

TEST(ComputerHandHeldDraw, ATurnStartsTheSlideAtTheTurnsPoint)
{
	HeldDraw draw;
	EXPECT_EQ(HeldDrawPoint(draw, 30), glm::vec3(0.0f));
	EXPECT_EQ(HeldDrawPoint(draw, 40), glm::vec3(0.0f));
	NewTurnsOfTheHand(draw, {5.0f, 6.0f, 7.0f}, 1);
	// The time drawn in the turn before is the slide's, and the slide goes from the point to the same point
	EXPECT_EQ(draw.durationMs, 70);
	EXPECT_EQ(draw.elapsedMs, 0);
	EXPECT_EQ(draw.start, draw.hand);
	EXPECT_EQ(HeldDrawPoint(draw, 20), glm::vec3(5.0f, 6.0f, 7.0f));
	// Two turns with no frame between keep no time
	NewTurnsOfTheHand(draw, {8.0f, 6.0f, 7.0f}, 2);
	EXPECT_EQ(draw.durationMs, 0);
	EXPECT_EQ(HeldDrawPoint(draw, 20), glm::vec3(8.0f, 6.0f, 7.0f));
	// No turn: nothing changes
	NewTurnsOfTheHand(draw, {1.0f, 1.0f, 1.0f}, 0);
	EXPECT_EQ(draw.hand, glm::vec3(8.0f, 6.0f, 7.0f));
}

// With the View menu's smooth drawing (not as the original) the thing slides from where the hand was a turn before
TEST(ComputerHandHeldDraw, SmoothItSlidesFromTheTurnBefore)
{
	HeldDraw draw;
	NewTurnsOfTheHand(draw, {0.0f, 0.0f, 0.0f}, 1, true);
	EXPECT_EQ(HeldDrawPoint(draw, 100), glm::vec3(0.0f));
	NewTurnsOfTheHand(draw, {10.0f, 20.0f, 30.0f}, 1, true);
	EXPECT_EQ(draw.start, glm::vec3(0.0f));
	EXPECT_EQ(draw.durationMs, 100);
	EXPECT_EQ(HeldDrawPoint(draw, 25), glm::vec3(2.5f, 5.0f, 7.5f));
	EXPECT_EQ(HeldDrawPoint(draw, 100), glm::vec3(10.0f, 20.0f, 30.0f));
}

TEST(ComputerHandHeldDraw, ThroughTheMapGrid)
{
	const FlatGround ground;
	EXPECT_EQ(OnTheMapGrid({10.0f, 15.0f, -20.0f}, ground), glm::vec3(10.0f, 15.0f, -20.0f));
	EXPECT_EQ(OnTheMapGrid({10.00001f, 15.0f, 0.0f}, ground).x, 10.0f);
}

TEST(ComputerHandActions, NamesAreFoundAsTheGameFindsThem)
{
	EXPECT_EQ(FindMindAction("FindPlaceToUseScaffoldForBuilding"), MindAction::FindPlaceToUseScaffoldForBuilding);
	EXPECT_EQ(FindMindAction("DIRECTPickupAndDropObject"), MindAction::DirectPickupAndDropObject);
	// A name only has to begin with the node's name
	EXPECT_EQ(FindMindAction("DIRECTPickupAndDropObject: 182 "), MindAction::DirectPickupAndDropObject);
	EXPECT_EQ(FindMindAction("ShallCastWoodSpell"), MindAction::CastIconsMiracleOnTownStore);
	EXPECT_EQ(FindMindAction("CastFoodSpellOn"), MindAction::NotPorted);
	EXPECT_EQ(FindMindAction("DestroyTown"), MindAction::NotPorted);
	EXPECT_EQ(FindMindAction("NoSuchAction"), std::nullopt);
	// The node's whole name must be there
	EXPECT_EQ(FindMindAction("DIRECTPickupAndDrop"), std::nullopt);
}

TEST(ComputerHandActions, DirectInAnyCase)
{
	EXPECT_TRUE(IsDirectAction("DIRECTPickupAndDropObject"));
	EXPECT_TRUE(IsDirectAction("directSomething"));
	EXPECT_FALSE(IsDirectAction("DIREC"));
	EXPECT_FALSE(IsDirectAction("FindPlaceToUseScaffoldForBuilding"));
}

TEST(ComputerHandActions, TheDirectPickUpAndDropGivesUpTheStepsAndGivesTwo)
{
	ComputerHand hand;
	FakeRunner runner;
	auto mind = computer_player::MakeMind();
	Append(hand, StepToPoint(HandSubAction::MoveHandToPos, {1.0f, 2.0f, 3.0f}));
	const auto villager = static_cast<entt::entity>(9);
	const auto marker = static_cast<entt::entity>(10);
	FakeFacts facts;
	facts.objects.insert(villager);
	facts.markers[marker] = glm::vec3(40.0f, 0.0f, 50.0f);
	EXPECT_TRUE(ForceAction(hand, mind, "DIRECTPickupAndDropObject", villager, marker, 0, runner, facts));
	ASSERT_EQ(hand.steps.size(), 2u);
	EXPECT_EQ(hand.steps.at(0).type, HandSubAction::PickUpObject);
	EXPECT_EQ(hand.steps.at(0).arguments.at(0).thing, villager);
	EXPECT_EQ(hand.steps.at(1).type, HandSubAction::DropObjectInHandAtPos);
	EXPECT_EQ(hand.steps.at(1).arguments.at(0).point, glm::vec3(40.0f, 0.0f, 50.0f));
	EXPECT_TRUE(hand.busy);
	EXPECT_FALSE(hand.held);
}

TEST(ComputerHandActions, TheDirectActionNeedsAnObjectAndAMarker)
{
	ComputerHand hand;
	FakeRunner runner;
	auto mind = computer_player::MakeMind();
	Append(hand, StepToPoint(HandSubAction::MoveHandToPos, {1.0f, 2.0f, 3.0f}));
	FakeFacts facts;
	facts.objects.insert(static_cast<entt::entity>(9));
	EXPECT_FALSE(
	    ForceAction(hand, mind, "DIRECTPickupAndDropObject", static_cast<entt::entity>(9), entt::null, 0, runner, facts));
	// It gave up the steps before it looked
	EXPECT_TRUE(IsReady(hand));
	EXPECT_FALSE(hand.busy);
}

TEST(ComputerHandActions, TheScaffoldGoesToTheMarker)
{
	ComputerHand hand;
	FakeRunner runner;
	auto mind = computer_player::MakeMind();
	const auto scaffold = static_cast<entt::entity>(5);
	const auto marker = static_cast<entt::entity>(11);
	FakeFacts facts;
	facts.markers[marker] = glm::vec3(7.0f);
	EXPECT_TRUE(ForceAction(hand, mind, "FindPlaceToUseScaffoldForBuilding", scaffold, marker, 0, runner, facts));
	ASSERT_EQ(hand.steps.size(), 2u);
	EXPECT_EQ(hand.steps.at(0).arguments.at(0).thing, scaffold);
	EXPECT_EQ(hand.steps.at(1).arguments.at(0).point, glm::vec3(7.0f));
	// Without a marker the place is weighed, which needs a way of scoring openblack lacks: nothing
	ComputerHand other;
	auto fresh = computer_player::MakeMind();
	EXPECT_FALSE(ForceAction(other, fresh, "FindPlaceToUseScaffoldForBuilding", scaffold, entt::null, 0, runner, facts));
	// a thing that may not be picked up gives nothing either
	runner.refused.insert(scaffold);
	EXPECT_FALSE(ForceAction(other, fresh, "FindPlaceToUseScaffoldForBuilding", scaffold, marker, 0, runner, facts));
	EXPECT_TRUE(IsReady(other));
	EXPECT_FALSE(other.busy);
}

TEST(ComputerHandActions, TheScaffoldGivenAThingThatIsNoMarkerGoesToTheChildsPointZeroUntilSet)
{
	// the child takes the second thing and is chosen, but no point: the nodes start at 0, so the drop is at 0
	ComputerHand hand;
	FakeRunner runner;
	auto mind = computer_player::MakeMind();
	const FakeFacts facts;
	const auto scaffold = static_cast<entt::entity>(5);
	EXPECT_TRUE(ForceAction(hand, mind, "FindPlaceToUseScaffoldForBuilding", scaffold, static_cast<entt::entity>(12), 0, runner,
	                        facts));
	ASSERT_EQ(hand.steps.size(), 2u);
	EXPECT_EQ(hand.steps.at(0).arguments.at(0).thing, scaffold);
	EXPECT_EQ(hand.steps.at(1).arguments.at(0).point, glm::vec3(0.0f));
	// a point a marker gave the child before stays for the next such force
	ComputerHand again;
	FakeFacts withMarker;
	withMarker.markers[static_cast<entt::entity>(11)] = glm::vec3(7.0f);
	EXPECT_TRUE(ForceAction(again, mind, "FindPlaceToUseScaffoldForBuilding", scaffold, static_cast<entt::entity>(11), 0,
	                        runner, withMarker));
	ComputerHand later;
	EXPECT_TRUE(ForceAction(later, mind, "FindPlaceToUseScaffoldForBuilding", scaffold, static_cast<entt::entity>(12), 0,
	                        runner, facts));
	EXPECT_EQ(later.steps.at(1).arguments.at(0).point, glm::vec3(7.0f));
}

TEST(ComputerHandActions, ADirectNodeKeepsOnlyASecondThingWithAPlace)
{
	ComputerHand hand;
	FakeRunner runner;
	auto mind = computer_player::MakeMind();
	FakeFacts facts;
	const auto villager = static_cast<entt::entity>(9);
	const auto town = static_cast<entt::entity>(13);
	facts.objects.insert(villager);
	facts.placeless.insert(town);
	EXPECT_FALSE(ForceAction(hand, mind, "DIRECTPickupAndDropObject", villager, town, 0, runner, facts));
	EXPECT_EQ(mind.nodes.at(6).secondThing, entt::entity {entt::null});
}

TEST(ComputerHandActions, ANodeNotPortedOrAMissingNameDoesNothing)
{
	ComputerHand hand;
	FakeRunner runner;
	auto mind = computer_player::MakeMind();
	const FakeFacts facts;
	Append(hand, StepToPoint(HandSubAction::MoveHandToPos, {1.0f, 2.0f, 3.0f}));
	EXPECT_FALSE(ForceAction(hand, mind, "SlapCreature", static_cast<entt::entity>(20), static_cast<entt::entity>(21), 0,
	                         runner, facts));
	EXPECT_FALSE(ForceAction(hand, mind, "NoSuchAction", entt::null, entt::null, 0, runner, facts));
	EXPECT_EQ(hand.steps.size(), 1u);
	// even once a weighing chose it, an action openblack lacks is not run, and its node is not held back
	const auto node = computer_player::FindNode("SlapCreature");
	ASSERT_TRUE(node.has_value());
	mind.nodes.at(*node).chosen = *node;
	EXPECT_FALSE(ForceAction(hand, mind, "SlapCreature", static_cast<entt::entity>(20), static_cast<entt::entity>(21), 0,
	                         runner, facts));
	EXPECT_EQ(hand.steps.size(), 1u);
	EXPECT_EQ(mind.nodes.at(*node).suppression, 1.0f);
}

TEST(ComputerHandActions, TheQueueKeepsItsOrderAndDropsGoneThings)
{
	ComputerHand hand;
	FakeRunner runner;
	const auto town = static_cast<entt::entity>(20);
	const auto icon = static_cast<entt::entity>(21);
	QueueAction(hand, "ShallCastWoodSpell", town, icon);
	QueueAction(hand, "DIRECTPickupAndDropObject", static_cast<entt::entity>(22), entt::null);
	QueueAction(hand, "FindPlaceToUseScaffoldForBuilding", entt::null, entt::null);
	runner.gone.insert(icon);
	SweepQueue(hand, runner);
	ASSERT_EQ(hand.queue.size(), 2u);
	const auto next = TakeNext(hand);
	ASSERT_TRUE(next.has_value());
	EXPECT_EQ(next->name, "DIRECTPickupAndDropObject");
	EXPECT_EQ(hand.queue.size(), 1u);
	EXPECT_TRUE(TakeNext(hand).has_value());
	EXPECT_FALSE(TakeNext(hand).has_value());
}

TEST(ComputerHandActions, AVillagerIsTakenToTheGateHeldASecondAndDropped)
{
	// the villager is the node's, the worship site the nearest above its parent; the steps the original's action gives
	ComputerHand hand;
	hand.playerSpeed = 120.0f;
	FakeRunner runner;
	FakeFacts facts;
	const auto villager = static_cast<entt::entity>(9);
	const auto site = static_cast<entt::entity>(20);
	facts.villagers.insert(villager);
	facts.sites.insert(site);
	facts.gates[site] = glm::vec3(30.0f, 4.0f, 50.0f);
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParent("TakeVillagerFromTownToWorshipSite");
	mind.nodes.at(node).thing = villager;
	mind.nodes.at(parent).thing = site;
	HandActions actions(hand, runner, facts);
	EXPECT_TRUE(actions.HasAction(computer_player::ActionKind::TakeVillagerFromTownToWorshipSite));
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::TakeVillagerFromTownToWorshipSite, mind, node));
	ASSERT_EQ(hand.steps.size(), 4u);
	EXPECT_EQ(hand.steps.at(0).type, HandSubAction::PickUpObject);
	EXPECT_EQ(hand.steps.at(0).arguments.at(0).thing, villager);
	EXPECT_EQ(hand.steps.at(0).arguments.at(1).floatValue, 120.0f);
	EXPECT_EQ(hand.steps.at(1).type, HandSubAction::MoveHandToPos);
	EXPECT_EQ(hand.steps.at(1).arguments.at(0).point, glm::vec3(30.0f, 4.0f, 50.0f));
	EXPECT_EQ(hand.steps.at(1).arguments.at(1).floatValue, 900.0f);
	EXPECT_EQ(hand.steps.at(2).type, HandSubAction::Wait);
	EXPECT_EQ(hand.steps.at(2).arguments.at(0).floatValue, 1.0f);
	EXPECT_EQ(hand.steps.at(3).type, HandSubAction::DropObjectInHandAtPos);
	EXPECT_EQ(hand.steps.at(3).arguments.at(0).point, glm::vec3(30.0f, 4.0f, 50.0f));
	EXPECT_EQ(hand.steps.at(3).arguments.at(1).floatValue, 120.0f);
}

TEST(ComputerHandActions, NoGateOrAVillagerInAHandIsDoneWithNoSteps)
{
	// the original's action is done (its suppression the row's after success) though it gave the hand nothing
	FakeRunner runner;
	FakeFacts facts;
	const auto villager = static_cast<entt::entity>(9);
	const auto site = static_cast<entt::entity>(20);
	facts.villagers.insert(villager);
	facts.sites.insert(site);
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParent("TakeVillagerFromTownToWorshipSite");
	mind.nodes.at(node).thing = villager;
	mind.nodes.at(parent).thing = site;
	ComputerHand noGate;
	HandActions withoutGate(noGate, runner, facts);
	EXPECT_TRUE(withoutGate.RunAction(computer_player::ActionKind::TakeVillagerFromTownToWorshipSite, mind, node));
	EXPECT_TRUE(noGate.steps.empty());
	facts.gates[site] = glm::vec3(1.0f);
	runner.refused.insert(villager);
	ComputerHand refused;
	HandActions withRefusal(refused, runner, facts);
	EXPECT_TRUE(withRefusal.RunAction(computer_player::ActionKind::TakeVillagerFromTownToWorshipSite, mind, node));
	EXPECT_TRUE(refused.steps.empty());
}

TEST(ComputerHandActions, NoVillagerOrNoWorshipSiteFails)
{
	FakeRunner runner;
	FakeFacts facts;
	const auto villager = static_cast<entt::entity>(9);
	const auto site = static_cast<entt::entity>(20);
	facts.gates[site] = glm::vec3(1.0f);
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParent("TakeVillagerFromTownToWorshipSite");
	mind.nodes.at(node).thing = villager;
	mind.nodes.at(parent).thing = site;
	ComputerHand hand;
	HandActions actions(hand, runner, facts);
	// neither is known as what it must be
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::TakeVillagerFromTownToWorshipSite, mind, node));
	facts.villagers.insert(villager);
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::TakeVillagerFromTownToWorshipSite, mind, node));
	facts.villagers.clear();
	facts.sites.insert(site);
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::TakeVillagerFromTownToWorshipSite, mind, node));
	EXPECT_TRUE(hand.steps.empty());
	EXPECT_EQ(FindMindAction("TakeVillagerFromTownToWorshipSite"), MindAction::TakeVillagerFromTownToWorshipSite);
}

TEST(ComputerHandActions, FoodIsFetchedFromThePileAndGivenToTheObject)
{
	// the pile is the node's, the object the nearest above its parent, the amount the parent's number; the steps the
	// original's action gives
	ComputerHand hand;
	FakeRunner runner;
	FakeFacts facts;
	const auto pile = static_cast<entt::entity>(3);
	const auto site = static_cast<entt::entity>(20);
	facts.objects.insert(pile);
	facts.objects.insert(site);
	facts.places[pile] = glm::vec3(60.0f, 2.0f, 70.0f);
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParent("GetFoodFromOtherTownForObject");
	mind.nodes.at(node).thing = pile;
	mind.nodes.at(parent).thing = site;
	mind.nodes.at(parent).intValue = 240;
	HandActions actions(hand, runner, facts);
	EXPECT_TRUE(actions.HasAction(computer_player::ActionKind::GetFoodFromOtherTownForObject));
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::GetFoodFromOtherTownForObject, mind, node));
	ASSERT_EQ(hand.steps.size(), 3u);
	EXPECT_EQ(hand.steps.at(0).type, HandSubAction::MoveHandToPos);
	ASSERT_EQ(hand.steps.at(0).arguments.size(), 1u);
	EXPECT_EQ(hand.steps.at(0).arguments.at(0).point, glm::vec3(60.0f, 2.0f, 70.0f));
	EXPECT_EQ(hand.steps.at(1).type, HandSubAction::CreateFoodFromContainer);
	EXPECT_EQ(hand.steps.at(1).arguments.at(0).thing, pile);
	EXPECT_EQ(hand.steps.at(1).arguments.at(1).intValue, 240);
	EXPECT_EQ(hand.steps.at(2).type, HandSubAction::DropObjectInHandOnObject);
	EXPECT_EQ(hand.steps.at(2).arguments.at(0).thing, site);
}

TEST(ComputerHandActions, NoPileOrNoObjectToFeedFails)
{
	FakeRunner runner;
	FakeFacts facts;
	const auto pile = static_cast<entt::entity>(3);
	const auto site = static_cast<entt::entity>(20);
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParent("GetFoodFromOtherTownForObject");
	mind.nodes.at(node).thing = pile;
	mind.nodes.at(parent).thing = site;
	ComputerHand hand;
	HandActions actions(hand, runner, facts);
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::GetFoodFromOtherTownForObject, mind, node));
	facts.objects.insert(pile);
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::GetFoodFromOtherTownForObject, mind, node));
	facts.objects.clear();
	facts.objects.insert(site);
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::GetFoodFromOtherTownForObject, mind, node));
	EXPECT_TRUE(hand.steps.empty());
	EXPECT_EQ(FindMindAction("GetFoodFromOtherTownForObject"), MindAction::GetFoodFromOtherTownForObject);
}

TEST(ComputerHandActions, AllThePilesWoodIsFetchedForTheThingAbove)
{
	// the original's steps: the take of all the pile's wood (its move in front of it comes with it), then the give
	ComputerHand hand;
	FakeRunner runner;
	FakeFacts facts;
	const auto pile = static_cast<entt::entity>(3);
	const auto town = static_cast<entt::entity>(20);
	facts.resource[pile] = 350;
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParent("GetWoodFromOtherTown");
	mind.nodes.at(node).thing = pile;
	mind.nodes.at(parent).thing = town;
	mind.nodes.at(parent).intValue = 40;
	HandActions actions(hand, runner, facts);
	EXPECT_TRUE(actions.HasAction(computer_player::ActionKind::GetWoodFromOtherTown));
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::GetWoodFromOtherTown, mind, node));
	ASSERT_EQ(hand.steps.size(), 2u);
	EXPECT_EQ(hand.steps.at(0).type, HandSubAction::CreateWoodFromContainer);
	EXPECT_EQ(hand.steps.at(0).arguments.at(0).thing, pile);
	EXPECT_EQ(hand.steps.at(0).arguments.at(1).intValue, 350);
	EXPECT_EQ(hand.steps.at(1).type, HandSubAction::DropObjectInHandOnObject);
	EXPECT_EQ(hand.steps.at(1).arguments.at(0).thing, town);
	EXPECT_EQ(FindMindAction("GetWoodFromOtherTown"), MindAction::GetWoodFromOtherTown);
}

TEST(ComputerHandActions, ABuildingSiteAboveGetsTheWoodAtItsBuilding)
{
	// a building site has no place of its own: the wood goes to its building, and a site without one fails
	ComputerHand hand;
	FakeRunner runner;
	FakeFacts facts;
	const auto pile = static_cast<entt::entity>(3);
	const auto site = static_cast<entt::entity>(30);
	const auto abode = static_cast<entt::entity>(31);
	facts.resource[pile] = 50;
	facts.placeless.insert(site);
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParent("GetWoodFromOtherTown");
	mind.nodes.at(node).thing = pile;
	mind.nodes.at(parent).thing = site;
	HandActions actions(hand, runner, facts);
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::GetWoodFromOtherTown, mind, node));
	EXPECT_TRUE(hand.steps.empty());
	facts.siteBuildings[site] = abode;
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::GetWoodFromOtherTown, mind, node));
	ASSERT_EQ(hand.steps.size(), 2u);
	EXPECT_EQ(hand.steps.at(1).arguments.at(0).thing, abode);
}

TEST(ComputerHandActions, NoPileOrAPileWithNoWoodFails)
{
	FakeRunner runner;
	FakeFacts facts;
	const auto pile = static_cast<entt::entity>(3);
	const auto town = static_cast<entt::entity>(20);
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParent("GetWoodFromOtherTown");
	mind.nodes.at(parent).thing = town;
	ComputerHand hand;
	HandActions actions(hand, runner, facts);
	// no pile at all: the parent's town is the nearest thing, which holds no wood
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::GetWoodFromOtherTown, mind, node));
	mind.nodes.at(node).thing = pile;
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::GetWoodFromOtherTown, mind, node));
	facts.placeless.insert(pile);
	facts.resource[pile] = 10;
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::GetWoodFromOtherTown, mind, node));
	EXPECT_TRUE(hand.steps.empty());
}

TEST(ComputerHandActions, FoodForAStoreIsTheNodesOwnAmountCutToAWholeNumber)
{
	// the original's steps: the move to the pile, the take of the node's own amount cut toward zero (not its parent's
	// number), and the give to where the town above takes the pile's resource
	ComputerHand hand;
	FakeRunner runner;
	FakeFacts facts;
	const auto pile = static_cast<entt::entity>(3);
	const auto town = static_cast<entt::entity>(20);
	const auto pit = static_cast<entt::entity>(21);
	facts.objects.insert(pile);
	facts.towns.insert(town);
	facts.types[pile] = ResourceType::Food;
	facts.drops[town] = pit;
	facts.places[pile] = glm::vec3(60.0f, 2.0f, 70.0f);
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParent("GetFoodFromOtherTownForStoragePit");
	mind.nodes.at(node).thing = pile;
	mind.nodes.at(node).floatValue = 712.9f;
	mind.nodes.at(parent).thing = town;
	mind.nodes.at(parent).intValue = 40;
	HandActions actions(hand, runner, facts);
	EXPECT_TRUE(actions.HasAction(computer_player::ActionKind::GetFoodFromOtherTownForStoragePit));
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::GetFoodFromOtherTownForStoragePit, mind, node));
	ASSERT_EQ(hand.steps.size(), 3u);
	EXPECT_EQ(hand.steps.at(0).type, HandSubAction::MoveHandToPos);
	EXPECT_EQ(hand.steps.at(0).arguments.at(0).point, glm::vec3(60.0f, 2.0f, 70.0f));
	EXPECT_EQ(hand.steps.at(1).type, HandSubAction::CreateFoodFromContainer);
	EXPECT_EQ(hand.steps.at(1).arguments.at(0).thing, pile);
	EXPECT_EQ(hand.steps.at(1).arguments.at(1).intValue, 712);
	EXPECT_EQ(hand.steps.at(2).type, HandSubAction::DropObjectInHandOnObject);
	EXPECT_EQ(hand.steps.at(2).arguments.at(0).thing, pit);
	EXPECT_EQ(facts.asked, (std::vector<std::tuple<entt::entity, ResourceType>> {{town, ResourceType::Food}}));
	EXPECT_EQ(FindMindAction("GetFoodFromOtherTownForStoragePit"), MindAction::GetFoodFromOtherTownForStoragePit);
}

TEST(ComputerHandActions, FoodForAStoreNeedsAPileATownAndSomewhereToTakeIt)
{
	FakeRunner runner;
	FakeFacts facts;
	const auto pile = static_cast<entt::entity>(3);
	const auto town = static_cast<entt::entity>(20);
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParent("GetFoodFromOtherTownForStoragePit");
	mind.nodes.at(node).thing = pile;
	mind.nodes.at(parent).thing = town;
	ComputerHand hand;
	HandActions actions(hand, runner, facts);
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::GetFoodFromOtherTownForStoragePit, mind, node));
	facts.objects.insert(pile);
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::GetFoodFromOtherTownForStoragePit, mind, node));
	// a town with neither a store nor a temporary pot
	facts.towns.insert(town);
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::GetFoodFromOtherTownForStoragePit, mind, node));
	EXPECT_EQ(facts.asked.size(), 1u);
	EXPECT_TRUE(hand.steps.empty());
}

TEST(ComputerHandActions, TheQueuedWoodMiracleIsCastOnlyOnceAWeighingReachedItsNode)
{
	// Land 2's script queues the wood miracle on a town and an icon: the force names the leaf, which runs only when a
	// weighing of the mind chose it; far from the icon the hand makes the gesture, the worshippers chant, and the
	// miracle is cast at the town's store
	ComputerHand hand;
	hand.mapX = 0;
	hand.mapZ = 0;
	FakeRunner runner;
	FakeFacts facts;
	const auto town = static_cast<entt::entity>(20);
	const auto icon = static_cast<entt::entity>(21);
	const auto pit = static_cast<entt::entity>(22);
	facts.towns.insert(town);
	facts.drops[town] = pit;
	facts.icons[icon] = MagicType::Wood;
	facts.mapPlaces[icon] = {.x = 25 << 16, .z = 0};
	auto mind = computer_player::MakeMind();
	EXPECT_FALSE(ForceAction(hand, mind, "ShallCastWoodSpell", town, icon, 0, runner, facts));
	EXPECT_TRUE(hand.steps.empty());
	EXPECT_FALSE(hand.busy);
	const auto [node, parent] = NodeAndParent("ShallCastWoodSpell");
	mind.nodes.at(node).chosen = node;
	EXPECT_TRUE(ForceAction(hand, mind, "ShallCastWoodSpell", town, icon, 0, runner, facts));
	EXPECT_EQ(mind.nodes.at(parent).thing, town);
	EXPECT_EQ(mind.nodes.at(node).thing, icon);
	ASSERT_EQ(hand.steps.size(), 3u);
	EXPECT_EQ(hand.steps.at(0).type, HandSubAction::MakeGesture);
	ASSERT_EQ(hand.steps.at(0).arguments.size(), 2u);
	EXPECT_EQ(hand.steps.at(0).arguments.at(0).kind, HandArgument::Kind::Int);
	EXPECT_EQ(hand.steps.at(0).arguments.at(0).intValue, static_cast<int32_t>(MagicType::Wood));
	EXPECT_EQ(hand.steps.at(0).arguments.at(1).intValue, 0);
	EXPECT_EQ(hand.steps.at(1).type, HandSubAction::MakeWorshippersChantForSpell);
	ASSERT_EQ(hand.steps.at(1).arguments.size(), 2u);
	EXPECT_EQ(hand.steps.at(1).arguments.at(0).intValue, static_cast<int32_t>(MagicType::Wood));
	EXPECT_EQ(hand.steps.at(1).arguments.at(1).thing, icon);
	EXPECT_EQ(hand.steps.at(2).type, HandSubAction::CastSpellAtObject);
	ASSERT_EQ(hand.steps.at(2).arguments.size(), 3u);
	EXPECT_EQ(hand.steps.at(2).arguments.at(0).thing, pit);
	EXPECT_EQ(hand.steps.at(2).arguments.at(1).intValue, static_cast<int32_t>(MagicType::Wood));
	EXPECT_EQ(hand.steps.at(2).arguments.at(2).thing, icon);
	EXPECT_EQ(facts.asked, (std::vector<std::tuple<entt::entity, ResourceType>> {{town, ResourceType::Wood}}));
	EXPECT_TRUE(hand.busy);
}

TEST(ComputerHandActions, NearItsIconTheHandGoesToItAndAFoodMiracleGoesWhereTheTownTakesFood)
{
	ComputerHand hand;
	hand.mapX = 0;
	hand.mapZ = 0;
	FakeRunner runner;
	FakeFacts facts;
	const auto town = static_cast<entt::entity>(20);
	const auto icon = static_cast<entt::entity>(21);
	const auto pit = static_cast<entt::entity>(22);
	facts.towns.insert(town);
	facts.drops[town] = pit;
	facts.icons[icon] = MagicType::Food;
	facts.mapPlaces[icon] = {.x = 0, .z = 15 << 16};
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParent("ShallCastFoodSpell");
	mind.nodes.at(node).thing = icon;
	mind.nodes.at(parent).thing = town;
	HandActions actions(hand, runner, facts);
	EXPECT_TRUE(actions.HasAction(computer_player::ActionKind::ShallCastFoodSpell));
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::ShallCastFoodSpell, mind, node));
	ASSERT_EQ(hand.steps.size(), 3u);
	EXPECT_EQ(hand.steps.at(0).type, HandSubAction::MoveHandToObject);
	ASSERT_EQ(hand.steps.at(0).arguments.size(), 1u);
	EXPECT_EQ(hand.steps.at(0).arguments.at(0).thing, icon);
	EXPECT_EQ(hand.steps.at(1).type, HandSubAction::MakeWorshippersChantForSpell);
	EXPECT_EQ(hand.steps.at(2).type, HandSubAction::CastSpellAtObject);
	EXPECT_EQ(hand.steps.at(2).arguments.at(0).thing, pit);
	EXPECT_EQ(facts.asked, (std::vector<std::tuple<entt::entity, ResourceType>> {{town, ResourceType::Food}}));
}

TEST(ComputerHandActions, TheTownsMiracleNeedsAWorshipIconATownAndAStore)
{
	FakeRunner runner;
	FakeFacts facts;
	const auto town = static_cast<entt::entity>(20);
	const auto icon = static_cast<entt::entity>(21);
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParent("ShallCastWoodSpell");
	mind.nodes.at(node).thing = icon;
	mind.nodes.at(parent).thing = town;
	ComputerHand hand;
	HandActions actions(hand, runner, facts);
	// no icon
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::ShallCastFoodSpell, mind, node));
	// no town
	facts.icons[icon] = MagicType::Wood;
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::ShallCastFoodSpell, mind, node));
	EXPECT_TRUE(facts.asked.empty());
	// a town with neither a store nor a temporary pot
	facts.towns.insert(town);
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::ShallCastFoodSpell, mind, node));
	EXPECT_EQ(facts.asked.size(), 1u);
	EXPECT_TRUE(hand.steps.empty());
}

TEST(ComputerHandActions, AMiracleWithNoCastAtAnObjectStopsAtTheChant)
{
	// a flock has no cast after the chant; a miracle cast at a place is not cast by openblack's hand yet
	const auto icon = static_cast<entt::entity>(21);
	const auto store = static_cast<entt::entity>(22);
	ComputerHand hand;
	EXPECT_TRUE(CastIconsMiracleAt(hand, icon, MagicType::FlockFlying, 10.0f, store));
	ASSERT_EQ(hand.steps.size(), 2u);
	EXPECT_EQ(hand.steps.at(1).type, HandSubAction::MakeWorshippersChantForSpell);
	ComputerHand atPlace;
	EXPECT_FALSE(CastIconsMiracleAt(atPlace, icon, MagicType::Fireball, 10.0f, store));
	EXPECT_FALSE(CastIconsMiracleAt(atPlace, icon, MagicType::Shield, 10.0f, store));
	EXPECT_TRUE(atPlace.steps.empty());
	// 200 m and more is far: the gesture
	ComputerHand distant;
	EXPECT_TRUE(CastIconsMiracleAt(distant, icon, MagicType::Water, k_GestureFromIconMetres, store));
	ASSERT_EQ(distant.steps.size(), 3u);
	EXPECT_EQ(distant.steps.at(0).type, HandSubAction::MakeGesture);
	EXPECT_EQ(distant.steps.at(2).type, HandSubAction::CastSpellAtObject);
}

TEST(ComputerHandActions, TheKindsOfCastFollowTheGamesTableAtEachBoundary)
{
	// the original's table by the miracle's number: 1 to 6 and 16 to 20 are cast at a place (not by openblack's hand
	// yet: no step), 7 to 15, 21 to 23 and 26 to 41 at the object, and the others (none, the flocks, past the last)
	// not at all, after the chant
	enum class Kind
	{
		AtPlace,
		AtObject,
		None,
	};
	const std::vector<std::pair<uint32_t, Kind>> table {
	    {0, Kind::None},     {1, Kind::AtPlace},   {6, Kind::AtPlace},   {7, Kind::AtObject},  {15, Kind::AtObject},
	    {16, Kind::AtPlace}, {20, Kind::AtPlace},  {21, Kind::AtObject}, {23, Kind::AtObject}, {24, Kind::None},
	    {25, Kind::None},    {26, Kind::AtObject}, {41, Kind::AtObject}, {42, Kind::None},
	};
	for (const auto& [number, kind] : table)
	{
		ComputerHand hand;
		const bool done = CastIconsMiracleAt(hand, static_cast<entt::entity>(21), static_cast<MagicType>(number), 10.0f,
		                                     static_cast<entt::entity>(22));
		EXPECT_EQ(done, kind != Kind::AtPlace) << number;
		const size_t steps = kind == Kind::AtPlace ? 0u : (kind == Kind::AtObject ? 3u : 2u);
		EXPECT_EQ(hand.steps.size(), steps) << number;
	}
}

TEST(ComputerHandActions, AnIconsMiracleIsCastOnTheObjectAboveAndTheHandStaysEightSeconds)
{
	// the creature's food, the workshop's wood and the burning building's water: one action casts the icon's miracle
	// on the object above the leaf's parent, then waits eight seconds and closes the miracle down
	const auto icon = static_cast<entt::entity>(21);
	const auto object = static_cast<entt::entity>(30);
	const std::vector<std::pair<std::string_view, MagicType>> leaves {
	    {"CastFoodSpellOn", MagicType::Food}, {"CastWoodSpellOn", MagicType::Wood}, {"CastWaterSpell", MagicType::Water}};
	for (const auto& [name, miracle] : leaves)
	{
		ComputerHand hand;
		hand.mapX = 0;
		hand.mapZ = 0;
		FakeRunner runner;
		FakeFacts facts;
		facts.objects.insert(object);
		facts.icons[icon] = miracle;
		facts.mapPlaces[icon] = {.x = 0, .z = 15 << 16};
		auto mind = computer_player::MakeMind();
		const auto [node, parent] = NodeAndParent(name);
		EXPECT_EQ(computer_player::k_MindRows.at(computer_player::MindNodes().at(node).row).action,
		          computer_player::ActionKind::CastFoodSpellOn)
		    << name;
		mind.nodes.at(node).thing = icon;
		mind.nodes.at(parent).thing = object;
		HandActions actions(hand, runner, facts);
		EXPECT_TRUE(actions.HasAction(computer_player::ActionKind::CastFoodSpellOn));
		EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::CastFoodSpellOn, mind, node)) << name;
		ASSERT_EQ(hand.steps.size(), 5u) << name;
		EXPECT_EQ(hand.steps.at(0).type, HandSubAction::MoveHandToObject);
		EXPECT_EQ(hand.steps.at(0).arguments.at(0).thing, icon);
		EXPECT_EQ(hand.steps.at(1).type, HandSubAction::MakeWorshippersChantForSpell);
		EXPECT_EQ(hand.steps.at(1).arguments.at(0).intValue, static_cast<int32_t>(miracle));
		EXPECT_EQ(hand.steps.at(2).type, HandSubAction::CastSpellAtObject);
		EXPECT_EQ(hand.steps.at(2).arguments.at(0).thing, object);
		EXPECT_EQ(hand.steps.at(2).arguments.at(2).thing, icon);
		EXPECT_EQ(hand.steps.at(3).type, HandSubAction::Wait);
		ASSERT_EQ(hand.steps.at(3).arguments.size(), 1u);
		EXPECT_EQ(hand.steps.at(3).arguments.at(0).kind, HandArgument::Kind::Float);
		EXPECT_EQ(hand.steps.at(3).arguments.at(0).floatValue, 8.0f);
		EXPECT_EQ(hand.steps.at(4).type, HandSubAction::CloseDownCurrentSpell);
		EXPECT_TRUE(hand.steps.at(4).arguments.empty());
	}
}

TEST(ComputerHandActions, TheMiracleOnAnObjectNeedsAWorshipIconAndAnObject)
{
	FakeRunner runner;
	FakeFacts facts;
	const auto icon = static_cast<entt::entity>(21);
	const auto object = static_cast<entt::entity>(30);
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParent("CastWaterSpell");
	mind.nodes.at(node).thing = icon;
	mind.nodes.at(parent).thing = object;
	ComputerHand hand;
	HandActions actions(hand, runner, facts);
	// no icon
	facts.objects.insert(object);
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::CastFoodSpellOn, mind, node));
	// no object
	facts.objects.clear();
	facts.icons[icon] = MagicType::Water;
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::CastFoodSpellOn, mind, node));
	EXPECT_TRUE(hand.steps.empty());
	// far from its icon the hand makes the gesture; a miracle cast at a place gives no step
	facts.objects.insert(object);
	facts.mapPlaces[icon] = {.x = 25 << 16, .z = 0};
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::CastFoodSpellOn, mind, node));
	ASSERT_EQ(hand.steps.size(), 5u);
	EXPECT_EQ(hand.steps.at(0).type, HandSubAction::MakeGesture);
	ComputerHand atPlace;
	EXPECT_FALSE(CastIconsMiracleOnAndStay(atPlace, icon, MagicType::Shield, 10.0f, object));
	EXPECT_TRUE(atPlace.steps.empty());
}

TEST(ComputerHandActions, TheBuildingSitesWoodMiracleIsCastAtItsBuildingAndAgainThereAsAShield)
{
	// the building site above the leaf's parent has a building: near its icon the hand goes to it, the worshippers
	// chant, and the wood miracle is cast at the building's place, then cast again there as a shield with no radius
	ComputerHand hand;
	hand.mapX = 0;
	hand.mapZ = 0;
	FakeRunner runner;
	FakeFacts facts;
	const auto site = static_cast<entt::entity>(20);
	const auto icon = static_cast<entt::entity>(21);
	const auto building = static_cast<entt::entity>(22);
	facts.siteBuildings[site] = building;
	facts.icons[icon] = MagicType::Wood;
	facts.mapPlaces[icon] = {.x = 0, .z = 15 << 16};
	facts.mapPlaces[building] = {.x = 10 << 16, .z = 4 << 16};
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParentOfRow("ShallCastWoodSpell: 136 ");
	EXPECT_EQ(computer_player::k_MindRows.at(computer_player::MindNodes().at(node).row).action,
	          computer_player::ActionKind::ShallCastWoodSpell);
	mind.nodes.at(node).thing = icon;
	mind.nodes.at(parent).thing = site;
	HandActions actions(hand, runner, facts);
	EXPECT_TRUE(actions.HasAction(computer_player::ActionKind::ShallCastWoodSpell));
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::ShallCastWoodSpell, mind, node));
	const glm::vec3 place(100.0f, 5.0f, 40.0f);
	ASSERT_EQ(hand.steps.size(), 5u);
	EXPECT_EQ(hand.steps.at(0).type, HandSubAction::MoveHandToObject);
	EXPECT_EQ(hand.steps.at(1).type, HandSubAction::MakeWorshippersChantForSpell);
	EXPECT_EQ(hand.steps.at(2).type, HandSubAction::CastSpellAtPos);
	ASSERT_EQ(hand.steps.at(2).arguments.size(), 3u);
	EXPECT_EQ(hand.steps.at(2).arguments.at(0).kind, HandArgument::Kind::Point);
	EXPECT_EQ(hand.steps.at(2).arguments.at(0).point, place);
	EXPECT_EQ(hand.steps.at(2).arguments.at(1).intValue, static_cast<int32_t>(MagicType::Wood));
	EXPECT_EQ(hand.steps.at(2).arguments.at(2).thing, icon);
	EXPECT_EQ(hand.steps.at(3).type, HandSubAction::MoveHandToPos);
	ASSERT_EQ(hand.steps.at(3).arguments.size(), 1u);
	EXPECT_EQ(hand.steps.at(3).arguments.at(0).point, place);
	EXPECT_EQ(hand.steps.at(4).type, HandSubAction::CastShieldAtPos);
	ASSERT_EQ(hand.steps.at(4).arguments.size(), 4u);
	EXPECT_EQ(hand.steps.at(4).arguments.at(0).point, place);
	EXPECT_EQ(hand.steps.at(4).arguments.at(1).intValue, static_cast<int32_t>(MagicType::Wood));
	EXPECT_EQ(hand.steps.at(4).arguments.at(2).kind, HandArgument::Kind::Float);
	EXPECT_EQ(hand.steps.at(4).arguments.at(2).floatValue, 0.0f);
	EXPECT_EQ(hand.steps.at(4).arguments.at(3).thing, icon);
}

TEST(ComputerHandActions, TheBuildingSitesWoodMiracleNeedsAWorshipIconAndASiteWithABuilding)
{
	FakeRunner runner;
	FakeFacts facts;
	const auto site = static_cast<entt::entity>(20);
	const auto icon = static_cast<entt::entity>(21);
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParentOfRow("ShallCastWoodSpell: 136 ");
	mind.nodes.at(node).thing = icon;
	mind.nodes.at(parent).thing = site;
	ComputerHand hand;
	HandActions actions(hand, runner, facts);
	facts.siteBuildings[site] = static_cast<entt::entity>(22);
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::ShallCastWoodSpell, mind, node));
	facts.siteBuildings.clear();
	facts.icons[icon] = MagicType::Wood;
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::ShallCastWoodSpell, mind, node));
	EXPECT_TRUE(hand.steps.empty());
}

TEST(ComputerHandActions, TheWorshippersFoodMiracleIsCastWhereTheSiteKeepsItsFood)
{
	ComputerHand hand;
	hand.mapX = 0;
	hand.mapZ = 0;
	FakeRunner runner;
	FakeFacts facts;
	const auto site = static_cast<entt::entity>(20);
	const auto icon = static_cast<entt::entity>(21);
	facts.icons[icon] = MagicType::Food;
	facts.mapPlaces[icon] = {.x = 25 << 16, .z = 0};
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParentOfRow("CastFoodSpellOnWorshipSite: 135 ");
	EXPECT_EQ(computer_player::k_MindRows.at(computer_player::MindNodes().at(node).row).action,
	          computer_player::ActionKind::CastFoodSpellOnWorshipSite);
	mind.nodes.at(node).thing = icon;
	mind.nodes.at(parent).thing = site;
	HandActions actions(hand, runner, facts);
	EXPECT_TRUE(actions.HasAction(computer_player::ActionKind::CastFoodSpellOnWorshipSite));
	// no worship site; then far from the icon: the gesture, the chant, the food at the site's food place, then again
	// there as a shield with no radius
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::CastFoodSpellOnWorshipSite, mind, node));
	EXPECT_TRUE(hand.steps.empty());
	facts.foodPlaces[site] = {.x = 3 << 16, .z = 2 << 16, .altitude = 1.0f};
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::CastFoodSpellOnWorshipSite, mind, node));
	ASSERT_EQ(hand.steps.size(), 5u);
	EXPECT_EQ(hand.steps.at(0).type, HandSubAction::MakeGesture);
	EXPECT_EQ(hand.steps.at(1).type, HandSubAction::MakeWorshippersChantForSpell);
	EXPECT_EQ(hand.steps.at(2).type, HandSubAction::CastSpellAtPos);
	EXPECT_EQ(hand.steps.at(2).arguments.at(0).point, glm::vec3(30.0f, 6.0f, 20.0f));
	EXPECT_EQ(hand.steps.at(2).arguments.at(1).intValue, static_cast<int32_t>(MagicType::Food));
	EXPECT_EQ(hand.steps.at(3).type, HandSubAction::MoveHandToPos);
	EXPECT_EQ(hand.steps.at(4).type, HandSubAction::CastShieldAtPos);
	EXPECT_EQ(hand.steps.at(4).arguments.at(0).point, glm::vec3(30.0f, 6.0f, 20.0f));
}

TEST(ComputerHandActions, ItsHurtCreatureIsHealedFromTheIconWhateverItsMiracle)
{
	// both leaves that heal the creature: the hand always goes to the icon, the worshippers chant for the heal
	// miracle, and it is cast on the creature, even from an icon of another miracle
	const auto creature = static_cast<entt::entity>(20);
	const auto icon = static_cast<entt::entity>(21);
	for (const auto row :
	     {std::string_view("RestoreMyCreatureHealth: 162 "), std::string_view("RestoreMyCreatureHealth: 172 ")})
	{
		ComputerHand hand;
		FakeRunner runner;
		FakeFacts facts;
		facts.creatures.insert(creature);
		facts.icons[icon] = MagicType::Food;
		facts.mapPlaces[icon] = {.x = 90 << 16, .z = 0};
		auto mind = computer_player::MakeMind();
		const auto [node, parent] = NodeAndParentOfRow(row);
		mind.nodes.at(node).thing = icon;
		mind.nodes.at(parent).thing = creature;
		HandActions actions(hand, runner, facts);
		EXPECT_TRUE(actions.HasAction(computer_player::ActionKind::RestoreMyCreatureHealth));
		EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::RestoreMyCreatureHealth, mind, node)) << row;
		ASSERT_EQ(hand.steps.size(), 3u) << row;
		EXPECT_EQ(hand.steps.at(0).type, HandSubAction::MoveHandToObject);
		EXPECT_EQ(hand.steps.at(0).arguments.at(0).thing, icon);
		EXPECT_EQ(hand.steps.at(1).type, HandSubAction::MakeWorshippersChantForSpell);
		EXPECT_EQ(hand.steps.at(1).arguments.at(0).intValue, static_cast<int32_t>(MagicType::Heal));
		EXPECT_EQ(hand.steps.at(1).arguments.at(1).thing, icon);
		EXPECT_EQ(hand.steps.at(2).type, HandSubAction::CastSpellAtObject);
		EXPECT_EQ(hand.steps.at(2).arguments.at(0).thing, creature);
		EXPECT_EQ(hand.steps.at(2).arguments.at(1).intValue, static_cast<int32_t>(MagicType::Heal));
		EXPECT_EQ(hand.steps.at(2).arguments.at(2).thing, icon);
		// no creature, or no worship icon: nothing
		ComputerHand other;
		HandActions without(other, runner, facts);
		facts.creatures.clear();
		EXPECT_FALSE(without.RunAction(computer_player::ActionKind::RestoreMyCreatureHealth, mind, node));
		facts.creatures.insert(creature);
		facts.icons.clear();
		EXPECT_FALSE(without.RunAction(computer_player::ActionKind::RestoreMyCreatureHealth, mind, node));
		EXPECT_TRUE(other.steps.empty());
	}
}

TEST(ComputerHandActions, AnIconsMiracleIsChargedWithoutWaiting)
{
	const auto icon = static_cast<entt::entity>(21);
	auto mind = computer_player::MakeMind();
	const auto node = computer_player::FindNode("ChargeUpSpell");
	ASSERT_TRUE(node.has_value());
	mind.nodes.at(*node).thing = icon;
	FakeRunner runner;
	FakeFacts facts;
	ComputerHand hand;
	hand.mapX = 0;
	hand.mapZ = 0;
	HandActions actions(hand, runner, facts);
	EXPECT_TRUE(actions.HasAction(computer_player::ActionKind::ChargeUpSpell));
	// no worship icon
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::ChargeUpSpell, mind, *node));
	EXPECT_TRUE(hand.steps.empty());
	// near the icon: the hand goes to it; far: the gesture; the chant does not wait for the charge
	facts.icons[icon] = MagicType::Heal;
	facts.mapPlaces[icon] = {.x = 0, .z = 19 << 16};
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::ChargeUpSpell, mind, *node));
	ASSERT_EQ(hand.steps.size(), 2u);
	EXPECT_EQ(hand.steps.at(0).type, HandSubAction::MoveHandToObject);
	EXPECT_EQ(hand.steps.at(1).type, HandSubAction::MakeWorshippersChantForSpellNoWait);
	EXPECT_EQ(hand.steps.at(1).arguments.at(0).intValue, static_cast<int32_t>(MagicType::Heal));
	EXPECT_EQ(hand.steps.at(1).arguments.at(1).thing, icon);
	ComputerHand far;
	ChargeIconsMiracle(far, icon, MagicType::Heal, k_GestureFromIconMetres);
	ASSERT_EQ(far.steps.size(), 2u);
	EXPECT_EQ(far.steps.at(0).type, HandSubAction::MakeGesture);
	EXPECT_EQ(far.steps.at(0).arguments.at(0).intValue, static_cast<int32_t>(MagicType::Heal));
	EXPECT_EQ(far.steps.at(0).arguments.at(1).intValue, 0);
}

TEST(ComputerHandChant, WithoutWaitingItsEndIsDoneAtOnce)
{
	const auto icon = static_cast<entt::entity>(21);
	const HandStep step {
	    .type = HandSubAction::MakeWorshippersChantForSpellNoWait,
	    .arguments = {{.kind = HandArgument::Kind::Int, .intValue = 10}, {.kind = HandArgument::Kind::Thing, .thing = icon}}};
	FakeRunner runner;
	runner.withSite.insert(icon);
	runner.worshipIcons.insert(icon);
	runner.charging.insert(icon);
	runner.chantsNeeded[icon] = 5.0f;
	ComputerHand hand;
	EXPECT_EQ(ChantNoWaitStage(hand, step, 0, runner), StageResult::Done);
	EXPECT_EQ(ChantNoWaitStage(hand, step, 1, runner), StageResult::Done);
	// the chant's own end would wait for the chants still needed
	EXPECT_EQ(ChantStage(hand, step, 2, runner), StageResult::Going);
	EXPECT_EQ(ChantNoWaitStage(hand, step, 2, runner), StageResult::Done);
	// its start and turn are the chant's
	FakeRunner noSite;
	EXPECT_EQ(ChantNoWaitStage(hand, step, 0, noSite), StageResult::Failed);
}

TEST(ComputerHandActions, AVillagerMadeADiscipleIsPickedUpAndDroppedWhereItStood)
{
	const auto villager = static_cast<entt::entity>(30);
	const auto town = static_cast<entt::entity>(20);
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParent("CreateDiscipleToFulfillTownDesire");
	mind.nodes.at(node).thing = villager;
	mind.nodes.at(node).intValue = 8;
	mind.nodes.at(parent).thing = town;
	FakeRunner runner;
	FakeFacts facts;
	facts.places[villager] = glm::vec3(40.0f, 6.0f, 50.0f);
	ComputerHand hand;
	hand.playerSpeed = 120.0f;
	HandActions actions(hand, runner, facts);
	EXPECT_TRUE(actions.HasAction(computer_player::ActionKind::CreateDiscipleToFulfillTownDesire));
	// no villager, then no town: nothing
	facts.towns.insert(town);
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::CreateDiscipleToFulfillTownDesire, mind, node));
	facts.towns.clear();
	facts.villagers.insert(villager);
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::CreateDiscipleToFulfillTownDesire, mind, node));
	EXPECT_TRUE(runner.disciples.empty());
	facts.towns.insert(town);
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::CreateDiscipleToFulfillTownDesire, mind, node));
	EXPECT_EQ(runner.disciples, (std::vector<std::tuple<entt::entity, int32_t>> {{villager, 8}}));
	ASSERT_EQ(hand.steps.size(), 2u);
	EXPECT_EQ(hand.steps.at(0).type, HandSubAction::PickUpObject);
	EXPECT_EQ(hand.steps.at(0).arguments.at(0).thing, villager);
	EXPECT_EQ(hand.steps.at(0).arguments.at(1).floatValue, 120.0f);
	EXPECT_EQ(hand.steps.at(1).type, HandSubAction::DropObjectInHandAtPos);
	EXPECT_EQ(hand.steps.at(1).arguments.at(0).point, glm::vec3(40.0f, 6.0f, 50.0f));
	EXPECT_EQ(hand.steps.at(1).arguments.at(1).floatValue, 120.0f);
	// a villager the hand may not take is a disciple all the same, and the action fails
	ComputerHand other;
	HandActions refused(other, runner, facts);
	runner.refused.insert(villager);
	EXPECT_FALSE(refused.RunAction(computer_player::ActionKind::CreateDiscipleToFulfillTownDesire, mind, node));
	EXPECT_EQ(runner.disciples.size(), 2u);
	EXPECT_TRUE(other.steps.empty());
}

TEST(ComputerHandActions, TheMostHurtWorshipperNotHidingIsTheOneHealed)
{
	using W = ThingFacts::Worshipper;
	const auto a = static_cast<entt::entity>(31);
	const auto b = static_cast<entt::entity>(32);
	const auto c = static_cast<entt::entity>(33);
	// the least life below full, a tie keeping the first; one hiding at the site is passed over
	EXPECT_EQ(MostHurtWorshipper(std::vector<W> {{a, false, 0.6f}, {b, false, 0.4f}, {c, false, 0.4f}}), b);
	EXPECT_EQ(MostHurtWorshipper(std::vector<W> {{a, false, 0.6f}, {b, true, 0.1f}}), a);
	EXPECT_EQ(MostHurtWorshipper(std::vector<W> {{a, false, 1.0f}, {b, true, 0.1f}}), entt::entity {entt::null});
	EXPECT_EQ(MostHurtWorshipper(std::vector<W> {}), entt::entity {entt::null});
}

TEST(ComputerHandActions, TheHealAtAWorshipSiteIsCastWhereItsMostHurtWorshipperIs)
{
	ComputerHand hand;
	hand.mapX = 0;
	hand.mapZ = 0;
	FakeRunner runner;
	FakeFacts facts;
	const auto site = static_cast<entt::entity>(20);
	const auto icon = static_cast<entt::entity>(21);
	const auto tired = static_cast<entt::entity>(31);
	facts.icons[icon] = MagicType::Heal;
	facts.mapPlaces[icon] = {.x = 0, .z = 15 << 16};
	facts.mapPlaces[tired] = {.x = 6 << 16, .z = 8 << 16};
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParentOfRow("CastHealSpellAtWorshipSite: 188 ");
	mind.nodes.at(node).thing = icon;
	mind.nodes.at(parent).thing = site;
	HandActions actions(hand, runner, facts);
	EXPECT_TRUE(actions.HasAction(computer_player::ActionKind::CastHealSpellAtWorshipSite));
	// no site; then a site with no worshipper to heal: nothing
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::CastHealSpellAtWorshipSite, mind, node));
	facts.sites.insert(site);
	facts.worshippers[site] = {{.villager = tired, .hiding = false, .life = 1.0f}};
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::CastHealSpellAtWorshipSite, mind, node));
	EXPECT_TRUE(hand.steps.empty());
	facts.worshippers[site] = {{.villager = tired, .hiding = false, .life = 0.5f}};
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::CastHealSpellAtWorshipSite, mind, node));
	ASSERT_EQ(hand.steps.size(), 5u);
	EXPECT_EQ(hand.steps.at(0).type, HandSubAction::MoveHandToObject);
	EXPECT_EQ(hand.steps.at(2).type, HandSubAction::CastSpellAtPos);
	EXPECT_EQ(hand.steps.at(2).arguments.at(0).point, glm::vec3(60.0f, 5.0f, 80.0f));
	EXPECT_EQ(hand.steps.at(2).arguments.at(1).intValue, static_cast<int32_t>(MagicType::Heal));
	EXPECT_EQ(hand.steps.at(4).type, HandSubAction::CastShieldAtPos);
}

TEST(ComputerHandActions, ATownIsImpressedByTheIconsMiracleCastWhereItsVillagersAre)
{
	const auto town = static_cast<entt::entity>(20);
	const auto icon = static_cast<entt::entity>(21);
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParent("CastSpellToImpress");
	mind.nodes.at(node).thing = icon;
	mind.nodes.at(node).point = glm::vec3(70.0f, 9.0f, 30.0f);
	mind.nodes.at(parent).thing = town;
	FakeRunner runner;
	FakeFacts facts;
	ComputerHand hand;
	HandActions actions(hand, runner, facts);
	EXPECT_TRUE(actions.HasAction(computer_player::ActionKind::CastSpellToImpress));
	// no town, then no icon: nothing
	facts.icons[icon] = MagicType::Fireball;
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::CastSpellToImpress, mind, node));
	facts.towns.insert(town);
	facts.icons.clear();
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::CastSpellToImpress, mind, node));
	EXPECT_TRUE(hand.steps.empty());
	// even a fireball is cast at the point, far from the icon too
	facts.icons[icon] = MagicType::Fireball;
	facts.mapPlaces[icon] = {.x = 90 << 16, .z = 0};
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::CastSpellToImpress, mind, node));
	ASSERT_EQ(hand.steps.size(), 3u);
	EXPECT_EQ(hand.steps.at(0).type, HandSubAction::MoveHandToObject);
	EXPECT_EQ(hand.steps.at(1).type, HandSubAction::MakeWorshippersChantForSpell);
	EXPECT_EQ(hand.steps.at(2).type, HandSubAction::CastSpellAtPos);
	EXPECT_EQ(hand.steps.at(2).arguments.at(0).point, glm::vec3(70.0f, 9.0f, 30.0f));
	EXPECT_EQ(hand.steps.at(2).arguments.at(1).intValue, static_cast<int32_t>(MagicType::Fireball));
	EXPECT_EQ(hand.steps.at(2).arguments.at(2).thing, icon);
}

TEST(ComputerHandActions, AThingIsThrownToImpressATownWhereItsVillagersAreWithin700Metres)
{
	const auto rock = static_cast<entt::entity>(30);
	const auto town = static_cast<entt::entity>(20);
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParent("ThrowThingsToImpress");
	mind.nodes.at(node).thing = rock;
	mind.nodes.at(node).point = glm::vec3(690.0f, 4.0f, 0.0f);
	mind.nodes.at(parent).thing = town;
	FakeRunner runner;
	FakeFacts facts;
	facts.objects.insert(rock);
	ComputerHand hand;
	hand.mapX = 0;
	hand.mapZ = 0;
	HandActions actions(hand, runner, facts);
	EXPECT_TRUE(actions.HasAction(computer_player::ActionKind::ThrowThingsToImpress));
	// no town: nothing
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::ThrowThingsToImpress, mind, node));
	facts.towns.insert(town);
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::ThrowThingsToImpress, mind, node));
	ASSERT_EQ(hand.steps.size(), 2u);
	EXPECT_EQ(hand.steps.at(1).type, HandSubAction::ThrowObjectInHandAtPos);
	EXPECT_EQ(hand.steps.at(1).arguments.at(0).point, glm::vec3(690.0f, 4.0f, 0.0f));
	// 700 m away or more: too far
	ComputerHand other;
	HandActions far(other, runner, facts);
	mind.nodes.at(node).point = glm::vec3(700.0f, 4.0f, 0.0f);
	EXPECT_FALSE(far.RunAction(computer_player::ActionKind::ThrowThingsToImpress, mind, node));
	EXPECT_TRUE(other.steps.empty());
}

TEST(ComputerHandActions, ARockIsThrownAtTheCreatureAboveWherever)
{
	const auto rock = static_cast<entt::entity>(30);
	const auto creature = static_cast<entt::entity>(40);
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParent("ThrowrockatCreature");
	mind.nodes.at(node).thing = rock;
	mind.nodes.at(parent).thing = creature;
	FakeRunner runner;
	FakeFacts facts;
	facts.places[creature] = glm::vec3(30.0f, 6.0f, -40.0f);
	ComputerHand hand;
	HandActions actions(hand, runner, facts);
	EXPECT_TRUE(actions.HasAction(computer_player::ActionKind::ThrowrockatCreature));
	// no creature, then a thing with no place: nothing
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::ThrowrockatCreature, mind, node));
	facts.creatures.insert(creature);
	facts.placeless.insert(rock);
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::ThrowrockatCreature, mind, node));
	EXPECT_TRUE(hand.steps.empty());
	facts.placeless.clear();
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::ThrowrockatCreature, mind, node));
	ASSERT_EQ(hand.steps.size(), 2u);
	EXPECT_EQ(hand.steps.at(0).arguments.at(0).thing, rock);
	EXPECT_EQ(hand.steps.at(1).type, HandSubAction::ThrowObjectInHandAtPos);
	EXPECT_EQ(hand.steps.at(1).arguments.at(0).point, glm::vec3(30.0f, 6.0f, -40.0f));
}

TEST(ComputerHandActions, AScaffoldIsGivenToTheOneTheLeafChoseToCombineThem)
{
	const auto small = static_cast<entt::entity>(30);
	const auto other = static_cast<entt::entity>(31);
	auto mind = computer_player::MakeMind();
	const auto node = computer_player::FindNode("CombineScaffolds");
	ASSERT_TRUE(node.has_value());
	const auto leaf = computer_player::GameTree().nodes[*node].children.at(0);
	mind.nodes.at(*node).thing = small;
	mind.nodes.at(*node).chosen = leaf;
	mind.nodes.at(leaf).chosen = leaf;
	mind.nodes.at(leaf).thing = other;
	EXPECT_EQ(ThingOfChosenLeaf(mind, *node), other);
	FakeRunner runner;
	FakeFacts facts;
	ComputerHand hand;
	hand.playerSpeed = 120.0f;
	HandActions actions(hand, runner, facts);
	EXPECT_TRUE(actions.HasAction(computer_player::ActionKind::CombineScaffolds));
	// not scaffolds, or one the hand may not take: nothing
	facts.scaffolds.insert(small);
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::CombineScaffolds, mind, *node));
	facts.scaffolds.insert(other);
	runner.refused.insert(small);
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::CombineScaffolds, mind, *node));
	EXPECT_TRUE(hand.steps.empty());
	runner.refused.clear();
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::CombineScaffolds, mind, *node));
	ASSERT_EQ(hand.steps.size(), 2u);
	EXPECT_EQ(hand.steps.at(0).type, HandSubAction::PickUpObject);
	EXPECT_EQ(hand.steps.at(0).arguments.at(0).thing, small);
	EXPECT_EQ(hand.steps.at(0).arguments.at(1).floatValue, 120.0f);
	EXPECT_EQ(hand.steps.at(1).type, HandSubAction::DropObjectInHandOnObject);
	EXPECT_EQ(hand.steps.at(1).arguments.at(0).thing, other);
}

TEST(ComputerHandActions, ItsCreatureIsFedTheFoodMiracleFromTheIconWhateverItsMiracle)
{
	const auto creature = static_cast<entt::entity>(20);
	const auto icon = static_cast<entt::entity>(21);
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParentOfRow("CastFoodForMyCreature: 160 ");
	mind.nodes.at(node).thing = icon;
	mind.nodes.at(parent).thing = creature;
	FakeRunner runner;
	FakeFacts facts;
	facts.icons[icon] = MagicType::Heal;
	ComputerHand hand;
	HandActions actions(hand, runner, facts);
	EXPECT_TRUE(actions.HasAction(computer_player::ActionKind::CastFoodForMyCreature));
	EXPECT_FALSE(actions.RunAction(computer_player::ActionKind::CastFoodForMyCreature, mind, node));
	facts.creatures.insert(creature);
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::CastFoodForMyCreature, mind, node));
	ASSERT_EQ(hand.steps.size(), 3u);
	EXPECT_EQ(hand.steps.at(0).type, HandSubAction::MoveHandToObject);
	EXPECT_EQ(hand.steps.at(1).arguments.at(0).intValue, static_cast<int32_t>(MagicType::Food));
	EXPECT_EQ(hand.steps.at(2).type, HandSubAction::CastSpellAtObject);
	EXPECT_EQ(hand.steps.at(2).arguments.at(0).thing, creature);
	EXPECT_EQ(hand.steps.at(2).arguments.at(1).intValue, static_cast<int32_t>(MagicType::Food));
}

TEST(ComputerHandActions, AWorkerIsDroppedRoundTheBuildingFirstWhereThereIsWood)
{
	const map_coords::MapCoords centre {.x = 100 << 16, .z = 100 << 16};
	const auto at = [&centre](float angle) { return centre + game_math::GetPosFromAngle(angle, 10.0f); };
	// no wood anywhere: the last of the nine places round it, at 1.25 times its radius; the angle summed in
	// single precision is still just below a whole turn after eight steps, so a ninth place is asked
	std::vector<map_coords::MapCoords> asked;
	const auto none = WorkerDropPlace(centre, 8.0f, [&asked](const map_coords::MapCoords& place) {
		asked.push_back(place);
		return false;
	});
	ASSERT_EQ(asked.size(), 9u);
	EXPECT_EQ(asked.front(), at(0.0f));
	EXPECT_EQ(none, asked.back());
	// wood at the third place: that one
	int count = 0;
	const auto third = WorkerDropPlace(centre, 8.0f, [&count](const map_coords::MapCoords&) { return ++count == 3; });
	EXPECT_EQ(third, asked.at(2));
	// off the map: no place is taken
	const map_coords::MapCoords edge {.x = -(50 << 16), .z = -(50 << 16)};
	EXPECT_EQ(WorkerDropPlace(edge, 8.0f, [](const map_coords::MapCoords&) { return false; }), map_coords::MapCoords {});
}

TEST(ComputerHandActions, TheLeavesWithNoActionRunTheirParentsWithTheThingTheyChose)
{
	const auto tree = static_cast<entt::entity>(30);
	const auto town = static_cast<entt::entity>(20);
	const auto pit = static_cast<entt::entity>(22);
	const auto site = static_cast<entt::entity>(24);
	const auto building = static_cast<entt::entity>(25);
	const auto workshop = static_cast<entt::entity>(26);
	FakeRunner runner;
	FakeFacts facts;
	facts.objects.insert(tree);
	facts.types[tree] = ResourceType::Wood;
	facts.towns.insert(town);
	facts.drops[town] = pit;
	facts.siteBuildings[site] = building;
	facts.mapPlaces[building] = {.x = 3 << 16, .z = 4 << 16};
	const auto chose = [](computer_player::ActionKind kind, entt::entity above, entt::entity leafThing) {
		auto mind = computer_player::MakeMind();
		const auto& nodes = computer_player::MindNodes();
		uint16_t node = 0;
		while (computer_player::k_MindRows.at(nodes.at(node).row).action != kind)
		{
			++node;
		}
		const auto leaf = nodes.at(node).children.at(0);
		mind.nodes.at(node).thing = above;
		mind.nodes.at(node).chosen = leaf;
		mind.nodes.at(leaf).chosen = leaf;
		mind.nodes.at(leaf).thing = leafThing;
		return std::pair {mind, node};
	};
	{
		auto [mind, node] = chose(computer_player::ActionKind::TownWithHighestFoodDesire, town, tree);
		ComputerHand hand;
		HandActions actions(hand, runner, facts);
		EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::TownWithHighestFoodDesire, mind, node));
		ASSERT_EQ(hand.steps.size(), 2u);
		EXPECT_EQ(hand.steps.at(0).arguments.at(0).thing, tree);
		EXPECT_EQ(hand.steps.at(1).type, HandSubAction::DropObjectInHandOnObject);
		EXPECT_EQ(hand.steps.at(1).arguments.at(0).thing, pit);
		EXPECT_EQ(facts.asked.back(), (std::tuple<entt::entity, ResourceType> {town, ResourceType::Wood}));
	}
	{
		auto [mind, node] = chose(computer_player::ActionKind::GiveBuildingSiteWood, site, tree);
		ComputerHand hand;
		HandActions actions(hand, runner, facts);
		EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::GiveBuildingSiteWood, mind, node));
		ASSERT_EQ(hand.steps.size(), 2u);
		EXPECT_EQ(hand.steps.at(1).type, HandSubAction::DropObjectInHandAtPos);
		EXPECT_EQ(hand.steps.at(1).arguments.at(0).point, glm::vec3(30.0f, 5.0f, 40.0f));
	}
	{
		auto [mind, node] = chose(computer_player::ActionKind::FillWorkshop, workshop, tree);
		ComputerHand hand;
		HandActions actions(hand, runner, facts);
		EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::FillWorkshop, mind, node));
		ASSERT_EQ(hand.steps.size(), 2u);
		EXPECT_EQ(hand.steps.at(1).arguments.at(0).thing, workshop);
		// a tree the hand may not take: done with nothing to do
		runner.refused.insert(tree);
		ComputerHand other;
		HandActions refused(other, runner, facts);
		EXPECT_TRUE(refused.RunAction(computer_player::ActionKind::FillWorkshop, mind, node));
		EXPECT_TRUE(other.steps.empty());
	}
}

TEST(ComputerHandActions, FoodFromAFieldGoesToTheCreatureOrTheWorshipSiteAbove)
{
	const auto field = static_cast<entt::entity>(30);
	const auto creature = static_cast<entt::entity>(40);
	const auto site = static_cast<entt::entity>(41);
	FakeRunner runner;
	FakeFacts facts;
	facts.creatures.insert(creature);
	facts.sites.insert(site);
	const auto chose = [](computer_player::ActionKind kind, entt::entity above, entt::entity leafThing) {
		auto mind = computer_player::MakeMind();
		const auto& nodes = computer_player::MindNodes();
		uint16_t node = 0;
		while (computer_player::k_MindRows.at(nodes.at(node).row).action != kind)
		{
			++node;
		}
		const auto leaf = nodes.at(node).children.at(0);
		mind.nodes.at(node).thing = above;
		mind.nodes.at(node).chosen = leaf;
		mind.nodes.at(leaf).chosen = leaf;
		mind.nodes.at(leaf).thing = leafThing;
		return std::pair {mind, node};
	};
	for (const auto& [kind, above] : {std::pair {computer_player::ActionKind::GiveCreatureFood, creature},
	                                  std::pair {computer_player::ActionKind::GiveWorshippersFood, site}})
	{
		auto [mind, node] = chose(kind, above, field);
		ComputerHand hand;
		HandActions actions(hand, runner, facts);
		EXPECT_TRUE(actions.HasAction(kind));
		EXPECT_TRUE(actions.RunAction(kind, mind, node));
		ASSERT_EQ(hand.steps.size(), 2u);
		EXPECT_EQ(hand.steps.at(0).arguments.at(0).thing, field);
		EXPECT_EQ(hand.steps.at(1).type, HandSubAction::DropObjectInHandOnObject);
		EXPECT_EQ(hand.steps.at(1).arguments.at(0).thing, above);
	}
	// the starving creature is given it from the hand
	auto [mind, node] = chose(computer_player::ActionKind::ReacttoCreatureStarvingEmergency, creature, field);
	ComputerHand hand;
	HandActions actions(hand, runner, facts);
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::ReacttoCreatureStarvingEmergency, mind, node));
	ASSERT_EQ(hand.steps.size(), 2u);
	EXPECT_EQ(hand.steps.at(1).type, HandSubAction::GiveObjectInHandToCreature);
	EXPECT_EQ(hand.steps.at(1).arguments.at(0).thing, creature);
	// no creature above: nothing
	auto [other, otherNode] = chose(computer_player::ActionKind::ReacttoCreatureStarvingEmergency, site, field);
	ComputerHand none;
	HandActions refused(none, runner, facts);
	EXPECT_FALSE(refused.RunAction(computer_player::ActionKind::ReacttoCreatureStarvingEmergency, other, otherNode));
	EXPECT_TRUE(none.steps.empty());
}

TEST(ComputerHandActions, AVillagerTakenHomeIsDroppedOutsideItsDoor)
{
	// from the building's place towards its door, 1.25 times its radius and the villager's own beyond it
	const map_coords::MapCoords abode {.x = 100 << 16, .z = 100 << 16};
	const map_coords::MapCoords door {.x = 103 << 16, .z = 100 << 16};
	const auto expected = abode + game_math::GetPosFromAngle(game_math::Get3DAngleFromXZ(abode, door), 8.0f * 1.25f + 1.5f);
	EXPECT_EQ(HomeDropPlace(abode, door, 8.0f, 1.5f), expected);
	EXPECT_GT(expected.x, abode.x);
}

TEST(ComputerHandActions, AMiracleAtAPlaceFollowsTheGamesTableOfKinds)
{
	// at a place 100 m east of the hand with a radius of 10: a miracle cast at an object goes on the circle's point
	// that faces the hand and then again as a shield at the place; a storm, a shield or a flock only as the shield;
	// a creature's miracle no further than the chant; a fireball or lightning gives no step
	FakeFacts facts;
	const auto icon = static_cast<entt::entity>(21);
	const map_coords::MapCoords place {.x = 10 << 16, .z = 0};
	const map_coords::MapCoords side =
	    place + game_math::GetPosFromAngle(game_math::Get3DAngleFromXZ(place, map_coords::MapCoords {}), 10.0f);
	EXPECT_LT(side.x, place.x);
	enum class Kind
	{
		Twice,
		Shield,
		Chant,
		None,
	};
	const std::vector<std::pair<uint32_t, Kind>> table {
	    {0, Kind::Chant},   {1, Kind::None},    {6, Kind::None},   {7, Kind::Twice},  {15, Kind::Twice},
	    {16, Kind::Shield}, {20, Kind::Shield}, {21, Kind::Twice}, {23, Kind::Twice}, {24, Kind::Shield},
	    {25, Kind::Shield}, {26, Kind::Chant},  {41, Kind::Chant}, {42, Kind::Chant},
	};
	for (const auto& [number, kind] : table)
	{
		ComputerHand hand;
		hand.mapX = 0;
		hand.mapZ = 0;
		const bool done = CastIconsMiracleAtPlace(hand, icon, static_cast<MagicType>(number), 10.0f, place, 10.0f, facts);
		EXPECT_EQ(done, kind != Kind::None) << number;
		const size_t steps = kind == Kind::None ? 0u : kind == Kind::Chant ? 2u : kind == Kind::Shield ? 4u : 5u;
		ASSERT_EQ(hand.steps.size(), steps) << number;
		if (kind == Kind::Twice)
		{
			EXPECT_EQ(hand.steps.at(2).type, HandSubAction::CastSpellAtPos) << number;
			EXPECT_EQ(hand.steps.at(2).arguments.at(0).point, facts.LandPoint(side)) << number;
		}
		if (kind == Kind::Twice || kind == Kind::Shield)
		{
			EXPECT_EQ(hand.steps.at(steps - 1).type, HandSubAction::CastShieldAtPos) << number;
			EXPECT_EQ(hand.steps.at(steps - 1).arguments.at(0).point, facts.LandPoint(place)) << number;
			EXPECT_EQ(hand.steps.at(steps - 1).arguments.at(2).floatValue, 10.0f) << number;
		}
	}
}

TEST(ComputerHandCast, AtAPlaceItsTurnCastsOnTheLandFromTheHandAndItsEndWaits)
{
	const auto icon = static_cast<entt::entity>(21);
	const auto spell = static_cast<entt::entity>(30);
	FakeRunner runner;
	runner.castGives = spell;
	runner.secondsAfterCast = 0.2f;
	ComputerHand hand;
	hand.point = {20.0f, 50.0f, 30.0f};
	const HandStep step {.type = HandSubAction::CastSpellAtPos,
	                     .arguments = {{.kind = HandArgument::Kind::Point, .point = {100.0f, 7.0f, 40.0f}},
	                                   {.kind = HandArgument::Kind::Int, .intValue = 21},
	                                   {.kind = HandArgument::Kind::Thing, .thing = icon}}};
	EXPECT_EQ(CastAtPosStage(hand, step, 0, runner), StageResult::Done);
	EXPECT_TRUE(runner.castsAt.empty());
	EXPECT_EQ(CastAtPosStage(hand, step, 1, runner), StageResult::Done);
	ASSERT_EQ(runner.castsAt.size(), 1u);
	const map_coords::MapCoords to {.x = map_coords::ToFixed(100.0f), .z = map_coords::ToFixed(40.0f), .altitude = 0.0f};
	EXPECT_EQ(runner.castsAt.front().to, to);
	EXPECT_EQ(runner.castsAt.front().from.x, map_coords::ToFixed(20.0f));
	EXPECT_EQ(runner.castsAt.front().from.z, map_coords::ToFixed(30.0f));
	EXPECT_EQ(runner.castsAt.front().miracle, 21);
	EXPECT_EQ(runner.castsAt.front().icon, icon);
	EXPECT_EQ(runner.castsAt.front().radius, 1.0f);
	EXPECT_EQ(runner.keptAt, (std::vector<std::tuple<entt::entity, map_coords::MapCoords>> {{spell, to}}));
	EXPECT_EQ(hand.waitTurns, 2);
	EXPECT_EQ(CastAtPosStage(hand, step, 2, runner), StageResult::Going);
	EXPECT_EQ(CastAtPosStage(hand, step, 2, runner), StageResult::Done);
	// no miracle cast: the step fails, and nothing is kept
	FakeRunner none;
	ComputerHand failed;
	EXPECT_EQ(CastAtPosStage(failed, step, 1, none), StageResult::Failed);
	EXPECT_TRUE(none.keptAt.empty());
}

TEST(ComputerHandCast, AShieldAtAPlaceIsCastOnceWithItsRadiusFromTheHandsPlace)
{
	const auto icon = static_cast<entt::entity>(21);
	const auto spell = static_cast<entt::entity>(30);
	FakeRunner runner;
	runner.castGives = spell;
	ComputerHand hand;
	hand.point = {20.0f, 50.0f, 30.0f};
	hand.mapX = 7 << 16;
	hand.mapZ = 9 << 16;
	const HandStep step {.type = HandSubAction::CastShieldAtPos,
	                     .arguments = {{.kind = HandArgument::Kind::Point, .point = {100.0f, 7.0f, 40.0f}},
	                                   {.kind = HandArgument::Kind::Int, .intValue = 19},
	                                   {.kind = HandArgument::Kind::Float, .floatValue = 25.0f},
	                                   {.kind = HandArgument::Kind::Thing, .thing = icon}}};
	EXPECT_EQ(CastShieldAtPosStage(hand, step, 0, runner), StageResult::Done);
	EXPECT_EQ(CastShieldAtPosStage(hand, step, 2, runner), StageResult::Done);
	EXPECT_TRUE(runner.castsAt.empty());
	EXPECT_EQ(CastShieldAtPosStage(hand, step, 1, runner), StageResult::Done);
	ASSERT_EQ(runner.castsAt.size(), 1u);
	EXPECT_EQ(runner.castsAt.front().from, (map_coords::MapCoords {.x = 7 << 16, .z = 9 << 16, .altitude = 0.0f}));
	EXPECT_EQ(runner.castsAt.front().miracle, 19);
	EXPECT_EQ(runner.castsAt.front().radius, 25.0f);
	EXPECT_EQ(runner.castsAt.front().icon, icon);
	EXPECT_EQ(runner.keptAt.size(), 1u);
	FakeRunner none;
	EXPECT_EQ(CastShieldAtPosStage(hand, step, 1, none), StageResult::Failed);
	EXPECT_TRUE(none.keptAt.empty());
}

/// Flat land at 3 m, as the original's runs of the moves were given
class GroundAt3 final: public Ground
{
public:
	[[nodiscard]] float AltitudeAt(int32_t /*mapX*/, int32_t /*mapZ*/) const override { return 3.0f; }
};

uint32_t BitsOfFloat(float value)
{
	return std::bit_cast<uint32_t>(value);
}

TEST(ComputerHandMoves, TheMoveNearAPointIsTheOriginalsBitForBit)
{
	// the original's own turns of this move, at 90 m/s unless given, on land 3 m high: the point after the turn, its
	// target's height, and whether it is done
	struct Case
	{
		glm::vec3 hand;
		glm::vec3 target;
		float speed;
		uint32_t x, y, z;
		bool done;
	};
	const std::vector<Case> cases {
	    // far: a step towards it, going on
	    {{0.0f, 50.0f, 0.0f}, {100.0f, 0.0f, 0.0f}, 90.0f, 0x41100000, 0x41500000, 0x00000000, false},
	    // too near: a drift along x of at most 1.4142, going on
	    {{0.0f, 50.0f, 0.0f}, {10.0f, 0.0f, 5.0f}, 90.0f, 0x3FB50481, 0x41500000, 0x00000000, false},
	    {{0.0f, 50.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 90.0f, 0x3FB50481, 0x41500000, 0x00000000, false},
	    // a slanting way
	    {{0.0f, 50.0f, 0.0f}, {300.0f, 0.0f, -200.0f}, 90.0f, 0x40EFA168, 0x41500000, 0xC09FC0F0, false},
	    {{7.25f, 50.0f, -3.5f}, {95.0f, 0.0f, 41.0f}, 37.0f, 0x4128CC7F, 0x41500000, 0xBFE9CBD9, false},
	    // within 80 m after the step: done
	    {{0.0f, 50.0f, 0.0f}, {85.0f, 0.0f, 0.0f}, 90.0f, 0x41100000, 0x41500000, 0x00000000, true},
	};
	// the nearness the original's start-up works out, 80 times 0.33
	EXPECT_EQ(BitsOfFloat(k_TooNearThePoint), 0x41D33334u);
	const GroundAt3 ground;
	for (const auto& c : cases)
	{
		ComputerHand hand;
		hand.point = c.hand;
		hand.speed = c.speed;
		StartTowards(hand, c.target);
		const auto result = MoveNearTurn(hand, ground);
		EXPECT_EQ(result, c.done ? StageResult::Done : StageResult::Going) << c.target.x;
		EXPECT_EQ(BitsOfFloat(hand.point.x), c.x) << c.target.x;
		EXPECT_EQ(BitsOfFloat(hand.point.y), c.y) << c.target.x;
		EXPECT_EQ(BitsOfFloat(hand.point.z), c.z) << c.target.x;
	}
}

TEST(ComputerHandMoves, TheMoveNearAThingIsTheOriginalsBitForBit)
{
	// the original's own turns of this move: as near a point, its drift at most 1.41421 and the target's height kept
	struct Case
	{
		glm::vec3 hand;
		glm::vec3 target;
		float speed;
		uint32_t x, y, z;
		bool done;
	};
	const std::vector<Case> cases {
	    {{0.0f, 50.0f, 0.0f}, {100.0f, 0.0f, 0.0f}, 90.0f, 0x41100000, 0x41500000, 0x00000000, false},
	    {{0.0f, 50.0f, 0.0f}, {10.0f, 0.0f, 5.0f}, 90.0f, 0x3FB504D5, 0x41500000, 0x00000000, false},
	    {{0.0f, 50.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 90.0f, 0x3FB504D5, 0x41500000, 0x00000000, false},
	    {{0.0f, 50.0f, 0.0f}, {300.0f, 0.0f, -200.0f}, 90.0f, 0x40EFA168, 0x41500000, 0xC09FC0F0, false},
	    {{7.25f, 50.0f, -3.5f}, {95.0f, 0.0f, 41.0f}, 37.0f, 0x4128CC7F, 0x41500000, 0xBFE9CBD9, false},
	    {{0.0f, 50.0f, 0.0f}, {85.0f, 0.0f, 0.0f}, 90.0f, 0x41100000, 0x41500000, 0x00000000, true},
	};
	const GroundAt3 ground;
	for (const auto& c : cases)
	{
		ComputerHand hand;
		hand.point = c.hand;
		hand.speed = c.speed;
		StartTowards(hand, c.target + glm::vec3(0.0f, 7.0f, 0.0f));
		const auto result = MoveNearThingTurn(hand, ground);
		EXPECT_EQ(result, c.done ? StageResult::Done : StageResult::Going) << c.target.x;
		EXPECT_EQ(BitsOfFloat(hand.point.x), c.x) << c.target.x;
		EXPECT_EQ(BitsOfFloat(hand.point.y), c.y) << c.target.x;
		EXPECT_EQ(BitsOfFloat(hand.point.z), c.z) << c.target.x;
		EXPECT_EQ(hand.target.y, c.target.y + 7.0f) << c.target.x;
	}
}

TEST(ComputerHand, AnAngryMiracleIsCastFromNearTheThingOrThePoint)
{
	// the move in front of a cast of an angry miracle is the move near, of any other the move to
	const auto creature = static_cast<entt::entity>(40);
	const auto icon = static_cast<entt::entity>(21);
	FakeRunner runner;
	runner.angry.insert(static_cast<int32_t>(MagicType::Fireball));
	ComputerHand hand;
	AttackCreatureWithIcon(hand, icon, MagicType::Fireball, creature);
	ASSERT_EQ(hand.steps.size(), 3u);
	EXPECT_EQ(hand.steps.at(0).type, HandSubAction::MoveHandNearObject);
	EXPECT_EQ(hand.steps.at(0).arguments.at(0).thing, creature);
	EXPECT_EQ(hand.steps.at(1).type, HandSubAction::MakeWorshippersChantForSpell);
	EXPECT_EQ(hand.steps.at(2).type, HandSubAction::CastSpellAtObject);
	EXPECT_EQ(hand.steps.at(2).arguments.at(0).thing, creature);
	EXPECT_EQ(hand.steps.at(2).arguments.at(1).intValue, static_cast<int32_t>(MagicType::Fireball));
	const auto moveInFront = [&runner](HandSubAction cast, MagicType miracle) {
		ComputerHand one;
		const HandArgument first =
		    cast == HandSubAction::CastSpellAtObject
		        ? HandArgument {.kind = HandArgument::Kind::Thing, .thing = static_cast<entt::entity>(40)}
		        : HandArgument {.kind = HandArgument::Kind::Point, .point = {1.0f, 2.0f, 3.0f}};
		Append(one, {.type = cast,
		             .arguments = {first,
		                           {.kind = HandArgument::Kind::Int, .intValue = static_cast<int32_t>(miracle)},
		                           {.kind = HandArgument::Kind::Thing, .thing = static_cast<entt::entity>(21)}}});
		Step(one, runner);
		return one.steps.front().type;
	};
	EXPECT_EQ(moveInFront(HandSubAction::CastSpellAtObject, MagicType::Fireball), HandSubAction::MoveHandNearObject);
	EXPECT_EQ(moveInFront(HandSubAction::CastSpellAtPos, MagicType::Fireball), HandSubAction::MoveHandNearPos);
	EXPECT_EQ(moveInFront(HandSubAction::CastSpellAtObject, MagicType::Heal), HandSubAction::MoveHandToObject);
	EXPECT_EQ(moveInFront(HandSubAction::CastSpellAtPos, MagicType::Wood), HandSubAction::MoveHandToPos);
}

TEST(ComputerHandMoves, AThrowsSpeedIsTheOriginalsBitForBit)
{
	// the original's throw turn's speeds, from the hand's point to the point to hit
	struct Case
	{
		glm::vec3 from;
		glm::vec3 to;
		uint32_t x, y, z;
	};
	const std::vector<Case> cases {
	    {{0.0f, 50.0f, 0.0f}, {100.0f, 3.0f, 0.0f}, 0x41EE5F20, 0x401D457C, 0x00000000},
	    {{10.0f, 40.0f, 20.0f}, {-35.5f, 12.25f, 160.75f}, 0xC139DD1D, 0x414209AD, 0x420FBCEF},
	    {{1000.5f, 33.0f, 2000.25f}, {1003.0f, 30.0f, 1990.0f}, 0x4017502E, 0x40165DBD, 0xC11B1895},
	};
	for (const auto& c : cases)
	{
		const auto velocity = ThrowVelocity(c.from, c.to);
		EXPECT_EQ(BitsOfFloat(velocity.x), c.x) << c.to.x;
		EXPECT_EQ(BitsOfFloat(velocity.y), c.y) << c.to.x;
		EXPECT_EQ(BitsOfFloat(velocity.z), c.z) << c.to.x;
	}
	// the same two points give no number
	EXPECT_TRUE(std::isnan(ThrowVelocity({5.0f, 20.0f, 5.0f}, {5.0f, 20.0f, 5.0f}).x));
	// the step's turn throws from the hand's point, its start and end do nothing
	FakeRunner runner;
	ComputerHand hand;
	hand.point = {0.0f, 50.0f, 0.0f};
	const HandStep throwStep {.type = HandSubAction::ThrowObjectInHandAtPos,
	                          .arguments = {{.kind = HandArgument::Kind::Point, .point = {100.0f, 3.0f, 0.0f}}}};
	EXPECT_EQ(ThrowAtPosStage(hand, throwStep, 0, runner), StageResult::Done);
	EXPECT_TRUE(runner.thrown.empty());
	EXPECT_EQ(ThrowAtPosStage(hand, throwStep, 1, runner), StageResult::Done);
	ASSERT_EQ(runner.thrown.size(), 1u);
	EXPECT_EQ(BitsOfFloat(std::get<1>(runner.thrown.front()).x), 0x41EE5F20u);
	EXPECT_EQ(ThrowAtPosStage(hand, throwStep, 2, runner), StageResult::Done);
	EXPECT_EQ(runner.thrown.size(), 1u);
}

TEST(ComputerHandMoves, GivingToACreatureThrowsAtWhereTheHandActsOnIt)
{
	const auto creature = static_cast<entt::entity>(40);
	FakeRunner runner;
	runner.interactPoints[creature] = glm::vec3(100.0f, 3.0f, 0.0f);
	ComputerHand hand;
	hand.point = {0.0f, 50.0f, 0.0f};
	const HandStep give {.type = HandSubAction::GiveObjectInHandToCreature,
	                     .arguments = {{.kind = HandArgument::Kind::Thing, .thing = creature}}};
	EXPECT_EQ(GiveToCreatureStage(hand, give, 0, runner), StageResult::Done);
	EXPECT_TRUE(runner.thrown.empty());
	EXPECT_EQ(GiveToCreatureStage(hand, give, 1, runner), StageResult::Done);
	ASSERT_EQ(runner.thrown.size(), 1u);
	EXPECT_EQ(BitsOfFloat(std::get<1>(runner.thrown.front()).x), 0x41EE5F20u);
	EXPECT_EQ(BitsOfFloat(std::get<1>(runner.thrown.front()).y), 0x401D457Cu);
}

TEST(ComputerHandMoves, TheMoveThatStopsAtTheInfluencesEdgeEndsWhereThereIsNone)
{
	const FlatGround ground;
	FakeRunner runner;
	// a step on with influence there goes on, and the place asked is the hand's on the map grid
	ComputerHand hand;
	hand.point = {0.0f, 0.0f, 0.0f};
	hand.speed = 90.0f;
	StartTowards(hand, {100.0f, 0.0f, 0.0f});
	EXPECT_EQ(MoveUntilOutOfInfluence(hand, ground, runner), StageResult::Going);
	EXPECT_EQ(hand.point.x, 9.0f);
	ASSERT_EQ(runner.influenceAsked.size(), 1u);
	EXPECT_EQ(runner.influenceAsked.front(),
	          (map_coords::MapCoords {.x = map_coords::ToFixed(9.0f), .z = 0, .altitude = 0.0f}));
	// a rival's influence (below 0) does not stop it; none at all does
	runner.influence = -0.5f;
	EXPECT_EQ(MoveUntilOutOfInfluence(hand, ground, runner), StageResult::Going);
	runner.influence = 0.0f;
	EXPECT_EQ(MoveUntilOutOfInfluence(hand, ground, runner), StageResult::Done);
	// arriving is done with no question asked
	ComputerHand near;
	near.point = {0.0f, 0.0f, 0.0f};
	near.speed = 90.0f;
	StartTowards(near, {5.0f, 0.0f, 0.0f});
	FakeRunner none;
	EXPECT_EQ(MoveUntilOutOfInfluence(near, ground, none), StageResult::Done);
	EXPECT_TRUE(none.influenceAsked.empty());
}

TEST(ComputerHandActions, AnAngryMiracleIsCastTowardsTheThingFromInsideTheInfluence)
{
	const auto icon = static_cast<entt::entity>(21);
	const auto target = static_cast<entt::entity>(30);
	const TowardPlaces toward {
	    .place = {50.0f, 5.0f, 50.0f}, .edge = {48.0f, 5.0f, 47.0f}, .inside = glm::vec3(20.0f, 5.0f, 20.0f)};
	// a fireball: the reach, the chant, the move inside the influence, the move until it would leave it, the cast
	ComputerHand hand;
	EXPECT_TRUE(CastIconsMiracleAtOrToward(hand, icon, MagicType::Fireball, 10.0f, target, toward));
	ASSERT_EQ(hand.steps.size(), 5u);
	EXPECT_EQ(hand.steps.at(2).type, HandSubAction::MoveHandToPos);
	EXPECT_EQ(hand.steps.at(2).arguments.at(0).point, glm::vec3(20.0f, 5.0f, 20.0f));
	EXPECT_EQ(hand.steps.at(3).type, HandSubAction::MoveHandToPosStopWhenOutOfInfluence);
	EXPECT_EQ(hand.steps.at(3).arguments.at(0).point, glm::vec3(48.0f, 5.0f, 47.0f));
	EXPECT_EQ(hand.steps.at(4).type, HandSubAction::CastSpellTowardPos);
	EXPECT_EQ(hand.steps.at(4).arguments.at(0).point, glm::vec3(48.0f, 5.0f, 47.0f));
	EXPECT_EQ(hand.steps.at(4).arguments.at(1).intValue, static_cast<int32_t>(MagicType::Fireball));
	EXPECT_EQ(hand.steps.at(4).arguments.at(2).thing, icon);
	// no influence to cast from: nothing after the chant
	ComputerHand outside;
	EXPECT_TRUE(CastIconsMiracleAtOrToward(outside, icon, MagicType::LightningBolt, 10.0f, target,
	                                       TowardPlaces {.place = toward.place, .edge = toward.edge}));
	EXPECT_EQ(outside.steps.size(), 2u);
	// a storm: at the thing's place as a shield with no radius
	ComputerHand storm;
	EXPECT_TRUE(CastIconsMiracleAtOrToward(storm, icon, MagicType::Tornado, 10.0f, target, toward));
	ASSERT_EQ(storm.steps.size(), 4u);
	EXPECT_EQ(storm.steps.at(2).type, HandSubAction::MoveHandToPos);
	EXPECT_EQ(storm.steps.at(2).arguments.at(0).point, glm::vec3(50.0f, 5.0f, 50.0f));
	EXPECT_EQ(storm.steps.at(3).type, HandSubAction::CastShieldAtPos);
	EXPECT_EQ(storm.steps.at(3).arguments.at(2).floatValue, 0.0f);
	// an explosion: on the thing, as CastIconsMiracleAt
	ComputerHand blast;
	EXPECT_TRUE(CastIconsMiracleAtOrToward(blast, icon, MagicType::ExplosionOne, 10.0f, target, toward));
	ASSERT_EQ(blast.steps.size(), 3u);
	EXPECT_EQ(blast.steps.at(2).type, HandSubAction::CastSpellAtObject);
	EXPECT_EQ(blast.steps.at(2).arguments.at(0).thing, target);
}

TEST(ComputerHandGesture, TheShapeStandsUprightFromWhereTheGestureBegan)
{
	// across along x and up as height, 30 m across; the hand keeps its z
	EXPECT_EQ(GesturePoint({100.0f, 20.0f, 300.0f}, {0.5f, 0.0f, 0.25f}), glm::vec3(115.0f, 27.5f, 300.0f));
	EXPECT_EQ(GesturePoint({100.0f, 20.0f, 300.0f}, {0.0f, 0.0f, 0.0f}), glm::vec3(100.0f, 20.0f, 300.0f));
}

TEST(ComputerHandGesture, AGestureGoesThroughItsShapeAndBackWhereItBegan)
{
	ComputerHand hand;
	hand.point = {100.0f, 20.0f, 300.0f};
	FakeRunner runner;
	// a shape a fifteenth of the circle's length takes two turns: half of it each
	runner.lengthRatio = 1.0f / 15.0f;
	const HandStep step {
	    .type = HandSubAction::MakeGesture,
	    .arguments = {{.kind = HandArgument::Kind::Int, .intValue = 21}, {.kind = HandArgument::Kind::Int, .intValue = 0}}};
	EXPECT_EQ(GestureStage(hand, step, 0, runner), StageResult::Done);
	EXPECT_EQ(hand.gestureOrigin, glm::vec3(100.0f, 20.0f, 300.0f));
	EXPECT_EQ(hand.point, glm::vec3(100.0f, 20.0f, 300.0f));
	EXPECT_EQ(GestureStage(hand, step, 1, runner), StageResult::Going);
	EXPECT_EQ(hand.gestureProgress, 0.5f);
	EXPECT_EQ(hand.point, glm::vec3(115.0f, 35.0f, 300.0f));
	// the whole of it: done, and the hand stays where the last turn left it
	EXPECT_EQ(GestureStage(hand, step, 1, runner), StageResult::Done);
	EXPECT_EQ(hand.gestureProgress, 1.0f);
	EXPECT_EQ(hand.point, glm::vec3(115.0f, 35.0f, 300.0f));
	EXPECT_EQ(GestureStage(hand, step, 2, runner), StageResult::Done);
	EXPECT_EQ(hand.point, glm::vec3(100.0f, 20.0f, 300.0f));
	// the miracle's selection gesture, as the town's casting action asks for it
	EXPECT_EQ(runner.gesturesAsked.front(), (std::tuple<int32_t, int32_t> {21, 0}));
}

TEST(ComputerHandGesture, AMiracleTakesTheGestureOfTheSeedTheGameGivesIt)
{
	// info.dat's rows name no seed (the game fills them at load): the gesture comes from the first seed with the
	// miracle, not from the row, which would index no seed
	auto info = std::make_unique<InfoConstants>();
	info->magicWood.at(0).spellSeedType = SpellSeedType::None;
	EXPECT_EQ(GestureOfMiracle(*info, static_cast<int32_t>(MagicType::Wood), 0), 0);
	auto& seed = info->spellSeed.at(static_cast<size_t>(SpellSeedType::Wood));
	seed.magicTypes = {MagicType::Wood, MagicType::None, MagicType::None, MagicType::None};
	seed.selectionGesture = GestureType::Spiral;
	seed.gesture = GestureType::Circle;
	EXPECT_EQ(GestureOfMiracle(*info, static_cast<int32_t>(MagicType::Wood), 0), static_cast<uint8_t>(GestureType::Spiral));
	EXPECT_EQ(GestureOfMiracle(*info, static_cast<int32_t>(MagicType::Wood), 1), static_cast<uint8_t>(GestureType::Circle));
	// out of the tables: none
	EXPECT_EQ(GestureOfMiracle(*info, -1, 0), 0);
	EXPECT_EQ(GestureOfMiracle(*info, 42, 0), 0);
}

TEST(ComputerHandGesture, ATurnNeverGoesPastTheWhole)
{
	ComputerHand hand;
	hand.gestureProgress = 0.9f;
	const std::vector<glm::vec3> shape {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}};
	EXPECT_EQ(GestureTurn(hand, shape, 1.0f / 15.0f), StageResult::Done);
	EXPECT_EQ(hand.gestureProgress, 1.0f);
}

TEST(ComputerHandGesture, TheHandsStepRunsTheGestureWithoutAMoveInFront)
{
	EXPECT_EQ(Prerequisite(HandSubAction::MakeGesture), HandSubAction::None);
}

namespace
{
/// The chant step for the wood miracle at the icon
HandStep ChantAt(entt::entity icon)
{
	return {
	    .type = HandSubAction::MakeWorshippersChantForSpell,
	    .arguments = {{.kind = HandArgument::Kind::Int, .intValue = 21}, {.kind = HandArgument::Kind::Thing, .thing = icon}}};
}
} // namespace

TEST(ComputerHandChant, ItStartsOnlyAtAnIconWithASite)
{
	const auto icon = static_cast<entt::entity>(21);
	ComputerHand hand;
	hand.waitTurns = 7;
	FakeRunner runner;
	EXPECT_EQ(ChantStage(hand, ChantAt(icon), 0, runner), StageResult::Failed);
	EXPECT_EQ(hand.waitTurns, 0);
	runner.withSite.insert(icon);
	EXPECT_EQ(ChantStage(hand, ChantAt(icon), 0, runner), StageResult::Done);
}

TEST(ComputerHandChant, TheIconStartsChargingForTheHandsPlayerAtTheMiraclesLevel)
{
	const auto icon = static_cast<entt::entity>(21);
	ComputerHand hand;
	FakeRunner runner;
	runner.withSite.insert(icon);
	runner.worshipIcons.insert(icon);
	runner.mayStartCharging = true;
	EXPECT_EQ(ChantStage(hand, ChantAt(icon), 1, runner), StageResult::Done);
	EXPECT_EQ(runner.started, (std::vector<std::tuple<entt::entity, int32_t>> {{icon, -1}}));
	// an icon already charging, for anyone, needs nothing started
	FakeRunner already;
	already.worshipIcons.insert(icon);
	already.charging.insert(icon);
	already.mayStartCharging = true;
	EXPECT_EQ(ChantStage(hand, ChantAt(icon), 1, already), StageResult::Done);
	EXPECT_TRUE(already.started.empty());
	// a thing that is no worship site's icon fails
	FakeRunner none;
	EXPECT_EQ(ChantStage(hand, ChantAt(icon), 1, none), StageResult::Failed);
}

TEST(ComputerHandChant, ItWaitsFiveWholeSecondsForTheChargeToStart)
{
	const auto icon = static_cast<entt::entity>(21);
	ComputerHand hand;
	FakeRunner runner;
	runner.withSite.insert(icon);
	runner.worshipIcons.insert(icon);
	EXPECT_EQ(ChantStage(hand, ChantAt(icon), 0, runner), StageResult::Done);
	// 59 turns are 5 whole seconds: it goes on; the 60th makes 6, and it fails
	for (int turn = 1; turn < 60; ++turn)
	{
		ASSERT_EQ(ChantStage(hand, ChantAt(icon), 1, runner), StageResult::Going) << turn;
	}
	EXPECT_EQ(ChantStage(hand, ChantAt(icon), 1, runner), StageResult::Failed);
	// without a site it fails at once
	ComputerHand other;
	runner.withSite.clear();
	EXPECT_EQ(ChantStage(other, ChantAt(icon), 1, runner), StageResult::Failed);
}

TEST(ComputerHandChant, ItsEndWaitsUpToThirtySecondsMoreForTheIconToBeCharged)
{
	const auto icon = static_cast<entt::entity>(21);
	ComputerHand hand;
	FakeRunner runner;
	runner.worshipIcons.insert(icon);
	runner.chantsNeeded[icon] = 12.0f;
	// the count goes on from the turns the start of the charge took
	hand.waitTurns = 3;
	for (int turn = 4; turn < 310; ++turn)
	{
		ASSERT_EQ(ChantStage(hand, ChantAt(icon), 2, runner), StageResult::Going) << turn;
	}
	EXPECT_EQ(ChantStage(hand, ChantAt(icon), 2, runner), StageResult::Failed);
	EXPECT_EQ(hand.waitTurns, 310);
	// charged: done, without counting
	ComputerHand charged;
	runner.chantsNeeded[icon] = 0.0f;
	EXPECT_EQ(ChantStage(charged, ChantAt(icon), 2, runner), StageResult::Done);
	EXPECT_EQ(charged.waitTurns, 0);
	EXPECT_EQ(CountedSeconds(59), 5u);
	EXPECT_EQ(CountedSeconds(60), 6u);
}

namespace
{
/// The cast step of the wood miracle at the store, from the icon
HandStep CastAt(entt::entity store, entt::entity icon, int32_t miracle = 21)
{
	return {.type = HandSubAction::CastSpellAtObject,
	        .arguments = {{.kind = HandArgument::Kind::Thing, .thing = store},
	                      {.kind = HandArgument::Kind::Int, .intValue = miracle},
	                      {.kind = HandArgument::Kind::Thing, .thing = icon}}};
}

/// A place on the map so many metres along x and z, at a height above the land
FollowedPlace PlaceAt(float x, float z, float altitude, float height)
{
	return {.place = {.x = map_coords::ToFixed(x), .z = map_coords::ToFixed(z), .altitude = altitude}, .height = height};
}
} // namespace

TEST(ComputerHandCast, ACastAtAnObjectAlwaysHasTheMoveToItInFront)
{
	// whatever the miracle, the wood one or an angry one
	const auto store = static_cast<entt::entity>(22);
	const auto icon = static_cast<entt::entity>(21);
	for (const int32_t miracle : {21, 7})
	{
		FakeRunner runner;
		ComputerHand hand;
		Append(hand, CastAt(store, icon, miracle));
		EXPECT_TRUE(Step(hand, runner));
		EXPECT_EQ(hand.steps.front().type, HandSubAction::MoveHandToObject) << miracle;
		EXPECT_EQ(hand.steps.front().arguments.at(0).thing, store) << miracle;
	}
}

TEST(ComputerHandCast, AFollowedThingIsGoneToAgainInStridesAndTheHandHeldOverIt)
{
	const FlatGround ground;
	ComputerHand hand;
	hand.point = {0.0f, 50.0f, 0.0f};
	hand.speed = 90.0f;
	// 50 m away: a step of the speed's, 9 m, held two and a half of the thing's 2 m over the land at 5 m
	FollowThing(hand, PlaceAt(50.0f, 0.0f, 1.0f, 2.0f), ground);
	EXPECT_EQ(hand.target, glm::vec3(50.0f, 3.0f, 0.0f));
	EXPECT_EQ(hand.point, glm::vec3(9.0f, 10.0f, 0.0f));
	// 150 m away: 10 m strides; 350 m away: 100 m strides
	ComputerHand nearer;
	FollowThing(nearer, PlaceAt(0.0f, 150.0f, 0.0f, 2.0f), ground);
	EXPECT_EQ(nearer.point, glm::vec3(0.0f, 10.0f, 10.0f));
	ComputerHand distant;
	FollowThing(distant, PlaceAt(0.0f, 350.0f, 0.0f, 2.0f), ground);
	EXPECT_EQ(distant.point, glm::vec3(0.0f, 10.0f, 100.0f));
	// no further than the thing
	ComputerHand nextToIt;
	nextToIt.point = {48.0f, 0.0f, 0.0f};
	FollowThing(nextToIt, PlaceAt(50.0f, 0.0f, 0.0f, 2.0f), ground);
	EXPECT_EQ(nextToIt.point, glm::vec3(50.0f, 10.0f, 0.0f));
}

TEST(ComputerHandCast, ItsTurnFollowsTheThingCastsFromTheHandAndKeepsTheCast)
{
	const FlatGround ground;
	const auto store = static_cast<entt::entity>(22);
	const auto icon = static_cast<entt::entity>(21);
	const auto spell = static_cast<entt::entity>(30);
	FakeRunner runner;
	runner.followed[store] = PlaceAt(50.0f, 0.0f, 0.0f, 2.0f);
	runner.castGives = spell;
	runner.secondsAfterCast = 2.5f;
	ComputerHand hand;
	hand.point = {0.0f, 50.0f, 0.0f};
	hand.speed = 90.0f;
	EXPECT_EQ(CastAtObjectStage(hand, CastAt(store, icon), 0, runner, ground), StageResult::Done);
	EXPECT_EQ(CastAtObjectStage(hand, CastAt(store, icon), 1, runner, ground), StageResult::Done);
	ASSERT_EQ(runner.casts.size(), 1u);
	EXPECT_EQ(runner.casts.front(),
	          (std::tuple<entt::entity, int32_t, glm::vec3, entt::entity> {store, 21, glm::vec3(9.0f, 10.0f, 0.0f), icon}));
	EXPECT_EQ(runner.kept, (std::vector<std::tuple<entt::entity, entt::entity>> {{spell, store}}));
	EXPECT_EQ(hand.waitTurns, 25);
	// no miracle cast: the step fails, and nothing is kept
	FakeRunner none;
	ComputerHand failed;
	EXPECT_EQ(CastAtObjectStage(failed, CastAt(store, icon), 1, none, ground), StageResult::Failed);
	EXPECT_TRUE(none.kept.empty());
}

TEST(ComputerHandCast, AnAngryMiracleIsCastWithoutFollowingTheThingFirst)
{
	const FlatGround ground;
	const auto store = static_cast<entt::entity>(22);
	const auto icon = static_cast<entt::entity>(21);
	FakeRunner runner;
	runner.followed[store] = PlaceAt(50.0f, 0.0f, 0.0f, 2.0f);
	runner.castGives = static_cast<entt::entity>(30);
	runner.angry.insert(21);
	ComputerHand hand;
	hand.point = {0.0f, 50.0f, 0.0f};
	hand.speed = 90.0f;
	EXPECT_EQ(CastAtObjectStage(hand, CastAt(store, icon), 1, runner, ground), StageResult::Done);
	ASSERT_EQ(runner.casts.size(), 1u);
	EXPECT_EQ(std::get<2>(runner.casts.front()), glm::vec3(0.0f, 50.0f, 0.0f));
	EXPECT_EQ(hand.point, glm::vec3(0.0f, 50.0f, 0.0f));
}

TEST(ComputerHandCast, ItsEndStaysWithTheThingForTheMiraclesTime)
{
	const FlatGround ground;
	const auto store = static_cast<entt::entity>(22);
	FakeRunner runner;
	runner.followed[store] = PlaceAt(50.0f, 0.0f, 0.0f, 2.0f);
	ComputerHand hand;
	hand.speed = 90.0f;
	hand.waitTurns = 3;
	EXPECT_EQ(CastAtObjectStage(hand, CastAt(store, entt::null), 2, runner, ground), StageResult::Going);
	EXPECT_EQ(hand.point.x, 9.0f);
	EXPECT_EQ(CastAtObjectStage(hand, CastAt(store, entt::null), 2, runner, ground), StageResult::Going);
	EXPECT_EQ(CastAtObjectStage(hand, CastAt(store, entt::null), 2, runner, ground), StageResult::Done);
	EXPECT_EQ(hand.point.x, 18.0f);
	// a time of no turns counts below zero and goes on
	ComputerHand forever;
	EXPECT_EQ(CastAtObjectStage(forever, CastAt(store, entt::null), 2, runner, ground), StageResult::Going);
	EXPECT_EQ(forever.waitTurns, -1);
}

namespace
{
/// The mind with the node of the action given, its thing the one given, choosing its first leaf, whose thing is given
std::pair<ecs::components::ComputerMind, uint16_t> ChoseLeaf(computer_player::ActionKind kind, entt::entity above,
                                                             entt::entity leafThing)
{
	auto mind = computer_player::MakeMind();
	const auto& nodes = computer_player::MindNodes();
	uint16_t node = 0;
	while (computer_player::k_MindRows.at(nodes.at(node).row).action != kind)
	{
		++node;
	}
	const auto leaf = nodes.at(node).children.at(0);
	mind.nodes.at(node).thing = above;
	mind.nodes.at(node).chosen = leaf;
	mind.nodes.at(leaf).chosen = leaf;
	mind.nodes.at(leaf).thing = leafThing;
	return {mind, node};
}
} // namespace

TEST(ComputerHandActions, ATownIsDestroyedWithTheIconsMiracleCastTowardsWhatIsAbove)
{
	// the icon is the node's, the object the nearest above its parent's; the miracle is cast towards it from inside
	// the influence, as the cast towards a thing gives it
	const auto icon = static_cast<entt::entity>(21);
	const auto object = static_cast<entt::entity>(30);
	FakeRunner runner;
	FakeFacts facts;
	facts.icons[icon] = MagicType::Fireball;
	facts.objects.insert(object);
	facts.mapPlaces[object] = {.x = 40 << 16, .z = 50 << 16};
	test::FakeScoreWorld scores;
	scores.inside = true;
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParent("DestroytownWithSpell");
	mind.nodes.at(node).thing = icon;
	mind.nodes.at(parent).thing = object;
	ComputerHand hand;
	// the icon where the hand is: it goes to the icon rather than gesturing
	facts.mapPlaces[icon] = {.x = hand.mapX, .z = hand.mapZ};
	HandActions actions(hand, runner, facts, &scores);
	EXPECT_TRUE(actions.HasAction(computer_player::ActionKind::DestroytownWithSpell));
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::DestroytownWithSpell, mind, node));

	const auto place = facts.LandPoint(facts.mapPlaces[object]);
	ComputerHand expected;
	ASSERT_TRUE(CastIconsMiracleAtOrToward(expected, icon, MagicType::Fireball, 0.0f, object,
	                                       {.place = place, .edge = place, .inside = place}));
	ASSERT_EQ(hand.steps.size(), expected.steps.size());
	for (size_t i = 0; i < hand.steps.size(); ++i)
	{
		EXPECT_EQ(hand.steps.at(i).type, expected.steps.at(i).type) << i;
	}

	// with no score world, or an icon with no miracle, nothing is done
	ComputerHand none;
	HandActions unscored(none, runner, facts);
	EXPECT_FALSE(unscored.RunAction(computer_player::ActionKind::DestroytownWithSpell, mind, node));
	facts.icons.erase(icon);
	HandActions noMiracle(none, runner, facts, &scores);
	EXPECT_FALSE(noMiracle.RunAction(computer_player::ActionKind::DestroytownWithSpell, mind, node));
	EXPECT_TRUE(none.steps.empty());
}

TEST(ComputerHandActions, ACreatureIsAttackedWithTheIconsMiracleTwoOrThreeTimesByTheGamesDraws)
{
	// each round moves near the creature, chants and casts on it; the rounds are drawn afresh after each
	const auto icon = static_cast<entt::entity>(21);
	const auto creature = static_cast<entt::entity>(40);
	FakeRunner runner;
	FakeFacts facts;
	facts.icons[icon] = MagicType::Fireball;
	facts.creatures.insert(creature);
	test::FakeScoreWorld scores;
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParent("AttackCreatureWithSpells");
	mind.nodes.at(node).thing = icon;
	mind.nodes.at(parent).thing = creature;
	uint32_t seed = scores.seed;
	uint32_t rounds = game_random::SeededRandom(2, seed) + 2;
	uint32_t done = 0;
	while (done < rounds)
	{
		++done;
		rounds = game_random::SeededRandom(2, seed) + 2;
	}
	ComputerHand hand;
	HandActions actions(hand, runner, facts, &scores);
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::AttackCreatureWithSpells, mind, node));
	ASSERT_EQ(hand.steps.size(), 3u * done);
	EXPECT_GE(done, 2u);
	for (size_t i = 0; i < hand.steps.size(); i += 3)
	{
		EXPECT_EQ(hand.steps.at(i).type, HandSubAction::MoveHandNearObject);
		EXPECT_EQ(hand.steps.at(i).arguments.at(0).thing, creature);
		EXPECT_EQ(hand.steps.at(i + 1).type, HandSubAction::MakeWorshippersChantForSpell);
		EXPECT_EQ(hand.steps.at(i + 2).type, HandSubAction::CastSpellAtObject);
		EXPECT_EQ(hand.steps.at(i + 2).arguments.at(0).thing, creature);
	}
	EXPECT_EQ(scores.seed, seed);

	// not a creature above: nothing
	facts.creatures.clear();
	ComputerHand none;
	HandActions noCreature(none, runner, facts, &scores);
	EXPECT_FALSE(noCreature.RunAction(computer_player::ActionKind::AttackCreatureWithSpells, mind, node));
	EXPECT_TRUE(none.steps.empty());
}

TEST(ComputerHandActions, ABuildingSitesWorkerIsDroppedRoundItsBuilding)
{
	// the villager is the chosen leaf's, the site the node's; it goes round the site's building
	const auto site = static_cast<entt::entity>(24);
	const auto building = static_cast<entt::entity>(25);
	const auto villager = static_cast<entt::entity>(9);
	FakeRunner runner;
	FakeFacts facts;
	facts.siteBuildings[site] = building;
	facts.mapPlaces[building] = {.x = 30 << 16, .z = 40 << 16};
	test::FakeScoreWorld scores;
	scores.radii[building] = 8.0f;
	auto [mind, node] = ChoseLeaf(computer_player::ActionKind::GiveBuildingSiteWorkers, site, villager);
	ComputerHand hand;
	HandActions actions(hand, runner, facts, &scores);
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::GiveBuildingSiteWorkers, mind, node));
	ASSERT_GE(hand.steps.size(), 2u);
	EXPECT_EQ(hand.steps.front().arguments.at(0).thing, villager);
	EXPECT_EQ(hand.steps.back().type, HandSubAction::DropObjectInHandAtPos);
	const auto place =
	    WorkerDropPlace(facts.mapPlaces[building], 8.0f, [](const map_coords::MapCoords& /*place*/) { return false; });
	EXPECT_EQ(hand.steps.back().arguments.at(0).point, facts.LandPoint(place));

	// a site with no building: nothing
	facts.siteBuildings.clear();
	ComputerHand none;
	HandActions noBuilding(none, runner, facts, &scores);
	EXPECT_FALSE(noBuilding.RunAction(computer_player::ActionKind::GiveBuildingSiteWorkers, mind, node));
	EXPECT_TRUE(none.steps.empty());
}

TEST(ComputerHandActions, AWorshipperIsTakenHomeOutsideItsAbode)
{
	// the villager is the node's, the worship site the nearest above its parent's; it is dropped outside its abode
	const auto villager = static_cast<entt::entity>(9);
	const auto site = static_cast<entt::entity>(20);
	const auto town = static_cast<entt::entity>(22);
	const auto abode = static_cast<entt::entity>(23);
	FakeRunner runner;
	FakeFacts facts;
	facts.villagers.insert(villager);
	facts.sites.insert(site);
	facts.homes[villager] = {.town = town, .abode = abode};
	facts.mapPlaces[abode] = {.x = 30 << 16, .z = 40 << 16};
	facts.arrivePlaces[abode] = {.x = 34 << 16, .z = 40 << 16};
	test::FakeScoreWorld scores;
	scores.radii[abode] = 6.0f;
	scores.radii[villager] = 1.0f;
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParent("TakeVillagerFromWorshipSiteToTown");
	mind.nodes.at(node).thing = villager;
	mind.nodes.at(parent).thing = site;
	ComputerHand hand;
	HandActions actions(hand, runner, facts, &scores);
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::TakeVillagerFromWorshipSiteToTown, mind, node));
	ASSERT_GE(hand.steps.size(), 2u);
	EXPECT_EQ(hand.steps.front().arguments.at(0).thing, villager);
	EXPECT_EQ(hand.steps.back().type, HandSubAction::DropObjectInHandAtPos);
	const auto place = HomeDropPlace(facts.mapPlaces[abode], facts.arrivePlaces[abode], 6.0f, 1.0f);
	EXPECT_EQ(hand.steps.back().arguments.at(0).point, facts.LandPoint(place));

	// a villager with no town: nothing
	facts.homes.clear();
	ComputerHand none;
	HandActions homeless(none, runner, facts, &scores);
	EXPECT_FALSE(homeless.RunAction(computer_player::ActionKind::TakeVillagerFromWorshipSiteToTown, mind, node));
	EXPECT_TRUE(none.steps.empty());
}

TEST(ComputerHandMoves, FoodIsThrownFromTwentyMetresShortOfTheCreatureOrBackFromANearOne)
{
	// far: on the way to the creature, 20 m short of it
	EXPECT_EQ(FoodThrowFrom({0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 50.0f}), glm::vec3(0.0f, 1.0f, 30.0f));
	const auto slant = FoodThrowFrom({0.0f, 0.0f, 0.0f}, {30.0f, 0.0f, 40.0f});
	EXPECT_NEAR(slant.x, 18.0f, 1e-4f);
	EXPECT_NEAR(slant.z, 24.0f, 1e-4f);
	// nearer than 20 m: 20 m back from the hand, away from the creature
	EXPECT_EQ(FoodThrowFrom({10.0f, 5.0f, 10.0f}, {10.0f, 5.0f, 13.0f}), glm::vec3(10.0f, 5.0f, -10.0f));
	// exactly 20 m is not nearer: the hand stays where it is
	EXPECT_EQ(FoodThrowFrom({0.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}), glm::vec3(0.0f, 0.0f, 0.0f));
	// on the creature: no way to go, so the hand's own place
	EXPECT_EQ(FoodThrowFrom({1.0f, 2.0f, 3.0f}, {1.0f, 2.0f, 3.0f}), glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST(ComputerHandActions, FoodIsThrownAtTheCreatureRoundAPointDrawnNearIt)
{
	// the food is the chosen leaf's, the creature the node's; picked up, carried and thrown at the creature's place
	// with two draws of up to 2 m each way, at the height of the creature's size
	const auto food = static_cast<entt::entity>(30);
	const auto creature = static_cast<entt::entity>(40);
	FakeRunner runner;
	FakeFacts facts;
	facts.creatures.insert(creature);
	facts.places[creature] = glm::vec3(30.0f, 7.0f, 40.0f);
	test::FakeScoreWorld scores;
	scores.scales[creature] = 2.0f;
	auto [mind, node] = ChoseLeaf(computer_player::ActionKind::ThrowFoodatCreature, creature, food);
	ComputerHand hand;
	hand.playerSpeed = 120.0f;
	HandActions actions(hand, runner, facts, &scores);
	EXPECT_TRUE(actions.HasAction(computer_player::ActionKind::ThrowFoodatCreature));
	uint32_t seed = scores.seed;
	const float x = (game_random::FloatRand(4.0f, seed) - 2.0f) + 30.0f;
	const float z = (game_random::FloatRand(4.0f, seed) - 2.0f) + 40.0f;
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::ThrowFoodatCreature, mind, node));
	ASSERT_EQ(hand.steps.size(), 3u);
	EXPECT_EQ(hand.steps.at(0).type, HandSubAction::PickUpObject);
	EXPECT_EQ(hand.steps.at(0).arguments.at(0).thing, food);
	EXPECT_EQ(hand.steps.at(0).arguments.at(1).floatValue, 120.0f);
	EXPECT_EQ(hand.steps.at(1).type, HandSubAction::MoveHandToPos);
	EXPECT_EQ(hand.steps.at(1).arguments.at(0).point, FoodThrowFrom(hand.point, facts.places[creature]));
	EXPECT_EQ(hand.steps.at(2).type, HandSubAction::ThrowObjectInHandAtPos);
	EXPECT_EQ(hand.steps.at(2).arguments.at(0).point, glm::vec3(x, 30.0f, z));
	EXPECT_EQ(scores.seed, seed);

	// food the hand may not take: no steps and no draws
	runner.refused.insert(food);
	ComputerHand refused;
	HandActions noFood(refused, runner, facts, &scores);
	EXPECT_FALSE(noFood.RunAction(computer_player::ActionKind::ThrowFoodatCreature, mind, node));
	EXPECT_TRUE(refused.steps.empty());
	EXPECT_EQ(scores.seed, seed);
}

TEST(ComputerHandActions, UsingTheCreatureAsADiversionFailsAtOnceAndIsHeldBack)
{
	// the action is the game's, and it does nothing: the steps already given are dropped, none are added, and the node
	// is held back as after any failure
	FakeRunner runner;
	FakeFacts facts;
	test::FakeScoreWorld scores;
	ComputerHand hand;
	Append(hand, StepToPoint(HandSubAction::MoveHandToPos, {1.0f, 2.0f, 3.0f}));
	HandActions actions(hand, runner, facts, &scores);
	EXPECT_TRUE(actions.HasAction(computer_player::ActionKind::UseCreatureAsDiverseionAgainstPlayer));
	auto mind = computer_player::MakeMind();
	const auto node = computer_player::FindNode("UseCreatureAsDiverseionAgainstPlayer");
	ASSERT_TRUE(node.has_value());
	EXPECT_FALSE(computer_player::RunAction(computer_player::GameTree(), mind, *node, actions));
	EXPECT_TRUE(hand.steps.empty());
	EXPECT_EQ(mind.nodes.at(*node).suppression, computer_player::k_SuppressionAfterFailure);
}

TEST(ComputerHandLeash, TheLeashTypeIsSetOnlyForACreatureWithAPlayerAndEveryStageIsDoneAtOnce)
{
	const auto creature = static_cast<entt::entity>(40);
	FakeRunner runner;
	HandStep step {.type = HandSubAction::SetLeash,
	               .arguments = {ThingOf(creature), {.kind = HandArgument::Kind::Int, .intValue = 3}}};
	EXPECT_EQ(SetLeashStage(step, 1, runner), StageResult::Done);
	EXPECT_TRUE(runner.leashTypes.empty());
	runner.owned.insert(creature);
	EXPECT_EQ(SetLeashStage(step, 0, runner), StageResult::Done);
	EXPECT_EQ(SetLeashStage(step, 2, runner), StageResult::Done);
	EXPECT_TRUE(runner.leashTypes.empty());
	EXPECT_EQ(SetLeashStage(step, 1, runner), StageResult::Done);
	ASSERT_EQ(runner.leashTypes.size(), 1u);
	EXPECT_EQ(runner.leashTypes.front(), (std::tuple<entt::entity, int32_t> {creature, 3}));
}

TEST(ComputerHandLeash, TheTieTakesATiedLeashOffTiesItWaitsTwoSecondsAndEndsWithinThirtyMetres)
{
	const auto creature = static_cast<entt::entity>(40);
	const auto object = static_cast<entt::entity>(30);
	FakeRunner runner;
	const FlatGround ground;
	HandStep step {.type = HandSubAction::AttachLeashToObject, .arguments = {ThingOf(creature), ThingOf(object)}};
	ComputerHand hand;
	hand.speed = 120.0f;
	// a creature with no player: the start fails
	EXPECT_EQ(AttachLeashStage(hand, step, 0, runner, ground), StageResult::Failed);
	runner.owned.insert(creature);
	runner.tied.insert(creature);
	EXPECT_EQ(AttachLeashStage(hand, step, 0, runner, ground), StageResult::Done);
	EXPECT_EQ(runner.takenOff, std::vector<entt::entity> {creature});
	// not an object: the tie fails
	EXPECT_EQ(AttachLeashStage(hand, step, 1, runner, ground), StageResult::Failed);
	EXPECT_TRUE(runner.ties.empty());
	runner.leashable.insert(object);
	EXPECT_EQ(AttachLeashStage(hand, step, 1, runner, ground), StageResult::Done);
	ASSERT_EQ(runner.ties.size(), 1u);
	EXPECT_EQ(runner.ties.front(), (std::tuple<entt::entity, entt::entity> {creature, object}));
	EXPECT_EQ(hand.waitTurns, WaitTurns(2.0f));
	// the end follows the thing and counts the wait down, then is done once the hand is within 30 m of it
	runner.followed[object] = FollowedPlace {.place = {.x = 100 << 16, .z = 0}, .height = 0.0f};
	const int32_t waits = hand.waitTurns;
	for (int32_t i = 0; i < waits; ++i)
	{
		EXPECT_EQ(AttachLeashStage(hand, step, 2, runner, ground), StageResult::Going) << i;
	}
	EXPECT_EQ(hand.waitTurns, 0);
	StageResult last = StageResult::Going;
	for (int i = 0; i < 1000 && last == StageResult::Going; ++i)
	{
		last = AttachLeashStage(hand, step, 2, runner, ground);
	}
	EXPECT_EQ(last, StageResult::Done);
	EXPECT_LT(game_math::GetDistanceInMetres(map_coords::FromWorld(hand.point), runner.followed[object].place),
	          k_TiedLeashNear);
}

TEST(ComputerHandActions, ItsCreatureIsPutOnTheAngryLeashTiedToTheOtherCreature)
{
	// its creature is the node's, the other creature the nearest above its parent's
	const auto mine = static_cast<entt::entity>(40);
	const auto other = static_cast<entt::entity>(41);
	FakeRunner runner;
	FakeFacts facts;
	facts.creatures.insert(mine);
	facts.creatures.insert(other);
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParent("AttackCreatureWithCreature");
	mind.nodes.at(node).thing = mine;
	mind.nodes.at(parent).thing = other;
	ComputerHand hand;
	HandActions actions(hand, runner, facts);
	EXPECT_TRUE(actions.HasAction(computer_player::ActionKind::AttackCreatureWithCreature));
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::AttackCreatureWithCreature, mind, node));
	ASSERT_EQ(hand.steps.size(), 2u);
	EXPECT_EQ(hand.steps.at(0).type, HandSubAction::SetLeash);
	EXPECT_EQ(hand.steps.at(0).arguments.at(0).thing, mine);
	EXPECT_EQ(hand.steps.at(0).arguments.at(1).intValue, static_cast<int32_t>(LeashType::Evil));
	EXPECT_EQ(hand.steps.at(1).type, HandSubAction::AttachLeashToObject);
	EXPECT_EQ(hand.steps.at(1).arguments.at(0).thing, mine);
	EXPECT_EQ(hand.steps.at(1).arguments.at(1).thing, other);

	// no other creature above: nothing
	facts.creatures.erase(other);
	ComputerHand none;
	HandActions noOther(none, runner, facts);
	EXPECT_FALSE(noOther.RunAction(computer_player::ActionKind::AttackCreatureWithCreature, mind, node));
	EXPECT_TRUE(none.steps.empty());
}

TEST(ComputerHandActions, ItsCreatureIsPutOnTheAngryLeashTiedToTheTownsStoreToDestroyIt)
{
	// the thing above its parent gives the town, the creature is the node's
	const auto mine = static_cast<entt::entity>(40);
	const auto building = static_cast<entt::entity>(42);
	const auto town = static_cast<entt::entity>(43);
	const auto pit = static_cast<entt::entity>(44);
	FakeRunner runner;
	FakeFacts facts;
	facts.creatures.insert(mine);
	test::FakeScoreWorld scores;
	scores.places[building] = {};
	scores.townOf[building] = town;
	scores.pits[town] = pit;
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParent("DestroytownWithCreature");
	mind.nodes.at(node).thing = mine;
	mind.nodes.at(parent).thing = building;
	ComputerHand hand;
	HandActions actions(hand, runner, facts, &scores);
	EXPECT_TRUE(actions.HasAction(computer_player::ActionKind::DestroytownWithCreature));
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::DestroytownWithCreature, mind, node));
	ASSERT_EQ(hand.steps.size(), 2u);
	EXPECT_EQ(hand.steps.at(0).type, HandSubAction::SetLeash);
	EXPECT_EQ(hand.steps.at(0).arguments.at(0).thing, mine);
	EXPECT_EQ(hand.steps.at(0).arguments.at(1).intValue, static_cast<int32_t>(LeashType::Evil));
	EXPECT_EQ(hand.steps.at(1).type, HandSubAction::AttachLeashToObject);
	EXPECT_EQ(hand.steps.at(1).arguments.at(0).thing, mine);
	EXPECT_EQ(hand.steps.at(1).arguments.at(1).thing, pit);
	// the creature knows some of the angry miracles it could be taught: no lesson owed
	EXPECT_EQ(mind.destroyLessons, 0u);

	// a town is its own; with no store its newest building, and a creature knowing none of them is owed a lesson
	scores.places[town] = {};
	scores.towns.insert(town);
	scores.pits.clear();
	scores.abodesOf[town] = {building};
	scores.miracleKnowledge[{mine, CreatureDesires::Anger}] = 0.0f;
	mind.nodes.at(parent).thing = town;
	ComputerHand atTown;
	HandActions onTown(atTown, runner, facts, &scores);
	EXPECT_TRUE(onTown.RunAction(computer_player::ActionKind::DestroytownWithCreature, mind, node));
	ASSERT_EQ(atTown.steps.size(), 2u);
	EXPECT_EQ(atTown.steps.at(1).arguments.at(1).thing, building);
	EXPECT_EQ(mind.destroyLessons, 1u);

	// nothing in the town to go for: nothing, and no lesson
	scores.abodesOf.clear();
	ComputerHand none;
	HandActions nothing(none, runner, facts, &scores);
	EXPECT_FALSE(nothing.RunAction(computer_player::ActionKind::DestroytownWithCreature, mind, node));
	EXPECT_TRUE(none.steps.empty());
	EXPECT_EQ(mind.destroyLessons, 1u);
}

TEST(ComputerHandLeash, TheDominantDesireIsForcedAtTheFirstStageAndTheHandWaitsNoMore)
{
	const auto creature = static_cast<entt::entity>(40);
	FakeRunner runner;
	ComputerHand hand;
	hand.waitTurns = 7;
	const HandStep step {.type = HandSubAction::ForceCreatureDominantDesire,
	                     .arguments = {{.kind = HandArgument::Kind::Thing, .thing = creature},
	                                   {.kind = HandArgument::Kind::Int, .intValue = 2},
	                                   {.kind = HandArgument::Kind::Float, .floatValue = 30.0f}}};
	EXPECT_EQ(ForceDominantDesireStage(hand, step, 0, runner), StageResult::Done);
	ASSERT_EQ(runner.dominant.size(), 1u);
	EXPECT_EQ(std::get<0>(runner.dominant.at(0)), creature);
	EXPECT_EQ(std::get<1>(runner.dominant.at(0)), 2);
	EXPECT_EQ(std::get<2>(runner.dominant.at(0)), 30.0f);
	EXPECT_EQ(hand.waitTurns, 0);
	// the other stages do nothing
	EXPECT_EQ(ForceDominantDesireStage(hand, step, 1, runner), StageResult::Done);
	EXPECT_EQ(ForceDominantDesireStage(hand, step, 2, runner), StageResult::Done);
	EXPECT_EQ(runner.dominant.size(), 1u);
}

TEST(ComputerHandActions, ItsCreatureImpressesATownOnTheLearningLeashTiedToItsStoreForTwoMinutes)
{
	// the thing above its parent gives the town, the creature is the node's
	const auto mine = static_cast<entt::entity>(40);
	const auto building = static_cast<entt::entity>(42);
	const auto town = static_cast<entt::entity>(43);
	const auto pit = static_cast<entt::entity>(44);
	FakeRunner runner;
	FakeFacts facts;
	facts.creatures.insert(mine);
	test::FakeScoreWorld scores;
	scores.places[building] = {};
	scores.townOf[building] = town;
	scores.pits[town] = pit;
	scores.townCentres[town] = static_cast<entt::entity>(45);
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParent("UseCreatureToImpressTown");
	mind.nodes.at(node).thing = mine;
	mind.nodes.at(parent).thing = building;
	ComputerHand hand;
	HandActions actions(hand, runner, facts, &scores);
	EXPECT_TRUE(actions.HasAction(computer_player::ActionKind::UseCreatureToImpressTown));
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::UseCreatureToImpressTown, mind, node));
	ASSERT_EQ(hand.steps.size(), 3u);
	EXPECT_EQ(hand.steps.at(0).type, HandSubAction::SetLeash);
	EXPECT_EQ(hand.steps.at(0).arguments.at(1).intValue, static_cast<int32_t>(LeashType::Rope));
	EXPECT_EQ(hand.steps.at(1).type, HandSubAction::AttachLeashToObject);
	EXPECT_EQ(hand.steps.at(1).arguments.at(1).thing, pit);
	EXPECT_EQ(hand.steps.at(2).type, HandSubAction::ForceCreatureDominantDesire);
	EXPECT_EQ(hand.steps.at(2).arguments.at(0).thing, mine);
	EXPECT_EQ(hand.steps.at(2).arguments.at(1).intValue, static_cast<int32_t>(CreatureDesires::ToImpress));
	EXPECT_EQ(hand.steps.at(2).arguments.at(2).floatValue, 120.0f);
	// no lesson is owed for impressing
	EXPECT_EQ(mind.impressLessons, 0u);

	// with no store, the newest building, not the centre
	scores.pits.clear();
	scores.abodesOf[town] = {building};
	ComputerHand noPit;
	HandActions withoutPit(noPit, runner, facts, &scores);
	EXPECT_TRUE(withoutPit.RunAction(computer_player::ActionKind::UseCreatureToImpressTown, mind, node));
	ASSERT_EQ(noPit.steps.size(), 3u);
	EXPECT_EQ(noPit.steps.at(1).arguments.at(1).thing, building);

	// no building either: nothing
	scores.abodesOf.clear();
	ComputerHand none;
	HandActions nothing(none, runner, facts, &scores);
	EXPECT_FALSE(nothing.RunAction(computer_player::ActionKind::UseCreatureToImpressTown, mind, node));
	EXPECT_TRUE(none.steps.empty());
}

TEST(ComputerHandLeash, TheForcedActionIsOrderedAtTheStartAndFollowedWhileTheCreatureIsCommanded)
{
	const auto creature = static_cast<entt::entity>(40);
	const auto icon = static_cast<entt::entity>(41);
	FakeRunner runner;
	FlatGround ground;
	ComputerHand hand;
	const HandStep step {.type = HandSubAction::ForceCreatureToPerformAction,
	                     .arguments = {{.kind = HandArgument::Kind::Thing, .thing = creature},
	                                   {.kind = HandArgument::Kind::Int, .intValue = 82},
	                                   {.kind = HandArgument::Kind::Thing, .thing = icon}}};
	EXPECT_EQ(ForceActionStage(hand, step, 0, runner, ground), StageResult::Done);
	ASSERT_EQ(runner.orders.size(), 1u);
	EXPECT_EQ(std::get<0>(runner.orders.at(0)), creature);
	EXPECT_EQ(std::get<1>(runner.orders.at(0)), 82u);
	EXPECT_EQ(std::get<2>(runner.orders.at(0)), icon);
	// it goes on while the creature carries out the commanded plan
	runner.commanded.insert(creature);
	EXPECT_EQ(ForceActionStage(hand, step, 1, runner, ground), StageResult::Going);
	runner.commanded.erase(creature);
	EXPECT_EQ(ForceActionStage(hand, step, 1, runner, ground), StageResult::Done);
	EXPECT_EQ(ForceActionStage(hand, step, 2, runner, ground), StageResult::Done);
	EXPECT_EQ(runner.orders.size(), 1u);
}

TEST(ComputerHandActions, AMiracleLessonTeachesTheFirstMiracleLeftAndCastsItOnTheCreatureFromTheIcon)
{
	// the creature is the nearest above its parent, the icon the node's
	const auto mine = static_cast<entt::entity>(40);
	const auto icon = static_cast<entt::entity>(41);
	FakeRunner runner;
	FakeFacts facts;
	facts.creatures.insert(mine);
	facts.icons[icon] = MagicType::Fireball;
	facts.places[mine] = {10.0f, 2.0f, 20.0f};
	test::FakeScoreWorld scores;
	scores.miraclesLeft[{mine, CreatureDesires::Anger}] = static_cast<uint32_t>(MagicType::LightningBolt);
	struct Lesson
	{
		std::string_view name;
		computer_player::ActionKind kind;
	};
	for (const auto& lesson :
	     {Lesson {"TeachCreatureAggressiveMagic: 164 ", computer_player::ActionKind::TeachCreatureAggressiveMagic},
	      Lesson {"TeachCreatureCompassionateMagic: 166 ", computer_player::ActionKind::TeachCreatureCompassionateMagic},
	      Lesson {"TeachCreatureImpressiveMagic: 168 ", computer_player::ActionKind::TeachCreatureImpressiveMagic}})
	{
		auto mind = computer_player::MakeMind();
		mind.destroyLessons = 2;
		mind.helpLessons = 3;
		mind.impressLessons = 4;
		const auto [node, parent] = NodeAndParentOfRow(lesson.name);
		mind.nodes.at(node).thing = icon;
		mind.nodes.at(parent).thing = mine;
		ComputerHand hand;
		HandActions actions(hand, runner, facts, &scores);
		EXPECT_TRUE(actions.HasAction(lesson.kind)) << lesson.name;
		const bool aggressive = lesson.kind == computer_player::ActionKind::TeachCreatureAggressiveMagic;
		runner.taught.clear();
		// only the angry lesson has a miracle left here
		EXPECT_EQ(actions.RunAction(lesson.kind, mind, node), aggressive) << lesson.name;
		if (!aggressive)
		{
			EXPECT_TRUE(hand.steps.empty());
			EXPECT_TRUE(runner.taught.empty());
			EXPECT_EQ(mind.helpLessons, 3u);
			continue;
		}
		EXPECT_EQ(mind.destroyLessons, 0u);
		EXPECT_EQ(mind.helpLessons, 3u);
		ASSERT_EQ(runner.taught.size(), 1u);
		EXPECT_EQ(runner.taught.at(0), std::make_tuple(mine, 1u, static_cast<uint32_t>(MagicType::LightningBolt)));
		ASSERT_EQ(hand.steps.size(), 6u);
		EXPECT_EQ(hand.steps.at(0).type, HandSubAction::MoveHandToPos);
		EXPECT_EQ(hand.steps.at(0).arguments.at(0).point, facts.places[mine]);
		EXPECT_EQ(hand.steps.at(1).type, HandSubAction::MoveHandToObject);
		EXPECT_EQ(hand.steps.at(1).arguments.at(0).thing, icon);
		EXPECT_EQ(hand.steps.at(2).type, HandSubAction::ForceCreatureToPerformAction);
		EXPECT_EQ(hand.steps.at(2).arguments.at(0).thing, mine);
		EXPECT_EQ(hand.steps.at(2).arguments.at(1).intValue, 82);
		EXPECT_EQ(hand.steps.at(2).arguments.at(2).thing, icon);
		EXPECT_EQ(hand.steps.at(3).type, HandSubAction::MoveHandToObject);
		EXPECT_EQ(hand.steps.at(3).arguments.at(0).thing, mine);
		EXPECT_EQ(hand.steps.at(4).type, HandSubAction::MakeWorshippersChantForSpell);
		EXPECT_EQ(hand.steps.at(4).arguments.at(0).intValue, static_cast<int32_t>(MagicType::LightningBolt));
		EXPECT_EQ(hand.steps.at(5).type, HandSubAction::CastSpellAtObject);
		EXPECT_EQ(hand.steps.at(5).arguments.at(0).thing, mine);
		EXPECT_EQ(hand.steps.at(5).arguments.at(2).thing, icon);
	}
}

TEST(ComputerHandLeash, TheTotemIsRaisedAtTheFirstStage)
{
	const auto totem = static_cast<entt::entity>(50);
	FakeRunner runner;
	const HandStep step {.type = HandSubAction::RaiseTotem,
	                     .arguments = {{.kind = HandArgument::Kind::Thing, .thing = totem},
	                                   {.kind = HandArgument::Kind::Float, .floatValue = 1.0f}}};
	EXPECT_EQ(RaiseTotemStage(step, 0, runner), StageResult::Done);
	ASSERT_EQ(runner.raised.size(), 1u);
	EXPECT_EQ(runner.raised.at(0), std::make_tuple(totem, 1.0f));
	EXPECT_EQ(RaiseTotemStage(step, 1, runner), StageResult::Done);
	EXPECT_EQ(RaiseTotemStage(step, 2, runner), StageResult::Done);
	EXPECT_EQ(runner.raised.size(), 1u);
}

TEST(ComputerHandActions, AnOrdinaryLessonAtATotemOrdersTheCreatureThereAndRaisesItFully)
{
	// the creature is the node's
	const auto mine = static_cast<entt::entity>(40);
	const auto totem = static_cast<entt::entity>(50);
	FakeRunner runner;
	FakeFacts facts;
	facts.creatures.insert(mine);
	facts.places[mine] = {10.0f, 2.0f, 20.0f};
	test::FakeScoreWorld scores;
	scores.places[mine] = {.x = 7 << 16, .z = 9 << 16};
	scores.normalLeft[mine] = 2u;
	scores.nearestOfType[ObjectType::TotemStatue] = totem;
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParentOfRow("TeachCreatureNormalAction: 68 ");
	mind.nodes.at(node).thing = mine;
	ComputerHand hand;
	HandActions actions(hand, runner, facts, &scores);
	EXPECT_TRUE(actions.HasAction(computer_player::ActionKind::TeachCreatureNormalAction));
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::TeachCreatureNormalAction, mind, node));
	ASSERT_EQ(runner.taught.size(), 1u);
	EXPECT_EQ(runner.taught.at(0), std::make_tuple(mine, 0u, 2u));
	ASSERT_EQ(scores.nearestAsked.size(), 1u);
	EXPECT_EQ(std::get<0>(scores.nearestAsked.at(0)), scores.places[mine]);
	EXPECT_EQ(std::get<2>(scores.nearestAsked.at(0)), 250.0f);
	ASSERT_EQ(hand.steps.size(), 4u);
	EXPECT_EQ(hand.steps.at(0).type, HandSubAction::MoveHandToPos);
	EXPECT_EQ(hand.steps.at(0).arguments.at(0).point, facts.places[mine]);
	EXPECT_EQ(hand.steps.at(1).type, HandSubAction::ForceCreatureToPerformAction);
	EXPECT_EQ(hand.steps.at(1).arguments.at(1).intValue, 82);
	EXPECT_EQ(hand.steps.at(1).arguments.at(2).thing, mine);
	EXPECT_EQ(hand.steps.at(2).type, HandSubAction::ForceCreatureToPerformAction);
	EXPECT_EQ(hand.steps.at(2).arguments.at(1).intValue, 20);
	EXPECT_EQ(hand.steps.at(2).arguments.at(2).thing, totem);
	EXPECT_EQ(hand.steps.at(3).type, HandSubAction::RaiseTotem);
	EXPECT_EQ(hand.steps.at(3).arguments.at(0).thing, totem);
	EXPECT_EQ(hand.steps.at(3).arguments.at(1).floatValue, 1.0f);

	// another ordinary action has no lesson: it is taught, the hand keeps its first two steps, and the action fails
	scores.normalLeft[mine] = 3u;
	runner.taught.clear();
	ComputerHand other;
	HandActions noLesson(other, runner, facts, &scores);
	EXPECT_FALSE(noLesson.RunAction(computer_player::ActionKind::TeachCreatureNormalAction, mind, node));
	EXPECT_EQ(runner.taught.size(), 1u);
	EXPECT_EQ(other.steps.size(), 2u);
	EXPECT_EQ(scores.nearestAsked.size(), 1u);

	// nothing left to learn: nothing
	scores.normalLeft.clear();
	ComputerHand none;
	HandActions nothing(none, runner, facts, &scores);
	EXPECT_FALSE(nothing.RunAction(computer_player::ActionKind::TeachCreatureNormalAction, mind, node));
	EXPECT_TRUE(none.steps.empty());
}

TEST(ComputerHandLeash, AJourneyBeyondAHundredMetresGoesAtHalfAgainTheRunElseAtTheMeanAfterADraw)
{
	EXPECT_EQ(JourneyDistance({0.0f, 0.0f, 0.0f}, {3.0f, 4.0f, 12.0f}), 13.0f);
	EXPECT_EQ(JourneyFraction(100.5f, 0.2f, 0.6f), 0.6f * 1.5f);
	EXPECT_EQ(JourneyFraction(100.0f, 0.2f, 0.6f), (0.6f + 0.2f) * 0.5f);

	const auto creature = static_cast<entt::entity>(40);
	FakeRunner runner;
	runner.bodies[creature] = {.point = {0.0f, 0.0f, 0.0f}, .walk = 0.2f, .run = 0.8f};
	HandStep step {.type = HandSubAction::SetCreatureSpeedForJourney,
	               .arguments = {{.kind = HandArgument::Kind::Thing, .thing = creature},
	                             {.kind = HandArgument::Kind::Point, .point = {0.0f, 0.0f, 150.0f}}}};
	// only the going-on stage does anything
	EXPECT_EQ(JourneySpeedStage(step, 0, runner), StageResult::Done);
	EXPECT_EQ(JourneySpeedStage(step, 2, runner), StageResult::Done);
	EXPECT_TRUE(runner.speeds.empty());
	// a long journey: half again the run, at most all of its top speed, no draw
	EXPECT_EQ(JourneySpeedStage(step, 1, runner), StageResult::Done);
	ASSERT_EQ(runner.speeds.size(), 1u);
	EXPECT_EQ(runner.speeds.at(0), std::make_tuple(creature, 1.0f));
	EXPECT_TRUE(runner.draws.empty());
	// a short one: a draw of up to 0.4 thrown away, then the mean of walk and run
	step.arguments.at(1).point = {0.0f, 0.0f, 50.0f};
	EXPECT_EQ(JourneySpeedStage(step, 1, runner), StageResult::Done);
	ASSERT_EQ(runner.draws.size(), 1u);
	EXPECT_EQ(runner.draws.at(0), 0.4f);
	EXPECT_EQ(runner.speeds.at(1), std::make_tuple(creature, (0.8f + 0.2f) * 0.5f));
	// no creature: failed
	step.arguments.at(0).thing = static_cast<entt::entity>(41);
	EXPECT_EQ(JourneySpeedStage(step, 1, runner), StageResult::Failed);
}

TEST(ComputerHandActions, ACreatureIsSentHomeAtItsJourneySpeed)
{
	const auto mine = static_cast<entt::entity>(40);
	FakeRunner runner;
	FakeFacts facts;
	facts.creatures.insert(mine);
	facts.places[mine] = {10.0f, 2.0f, 20.0f};
	test::FakeScoreWorld scores;
	const map_coords::MapCoords home {.x = 30 << 16, .z = 40 << 16};
	scores.homes[mine] = home;
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParentOfRow("SendCreatureHome: 64 ");
	mind.nodes.at(node).thing = mine;
	ComputerHand hand;
	HandActions actions(hand, runner, facts, &scores);
	EXPECT_TRUE(actions.HasAction(computer_player::ActionKind::SendCreatureHome));
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::SendCreatureHome, mind, node));
	ASSERT_EQ(hand.steps.size(), 3u);
	EXPECT_EQ(hand.steps.at(0).type, HandSubAction::SetCreatureSpeedForJourney);
	EXPECT_EQ(hand.steps.at(0).arguments.at(0).thing, mine);
	EXPECT_EQ(hand.steps.at(0).arguments.at(1).point, facts.LandPoint(home));
	EXPECT_EQ(hand.steps.at(1).type, HandSubAction::MoveHandToPos);
	EXPECT_EQ(hand.steps.at(1).arguments.at(0).point, facts.places[mine]);
	EXPECT_EQ(hand.steps.at(2).type, HandSubAction::ForceCreatureToPerformAction);
	EXPECT_EQ(hand.steps.at(2).arguments.at(1).intValue, 162);
	EXPECT_EQ(hand.steps.at(2).arguments.at(2).thing, mine);

	// no home: nothing
	scores.homes.clear();
	ComputerHand none;
	HandActions nothing(none, runner, facts, &scores);
	EXPECT_FALSE(nothing.RunAction(computer_player::ActionKind::SendCreatureHome, mind, node));
	EXPECT_TRUE(none.steps.empty());
}

TEST(ComputerHandActions, ACreatureHelpsATownOnTheKindLeashTiedToItsStoreAndDefendingItRunsTheSame)
{
	const auto mine = static_cast<entt::entity>(40);
	const auto building = static_cast<entt::entity>(42);
	const auto town = static_cast<entt::entity>(43);
	const auto pit = static_cast<entt::entity>(44);
	FakeRunner runner;
	FakeFacts facts;
	facts.creatures.insert(mine);
	facts.places[pit] = {5.0f, 1.0f, 6.0f};
	test::FakeScoreWorld scores;
	scores.places[building] = {};
	scores.townOf[building] = town;
	scores.towns.insert(town);
	scores.pits[town] = pit;
	scores.miracleKnowledge[{mine, CreatureDesires::Compassion}] = 0.0f;
	const std::array<std::pair<std::string_view, computer_player::ActionKind>, 2> rows {
	    std::pair {std::string_view {"GetCreaturetoHelp: 124 "}, computer_player::ActionKind::GetCreaturetoHelp},
	    std::pair {std::string_view {"DefendTownWithCreature: 155 "}, computer_player::ActionKind::DefendTownWithCreature}};
	for (const auto& [rowName, kind] : rows)
	{
		auto mind = computer_player::MakeMind();
		const auto [node, parent] = NodeAndParentOfRow(rowName);
		mind.nodes.at(node).thing = mine;
		mind.nodes.at(parent).thing = building;
		ComputerHand hand;
		HandActions actions(hand, runner, facts, &scores);
		EXPECT_TRUE(actions.HasAction(kind)) << rowName;
		EXPECT_TRUE(actions.RunAction(kind, mind, node)) << rowName;
		ASSERT_EQ(hand.steps.size(), 3u) << rowName;
		EXPECT_EQ(hand.steps.at(0).type, HandSubAction::SetCreatureSpeedForJourney);
		EXPECT_EQ(hand.steps.at(0).arguments.at(1).point, facts.places[pit]);
		EXPECT_EQ(hand.steps.at(1).type, HandSubAction::SetLeash);
		EXPECT_EQ(hand.steps.at(1).arguments.at(1).intValue, static_cast<int32_t>(LeashType::Good));
		EXPECT_EQ(hand.steps.at(2).type, HandSubAction::AttachLeashToObject);
		EXPECT_EQ(hand.steps.at(2).arguments.at(1).thing, pit);
		// knowing none of the compassionate miracles, a lesson of them is owed
		EXPECT_EQ(mind.helpLessons, 1u) << rowName;
	}
}

TEST(ComputerHandActions, ACreatureHelpsBuildThenWaitsAndHasItsCompassionMadeDominant)
{
	const auto mine = static_cast<entt::entity>(40);
	const auto site = static_cast<entt::entity>(45);
	const auto building = static_cast<entt::entity>(46);
	FakeRunner runner;
	FakeFacts facts;
	facts.creatures.insert(mine);
	facts.siteBuildings[site] = building;
	facts.places[building] = {7.0f, 1.0f, 8.0f};
	test::FakeScoreWorld scores;
	scores.buildingSites[site] = {};
	auto mind = computer_player::MakeMind();
	const auto [node, parent] = NodeAndParentOfRow("GetCreaturetoHelpBuild: 125 ");
	mind.nodes.at(node).thing = mine;
	mind.nodes.at(parent).thing = site;
	ComputerHand hand;
	HandActions actions(hand, runner, facts, &scores);
	EXPECT_TRUE(actions.HasAction(computer_player::ActionKind::GetCreaturetoHelpBuild));
	EXPECT_TRUE(actions.RunAction(computer_player::ActionKind::GetCreaturetoHelpBuild, mind, node));
	ASSERT_EQ(hand.steps.size(), 4u);
	EXPECT_EQ(hand.steps.at(0).type, HandSubAction::SetCreatureSpeedForJourney);
	EXPECT_EQ(hand.steps.at(0).arguments.at(1).point, facts.places[building]);
	EXPECT_EQ(hand.steps.at(1).type, HandSubAction::ForceCreatureToPerformAction);
	EXPECT_EQ(hand.steps.at(1).arguments.at(1).intValue, 34);
	EXPECT_EQ(hand.steps.at(1).arguments.at(2).thing, building);
	EXPECT_EQ(hand.steps.at(2).type, HandSubAction::Wait);
	EXPECT_EQ(hand.steps.at(2).arguments.at(0).floatValue, 0.5f);
	EXPECT_EQ(hand.steps.at(3).type, HandSubAction::ForceCreatureDominantDesire);
	EXPECT_EQ(hand.steps.at(3).arguments.at(1).intValue, static_cast<int32_t>(CreatureDesires::Compassion));
	EXPECT_EQ(hand.steps.at(3).arguments.at(2).floatValue, 120.0f);
	// it knows some compassionate miracle: no lesson owed
	EXPECT_EQ(mind.helpLessons, 0u);

	// a site with no building: nothing
	facts.siteBuildings.clear();
	ComputerHand none;
	HandActions nothing(none, runner, facts, &scores);
	EXPECT_FALSE(nothing.RunAction(computer_player::ActionKind::GetCreaturetoHelpBuild, mind, node));
	EXPECT_TRUE(none.steps.empty());
}

// Only the local player's hand plays the guidance's heartbeat and remarks: a computer god's hand taking a thing, going
// through its steps to the end of its line and letting go leaves the local guidance as it was
TEST(ComputerHandLocal, ItsTurnsLeaveTheLocalGuidanceAsItWas)
{
	namespace guidance = openblack::audio::guidance;
	guidance::ResetForTests();
	const guidance::State before = guidance::GetState();
	ComputerHand hand;
	FakeRunner runner;
	const auto rock = static_cast<entt::entity>(7);
	ASSERT_EQ(PickUpTurn(hand, rock, runner), StageResult::Done);
	Append(hand, {.type = HandSubAction::MoveHandToPos, .arguments = {PointAt(1200.0f, 30.0f, 900.0f)}});
	Append(hand, {.type = HandSubAction::DropObjectInHandAtPos, .arguments = {PointAt(1200.0f, 30.0f, 900.0f)}});
	for (int turn = 0; turn < 20 && !IsReady(hand); ++turn)
	{
		Step(hand, runner);
	}
	ASSERT_TRUE(IsReady(hand));
	const auto& after = guidance::GetState();
	EXPECT_EQ(after.lastPlayed, before.lastPlayed);
	EXPECT_EQ(after.lastDesireSample, before.lastDesireSample);
	EXPECT_EQ(after.lastBeliefSample, before.lastBeliefSample);
	EXPECT_EQ(after.heartBeatPlaying, before.heartBeatPlaying);
	EXPECT_EQ(after.heartBeatValue, before.heartBeatValue);
	EXPECT_EQ(after.heartBeatPitch, before.heartBeatPitch);
	EXPECT_EQ(after.heartBeatPulse, before.heartBeatPulse);
	EXPECT_EQ(after.heartBeatPulsePrevious, before.heartBeatPulsePrevious);
	EXPECT_EQ(after.heartBeatPhase, before.heartBeatPhase);
	EXPECT_EQ(after.believers, before.believers);
	EXPECT_EQ(after.beliefShare, before.beliefShare);
	EXPECT_EQ(after.oneOffs, before.oneOffs);
	EXPECT_EQ(after.moonCountdown, before.moonCountdown);
}
