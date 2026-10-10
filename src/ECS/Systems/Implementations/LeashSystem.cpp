/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "LeashSystem.h"

#include <cmath>

#include <algorithm>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/LandAvoid.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Audio/Audio.h"
#include "Common/GUtilsDistance.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSpellMind.h"
#include "Creature/LeashKeys.h"
#include "Creature/LeashOwnership.h"
#include "Creature/LeashRules.h"
#include "Creature/LocalPlayer.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/MapScriptGlobals.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerLeash.h"
#include "ECS/Components/ScriptHeld.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TotemStatue.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/CreaturePose.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "ECS/Town/TownBelief.h"
#include "ECS/Town/TownQueries.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using openblack::ecs::Registry;
namespace leash = openblack::creature_leash;

namespace
{
/// Where the leash meets something it is tied to, as a share of its height, and that height when it has no mesh
constexpr float k_TiedHeightShare = 0.5f;
constexpr float k_DefaultObjectHeight = 5.0f;
/// The two sounds of tying a leash, in the in-game bank
constexpr int k_AttachSound = 148;
constexpr int k_SecondAttachSound = 149;

std::optional<glm::vec3> HandPoint()
{
	if (!Locator::handSystem::has_value())
	{
		return std::nullopt;
	}
	return Locator::handSystem::value().GetPlayerHandPositions()[static_cast<size_t>(HandSystemInterface::Side::Left)];
}

float CreatureHeight(const Creature& creature)
{
	return creature_morph::k_HeightAtSizeOne * creature.size;
}

float GroundAt(glm::vec2 point)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
}

/// Where the leash meets the creature: its collar bone as posed this frame on the body as it is drawn (between its
/// turns, and smaller in its temple's pen), or high on that body when it has none. The rope is laid, pulled taut,
/// drawn and picked from this one point
glm::vec3 CollarPoint(const Registry& registry, entt::entity entity)
{
	const auto& creature = registry.Get<const Creature>(entity);
	const auto* animation = registry.TryGet<const CreatureAnimation>(entity);
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto rigId = creature::GetRigId(creature.species);
	std::span<const glm::mat4> bones;
	std::optional<uint32_t> bone;
	if (animation != nullptr && rigs.Contains(rigId))
	{
		bones = animation->boneMatrices;
		bone = rigs.Handle(rigId)->leashBone;
	}
	return leash::CollarAt(ecs::DrawnBodyModel(registry, entity), bones, bone, CreatureHeight(creature),
	                       ecs::creature_pose::DrawnSizeShare(registry, entity));
}

float ObjectHeight(const Registry& registry, entt::entity entity)
{
	const auto& transform = registry.Get<const Transform>(entity);
	if (const auto* creature = registry.TryGet<const Creature>(entity))
	{
		return CreatureHeight(*creature);
	}
	const auto* mesh = registry.TryGet<const Mesh>(entity);
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (mesh == nullptr || !meshes.Contains(mesh->id))
	{
		return k_DefaultObjectHeight;
	}
	const auto box = meshes.Handle(mesh->id)->GetBoundingBox();
	return std::max((box.maxima.y - box.minima.y) * transform.scale.y, 0.0f);
}

/// Where the leash meets something it is tied to: a creature's collar, else halfway up it
glm::vec3 TiedPoint(const Registry& registry, entt::entity entity)
{
	if (registry.TryGet<const Creature>(entity) != nullptr)
	{
		return CollarPoint(registry, entity);
	}
	return registry.Get<const Transform>(entity).position +
	       glm::vec3(0.0f, ObjectHeight(registry, entity) * k_TiedHeightShare, 0.0f);
}

/// A growing tree, not a felled one
bool IsTree(const Registry& registry, entt::entity entity)
{
	return registry.TryGet<const Tree>(entity) != nullptr;
}

/// The leash's lengths: by the creature's size in the hand, by its height tied to a tree, by how far it is from
/// anything else it is tied to. The size and the height are the ones it is drawn at, so its temple's pen shortens them
leash::Lengths LengthsOf(const Registry& registry, entt::entity creature, const CreatureLeash::Worn& worn)
{
	const auto drawnSize = ecs::creature_pose::DrawnSize(registry, creature);
	if (!worn.tiedTo.has_value())
	{
		return leash::InHand(drawnSize);
	}
	if (IsTree(registry, *worn.tiedTo))
	{
		return leash::TiedToTree(creature_morph::k_HeightAtSizeOne * drawnSize);
	}
	const auto& from = registry.Get<const Transform>(creature).position;
	const auto& to = registry.Get<const Transform>(*worn.tiedTo).position;
	return leash::TiedToObject(gutils::GetDistanceInMetres(from, to));
}

/// The entity's component to write to, looked up without making its storage; nothing when it has none
template <typename Component>
Component* Find(Registry& registry, entt::entity entity)
{
	return registry.Valid(entity) && std::as_const(registry).AllOf<Component>(entity) ? &registry.Get<Component>(entity)
	                                                                                  : nullptr;
}

/// One of the in-game bank's samples, not placed in the world
void PlaySound(int sample)
{
	audio::PlayOptions options;
	options.sample = {audio::Bank(audio::SfxBank::InGame), sample};
	options.owner = audio::Owner::None();
	options.is3D = false;
	audio::PlaySoundEffect(options);
}

/// The creatures with a leash component, found without making any storage, in the order of their entities
std::vector<entt::entity> Leashed(const Registry& registry)
{
	std::vector<entt::entity> leashed;
	std::as_const(registry).Each<const CreatureLeash>(
	    [&leashed](entt::entity entity, const CreatureLeash& /*leashes*/) { leashed.push_back(entity); });
	std::ranges::sort(leashed, {}, [](entt::entity entity) { return entt::to_integral(entity); });
	return leashed;
}

/// The creature's leash component, made when it has none
CreatureLeash& LeashOf(Registry& registry, entt::entity creature)
{
	if (std::as_const(registry).AllOf<CreatureLeash>(creature))
	{
		return registry.Get<CreatureLeash>(creature);
	}
	return registry.AssignState<CreatureLeash>(creature);
}

