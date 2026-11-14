/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A creature's mind casting miracles: whether it may try one, what it casts for an action, and how it goes near, gets
// away from and turns to face what it casts at. The creature's turn calls them (CreatureMindSystem).

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/LandAvoid.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureBodyActivity.h"
#include "Creature/CreatureCastAgenda.h"
#include "Creature/CreatureCastGesture.h"
#include "Creature/CreatureCastMoves.h"
#include "Creature/CreatureFollowUp.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSize.h"
#include "Creature/CreatureSpellCasting.h"
#include "Creature/CreatureSpellMind.h"
#include "Creature/CreatureThrow.h"
#include "Creature/CreatureTurnPose.h"
#include "Creature/CreatureWatching.h"
#include "Creature/LeashRules.h"
#include "CreatureMindSystem.h"
#include "CreatureMindSystemDetail.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCasting.h"
#include "ECS/Components/CreatureClearAreaReadout.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureTurnPose.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/ShowNeeds.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/CreaturePose.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/MapCollide.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/RoutePlanWorld.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/SpellCreator.h"
#include "Magic/Gestures/GestureIcons.h"
#include "Magic/Gestures/GestureShapes.h"
#include "Magic/MagicTables.h"
#include "ObjectMeasures.h"
#include "Particles/ParticleTypes.h"
#include "Particles/SpellLink.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
namespace cast_moves = openblack::creature_cast_moves;
using Kind = creature_mind::Movement::Kind;
/// Backing away to throw where it can't stand, it looks this far round for the nearest place it can, which is only any
/// use nearer than the second
constexpr float k_ThrowStandSearch = 1000.0f;
constexpr float k_ThrowStandReach = 30.0f;

constexpr float k_TurnsPerSecond = 10.0f;
/// Getting stuck holds back the desire it went for this many seconds
constexpr float k_StuckSuppressSeconds = 10.0f;
/// Walking up to something, it stops no nearer than this short of where it stands, and its furthest is at least this
/// past its nearest
constexpr float k_NearMargin = 0.05f;
constexpr float k_FarMargin = 0.001f;
/// The miracles a creature casts are numbered from the first to the last magic type
constexpr uint32_t k_LastCreatureMagic = 41;
/// A creature tries a power-up miracle only once grown past this stage
constexpr uint32_t k_PowerUpPhase = 7;

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

glm::vec2 Flat(const glm::vec3& point)
{
	return {point.x, point.z};
}

/// Whether the collide data a thing has in the map keeps a cell from being clear: each circle of its row, or its one
/// circle when it has no row
bool ShapeBlocksCell(const ecs::map_collide::Shape& shape, int32_t x, int32_t z)
{
	if (shape.children.empty())
	{
		return cast_moves::BlocksCell({.centre = shape.centre, .radius = shape.radius}, x, z);
	}
	for (size_t i = 0; i < shape.children.size(); ++i)
	{
		if (cast_moves::BlocksCell({.centre = shape.children[i], .radius = ecs::map_collide::ChildRadius(shape, i)}, x, z))
		{
			return true;
		}
	}
	return false;
}

/// How many times a creature must see a miracle to learn it, and how often it has
struct Sightings
{
	float seen;
	float needed;
	bool known;
};
std::optional<Sightings> SightingsOf(const CreatureMindState& mind, const creature_mind_tables::Tables& tables,
                                     CreatureType species, uint32_t magicType)
{
	if (!mind.learnt.has_value() || magicType >= tables.miracles.size())
	{
		return std::nullopt;
	}
	const auto& knowledge = mind.learnt->knowledge;
	if (magicType >= knowledge.miraclesSeen.size() || magicType >= knowledge.miraclesKnown.size())
	{
		return std::nullopt;
	}
	return Sightings {.seen = static_cast<float>(knowledge.miraclesSeen[magicType].count),
	                  .needed =
	                      creature_watching::TimesNeeded(tables.miracles[magicType].timesToSee,
	                                                     creature_mind_tables::MiracleMultiplier(creature::InfoRow(species))),
	                  .known = knowledge.miraclesKnown[magicType]};
}

/// The executor of an action of the game's table, none without the tables or for an action nothing carries out
const creature_plan_actions::Executor* ExecutorOf(const creature_mind_tables::Tables* tables, uint32_t action)
{
	return tables != nullptr && action < tables->actions.size() ? creature_plan_actions::For(tables->actions[action].name)
	                                                            : nullptr;
}

/// Whether the creature's body can pay for making a miracle, as its body pays for it
bool CanPayFor(entt::entity creature, MagicType type)
{
	return magic::creator::CreatureCanPayFor(creature,
	                                         magic::GetMagicEffectInfo(Locator::infoConstants::value(), type).costToCreate);
}

/// The miracle a creature casts to impress, none when it knows none of them. The turn is noted for every creature: the
/// next creature to impress picks a miracle that no creature cast for longer
std::optional<uint32_t> ImpressiveMiracleFor(entt::entity creature, const creature_mind_tables::Tables& tables)
{
	auto& registry = Entities();
	const auto* mind = std::as_const(registry).TryGet<const CreatureMindState>(creature);
	const auto* body = std::as_const(registry).TryGet<const Creature>(creature);
	if (mind == nullptr || body == nullptr)
	{
		return std::nullopt;
	}
	std::array<bool, creature_mind::k_ImpressiveMiracles.size()> known {};
	std::ranges::transform(creature_mind::k_ImpressiveMiracles, known.begin(), [&](uint32_t type) {
		const auto sightings = SightingsOf(*mind, tables, body->species, type);
		return sightings.has_value() && sightings->known;
	});
	auto& castTurn = registry.Context().impressiveMiracleCastTurn;
	const auto picked = creature_mind::PickImpressiveMiracle(known, castTurn);
	if (!picked.has_value())
	{
		return std::nullopt;
	}
	castTurn.at(*picked) = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
	return creature_mind::k_ImpressiveMiracles.at(*picked);
}

/// Walks the creature on towards what it goes near; false only where it can't go at all. It stops short of the thing
/// by what it keeps clear of and its own radius, and no further than the distance more. While it plays an action or
/// turns on the spot it can't set off, and goes on as it is
/// A move of a cast goes on at the speed that the creature got last, unless the move asks for the run speed
CreatureLocomotionSystemInterface::Pace KeptOrRun(const CreatureCasting& casting)
{
	return casting.run ? CreatureLocomotionSystemInterface::Pace::Run : CreatureLocomotionSystemInterface::Pace::Keep;
}

