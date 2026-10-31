/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Scaffolds, building sites and buildings, played in a small world of the game's own systems and data: a Celtic town
// of the first player on dry land, its buildings made by the game's own archetypes from their models. A scaffold held
// over the town tries its building at eight angles an eighth of a turn apart from a random one and keeps the
// first that fits; where nothing fits it puffs once and shows no ghost; put down gently it becomes the site of the
// building the town wants, and the villagers around get its reaction; when that building is destroyed unfinished its
// scaffold makes a fresh site at once; a script's SET_ACTIVE builds it where it stands; and far from every town, a
// scaffold of five founds a new town of its home town's tribe, with the forests around its wood store. On synthetic
// data the rows are info.dat's for the buildings used, and the models boxes; on the game's data, info.dat's own and its
// models. Every expected value is the original's (docs/wiki/world/building-sites.md#scaffold-details,
// docs/wiki/project/progress/building/workshop_and_scaffolds.md).

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <entt/entity/entity.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Audio/AudioManagerNoOp.h"
#include "CHLApi.h"
#include "Common/GameRandomTesting.h"
#include "ECS/Abodes.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/TownArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Scaffolds.h"
#include "ECS/Systems/Implementations/TownDesireSystem.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownPlacement.h"
#include "ECS/Trees.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "support/ParticleFakes.h"
#include "support/RestoreService.h"
#include "support/VillagerWorld.h"

using namespace openblack;
using ecs::components::Abode;
using ecs::components::Mesh;
using ecs::components::Scaffold;
using ecs::components::Town;
using ecs::components::TownInfluence;
using ecs::components::Transform;
using ecs::components::Tree;
namespace scaffolds = ecs::scaffolds;
namespace sites = ecs::building_sites;
namespace reactions = ecs::effects::reactions;

namespace
{
/// The town: its number, and where it stands
constexpr uint32_t k_TownId = 0;
const glm::vec2 k_TownAt {1000.0f, 1200.0f};
/// Where the scaffolds stand by the town: the middle of a map cell, 7 m from the town
const glm::vec2 k_At {1005.0f, 1205.0f};
/// The middle of the map cell diagonally next to it, 14 m away
const glm::vec2 k_Beside {1015.0f, 1215.0f};
/// Far from the town: over 400 m away, the middle of a map cell
const glm::vec2 k_Away {1305.0f, 1505.0f};
/// The buildings used: the Celtic hut (ABODE_A), the Celtic tent (ABODE_B) and the Celtic village centre
constexpr auto k_Hut = AbodeInfo::CelticHut;
constexpr auto k_Tent = AbodeInfo::CelticTent;
constexpr auto k_Centre = AbodeInfo::CelticTownCentre;
/// The original's first random angle (up to 2 pi) from a new campaign's streams, and its step of an eighth of a turn
constexpr float k_FirstAngle = std::bit_cast<float>(0x408A11D0u);
constexpr float k_EighthTurn = std::bit_cast<float>(0x3F490FDBu);
/// The script number every town a scaffold founds is given
constexpr uint32_t k_FoundedTownScriptId = 0xABA52;
/// The "no building here" puff
constexpr auto k_NoBuildingPuff = static_cast<SpotVisualType>(4);

/// The spot visuals started, with where
class RecordedSpots final: public test::InertParticleSystem
{
public:
	entt::entity StartSpotVisual(SpotVisualType type, glm::vec3 position, std::optional<int> /*turns*/, entt::entity /*owner*/,
	                             float /*magnitude*/) override
	{
		started.emplace_back(type, position);
		return entt::null;
	}