/// The turn two creatures last warmed or cooled to each other, kept on the one leashed to; made when it has none
CreatureLeashAttitude& AttitudeOf(Registry& registry, entt::entity creature)
{
	if (std::as_const(registry).AllOf<CreatureLeashAttitude>(creature))
	{
		return registry.Get<CreatureLeashAttitude>(creature);
	}
	return registry.AssignState<CreatureLeashAttitude>(creature);
}

/// Where its player's temple is, if it has one
std::optional<glm::vec3> TempleOf(const Registry& registry, PlayerNames owner)
{
	std::optional<glm::vec3> temple;
	std::as_const(registry).Each<const Temple, const Transform>([&temple, owner](const Temple& building, const Transform& transform) {
		if (building.owner == owner && !temple.has_value())
		{
			temple = transform.position;
		}
	});
	return temple;
}

/// What decides whether a young creature is kept at its home this turn
leash::HomeKeeping HomeKeepingFor(const Registry& registry, entt::entity creature)
{
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	return {
	    .leashed = leashes != nullptr && leashes->worn.has_value(),
	    .developmentPhase = mind != nullptr ? mind->developmentPhase : CreatureMindState::k_FullyGrownUp,
	    .localPlayers = body != nullptr && body->owner == creature::LocalPlayer(),
	    // openblack keeps no player kind yet: no player is taken for a computer player
	    .computerPlayer = false,
	    .multiplayer = false,
	    .landNumber = Locator::mapScriptSystem::has_value() ? Locator::mapScriptSystem::value().Globals().landNumber : 0,
	};
}

CreatureMindState* MindOf(Registry& registry, entt::entity creature)
{
	return Find<CreatureMindState>(registry, creature);
}

/// The whole turns a second a desire made dominant or held back for a while is counted in
float DominantTurnsPerSecond()
{
	const auto msPerTurn = game_clock::MsPerTurn();
	return msPerTurn > 0 ? static_cast<float>(1000 / msPerTurn) : 0.0f;
}

/// The least the species wants any desire, where another is made fully dominant
float DesireFloor(CreatureType species)
{
	if (!Locator::infoConstants::has_value())
	{
		return 0.0f;
	}
	const auto& creatures = Locator::infoConstants::value().creature;
	const auto row = creature::InfoRow(species);
	return row < creatures.size() ? creatures.at(row).desireFloor : 0.0f;
}

/// A desire made dominant over the creature's others for some seconds, its body's needs left free
void MakeDominant(Registry& registry, entt::entity creature, creature_desires::Desire desire, float seconds)
{
	auto* mind = MindOf(registry, creature);
	const auto* body = std::as_const(registry).TryGet<const Creature>(creature);
	if (mind == nullptr || body == nullptr || !mind->desires.has_value())
	{
		return;
	}
	mind->dominantDesire = creature_spell_mind::SetCheatDominant(*mind->desires, desire, false, DominantTurnsPerSecond(),
	                                                             DesireFloor(body->species), seconds);
}

/// No desire is dominant over the creature's others any more, and every one held down is let go of
void EndDominance(Registry& registry, entt::entity creature)
{
	if (auto* mind = MindOf(registry, creature); mind != nullptr && mind->desires.has_value())
	{
		creature_spell_mind::ClearCheatDominance(*mind->desires, mind->dominantDesire);
	}
}

/// What a leash makes the creature feel as it is put on or changed: its desire made dominant, or, on the learning
/// leash, whatever desire was dominant let go of. The game lets go only while its player's leash is the learning leash,
/// which it has just become
void TakeMoodOf(Registry& registry, entt::entity creature, LeashType type)
{
	if (const auto desire = leash::ForcedDesireFor(type))
	{
		MakeDominant(registry, creature, *desire, leash::k_LeashDesireSeconds);
	}
	else if (type == LeashType::Rope)
	{
		EndDominance(registry, creature);
	}
}

/// The creature's walkable mask at a map cell; with no land open, as off the map
uint8_t LandAvoidAt(glm::ivec2 cell)
{
	return Locator::landAvoidSystem::has_value() ? land_avoid::At(cell.x, cell.y) : land_avoid::k_Unreachable;
}

bool ControlledByScript(const Registry& registry, entt::entity entity)
{
	const auto* held = registry.TryGet<const ScriptHeld>(entity);
	return held != nullptr && held->controlledByScript;
}

/// A plan the leash makes the creature carry out: to obey the player, about itself, with an action of the game's table
creature_planner::Plan ObeyPlan(entt::entity creature, uint32_t action)
{
	return {.desire = creature_desires::Desire::ObeyPlayer, .action = action, .object = static_cast<uint32_t>(creature)};
}

/// The creature's mind drops the plan it carried out and stops what it was doing, for a plan the leash gives it
void ReplacePlan(CreatureMindState& mind, entt::entity creature, uint32_t action)
{
	mind.planActive = false;
	mind.planner.current = ObeyPlan(creature, action);
	creature_mind::Plan(mind.idle, creature_mind::Activity::None, {});
}

/// Sent walking to a point by the leash or its home: it remembers where to, and its mind gives up what it was doing to
/// walk there, obeying the player until the walk is over
void SendWalking(Registry& registry, entt::entity creature, CreatureLeash& leashes, const glm::vec3& point)
{
	leashes.returning = true;
	leashes.returningTo = point;
	if (auto* mind = MindOf(registry, creature))
	{
		ReplacePlan(*mind, creature, leash::k_WalkToPointAction);
		mind->leash.obeying = true;
	}
}

