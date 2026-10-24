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
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/mat2x2.hpp>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/LandAvoid.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureCastAgenda.h"
#include "Creature/CreatureCastMoves.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureSpellCasting.h"
#include "Creature/CreatureSpellMind.h"
#include "Creature/CreatureThrow.h"
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
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/ShowNeeds.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/RoutePlanWorld.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/SpellCreator.h"
#include "Magic/MagicTables.h"
#include "ObjectMeasures.h"
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
/// Going near something, it looks every this many turns whether it has got anywhere, and gives up when it has moved
/// less than this since
constexpr uint32_t k_StuckTurns = 50;
constexpr float k_StuckDistance = 0.01f;
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
/// What a fizzled try shows: the embarrassed action, then one time in two the sad one, which makes it sadder by up to
/// this much
constexpr size_t k_Embarrassed = 73;
constexpr float k_SadnessFromFizzle = 0.3f;

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

glm::vec2 Flat(const glm::vec3& point)
{
	return {point.x, point.z};
}

/// The circles a fixed thing keeps creatures off, from its model's box and its place
std::vector<cast_moves::CollideCircle> ModelCircles(const ecs::Registry& registry, entt::entity entity)
{
	const auto* mesh = registry.TryGet<const Mesh>(entity);
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (mesh == nullptr || transform == nullptr || !Locator::resources::has_value() ||
	    !Locator::resources::value().GetMeshes().Contains(mesh->id))
	{
		return {};
	}
	const auto& box = Locator::resources::value().GetMeshes().Handle(mesh->id)->GetBoundingBox();
	const auto half = (box.maxima - box.minima) * 0.5f * transform->scale.x;
	const auto centre = transform->position + transform->rotation * (box.Center() * transform->scale);
	const glm::mat2 turn(transform->rotation[0].x, transform->rotation[0].z, transform->rotation[2].x,
	                     transform->rotation[2].z);
	return cast_moves::CollideCircles({half.x, half.z}, Flat(centre), turn);
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
	// The game sets no pace here: it goes on at the one it was last asked for. Nothing keeps that pace yet, so it walks.
	// Asked again for the same place while on its way, the walk goes on as it is
	const auto result = Locator::creatureLocomotionSystem::value().MoveTo(
	    creature, target, CreatureLocomotionSystemInterface::Pace::Walk, near, far);
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
} // namespace