	std::vector<std::pair<SpotVisualType, glm::vec3>> started;
};

class ScaffoldWorld: public test::world::VillagerWorld
{
protected:
	void SetUp() override
	{
		VillagerWorld::SetUp();
		if (IsSkipped() || HasFatalFailure())
		{
			return;
		}
		// the town desires, the sounds the scaffold plays (none heard) and the puffs it starts
		Locator::townDesireSystem::emplace<ecs::systems::TownDesireSystem>();
		Locator::audio::emplace<audio::AudioManagerNoOp>();
		_spots = &static_cast<RecordedSpots&>(Locator::particleSystem::emplace<RecordedSpots>());
		game_clock::SetTurn(0);
		if (!GameData())
		{
			FillSyntheticRows();
		}
		// on synthetic data the scaffold is a box a metre across, the hut a long box 40 m by 3 m, the tent a box 2 m
		// across and the village centre a box 16 m across
		LoadMesh(Info().scaffold.meshId, glm::vec3(-0.5f, 0.0f, -0.5f), glm::vec3(0.5f, 1.0f, 0.5f));
		LoadMesh(Info().mobileObject.at(static_cast<size_t>(MobileObjectInfo::OldScaffold)).meshId,
		         glm::vec3(-0.5f, 0.0f, -0.5f), glm::vec3(0.5f, 1.0f, 0.5f));
		LoadMesh(Row(k_Hut).meshId, glm::vec3(-20.0f, 0.0f, -1.5f), glm::vec3(20.0f, 4.0f, 1.5f));
		LoadMesh(Row(k_Tent).meshId, glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(1.0f, 3.0f, 1.0f));
		LoadMesh(Row(k_Centre).meshId, glm::vec3(-8.0f, 0.0f, -8.0f), glm::vec3(8.0f, 6.0f, 8.0f));
		_town = ecs::archetypes::TownArchetype::Create(static_cast<int>(k_TownId), Ground(k_TownAt), PlayerNames::PLAYER_ONE,
		                                               Tribe::CELTIC);
	}

	void TearDown() override
	{
		game_clock::SetTurn(0);
		VillagerWorld::TearDown();
	}

	/// The rows the scenes read, as info.dat has them: the Celtic hut, tent and village centre, the scaffold a mobile
	/// object, its reaction, the town's forest reach and a tree's wood; the town flies no desire flags
	void FillSyntheticRows() const
	{
		auto& info = Info();
		const auto building = [&info](AbodeInfo row, AbodeType type, AbodeNumber number, MeshId mesh, uint32_t required,
		                              float desire) {
			auto& abode = info.abode.at(static_cast<size_t>(row));
			abode.type = ObjectType::Abode;
			abode.abodeType = type;
			abode.abodeNumber = number;
			abode.tribeType = Tribe::CELTIC;
			abode.meshId = mesh;
			abode.scaffoldsRequired = required;
			abode.maxVillagerNeededToBuild = 6;
			abode.desireToBeBuilt = desire;
		};
		building(k_Hut, AbodeType::LivingQuarters, AbodeNumber::A, MeshId::BuildingCeltic1, 1, 0.25f);
		building(k_Tent, AbodeType::LivingQuarters, AbodeNumber::B, MeshId::BuildingCeltic2, 1, 0.25f);
		building(k_Centre, AbodeType::TownCentre, AbodeNumber::TownCentre, MeshId::BuildingCelticVillageCentre, 5, 1.0f);
		info.mobileObject.at(static_cast<size_t>(MobileObjectInfo::OldScaffold)).type = ObjectType::MobileObject;
		auto& react = info.reaction.at(static_cast<size_t>(openblack::Reaction::ReactToScaffold));
		react.maxReactionDistance = 60.0f;
		react.whetherReactionGrows = 0;
		info.town.maxDistanceForTownForest = 250.0f;
		info.tree.at(static_cast<size_t>(TreeInfo::Beech)).woodValue = 100;
		for (auto& desire : info.townDesire)
		{
			desire.worshipSiteSlot = -1;
		}
	}