bool GoNear(entt::entity creature, CreatureCasting& casting, bool bodyActing)
{
	auto& registry = Entities();
	const auto at = object_measures::PositionOf(registry, casting.target);
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (!at.has_value() || transform == nullptr || !Locator::creatureLocomotionSystem::has_value())
	{
		return false;
	}
	if (bodyActing)
	{
		return true;
	}
	const float clear =
	    object_measures::RoutePlanRadius(registry, casting.target, creature) + object_measures::TwoDRadius(registry, creature);
	const auto target = Flat(*at);
	const float distance = glm::distance(Flat(transform->position), target);
	const float near = std::min(clear, distance - k_NearMargin);
	const float far = std::max(casting.keep + clear, near + k_FarMargin);
	// The game sets no pace here: it goes on at the one it was last asked for. Asked again for the same place while on
	// its way, the walk goes on as it is
	const auto result = Locator::creatureLocomotionSystem::value().MoveTo(creature, target, KeptOrRun(casting), near, far);
	if (result == CreatureLocomotionSystemInterface::MoveResult::InvalidDestination)
	{
		return false;
	}
	if (result == CreatureLocomotionSystemInterface::MoveResult::Started)
	{
		casting.destination = target;
	}
	return true;
}

/// Where the creature's head is, as the game's logic poses its body for the turn: its species' head bone; none without
/// the bone or a pose
std::optional<glm::vec3> HeadOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* pose = registry.TryGet<const CreatureTurnPose>(creature);
	if (body == nullptr || pose == nullptr || !Locator::resources::has_value())
	{
		return std::nullopt;
	}
	auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto id = creature::GetRigId(body->species);
	const auto* rig = rigs.Contains(id) ? &*rigs.Handle(id) : nullptr;
	if (rig == nullptr || !rig->actionPoints.has_value())
	{
		return std::nullopt;
	}
	const auto bones = creature_turn_pose::Safe(*pose);
	if (rig->actionPoints->head >= bones.size())
	{
		return std::nullopt;
	}
	return glm::vec3(bones[rig->actionPoints->head][3]);
}

/// Where the creature points for the share of a gesture's shape drawn, if it has a head to point from
std::optional<glm::vec3> GesturePointOf(const ecs::Registry& registry, entt::entity creature, uint32_t gesture, float progress)
{
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto head = HeadOf(registry, creature);
	if (transform == nullptr || !head.has_value())
	{
		return std::nullopt;
	}
	const auto& shape = magic::gestures::ShapeOf(static_cast<magic::gestures::Gesture>(gesture));
	return creature_cast_gesture::PointOf(magic::gestures::ShapePointAt(shape.points, progress),
	                                      object_measures::TwoDRadius(registry, creature),
	                                      ecs::creature_pose::BodyYaw(registry, creature, transform->rotation), *head);
}

/// How many times as long as the circle a gesture's shape is
float GestureLengthRatio(uint32_t gesture)
{
	return magic::gestures::ShapeOf(static_cast<magic::gestures::Gesture>(gesture)).length /
	       magic::gestures::ShapeOf(magic::gestures::k_Circle).length;
}

/// Whether the creature's mind still carries out a gesture step, the step that drawing is for
bool DrawsAGestureStep(const ecs::Registry& registry, entt::entity creature)
{
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	return mind != nullptr && mind->idle.stepStarted && mind->idle.step < mind->idle.agenda.size() &&
	       mind->idle.agenda[mind->idle.step].kind == creature_mind::Step::Kind::Gesture;
}

/// The glowing chain that follows the hand: where its hand is while its arm points, else at its feet making nothing
void FeedChain(entt::entity creature, const CreatureCasting& casting)
{
	if (casting.gestureChain == 0 || casting.gestureChainClosing || !Locator::particleSystem::has_value() ||
	    !Locator::creatureObjectActionSystem::has_value())
	{
		return;
	}
	const auto hand = Locator::creatureObjectActionSystem::value().GetPointingHand(creature);
	const auto feet = Entities().Get<const Transform>(creature).position;
	Locator::particleSystem::value().SetProcessInfo(casting.gestureChain,
	                                                {.handPos = hand.value_or(feet), .enabled = hand.has_value()});
}

/// The chain stops making anything and fades out as its file has it, still stepped by the frame until it has gone
void StopChain(CreatureCasting& casting)
{
	if (casting.gestureChain == 0 || casting.gestureChainClosing || !Locator::particleSystem::has_value())
	{
		return;
	}
	auto& effects = Locator::particleSystem::value();
	effects.SetProcessInfo(casting.gestureChain, {.enabled = false});
	effects.CloseDown(casting.gestureChain);
	casting.gestureChainClosing = true;
}

/// The chain starts at the shape's start as the arm starts to point: as big as the creature is tall, in its player's
/// colour. A chain the last drawing left goes first
void StartChain(entt::entity creature, CreatureCasting& casting, glm::vec3 start)
{
	if (!Locator::particleSystem::has_value())
	{
		return;
	}
	auto& effects = Locator::particleSystem::value();
	if (casting.gestureChain != 0)
	{
		effects.Delete(casting.gestureChain);
	}
	const auto& registry = Entities();
	const auto& body = std::as_const(registry).Get<const Creature>(creature);
	const float size = creature_morph::ClampScale(ecs::creature_pose::DrawnSize(std::as_const(registry), creature));
	// Its owner steps it as the body is drawn (CreatureAnimationSystem::Update), so the turn leaves it alone
	casting.gestureChain = effects.StartForSpell(openblack::particles::ParticleTypeFile(ParticleType::CreatureGesture), start,
	                                             glm::vec3(0.0f), creature_size::k_HeightOfSizeOne * size, nullptr, false);
	casting.gestureChainClosing = false;
	effects.SetPlayer(casting.gestureChain, static_cast<int>(body.owner));
	effects.SetProcessInfo(casting.gestureChain, {.handPos = start, .enabled = false});
}

/// One turn of drawing a gesture: the start once the body is free, the drawing along the shape, and the end once the
/// body is free again
void StepGesture(entt::entity creature, CreatureCasting& casting, bool animating)
{
	auto& registry = Entities();
	const auto fail = [&] { casting.outcome = CreatureCasting::Outcome::Failed; };
	if (!Locator::creatureObjectActionSystem::has_value())
	{
		fail();
		return;
	}
	auto& hands = Locator::creatureObjectActionSystem::value();
	// The drawing goes with its step: an agenda given up or done with leaves it, its point stopped
	if (!DrawsAGestureStep(registry, creature))
	{
		if (casting.phase == 1)
		{
			hands.StopPointing(creature);
		}
		StopChain(casting);
		casting.move = CreatureCasting::Move::None;
		casting.gesturePointing = false;
		return;
	}
	// The step's turns count on before each of its turns
	++casting.turns;
	const bool bodyBusy =
	    creature_body::IsPerformingAction({.playing = animating,
	                                       .moving = Locator::creatureLocomotionSystem::value().IsMoving(creature),
	                                       .handling = hands.GetProgress(creature).has_value() || hands.IsCatching(creature)});
	switch (casting.phase)
	{
	case 0:
	{
		if (bodyBusy)
		{
			return;
		}
		casting.gestureProgress = 0.0f;
		const auto point = GesturePointOf(registry, creature, casting.gesture, casting.gestureProgress);
		if (!point.has_value())
		{
			fail();
			return;
		}
		casting.gesturePoint = *point;
		hands.PointUntilStopped(creature, *point);
		StartChain(creature, casting, *point);
		casting.phase = 1;
		return;
	}
	case 1:
	{
		const bool pointing = hands.IsPointing(creature);
		const float share = casting.gesture <= creature_cast_gesture::k_LastGesture
		                        ? creature_cast_gesture::ShareOfATurn(GestureLengthRatio(casting.gesture))
		                        : 0.0f;
		const auto outcome =
		    creature_cast_gesture::Turn(casting.gestureProgress, casting.turns, casting.gesture, pointing, share);
		if (outcome == creature_cast_gesture::Outcome::Waiting)
		{
			return;
		}
		if (outcome == creature_cast_gesture::Outcome::Failed)
		{
			StopChain(casting);
			fail();
			return;
		}
		if (const auto point = GesturePointOf(registry, creature, casting.gesture, casting.gestureProgress))
		{
			casting.gesturePoint = *point;
		}
		hands.UpdatePointPoint(creature, casting.gesturePoint);
		if (outcome == creature_cast_gesture::Outcome::Done)
		{
			hands.StopPointing(creature);
			StopChain(casting);
			casting.phase = 2;
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} has drawn gesture {} in the air", entt::to_integral(creature),
			                   casting.gesture);
			return;
		}
		// The chain follows the hand while the arm points, else it stays at the creature
		casting.gesturePointing = pointing;
		FeedChain(creature, casting);
		return;
	}
	default:
		casting.gesturePointing = false;
		if (!bodyBusy)
		{
			casting.outcome = CreatureCasting::Outcome::Done;
		}
		return;
	}
}
} // namespace