/// Strayed farther than the radius from the point it is kept near, the creature stops what it was doing and walks back,
/// the faster the farther it strayed, until it is within its height or the radius of the point. Walking back already, it
/// sets off again only once the point has moved far enough from where it walks to.
void KeepWithin(Registry& registry, entt::entity entity, CreatureLeash& leashes, bool leashed, bool leashWorks)
{
	const auto radius = leashes.confinementRadius;
	if (!leash::IsConfined(radius, leashed, leashWorks) || !Locator::creatureLocomotionSystem::has_value())
	{
		return;
	}
	const auto& lookup = std::as_const(registry);
	const auto point = leashes.confinementCentre;
	const leash::WalkBackCheck check {
	    .confined = true,
	    .controlledByScript = ControlledByScript(lookup, entity),
	    .pointOnMap = map_coords::InBounds(point),
	    .pointOnLand = LandAvoidAt(map_coords::CellOf(point)) == land_avoid::k_Land,
	    .distance = gutils::GetDistanceInMetres(lookup.Get<const Transform>(entity).position, point),
	    .radius = radius,
	    .walkingBack = leashes.returning,
	    .walkingToFromPoint = gutils::GetDistanceInMetres(leashes.returningTo, point),
	};
	if (!leash::ShouldWalkBack(check))
	{
		return;
	}
	const auto arrival = leash::WalkBackArrival(CreatureHeight(lookup.Get<const Creature>(entity)), radius);
	// A point it can't stand on refuses the walk, and a refused walk ends what the creature was doing, its stop
	// included, as the original's failed move does; a walk that starts is not stopped. It is sent all the same: the
	// original's walk fails only once its mind has passed for the turn, so it obeys this turn and is sent again the
	// next, as long as the point stays out of reach
	auto& locomotion = Locator::creatureLocomotionSystem::value();
	const auto hurry = leash::WalkBackHurry(check.distance, radius);
	const auto walk = locomotion.WalkBack(entity, glm::vec2(point.x, point.z), hurry, arrival);
	if (walk != CreatureLocomotionSystemInterface::MoveResult::Started)
	{
		locomotion.Stop(entity);
	}
	SendWalking(registry, entity, leashes, point);
	if (leashed)
	{
		leashes.control = CreatureLeash::Control::Led;
	}
}

bool IsTownCentre(const Registry& registry, entt::entity object)
{
	return registry.TryGet<const Abode>(object) != nullptr && ecs::abodes::TypeOf(object) == AbodeType::TownCentre;
}

/// The village a leash tied to the object holds the creature by, that of a village centre or of the centre a totem
/// statue stands by, and what it thinks of the creature's player; nothing for anything else
std::optional<leash::TiedTown> TownTiedTo(const Registry& registry, entt::entity object, PlayerNames player)
{
	auto centre = IsTownCentre(registry, object) ? object : entt::entity {entt::null};
	if (const auto* totem = registry.TryGet<const TotemStatue>(object))
	{
		centre = totem->townCentre;
	}
	if (centre == entt::null)
	{
		return std::nullopt;
	}
	const auto town = ecs::town_queries::GetTown(centre);
	const auto* village = town != entt::null ? registry.TryGet<const Town>(town) : nullptr;
	if (village == nullptr)
	{
		return std::nullopt;
	}
	return leash::TiedTown {
	    .beliefInPlayer = ecs::town_belief::GetBeliefInPlayer(village->belief, player),
	    .mostInAnother = ecs::town_belief::GetMaxBeliefMeNotIncluded(village->belief, player),
	    .playersOwn = village->owner == player,
	};
}

/// What the leash shortcuts need to know about a creature
leash::KeyState KeyStateOf(const Registry& registry, entt::entity creature)
{
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	if (leashes == nullptr)
	{
		return {};
	}
	return {
	    .worn = leashes->worn.has_value(),
	    .tied = leashes->worn.has_value() && leashes->worn->tiedTo.has_value(),
	    .known = leashes->known,
	    .selected = leashes->worn.has_value() ? leashes->worn->type : leashes->selected,
	};
}

int PlayerNumber(PlayerNames player)
{
	return static_cast<int>(player) + 1;
}

/// The player's entity on this land, the first with their name, looked up through the const registry so that no
/// storage is made
std::optional<entt::entity> LandPlayerEntity(const Registry& registry, PlayerNames player)
{
	std::optional<entt::entity> found;
	registry.Each<const Player>([&found, player](entt::entity entity, const Player& component) {
		if (!found.has_value() && component.name == player)
		{
			found = entity;
		}
	});
	return found;
}

/// A refusal is kept as the player's last, on their entity; a player with no entity (the neutral one) keeps none
void KeepRefusal(Registry& registry, PlayerNames player, entt::entity creature, leash::Refusal why)
{
	if (const auto entity = LandPlayerEntity(std::as_const(registry), player); entity.has_value())
	{
		registry.AssignOrReplaceState<PlayerLeashRefusal>(*entity, PlayerLeashRefusal {.creature = creature, .why = why});
	}
}
} // namespace

bool LeashSystem::Knows(entt::entity creature, LeashType type) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	const auto index = leash::IndexOf(type);
	return leashes != nullptr && index.has_value() && leashes->known.test(*index);
}

void LeashSystem::SetKnown(entt::entity creature, LeashType type, bool known)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto index = leash::IndexOf(type);
	if (!index.has_value() || !registry.Valid(creature) || !std::as_const(registry).AllOf<Creature>(creature))
	{
		return;
	}
	auto& leashes = LeashOf(registry, creature);
	leashes.known.set(*index, known);
	// Forgetting the learning leash takes any leash off, as no leash can be worn without it
	if (!known && leashes.worn.has_value() && (leashes.worn->type == type || type == LeashType::Rope))
	{
		TakeOff(creature);
	}
}

std::vector<leash::Claim> LeashSystem::Claims() const
{
	const auto& registry = Locator::entitiesRegistry::value();
	std::vector<leash::Claim> claims;
	std::as_const(registry).Each<const Creature>([&claims](entt::entity entity, const Creature& creature) {
		claims.push_back({.creature = entt::to_integral(entity), .owner = creature.owner, .leashable = creature.leashable});
	});
	return claims;
}

bool LeashSystem::IsLeashable(entt::entity creature) const
{
	const auto* body = std::as_const(Locator::entitiesRegistry::value()).TryGet<const Creature>(creature);
	return body != nullptr && body->leashable;
}