	[[nodiscard]] const GAbodeInfo& Row(AbodeInfo row) const { return Info().abode.at(static_cast<size_t>(row)); }
	[[nodiscard]] static glm::vec3 Ground(glm::vec2 at) { return {at.x, k_Ground, at.y}; }
	[[nodiscard]] static map_coords::MapCoords CoordsAt(glm::vec2 at) { return map_coords::FromWorld(nullptr, Ground(at)); }
	[[nodiscard]] static Scaffold& Of(entt::entity scaffold) { return Registry().Get<Scaffold>(scaffold); }

	/// A scaffold standing for `value` scaffolds at the place, of the town (or none), owned by no workshop
	[[nodiscard]] static entt::entity MakeScaffold(glm::vec2 at, entt::entity town, uint32_t value)
	{
		const auto scaffold = scaffolds::Create(CoordsAt(at), town, entt::null, 0.0f, 1.0f);
		scaffolds::SetValue(scaffold, value);
		return scaffold;
	}

	/// A building of the town as its plans make one: under construction, built none
	[[nodiscard]] static entt::entity Building(AbodeInfo row, glm::vec2 at)
	{
		return ecs::archetypes::AbodeArchetype::Create(k_TownId, Ground(at), row, 0.0f, 1.0f, 0, 0, true);
	}

	/// The scaffold the first player's hand put down gently where it stands, as the physics ends it
	static entt::entity PutDownGently(entt::entity scaffold)
	{
		scaffolds::OnPickedUp(scaffold, PlayerNames::PLAYER_ONE);
		Of(scaffold).flags |= Scaffold::k_DroppedGently;
		return scaffolds::OnEndPhysics(scaffold, true);
	}

	/// The building of the scaffold's site
	[[nodiscard]] static entt::entity BuildingOf(entt::entity scaffold) { return sites::GetBuilding(Of(scaffold).site); }

	[[nodiscard]] static bool InSites(entt::entity town, entt::entity site)
	{
		const auto& list = sites::SitesOf(town);
		return std::ranges::find(list, site) != list.end();
	}

	/// Every town but the scene's own
	[[nodiscard]] std::vector<entt::entity> OtherTowns() const
	{
		std::vector<entt::entity> found;
		Registry().Each<const Town>([this, &found](entt::entity town, const Town& /*data*/) {
			if (town != _town)
			{
				found.push_back(town);
			}
		});
		return found;
	}

	/// A forest at the place with one beech in it, which has wood
	[[nodiscard]] static uint32_t ForestWithATree(glm::vec2 at)
	{
		const auto forest = ecs::CreateForest(0, Ground(at));
		const auto tree = Registry().Create();
		ecs::object_index::Assign(tree);
		Registry().Assign<Transform>(tree, Ground(at), glm::mat3(1.0f), glm::vec3(1.0f));
		Registry().Assign<Tree>(tree).type = TreeInfo::Beech;
		ecs::SetTreeForest(tree, forest);
		return forest;
	}

	/// The original keeps a place in its map coordinates, so two places are the same to it within one of their units
	/// in x and in z
	static void ExpectSamePlace(const glm::vec3& at, glm::vec2 expected)
	{
		EXPECT_NEAR(at.x, expected.x, map_coords::k_MetresPerFixed);
		EXPECT_NEAR(at.z, expected.y, map_coords::k_MetresPerFixed);
	}

	/// What a new building of the town's site is: of the row and the town, under construction, built none, standing at
	/// the place
	void ExpectNewBuilding(entt::entity building, AbodeInfo row, uint32_t townId, glm::vec2 at) const
	{
		ASSERT_TRUE(building != entt::null);
		ASSERT_TRUE(Registry().AllOf<Abode>(building));
		const auto& abode = Registry().Get<const Abode>(building);
		EXPECT_EQ(abode.info, row);
		EXPECT_EQ(abode.townId, townId);
		EXPECT_NE(abode.buildFlags & Abode::k_UnderConstruction, 0u);
		EXPECT_EQ(abode.percentBuilt, 0.0f);
		ExpectSamePlace(Registry().Get<const Transform>(building).position, at);
	}