std::optional<glm::vec3> mind_detail::HeadPosition(const ecs::Registry& registry, entt::entity creature)
{
	return HeadOf(registry, creature);
}

bool mind_detail::CellBlocked(const ecs::Registry& registry, entt::entity creature, int32_t x, int32_t z, bool livingBlocks)
{
	return BlockerOf(registry, creature, x, z, livingBlocks).has_value();
}

std::optional<ClearCellBlocker> mind_detail::BlockerOf(const ecs::Registry& registry, entt::entity creature, int32_t x,
                                                       int32_t z, bool livingBlocks)
{
	if (!Locator::entitiesMap::has_value() || x < 0 || z < 0)
	{
		return std::nullopt;
	}
	const auto& map = Locator::entitiesMap::value();
	const ecs::MapInterface::CellId cell {static_cast<uint16_t>(x), static_cast<uint16_t>(z)};
	const auto blocks = [&](entt::entity entity) -> std::optional<ClearCellBlock> {
		if (!registry.Valid(entity))
		{
			return std::nullopt;
		}
		// A temple's heart takes the cell outright
		if (registry.AllOf<Temple>(entity))
		{
			return ClearCellBlock::TempleHeart;
		}
		// A tree, a field, or a thing the creature must go round, by the shape it keeps clear: the collide data the map
		// built for it, which only things that stay put have (a tree a little circle where it stands, the others their
		// model's shape; a big forest none). Villagers, animals, creatures, mobile objects, pots and street lanterns
		// have none, so they never keep a cell clear by a shape
		if (registry.AnyOf<Tree, Field>(entity) || ecs::route_plan_world::CreatureMustAvoid(entity, creature))
		{
			if (const auto* shape = ecs::map_cells::CollideDataOf(entity); shape != nullptr && ShapeBlocksCell(*shape, x, z))
			{
				return ClearCellBlock::Shape;
			}
		}
		// Asked for a place with nothing living in it, any living thing there takes the cell
		if (livingBlocks && registry.AnyOf<Villager, Animal, Creature>(entity))
		{
			return ClearCellBlock::Living;
		}
		return std::nullopt;
	};
	// The things that stay put first, then the things that move: the first that keeps the cell from being clear
	for (const auto entity : map.GetFixedInGridCell(cell))
	{
		if (const auto why = blocks(entity); why.has_value())
		{
			return ClearCellBlocker {.why = *why, .by = entity};
		}
	}
	for (const auto entity : map.GetMobileInGridCell(cell))
	{
		if (const auto why = blocks(entity); why.has_value())
		{
			return ClearCellBlocker {.why = *why, .by = entity};
		}
	}
	return std::nullopt;
}

std::optional<uint32_t> mind_detail::CastMagicOf(uint32_t action)
{
	const auto& info = Locator::infoConstants::value();
	if (action >= info.creatureAction.size())
	{
		return std::nullopt;
	}
	const auto magic = info.creatureAction.at(action).magicType;
	if (magic == 0 || magic > k_LastCreatureMagic)
	{
		return std::nullopt;
	}
	return magic;
}

std::optional<std::pair<float, float>> mind_detail::MiracleSightings(const CreatureMindState& mind,
                                                                     const creature_mind_tables::Tables& tables,
                                                                     CreatureType species, MagicType magic)
{
	const auto sightings = SightingsOf(mind, tables, species, static_cast<uint32_t>(magic));
	if (!sightings.has_value())
	{
		return std::nullopt;
	}
	return std::pair {sightings->seen, sightings->needed};
}

std::vector<creature_mind::AmusingSpell> mind_detail::AmusingSpells(entt::entity creature, const CreatureMindState& mind,
                                                                    const creature_mind_tables::Tables& tables)
{
	std::vector<creature_mind::AmusingSpell> spells;
	const auto* body = Entities().TryGet<const Creature>(creature);
	if (body == nullptr || !Locator::infoConstants::has_value())
	{
		return spells;
	}
	const auto& info = Locator::infoConstants::value();
	for (auto type = creature_mind::k_FirstAmusingSpell; type <= creature_mind::k_LastAmusingSpell; ++type)
	{
		const auto sightings = SightingsOf(mind, tables, body->species, type);
		const auto magic = static_cast<MagicType>(type);
		if (sightings.has_value() && sightings->known && CanPayFor(creature, magic))
		{
			spells.push_back({.magicType = type, .gesture = static_cast<uint32_t>(magic::GetGestureOfMagicInfo(info, magic))});
		}
	}
	return spells;
}

namespace
{
/// Whether an action of the game's table is the spell that a creature casts to amuse another creature
bool IsAmusingCast(const creature_mind_tables::Tables* tables, uint32_t action)
{
	if (tables == nullptr || action >= tables->actions.size())
	{
		return false;
	}
	const auto* executor = creature_plan_actions::For(tables->actions[action].name);
	return executor != nullptr && executor->build == creature_plan_actions::Build::CastAmusing;
}
} // namespace