bool LeashSystem::SetLeashable(entt::entity creature, bool leashable)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* body = Find<Creature>(registry, creature);
	if (body == nullptr)
	{
		return false;
	}
	const auto owner = body->owner;
	if (!leashable)
	{
		if (body->leashable)
		{
			TakeOff(creature);
			registry.Get<Creature>(creature).leashable = false;
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} is no longer the one player {} leads",
			                   entt::to_integral(creature), PlayerNumber(owner));
		}
		return true;
	}
	if (!leash::CanLead(owner))
	{
		Refuse(owner, creature, leash::Refusal::NoPlayer);
		return false;
	}
	// A player leads one creature: the one chosen last
	for (const auto id : leash::Displaced(Claims(), entt::to_integral(creature), owner))
	{
		const auto other = static_cast<entt::entity>(id);
		TakeOff(other);
		registry.Get<Creature>(other).leashable = false;
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} is no longer the one player {} leads", id, PlayerNumber(owner));
	}
	if (!registry.Get<Creature>(creature).leashable)
	{
		registry.Get<Creature>(creature).leashable = true;
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} is now the one player {} leads", entt::to_integral(creature),
		                   PlayerNumber(owner));
	}
	return true;
}

void LeashSystem::SetOwner(entt::entity creature, PlayerNames owner)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* body = Find<Creature>(registry, creature);
	if (body == nullptr || body->owner == owner)
	{
		return;
	}
	TakeOff(creature);
	auto& changed = registry.Get<Creature>(creature);
	changed.owner = owner;
	// It stays the one its new owner leads only if they have no other
	if (changed.leashable &&
	    (!leash::CanLead(owner) || !leash::Displaced(Claims(), entt::to_integral(creature), owner).empty()))
	{
		registry.Get<Creature>(creature).leashable = false;
	}
}

void LeashSystem::ClaimOnArrival(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr || body->leashable)
	{
		return;
	}
	auto others = Claims();
	std::erase_if(others, [creature](const leash::Claim& claim) { return claim.creature == entt::to_integral(creature); });
	if (leash::ClaimsOnArrival(others, body->owner))
	{
		registry.Get<Creature>(creature).leashable = true;
	}
}

leash::Refusal LeashSystem::WhyNot(PlayerNames player, entt::entity creature, LeashType type) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
	if (body == nullptr)
	{
		return leash::Refusal::NotACreature;
	}
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	return leash::WhyNot(
	    player,
	    {
	        .owner = body->owner,
	        .leashable = body->leashable,
	        .knowsLearningLeash = Knows(creature, LeashType::Rope),
	        .knowsType = Knows(creature, type),
	        .heldBy = leashes != nullptr && leashes->worn.has_value() ? std::optional(leashes->worn->holder) : std::nullopt,
	    });
}

std::optional<LeashSystemInterface::Refused> LeashSystem::LastRefusal(PlayerNames player) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto entity = LandPlayerEntity(registry, player);
	const auto* refusal = entity.has_value() ? registry.TryGet<const PlayerLeashRefusal>(*entity) : nullptr;
	if (refusal == nullptr)
	{
		return std::nullopt;
	}
	return Refused {.player = player, .creature = refusal->creature, .why = refusal->why};
}

void LeashSystem::Refuse(PlayerNames player, entt::entity creature, leash::Refusal why)
{
	KeepRefusal(Locator::entitiesRegistry::value(), player, creature, why);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Player {} can't leash creature {}: {}", PlayerNumber(player),
	                   entt::to_integral(creature), leash::Describe(why));
}

bool LeashSystem::PutOn(entt::entity creature, LeashType type)
{
	const auto* body = Locator::entitiesRegistry::value().TryGet<const Creature>(creature);
	if (body == nullptr)
	{
		Refuse(PlayerNames::NEUTRAL, creature, leash::Refusal::NotACreature);
		return false;
	}
	// Whoever puts it on, it is held by the creature's owner
	return PutOnFor(body->owner, creature, type);
}

bool LeashSystem::PutOnFor(PlayerNames player, entt::entity creature, LeashType type)
{
	if (const auto why = WhyNot(player, creature, type); why != leash::Refusal::None)
	{
		Refuse(player, creature, why);
		return false;
	}
	PutOnUnchecked(player, creature, type);
	return true;
}

void LeashSystem::PutOnUnchecked(PlayerNames player, entt::entity creature, LeashType type)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* leashes = &registry.Get<CreatureLeash>(creature);
	if (!leashes->worn.has_value())
	{
		// Assigned rather than emplaced: clang can't yet see that the nested type is default constructible
		leashes->worn = CreatureLeash::Worn {};
		leashes->worn->holder = player;
	}
	leashes->worn->type = type;
	leashes->selected = type;
	leashes->picked = true;
	leashes->control = CreatureLeash::Control::Idle;
	// Its lengths are taken as it goes on, so that its first turn keeps the creature within them
	if (!leashes->worn->tiedTo.has_value())
	{
		const auto lengths = LengthsOf(registry, creature, *leashes->worn);
		leashes->worn->rope.slackLength = lengths.slack;
		leashes->worn->rope.maxLength = lengths.max;
	}
	// The rope is laid afresh from the hand to the collar next frame
	leashes->worn->ropeStarted = false;
	TakeMoodOf(registry, creature, type);
}

void LeashSystem::TakeOff(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* leashes = Find<CreatureLeash>(registry, creature);
	if (leashes == nullptr || !leashes->worn.has_value())
	{
		return;
	}
	// Any leash but the learning leash lets go of the desire made dominant, the leash's own or a village's
	if (leashes->worn->type != LeashType::Rope)
	{
		EndDominance(registry, creature);
	}
	leashes->worn.reset();
	leashes->control = CreatureLeash::Control::Idle;
	leashes->pull = 0.0f;
	leashes->confinementRadius = 0.0f;
	if (auto* mind = MindOf(registry, creature))
	{
		// A walk back it was sent on goes on to its end
		mind->leash.obeying = mind->leash.obeying && leashes->returning;
		mind->leash.learningInHand = false;
		mind->leash.miracleSightingWeight = leash::MiracleSightingWeight(false);
	}
}