	RecordedSpots* _spots {nullptr};
	entt::entity _town {entt::null};

private:
	test::RestoreService<Locator::townDesireSystem> _restoreDesires;
	test::RestoreService<Locator::audio> _restoreAudio;
	test::RestoreService<Locator::particleSystem> _restoreParticles;
};
} // namespace

// ---- the building on offer -------------------------------------------------------------------------------------------

// A scaffold offering only the hut tries it at a random angle first and then at seven more, each an eighth of a turn
// on, and keeps the first angle at which it fits: with the tent 14 m away in the way of the first, the next; on
// synthetic data the long hut fits at the second, as the original
// (docs/wiki/world/building-sites.md#the-building-a-scaffold-chooses, docs/wiki/world/building-sites.md#in-the-hand)
TEST_P(ScaffoldWorld, AScaffoldTriesItsBuildingAtEightAnglesFromARandomOneAndKeepsTheFirstThatFits)
{
	static_cast<void>(Building(k_Tent, k_Beside));
	const auto scaffold = MakeScaffold(k_At, _town, 1);
	scaffolds::SetScaffoldProperties(scaffold, static_cast<int32_t>(AbodeNumber::A), 1.0f, false);
	ASSERT_EQ(scaffolds::GetTownForBuilding(scaffold), _town);
	// the angles in turn, each the one before plus an eighth of a turn, and whether the hut fits at each
	const auto pos = ecs::object::MapCoordsOf(scaffold);
	const auto mesh = resources::HashIdentifier(Row(k_Hut).meshId);
	std::array<float, 8> angles {};
	std::optional<size_t> first;
	float angle = k_FirstAngle;
	for (size_t k = 0; k < angles.size(); ++k)
	{
		angles.at(k) = angle;
		if (!first.has_value() && ecs::town_placement::IsSuitableForFixedObject(pos, mesh, angle, 1.0f, entt::null))
		{
			first = k;
		}
		angle = angle + k_EighthTurn;
	}
	if (!GameData())
	{
		ASSERT_EQ(first, std::optional<size_t>(1));
	}
	// the random streams as a new campaign starts them, so that the first angle is the original's first draw
	Locator::gameRandom::emplace<GameRandomTesting>(
	    GameRandomSeeds {.synced = game_random::k_InitialSeed, .local = game_random::k_InitialSeed});
	const bool chosen = scaffolds::ChoosePlanAndUpdate(scaffold, _town);
	ASSERT_EQ(chosen, first.has_value());
	if (!first.has_value())
	{
		return;
	}
	const auto& plan = Of(scaffold).plan;
	ASSERT_TRUE(plan.has_value());
	EXPECT_EQ(plan->info, &Row(k_Hut));
	EXPECT_EQ(plan->yAngle, angles.at(*first));
	EXPECT_EQ(plan->scale, 1.0f);
}

// Held still where no building can go (on the tent), the scaffold offers none: it shows no ghost, and the "no building
// here" puff comes once where it is, its counter starting at one; the hand still on the next turn brings no other
// puff, the counter stepping on, as the original (docs/wiki/world/building-sites.md#in-the-hand)
TEST_P(ScaffoldWorld, WhereNoBuildingFitsAHeldScaffoldShowsNoGhostAndPuffsOnce)
{
	static_cast<void>(Building(k_Tent, k_At));
	const auto scaffold = MakeScaffold(k_At, _town, 1);
	scaffolds::OnPickedUp(scaffold, PlayerNames::PLAYER_ONE);
	const scaffolds::HandStatus still {.player = PlayerNames::PLAYER_ONE, .stillTurns = 1, .hand = Ground(k_At)};
	EXPECT_TRUE(scaffolds::ProcessInHand(scaffold, still));
	EXPECT_FALSE(scaffolds::HasPlan(scaffold, false));
	EXPECT_EQ(Of(scaffold).phantomMesh, 0u);
	ASSERT_EQ(_spots->started.size(), 1u);
	EXPECT_EQ(_spots->started.front().first, k_NoBuildingPuff);
	ExpectSamePlace(_spots->started.front().second, k_At);
	EXPECT_EQ(Of(scaffold).flags & Scaffold::k_FeedbackMask, Scaffold::k_FeedbackStep);
	EXPECT_TRUE(scaffolds::ProcessInHand(scaffold, still));
	EXPECT_EQ(_spots->started.size(), 1u);
	EXPECT_EQ(Of(scaffold).phantomMesh, 0u);
	EXPECT_EQ(Of(scaffold).flags & Scaffold::k_FeedbackMask, 2 * Scaffold::k_FeedbackStep);
}