bool mind_detail::MayCast(entt::entity creature, const CreatureMindState& mind, const creature_mind_tables::Tables* tables,
                          uint32_t action, bool powerUp)
{
	// To impress, it looks at all the miracles that impress, not at the action's own miracle: it must know one, be able
	// to pay for it and have seen it more than half the times it needs to
	const auto* executor = ExecutorOf(tables, action);
	if (executor != nullptr && executor->build == creature_plan_actions::Build::CastToImpress)
	{
		const auto* body = Entities().TryGet<const Creature>(creature);
		if (body == nullptr)
		{
			return false;
		}
		std::array<creature_mind::ImpressiveMiracleState, creature_mind::k_ImpressMiraclesItMayKnow.size()> states {};
		std::ranges::transform(creature_mind::k_ImpressMiraclesItMayKnow, states.begin(), [&](uint32_t type) {
			const auto sightings = SightingsOf(mind, *tables, body->species, type);
			return creature_mind::ImpressiveMiracleState {
			    .known = sightings.has_value() && sightings->known,
			    .canPay = CanPayFor(creature, static_cast<MagicType>(type)),
			    .seen = sightings.has_value() ? sightings->seen : 0.0f,
			    .needed = sightings.has_value() ? sightings->needed : 0.0f,
			};
		});
		return creature_mind::CanCastToImpress(states);
	}
	// To amuse, it needs a spell on creatures that it knows and can pay for
	if (IsAmusingCast(tables, action))
	{
		return !AmusingSpells(creature, mind, *tables).empty();
	}
	const auto magic = CastMagicOf(action);
	const auto* body = Entities().TryGet<const Creature>(creature);
	if (tables == nullptr || !magic.has_value() || body == nullptr || (powerUp && mind.developmentPhase <= k_PowerUpPhase))
	{
		return false;
	}
	// The game goes by the sightings alone. It also has a test of whether the player has the miracle, behind a switch
	// that is off as the game loads; that test is left out
	const auto sightings = SightingsOf(mind, *tables, body->species, *magic);
	if (!sightings.has_value() || !creature_spell_casting::MayTry(sightings->seen, sightings->needed))
	{
		return false;
	}
	return CanPayFor(creature, static_cast<MagicType>(*magic));
}

std::optional<creature_plan_actions::CastInfo> mind_detail::CastInfoFor(entt::entity creature, uint32_t action,
                                                                        const creature_mind_tables::Tables* tables)
{
	const auto& registry = Entities();
	// To impress, it picks the miracle as the agenda is built, and casts none when it knows none
	const auto* executor = ExecutorOf(tables, action);
	if (executor != nullptr && executor->build == creature_plan_actions::Build::CastToImpress)
	{
		if (!registry.AllOf<Creature>(creature))
		{
			return std::nullopt;
		}
		return creature_plan_actions::CastInfo {.magicType = ImpressiveMiracleFor(creature, *tables).value_or(0),
		                                        .height = object_measures::Height(registry, creature),
		                                        .caster = entt::to_integral(creature)};
	}
	// To amuse, it picks the spell as it builds the agenda, from the spells it may cast now
	if (IsAmusingCast(tables, action) && registry.AllOf<Creature>(creature))
	{
		const auto* mind = registry.TryGet<const CreatureMindState>(creature);
		return creature_plan_actions::CastInfo {.height = object_measures::Height(registry, creature),
		                                        .caster = entt::to_integral(creature),
		                                        .amusingSpells = mind != nullptr ? AmusingSpells(creature, *mind, *tables)
		                                                                         : std::vector<creature_mind::AmusingSpell> {}};
	}
	// A row the game's table gives no miracle may still cast one, which only a forced plan reaches
	auto magic = CastMagicOf(action);
	if (!magic.has_value() && executor != nullptr)
	{
		magic = executor->castMagic;
	}
	if (!magic.has_value() || !registry.AllOf<Creature>(creature))
	{
		return std::nullopt;
	}
	const auto& info = Locator::infoConstants::value();
	const auto type = static_cast<MagicType>(*magic);
	// The miracle's gesture as the running game names it: a power-up's is the one its seed powers up to. Its height is
	// the creature's own
	const auto gesture = magic::GetGestureOfMagicInfo(info, type);
	creature_plan_actions::CastInfo cast {.magicType = *magic,
	                                      .gesture = static_cast<uint32_t>(gesture),
	                                      .height = object_measures::Height(registry, creature),
	                                      .caster = entt::to_integral(creature)};
	// The creature casts a fireball from a clear area near it, as wide as five times its radius. Living things in the
	// area do not block it
	if (executor != nullptr && executor->build == creature_plan_actions::Build::CastFireball)
	{
		if (const auto* transform = registry.TryGet<const Transform>(creature))
		{
			cast.clearArea = FindClearArea(registry, creature, Flat(transform->position),
			                               creature_mind::k_FireballClearRadii * ecs::object::Get2DRadius(creature));
		}
	}
	return cast;
}

std::optional<glm::vec2> mind_detail::FindClearArea(const ecs::Registry& registry, entt::entity creature, glm::vec2 point,
                                                    float width, bool nothingLiving)
{
	// With the debug tools' readout on the creature, each cell of the window that is not clear is noted with what keeps
	// it so; the cells are tested the same either way
	const auto* readout = registry.TryGet<const CreatureClearAreaReadout>(creature);
	std::vector<CreatureClearAreaReadout::BlockedCell> blocked;
	// The cells it may stand in are the walkable land's; the game's own test of a cell is still to be read
	const auto found = cast_moves::FindClearArea(point, width, [&](int32_t x, int32_t z) {
		if (readout == nullptr)
		{
			return land_avoid::At(x, z) == land_avoid::k_Land && !CellBlocked(registry, creature, x, z, nothingLiving);
		}
		if (land_avoid::At(x, z) != land_avoid::k_Land)
		{
			blocked.push_back({.cell = {x, z}, .blocker = {.why = ClearCellBlock::NotLand, .by = entt::null}});
			return false;
		}
		const auto blocker = BlockerOf(registry, creature, x, z, nothingLiving);
		if (blocker.has_value())
		{
			blocked.push_back({.cell = {x, z}, .blocker = *blocker});
		}
		return !blocker.has_value();
	});
	// A creature kept within an area takes no spot farther than the area's radius from its centre
	const auto result = [&]() {
		const auto* kept = registry.TryGet<const CreatureLeash>(creature);
		if (!found.has_value() || kept == nullptr || !Locator::leashSystem::has_value())
		{
			return found;
		}
		const auto& leashes = Locator::leashSystem::value();
		const bool leashed = leashes.IsLeashed(creature);
		const bool confined = creature_leash::IsConfined(kept->confinementRadius, leashed, !leashed || leashes.Works(creature));
		return cast_moves::WithinKeptArea(found, glm::vec2(kept->confinementCentre.x, kept->confinementCentre.z),
		                                  kept->confinementRadius, confined);
	}();
	if (readout != nullptr)
	{
		readout->last = CreatureClearAreaReadout::Search {
		    .turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0,
		    .width = width,
		    .point = point,
		    .nothingLiving = nothingLiving,
		    .found = found,
		    .result = result,
		    .blocked = std::move(blocked),
		};
	}
	return result;
}

std::optional<glm::vec2> mind_detail::ClearSpotToCastOnItself(const ecs::Registry& registry, entt::entity creature)
{
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	return FindClearArea(registry, creature, Flat(transform->position),
	                     creature_mind::k_WaterOnItselfClearWidth * object_measures::Height(registry, creature));
}

