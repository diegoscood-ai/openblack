/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The player's hand played as the game plays it, frame by frame and turn by turn, in a small world of the game's own
// systems and data (support/HandFrameHarness.h), with the things of the testbed scene objects.pickup_kinds. Two ways
// in: the packets the hand's own presses and releases send, applied at the turn's start (these need no animation and
// run on synthetic data too), and the hand's real frame with the mouse and the buttons (its states need its animations,
// so those run on the game's data only and skip their synthetic run, saying why). Every expected value is the original's
// (docs/wiki/hand/, docs/wiki/project/progress/hand/).

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <chrono>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include <entt/entity/entity.hpp>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/HandAnimator.h"
#include "3D/HandOrientation.h"
#include "3D/L3DMesh.h"
#include "3D/MapCoords.h"
#include "Audio/Audio.h"
#include "ECS/Archetypes/BigForestArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/OneOffSpellSeedArchetype.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectFlags.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/ObjectResources.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Trees.h"
#include "Hand/HandPlaceRules.h"
#include "Input/GamePackets.h"
#include "Magic/Core/Players.h"
#include "Magic/Core/SpellSeed.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Magic/HandHoldPose.h"
#include "Magic/MagicTables.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Worship/PlayerSpellIcons.h"
#include "support/CreatureFakes.h"
#include "support/HandFrameHarness.h"
#include "support/LandFakes.h"

using namespace openblack;
using ecs::components::MobileStatic;
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
/// The first handful from a pile: the hand pots' rows of info.dat
constexpr uint32_t k_FirstHandful = 25;
/// The Conifer's wood: its row of info.dat, what a big forest gives up for the tree it puts in the hand
constexpr uint32_t k_ConiferWood = 350;

class HandInputWorld: public test::world::HandFrameHarness
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
		HandFrameHarness::SetUp();
		if (IsSkipped() || HasFatalFailure())
		{
			return;
		}
		if (!GameData())
		{
			FillSyntheticRows();
		}
		magic::players::Reset();
		_ready = true;
	}

	void TearDown() override
	{
		if (_ready)
		{
			magic::players::Reset();
		}
		HandFrameHarness::TearDown();
	}

	/// The scene's rock where it stands, or moved along the land, at its scale times `scaled`
	[[nodiscard]] entt::entity Rock(glm::vec2 shift = glm::vec2(0.0f), float scaled = 1.0f) const
	{
		const auto type = RockType();
		// on synthetic data a box 2 m wide and 1.5 m tall, standing on its base
		LoadMesh(Info().mobileStatic.at(static_cast<size_t>(type)).meshId, glm::vec3(-1.0f, 0.0f, -1.0f),
		         glm::vec3(1.0f, 1.5f, 1.0f));
		return ecs::archetypes::MobileStaticArchetype::CreateFromInfo(Place(k_SceneRock, shift), type, 0.0f, 0.0f,
		                                                              SceneObject(k_SceneRock).scale * scaled);
	}

	/// The scene's birch, grown, rooted where it stands
	[[nodiscard]] entt::entity Birch() const
	{
		const auto type = BirchType();
		// on synthetic data a trunk 1 m across and 8 m tall
		LoadMesh(Info().tree.at(static_cast<size_t>(type)).normal, glm::vec3(-0.5f, 0.0f, -0.5f), glm::vec3(0.5f, 8.0f, 0.5f));
		const float scale = SceneObject(k_SceneBirch).scale;
		return ecs::archetypes::TreeArchetype::Create(0, Place(k_SceneBirch), type, false, 0.0f, scale, scale);
	}

	/// A pile of `type` holding `amount`, on the land in front of the scene's rock
	[[nodiscard]] entt::entity Pile(PotInfo type, int32_t amount, bool poisoned = false) const
	{
		for (const auto row : {type, PotInfo::HandFood, PotInfo::HandWood})
		{
			LoadMesh(Info().pot.at(static_cast<size_t>(row)).meshId, glm::vec3(-1.0f, 0.0f, -1.0f),
			         glm::vec3(1.0f, 0.5f, 1.0f));
		}
		const auto pile = ecs::archetypes::PotArchetype::Create(Place(k_SceneRock, glm::vec2(0.0f, -8.0f)), 0.0f, type, amount);
		Registry().Get<Pot>(pile).poisoned = poisoned;
		return pile;
	}

	/// The turn's packets sent, then the next turn played: it applies them first thing
	void PlayTurn()
	{
		game_packets::Flush();
		Turn();
	}

	/// The hand's press on a thing: its place-in-hand packet
	void PickUp(entt::entity thing)
	{
		game_packets::Push({game_packets::Type::PlaceInHand, thing});
		PlayTurn();
	}

	/// The hand's press on a pile: the synced hand over it, then the locked select's start
	void Scoop(entt::entity pile)
	{
		game_packets::Packet hand {game_packets::Type::Hand};
		const auto at = Registry().Get<const Transform>(pile).position;
		hand.coords = map_coords::FromWorld(at);
		hand.data = {at.x, at.y, at.z};
		game_packets::Push(hand);
		game_packets::Push({game_packets::Type::StartLockedSelect, pile});
		PlayTurn();
	}

	/// The hand's release of the pile it scoops from: the locked select's end
	void EndScoop(entt::entity pile)
	{
		game_packets::Push({game_packets::Type::EndLockedSelect, pile});
		PlayTurn();
	}

	/// The hand's release: the throw data (the velocity, no spin, where the hand holds the thing and its angles), then
	/// the apply to the map point under it
	void LetGo(glm::vec3 velocity, glm::vec3 from, glm::vec3 angles)
	{
		game_packets::Packet data {game_packets::Type::ThrowData};
		data.data = {velocity.x, velocity.y, velocity.z, 0.0f,     0.0f,     0.0f,
		             from.x,     from.y,     from.z,     angles.x, angles.y, angles.z};
		game_packets::Push(data);
		game_packets::Push({game_packets::Type::ApplyToMapCoord, entt::null, glm::vec3(from.x, k_Ground, from.z)});
		PlayTurn();
	}

	/// The synced hand moved to a point
	void HandAt(glm::vec3 at)
	{
		game_packets::Packet hand {game_packets::Type::Hand};
		hand.coords = map_coords::FromWorld(at);
		hand.data = {at.x, at.y, at.z};
		game_packets::Push(hand);
		PlayTurn();
	}

	/// The camera 24 m south of a point and 14 m above it, looking at it, the cursor in the window's middle: the mouse
	/// ray goes through the point
	void Aim(glm::vec3 at)
	{
		LookFrom(at + glm::vec3(0.0f, 14.0f, -24.0f), at);
		SetMouse(k_Screen / 2);
	}
	/// Frames until the hand holds the thing, at most `frames`; whether it does
	bool FramesUntilHeld(entt::entity thing, int frames)
	{
		return FramesUntil([this, thing]() { return Hand().GetHeldObject() == std::optional(thing); }, frames);
	}
	/// The hand's place and its up: its matrix's translation, and its third axis turned over (the hand's matrix is the
	/// side, the side across the up, and the up turned down)
	[[nodiscard]] glm::vec3 HandPoint() const { return glm::vec3(Hand().GetHandMatrix()[3]); }
	[[nodiscard]] glm::vec3 HandUp() const { return -glm::normalize(glm::vec3(Hand().GetHandMatrix()[2])); }
	/// The hand's animations loaded as the hand loads them (hh.HBN laid out by the game's hand spec), for what a clip
	/// is: its length and its frames
	[[nodiscard]] static std::unique_ptr<HandAnimator> Animations()
	{
		auto& files = Locator::filesystem::value();
		auto& cache = Locator::resources::value();
		const auto& mesh = cache.GetMeshes().Handle(ecs::components::Hand::k_MeshId);
		auto animator = std::make_unique<HandAnimator>();
		// the spec file read through the file cache, as the hand reads it
		const auto readSpec = [&files, &cache](const std::string& specName) -> std::optional<std::vector<uint8_t>> {
			const auto path = files.GetPath<filesystem::Path::Data>() / specName;
			if (!files.Exists(path))
			{
				return std::nullopt;
			}
			return resources::LoadBlob(cache.GetBlobs(), path);
		};
		if (!mesh ||
		    !animator->Load(resources::LoadBlob(cache.GetBlobs(), files.GetPath<filesystem::Path::Data>() / "CTR" / "hh.HBN"),
		                    readSpec, mesh->GetBoneLocals(), mesh->GetBoneParents()))
		{
			return nullptr;
		}
		return animator;
	}

	/// Through the hand's frame: the scene's rock under the cursor in the player's influence, put in the hand (its
	/// place-in-hand packet) and held still, then the action pressed again and the mouse swept east, three frames; the
	/// rock's drawn pose before the release, which is the next frame
	ecs::components::HandDrawPose HoldAndSweep(entt::entity rock)
	{
		const auto at = Registry().Get<const Transform>(rock).position;
		static_cast<void>(InfluenceOver(at, 500.0f));
		Aim(at + glm::vec3(0.0f, 1.0f, 0.0f));
		Frames(4);
		PickUp(rock);
		Frames(8);
		EXPECT_EQ(Hand().GetHeldObject(), std::optional(rock));
		PressAction();
		Frame();
		for (int frame = 1; frame <= 3; ++frame)
		{
			SetMouse(k_Screen / 2 + glm::ivec2(60 * frame, 0));
			Frame();
		}
		const auto* pose = Registry().TryGet<const ecs::components::HandDrawPose>(rock);
		EXPECT_NE(pose, nullptr);
		return pose != nullptr ? *pose : ecs::components::HandDrawPose {};
	}

	/// What the cursor finds, for a failure's message: the entity, its model and place, and the kinds it is
	[[nodiscard]] std::string WhatIsUnderTheCursor() const
	{
		const auto thing = Hand().GetCursorObject();
		if (!thing.has_value())
		{
			return "nothing under the cursor, the hand's land " +
			       (LastFrame().handLand
			            ? fmt::format("({}, {}, {})", LastFrame().handLand->x, LastFrame().handLand->y, LastFrame().handLand->z)
			            : std::string("none"));
		}
		const auto& registry = Registry();
		std::string text = fmt::format("entity {}", static_cast<uint32_t>(*thing));
		if (const auto* mesh = registry.TryGet<const ecs::components::Mesh>(*thing); mesh != nullptr)
		{
			text += fmt::format(", mesh {}", mesh->id);
		}
		if (const auto* transform = registry.TryGet<const Transform>(*thing); transform != nullptr)
		{
			text += fmt::format(", at ({}, {}, {}) scale {}", transform->position.x, transform->position.y,
			                    transform->position.z, transform->scale.x);
		}
		text += registry.AllOf<ecs::components::OneOffSpellSeed>(*thing) ? ", a one-shot orb" : "";
		text += registry.AllOf<ecs::components::SpellSeedGraphic>(*thing) ? ", a seed graphic" : "";
		text += registry.AllOf<Pot>(*thing) ? ", a pot" : "";
		return text;
	}

	/// The middle of a thing's model's box, where it stands
	[[nodiscard]] static glm::vec3 MiddleOf(entt::entity thing)
	{
		const auto& transform = Registry().Get<const Transform>(thing);
		const auto& mesh = Registry().Get<const ecs::components::Mesh>(thing);
		const auto model = Locator::resources::value().GetMeshes().Handle(mesh.id);
		if (!model)
		{
			return transform.position;
		}
		return transform.position + transform.rotation * (transform.scale * model->GetBoundingBox().Center());
	}

	/// A thing's model and place, for a failure's message
	[[nodiscard]] static std::string Describe(entt::entity thing)
	{
		const auto& registry = Registry();
		std::string text = fmt::format("entity {}", static_cast<uint32_t>(thing));
		if (const auto* transform = registry.TryGet<const Transform>(thing); transform != nullptr)
		{
			text += fmt::format(", at ({}, {}, {}) scale {}", transform->position.x, transform->position.y,
			                    transform->position.z, transform->scale.x);
		}
		if (const auto* mesh = registry.TryGet<const ecs::components::Mesh>(thing); mesh != nullptr)
		{
			const auto model = Locator::resources::value().GetMeshes().Handle(mesh->id);
			text += fmt::format(", mesh {} {}", mesh->id, model ? "loaded" : "not loaded");
			if (model)
			{
				const auto box = model->GetBoundingBox();
				text += fmt::format(", box ({}, {}, {})..({}, {}, {})", box.minima.x, box.minima.y, box.minima.z, box.maxima.x,
				                    box.maxima.y, box.maxima.z);
			}
		}
		if (const auto* sink = registry.TryGet<const ecs::components::PileSink>(thing); sink != nullptr)
		{
			text += fmt::format(", sunk {} of {} (base {})", sink->offset.value, sink->height, sink->baseY);
		}
		if (const auto* pot = registry.TryGet<const Pot>(thing); pot != nullptr)
		{
			text += fmt::format(", pot of {} (max {})", pot->amount, pot->maxAmount);
		}
		text += ecs::IsAvailable(thing) ? "" : ", unavailable";
		return text;
	}

	/// Every rock of the scene's kind but this one, in the order they were made
	[[nodiscard]] static std::vector<entt::entity> OtherRocks(entt::entity rock)
	{
		std::vector<entt::entity> found;
		Registry().Each<const MobileStatic>([&](entt::entity thing, const MobileStatic& statics) {
			if (thing != rock && statics.type == RockType())
			{
				found.push_back(thing);
			}
		});
		std::ranges::sort(found);
		return found;
	}