// Put down gently where no building can go, the scaffold makes no site and the "no building here" puff comes where it
// lies, as the original (docs/wiki/world/building-sites.md#leaving-the-hand-and-the-physics)
TEST_P(ScaffoldWorld, AScaffoldPutDownWhereNoBuildingFitsMakesNoSiteAndPuffs)
{
	static_cast<void>(Building(k_Tent, k_At));
	const auto scaffold = MakeScaffold(k_At, _town, 1);
	EXPECT_EQ(PutDownGently(scaffold), scaffold);
	EXPECT_TRUE(Of(scaffold).site == entt::null);
	ASSERT_EQ(_spots->started.size(), 1u);
	EXPECT_EQ(_spots->started.front().first, k_NoBuildingPuff);
	ExpectSamePlace(_spots->started.front().second, k_At);
}

// ---- building ------------------------------------------------------------------------------------------------------

// Put down gently by the player in the town, a scaffold becomes the site of the building the town wants: the town's
// own site, its building the hut under construction where the scaffold stands, the scaffold at the head of the site's
// own and its ghost the building's model, no puff; the town is pulsed, and the scaffold is placed now, held by no one,
// as the original (docs/wiki/world/building-sites.md#building,
// docs/wiki/world/building-sites.md#leaving-the-hand-and-the-physics, docs/wiki/beings/towns.md#town-lists-and-queries)
TEST_P(ScaffoldWorld, AScaffoldThePlayerPutsDownInTheTownBecomesTheSiteOfItsBuilding)
{
	game_clock::SetTurn(321);
	const auto scaffold = MakeScaffold(k_At, _town, 1);
	EXPECT_EQ(PutDownGently(scaffold), scaffold);
	const auto site = Of(scaffold).site;
	ASSERT_TRUE(site != entt::null);
	EXPECT_TRUE(InSites(_town, site));
	const auto building = BuildingOf(scaffold);
	ExpectNewBuilding(building, k_Hut, k_TownId, k_At);
	ASSERT_FALSE(sites::ScaffoldsOf(site).empty());
	EXPECT_EQ(sites::ScaffoldsOf(site).front(), scaffold);
	EXPECT_EQ(Of(scaffold).phantomMesh, Registry().Get<const Mesh>(building).id);
	EXPECT_TRUE(_spots->started.empty());
	const auto& town = Registry().Get<const Town>(_town);
	EXPECT_EQ(town.buildPulse, 1u);
	EXPECT_EQ(town.buildPulsePrevious, 0u);
	EXPECT_EQ(Of(scaffold).placedTurn, 321u);
	EXPECT_FALSE(Of(scaffold).holder.has_value());
	EXPECT_EQ(Of(scaffold).flags & Scaffold::k_DroppedGently, 0);
}