void mind_detail::StartSubMove(entt::entity creature, const creature_mind::Movement& movement, float seconds)
{
	auto& registry = Entities();
	auto* existing = registry.TryGet<CreatureCasting>(creature);
	auto& casting = existing != nullptr ? *existing : registry.AssignState<CreatureCasting>(creature);
	casting.target = movement.object.has_value() ? static_cast<entt::entity>(*movement.object) : entt::null;
	casting.keep = movement.maxDistance;
	casting.settleSeconds = seconds;
	casting.run = movement.run;
	casting.phase = 0;
	casting.turns = 0;
	casting.holdTurns = 0;
	casting.destination.reset();
	casting.outcome = CreatureCasting::Outcome::Running;
	switch (movement.kind)
	{
	case Kind::GoNearObject:
		casting.move = CreatureCasting::Move::GoNear;
		break;
	case Kind::GetAwayFromObject:
		casting.move = CreatureCasting::Move::GetAway;
		break;
	case Kind::TurnToFaceObject:
		casting.move = CreatureCasting::Move::TurnToFace;
		break;
	case Kind::TurnAwayFromObject:
		casting.move = CreatureCasting::Move::TurnAway;
		break;
	case Kind::ToThrowPosition:
		casting.move = CreatureCasting::Move::ToThrowPosition;
		break;
	default:
		casting.move = CreatureCasting::Move::None;
		return;
	}
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	StepSubMove(creature, animation != nullptr && creature_layers::IsPlaying(animation->body));
}

void mind_detail::StartGesture(entt::entity creature, uint32_t gesture)
{
	auto& registry = Entities();
	auto* existing = registry.TryGet<CreatureCasting>(creature);
	auto& casting = existing != nullptr ? *existing : registry.AssignState<CreatureCasting>(creature);
	casting.move = CreatureCasting::Move::Gesture;
	casting.outcome = CreatureCasting::Outcome::Running;
	casting.target = entt::null;
	casting.phase = 0;
	casting.turns = 0;
	casting.gesture = gesture;
	casting.gestureProgress = 0.0f;
	casting.gesturePointing = false;
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} draws gesture {} before its cast", entt::to_integral(creature),
	                   gesture);
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	StepSubMove(creature, animation != nullptr && creature_layers::IsPlaying(animation->body));
}

void mind_detail::StepSubMove(entt::entity creature, bool animating)
{
	auto& registry = Entities();
	auto* casting = registry.TryGet<CreatureCasting>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (casting == nullptr || casting->move == CreatureCasting::Move::None ||
	    casting->outcome != CreatureCasting::Outcome::Running || transform == nullptr ||
	    !Locator::creatureLocomotionSystem::has_value())
	{
		return;
	}
	auto& locomotion = Locator::creatureLocomotionSystem::value();
	const auto at = object_measures::PositionOf(registry, casting->target);
	const auto here = Flat(transform->position);
	const auto* moves = registry.TryGet<const CreatureLocomotion>(creature);
	const bool turning = moves != nullptr && moves->motion == CreatureLocomotion::Motion::Turning;
	// Playing an action, walking or turning: the game counts all as its body doing something
	const bool bodyBusy = animating || locomotion.IsMoving(creature);
	const auto fail = [&] { casting->outcome = CreatureCasting::Outcome::Failed; };
	const auto done = [&] { casting->outcome = CreatureCasting::Outcome::Done; };
	switch (casting->move)
	{
	case CreatureCasting::Move::GoNear:
	{
		const bool itself = at.has_value() && object_measures::WalkedUpToItself(registry, casting->target);
		if (casting->phase == 0)
		{
			// A building or a tree it walks up to by its walk alone
			if (itself && !GoNear(creature, *casting, animating || turning))
			{
				casting->destination.reset();
			}
			casting->stuckCheck = here;
			casting->phase = 1;
			casting->turns = 0;
			break;
		}
		++casting->turns;
		if (itself && ecs::map_cells::IsObjectInMap(casting->target) && !bodyBusy)
		{
			done();
			break;
		}
		const float routeRadius = at.has_value() ? object_measures::RoutePlanRadius(registry, casting->target, creature)
		                                         : object_measures::TwoDRadius(registry, creature);
		const auto target = at.has_value() ? Flat(*at) : here;
		const float selfRadius = object_measures::TwoDRadius(registry, creature);
		if (cast_moves::ArrivedNear(here, target, selfRadius, routeRadius, casting->keep))
		{
			// near enough, a walk brakes to a stop
			locomotion.BrakeToStop(creature);
			done();
			break;
		}
		// Every turn it is sent on again, so that it follows a thing that moves; the game's look at whether it heads
		// for the thing changes nothing, and is left out
		if (!GoNear(creature, *casting, animating || turning))
		{
			// it keeps in mind a while that it found no way to the thing
			if (auto* blocked = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature); blocked != nullptr)
			{
				blocked->beliefs.MarkBlocked(casting->target, game_clock::Turn());
			}
			fail();
			break;
		}
		// It gives up when it has got nowhere for a while, and wants what it went for less for a while; not while a
		// script controls it
		auto* mindState = registry.TryGet<CreatureMindState>(creature);
		const uint32_t count = mindState != nullptr ? mindState->stepTurns : casting->turns;
		const auto look = creature_cast_moves::GoingNearStuck(count, glm::distance(here, casting->stuckCheck),
		                                                      ecs::script_held::IsControlledByScript(creature));
		if (look == creature_cast_moves::StuckLook::Mark)
		{
			casting->stuckCheck = here;
		}
		else if (look == creature_cast_moves::StuckLook::GiveUp)
		{
			if (mindState != nullptr && mindState->desires.has_value() && mindState->planActive &&
			    mindState->planner.current.has_value())
			{
				creature_desires::Suppress(*mindState->desires, mindState->planner.current->desire, k_StuckSuppressSeconds,
				                           k_TurnsPerSecond);
			}
			fail();
		}
		break;
	}
	case CreatureCasting::Move::GetAway:
	{
		if (casting->phase == 0)
		{
			// Not while it plays an action or turns on the spot, only standing or walking
			if (animating || turning)
			{
				break;
			}
			if (!at.has_value())
			{
				done();
				break;
			}
			const float far =
			    cast_moves::GetAwayDistance(casting->keep, object_measures::TwoDRadius(registry, casting->target));
			if (cast_moves::FarEnoughAway(Flat(*at), here, far))
			{
				done();
				break;
			}
			const auto away = cast_moves::GetAwayPoint(transform->position, *at, far);
			const auto clear = FindClearArea(registry, creature, Flat(away), object_measures::Height(registry, creature));
			if (!clear.has_value() || locomotion.MoveTo(creature, *clear, KeptOrRun(*casting), 0.0f, 1.0f) !=
			                              CreatureLocomotionSystemInterface::MoveResult::Started)
			{
				fail();
				break;
			}
			casting->phase = 1;
			break;
		}
		if (!bodyBusy)
		{
			done();
		}
		break;
	}
	case CreatureCasting::Move::ToThrowPosition:
	{
		if (casting->phase == 1)
		{
			if (!locomotion.IsMoving(creature))
			{
				done();
			}
			break;
		}
		if (!at.has_value())
		{
			fail();
			break;
		}
		const auto stand =
		    creature_throw::WhereToThrowFrom(here, Flat(*at), object_measures::Height(registry, creature),
		                                     object_measures::TwoDRadius(registry, casting->target), casting->keep);
		if (!stand.has_value())
		{
			fail();
			break;
		}
		if (stand->kind == creature_throw::ThrowStand::Kind::There)
		{
			done();
			break;
		}
		auto point = stand->point;
		// Backing away, where it can't stand it goes to the nearest place it can, if near enough
		if (stand->kind == creature_throw::ThrowStand::Kind::BackOff &&
		    !land_avoid::IsPosValid(glm::vec3(point.x, 0.0f, point.y), land_avoid::k_CreatureRadius))
		{
			const auto valid = land_avoid::NearestValid(point, land_avoid::k_CreatureRadius, k_ThrowStandSearch);
			if (!valid.has_value() || glm::distance(*valid, point) >= k_ThrowStandReach)
			{
				fail();
				break;
			}
			point = *valid;
		}
		switch (locomotion.MoveTo(creature, point, KeptOrRun(*casting), stand->minDistance, stand->maxDistance))
		{
		case CreatureLocomotionSystemInterface::MoveResult::Started:
			casting->phase = 1;
			break;
		case CreatureLocomotionSystemInterface::MoveResult::Busy:
			// It tries again next turn
			break;
		case CreatureLocomotionSystemInterface::MoveResult::InvalidDestination:
			fail();
			break;
		}
		break;
	}
	case CreatureCasting::Move::TurnToFace:
	{
		if (!at.has_value())
		{
			done();
			break;
		}
		const auto target = Flat(*at);
		const float heading = moves != nullptr ? moves->heading : 0.0f;
		const bool onTop = glm::distance(here, target) < cast_moves::k_OnTopDistance;
		if (casting->phase == 0)
		{
			// Right on top of it there is nothing to turn to, but it still settles as it would
			if (onTop)
			{
				casting->phase = 1;
				break;
			}
			// a walk brakes to a stop first, and the turn is asked for again every turn until it can start
			locomotion.BrakeToStop(creature);
			if (locomotion.TurnToFace(creature, target))
			{
				casting->phase = 1;
			}
			break;
		}
		if (bodyBusy)
		{
			break;
		}
		if (onTop && casting->phase == 1)
		{
			casting->phase = 2;
			break;
		}
		if (onTop)
		{
			done();
			break;
		}
		const bool facing = cast_moves::Facing(here, heading, target);
		if (casting->phase == 1)
		{
			if (!facing)
			{
				locomotion.TurnToFace(creature, target);
				break;
			}
			// It holds still the step's seconds in whole turns, as the game counts them
			casting->holdTurns = static_cast<uint32_t>(std::max(game_clock::TicksForSeconds(casting->settleSeconds), 0));
			casting->phase = 2;
			break;
		}
		if (casting->holdTurns == 0)
		{
			done();
			break;
		}
		--casting->holdTurns;
		if (!facing)
		{
			locomotion.TurnToFace(creature, target);
		}
		break;
	}
	case CreatureCasting::Move::TurnAway:
	{
		// Its back to the thing: right on top of it there is nothing to turn from and it goes on at once; else a stop,
		// then the turn to face the other way, asked for until it starts; done once the body is free
		if (!at.has_value())
		{
			done();
			break;
		}
		const auto away = here - Flat(*at);
		if (casting->phase == 0)
		{
			if (glm::length(away) < cast_moves::k_OnTopDistance)
			{
				casting->phase = 1;
				break;
			}
			locomotion.BrakeToStop(creature);
			if (locomotion.TurnToFace(creature, here + away))
			{
				casting->phase = 1;
			}
			break;
		}
		if (!bodyBusy)
		{
			done();
		}
		break;
	}
	case CreatureCasting::Move::Gesture:
		StepGesture(creature, *casting, animating);
		break;
	case CreatureCasting::Move::None:
		break;
	}
}