private:
	/// The rows the synthetic world's things read, as the game's tables have them: a rock of the rock kind, a birch, the
	/// Conifer a big forest gives, a big forest, piles of food and of wood and the hand's handfuls of each (the first 25,
	/// then 8 a turn rising to 70 over 6 s, at most 20000), a store's food pile and a seed's power-up levels
	void FillSyntheticRows() const
	{
		auto& rock = Info().mobileStatic.at(static_cast<size_t>(RockType()));
		rock.mobileType = static_cast<MobileStaticInfo>(2);
		rock.meshId = MeshId::ObjectRockChalk;
		rock.weight = 400.0f;
		Info().tree.at(static_cast<size_t>(BirchType())).normal = MeshId::TreeBirch;
		auto& conifer = Info().tree.at(static_cast<size_t>(TreeInfo::Conifer));
		conifer.normal = MeshId::TreeConifer;
		conifer.woodValue = k_ConiferWood;
		auto& forest = Info().bigForest.at(static_cast<size_t>(BigForestInfo::Info1));
		forest.meshId = MeshId::FeatureForest;
		forest.woodValue = 1000;
		const auto pile = [this](PotInfo type, MeshId mesh, PotType kind, ResourceType resource) -> GPotInfo& {
			auto& row = Info().pot.at(static_cast<size_t>(type));
			row.meshId = mesh;
			row.potType = kind;
			row.resourceType = resource;
			row.maxAmountInPot = 20000;
			return row;
		};
		pile(PotInfo::FoodPile, MeshId::I_Food, PotType::PileFood, ResourceType::Food);
		pile(PotInfo::WoodPile_1, MeshId::FeatureForest01, PotType::PileWood, ResourceType::Wood);
		pile(PotInfo::StoragePitFoodPile, MeshId::FeatureForest02, PotType::PileFood, ResourceType::Food);
		for (const auto& [type, mesh, kind, resource] :
		     {std::tuple {PotInfo::HandFood, MeshId::I_OfferingFood, PotType::PileFood, ResourceType::Food},
		      std::tuple {PotInfo::HandWood, MeshId::I_OfferingWood, PotType::PileWood, ResourceType::Wood}})
		{
			auto& handful = pile(type, mesh, kind, resource);
			handful.canBecomeAPhysicsObject = 1;
			handful.amountPickedUpInitially = k_FirstHandful;
			handful.amountPickedUpPerTurn = 8;
			handful.amountPickedUpPerTurnEnd = 70;
			handful.maxAmountCanBePickedUp = 20000;
			handful.multiPickUpRampTime = 6.0f;
		}
		Info().spellSeed.at(static_cast<size_t>(SpellSeedType::Fire)).magicTypes.fill(MagicType::Fireball);
	}

	bool _ready {false};
};
} // namespace

// ---- picking_up.md -------------------------------------------------------------------------------------------------

// A big forest put in the hand gives up a Conifer's wood and puts in the hand a new Conifer of the forest's forest, at
// its full size; the forest stays with what is left
TEST_P(HandInputWorld, ABigForestPutInTheHandGivesAConiferMadeWhereItStandsAndItsWood)
{
	EXPECT_EQ(Info().tree.at(static_cast<size_t>(TreeInfo::Conifer)).woodValue, k_ConiferWood);
	const auto& forestRow = Info().bigForest.at(static_cast<size_t>(BigForestInfo::Info1));
	LoadMesh(forestRow.meshId, glm::vec3(-10.0f, 0.0f, -10.0f), glm::vec3(10.0f, 10.0f, 10.0f));
	LoadMesh(Info().tree.at(static_cast<size_t>(TreeInfo::Conifer)).normal, glm::vec3(-0.5f, 0.0f, -0.5f),
	         glm::vec3(0.5f, 8.0f, 0.5f));
	ASSERT_GT(forestRow.woodValue, 50u);
	// more wood than its size shows, by less than the 250 past which the taking would replant: no sapling is drawn
	const float wood = static_cast<float>(forestRow.woodValue) + 300.0f;
	const auto at = Place(k_SceneRock, glm::vec2(0.0f, -40.0f));
	const auto forestId = ecs::CreateForest(0, at);
	const auto forest = ecs::archetypes::BigForestArchetype::Create(at, BigForestInfo::Info1, 0.0f, 1.0f, forestId, wood);
	PickUp(forest);
	const auto held = Hand().GetHeldObject();
	ASSERT_TRUE(held.has_value());
	ASSERT_NE(*held, forest);
	const auto* tree = Registry().TryGet<const ecs::components::Tree>(*held);
	ASSERT_NE(tree, nullptr);
	EXPECT_EQ(tree->type, TreeInfo::Conifer);
	EXPECT_EQ(tree->maxSize, 1.0f);
	EXPECT_EQ(tree->forestId, forestId);
	// where it was made is not seen: in the same turn the hand's pass puts what it holds at the synced hand
	EXPECT_EQ(Registry().Get<const Transform>(*held).scale, glm::vec3(1.0f));
	ASSERT_TRUE(Registry().Valid(forest));
	EXPECT_EQ(Registry().Get<const ecs::components::BigForest>(forest).wood, wood - static_cast<float>(k_ConiferWood));
}

// A big forest with less wood than a Conifer's gives what it has and goes, and the hand still takes the new tree
TEST_P(HandInputWorld, ABigForestWithLessWoodThanATreeGoesAndStillGivesTheTree)
{
	const auto& forestRow = Info().bigForest.at(static_cast<size_t>(BigForestInfo::Info1));
	LoadMesh(forestRow.meshId, glm::vec3(-10.0f, 0.0f, -10.0f), glm::vec3(10.0f, 10.0f, 10.0f));
	LoadMesh(Info().tree.at(static_cast<size_t>(TreeInfo::Conifer)).normal, glm::vec3(-0.5f, 0.0f, -0.5f),
	         glm::vec3(0.5f, 8.0f, 0.5f));
	const auto at = Place(k_SceneRock, glm::vec2(0.0f, -40.0f));
	const auto forestId = ecs::CreateForest(0, at);
	const auto forest = ecs::archetypes::BigForestArchetype::Create(at, BigForestInfo::Info1, 0.0f, 1.0f, forestId, 100.0f);
	PickUp(forest);
	EXPECT_FALSE(Registry().Valid(forest));
	EXPECT_TRUE(ecs::ForestBigForest(forestId) == entt::null);
	const auto held = Hand().GetHeldObject();
	ASSERT_TRUE(held.has_value());
	const auto* tree = Registry().TryGet<const ecs::components::Tree>(*held);
	ASSERT_NE(tree, nullptr);
	EXPECT_EQ(tree->type, TreeInfo::Conifer);
}