// Planting the scaffold with the player's hand starts its reaction for the villagers around: a REACT_TO_SCAFFOLD of
// the scaffold for that player, reaching out to the 60 m of its info.dat row, as the original
// (docs/wiki/world/building-sites.md#building)
TEST_P(ScaffoldWorld, PlantingAScaffoldMakesTheVillagersAroundReactOutTo60Metres)
{
	const auto scaffold = MakeScaffold(k_At, _town, 1);
	ASSERT_EQ(PutDownGently(scaffold), scaffold);
	ASSERT_TRUE(Of(scaffold).site != entt::null);
	const auto id = reactions::GetReactionOfTypeInitiatedBy(scaffold, openblack::Reaction::ReactToScaffold);
	ASSERT_NE(id, 0u);
	const auto* reaction = reactions::Find(id);
	ASSERT_NE(reaction, nullptr);
	EXPECT_EQ(reaction->player, std::optional<PlayerNames>(PlayerNames::PLAYER_ONE));
	EXPECT_EQ(reaction->radius, 60.0f);
}

// When the unfinished building is destroyed, the head scaffold of its site makes a fresh site there at once: a new
// site of the town, a new hut under construction where the scaffold stands, the scaffold on it with the building's
// model as its ghost and placed now; the old building goes, as the original
// (docs/wiki/world/buildings.md#life-and-damage, docs/wiki/world/building-sites.md#script-commands-on-a-scaffold)
TEST_P(ScaffoldWorld, AnUnfinishedBuildingDestroyedLeavesItsScaffoldMakingAFreshSiteAtOnce)
{
	const auto scaffold = MakeScaffold(k_At, _town, 1);
	ASSERT_EQ(PutDownGently(scaffold), scaffold);
	const auto oldSite = Of(scaffold).site;
	const auto oldBuilding = BuildingOf(scaffold);
	ASSERT_TRUE(oldBuilding != entt::null);
	game_clock::SetTurn(77);
	// somebody lives in the town, so it stays when its only building goes
	Registry().Get<Town>(_town).stats.adults = 1;
	ecs::abodes::DestroyedByEffect(oldBuilding);
	const auto site = Of(scaffold).site;
	ASSERT_TRUE(site != entt::null);
	EXPECT_TRUE(site != oldSite);
	EXPECT_TRUE(InSites(_town, site));
	const auto building = BuildingOf(scaffold);
	EXPECT_TRUE(building != oldBuilding);
	ExpectNewBuilding(building, k_Hut, k_TownId, k_At);
	ASSERT_FALSE(sites::ScaffoldsOf(site).empty());
	EXPECT_EQ(sites::ScaffoldsOf(site).front(), scaffold);
	EXPECT_EQ(Of(scaffold).phantomMesh, Registry().Get<const Mesh>(building).id);
	EXPECT_EQ(Of(scaffold).placedTurn, 77u);
	EXPECT_FALSE(ecs::IsAvailable(oldBuilding));
}

// A script's SET_ACTIVE on a scaffold lying in the town builds it at once where it stands: active, its site and
// building are made, its ghost the building's model and placed now; inactive, nothing, as the original
// (docs/wiki/world/building-sites.md#script-commands-on-a-scaffold)
TEST_P(ScaffoldWorld, AScriptsSetActiveMakesAScaffoldBuildAtOnce)
{
	const test::RestoreService<Locator::vm> keepVm;
	Locator::vm::emplace<lhvm::LHVM>();
	chlapi::CHLApi api;
	const auto& table = api.GetFunctionsTable();
	const auto native =
	    std::ranges::find_if(table, [](const lhvm::NativeFunction& f) { return std::string_view(f.name) == "SET_ACTIVE"; });
	ASSERT_NE(native, table.end());
	ASSERT_EQ(native->stackIn, 2);
	const auto scaffold = MakeScaffold(k_At, _town, 1);
	ASSERT_NE(static_cast<uint32_t>(scaffold), 0u);
	// the on or off pushed first, then the object
	const auto setActive = [&native, scaffold](bool active) {
		auto& vm = Locator::vm::value();
		vm.Pushb(active);
		vm.Pusho(static_cast<uint32_t>(scaffold));
		native->impl();
	};
	game_clock::SetTurn(55);
	setActive(false);
	EXPECT_TRUE(Of(scaffold).site == entt::null);
	EXPECT_TRUE(sites::SitesOf(_town).empty());
	setActive(true);
	const auto site = Of(scaffold).site;
	ASSERT_TRUE(site != entt::null);
	EXPECT_TRUE(InSites(_town, site));
	const auto building = BuildingOf(scaffold);
	ExpectNewBuilding(building, k_Hut, k_TownId, k_At);
	EXPECT_EQ(Of(scaffold).phantomMesh, Registry().Get<const Mesh>(building).id);
	EXPECT_EQ(Of(scaffold).placedTurn, 55u);
}