creature_mind::SubMove mind_detail::SubMoveOf(entt::entity creature)
{
	const auto* casting = Entities().TryGet<const CreatureCasting>(creature);
	if (casting == nullptr || casting->move == CreatureCasting::Move::None)
	{
		return creature_mind::SubMove::Running;
	}
	switch (casting->outcome)
	{
	case CreatureCasting::Outcome::Done:
		return creature_mind::SubMove::Done;
	case CreatureCasting::Outcome::Failed:
		return creature_mind::SubMove::Failed;
	case CreatureCasting::Outcome::Running:
		break;
	}
	return creature_mind::SubMove::Running;
}

std::optional<uint32_t> CreatureMindSystem::CastMagicOf(uint32_t action)
{
	return mind_detail::CastMagicOf(action);
}

std::optional<std::pair<float, float>> CreatureMindSystem::MiracleSightings(entt::entity creature, MagicType type)
{
	const auto* tables = GetTables();
	const auto& registry = Entities();
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	if (tables == nullptr || mind == nullptr || body == nullptr)
	{
		return std::nullopt;
	}
	return mind_detail::MiracleSightings(*mind, *tables, body->species, type);
}

bool CreatureMindSystem::MayCast(entt::entity creature, const CreatureMindState& mind, uint32_t action, bool powerUp)
{
	return mind_detail::MayCast(creature, mind, GetTables(), action, powerUp);
}

std::optional<creature_plan_actions::CastInfo> CreatureMindSystem::CastInfoFor(entt::entity creature, uint32_t action)
{
	return mind_detail::CastInfoFor(creature, action, GetTables());
}

void CreatureMindSystem::StartSubMove(entt::entity creature, const creature_mind::Movement& movement, float seconds)
{
	mind_detail::StartSubMove(creature, movement, seconds);
}

void CreatureMindSystem::StartGesture(entt::entity creature, uint32_t gesture)
{
	mind_detail::StartGesture(creature, gesture);
}

void CreatureMindSystem::StepSubMove(entt::entity creature, bool animating)
{
	mind_detail::StepSubMove(creature, animating);
}

creature_mind::SubMove CreatureMindSystem::SubMoveOf(entt::entity creature)
{
	return mind_detail::SubMoveOf(creature);
}