bool mind_detail::CellBlocked(const ecs::Registry& registry, entt::entity creature, int32_t x, int32_t z, bool livingBlocks)
{
	if (!Locator::entitiesMap::has_value() || x < 0 || z < 0)
	{
		return false;
	}
	const auto& map = Locator::entitiesMap::value();
	const ecs::MapInterface::CellId cell {static_cast<uint16_t>(x), static_cast<uint16_t>(z)};
	const auto blocks = [&](entt::entity entity) {
		if (!registry.Valid(entity))
		{
			return false;
		}
		// A temple's heart takes the cell outright
		if (registry.AllOf<Temple>(entity))
		{
			return true;
		}
		// A tree, a field, or a thing the creature must go round, by the shapes it keeps clear
		if (registry.AnyOf<Tree, Field>(entity) || ecs::route_plan_world::CreatureMustAvoid(entity, creature))
		{
			std::vector<cast_moves::CollideCircle> circles;
			if (registry.AllOf<Tree>(entity) && !registry.AllOf<DeadTree>(entity))
			{
				if (const auto* at = registry.TryGet<const Transform>(entity))
				{
					circles.push_back({.centre = Flat(at->position), .radius = cast_moves::k_TreeCollideRadius});
				}
			}
			else
			{
				circles = ModelCircles(registry, entity);
			}
			if (std::ranges::any_of(circles, [x, z](const auto& circle) { return cast_moves::BlocksCell(circle, x, z); }))
			{
				return true;
			}
		}
		// Asked for a place with nothing living in it, any living thing there takes the cell
		return livingBlocks && registry.AnyOf<Villager, Animal, Creature>(entity);
	};
	return std::ranges::any_of(map.GetFixedInGridCell(cell), blocks) ||
	       std::ranges::any_of(map.GetMobileInGridCell(cell), blocks);
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
			spells.push_back(
			    {.magicType = type, .gesture = static_cast<uint32_t>(magic::GetMagicInfo(info, magic).gestureType)});
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
	// The miracle's own gesture, power-ups too: none in the game's tables. Its height is the creature's own
	const auto gesture = magic::GetMagicInfo(info, type).gestureType;
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
	// The cells it may stand in are the walkable land's; the game's own test of a cell is still to be read
	const auto found = cast_moves::FindClearArea(point, width, [&](int32_t x, int32_t z) {
		return land_avoid::At(x, z) == land_avoid::k_Land && !CellBlocked(registry, creature, x, z, nothingLiving);
	});
	// A creature kept within an area takes no spot farther than the area's radius from its centre
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
		if (cast_moves::Arrived(glm::distance(here, target), selfRadius, routeRadius, casting->keep))
		{
			locomotion.Stop(creature);
			done();
			break;
		}
		// Every turn it is sent on again, so that it follows a thing that moves; the game's look at whether it heads
		// for the thing changes nothing, and is left out
		if (!GoNear(creature, *casting, animating || turning))
		{
			fail();
			break;
		}
		// It gives up when it has got nowhere for a while, and wants what it went for less for a while
		auto* mindState = registry.TryGet<CreatureMindState>(creature);
		const uint32_t count = mindState != nullptr ? mindState->stepTurns : casting->turns;
		if (count % k_StuckTurns == 0)
		{
			casting->stuckCheck = here;
		}
		else if (count % k_StuckTurns == k_StuckTurns - 1 && glm::distance(here, casting->stuckCheck) < k_StuckDistance)
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
			if (!(glm::distance(Flat(*at), here) < far))
			{
				done();
				break;
			}
			const auto away = cast_moves::GetAwayPoint(transform->position, *at, far);
			const auto clear = FindClearArea(registry, creature, Flat(away), object_measures::Height(registry, creature));
			if (!clear.has_value() || locomotion.MoveTo(creature, *clear, CreatureLocomotionSystemInterface::Pace::Walk, 0.0f,
			                                            1.0f) != CreatureLocomotionSystemInterface::MoveResult::Started)
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
		switch (locomotion.MoveTo(creature, point, CreatureLocomotionSystemInterface::Pace::Walk, stand->minDistance,
		                          stand->maxDistance))
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
			locomotion.Stop(creature);
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
	// in place of what a stroke or slap left it to do next
	mind.afterFeedback.reset();
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
		const float staminaCost = creature_spell_casting::StaminaCost(cost);
		auto& stamina = fighting->fighter.stamina;
		if (stamina < staminaCost)
		{
			return false;
		}
		stamina = std::clamp(stamina - staminaCost, 0.0f, 1.0f);
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
	return magic.CastByCreature(creature, type, target);
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
	if (after == CreatureMindState::AfterFeedback::Frightened)
	{
		// One time in three it runs from the player, else it is frightened where it stands
		const std::string_view action = Random(3) == 0 ? "RunAwayFromPlayer" : "BeFrightenedOnTheSpot";
		std::ignore = ForceActivity(creature, {.desire = creature_desires::Desire::Fear, .action = action});
		return;
	}
	// Happy, under the desire to show its state, as the fizzle shows it is embarrassed
	if (!Replan(creature, creature_mind::Activity::Planned,
	            {{.kind = creature_mind::Step::Kind::Action, .animation = creature_layers::animations::k_Happy}}))
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
	            {{.kind = creature_mind::Step::Kind::Action, .animation = k_Embarrassed}}))
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
	if (Random(2) == 0)
	{
		mind.idle.agenda.push_back(
		    {.kind = creature_mind::Step::Kind::Action, .animation = creature_layers::animations::k_Sad});
		if (mind.desires.has_value())
		{
			creature_desires::ChangeSource(*mind.desires, creature_desires::sources::k_Sadness,
			                               game_random::GameFloatRand(k_SadnessFromFizzle));
		}
	}
}

void CreatureMindSystem::KnowMiracle(entt::entity creature, size_t miracle)
{
	// As the game's scripts teach it a miracle: known, and seen as often as its species must see it
	SetKnowsAction(creature, static_cast<uint32_t>(CreatureActionLearningType::Magic), static_cast<uint32_t>(miracle), true);
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