// ---- founding a town -----------------------------------------------------------------------------------------------

// Away from every town, a scaffold of its home town offers the village centre of the home town's tribe once it stands
// for the village centre's five scaffolds: none for four; for five, the village centre at the first of the eight
// angles from none that fits, at its own size, a plan that founds a town, as the original
// (docs/wiki/world/building-sites.md#in-the-hand)
TEST_P(ScaffoldWorld, AwayFromEveryTownAScaffoldOfFiveOffersAVillageCentreThatFoundsATown)
{
	if (GameData())
	{
		EXPECT_EQ(Row(k_Centre).scaffoldsRequired, 5u);
	}
	const auto scaffold = MakeScaffold(k_Away, _town, 4);
	ASSERT_TRUE(scaffolds::GetTownForBuilding(scaffold) == entt::null);
	EXPECT_FALSE(scaffolds::ChoosePlanAndUpdate(scaffold, entt::null));
	EXPECT_FALSE(scaffolds::HasPlan(scaffold, false));
	scaffolds::SetValue(scaffold, 5);
	ASSERT_TRUE(scaffolds::ChoosePlanAndUpdate(scaffold, entt::null));
	const auto& plan = Of(scaffold).plan;
	ASSERT_TRUE(plan.has_value());
	EXPECT_EQ(plan->info, &Row(k_Centre));
	EXPECT_EQ(plan->yAngle, 0.0f);
	EXPECT_EQ(plan->scale, 1.0f);
	EXPECT_NE(Of(scaffold).flags & Scaffold::k_FoundsTown, 0);
}