// A field the scooping refuses (grown, with a unit of food or less) put in the hand is not held itself: a new handful of
// the hand's food of the field's hand amount (25 in info.dat) goes into the hand, and the field is asked for as much, so
// a ripe one is left bare and unsown
TEST_P(HandInputWorld, AFieldPutInTheHandGivesAHandfulOfItsHandAmountAndLosesIt)
{
	auto& row = Info().fieldType.at(static_cast<size_t>(FieldTypeInfo::Wheat));
	if (!GameData())
	{
		row.ageRecolt = 1200.0f;
		row.timesToSow = 30.0f;
		row.foodValueTakenWithHand = 25.0f;
		row.ratioBeforeRipe = 0.2f;
	}
	EXPECT_EQ(row.foodValueTakenWithHand, 25.0f);
	LoadMesh(Info().pot.at(static_cast<size_t>(PotInfo::HandFood)).meshId, glm::vec3(-1.0f, 0.0f, -1.0f),
	         glm::vec3(1.0f, 0.5f, 1.0f));
	auto& registry = Registry();
	const auto field = registry.Create();
	ecs::object_index::Assign(field);
	registry.Assign<Transform>(field, Place(k_SceneRock, glm::vec2(0.0f, -20.0f)), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& data = registry.Assign<ecs::components::Field>(field, -1);
	data.crops = 30;
	data.growth = row.ageRecolt;
	data.food = 1.0f;
	PickUp(field);
	const auto held = Hand().GetHeldObject();
	ASSERT_TRUE(held.has_value());
	ASSERT_NE(*held, field);
	const auto* pot = registry.TryGet<const Pot>(*held);
	ASSERT_NE(pot, nullptr);
	EXPECT_EQ(pot->type, PotInfo::HandFood);
	EXPECT_EQ(pot->amount, 25u);
	ASSERT_TRUE(registry.Valid(field));
	const auto& left = registry.Get<const ecs::components::Field>(field);
	EXPECT_EQ(left.food, 0.0f);
	EXPECT_EQ(left.crops, 0);
	EXPECT_EQ(left.growth, 0.0f);
}

// A fish farm put in the hand is not held itself: a new handful of the hand's food of that food's food value (20 in
// info.dat) goes into the hand, and the farm keeps all its fish
TEST_P(HandInputWorld, AFishFarmPutInTheHandGivesAHandfulOfTheHandFoodsFoodValueAndKeepsItsFish)
{
	auto& handFood = Info().pot.at(static_cast<size_t>(PotInfo::HandFood));
	if (!GameData())
	{
		handFood.foodValue = 20.0f;
	}
	EXPECT_EQ(handFood.foodValue, 20.0f);
	LoadMesh(handFood.meshId, glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(1.0f, 0.5f, 1.0f));
	auto& registry = Registry();
	const auto farm = registry.Create();
	ecs::object_index::Assign(farm);
	registry.Assign<Transform>(farm, Place(k_SceneRock, glm::vec2(0.0f, -20.0f)), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<ecs::components::FishFarm>(farm).food = 300.0f;
	PickUp(farm);
	const auto held = Hand().GetHeldObject();
	ASSERT_TRUE(held.has_value());
	ASSERT_NE(*held, farm);
	const auto* pot = registry.TryGet<const Pot>(*held);
	ASSERT_NE(pot, nullptr);
	EXPECT_EQ(pot->type, PotInfo::HandFood);
	EXPECT_EQ(pot->amount, 20u);
	EXPECT_EQ(registry.Get<const ecs::components::FishFarm>(farm).food, 300.0f);
}

// A one-shot orb put in the hand is held itself, and the miracle of its seed at its power-up level counts as one the
// player has had
TEST_P(HandInputWorld, AOneShotOrbPutInTheHandIsHeldItselfAndItsMiracleCountsAsHad)
{
	const auto orb = ecs::archetypes::OneOffSpellSeedArchetype::Create(Place(k_SceneRock, glm::vec2(12.0f, 0.0f)),
	                                                                   SpellSeedType::Fire, 0, 1.0f);
	ASSERT_TRUE(orb != entt::null);
	const auto type = magic::MagicTypeForPowerUpLevel(magic::GetSpellSeedInfo(Info(), SpellSeedType::Fire), 0);
	ASSERT_NE(type, MagicType::None);
	EXPECT_FALSE(magic::players::HasMagicTypeEverBeenEnabled(PlayerNames::PLAYER_ONE, type));
	PickUp(orb);
	EXPECT_EQ(Hand().GetHeldObject(), std::optional(orb));
	EXPECT_TRUE(Hand().IsInMagicHand(orb));
	EXPECT_TRUE(magic::players::HasMagicTypeEverBeenEnabled(PlayerNames::PLAYER_ONE, type));
}

// ---- multi_pickup.md ------------------------------------------------------------------------------------------------

// Once the scoop ends the hand keeps its handful, takes no more from the pile, and lets it go as anything else: thrown,
// it counts as a throw
TEST_P(HandInputWorld, AScoopedHandfulIsHeldAsAnythingElseOnceTheScoopEnds)
{
	const auto pile = Pile(PotInfo::FoodPile, 1000);
	// the player's influence over the pile and the hand, which the scoop needs every turn
	static_cast<void>(InfluenceOver(Registry().Get<const Transform>(pile).position, 3000.0f));
	Scoop(pile);
	const auto handful = Hand().GetHeldObject();
	ASSERT_TRUE(handful.has_value());
	ASSERT_NE(*handful, pile);
	EndScoop(pile);
	EXPECT_EQ(Hand().GetHeldObject(), handful);
	EXPECT_FALSE(Hand().HoldsLockedSelect());
	EXPECT_EQ(Hand().GetLockedSelectObject(), std::nullopt);
	const auto held = Registry().Get<const Pot>(*handful).amount;
	const auto left = Registry().Get<const Pot>(pile).amount;
	PlayTurn();
	EXPECT_EQ(Registry().Get<const Pot>(*handful).amount, held);
	EXPECT_EQ(Registry().Get<const Pot>(pile).amount, left);
	LetGo(k_Throw, Place(k_SceneRock) + glm::vec3(0.0f, 6.0f, 0.0f), k_HandAngles);
	EXPECT_FALSE(Hand().GetHeldObject().has_value());
	EXPECT_EQ(help_profile::Get(HelpEvent::Throw).Total(), 1u);
}

// ---- holding.md -----------------------------------------------------------------------------------------------------

// A handful is made at its own size and keeps it while the scoop fills it: what grows is what it holds
TEST_P(HandInputWorld, AHeldHandfulKeepsItsOwnSizeAsItFills)
{
	const auto pile = Pile(PotInfo::FoodPile, 1000);
	static_cast<void>(InfluenceOver(Registry().Get<const Transform>(pile).position, 3000.0f));
	Scoop(pile);
	const auto handful = Hand().GetHeldObject();
	ASSERT_TRUE(handful.has_value());
	ASSERT_NE(*handful, pile);
	const auto first = Registry().Get<const Pot>(*handful).amount;
	EXPECT_EQ(Registry().Get<const Transform>(*handful).scale, glm::vec3(1.0f));
	for (int turn = 0; turn < 5; ++turn)
	{
		PlayTurn();
		ASSERT_EQ(Hand().GetHeldObject(), handful);
		EXPECT_EQ(Registry().Get<const Transform>(*handful).scale, glm::vec3(1.0f));
	}
	EXPECT_GT(Registry().Get<const Pot>(*handful).amount, first);
}

// A handful of food and one of wood are each held in their own in-hand model: the hand pot's row's
TEST_P(HandInputWorld, AHandfulOfFoodOrOfWoodIsHeldInItsOwnInHandModel)
{
	if (GameData())
	{
		EXPECT_EQ(Info().pot.at(static_cast<size_t>(PotInfo::HandWood)).meshId, MeshId::I_OfferingWood);
	}
	ASSERT_EQ(Info().pot.at(static_cast<size_t>(PotInfo::WoodPile_1)).resourceType, ResourceType::Wood);
	for (const auto& [source, hand] :
	     {std::pair {PotInfo::FoodPile, PotInfo::HandFood}, std::pair {PotInfo::WoodPile_1, PotInfo::HandWood}})
	{
		const auto pile = Pile(source, 1000);
		Scoop(pile);
		const auto handful = Hand().GetHeldObject();
		ASSERT_TRUE(handful.has_value());
		ASSERT_NE(*handful, pile);
		EXPECT_EQ(Registry().Get<const Pot>(*handful).type, hand);
		EXPECT_EQ(Registry().Get<const ecs::components::Mesh>(*handful).id,
		          resources::HashIdentifier(Info().pot.at(static_cast<size_t>(hand)).meshId));
		EndScoop(pile);
		LetGo(k_Throw, Place(k_SceneRock) + glm::vec3(0.0f, 6.0f, 0.0f), k_HandAngles);
		ASSERT_FALSE(Hand().GetHeldObject().has_value());
	}
}

// A held thing that is deleted leaves the hand at the next turn, which is empty, held and drawn
TEST_P(HandInputWorld, AHeldThingThatIsDeletedLeavesTheHandWhichEmpties)
{
	const auto rock = Rock();
	PickUp(rock);
	ASSERT_EQ(Hand().GetHeldObject(), std::optional(rock));
	ecs::ToBeDeleted(rock);
	PlayTurn();
	EXPECT_FALSE(Hand().GetHeldObject().has_value());
	EXPECT_FALSE(Hand().GetRenderHandObject().has_value());
	EXPECT_EQ(Dynamics().Find(rock), nullptr);
}

// A held miracle seed that its spell's end deletes leaves the hand at the next turn, which empties; its type becomes the
// last seed type, the interface's and its player's, as for any seed that leaves the hand
TEST_P(HandInputWorld, AHeldMiracleSeedThatIsDeletedLeavesTheHand)
{
	const auto seed = Registry().Create();
	Registry().Assign<Transform>(seed, Place(k_SceneRock, glm::vec2(12.0f, 0.0f)), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& component = Registry().Assign<ecs::components::SpellSeed>(seed);
	component.seedType = SpellSeedType::Fire;
	component.creator.player = PlayerNames::PLAYER_ONE;
	component.ready = true;
	Hand().PlaceObjectInMagicHand(seed);
	ASSERT_EQ(Hand().GetHeldObject(), std::optional(seed));
	magic::gestures::State().lastSeedType = static_cast<int>(SpellSeedType::Water);
	worship::player::SetLastSeedType(PlayerNames::PLAYER_ONE, SpellSeedType::Water);
	// what a spell's deletion does to its seed
	magic::seed::ToBeDeleted(seed);
	PlayTurn();
	EXPECT_FALSE(Hand().GetHeldObject().has_value());
	EXPECT_FALSE(Hand().GetRenderHandObject().has_value());
	EXPECT_EQ(magic::gestures::State().lastSeedType, static_cast<int>(SpellSeedType::Fire));
	EXPECT_EQ(worship::player::LastSeedType(PlayerNames::PLAYER_ONE), SpellSeedType::Fire);
}

// The same seed destroyed between two turns: the next frame's hand finds it gone before any turn's check, and its type
// still becomes the last seed type, the interface's and its player's. The hand's frame runs only with its animations
TEST_P(HandInputWorld, AHeldMiracleSeedDestroyedBetweenTurnsStillLeavesItsTypeAtTheNextFrame)
{
	if (!GameData())
	{
		GTEST_SKIP() << k_NeedsAnimations;
	}
	ASSERT_TRUE(HasAnimator());
	const auto seed = Registry().Create();
	Registry().Assign<Transform>(seed, Place(k_SceneRock, glm::vec2(12.0f, 0.0f)), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& component = Registry().Assign<ecs::components::SpellSeed>(seed);
	component.seedType = SpellSeedType::LightningBolt;
	component.creator.player = PlayerNames::PLAYER_ONE;
	component.ready = true;
	Hand().PlaceObjectInMagicHand(seed);
	ASSERT_EQ(Hand().GetHeldObject(), std::optional(seed));
	magic::gestures::State().lastSeedType = static_cast<int>(SpellSeedType::Water);
	worship::player::SetLastSeedType(PlayerNames::PLAYER_ONE, SpellSeedType::Water);
	// frames until a turn has just been played, so that the next frame plays none
	const auto before = game_clock::Turn();
	ASSERT_TRUE(FramesUntil([before] { return game_clock::Turn() != before; }, 50));
	const auto turn = game_clock::Turn();
	// what a spell's deletion does to its seed, then one frame with no turn
	magic::seed::ToBeDeleted(seed);
	Frame();
	ASSERT_EQ(game_clock::Turn(), turn);
	EXPECT_FALSE(Hand().GetHeldObject().has_value());
	EXPECT_EQ(magic::gestures::State().lastSeedType, static_cast<int>(SpellSeedType::LightningBolt));
	EXPECT_EQ(worship::player::LastSeedType(PlayerNames::PLAYER_ONE), SpellSeedType::LightningBolt);
}

// ---- hand_physics.md ------------------------------------------------------------------------------------------------

// The velocity a miracle cast from the hand leaves with is the hand's turn velocity: each turn it goes 0.6 of the way
// from what it was to the turn's move of the synced hand times the turns in a second
TEST_P(HandInputWorld, AMiracleCastFromTheHandLeavesWithTheHandsTurnVelocity)
{
	const float perSecond = 1000.0f / static_cast<float>(game_clock::MsPerTurn());
	const std::array<glm::vec3, 4> hands {glm::vec3(1000.0f, 20.0f, 1000.0f), glm::vec3(1003.0f, 20.0f, 1000.0f),
	                                      glm::vec3(1003.0f, 22.0f, 996.0f), glm::vec3(1010.0f, 21.0f, 990.0f)};
	glm::vec3 interfacePoint {0.0f};
	glm::vec3 handPoint {0.0f};
	glm::vec3 forward {0.0f};
	glm::vec3 velocity {0.0f};
	HandAt(hands[0]);
	Hand().GetSpellInfo(interfacePoint, handPoint, forward, velocity);
	ASSERT_EQ(handPoint, hands[0]);
	for (size_t turn = 1; turn < hands.size(); ++turn)
	{
		const glm::vec3 before = velocity;
		HandAt(hands[turn]);
		Hand().GetSpellInfo(interfacePoint, handPoint, forward, velocity);
		EXPECT_EQ(handPoint, hands[turn]);
		const glm::vec3 expected = before + 0.6f * (perSecond * (hands[turn] - hands[turn - 1]) - before);
		EXPECT_FLOAT_EQ(velocity.x, expected.x);
		EXPECT_FLOAT_EQ(velocity.y, expected.y);
		EXPECT_FLOAT_EQ(velocity.z, expected.z);
	}
}

// ---- throwing.md ----------------------------------------------------------------------------------------------------

// A handful of food pressed onto a worship site goes into the site's food and leaves the hand. A handful of wood is not
// taken by a site with no building going up: it stays in the hand. A press the site takes counts a supply, one it
// can't take counts none
TEST_P(HandInputWorld, FoodPressedOntoAWorshipSiteSuppliesItButWoodIsNotTaken)
{
	const auto site = Registry().Create();
	Registry().Assign<Transform>(site, Place(k_SceneRock, glm::vec2(0.0f, 30.0f)), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& worshipSite = Registry().Assign<ecs::components::WorshipSite>(site);
	worshipSite.player = PlayerNames::PLAYER_ONE;
	worshipSite.foodPot = Pile(PotInfo::StoragePitFoodPile, 40);
	ASSERT_EQ(ecs::object_resources::GetResource(site, ResourceType::Food), 40u);
	const auto food = Pile(PotInfo::FoodPile, 1000);
	Scoop(food);
	const auto handful = Hand().GetHeldObject();
	ASSERT_TRUE(handful.has_value());
	ASSERT_NE(*handful, food);
	const auto amount = Registry().Get<const Pot>(*handful).amount;
	game_packets::Push({game_packets::Type::ApplyToObject, site});
	PlayTurn();
	EXPECT_EQ(ecs::object_resources::GetResource(site, ResourceType::Food), 40u + amount);
	EXPECT_FALSE(Hand().GetHeldObject().has_value());
	EXPECT_FALSE(ecs::IsAvailable(*handful));
	// the apply to an object counts one supply for the local interface, whatever it does
	EXPECT_EQ(help_profile::Get(HelpEvent::Supply).Total(), 1u);
	// wood
	const auto wood = Pile(PotInfo::WoodPile_1, 1000);
	Scoop(wood);
	const auto woodHandful = Hand().GetHeldObject();
	ASSERT_TRUE(woodHandful.has_value());
	game_packets::Push({game_packets::Type::ApplyToObject, site});
	PlayTurn();
	EXPECT_EQ(Hand().GetHeldObject(), woodHandful);
	EXPECT_EQ(ecs::object_resources::GetResource(site, ResourceType::Food), 40u + amount);
	// an apply the held thing is not valid for does nothing, and counts nothing
	EXPECT_EQ(help_profile::Get(HelpEvent::Supply).Total(), 1u);
}

// Whatever the hand throws, small or as big as it can lift, rock or tree, flies as the player's throw from the hand
TEST_P(HandInputWorld, EveryThrowFromTheHandIsThePlayersWhateverItsSize)
{
	const auto check = [this](entt::entity thing, glm::vec3 from) {
		PickUp(thing);
		ASSERT_EQ(Hand().GetHeldObject(), std::optional(thing));
		LetGo(k_Throw, from, k_HandAngles);
		ASSERT_FALSE(Hand().GetHeldObject().has_value());
		const auto* entry = Dynamics().Find(thing);
		ASSERT_NE(entry, nullptr);
		EXPECT_EQ(entry->throwerPlayer, std::optional(PlayerNames::PLAYER_ONE));
		EXPECT_NE(entry->flags & ecs::PhysicsEntry::k_FromHand, 0u);
	};
	check(Rock(), Place(k_SceneRock) + glm::vec3(0.0f, 6.0f, 0.0f));
	// a rock as big as the hand can lift: 3.5 across, the hand lifting no rock wider than 3.6
	const auto probe = Rock(glm::vec2(40.0f, 0.0f));
	const float big = 3.5f / ecs::object_physics::RockRadius2D(probe);
	check(Rock(glm::vec2(20.0f, 0.0f), big), Place(k_SceneRock, glm::vec2(20.0f, 0.0f)) + glm::vec3(0.0f, 12.0f, 0.0f));
	check(Birch(), Place(k_SceneBirch) + glm::vec3(0.0f, 6.0f, 0.0f));
}

// ---- clicking_and_activating.md -------------------------------------------------------------------------------------

// A tap the hand sent is applied at the next turn's start: the rock, still tappable (taller than 0.7), splits in two
// halves of its kind at 0.7935 of its size, and cracks with the next rock tap sample at the hand's synced place; the tap
// counts in the help profile
TEST_P(HandInputWorld, ATapIsAppliedAtTheNextTurnAtTheHandsSyncedPlace)
{
	const auto rock = Rock();
	ASSERT_GT(ecs::object::GetHeight(rock), 0.7f);
	const float scale = Registry().Get<const Transform>(rock).scale.x;
	const glm::vec3 hand = Place(k_SceneRock) + glm::vec3(0.0f, 3.0f, 0.0f);
	HandAt(hand);
	Sounds().played.clear();
	Sounds().counters.clear();
	game_packets::Push({game_packets::Type::Tap, rock});
	game_packets::Flush();
	// nothing happens before the turn
	EXPECT_TRUE(ecs::IsAvailable(rock));
	EXPECT_TRUE(Sounds().played.empty());
	Turn();
	EXPECT_FALSE(ecs::IsAvailable(rock));
	const auto halves = OtherRocks(rock);
	ASSERT_EQ(halves.size(), 2u);
	for (const auto half : halves)
	{
		EXPECT_EQ(Registry().Get<const Transform>(half).scale.x, scale * 0.7935f);
	}
	EXPECT_EQ(std::ranges::count(Sounds().counters, audio::Counter::RockTap), 1);
	std::vector<audio::PlayOptions> cracks;
	std::ranges::copy_if(Sounds().played, std::back_inserter(cracks),
	                     [](const audio::PlayOptions& played) { return played.sample.number == 130; });
	ASSERT_EQ(cracks.size(), 1u);
	EXPECT_EQ(cracks.front().position, hand);
	EXPECT_EQ(help_profile::Get(HelpEvent::Tap).Total(), 1u);
}

// The tap's turn checks again: a rock made no taller than 0.7 after the tap was sent is not tapped, nor counted
TEST_P(HandInputWorld, ATapIsCheckedAgainWhenItIsApplied)
{
	const auto rock = Rock();
	ASSERT_GT(ecs::object::GetHeight(rock), 0.7f);
	game_packets::Push({game_packets::Type::Tap, rock});
	// before the turn it is made smaller, 0.6 tall
	auto& transform = Registry().Get<Transform>(rock);
	transform.scale *= 0.6f / ecs::object::GetHeight(rock);
	ASSERT_LE(ecs::object::GetHeight(rock), 0.7f);
	PlayTurn();
	EXPECT_TRUE(ecs::IsAvailable(rock));
	EXPECT_TRUE(OtherRocks(rock).empty());
	EXPECT_EQ(help_profile::Get(HelpEvent::Tap).Total(), 0u);
}

// ---- picking_up.md, played through the hand's frame ------------------------------------------------------------------

// A one-shot orb resting on the land, pressed and held, is taken into the hand itself once the grab's short pull ends;
// its miracle counts as one the player has had
TEST_P(HandInputWorld, AOneShotOrbPressedAndHeldIsTakenIntoTheHandItself)
{
	if (!GameData())
	{
		GTEST_SKIP() << k_NeedsAnimations;
	}
	ASSERT_TRUE(HasAnimator());
	// the orb's shared model, as the game loads it with the loose meshes
	auto& files = Locator::filesystem::value();
	Locator::resources::value().GetMeshes().Load(
	    ecs::archetypes::OneOffSpellSeedArchetype::k_MeshName, resources::L3DLoader::FromDiskTag {},
	    files.GetPath<filesystem::Path::Data>() / "Spells" / "Meshes" / "O_bibble_up.l3d");
	const auto at = Place(k_SceneRock, glm::vec2(12.0f, 0.0f)) + glm::vec3(0.0f, 1.5f, 0.0f);
	const auto orb = ecs::archetypes::OneOffSpellSeedArchetype::Create(at, SpellSeedType::Fire, 0, 1.0f);
	ASSERT_TRUE(orb != entt::null);
	static_cast<void>(InfluenceOver(at, 500.0f));
	Aim(at);
	Frames(6);
	ASSERT_EQ(Hand().GetCursorObject(), std::optional(orb)) << WhatIsUnderTheCursor();
	PressAction();
	ASSERT_TRUE(FramesUntilHeld(orb, 30));
	const auto type = magic::MagicTypeForPowerUpLevel(magic::GetSpellSeedInfo(Info(), SpellSeedType::Fire), 0);
	EXPECT_TRUE(magic::players::HasMagicTypeEverBeenEnabled(PlayerNames::PLAYER_ONE, type));
}

// ---- multi_pickup.md, played through the hand's frame ----------------------------------------------------------------

// Scooping wood streams pieces of wood from the land under the hand: eight a second from the frame the handful is in
// the hand, each the hand pot's own model at 0.35 of its size, and they go with the scoop's end
TEST_P(HandInputWorld, ScoopingWoodStreamsItsPiecesEightASecondUntilTheScoopEnds)
{
	if (!GameData())
	{
		GTEST_SKIP() << k_NeedsAnimations;
	}
	ASSERT_TRUE(HasAnimator());
	const auto pile = Pile(PotInfo::WoodPile_1, 5000);
	const auto at = Registry().Get<const Transform>(pile).position;
	static_cast<void>(InfluenceOver(at, 3000.0f));
	Aim(at);
	// a new pile rises out of the land over a second; then the cursor on the middle of its model
	Frames(40);
	Aim(MiddleOf(pile));
	Frames(6);
	ASSERT_EQ(Hand().GetCursorObject(), std::optional(pile)) << WhatIsUnderTheCursor() << "; the pile: " << Describe(pile);
	const auto woodMesh = resources::HashIdentifier(Info().pot.at(static_cast<size_t>(PotInfo::HandWood)).meshId);
	const auto pieces = [woodMesh]() {
		size_t count = 0;
		Registry().Each<const ecs::components::Mesh, const Transform>(
		    [&count, woodMesh](entt::entity, const ecs::components::Mesh& mesh, const Transform& transform) {
			    if (mesh.id == woodMesh && transform.scale == glm::vec3(0.35f))
			    {
				    ++count;
			    }
		    });
		return count;
	};
	PressAction();
	ASSERT_TRUE(FramesUntil([this]() { return Hand().HoldsLockedSelect() && Hand().GetHeldObject().has_value(); }, 30));
	// the frame the handful came into the hand emitted the first; each frame owes eight a second of its time
	const float seconds = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(k_Frame).count()) / 1e6f;
	float owed = seconds * 8.0f;
	EXPECT_EQ(pieces(), 1u);
	for (int frame = 0; frame < 20; ++frame)
	{
		Frame();
		owed += seconds * 8.0f;
		EXPECT_EQ(pieces(), static_cast<size_t>(std::ceil(owed))) << "frame " << frame;
	}
	ReleaseAction();
	ASSERT_TRUE(FramesUntil([this]() { return !Hand().HoldsLockedSelect(); }, 10));
	Frame();
	EXPECT_EQ(pieces(), 0u);
}

// ---- holding.md, played through the hand's frame ---------------------------------------------------------------------

// A held handful is held from the side, the hand open as far as the handful's radius against its own size: the hand
// opens as the food in it grows, and no further once the radius reaches its size or the food its full height
TEST_P(HandInputWorld, AHeldHandfulOpensTheHandAsTheFoodInItGrows)
{
	if (!GameData())
	{
		GTEST_SKIP() << k_NeedsAnimations;
	}
	ASSERT_TRUE(HasAnimator());
	const auto pile = Pile(PotInfo::FoodPile, 10000);
	Scoop(pile);
	const auto handful = Hand().GetHeldObject();
	ASSERT_TRUE(handful.has_value());
	ASSERT_NE(*handful, pile);
	Aim(Place(k_SceneRock, glm::vec2(0.0f, -20.0f)));
	// the hand at an amount, still, its pose settled; the hand's grip: its radius against its own size, at most 1
	const auto heldAt = [this, &handful](uint32_t amount) {
		Registry().Get<Pot>(*handful).amount = amount;
		Frames(8);
		const float grip = std::min(1.0f, ecs::object::Get2DRadius(*handful) / (3.2f * Hand().GetHandScale()));
		const auto* bones = Hand().GetBoneMatrices();
		return std::pair {grip, bones != nullptr ? *bones : std::vector<glm::mat4> {}};
	};
	const auto [smallGrip, small] = heldAt(k_FirstHandful);
	EXPECT_EQ(Hand().GetCurrentAnimation(), "Chold_side");
	const auto [fullGrip, full] = heldAt(1600);
	const auto [overGrip, over] = heldAt(5000);
	ASSERT_FALSE(small.empty());
	EXPECT_EQ(smallGrip != fullGrip, small != full);
	EXPECT_EQ(fullGrip, overGrip);
	EXPECT_EQ(full, over);
}

// Every drawn frame the hand asks the thing it holds again for its hold radius and its height, as the original's holding
// state does: a held rock made larger while it is held opens the hand further from the next frames on, its hold type
// kept
TEST_P(HandInputWorld, AHeldThingMadeLargerOpensTheHandAtTheNextFrames)
{
	if (!GameData())
	{
		GTEST_SKIP() << k_NeedsAnimations;
	}
	ASSERT_TRUE(HasAnimator());
	const auto rock = Rock(glm::vec2(0.0f), 0.1f);
	PickUp(rock);
	ASSERT_EQ(Hand().GetHeldObject(), std::optional(rock));
	Aim(Place(k_SceneRock, glm::vec2(0.0f, -20.0f)));
	Frames(8);
	EXPECT_EQ(Hand().GetCurrentAnimation(), "Chold_above");
	const auto* before = Hand().GetBoneMatrices();
	ASSERT_NE(before, nullptr);
	const auto small = *before;
	Registry().Get<Transform>(rock).scale *= 2.0f;
	Frames(8);
	EXPECT_EQ(Hand().GetCurrentAnimation(), "Chold_above");
	const auto* after = Hand().GetBoneMatrices();
	ASSERT_NE(after, nullptr);
	EXPECT_NE(*after, small);
}

// Gripping the land while holding something keeps it in the hand, and the hand is still drawn holding it
TEST_P(HandInputWorld, GrippingTheLandWhileHoldingKeepsTheThingInTheHand)
{
	if (!GameData())
	{
		GTEST_SKIP() << k_NeedsAnimations;
	}
	ASSERT_TRUE(HasAnimator());
	const auto rock = Rock();
	PickUp(rock);
	ASSERT_EQ(Hand().GetHeldObject(), std::optional(rock));
	Aim(Place(k_SceneRock, glm::vec2(0.0f, -20.0f)));
	Frames(6);
	const auto holdClip = Hand().GetCurrentAnimation();
	ASSERT_NE(holdClip, "Cgrip");
	PressGrip();
	Frames(6);
	EXPECT_EQ(Hand().GetHeldObject(), std::optional(rock));
	EXPECT_EQ(Hand().GetRenderHandObject(), std::optional(rock));
	EXPECT_EQ(Hand().GetCurrentAnimation(), holdClip);
	ReleaseGrip();
	Frames(2);
	EXPECT_EQ(Hand().GetHeldObject(), std::optional(rock));
}

// Gripping the land while holding something is the land grip all the same: the interface's hand state goes from Object
// In Hand (24) to Grip Landscape (20), the grip starts once with one of the six grab samples (4 to 9), and letting go
// gives Object In Hand back
TEST_P(HandInputWorld, GrippingTheLandWhileHoldingIsTheLandGripAllTheSame)
{
	if (!GameData())
	{
		GTEST_SKIP() << k_NeedsAnimations;
	}
	ASSERT_TRUE(HasAnimator());
	const auto rock = Rock();
	PickUp(rock);
	ASSERT_EQ(Hand().GetHeldObject(), std::optional(rock));
	Aim(Place(k_SceneRock, glm::vec2(0.0f, -20.0f)));
	Frames(6);
	EXPECT_EQ(Hand().GetInterfaceHandState(), 24);
	Tags().points.clear();
	PressGrip();
	Frames(6);
	EXPECT_EQ(Hand().GetInterfaceHandState(), 20);
	ASSERT_EQ(Tags().points.size(), 1U);
	EXPECT_GE(Tags().points.back().sample, 4);
	EXPECT_LT(Tags().points.back().sample, 4 + 6);
	EXPECT_EQ(Hand().GetHeldObject(), std::optional(rock));
	ReleaseGrip();
	Frames(2);
	EXPECT_EQ(Hand().GetInterfaceHandState(), 24);
	EXPECT_EQ(Tags().points.size(), 1U);
	EXPECT_EQ(Hand().GetHeldObject(), std::optional(rock));
}

// ---- states_overview.md and hand_animations.md, played through the hand's frame ---------------------------------------

// A set animation started at a fixed place plays there until its length has passed in the frames' real time, then the
// hand goes back to its normal pose under the cursor
TEST_P(HandInputWorld, ASetAnimationAtAFixedPlacePlaysItsLengthThenTheHandGoesBack)
{
	if (!GameData())
	{
		GTEST_SKIP() << k_NeedsAnimations;
	}
	ASSERT_TRUE(HasAnimator());
	const auto animations = Animations();
	ASSERT_NE(animations, nullptr);
	ASSERT_TRUE(animations->Has("Ctap_house"));
	const auto length = static_cast<int64_t>(animations->GetDurationMs("Ctap_house"));
	ASSERT_GT(length, 0);
	Aim(Place(k_SceneRock, glm::vec2(10.0f, -30.0f)));
	Frames(6);
	const auto normalClip = Hand().GetCurrentAnimation();
	Hand().StartFixedPosAnimation("Ctap_house", Place(k_SceneRock, glm::vec2(0.0f, -20.0f)) + glm::vec3(0.0f, 2.0f, 0.0f));
	ASSERT_TRUE(FramesUntil([this]() { return Hand().GetCurrentAnimation() == "Ctap_house"; }, 3));
	int64_t frames = 0;
	while (Hand().GetCurrentAnimation() == "Ctap_house" && frames < 1000)
	{
		Frame();
		++frames;
	}
	// it ends on the first frame its time reaches its length, and the frames after show the normal pose
	const int64_t frameMs = k_Frame.count();
	EXPECT_GE((frames + 1) * frameMs, length);
	EXPECT_LT((frames - 2) * frameMs, length);
	EXPECT_EQ(Hand().GetCurrentAnimation(), normalClip);
}

// While a set animation plays, the hand stays upright at the place it started, wherever the mouse goes
TEST_P(HandInputWorld, WhileASetAnimationPlaysTheHandStaysUprightAtItsPlace)
{
	if (!GameData())
	{
		GTEST_SKIP() << k_NeedsAnimations;
	}
	ASSERT_TRUE(HasAnimator());
	const auto place = Place(k_SceneRock, glm::vec2(0.0f, -20.0f)) + glm::vec3(0.0f, 2.0f, 0.0f);
	Aim(Place(k_SceneRock, glm::vec2(10.0f, -30.0f)));
	Frames(6);
	Hand().StartFixedPosAnimation("Ctap_house", place);
	// past the state's blend of 0.13 s
	Frames(6);
	ASSERT_EQ(Hand().GetCurrentAnimation(), "Ctap_house");
	for (const auto pixel : {k_Screen / 2 + glm::ivec2(60, 0), k_Screen / 2 + glm::ivec2(-40, 30)})
	{
		SetMouse(pixel);
		Frame();
		ASSERT_EQ(Hand().GetCurrentAnimation(), "Ctap_house");
		EXPECT_EQ(HandPoint(), place);
		const auto up = HandUp();
		EXPECT_NEAR(up.x, 0.0f, 1e-6f);
		EXPECT_NEAR(up.y, 1.0f, 1e-6f);
		EXPECT_NEAR(up.z, 0.0f, 1e-6f);
	}
}

// The hand's animations come from its animation file laid out by the hand spec: the hand's 22 bones posed, the clips its
// states play there, and the normal state's wiggle playing first
TEST_P(HandInputWorld, TheHandsAnimationsLoadFromItsFileLaidOutByTheHandSpec)
{
	if (!GameData())
	{
		GTEST_SKIP() << k_NeedsAnimations;
	}
	ASSERT_TRUE(HasAnimator());
	ASSERT_NE(Hand().GetBoneMatrices(), nullptr);
	EXPECT_EQ(Hand().GetBoneMatrices()->size(), 22u);
	EXPECT_EQ(Hand().GetCurrentAnimation(), "Cwiggle");
	const auto names = Hand().GetAnimationNames();
	for (const auto* clip : {"Cwiggle", "Cgrip", "Chold_side", "Chold_above", "Ctap_house"})
	{
		EXPECT_NE(std::find(names.begin(), names.end(), std::string(clip)), names.end()) << clip;
	}
}

// ---- placement.md, played through the hand's frame -------------------------------------------------------------------

// Gripping the land, the hand goes to the land under the cursor as it takes hold and stays on that point while the mouse
// and the camera move
TEST_P(HandInputWorld, GrippingTheLandTheHandStaysOnTheGrippedPoint)
{
	if (!GameData())
	{
		GTEST_SKIP() << k_NeedsAnimations;
	}
	ASSERT_TRUE(HasAnimator());
	const auto at = Place(k_SceneRock, glm::vec2(0.0f, -20.0f));
	Aim(at);
	Frames(6);
	PressGrip();
	ASSERT_TRUE(FramesUntil([this]() { return Hand().GetCurrentAnimation() == "Cgrip"; }, 10));
	ASSERT_TRUE(LastFrame().handLand.has_value());
	const auto gripped = HandPoint();
	EXPECT_NEAR(gripped.x, LastFrame().handLand->x, 1e-3f);
	EXPECT_NEAR(gripped.y, LastFrame().handLand->y, 1e-3f);
	EXPECT_NEAR(gripped.z, LastFrame().handLand->z, 1e-3f);
	for (int frame = 0; frame < 6; ++frame)
	{
		SetMouse(k_Screen / 2 + glm::ivec2(10 * frame, 5 * frame));
		LookFrom(at + glm::vec3(2.0f * static_cast<float>(frame), 14.0f, -24.0f + static_cast<float>(frame)), at);
		Frame();
		EXPECT_EQ(HandPoint(), gripped) << "frame " << frame;
	}
	ReleaseGrip();
	Frames(2);
	EXPECT_NE(Hand().GetCurrentAnimation(), "Cgrip");
}

// Over sloping land the empty hand's up eases from straight up to the land's normal in 0.4 s, and then stays there
TEST_P(HandInputWorld, TheHandsUpEasesToTheLandsSlopeInFourTenthsOfASecond)
{
	if (!GameData())
	{
		GTEST_SKIP() << k_NeedsAnimations;
	}
	ASSERT_TRUE(HasAnimator());
	// dry land rising 2 height units a cell eastwards
	auto& slope = static_cast<test::SlopedIsland&>(Locator::terrainSystem::emplace<test::SlopedIsland>(15, 2));
	const glm::vec3 at {1000.0f, slope.HeightOfX(1000.0f), 1200.0f};
	const auto normal = glm::normalize(slope.GetNormalAt(glm::vec2(at.x, at.z)));
	Aim(at);
	const float frameSeconds = static_cast<float>(k_Frame.count()) * 0.001f;
	for (int frame = 1; frame <= 20; ++frame)
	{
		Frame();
		const float apart = glm::length(HandUp() - normal);
		if (static_cast<float>(frame) * frameSeconds < 0.3f)
		{
			EXPECT_GT(apart, 1e-4f) << "frame " << frame;
		}
		else if (static_cast<float>(frame - 1) * frameSeconds > hand_orientation::k_UpEaseSeconds)
		{
			EXPECT_LT(apart, 1e-5f) << "frame " << frame;
		}
	}
}

// ---- navigation.md, played through the hand's frame ------------------------------------------------------------------

// Taking hold of the land plays one of the six grab samples, once, as the grip starts: a two-dimensional point sound in
// the game's bank that plays at once and does not loop; holding on plays no more, and the next grip plays again
TEST_P(HandInputWorld, GrippingTheLandPlaysOneOfSixGrabSamplesOnceAsItTakesHold)
{
	if (!GameData())
	{
		GTEST_SKIP() << k_NeedsAnimations;
	}
	ASSERT_TRUE(HasAnimator());
	Aim(Place(k_SceneRock, glm::vec2(0.0f, -20.0f)));
	Frames(6);
	Tags().points.clear();
	for (int grip = 0; grip < 2; ++grip)
	{
		PressGrip();
		Frames(6);
		ASSERT_EQ(Tags().points.size(), static_cast<size_t>(grip + 1)) << "grip " << grip;
		const auto& grab = Tags().points.back();
		EXPECT_GE(grab.sample, 4);
		EXPECT_LT(grab.sample, 4 + 6);
		EXPECT_EQ(grab.mode, 3);
		EXPECT_EQ(grab.loops, 0);
		EXPECT_FALSE(grab.extra3DFlag);
		EXPECT_FALSE(grab.is3D);
		EXPECT_EQ(grab.bank, audio::SfxBank::InGame);
		EXPECT_EQ(grab.delay, 0);
		ReleaseGrip();
		Frames(3);
	}
}

// The land grip starts once, at the press, even with the cursor over the sky: with no land under the cursor and the
// action point cleared, it starts at the map's corner (on this dry land the dust and one of the six grab samples there),
// the interface's hand state is Grip Landscape from then on, and the land the cursor comes over while the button is
// still held starts nothing more
TEST_P(HandInputWorld, GrippingOverTheSkyStartsTheGripAtThePressAtTheMapsCorner)
{
	if (!GameData())
	{
		GTEST_SKIP() << k_NeedsAnimations;
	}
	ASSERT_TRUE(HasAnimator());
	const auto at = Place(k_SceneRock, glm::vec2(0.0f, -20.0f));
	const auto eye = at + glm::vec3(0.0f, 14.0f, -24.0f);
	// the camera looking up above the horizon: nothing under the cursor
	LookFrom(eye, eye + glm::vec3(0.0f, 10.0f, 24.0f));
	SetMouse(k_Screen / 2);
	Frames(6);
	ASSERT_FALSE(LastFrame().handLand.has_value());
	ASSERT_FALSE(Hand().GetActionPoint().has_value());
	Tags().points.clear();
	PressGrip();
	Frame();
	ASSERT_EQ(Tags().points.size(), 1U);
	const auto& grab = Tags().points.back();
	EXPECT_EQ(grab.point.x, 0.0f);
	EXPECT_EQ(grab.point.z, 0.0f);
	EXPECT_GE(grab.sample, 4);
	EXPECT_LT(grab.sample, 4 + 6);
	Frame();
	EXPECT_EQ(Hand().GetInterfaceHandState(), 20);
	// the camera down onto the land, the button still held
	LookFrom(eye, at);
	Frames(6);
	ASSERT_TRUE(LastFrame().handLand.has_value());
	EXPECT_EQ(Tags().points.size(), 1U);
	EXPECT_EQ(Hand().GetInterfaceHandState(), 20);
	ReleaseGrip();
	Frames(2);
	EXPECT_NE(Hand().GetInterfaceHandState(), 20);
}

// ---- pouring.md, played through the hand's frame ---------------------------------------------------------------------

// Pouring a miracle held in the hand, the pour's lift raises the hand by the lift drawn between the last two turns, and
// its tilt tips it; a pour that does not hold the hand where it started (the water's) keeps the hand under the cursor
TEST_P(HandInputWorld, ThePoursLiftRaisesTheHandAndItsTiltTipsIt)
{
	if (!GameData())
	{
		GTEST_SKIP() << k_NeedsAnimations;
	}
	ASSERT_TRUE(HasAnimator());
	const auto seed = Registry().Create();
	Registry().Assign<Transform>(seed, Place(k_SceneRock, glm::vec2(0.0f, -20.0f)), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& component = Registry().Assign<ecs::components::SpellSeed>(seed);
	component.seedType = SpellSeedType::Water;
	component.creator.player = PlayerNames::PLAYER_ONE;
	component.ready = true;
	Hand().PlaceObjectInMagicHand(seed);
	ASSERT_EQ(Hand().GetHeldObject(), std::optional(seed));
	Aim(Place(k_SceneRock, glm::vec2(0.0f, -20.0f)));
	// the hand settled where it holds the seed, the mouse still
	Frames(30);
	const auto still = HandPoint();
	const auto stillUp = HandUp();
	Frame();
	// settled: the hand's distance still eases by less than a ten-thousandth
	ASSERT_NEAR(HandPoint().y, still.y, 1e-4f);
	// the pour as a miracle's effect starts it: not holding the hand where it started, looping
	ecs::systems::hand_grain::Start(false, 4.0f, 2.0f, 0.5f, true);
	bool tipped = false;
	for (int frame = 0; frame < 15; ++frame)
	{
		Frame();
		const auto point = HandPoint();
		EXPECT_NEAR(point.y - still.y, ecs::systems::hand_grain::Height(), 1e-4f) << "frame " << frame;
		EXPECT_NEAR(point.x, still.x, 1e-4f);
		EXPECT_NEAR(point.z, still.z, 1e-4f);
		tipped = tipped || (ecs::systems::hand_grain::Tilt() != 0.0f && glm::length(HandUp() - stillUp) > 1e-4f);
	}
	EXPECT_TRUE(tipped);
	ecs::systems::hand_grain::Stop();
}

// Holding a miracle's seed, the hand is placed as a holding hand is: over the point under the cursor, lifted by its hold
// (the seed info's hold type once the seed is ready, hung by its lowering times the height of the seed info's model)
TEST_P(HandInputWorld, HoldingASeedTheHandIsPlacedAsAHoldingHandIsUnderTheCursor)
{
	if (!GameData())
	{
		GTEST_SKIP() << k_NeedsAnimations;
	}
	ASSERT_TRUE(HasAnimator());
	ASSERT_FALSE(ecs::systems::hand_grain::Active());
	const auto at = Place(k_SceneRock, glm::vec2(0.0f, -20.0f));
	const auto seed = Registry().Create();
	Registry().Assign<Transform>(seed, at, glm::mat3(1.0f), glm::vec3(1.0f));
	auto& component = Registry().Assign<ecs::components::SpellSeed>(seed);
	component.seedType = SpellSeedType::Water;
	component.creator.player = PlayerNames::PLAYER_ONE;
	component.ready = true;
	Hand().PlaceObjectInMagicHand(seed);
	ASSERT_EQ(Hand().GetHeldObject(), std::optional(seed));
	Aim(at);
	Frames(30);
	ASSERT_TRUE(LastFrame().point.has_value());
	const auto ground = *LastFrame().point;
	// the hold as the hand works it out for a seed
	const auto& info = magic::seed::InfoOf(Registry().Get<const ecs::components::SpellSeed>(seed));
	float height = ecs::object::GetHeight(seed);
	if (const auto half = ecs::object::MeshHalfExtents(resources::HashIdentifier(info.mesh)); half)
	{
		height = ecs::object::Height(*half, ecs::object::GetScaleField(seed));
	}
	const auto hold = magic::hand_hold::HoldTypeOf(true, info.holdType);
	const float lift = hand_place::HoldHeight(
	    magic::hand_hold::HoldLift(hold, magic::hand_hold::SeedHang(hold, info.holdLoweringMultiplier, height),
	                               Hand().GetHandScale()),
	    false, height);
	const auto point = HandPoint();
	EXPECT_NEAR(point.x, ground.x, 1e-3f);
	EXPECT_NEAR(point.z, ground.z, 1e-3f);
	EXPECT_NEAR(point.y - ground.y, lift, 1e-3f);
	// and that point is under the cursor: on the mouse's ray from the eye
	const auto across =
	    glm::length(glm::cross(glm::normalize(ground - LastFrame().rayOrigin), glm::normalize(LastFrame().rayDirection)));
	EXPECT_LT(across, 1e-3f);
}

// ---- tug.md, played through the hand's frame -------------------------------------------------------------------------

// A tree pulled at, near where the hand grips it, and let go before it comes free stays rooted as it stood: not in the
// hand nor in the physics, in its cells, its place and size untouched, with no creak and no deed counted
TEST_P(HandInputWorld, ATreeLetGoBeforeItComesFreeStaysRootedAsItStood)
{
	if (!GameData())
	{
		GTEST_SKIP() << k_NeedsAnimations;
	}
	ASSERT_TRUE(HasAnimator());
	const auto birch = Birch();
	const auto standing = Registry().Get<const Transform>(birch);
	// the grip, a tenth of its height up its trunk: pressed there it does not come free
	const auto grip = standing.position + glm::vec3(0.0f, 0.1f * ecs::object::GetHeight(birch), 0.0f);
	static_cast<void>(InfluenceOver(grip, 500.0f));
	LookFrom(grip + glm::vec3(0.0f, 5.0f, -20.0f), grip);
	SetMouse(k_Screen / 2);
	Frames(6);
	ASSERT_EQ(Hand().GetCursorObject(), std::optional(birch));
	Tags().points.clear();
	PressAction();
	// past the pull's start: the tree is drawn leaning in the hand
	ASSERT_TRUE(FramesUntil([&birch]() { return Registry().AllOf<ecs::components::HandDrawPose>(birch); }, 20));
	Frames(2);
	ASSERT_FALSE(Hand().GetHeldObject().has_value());
	ReleaseAction();
	Frames(2);
	EXPECT_FALSE(Hand().GetHeldObject().has_value());
	ASSERT_TRUE(Registry().Valid(birch));
	EXPECT_TRUE(Registry().AllOf<ecs::components::Tree>(birch));
	EXPECT_TRUE(ecs::map_cells::IsObjectInMap(birch));
	EXPECT_EQ(Dynamics().Find(birch), nullptr);
	const auto& transform = Registry().Get<const Transform>(birch);
	EXPECT_EQ(transform.position, standing.position);
	EXPECT_EQ(transform.rotation, standing.rotation);
	EXPECT_EQ(transform.scale, standing.scale);
	EXPECT_TRUE(Tags().points.empty());
	EXPECT_EQ(ecs::effects::alignment::Of(PlayerNames::PLAYER_ONE).pending, 0.0f);
}

// A tree pulled until it comes free: that frame is still the pulling state's. The tree is drawn as it leans at its
// base, and the hand at its grip low on the trunk, not yet up where it holds it; from the next frames the hand holds it
TEST_P(HandInputWorld, TheFrameATreeComesFreeItStillLeansAtItsBaseWithTheHandAtItsGrip)
{
	if (!GameData())
	{
		GTEST_SKIP() << k_NeedsAnimations;
	}
	ASSERT_TRUE(HasAnimator());
	const auto birch = Birch();
	const auto base = Registry().Get<const Transform>(birch).position;
	const float height = ecs::object::GetHeight(birch);
	// pressed at its grip, a tenth of its height up, as the tug's testbed pulls a beech
	const auto grip = base + glm::vec3(0.0f, 0.1f * height, 0.0f);
	static_cast<void>(InfluenceOver(grip, 500.0f));
	LookFrom(grip + glm::vec3(0.0f, 5.0f, -20.0f), grip);
	SetMouse(k_Screen / 2);
	Frames(6);
	ASSERT_EQ(Hand().GetCursorObject(), std::optional(birch));
	PressAction();
	// the pointer sweeps right with the button down, the hand pulling harder, until the tree comes free: the hand then
	// waits for it to be put in it (the action's state 7)
	bool freed = false;
	for (int frame = 0; frame < 240 && !freed; ++frame)
	{
		SetMouse(k_Screen / 2 + glm::ivec2(std::min(frame * 2, 300), 0));
		Frames(1);
		freed = Hand().GetActionStateNumber() == 7;
	}
	ASSERT_TRUE(freed);
	ASSERT_FALSE(Hand().GetHeldObject().has_value());
	ASSERT_TRUE(Registry().AllOf<ecs::components::HandDrawPose>(birch));
	EXPECT_LT(glm::distance(Registry().Get<const ecs::components::HandDrawPose>(birch).position, base), 1e-4f);
	EXPECT_LT(glm::distance(HandPoint(), base), 0.5f * height);
	// then it is held
	EXPECT_TRUE(FramesUntilHeld(birch, 40));
	ReleaseAction();
	Frames(2);
}

// With the leash in the hand and not tied, a standing tree pressed and let go within the grab's wait is tapped, which
// sends the creature to it at the next turn; held past the wait and let go before it comes free, it sends nothing. The
// tree stays rooted both times
TEST_P(HandInputWorld, ATreeLetGoWithinTheGrabsWaitWithTheLeashOnSendsTheCreatureToIt)
{
	if (!GameData())
	{
		GTEST_SKIP() << k_NeedsAnimations;
	}
	ASSERT_TRUE(HasAnimator());
	static test::creature_loop_fakes::CallLog log;
	log.clear();
	auto& leash = static_cast<test::creature_loop_fakes::FakeLeash&>(
	    Locator::leashSystem::emplace<test::creature_loop_fakes::FakeLeash>(log));
	leash.playersCreature = Registry().Create();
	leash.leashed = true;
	static std::vector<entt::entity> sentTo;
	sentTo.clear();
	game_packets::SetHandler(game_packets::Type::LeashActOnObject,
	                         [](const game_packets::Packet& packet) { sentTo.push_back(packet.object); });
	const auto birch = Birch();
	// the grip, a tenth of its height up its trunk: pressed there it does not come free
	const auto grip =
	    Registry().Get<const Transform>(birch).position + glm::vec3(0.0f, 0.1f * ecs::object::GetHeight(birch), 0.0f);
	static_cast<void>(InfluenceOver(grip, 500.0f));
	LookFrom(grip + glm::vec3(0.0f, 5.0f, -20.0f), grip);
	SetMouse(k_Screen / 2);
	Frames(6);
	ASSERT_EQ(Hand().GetCursorObject(), std::optional(birch));
	// held three frames of 33 ms, let go on the fourth: 99 ms after the press, in the grab state all along
	PressAction();
	Frames(3);
	EXPECT_EQ(Hand().GetActionStateNumber(), 13);
	ReleaseAction();
	Frames(4);
	EXPECT_EQ(sentTo, std::vector<entt::entity> {birch});
	// held ten frames, 330 ms, then let go: no tap
	sentTo.clear();
	PressAction();
	Frames(10);
	ReleaseAction();
	Frames(4);
	EXPECT_TRUE(sentTo.empty());
	EXPECT_FALSE(Hand().GetHeldObject().has_value());
	EXPECT_TRUE(ecs::map_cells::IsObjectInMap(birch));
	Locator::leashSystem::reset();
}

// ---- throwing.md, played through the hand's frame --------------------------------------------------------------------

// ---- clicking_and_activating.md, played through the hand's frame -----------------------------------------------------

// A quick press and release of the action over a rock taps it: nothing happens at once, and at the next turn's start the
// rock, taller than 0.7, splits in two; the tap counts in the help profile
TEST_P(HandInputWorld, AQuickClickOnARockTapsItAtTheNextTurn)
{
	if (!GameData())
	{
		GTEST_SKIP() << k_NeedsAnimations;
	}
	ASSERT_TRUE(HasAnimator());
	const auto rock = Rock();
	ASSERT_GT(ecs::object::GetHeight(rock), 0.7f);
	const auto at = Registry().Get<const Transform>(rock).position;
	static_cast<void>(InfluenceOver(at, 500.0f));
	Aim(at + glm::vec3(0.0f, 0.5f * ecs::object::GetHeight(rock), 0.0f));
	Frames(6);
	ASSERT_EQ(Hand().GetCursorObject(), std::optional(rock));
	// pressed for a frame, well within the 225 ms of a tap
	PressAction();
	Frame();
	ReleaseAction();
	Frame();
	EXPECT_TRUE(ecs::IsAvailable(rock));
	EXPECT_FALSE(Hand().GetHeldObject().has_value());
	ASSERT_TRUE(FramesUntil([&rock]() { return !ecs::IsAvailable(rock); }, 10));
	EXPECT_EQ(OtherRocks(rock).size(), 2u);
	EXPECT_EQ(help_profile::Get(HelpEvent::Tap).Total(), 1u);
	EXPECT_FALSE(Hand().GetHeldObject().has_value());
}

// A resting thing is taken into the hand the frame after its tug hold ends, as the grab state reads the hold as the
// hand's draw left it the frame before: let go on the frame the hold ends, a rock is still tapped; held one frame more, it
// is taken
TEST_P(HandInputWorld, ARockLetGoOnTheFrameItsHoldEndsIsStillTapped)
{
	if (!GameData())
	{
		GTEST_SKIP() << k_NeedsAnimations;
	}
	ASSERT_TRUE(HasAnimator());
	const auto rock = Rock();
	ASSERT_GT(ecs::object::GetHeight(rock), 0.7f);
	const auto at = Registry().Get<const Transform>(rock).position;
	static_cast<void>(InfluenceOver(at, 500.0f));
	Aim(at + glm::vec3(0.0f, 0.5f * ecs::object::GetHeight(rock), 0.0f));
	Frames(6);
	ASSERT_EQ(Hand().GetCursorObject(), std::optional(rock));
	// at 33 ms a frame the hold's 0.13 s is reached on the fifth frame of the press, and the hold ends on the sixth
	PressAction();
	Frames(5);
	EXPECT_EQ(Hand().GetActionStateNumber(), 13);
	ReleaseAction();
	Frame();
	EXPECT_FALSE(Hand().GetHeldObject().has_value());
	ASSERT_TRUE(FramesUntil([&rock]() { return !ecs::IsAvailable(rock); }, 10));
	EXPECT_EQ(help_profile::Get(HelpEvent::Tap).Total(), 1u);
	// another rock, held through the frame after: out of the grab state, taken
	const auto other = Rock(glm::vec2(8.0f, 0.0f));
	const auto there = Registry().Get<const Transform>(other).position;
	Aim(there + glm::vec3(0.0f, 0.5f * ecs::object::GetHeight(other), 0.0f));
	Frames(6);
	ASSERT_EQ(Hand().GetCursorObject(), std::optional(other));
	PressAction();
	Frames(7);
	EXPECT_NE(Hand().GetActionStateNumber(), 13);
	ReleaseAction();
	Frames(4);
	EXPECT_TRUE(ecs::IsAvailable(other));
	EXPECT_EQ(help_profile::Get(HelpEvent::Tap).Total(), 1u);
}

// A resting thing pressed and then, before it is taken, made unpickable (a script's SET_ID_PICKUPABLE): the pick-up
// checks again as the grab completes and fails, nothing is held, and the hand lets go of the thing it drew from the
// press, as a release does. Before, the thing stayed drawn under the empty hand, and no press could drop it
TEST_P(HandInputWorld, AThingThePickUpRefusesIsNotLeftDrawnInTheEmptyHand)
{
	if (!GameData())
	{
		GTEST_SKIP() << k_NeedsAnimations;
	}
	ASSERT_TRUE(HasAnimator());
	const auto rock = Rock();
	const auto at = Registry().Get<const Transform>(rock).position;
	static_cast<void>(InfluenceOver(at, 500.0f));
	Aim(at + glm::vec3(0.0f, 0.5f * ecs::object::GetHeight(rock), 0.0f));
	Frames(6);
	ASSERT_EQ(Hand().GetCursorObject(), std::optional(rock));
	PressAction();
	Frame();
	ASSERT_EQ(Hand().GetActionStateNumber(), 13);
	// made unpickable while the press still waits on the tug hold
	ecs::object_flags::SetPickupable(rock, false);
	Frames(8);
	EXPECT_NE(Hand().GetActionStateNumber(), 13);
	EXPECT_FALSE(Hand().GetHeldObject().has_value());
	EXPECT_FALSE(Registry().AllOf<ecs::components::HandDrawPose>(rock));
	EXPECT_EQ(Registry().Get<const Transform>(rock).position, at);
	ReleaseAction();
	Frames(2);
	EXPECT_FALSE(Hand().GetHeldObject().has_value());
	EXPECT_FALSE(Registry().AllOf<ecs::components::HandDrawPose>(rock));
}

// A resting thing pressed is taken at the press, but while its tug hold lasts the hand tugs and draws nothing in it: the
// thing stays where it stands, so a quick tap never shows it in the hand. Held past the hold, it is in the hand
TEST_P(HandInputWorld, ARockPressedStaysWhereItStandsWhileItsTugHoldLasts)
{
	if (!GameData())
	{
		GTEST_SKIP() << k_NeedsAnimations;
	}
	ASSERT_TRUE(HasAnimator());
	const auto rock = Rock();
	const auto at = Registry().Get<const Transform>(rock).position;
	static_cast<void>(InfluenceOver(at, 500.0f));
	Aim(at + glm::vec3(0.0f, 0.5f * ecs::object::GetHeight(rock), 0.0f));
	Frames(6);
	ASSERT_EQ(Hand().GetCursorObject(), std::optional(rock));
	PressAction();
	for (int frame = 1; frame <= 4; ++frame)
	{
		Frame();
		EXPECT_EQ(Hand().GetActionStateNumber(), 13) << "frame " << frame;
		EXPECT_FALSE(Registry().AllOf<ecs::components::HandDrawPose>(rock)) << "frame " << frame;
		EXPECT_EQ(Registry().Get<const Transform>(rock).position, at) << "frame " << frame;
		// the tug state holds the hand on the thing itself: a rock, held from above, at its very origin
		if (frame >= 2)
		{
			const auto hand = Registry().Get<const Transform>(Hand().GetPlayerHands()[0]).position;
			EXPECT_LT(glm::length(hand - at), 1e-3f) << "frame " << frame;
		}
	}
	// the hold ends on the sixth frame of the press and the seventh takes it: the grab sends the place-in-hand packet
	Frames(3);
	EXPECT_NE(Hand().GetActionStateNumber(), 13);
	EXPECT_FALSE(Hand().GetHeldObject().has_value());
	// the packet goes out with the next frame's flush, and the first turn after applies it: held on that turn's frame
	Frame();
	const auto flushed = game_clock::Turn();
	for (int frame = 0; frame < 4 && game_clock::Turn() == flushed; ++frame)
	{
		EXPECT_FALSE(Hand().GetHeldObject().has_value());
		Frame();
	}
	ASSERT_EQ(game_clock::Turn(), flushed + 1);
	EXPECT_EQ(Hand().GetHeldObject(), std::optional(rock));
	ReleaseAction();
	Frames(2);
}

// While a pressed thing's tug hold lasts, the tug state's hand takes the thing's own up, not the land's: a rock tipped
// over leans the hand with it
TEST_P(HandInputWorld, TheTugHoldsHandLeansWithTheThingsOwnUp)
{
	if (!GameData())
	{
		GTEST_SKIP() << k_NeedsAnimations;
	}
	ASSERT_TRUE(HasAnimator());
	const auto rock = Rock();
	// tipped 0.3 radians about the world's z axis
	const float c = std::cos(0.3f);
	const float s = std::sin(0.3f);
	auto& transform = Registry().Get<Transform>(rock);
	transform.rotation = glm::mat3(glm::vec3(c, s, 0.0f), glm::vec3(-s, c, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	const auto up = glm::normalize(transform.rotation[1]);
	const auto at = transform.position;
	static_cast<void>(InfluenceOver(at, 500.0f));
	Aim(at + glm::vec3(0.0f, 0.5f * ecs::object::GetHeight(rock), 0.0f));
	Frames(6);
	ASSERT_EQ(Hand().GetCursorObject(), std::optional(rock));
	PressAction();
	Frames(2);
	ASSERT_EQ(Hand().GetActionStateNumber(), 13);
	EXPECT_LT(glm::length(HandUp() - up), 1e-4f);
	ReleaseAction();
	Frames(2);
}

INSTANTIATE_TEST_SUITE_P(Data, HandInputWorld,
                         ::testing::Values(test::world::WorldData::Synthetic, test::world::WorldData::Game),
                         test::world::DataName);

// ---- throwing.md: the release's spin, played through the hand's frame -------------------------------------------------

namespace
{
/// The hand's world watching a thrown thing at each turn's start, between the turn's packets and its physics: its
/// turning push (cleared at the turn's end), and where it was as the hand let go of it
class HandReleaseWorld: public HandInputWorld
{
protected:
	void OnTurnApplied() override
	{
		if (watched == entt::null)
		{
			return;
		}
		const auto* entry = Dynamics().Find(watched);
		turns.push_back({.gameMs = sinceRelease, .torque = entry != nullptr ? entry->body.externalTorque : glm::vec3(0.0f)});
		// the first turn the hand no longer holds it: where it was let go, before the physics moves it
		if (!letGo.has_value() && Hand().GetHeldObject() != std::optional(watched))
		{
			letGo = Registry().Get<const Transform>(watched);
		}
	}

	struct TurnSeen
	{
		int64_t gameMs;
		glm::vec3 torque;
	};
	entt::entity watched {entt::null};
	/// The frames' game time since the release was applied
	int64_t sinceRelease {0};
	std::vector<TurnSeen> turns;
	/// The watched thing's place as the hand let go of it
	std::optional<Transform> letGo;
};
} // namespace

// The hand remembers what it last let go of: 180 ms of the frames' game time after it last held it, the thing is given
// a turn of spin from the hand's last move, once, at a turn's start
TEST_P(HandReleaseWorld, AThingLetGoIsSpunOnceAfterOneHundredAndEightyMilliseconds)
{
	if (!GameData())
	{
		GTEST_SKIP() << k_NeedsAnimations;
	}
	ASSERT_TRUE(HasAnimator());
	const auto rock = Rock();
	static_cast<void>(HoldAndSweep(rock));
	ASSERT_FALSE(HasFatalFailure());
	ReleaseAction();
	ASSERT_TRUE(FramesUntil([this]() { return !Hand().GetHeldObject().has_value(); }, 10));
	watched = rock;
	// the mouse goes on sweeping, so that the hand's last move is not nothing
	for (int frame = 1; frame <= 15; ++frame)
	{
		SetMouse(k_Screen / 2 + glm::ivec2(180 + 20 * frame, 0));
		Frame();
		sinceRelease += k_Frame.count();
	}
	std::vector<TurnSeen> spun;
	std::ranges::copy_if(turns, std::back_inserter(spun), [](const TurnSeen& turn) { return turn.torque != glm::vec3(0.0f); });
	ASSERT_EQ(spun.size(), 1u);
	EXPECT_GT(spun.front().gameMs, 180);
}

// Thrown with a sweep of the mouse, the thing leaves the hand where the hand drew it the frame before the release,
// turned as it was drawn and at its own size: as it is let go, at the start of the turn that applies the release and
// before that turn's physics moves it
TEST_P(HandReleaseWorld, AThrownThingLeavesTheHandWhereTheHandDrewIt)
{
	if (!GameData())
	{
		GTEST_SKIP() << k_NeedsAnimations;
	}
	ASSERT_TRUE(HasAnimator());
	const auto rock = Rock();
	const auto scale = Registry().Get<const Transform>(rock).scale;
	const auto drawn = HoldAndSweep(rock);
	ASSERT_FALSE(HasFatalFailure());
	watched = rock;
	ReleaseAction();
	ASSERT_TRUE(FramesUntil([this]() { return letGo.has_value(); }, 10));
	// the body's round trip rounds the last bits of the matrix
	for (int axis = 0; axis < 3; ++axis)
	{
		EXPECT_NEAR(letGo->position[axis], drawn.position[axis], 1e-3f) << "axis " << axis;
		for (int row = 0; row < 3; ++row)
		{
			EXPECT_NEAR(letGo->rotation[axis][row], drawn.rotation[axis][row], 1e-3f) << axis << ", " << row;
		}
	}
	EXPECT_EQ(letGo->scale, scale);
}

INSTANTIATE_TEST_SUITE_P(Data, HandReleaseWorld,
                         ::testing::Values(test::world::WorldData::Synthetic, test::world::WorldData::Game),
                         test::world::DataName);

// ---- hand_in_scripts.md --------------------------------------------------------------------------------------------

namespace
{
using ScriptHighlightWorld = test::world::WorldHarness;
} // namespace

// A script's highlight stands where the script put it, its altitude over the land kept, with the script's challenge, at
// its size, in its cell's list for the hand to find; only the four highlight rows make one. Its place is a whole map
// coordinate, where the map coordinates the script's point is stored in give the same place
TEST_P(ScriptHighlightWorld, AScriptsHighlightStandsAtItsPlaceWithItsChallengeInItsCell)
{
	const glm::vec3 point {1000.0f, k_Ground + 2.0f, 1200.0f};
	const auto thing = ecs::script_highlight::Create(point, 1, 77, 0.0f, 1.0f);
	ASSERT_TRUE(thing != entt::null);
	const auto& highlight = Registry().Get<const ecs::components::ScriptHighlight>(thing);
	EXPECT_EQ(highlight.infoIndex, 1u);
	EXPECT_EQ(highlight.scriptId, 77u);
	EXPECT_EQ(highlight.x, point.x);
	EXPECT_EQ(highlight.z, point.z);
	EXPECT_FLOAT_EQ(highlight.altitude, 2.0f);
	EXPECT_EQ(highlight.scale, 1.0f);
	const auto& transform = Registry().Get<const Transform>(thing);
	EXPECT_FLOAT_EQ(transform.position.y, point.y);
	EXPECT_EQ(transform.position.x, point.x);
	EXPECT_EQ(transform.position.z, point.z);
	EXPECT_EQ(transform.scale, glm::vec3(1.0f));
	EXPECT_TRUE(ecs::map_cells::IsObjectInMap(thing));
	EXPECT_EQ(ecs::script_highlight::Create(point, ecs::script_highlight::k_InfoCount, 78, 0.0f, 1.0f),
	          entt::entity {entt::null});
}

INSTANTIATE_TEST_SUITE_P(Data, ScriptHighlightWorld,
                         ::testing::Values(test::world::WorldData::Synthetic, test::world::WorldData::Game),
                         test::world::DataName);