bool LeashSystem::Toggle(entt::entity creature)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr)
	{
		Refuse(PlayerNames::NEUTRAL, creature, leash::Refusal::NotACreature);
		return false;
	}
	// As the leash key does, for the creature's owner
	return Carry(body->owner, creature, leash::CommandFor(leash::LeashKey::Leash, KeyStateOf(registry, creature)));
}

bool LeashSystem::Carry(PlayerNames player, entt::entity creature, const leash::KeyCommand& command)
{
	using Kind = leash::KeyCommand::Kind;
	const auto checked = command.kind == Kind::PutOn || command.kind == Kind::ChangeType ? command.type : LeashType::Rope;
	if (const auto why = WhyNot(player, creature, checked); why != leash::Refusal::None)
	{
		Refuse(player, creature, why);
		return false;
	}
	switch (command.kind)
	{
	case Kind::None:
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} knows no other leash to pick", entt::to_integral(creature));
		return false;
	case Kind::PutOn:
		return PutOnFor(player, creature, command.type);
	case Kind::TakeOff:
		TakeOff(creature);
		return true;
	case Kind::UntieToHand:
		UntieToHand(creature);
		return true;
	case Kind::ChangeType:
		return ChangeType(creature, command.type);
	}
	return false;
}

bool LeashSystem::PressKey(PlayerNames player, leash::LeashKey key)
{
	const auto creature = PlayersCreature(player);
	if (!creature.has_value())
	{
		KeepRefusal(Locator::entitiesRegistry::value(), player, entt::null, leash::Refusal::NotACreature);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Player {} has no creature they can lead", PlayerNumber(player));
		return false;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	return Carry(player, *creature, leash::CommandFor(key, KeyStateOf(registry, *creature)));
}

bool LeashSystem::TakeOffHeldLeash(PlayerNames player)
{
	const auto creature = PlayersCreature(player);
	if (!creature.has_value() || !IsLeashed(*creature) || TiedTo(*creature).has_value())
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Player {} has no leash held in the hand to take off", PlayerNumber(player));
		return false;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Player {} took the leash off creature {}", PlayerNumber(player),
	                   entt::to_integral(*creature));
	TakeOff(*creature);
	return true;
}

bool LeashSystem::TapCreature(PlayerNames player, entt::entity creature)
{
	if (IsLeashed(creature))
	{
		// Already on: tapping its own creature again does nothing, and another's is refused
		if (const auto why = WhyNot(player, creature, TypeOf(creature)); why != leash::Refusal::None)
		{
			Refuse(player, creature, why);
		}
		return false;
	}
	const auto* leashes = Locator::entitiesRegistry::value().TryGet<const CreatureLeash>(creature);
	const auto picked = leashes != nullptr ? leashes->selected : LeashType::Rope;
	return PutOnFor(player, creature, Knows(creature, picked) ? picked : LeashType::Rope);
}

bool LeashSystem::ChangeType(entt::entity creature, LeashType type)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* leashes = Find<CreatureLeash>(registry, creature);
	if (leashes == nullptr || !Knows(creature, type))
	{
		return false;
	}
	leashes->selected = type;
	leashes->picked = true;
	if (leashes->worn.has_value())
	{
		leashes->worn->type = type;
		leashes->worn->rope.look = leash::LookFor(type);
		TakeMoodOf(registry, creature, type);
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {}'s picked leash is now the {} leash", entt::to_integral(creature),
	                   leash::Name(type));
	return true;
}

bool LeashSystem::TieTo(entt::entity creature, entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (object == creature || !registry.Valid(object) || registry.TryGet<const Transform>(object) == nullptr)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {}'s leash can't be tied to that", entt::to_integral(creature));
		return false;
	}
	if (!IsLeashed(creature))
	{
		const auto* picked = registry.TryGet<const CreatureLeash>(creature);
		if (!PutOn(creature, picked != nullptr ? picked->selected : LeashType::Rope))
		{
			return false;
		}
	}
	else
	{
		// Tying puts the leash on again, with its mood
		TakeMoodOf(registry, creature, registry.Get<const CreatureLeash>(creature).worn->type);
	}
	auto* leashes = &registry.Get<CreatureLeash>(creature);
	auto& worn = *leashes->worn;
	worn.tiedTo = object;
	worn.tiedTurn = game_clock::Turn();
	const auto lengths = LengthsOf(registry, creature, worn);
	worn.rope.slackLength = lengths.slack;
	worn.rope.maxLength = lengths.max;
	worn.ropeStarted = false;
	leashes->control = CreatureLeash::Control::Idle;

	// The two tying sounds in turn
	PlaySound(_secondAttachSound ? k_SecondAttachSound : k_AttachSound);
	_secondAttachSound = !_secondAttachSound;

	// The creature learns that the player wants something done with what it is tied to
	if (auto* mind = MindOf(registry, creature))
	{
		mind->leash.obeying = false;
		const bool isCreature = registry.TryGet<const Creature>(object) != nullptr;
		mind->leash.shown.push_back({
		    .object = static_cast<uint32_t>(object),
		    .type = worn.type,
		    .lessons = leash::LessonsFor(worn.type, isCreature),
		});
		mind->leash.actOn.push_back(static_cast<uint32_t>(object));
	}
	// Tied to a village's centre on any leash but aggression, it wants to impress the village for a while
	if (worn.type != LeashType::Evil && IsTownCentre(std::as_const(registry), object))
	{
		MakeDominant(registry, creature, creature_desires::Desire::Impress, leash::k_ImpressTownSeconds);
	}
	return true;
}

void LeashSystem::UntieToHand(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* leashes = Find<CreatureLeash>(registry, creature);
	if (leashes == nullptr || !leashes->worn.has_value() || !leashes->worn->tiedTo.has_value())
	{
		return;
	}
	leashes->worn->tiedTo.reset();
	leashes->worn->ropeStarted = false;
}