// Planting the founding scaffold first makes the new town where it stands: of the player, of its home town's tribe,
// with the script number every founded town has and no influence until its centre is working, and with the forests
// that have wood within reach of its wood store (one 20 m away, not one 600 m away); then the village centre is the new
// town's site, as the original (docs/wiki/world/building-sites.md#building,
// docs/wiki/beings/towns.md#town-lists-and-queries, docs/wiki/world/trees.md#tree-details)
TEST_P(ScaffoldWorld, AFoundingVillageCentreFirstMakesTheNewTownWithItsForests)
{
	const auto near = ForestWithATree(k_Away + glm::vec2(20.0f, 0.0f));
	const auto far = ForestWithATree(k_Away + glm::vec2(600.0f, 0.0f));
	ASSERT_GT(ecs::ForestWood(near), 0.0f);
	ASSERT_GT(ecs::ForestWood(far), 0.0f);
	const auto scaffold = MakeScaffold(k_Away, _town, 5);
	ASSERT_TRUE(scaffolds::ChoosePlanAndUpdate(scaffold, entt::null));
	ASSERT_TRUE(scaffolds::BuildBuilding(scaffold, PlayerNames::PLAYER_ONE, false));
	const auto founded = OtherTowns();
	ASSERT_EQ(founded.size(), 1u);
	const auto town = founded.front();
	const auto& data = Registry().Get<const Town>(town);
	EXPECT_EQ(data.owner, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(Registry().Get<const Tribe>(town), Tribe::CELTIC);
	EXPECT_EQ(data.scriptId, std::optional<uint32_t>(k_FoundedTownScriptId));
	ASSERT_TRUE(Registry().AllOf<TownInfluence>(town));
	EXPECT_TRUE(Registry().Get<const TownInfluence>(town).noInfluence);
	ExpectSamePlace(Registry().Get<const Transform>(town).position, k_Away);
	const auto forests = ecs::TownForests(data.id);
	EXPECT_NE(std::ranges::find(forests, near), forests.end());
	EXPECT_EQ(std::ranges::find(forests, far), forests.end());
	const auto site = Of(scaffold).site;
	ASSERT_TRUE(site != entt::null);
	EXPECT_TRUE(InSites(town, site));
	EXPECT_FALSE(InSites(_town, site));
	ExpectNewBuilding(BuildingOf(scaffold), k_Centre, data.id, k_Away);
}

// The founded town measures its forests from its wood store, not from where the scaffold stands: having no storage
// pit, it makes its temporary wood store by its gathering point (10 to 20 m from the town, then 5 m along angle 0) and
// takes every forest with wood whose centre is within the town's forest reach of that store's nearest edge to the town.
// A ring of forests 1 m inside and 1 m outside that reach of the scaffold tells the two points apart, as the original
// (docs/wiki/world/building-sites.md#building, docs/wiki/world/trees.md#tree-details)
TEST_P(ScaffoldWorld, AFoundedTownMeasuresItsForestsFromItsWoodStore)
{
	constexpr int k_Directions = 32;
	const float reach = Info().town.maxDistanceForTownForest;
	ASSERT_GT(reach, 1.0f);
	std::vector<uint32_t> ring;
	for (int i = 0; i < k_Directions; ++i)
	{
		const float angle = glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(k_Directions);
		const glm::vec2 direction {std::cos(angle), std::sin(angle)};
		for (const float distance : {reach - 1.0f, reach + 1.0f})
		{
			ring.push_back(ForestWithATree(k_Away + direction * distance));
		}
	}
	const auto scaffold = MakeScaffold(k_Away, _town, 5);
	ASSERT_TRUE(scaffolds::ChoosePlanAndUpdate(scaffold, entt::null));
	ASSERT_TRUE(scaffolds::BuildBuilding(scaffold, PlayerNames::PLAYER_ONE, false));
	const auto founded = OtherTowns();
	ASSERT_EQ(founded.size(), 1u);
	const auto town = founded.front();
	const auto& data = Registry().Get<const Town>(town);
	// the founding made the new town's temporary wood store
	const auto store = data.temporaryPots.at(static_cast<size_t>(ResourceType::Wood));
	ASSERT_TRUE(store != entt::null);
	const auto from = map_coords::ToMetres(ecs::object::GetNearestEdgeToPos(store, ecs::object::MapCoordsOf(town)));
	const auto taken = ecs::TownForests(data.id);
	int onlyByTheStore = 0;
	int onlyByTheScaffold = 0;
	for (const auto forest : ring)
	{
		const auto centre = ecs::ForestCentre(forest);
		const glm::vec2 at {centre.x, centre.z};
		const bool byTheStore = glm::distance(at, from) < reach;
		const bool byTheScaffold = glm::distance(at, k_Away) < reach;
		onlyByTheStore += byTheStore && !byTheScaffold ? 1 : 0;
		onlyByTheScaffold += byTheScaffold && !byTheStore ? 1 : 0;
		EXPECT_EQ(std::ranges::find(taken, forest) != taken.end(), byTheStore) << "forest " << forest;
	}
	// the store stands far enough from the scaffold for the ring to tell the two points apart, both ways
	EXPECT_GT(onlyByTheStore, 0);
	EXPECT_GT(onlyByTheScaffold, 0);
}

INSTANTIATE_TEST_SUITE_P(Data, ScaffoldWorld,
                         ::testing::Values(test::world::WorldData::Synthetic, test::world::WorldData::Game),
                         test::world::DataName);