namespace
{
/// Whether a try at a miracle fizzles: short of all but one of the sightings it needs. A fizzle counts as one more
/// sighting, the advisor may tell the player that the creature messed the miracle up, and it is shown later, about the
/// thing it was tried at. A try at a place has no thing: it is shown about the thing of the last try at a thing that
/// fizzled.
bool Fizzles(entt::entity creature, CreatureMindState& mind, const creature_mind_tables::Tables& tables, const Creature& body,
             MagicType type, std::optional<entt::entity> target)
{
	const auto magicType = static_cast<uint32_t>(type);
	const auto sightings = SightingsOf(mind, tables, body.species, magicType);
	if (!sightings.has_value() || creature_spell_casting::TrySucceeds(sightings->seen, sightings->needed))
	{
		return false;
	}
	auto& registry = Entities();
	auto* existing = registry.TryGet<CreatureCasting>(creature);
	auto& casting = existing != nullptr ? *existing : registry.AssignState<CreatureCasting>(creature);
	if (target.has_value())
	{
		casting.lastFizzleTarget = *target;
	}
	casting.fizzle = CreatureCasting::Fizzle {.target = casting.lastFizzleTarget, .magicType = type};
	// in place of what a stroke or slap left it to do next, or a cast on the leash
	mind.afterFeedback.reset();
	mind.leashCast.reset();
	if (Locator::creatureHelpSystem::has_value())
	{
		Locator::creatureHelpSystem::value().Raise(
		    creature, {.kind = creature_help::Kind::MessedUpAction, .parameter = creature_help::k_MiracleFizzled});
	}
	++mind.learnt->knowledge.miraclesSeen[magicType].count;
	return true;
}

/// Without the energy for a miracle, or where the miracle can't be cast, the creature is frustrated and wants food
/// above all
void Frustrate(CreatureMindState& mind, const Creature& body)
{
	const auto& info = Locator::infoConstants::value();
	const auto row = creature::InfoRow(body.species);
	if (mind.desires.has_value() && row < info.creature.size())
	{
		creature_spell_mind::MakeFullyDominantOverOthers(*mind.desires, creature_desires::Desire::Hunger,
		                                                 info.creature.at(row).desireFloor);
	}
}
} // namespace

bool mind_detail::TryMiracle(entt::entity creature, CreatureMindState& mind, const creature_mind_tables::Tables& tables,
                             MagicSystemInterface& magic, MagicType type, entt::entity target)
{
	auto& registry = Entities();
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr)
	{
		return false;
	}
	// It lets go of what it held first
	magic.ReleaseCreatureCast(creature);
	const float cost = magic::GetMagicEffectInfo(Locator::infoConstants::value(), type).costToCreate;
	// In a fight every miracle costs stamina, and it can't cast one it hasn't the stamina for
	if (auto* fighting = registry.TryGet<CreatureFighting>(creature))
	{
		const auto left =
		    creature_spell_casting::PayStamina(fighting->fighter.stamina, creature_spell_casting::StaminaCost(cost));
		if (!left.has_value())
		{
			return false;
		}
		fighting->fighter.stamina = *left;
	}
	if (Fizzles(creature, mind, tables, *body, type, target))
	{
		return false;
	}
	// Without the energy for it, or at something the miracle can't be cast at, it is frustrated and wants food above
	// all; it is not for any other reason it can't cast
	if (!magic::creator::CreatureCanPayFor(creature, cost) || !magic.CanCreatureCastAt(type, target))
	{
		Frustrate(mind, *body);
		return false;
	}
	const bool cast = magic.CastByCreature(creature, type, target);
	// A readout at run time (the game channel at debug, not compiled out of release builds): each miracle a creature
	// casts at a thing, and the action of the plan it casts it for, so that a played run shows which casting row ran
	if (const auto logger = spdlog::get("game"); logger != nullptr && cast)
	{
		const auto& plan = mind.planner.current;
		logger->debug("Creature {} casts miracle {} at {} for its plan {}", entt::to_integral(creature),
		              static_cast<uint32_t>(type), entt::to_integral(target),
		              plan.has_value() && plan->action < tables.actions.size() ? tables.actions[plan->action].name
		                                                                       : std::string("none"));
	}
	return cast;
}

bool mind_detail::TryMiracleAt(entt::entity creature, CreatureMindState& mind, const creature_mind_tables::Tables& tables,
                               MagicSystemInterface& magic, MagicType type, glm::vec3 place)
{
	const auto* body = Entities().TryGet<const Creature>(creature);
	if (body == nullptr)
	{
		return false;
	}
	// A try at a place costs no stamina in a fight. It fizzles before it lets go of what it held.
	if (Fizzles(creature, mind, tables, *body, type, std::nullopt))
	{
		return false;
	}
	magic.ReleaseCreatureCast(creature);
	// Without the energy for it, or at a place the miracle can't be cast at, it is frustrated and wants food above all
	const float cost = magic::GetMagicEffectInfo(Locator::infoConstants::value(), type).costToCreate;
	if (!magic::creator::CreatureCanPayFor(creature, cost) || !magic.CanCreatureCastAtPlace(type, place))
	{
		Frustrate(mind, *body);
		return false;
	}
	return magic.CastByCreatureAt(creature, type, place);
}

bool CreatureMindSystem::TryMiracle(entt::entity creature, MagicType type, entt::entity target)
{
	auto* mind = Entities().TryGet<CreatureMindState>(creature);
	const auto* tables = GetTables();
	return mind != nullptr && tables != nullptr && Locator::magicSystem::has_value() &&
	       mind_detail::TryMiracle(creature, *mind, *tables, Locator::magicSystem::value(), type, target);
}

bool CreatureMindSystem::TryMiracleAt(entt::entity creature, MagicType type, glm::vec3 place)
{
	auto* mind = Entities().TryGet<CreatureMindState>(creature);
	const auto* tables = GetTables();
	return mind != nullptr && tables != nullptr && Locator::magicSystem::has_value() &&
	       mind_detail::TryMiracleAt(creature, *mind, *tables, Locator::magicSystem::value(), type, place);
}

void CreatureMindSystem::DoAfterFeedback(entt::entity creature, CreatureMindState& mind)
{
	if (!mind.afterFeedback.has_value())
	{
		return;
	}
	const auto after = *mind.afterFeedback;
	mind.afterFeedback.reset();
	// Shut in, it walks on to where it was going once it has done what it was given to clear its way
	if (after == CreatureMindState::AfterFeedback::WalkToKeptPoint)
	{
		WalkToKeptPoint(creature, mind);
		return;
	}
	if (after == CreatureMindState::AfterFeedback::RepeatLastAction)
	{
		// What it learns most from now, as the feedback found it
		if (mind.learnt.has_value())
		{
			if (const auto index = creature_learning::BestContext(mind.learnt->contexts); index.has_value())
			{
				const auto lesson = mind.learnt->contexts.at(*index);
				RepeatLastAction(creature, mind, lesson);
			}
		}
		return;
	}
	if (after == CreatureMindState::AfterFeedback::Frightened)
	{
		// One time in three it runs from the player, else it is frightened where it stands
		const std::string_view action = Random(3) == 0 ? "RunAwayFromPlayer" : "BeFrightenedOnTheSpot";
		std::ignore = ForceActivity(creature, {.desire = creature_desires::Desire::Fear, .action = action});
		// what the feedback left it to do is carried out as an order is, its own choices waiting
		mind.commanded = true;
		return;
	}
	// Happy, or how a slap made it feel, under the desire to show its state, as the fizzle shows it is embarrassed
	std::vector<creature_mind::Step> agenda;
	if (after == CreatureMindState::AfterFeedback::ShowSlapFeeling)
	{
		const auto& desires = mind.desires;
		const creature_follow_up::Feelings feelings {
		    .compassion = desires.has_value() ? (*desires)[creature_desires::Desire::Compassion].value : 0.0f,
		    .anger = desires.has_value() ? (*desires)[creature_desires::Desire::Anger].value : 0.0f,
		};
		agenda = creature_follow_up::ShowSlapFeeling(
		    feelings, [](uint32_t range) { return Random(range); }, [] { return Chance(); },
		    static_cast<uint32_t>(k_TurnsPerSecond));
		// A trace at run time (the game channel at trace level), so a moved run can name the reaction
		if (const auto logger = spdlog::get("game"))
		{
			logger->trace("Creature {} shows how a slap made it feel, clip {}", entt::to_integral(creature),
			              agenda.front().animation);
		}
	}
	else
	{
		agenda = {{.kind = creature_mind::Step::Kind::Action, .animation = creature_layers::animations::k_Happy}};
	}
	if (!Replan(creature, creature_mind::Activity::Planned, std::move(agenda)))
	{
		return;
	}
	const auto* tables = GetTables();
	if (const auto action = tables != nullptr ? creature_mind_tables::FindAction(*tables, "CommunicateState") : std::nullopt)
	{
		mind.planner.current = creature_planner::Plan {.desire = creature_desires::Desire::ManifestState,
		                                               .action = *action,
		                                               .object = entt::to_integral(creature),
		                                               .goalUsefulness = creature_planner::k_ForcedScore,
		                                               .actionPriority = creature_planner::k_ForcedScore,
		                                               .priority = creature_planner::k_ForcedScore,
		                                               .forced = true};
		mind.planActive = true;
		mind.planSerial = mind.idle.serial;
		mind.agendaSeen = mind.idle.serial;
		if (mind.desires.has_value())
		{
			creature_learning::SuppressOpposed(*mind.desires, creature_desires::Desire::ManifestState, tables->dependencies,
			                                   k_TurnsPerSecond);
		}
	}
	mind.commanded = true;
}