void LeashSystem::ReturnToHand(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* leashes = Find<CreatureLeash>(registry, creature);
	if (leashes == nullptr || !leashes->worn.has_value())
	{
		return;
	}
	const auto holder = leashes->worn->holder;
	const auto type = leashes->worn->type;
	const auto works = leashes->worn->works;
	// Off, then on again in the holder's hand, untied; whether it works belongs to the player's leash and stays. It
	// goes back on with no rule asked, only that the creature knows the learning leash; without it the leash stays off
	TakeOff(creature);
	if (Knows(creature, LeashType::Rope))
	{
		PutOnUnchecked(holder, creature, type);
		registry.Get<CreatureLeash>(creature).worn->works = works;
	}
	// The two sounds in turn, the first time the first, for the player at this machine only, whether or not the leash
	// went back on
	if (holder == creature::LocalPlayer())
	{
		PlaySound(_secondUntieSound ? k_SecondAttachSound : k_AttachSound);
		_secondUntieSound = !_secondUntieSound;
	}
}

void LeashSystem::SetWorks(entt::entity creature, bool works)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* leashes = Find<CreatureLeash>(registry, creature); leashes != nullptr && leashes->worn.has_value())
	{
		leashes->worn->works = works;
	}
}

bool LeashSystem::Works(entt::entity creature) const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* leashes = registry.Valid(creature) ? registry.TryGet<const CreatureLeash>(creature) : nullptr;
	return leashes != nullptr && leashes->worn.has_value() && leashes->worn->works;
}

void LeashSystem::PullAwayFromAction(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (Find<Creature>(registry, creature) == nullptr)
	{
		return;
	}
	// As an action finished unsuccessfully: it stops where it is, a walk back is over, it is no longer led, and its
	// forced plan, which is what obeying stands for, is cleared
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().Stop(creature);
	}
	if (auto* leashes = Find<CreatureLeash>(registry, creature))
	{
		leashes->returning = false;
		leashes->control = CreatureLeash::Control::Idle;
	}
	// Its mind no longer obeys and gives up its plan, which leaves it no desire, so it makes a new one
	if (auto* mind = MindOf(registry, creature))
	{
		mind->leash.obeying = false;
		mind->planActive = false;
		mind->planner.current.reset();
		creature_mind::Plan(mind->idle, creature_mind::Activity::None, {});
	}
}

void LeashSystem::ActOn(entt::entity creature, entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = MindOf(registry, creature);
	if (mind == nullptr || object == creature || !registry.Valid(object))
	{
		return;
	}
	// Every call adds it, even twice in a row, as every act is a new one. The repeat is harmless: the mind takes only
	// the last thing and clears the rest, and a creature already fighting the first time is busy the second
	mind->leash.actOn.push_back(static_cast<uint32_t>(object));
}

void LeashSystem::ConfineToHome(entt::entity creature, float radius)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !std::as_const(registry).AllOf<Creature>(creature))
	{
		return;
	}
	auto& leashes = LeashOf(registry, creature);
	if (!leashes.home.has_value())
	{
		// Its home is by its player's temple, or where it stands when there is none
		leashes.home = TempleOf(registry, registry.Get<const Creature>(creature).owner);
		if (!leashes.home.has_value())
		{
			leashes.home = registry.Get<const Transform>(creature).position;
		}
	}
	leashes.confinementCentre = *leashes.home;
	leashes.confinementRadius = radius;
}

void LeashSystem::ClearConfinement(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* leashes = Find<CreatureLeash>(registry, creature))
	{
		leashes->confinementRadius = 0.0f;
		leashes->returning = false;
	}
}

void LeashSystem::SetHome(entt::entity creature, const glm::vec3& home)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !std::as_const(registry).AllOf<Creature>(creature))
	{
		return;
	}
	LeashOf(registry, creature).home = home;
}

bool LeashSystem::FreeOfHome(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (body == nullptr || transform == nullptr)
	{
		return false;
	}
	const auto temple = TempleOf(registry, body->owner);
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	const auto home = leashes != nullptr && leashes->home.has_value() ? leashes->home : temple;
	if (!home.has_value())
	{
		return false;
	}
	return leash::FreeOfHome(glm::distance(transform->position, *home), temple.has_value());
}

leash::HomeKeeping LeashSystem::HomeKeepingOf(entt::entity creature) const
{
	return HomeKeepingFor(Locator::entitiesRegistry::value(), creature);
}

bool LeashSystem::IsLeashed(entt::entity creature) const
{
	const auto* leashes = std::as_const(Locator::entitiesRegistry::value()).TryGet<const CreatureLeash>(creature);
	return leashes != nullptr && leashes->worn.has_value();
}

std::optional<entt::entity> LeashSystem::TiedTo(entt::entity creature) const
{
	const auto* leashes = std::as_const(Locator::entitiesRegistry::value()).TryGet<const CreatureLeash>(creature);
	return leashes != nullptr && leashes->worn.has_value() ? leashes->worn->tiedTo : std::nullopt;
}

LeashType LeashSystem::TypeOf(entt::entity creature) const
{
	const auto* leashes = std::as_const(Locator::entitiesRegistry::value()).TryGet<const CreatureLeash>(creature);
	return leashes != nullptr && leashes->worn.has_value() ? leashes->worn->type : LeashType::None;
}

LeashType LeashSystem::Picked(entt::entity creature) const
{
	const auto* leashes = std::as_const(Locator::entitiesRegistry::value()).TryGet<const CreatureLeash>(creature);
	if (leashes == nullptr)
	{
		return LeashType::None;
	}
	if (leashes->worn.has_value())
	{
		return leashes->worn->type;
	}
	// nothing picked yet: none, as the temple's leash reports before a pick
	return leashes->picked ? leashes->selected : LeashType::None;
}

std::optional<entt::entity> LeashSystem::PlayersCreature(PlayerNames player) const
{
	if (const auto id = leash::LeashableOf(Claims(), player))
	{
		return static_cast<entt::entity>(*id);
	}
	return std::nullopt;
}

void LeashSystem::Tug(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* leashes = Find<CreatureLeash>(registry, creature);
	const auto hand = HandPoint();
	// A tug on a tied leash leads the creature onto what it is tied to, which is not here yet
	if (leashes == nullptr || !leashes->worn.has_value() || leashes->worn->tiedTo.has_value() || !hand.has_value() ||
	    !Locator::creatureLocomotionSystem::has_value() || !std::as_const(registry).AllOf<Creature, Transform>(creature))
	{
		return;
	}
	auto& locomotion = Locator::creatureLocomotionSystem::value();
	auto* mind = MindOf(registry, creature);
	const auto& lookup = std::as_const(registry);
	const auto position = lookup.Get<const Transform>(creature).position;
	const auto handOnMap = glm::vec2(hand->x, hand->z);
	const auto* walk = lookup.TryGet<const CreatureLocomotion>(creature);
	const auto self = static_cast<uint32_t>(creature);
	const bool led = leashes->control == CreatureLeash::Control::Led;
	std::optional<float> routeEndToHand;
	if (led && walk != nullptr && walk->destination.has_value())
	{
		routeEndToHand = gutils::GetDistanceInMetres(*walk->destination, handOnMap);
	}
	const auto tug = leash::DecideTug({
	    .walkingBack = leashes->returning,
	    .controlledByScript = ControlledByScript(lookup, creature),
	    .led = led,
	    .planOnOther = mind != nullptr && mind->planner.current.has_value() && mind->planner.current->object != self,
	    .tied = false,
	    .bodyToHand = glm::distance(position, *hand),
	    .creatureToHand = gutils::GetDistanceInMetres(glm::vec2(position.x, position.z), handOnMap),
	    .routeEndToHand = routeEndToHand,
	    .sentToFromHand = gutils::GetDistanceInMetres(glm::vec2(leashes->returningTo.x, leashes->returningTo.z), handOnMap),
	});
	// Not led yet, it is pulled away from the plan it carried out, to go to the hand. Pulled away from a plan for the
	// same desire a second time, the desire is held back a while. A plan the leash or the player made it carry out
	// doesn't count, nor the walk the leash sent it on once over: the original's walk ends by stopping what it was
	// doing, which leaves its plan with no desire, and openblack's mind keeps that plan until it makes its own
	if (tug.pulledAway && mind != nullptr)
	{
		const auto& plan = mind->planner.current;
		const bool walkSentOn = plan.has_value() && plan->desire == creature_desires::Desire::ObeyPlayer &&
		                        plan->action == leash::k_WalkToPointAction && plan->object == self;
		if (plan.has_value() && !mind->leash.obeying && !walkSentOn)
		{
			const auto desire = plan->desire;
			if (const auto seconds = leash::RecordPull(leashes->pulls, desire);
			    seconds.has_value() && mind->desires.has_value())
			{
				creature_desires::Suppress(*mind->desires, desire, *seconds, DominantTurnsPerSecond());
			}
		}
		ReplacePlan(*mind, creature, leash::k_GoToHandAction);
	}
	if (tug.lead != leash::Lead::GoToHand)
	{
		return;
	}
	// Off to the hand, at its walk sped up by how hard it was pulled before, until it is within its height or the leash's
	// full length of it; from then on it is pulled along as hard as can be. With the hand where it can't stand, the
	// refused lead ends what it was doing, its stop included, and it is sent all the same, as on the walk back
	const auto arrival =
	    leash::WalkBackArrival(CreatureHeight(lookup.Get<const Creature>(creature)), leashes->confinementRadius);
	const auto toHand = locomotion.LeadTo(creature, handOnMap, leashes->pull, arrival);
	if (toHand != CreatureLocomotionSystemInterface::MoveResult::Started)
	{
		locomotion.Stop(creature);
	}
	SendWalking(registry, creature, *leashes, glm::vec3(hand->x, 0.0f, hand->z));
	leashes->pull = 1.0f;
	leashes->control = CreatureLeash::Control::Led;
}

void LeashSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto entity : Leashed(registry))
	{
		auto& leashes = registry.Get<CreatureLeash>(entity);
		auto* mind = MindOf(registry, entity);
		const auto* body = registry.TryGet<const Creature>(entity);
		const auto* transform = registry.TryGet<const Transform>(entity);
		if (body == nullptr || transform == nullptr)
		{
			continue;
		}
		// How hard it was pulled fades once a turn, whatever it is doing
		leashes.pull = leash::FadePull(leashes.pull);
		// Fighting or knocked out, the leash doesn't move it about. Its moods, the area it is kept within and how it
		// warms to a creature it is tied to go on all the same, as the game's creature turn takes them whatever the
		// creature is doing
		const bool fighting = registry.AnyOf<CreatureFighting, CreatureKnockedOut>(entity);
		const bool moving =
		    Locator::creatureLocomotionSystem::has_value() && Locator::creatureLocomotionSystem::value().IsMoving(entity);

		// The walk the leash or its home sent it on is over once it stops, arrived or given up on the way: its mind
		// takes over again
		if (!fighting && (leashes.control == CreatureLeash::Control::Led || leashes.returning) && !moving)
		{
			leashes.control = CreatureLeash::Control::Idle;
			leashes.returning = false;
			if (mind != nullptr)
			{
				mind->leash.obeying = false;
			}
		}

		if (!leashes.worn.has_value())
		{
			if (mind != nullptr)
			{
				// Still walking back where a leash taken off or its home sent it, it obeys until the walk is over
				mind->leash.obeying = mind->leash.obeying && leashes.returning;
				mind->leash.learningInHand = false;
				mind->leash.miracleSightingWeight = leash::MiracleSightingWeight(false);
			}
			// Young, on the first land, it is kept near its home
			if (leashes.home.has_value() && leash::KeptAtHome(HomeKeepingFor(registry, entity)))
			{
				leashes.confinementCentre = *leashes.home;
				leashes.confinementRadius = leash::k_YoungHomeRadius;
			}
			if (!fighting)
			{
				KeepWithin(registry, entity, leashes, false, true);
			}
			continue;
		}

		// Made someone else's or no longer the one its owner leads, the leash comes off
		if (!body->leashable || leashes.worn->holder != body->owner)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {}'s leash comes off: it isn't its holder's to lead",
			                   entt::to_integral(entity));
			TakeOff(entity);
			continue;
		}
		auto& worn = *leashes.worn;
		if (worn.tiedTo.has_value() &&
		    (!registry.Valid(*worn.tiedTo) || registry.TryGet<const Transform>(*worn.tiedTo) == nullptr))
		{
			worn.tiedTo.reset();
			worn.ropeStarted = false;
		}
		// It is kept as near what holds the leash as the leash is long at full stretch: what it is tied to, or the hand's
		// place on the ground. With the hand off the land, it stays kept where it was
		const auto hand = HandPoint();
		if (worn.tiedTo.has_value())
		{
			leashes.confinementCentre = registry.Get<const Transform>(*worn.tiedTo).position;
			leashes.confinementRadius = worn.rope.maxLength;
		}
		else if (hand.has_value())
		{
			leashes.confinementCentre = glm::vec3(hand->x, 0.0f, hand->z);
			leashes.confinementRadius = worn.rope.maxLength;
		}

		if (mind != nullptr)
		{
			mind->leash.learningInHand = worn.type == LeashType::Rope && !worn.tiedTo.has_value();
			mind->leash.miracleSightingWeight = leash::MiracleSightingWeight(worn.type == LeashType::Rope);
		}
		// Tied to a village's centre, or to the totem statue by one, on any leash but aggression, it wants to impress the
		// village while that does not believe in its player enough, unless that is already what it wants most
		if (worn.tiedTo.has_value() && worn.type != LeashType::Evil && mind != nullptr && mind->desires.has_value() &&
		    creature_desires::FindDominant(*mind->desires) != creature_desires::Desire::Impress)
		{
			if (const auto town = TownTiedTo(std::as_const(registry), *worn.tiedTo, body->owner);
			    town.has_value() && leash::WantsToImpress(*town))
			{
				MakeDominant(registry, entity, creature_desires::Desire::Impress, leash::k_ImpressTownSeconds);
			}
		}
		// The leash's mood, made dominant again every turn
		if (const auto desire = leash::ForcedDesireFor(worn.type))
		{
			MakeDominant(registry, entity, *desire, leash::k_LeashDesireSeconds);
		}

		if (worn.tiedTo.has_value())
		{
			const auto object = *worn.tiedTo;
			// Tied to another creature: anger spreads on the aggression leash to one near enough that is not angry above
			// all already, and they warm or cool to each other
			if (registry.TryGet<const Creature>(object) != nullptr)
			{
				auto* otherMind = MindOf(registry, object);
				const auto otherAt = registry.Get<const Transform>(object).position;
				if (worn.type == LeashType::Evil && otherMind != nullptr && otherMind->desires.has_value() &&
				    creature_desires::FindDominant(*otherMind->desires) != creature_desires::Desire::Anger &&
				    gutils::GetDistanceInMetres(otherAt, transform->position) <
				        leash::k_AngerOtherReach * CreatureHeight(*body))
				{
					MakeDominant(registry, object, creature_desires::Desire::Anger, leash::k_LeashDesireSeconds);
				}
				// The turn of their last step is the other creature's
				auto& attitude = AttitudeOf(registry, object);
				if (const auto turn = game_clock::Turn(); leash::AttitudeStepDue(attitude.lastStep, turn))
				{
					attitude.lastStep = turn;
					// Taken on every leash, the learning leash's step being none
					const auto change = leash::AttitudeStep(worn.type);
					if (mind != nullptr)
					{
						mind->leash.attitudes.push_back({.creature = static_cast<uint32_t>(object), .change = change});
					}
					if (otherMind != nullptr)
					{
						otherMind->leash.attitudes.push_back({.creature = static_cast<uint32_t>(entity), .change = change});
					}
				}
			}
		}

		// Strayed beyond the leash's full length, it walks back to the hand or to what it is tied to
		if (!fighting)
		{
			KeepWithin(registry, entity, leashes, true, worn.works);
		}
	}
}

void LeashSystem::Update(float seconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto hand = HandPoint();
	for (const auto entity : Leashed(registry))
	{
		if (!std::as_const(registry).AllOf<Creature, Transform>(entity))
		{
			continue;
		}
		auto& leashes = registry.Get<CreatureLeash>(entity);
		if (!leashes.worn.has_value())
		{
			continue;
		}
		auto& worn = *leashes.worn;
		std::optional<glm::vec3> start;
		if (worn.tiedTo.has_value() && registry.Valid(*worn.tiedTo) &&
		    registry.TryGet<const Transform>(*worn.tiedTo) != nullptr)
		{
			start = TiedPoint(registry, *worn.tiedTo);
		}
		else
		{
			start = hand;
		}
		if (!start.has_value())
		{
			// The hand is off the land: the rope keeps its last place
			continue;
		}
		const auto end = CollarPoint(registry, entity);
		// Held in the hand, its length follows the creature's size; tied, it keeps the lengths it was tied with
		if (!worn.tiedTo.has_value())
		{
			const auto lengths = LengthsOf(registry, entity, worn);
			worn.rope.slackLength = lengths.slack;
			worn.rope.maxLength = lengths.max;
		}
		if (!worn.ropeStarted)
		{
			worn.rope = leash_rope::Create(*start, end, worn.rope.slackLength, worn.rope.maxLength, leash::LookFor(worn.type),
			                               GroundAt);
			worn.ropeStarted = true;
			continue;
		}
		worn.rope.look = leash::LookFor(worn.type);
		leash_rope::Step(worn.rope, *start, end, seconds, GroundAt);
	}
}