void CreatureMindSystem::ShowFizzle(entt::entity creature, CreatureMindState& mind)
{
	auto* casting = Entities().TryGet<CreatureCasting>(creature);
	if (casting == nullptr || !casting->fizzle.has_value())
	{
		return;
	}
	const auto fizzle = *casting->fizzle;
	casting->fizzle.reset();
	// It stops what it was doing to show how it feels about what it tried, under the desire to show its state; deciding
	// on that holds back the desires it opposes, as deciding on any plan does
	if (!Replan(creature, creature_mind::Activity::Planned,
	            {{.kind = creature_mind::Step::Kind::Action, .animation = creature_mind::k_FizzleEmbarrassed}}))
	{
		return;
	}
	const auto* tables = GetTables();
	if (const auto action = tables != nullptr ? creature_mind_tables::FindAction(*tables, "CommunicateState") : std::nullopt)
	{
		mind.planner.current = creature_planner::Plan {
		    .desire = creature_desires::Desire::ManifestState,
		    .action = *action,
		    .object = Entities().Valid(fizzle.target) ? std::optional(entt::to_integral(fizzle.target)) : std::nullopt,
		    .forced = true};
		mind.planActive = true;
		mind.planSerial = mind.idle.serial;
		mind.agendaSeen = mind.idle.serial;
		if (mind.desires.has_value())
		{
			creature_learning::SuppressOpposed(*mind.desires, creature_desires::Desire::ManifestState, tables->dependencies,
			                                   k_TurnsPerSecond);
		}
	}
	// Embarrassed, then one time in two sad as well, and sadder for it
	if (const auto sad = creature_mind::SadAfterFizzle([](uint32_t range) { return Random(range); }, mind.desires.has_value(),
	                                                   [](float most) { return game_random::GameFloatRand(most); }))
	{
		mind.idle.agenda.push_back({.kind = creature_mind::Step::Kind::Action, .animation = sad->animation});
		if (sad->sadness.has_value())
		{
			creature_desires::ChangeSource(*mind.desires, creature_desires::sources::k_Sadness, *sad->sadness);
		}
	}
	// shown as an order is carried out, its own choices waiting
	mind.commanded = true;
}

void CreatureMindSystem::KnowMiracle(entt::entity creature, size_t miracle)
{
	// As the game's scripts teach it a miracle: known, and seen as often as its species must see it. The debug tools'
	// teaching is not told of
	ChangeKnows(creature, {.kind = static_cast<uint32_t>(CreatureActionLearningType::Magic),
	                       .action = static_cast<uint32_t>(miracle),
	                       .knows = true});
}

bool CreatureMindSystem::TellCast(entt::entity creature, MagicType type, entt::entity target)
{
	auto& registry = Entities();
	const auto* tables = GetTables();
	if (!registry.AllOf<CreatureMindState>(creature) || tables == nullptr || !registry.Valid(target))
	{
		return false;
	}
	for (uint32_t action = 0; action < tables->actions.size(); ++action)
	{
		const auto* executor = creature_plan_actions::For(tables->actions[action].name);
		// Healing itself casts at the creature, never at the target it was told
		if (executor == nullptr || !creature_plan_actions::IsCast(*executor) ||
		    executor->build == creature_plan_actions::Build::HealItself || CastMagicOf(action) != static_cast<uint32_t>(type) ||
		    tables->actions[action].desire >= creature_desires::k_DesireCount)
		{
			continue;
		}
		// Told by the debug tools, it goes about the casting as it would by itself, without weighing other plans, and the
		// casting is about its target
		const auto at = object_measures::PositionOf(registry, target);
		auto cast = CastInfoFor(creature, action);
		if (cast.has_value())
		{
			cast->atNeedsSign = registry.AllOf<ShowNeedsVisuals>(target);
			cast->targetLiving = registry.AnyOf<Villager, Animal, Creature>(target);
			// A forest goes to the place where the thing is now
			if (executor->build == creature_plan_actions::Build::CastForest)
			{
				cast->place = at;
			}
		}
		auto agenda = creature_plan_actions::Agenda(
		    *executor, entt::to_integral(target), at.has_value() ? Flat(*at) : glm::vec2(0.0f),
		    creature_plan_actions::Situation {.activity = entt::to_integral(target)},
		    [](uint32_t range) { return Random(range); }, cast);
		return agenda.has_value() && Replan(creature, creature_mind::Activity::Told, std::move(*agenda));
	}
	return false;
}

bool CreatureMindSystem::SeemsFriendly(entt::entity creature, entt::entity other)
{
	const auto& registry = std::as_const(Entities());
	const auto* tables = GetTables();
	const auto* mine = registry.Valid(creature) ? registry.TryGet<const CreatureMindState>(creature) : nullptr;
	const auto* theirs = registry.Valid(other) ? registry.TryGet<const CreatureMindState>(other) : nullptr;
	if (tables == nullptr || mine == nullptr || theirs == nullptr)
	{
		return false;
	}
	return creature_mind::SeemsFriendly(mind_detail::DesireAboutCreature(registry, creature, *mine, other, *tables),
	                                    mind_detail::DesireAboutCreature(registry, other, *theirs, creature, *tables));
}

bool mind_detail::CanPayForMiracle(entt::entity creature, MagicType type)
{
	return Locator::infoConstants::has_value() && CanPayFor(creature, type);
}
