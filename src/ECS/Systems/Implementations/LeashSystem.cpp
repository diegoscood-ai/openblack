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
#include <tuple>
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
#include "Common/GameDistance.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureBodyActivity.h"
#include "Creature/CreatureHelp.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreaturePhysiology.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSpellMind.h"
#include "Creature/CreatureTownCompassion.h"
#include "Creature/CreatureTurnPose.h"
#include "Creature/LeashKeys.h"
#include "Creature/LeashOwnership.h"
#include "Creature/LeashPull.h"
#include "Creature/LeashRules.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/MapScriptGlobals.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerLeash.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptHeld.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/CreatureHome.h"
#include "ECS/CreaturePose.h"
#include "ECS/MapCells.h"
#include "ECS/MobileDrawing.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/CreatureAudioSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MapCellsSystemInterface.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownBelief.h"
#include "ECS/Town/TownQueries.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Input/GamePackets.h"
#include "Input/LocalPlayer.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Worship/TempleLeash.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using openblack::ecs::Registry;
namespace leash = openblack::creature_leash;

namespace
{
/// The advisor may tell the player why the creature stopped what it was doing
void TellHelp(entt::entity creature, const creature_help::Message& message)
{
	if (Locator::creatureHelpSystem::has_value())
	{
		Locator::creatureHelpSystem::value().Raise(creature, message);
	}
}

/// The player has shown they know one of the creature's prompts, which is not given again
void PromptKnown(entt::entity creature, creature_help::Prompt prompt)
{
	if (Locator::creatureHelpSystem::has_value())
	{
		Locator::creatureHelpSystem::value().PromptKnown(creature, prompt);
	}
}

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

/// Where a leash held in the hand meets it this frame: the drawn hand's wrist, or the hand's point when the hand has no
/// posed bones
std::optional<glm::vec3> HandRopePoint(const std::optional<glm::vec3>& hand)
{
	if (!hand.has_value())
	{
		return std::nullopt;
	}
	const auto& hands = Locator::handSystem::value();
	// the root bone in the world as the hand finished it, or the model-space one under the hand's matrix before that
	const auto* world = hands.GetWorldBoneMatrices();
	if (const auto* bones = world != nullptr ? world : hands.GetBoneMatrices(); bones != nullptr)
	{
		if (const auto wrist = leash::WristAt(world != nullptr ? glm::mat4(1.0f) : hands.GetHandMatrix(), *bones))
		{
			return wrist;
		}
	}
	return hand;
}

float CreatureHeight(const Creature& creature)
{
	return creature_morph::k_HeightAtSizeOne * creature.size;
}

float GroundAt(glm::vec2 point)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
}

/// Where the leash meets the creature: its collar bone as the body is drawn this frame (creature_turn_pose::BonesDrawn:
/// between its last two turn poses, and smaller in its temple's pen) under its drawn matrix, or high on that body when
/// it has none. The rope is laid, pulled taut, drawn and picked from this one point
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
		bones = creature_turn_pose::BonesDrawn(*animation);
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

/// Where a pull's ropes meet what the leash is tied to: its place on the map, at the land's height there plus its
/// altitude over the land
glm::vec3 PullTiedPoint(const Registry& registry, entt::entity tied)
{
	const auto place = map_coords::FromWorld(registry.Get<const Transform>(tied).position);
	return creature_leash::TiedPointOf(place, GroundAt(map_coords::ToMetres(place)));
}

/// Where the leash meets the creature as the rope is laid: its collar bone in its latest turn pose
/// (creature_turn_pose::Latest, the bones posed onto the turn's body matrix, even while the hand keeps a copy for the
/// game), not the body drawn between turns. With no turn pose or no collar bone, the drawn collar (CollarPoint)
glm::vec3 LaidCollarPoint(const Registry& registry, entt::entity entity)
{
	const auto& creature = registry.Get<const Creature>(entity);
	const auto* pose = registry.TryGet<const CreatureTurnPose>(entity);
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto rigId = creature::GetRigId(creature.species);
	if (pose != nullptr && rigs.Contains(rigId))
	{
		if (const auto collar = leash::LaidCollarAt(creature_turn_pose::Latest(*pose), rigs.Handle(rigId)->leashBone))
		{
			return *collar;
		}
	}
	return CollarPoint(registry, entity);
}

/// Where the rope starts as it is laid: what the leash is tied to (a creature's collar in its latest turn pose), else
/// the hand's point. None with the hand off the land
std::optional<glm::vec3> LaidStartPoint(const Registry& registry, const CreatureLeash::Worn& worn)
{
	if (worn.tiedTo.has_value() && registry.Valid(*worn.tiedTo) && registry.TryGet<const Transform>(*worn.tiedTo) != nullptr)
	{
		return registry.TryGet<const Creature>(*worn.tiedTo) != nullptr ? LaidCollarPoint(registry, *worn.tiedTo)
		                                                                : TiedPoint(registry, *worn.tiedTo);
	}
	return HandPoint();
}

/// Lays the rope afresh as the leash is put on, tied or untied, from where it starts to the creature's collar in its
/// latest turn pose. When it can't be laid then (the hand off the land, no creature data to read), it is laid at the
/// next frame from the same points
void LayRope(const Registry& registry, entt::entity creature, CreatureLeash::Worn& worn)
{
	worn.ropeStarted = false;
	if (!Locator::resources::has_value() || !registry.AllOf<Creature, Transform>(creature))
	{
		return;
	}
	const auto start = LaidStartPoint(registry, worn);
	if (!start.has_value())
	{
		return;
	}
	worn.rope = leash_rope::Create(*start, LaidCollarPoint(registry, creature), worn.rope.slackLength, worn.rope.maxLength,
	                               leash::LookFor(worn.type), GroundAt);
	worn.ropeStarted = true;
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
	return leash::TiedToObject(game_math::GetDistanceInMetres(from, to));
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

/// The hand's gesture path is wiped when the player is the one at this machine
void ForgetLocalGesturePath(PlayerNames player)
{
	if (player == creature::LocalPlayer() && Locator::gestureSystem::has_value())
	{
		Locator::gestureSystem::value().ForgetPath();
	}
}

/// The player whose creature it is, if it is a creature
std::optional<PlayerNames> OwnerOfCreature(const Registry& registry, entt::entity creature)
{
	const auto* body = registry.TryGet<const Creature>(creature);
	return body != nullptr ? std::optional(body->owner) : std::nullopt;
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
	std::as_const(registry).Each<const Temple, const Transform>(
	    [&temple, owner](const Temple& building, const Transform& transform) {
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

/// The game's lists of actions for each of a town's desires; with no tables, every list is empty
const creature_town_compassion::DesireActions& TownActions()
{
	static const creature_town_compassion::DesireActions k_NoActions {};
	const auto* tables = Locator::creatureMindSystem::has_value() ? Locator::creatureMindSystem::value().GetTables() : nullptr;
	return tables != nullptr ? tables->townActions : k_NoActions;
}

/// Whether the creature's worn leash is tied to a town's desire flag
bool TiedToTownDesireFlag(const Registry& registry, entt::entity creature)
{
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	return leashes != nullptr && leashes->worn.has_value() && leashes->worn->tiedTo.has_value() &&
	       registry.Valid(*leashes->worn->tiedTo) && registry.AllOf<TownDesireFlag>(*leashes->worn->tiedTo);
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
	return creature_spell_mind::SpeciesDesireFloor(species, Locator::infoConstants::value().creature);
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
	return registry.AllOf<ScriptHeld, ScriptControlled>(entity);
}

/// A plan the leash makes the creature carry out: to obey the player, about itself, with an action of the game's table;
/// a forced one scores the most a score can be
creature_planner::Plan ObeyPlan(entt::entity creature, uint32_t action, bool forced)
{
	creature_planner::Plan plan {
	    .desire = creature_desires::Desire::ObeyPlayer, .action = action, .object = static_cast<uint32_t>(creature)};
	if (forced)
	{
		plan.goalUsefulness = creature_planner::k_ForcedScore;
		plan.actionPriority = creature_planner::k_ForcedScore;
		plan.priority = creature_planner::k_ForcedScore;
	}
	return plan;
}

/// The creature's mind drops the plan it carried out and stops what it was doing, for a plan the leash gives it
void ReplacePlan(CreatureMindState& mind, entt::entity creature, uint32_t action, bool forced)
{
	mind.planActive = false;
	mind.planner.current = ObeyPlan(creature, action, forced);
	creature_mind::Plan(mind.idle, creature_mind::Activity::None, {});
}

/// Sent walking to a point by the leash or its home: it remembers where to, and its mind gives up what it was doing to
/// walk there, obeying the player until the walk is over
void SendWalking(Registry& registry, entt::entity creature, CreatureLeash& leashes, const glm::vec3& point)
{
	leashes.returning = true;
	leashes.returningTo = point;
	// A new walk is all it has to do: a wait after an earlier one is given up, and a put-down after it
	leashes.waitAfterWalk.reset();
	leashes.putDownAfterWalk = CreatureLeash::PutDownAfterWalk::None;
	leashes.throwAt.reset();
	if (auto* mind = MindOf(registry, creature))
	{
		// The walk is forced on it, as pressing as a plan can be
		ReplacePlan(*mind, creature, leash::k_WalkToPointAction, true);
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
	const auto* mind = lookup.TryGet<const CreatureMindState>(entity);
	const leash::WalkBackCheck check {
	    .confined = true,
	    .clearingWay = mind != nullptr && mind->trapped,
	    .sentByLeash = leashes.control == CreatureLeash::Control::Sent,
	    .teleporting = mind != nullptr && mind->controlledByTeleport,
	    .controlledByScript = ControlledByScript(lookup, entity),
	    .pointOnMap = map_coords::InBounds(point),
	    .pointOnLand = LandAvoidAt(map_coords::CellOf(point)) == land_avoid::k_Land,
	    .distance = game_math::GetDistanceInMetres(lookup.Get<const Transform>(entity).position, point),
	    .radius = radius,
	    .walkingBack = leashes.returning,
	    .walkingToFromPoint = game_math::GetDistanceInMetres(leashes.returningTo, point),
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
	// The walk back ends its own side of a fight it is in, as any walk sent to it does; the opponent is left alone
	if (Locator::creatureFightSystem::has_value() && Locator::creatureFightSystem::value().IsFighting(entity))
	{
		Locator::creatureFightSystem::value().EndOwnFight(entity);
	}
	auto& locomotion = Locator::creatureLocomotionSystem::value();
	const auto hurry = leash::WalkBackHurry(check.distance, radius);
	const auto walk = locomotion.WalkBack(entity, glm::vec2(point.x, point.z), hurry, arrival);
	if (walk != CreatureLocomotionSystemInterface::MoveResult::Started)
	{
		// as the failed move's end of the action: a walk brakes to a stop
		locomotion.BrakeToStop(entity);
	}
	SendWalking(registry, entity, leashes, point);
	if (leashed)
	{
		leashes.control = CreatureLeash::Control::Led;
	}
}

/// Told to act on a thing, whatever came of it, it is doing what the leash sent it to do until its mind is done with the
/// thing: it is not walked back within the leash meanwhile
void SentToAct(Registry& registry, entt::entity creature)
{
	if (auto* leashes = registry.TryGet<CreatureLeash>(creature); leashes != nullptr)
	{
		leashes->control = CreatureLeash::Control::Sent;
		leashes->sentToThing = true;
	}
}

/// It acknowledges an order: its player hears the order taken when it is the local player's creature, and it makes its
/// acknowledging sound
void AcknowledgeOrder(const Registry& registry, entt::entity creature)
{
	if (const auto* body = registry.TryGet<const Creature>(creature); body != nullptr && body->owner == creature::LocalPlayer())
	{
		PlaySound(leash::k_OrderAcknowledgedSample);
	}
	if (Locator::creatureAudioSystem::has_value())
	{
		Locator::creatureAudioSystem::value().PlayNow(creature, audio::SoundAction::Acknowledge);
	}
	// and it nods, on top of whatever its body does, unless something already plays there
	if (Locator::creatureMindSystem::has_value())
	{
		std::ignore = Locator::creatureMindSystem::value().PlayGesture(creature, creature_layers::animations::k_Nod);
	}
}

/// It stops what it does, as an action finished unsuccessfully stops it: a fight it is in is called off, a walk brakes
/// along its route to a stop, a miracle it holds is let go, and its body's own action ends (a static action's loop or a
/// dance plays its end, a point held is let go). Whether the body still plays the end
bool StopWhatItDoes(Registry& registry, entt::entity creature)
{
	if (Locator::creatureFightSystem::has_value() && Locator::creatureFightSystem::value().IsFighting(creature))
	{
		Locator::creatureFightSystem::value().CallOff(creature);
	}
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().BrakeToStop(creature);
	}
	if (Locator::magicSystem::has_value())
	{
		Locator::magicSystem::value().ReleaseCreatureCast(creature);
	}
	bool bodyEnding = false;
	if (auto* animation = Find<CreatureAnimation>(registry, creature))
	{
		animation->body = creature_layers::EndDance(creature_layers::EndLoop(animation->body));
		bodyEnding = creature_layers::IsPlaying(animation->body);
	}
	if (Locator::creatureObjectActionSystem::has_value())
	{
		Locator::creatureObjectActionSystem::value().StopPointing(creature);
	}
	return bodyEnding;
}

/// Whether the creature carries out its player's order and walks where it was sent now: on the walk the leash sent it
/// on, or on a walk to a point of a plan its player ordered
bool WalksAsOrdered(const Registry& registry, entt::entity creature, const CreatureLeash& leashes)
{
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	if (mind == nullptr)
	{
		return false;
	}
	// The walk the leash sent it on is what it does until the walk is over: while it moves, and once it stops until its
	// turn sees it over, unless it then waits there or puts down what it carried
	const bool moving =
	    Locator::creatureLocomotionSystem::has_value() && Locator::creatureLocomotionSystem::value().IsMoving(creature);
	if (mind->leash.obeying && leashes.returning &&
	    (moving || (!leashes.waitAfterWalk.has_value() && leashes.putDownAfterWalk == CreatureLeash::PutDownAfterWalk::None)))
	{
		return true;
	}
	if (!mind->commanded || !mind->planActive || mind->idle.step >= mind->idle.agenda.size())
	{
		return false;
	}
	const auto& step = mind->idle.agenda[mind->idle.step];
	return step.kind == creature_mind::Step::Kind::Move && (step.movement.kind == creature_mind::Movement::Kind::ToPoint ||
	                                                        step.movement.kind == creature_mind::Movement::Kind::Nearby);
}

/// The first step of a plan goes to the point, and looks there on its way, in place of the place the plan found
void SendFirstStepTo(std::vector<creature_mind::Step>& agenda, glm::vec2 point)
{
	if (agenda.empty())
	{
		return;
	}
	auto& first = agenda.front();
	first.movement.point = point;
	if (first.gaze.has_value() && first.gaze->point.has_value())
	{
		first.gaze->point = glm::vec3(point.x, first.gaze->point->y, point.y);
	}
}

/// Sent onto the sea, the creature is made to drink from it, thirsty and wanting water, or else to look at its
/// reflection in it, its plan's first walk going to the point tapped instead of the place the plan found; sent near its
/// home while tired, it is made to sleep at home. It carries the plan out as its player's order whether or not the
/// plan could be made, and sent onto the sea with a plan made it tells the player what it wants now
void OrderPlanAtPoint(Registry& registry, entt::entity creature, CreatureLeash& leashes, const leash::PointOrderPlan& plan,
                      bool sea, const glm::vec3& point)
{
	if (MindOf(registry, creature) == nullptr || !Locator::creatureMindSystem::has_value())
	{
		return;
	}
	// Sent to act, it has something to do with its player again: its turns since are counted from none
	MindOf(registry, creature)->turnsSinceMadeToAct = 0;
	// Already walking where its player sent it, carrying out its player's order, it goes on walking and doing what its
	// body does as the new plan takes over
	const bool walking = WalksAsOrdered(registry, creature, leashes);
	const bool planned = Locator::creatureMindSystem::value().ForceActivity(
	    creature, {.desire = plan.desire, .action = plan.action, .actionObject = creature, .endsWhatItDoes = !walking});
	auto* mind = MindOf(registry, creature);
	if (mind == nullptr)
	{
		return;
	}
	if (sea && planned)
	{
		SendFirstStepTo(mind->idle.agenda, {point.x, point.z});
	}
	mind->commanded = true;
	if (sea)
	{
		if (const auto told = creature_help::LeashOrderMessage(planned, plan.desire, creature, leashes.lastOrderPoint))
		{
			TellHelp(creature, *told);
		}
	}
	// It counts as walking back where the leash sent it until it stops what it does
	leashes.returning = true;
}

/// The mark of where the leash sent it fades, if it is still shown, and it has no target any more (the point stays)
void CloseTargetMarker(CreatureLeash& leashes)
{
	if (leashes.targetMarker != entt::null && Locator::particleSystem::has_value())
	{
		Locator::particleSystem::value().CloseDownSpotVisual(leashes.targetMarker);
	}
	leashes.targetMarker = entt::null;
	leashes.targetThing = entt::null;
	leashes.hasTarget = false;
}

/// The mark of where the leash sent it, on the ground at the point, in place of the one before; the point above it
/// is its target
void ShowTargetMarker(entt::entity creature, CreatureLeash& leashes, glm::vec2 point)
{
	CloseTargetMarker(leashes);
	const glm::vec3 place(point.x, GroundAt(point), point.y);
	if (Locator::particleSystem::has_value())
	{
		leashes.targetMarker = Locator::particleSystem::value().StartSpotVisual(
		    SpotVisualType::CreatureTarget, place, leash::k_TargetMarkerTurns, creature, leash::k_TargetMarkerMagnitude);
	}
	leashes.targetPoint = leash::TargetAbove(place, 0.0f);
	leashes.hasTarget = true;
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {}'s leash target: the point ({:.1f}, {:.1f}, {:.1f})",
	                   entt::to_integral(creature), leashes.targetPoint.x, leashes.targetPoint.y, leashes.targetPoint.z);
}

/// Each turn: a mark that has gone is forgotten, and a target thing that has gone is let go of, with the target
void ForgetGoneTarget(const Registry& registry, CreatureLeash& leashes)
{
	if (leashes.targetMarker != entt::null &&
	    (!registry.Valid(leashes.targetMarker) || !ecs::IsAvailable(leashes.targetMarker)))
	{
		leashes.targetMarker = entt::null;
	}
	if (leashes.targetThing != entt::null && (!registry.Valid(leashes.targetThing) || !ecs::IsAvailable(leashes.targetThing)))
	{
		leashes.targetThing = entt::null;
		leashes.hasTarget = false;
	}
}

/// Whether a creature can stand at a point of the land, as a walk asks
bool CanStandAt(glm::vec2 point)
{
	return land_avoid::IsPosValid(glm::vec3(point.x, GroundAt(point), point.y), land_avoid::k_CreatureRadius);
}

/// Sent by the leash to a point of the land: what it does there, the place it walks to, and the field it acts on
struct PointOrderTaken
{
	leash::PointOrder order {leash::PointOrder::Refused};
	glm::vec2 walkTo {0.0f};
	entt::entity field {entt::null};
};
PointOrderTaken TakePointOrder(Registry& registry, entt::entity creature, const CreatureLeash& leashes, const glm::vec3& point)
{
	const auto& lookup = std::as_const(registry);
	const auto& body = lookup.Get<const Creature>(creature);
	const auto* needs = lookup.TryGet<const CreatureNeeds>(creature);
	const auto* mind = lookup.TryGet<const CreatureMindState>(creature);
	const glm::vec2 onMap {point.x, point.z};
	PointOrderTaken taken {.walkTo = onMap};

	leash::PointOrderCheck check {
	    .controlledByScript = ControlledByScript(lookup, creature),
	    .leashWorks = !leashes.worn.has_value() || leashes.worn->works,
	    .lockedSelect = Locator::handSystem::has_value() && Locator::handSystem::value().IsInLockedSelect(creature),
	    .holding = lookup.AllOf<CreatureHeldObject>(creature),
	    .evilLeash = leashes.worn.has_value() && leashes.worn->type == LeashType::Evil,
	    .pointValid = CanStandAt(onMap),
	    .fighting = Locator::creatureFightSystem::has_value() && Locator::creatureFightSystem::value().IsFighting(creature),
	    .busy = mind != nullptr && mind->fightStarted,
	};
	if (needs != nullptr)
	{
		check.life = needs->needs.life;
		check.exhaustion = needs->needs.exhaustion;
		check.thirst = needs->needs.dehydration;
	}
	// Where it can't stand, the nearest place round the point where it can is looked for
	if (!check.pointValid && !check.holding)
	{
		const auto found = land_avoid::NearestValid(onMap, land_avoid::k_CreatureRadius, leash::k_PointOrderSearchReach);
		check.foundValid = found.has_value() && CanStandAt(*found);
		if (found.has_value())
		{
			taken.walkTo = *found;
		}
	}
	// What lies at the point is asked of the point tapped, not of the place found round it
	check.water = ecs::sea_cells::IsWater(point);
	if (Locator::mapCellsSystem::has_value())
	{
		taken.field = ecs::map_cells::FindType(map_coords::CellOf(point), ObjectType::Field);
	}
	check.field = taken.field != entt::null;
	if (mind != nullptr && mind->desires.has_value())
	{
		check.wantsWater = mind->desires->desires.at(static_cast<size_t>(creature_desires::Desire::Water)).activated;
	}
	check.hasTemple = TempleOf(registry, body.owner).has_value();
	if (const auto home = ecs::creature_home::HomeOf(registry, creature))
	{
		check.homeToPoint = game_math::GetDistanceInMetres(*home, point);
	}
	check.planIsSleep = mind != nullptr && mind->planner.current.has_value() &&
	                    mind->planner.current->action == creature_physiology::actions::k_SleepAtHome;
	taken.order = leash::DecidePointOrder(check);
	SPDLOG_LOGGER_INFO(
	    spdlog::get("game"),
	    "Creature {}'s leash order at ({:.1f}, {:.1f}): order {}, holding {}, can stand {}, water {}, walk to ({:.1f}, {:.1f})",
	    entt::to_integral(creature), onMap.x, onMap.y, static_cast<int>(taken.order), check.holding, check.pointValid,
	    check.water, taken.walkTo.x, taken.walkTo.y);
	return taken;
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
	if (const auto* totem = registry.TryGet<const VillageTotem>(object))
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

/// The player service keeps each player's creature: the one creature the leash lets them lead is written there as it
/// changes
void KeepAsPlayersCreature(PlayerNames player, std::optional<entt::entity> creature)
{
	if (Locator::playerSystem::has_value())
	{
		Locator::playerSystem::value().SetPrimaryCreature(player, creature);
	}
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
			KeepAsPlayersCreature(owner, std::nullopt);
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
	DisplaceOthers(creature, owner);
	if (!registry.Get<Creature>(creature).leashable)
	{
		registry.Get<Creature>(creature).leashable = true;
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} is now the one player {} leads", entt::to_integral(creature),
		                   PlayerNumber(owner));
	}
	KeepAsPlayersCreature(owner, creature);
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
	const auto previous = changed.owner;
	changed.owner = owner;
	if (!changed.leashable)
	{
		return;
	}
	// Its last owner is left without it. It stays the one its new owner leads only if they have no other
	KeepAsPlayersCreature(previous, std::nullopt);
	if (!leash::CanLead(owner) || !leash::Displaced(Claims(), entt::to_integral(creature), owner).empty())
	{
		registry.Get<Creature>(creature).leashable = false;
		return;
	}
	KeepAsPlayersCreature(owner, creature);
}

void LeashSystem::DisplaceOthers(entt::entity creature, PlayerNames owner)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto id : leash::Displaced(Claims(), entt::to_integral(creature), owner))
	{
		const auto other = static_cast<entt::entity>(id);
		TakeOff(other);
		registry.Get<Creature>(other).leashable = false;
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} is no longer the one player {} leads", id, PlayerNumber(owner));
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
	// A new creature is its owner's creature, whichever they had, so it is the one a player who leads creatures leads:
	// the newest made wins, and the one before loses its leash
	const auto owner = body->owner;
	if (!leash::CanLead(owner))
	{
		return;
	}
	DisplaceOthers(creature, owner);
	registry.Get<Creature>(creature).leashable = true;
	KeepAsPlayersCreature(owner, creature);
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
		leashes->worn->works = leashes->leashWorks;
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
	// The rope is laid afresh from the hand to the collar as it goes on
	LayRope(registry, creature, *leashes->worn);
	TakeMoodOf(registry, creature, type);
	// Going on, it becomes the leash picked at the temple of the creature's owner, and it wipes the gesture path of the
	// owner's hand when they play at this machine
	if (const auto owner = OwnerOfCreature(registry, creature); owner.has_value())
	{
		worship::temple_leash::TakeLeashType(*owner, type);
	}
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
	const auto holder = leashes->worn->holder;
	leashes->worn.reset();
	leashes->control = CreatureLeash::Control::Idle;
	leashes->confinementRadius = 0.0f;
	if (auto* mind = MindOf(registry, creature))
	{
		// The actions it gave up for walking it out of the area it was kept within may be taken again
		mind->unavailableWhenConfined.clear();
		// A walk back it was sent on goes on to its end
		mind->leash.obeying = mind->leash.obeying && leashes->returning;
		mind->leash.learningInHand = false;
		mind->leash.miracleSightingWeight = leash::MiracleSightingWeight(false);
	}
	// Coming off, it wipes the gesture path of the holder's hand when they play at this machine
	ForgetLocalGesturePath(holder);
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
	{
		// Only this untie, the leash key's, frees the creature from the desire of a town's desire flag it was tied to
		auto& registry = Locator::entitiesRegistry::value();
		if (auto* mind = MindOf(registry, creature); mind != nullptr && TiedToTownDesireFlag(registry, creature))
		{
			creature_town_compassion::UntiedFromFlag(mind->townCompassion);
		}
		UntieToHand(creature);
		return true;
	}
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
	// the leash key, and the script's toggle, which is the same step, do nothing for a creature no longer available
	// (one marked to go, as the old body of a swap), as the game's own key does
	if (key == leash::LeashKey::Leash && !ecs::IsAvailable(*creature))
	{
		return false;
	}
	const auto command = leash::CommandFor(key, KeyStateOf(registry, *creature));
	if (key != leash::LeashKey::Leash)
	{
		// The keys that step the leash only send the leash one step on, when the creature knows it, as the temple's pick
		// does: the leash changes at the next turn, through the leash type's packet, and nothing else is asked
		if (command.kind != leash::KeyCommand::Kind::ChangeType)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} knows no other leash to pick", entt::to_integral(*creature));
			return false;
		}
		game_packets::Push(
		    {.type = game_packets::Type::LeashType, .value = static_cast<int32_t>(command.type), .player = player});
		return true;
	}
	return Carry(player, *creature, command);
}

bool LeashSystem::Shake(PlayerNames player)
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
	if (Find<CreatureLeash>(registry, creature) == nullptr || !Knows(creature, type))
	{
		return false;
	}
	SetType(creature, type);
	return true;
}

void LeashSystem::SetType(entt::entity creature, LeashType type)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* leashes = Find<CreatureLeash>(registry, creature);
	if (leashes == nullptr)
	{
		return;
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
}

bool LeashSystem::Switch(entt::entity creature, bool on)
{
	auto& registry = Locator::entitiesRegistry::value();
	// On goes on only for a creature that knows the learning leash; off, or for any other, the leash comes off
	if (!on || !Knows(creature, LeashType::Rope))
	{
		if (IsLeashed(creature))
		{
			TakeOff(creature);
		}
		else
		{
			ReleaseUnleashed(creature);
		}
		return false;
	}
	auto& kept = registry.Get<CreatureLeash>(creature);
	if (!IsLeashed(creature))
	{
		// On with the leash its owner picked, held by its owner, with none of the rules a leash put on by hand asks
		PutOnUnchecked(std::as_const(registry).Get<const Creature>(creature).owner, creature, kept.selected);
	}
	else
	{
		// A worn leash is put on again, with its mood, picked again at the owner's temple and the owner's gesture path
		// wiped
		const auto wornType = kept.worn->type;
		TakeMoodOf(registry, creature, wornType);
		if (const auto owner = OwnerOfCreature(registry, creature); owner.has_value())
		{
			worship::temple_leash::TakeLeashType(*owner, wornType);
		}
		// Then its lengths are taken again, tied or not, over the ones it had: by the creature's size in the hand, by
		// what it is tied to otherwise. A tie whose thing is gone keeps them, as the frame's rope does
		auto& worn = *registry.Get<CreatureLeash>(creature).worn;
		if (!worn.tiedTo.has_value() ||
		    (registry.Valid(*worn.tiedTo) && std::as_const(registry).TryGet<const Transform>(*worn.tiedTo) != nullptr))
		{
			const auto lengths = LengthsOf(registry, creature, worn);
			worn.rope.slackLength = lengths.slack;
			worn.rope.maxLength = lengths.max;
		}
	}
	return true;
}

void LeashSystem::SwitchAtLoad(entt::entity creature, bool on)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || registry.TryGet<CreatureLeash>(creature) == nullptr ||
	    std::as_const(registry).TryGet<const Creature>(creature) == nullptr)
	{
		return;
	}
	static_cast<void>(Switch(creature, on));
}

bool LeashSystem::TieTo(entt::entity creature, entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* kept = registry.TryGet<CreatureLeash>(creature);
	if (object == creature || !registry.Valid(object) || registry.TryGet<const Transform>(object) == nullptr ||
	    kept == nullptr || std::as_const(registry).TryGet<const Creature>(creature) == nullptr)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {}'s leash can't be tied to that", entt::to_integral(creature));
		return false;
	}
	// The turn of the tie is kept by the player's leash before the leash is put on, whether that is done or not
	kept->lastTiedTurn = game_clock::Turn();

	// The two tying sounds in turn, whether or not the leash goes on: with no place for the local player's own
	// creature, at the thing for any other player's, that player's sound
	const int tieSound = _secondAttachSound ? k_SecondAttachSound : k_AttachSound;
	_secondAttachSound = !_secondAttachSound;
	const auto owner = std::as_const(registry).Get<const Creature>(creature).owner;
	if (!leash::TieSoundIsPlaced(owner, creature::LocalPlayer()))
	{
		PlaySound(tieSound);
	}
	else
	{
		audio::PlayOptions options;
		options.sample = {audio::Bank(audio::SfxBank::InGame), tieSound};
		options.owner = audio::Owner::Key(static_cast<uint32_t>(owner));
		options.is3D = true;
		options.track = false;
		options.position = std::as_const(registry).Get<const Transform>(object).position;
		audio::PlaySoundEffect(options);
	}

	// The tie puts the leash on, and the only creature it refuses is one that does not know the learning leash: for
	// that one the leash comes off instead
	const bool on = Switch(creature, true);
	if (!on)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} doesn't know the learning leash, so the tie takes it off",
		                   entt::to_integral(creature));
	}
	auto* leashes = &registry.Get<CreatureLeash>(creature);
	if (on)
	{
		auto& worn = *leashes->worn;
		worn.tiedTo = object;
		worn.tiedTurn = game_clock::Turn();
		leashes->lastTiedTurn = worn.tiedTurn;
		const auto lengths = LengthsOf(registry, creature, worn);
		worn.rope.slackLength = lengths.slack;
		worn.rope.maxLength = lengths.max;
		// laid afresh from what it is tied to as it is tied
		LayRope(registry, creature, worn);
		leashes->control = CreatureLeash::Control::Idle;
	}
	const auto type = on ? leashes->worn->type : leashes->selected;

	// The creature learns that the player wants something done with what it is tied to, only with the leash on. The tie
	// itself does not send it to act on the thing: the hand's tie does that after it (ActOn), a script's tie never does
	if (auto* mind = MindOf(registry, creature))
	{
		if (on)
		{
			mind->leash.obeying = false;
			const bool isCreature = registry.TryGet<const Creature>(object) != nullptr;
			mind->leash.shown.push_back({
			    .object = static_cast<uint32_t>(object),
			    .type = type,
			    .lessons = leash::LessonsFor(type, isCreature),
			});
		}
		// Tied to a town's desire flag, it helps the town with that desire and no other until the leash key unties it,
		// even when the tie took the leash off
		if (const auto* flag = std::as_const(registry).TryGet<const TownDesireFlag>(object); flag != nullptr)
		{
			creature_town_compassion::TiedToFlag(mind->townCompassion, static_cast<uint32_t>(flag->desire), TownActions());
		}
	}
	// Tied to a village's centre on any leash but aggression, it wants to impress the village for a while, even when
	// the tie took the leash off (the leash picked is the one that counts then)
	if (type != LeashType::Evil && IsTownCentre(std::as_const(registry), object))
	{
		MakeDominant(registry, creature, creature_desires::Desire::Impress, leash::k_ImpressTownSeconds);
	}
	return on;
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
	// laid afresh from the hand as it is untied
	LayRope(registry, creature, *leashes->worn);
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
	// The two sounds in turn, whether or not the leash went back on
	PlayUntieSound(holder);
}

void LeashSystem::PutInHand(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* body = std::as_const(registry).TryGet<const Creature>(creature);
	auto* leashes = Find<CreatureLeash>(registry, creature);
	if (body == nullptr || leashes == nullptr)
	{
		return;
	}
	const auto owner = body->owner;
	// A player has one leash, which is the creature they lead's: another creature of theirs, or one they don't lead, is
	// refused. Whether it knows the learning leash is not a refusal: without it, the leash comes off
	switch (const auto why = WhyNot(owner, creature, LeashType::Rope))
	{
	case leash::Refusal::None:
	case leash::Refusal::DoesNotKnowLearningLeash:
	case leash::Refusal::DoesNotKnowThatLeash:
		break;
	default:
		Refuse(owner, creature, why);
		return;
	}
	const bool held = leashes->worn.has_value();
	const auto control = leashes->control;
	// Untied first, so that the lengths are taken in the hand
	if (held)
	{
		leashes->worn->tiedTo.reset();
	}
	if (Knows(creature, LeashType::Rope))
	{
		// On again, with the leash its owner picked, which is the one worn when one is
		PutOnUnchecked(owner, creature, leashes->selected);
		if (held)
		{
			// Put on again, a held leash goes on leading the creature as it was
			registry.Get<CreatureLeash>(creature).control = control;
		}
	}
	else if (held)
	{
		TakeOff(creature);
	}
	else
	{
		ReleaseUnleashed(creature);
	}
	PlayUntieSound(owner);
}

void LeashSystem::PlayUntieSound(PlayerNames holder)
{
	if (holder == creature::LocalPlayer())
	{
		PlaySound(_secondUntieSound ? k_SecondAttachSound : k_AttachSound);
		_secondUntieSound = !_secondUntieSound;
	}
}

void LeashSystem::SetWorks(entt::entity creature, bool works)
{
	// The switch is the player's leash's, so it stays while no leash is on, for the next leash put on
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !std::as_const(registry).AllOf<Creature>(creature))
	{
		return;
	}
	auto& leashes = LeashOf(registry, creature);
	leashes.leashWorks = works;
	if (leashes.worn.has_value())
	{
		leashes.worn->works = works;
	}
}

bool LeashSystem::Works(entt::entity creature) const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* leashes = registry.Valid(creature) ? registry.TryGet<const CreatureLeash>(creature) : nullptr;
	return leashes != nullptr && leashes->worn.has_value() && leashes->worn->works;
}

void LeashSystem::OrderGivenUp(entt::entity creature)
{
	WalkFoundNoWay(creature);
	// What it did stops, as every forced plan stops it: a walk back into the area it is kept within is over
	if (auto* leashes = Find<CreatureLeash>(Locator::entitiesRegistry::value(), creature))
	{
		leashes->returning = false;
	}
}

void LeashSystem::WalkFoundNoWay(entt::entity creature)
{
	if (auto* leashes = Find<CreatureLeash>(Locator::entitiesRegistry::value(), creature))
	{
		leashes->waitAfterWalk.reset();
		leashes->putDownAfterWalk = CreatureLeash::PutDownAfterWalk::None;
		leashes->throwAt.reset();
	}
}

void LeashSystem::PullAwayFromAction(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (Find<Creature>(registry, creature) == nullptr)
	{
		return;
	}
	// As an action finished unsuccessfully: a fight is called off with no winner, a walk brakes along its route to a
	// stop, a miracle held is let go and the body's own action ends; a walk back is over, it is no longer led, and its
	// forced plan, which is what obeying stands for, is cleared
	const bool bodyEnding = StopWhatItDoes(registry, creature);
	if (auto* leashes = Find<CreatureLeash>(registry, creature))
	{
		leashes->returning = false;
		leashes->control = CreatureLeash::Control::Idle;
		leashes->sentToThing = false;
		leashes->waitAfterWalk.reset();
		leashes->putDownAfterWalk = CreatureLeash::PutDownAfterWalk::None;
		leashes->throwAt.reset();
		CloseTargetMarker(*leashes);
	}
	// Its mind no longer obeys nor carries out its player's order, and gives up its plan, which leaves it no desire, so it
	// makes a new one
	if (auto* mind = MindOf(registry, creature))
	{
		mind->leash.obeying = false;
		mind->commanded = false;
		mind->leash.bodyEndPlaysOut = bodyEnding;
		mind->planActive = false;
		creature_planner::StopPlan(mind->planner);
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
	// Told to act on a thing, it stops copying its player first, whether or not it takes the order
	if (mind->learnt.has_value())
	{
		mind->learnt->mimicry.reset();
	}
	// A creature that can't take orders refuses this one before anything: not even the acknowledgement
	{
		const auto& lookup = std::as_const(registry);
		const auto* leashes = lookup.TryGet<const CreatureLeash>(creature);
		const auto* needs = lookup.TryGet<const CreatureNeeds>(creature);
		const leash::ActOnCheck check {
		    .leashWorks = leashes == nullptr || !leashes->worn.has_value() || leashes->worn->works,
		    .lockedSelect = Locator::handSystem::has_value() && Locator::handSystem::value().IsInLockedSelect(creature),
		    .life = needs != nullptr ? needs->needs.life : 1.0f,
		    .exhaustion = needs != nullptr ? needs->needs.exhaustion : 0.0f,
		    .fightStarted = mind->fightStarted,
		    .fighting = Locator::creatureFightSystem::has_value() && Locator::creatureFightSystem::value().IsFighting(creature),
		};
		if (leash::RefusesActOn(check))
		{
			return;
		}
	}
	// A big forest is acted on where it stands, as the leash's order on the land there
	if (std::as_const(registry).AllOf<BigForest>(object))
	{
		if (const auto* at = std::as_const(registry).TryGet<const Transform>(object))
		{
			ActOnPoint(creature, at->position);
		}
		return;
	}
	// Sent to a fish farm while hunger is one of its desires, knowing the action the game asks and short of full energy,
	// it fishes there and eats the catch, as its player's order, before any acknowledgement: the player is taken to
	// know how to make it drink, the farm is marked as its target, and it counts as walking back where the leash sent it
	if (std::as_const(registry).AllOf<FishFarm>(object) && Locator::creatureMindSystem::has_value())
	{
		const auto* needs = std::as_const(registry).TryGet<const CreatureNeeds>(creature);
		const bool hungry = mind->desires.has_value() &&
		                    mind->desires->desires.at(static_cast<size_t>(creature_desires::Desire::Hunger)).activated;
		const auto& skills = mind->learnt.has_value() ? mind->learnt->knowledge.skillsKnown : std::vector<bool> {};
		const bool knows = leash::k_FishFarmKnownAction < skills.size() && skills.at(leash::k_FishFarmKnownAction);
		if (leash::FishesAtFarm(hungry, knows, needs != nullptr ? needs->needs.energy : 1.0f))
		{
			PromptKnown(creature, creature_help::Prompt::Drink);
			const bool planned = Locator::creatureMindSystem::value().ForceActivity(
			    creature, {.desire = creature_desires::Desire::Hunger, .action = "FishAndEat", .actionObject = object});
			if (auto* ordered = MindOf(registry, creature))
			{
				ordered->commanded = true;
			}
			auto* leashes = Find<CreatureLeash>(registry, creature);
			if (planned && leashes != nullptr)
			{
				if (const auto told = creature_help::LeashOrderMessage(true, creature_desires::Desire::Hunger, object,
				                                                       leashes->lastOrderPoint))
				{
					TellHelp(creature, *told);
				}
			}
			MarkTargetAt(creature, object);
			if (leashes != nullptr)
			{
				leashes->returning = true;
			}
			return;
		}
	}
	// Told to act on something, it acknowledges the order, every time it is told
	AcknowledgeOrder(registry, creature);
	// Sent to act, it has something to do with its player again: its turns since are counted from none
	mind->turnsSinceMadeToAct = 0;
	// Whatever it is told to do with the thing, what it was doing stops as a failure first, its plan given up
	PullAwayFromAction(creature);
	mind = MindOf(registry, creature);
	if (mind == nullptr)
	{
		return;
	}
	// Empty-handed, a thing it can pick up is not weighed by its desires: it is made curious about the thing and holds it
	// up to its player, the thing marked as its target
	const bool emptyHanded = !std::as_const(registry).AllOf<CreatureHeldObject>(creature);
	const bool canPickUp = Locator::creatureObjectActionSystem::has_value() &&
	                       Locator::creatureObjectActionSystem::value().CanPickUp(creature, object);
	if (emptyHanded && canPickUp && Locator::creatureMindSystem::has_value())
	{
		// the last order is the one it carries out: an earlier one its mind has not taken up yet is no longer its order
		mind->leash.ordered.reset();
		const CreatureMindSystemInterface::ForcedActivity hold {
		    .desire = creature_desires::Desire::Curiosity,
		    .action = leash::OrderedThingAction(true),
		    .actionObject = object,
		};
		const bool held = Locator::creatureMindSystem::value().ForceActivity(creature, hold);
		// it carries the hold out as its player's order, whether or not the plan could be made
		if (auto* ordered = MindOf(registry, creature))
		{
			ordered->commanded = true;
		}
		if (held)
		{
			MarkTargetAt(creature, object);
		}
		// the order taken with a plan, it tells what it wants now, about the thing
		if (const auto* leashes = Find<CreatureLeash>(registry, creature); leashes != nullptr)
		{
			if (const auto told = creature_help::LeashOrderMessage(held, creature_desires::Desire::Curiosity, object,
			                                                       leashes->lastOrderPoint))
			{
				TellHelp(creature, *told);
			}
		}
		SentToAct(registry, creature);
		return;
	}
	// Every call adds it, even twice in a row, as every act is a new one. The repeat is harmless: the mind takes only
	// the last thing and clears the rest, and a creature already fighting the first time is busy the second. The mind
	// knows it as its player's order, which it is made curious about when its desires find nothing to do with it
	mind->leash.actOn.push_back(static_cast<uint32_t>(object));
	mind->leash.ordered = static_cast<uint32_t>(object);
	SentToAct(registry, creature);
}

void LeashSystem::SetPulledLength(entt::entity creature, float full)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* leashes = Find<CreatureLeash>(registry, creature);
	if (leashes == nullptr || !leashes->worn.has_value())
	{
		return;
	}
	leashes->worn->rope.maxLength = full;
	leashes->worn->rope.slackLength = creature_leash::PullSlackLength(full);
}

void LeashSystem::StartPull(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* leashes = Find<CreatureLeash>(registry, creature);
	const auto hand = HandPoint();
	if (leashes == nullptr || !leashes->worn.has_value() || !leashes->worn->tiedTo.has_value() || !hand.has_value() ||
	    !registry.Valid(*leashes->worn->tiedTo) || !std::as_const(registry).AllOf<Transform>(*leashes->worn->tiedTo) ||
	    !std::as_const(registry).AllOf<Creature, Transform>(creature))
	{
		return;
	}
	auto& worn = *leashes->worn;
	// laid from the hand to the collar and to where the tied thing stands, as any rope is laid, then given the tied
	// rope's look: the one to the collar half as long again as the tied rope, the one to the tied thing short
	auto toCollar = leash_rope::Create(*hand, LaidCollarPoint(registry, creature),
	                                   worn.rope.slackLength * creature_leash::k_PullCollarRopeScale,
	                                   worn.rope.maxLength * creature_leash::k_PullCollarRopeScale, {}, GroundAt);
	toCollar.look = worn.rope.look;
	auto toTied = leash_rope::Create(*hand, PullTiedPoint(registry, *worn.tiedTo), creature_leash::k_PullTiedRopeSlack,
	                                 creature_leash::k_PullTiedRopeFull, {}, GroundAt);
	toTied.look = worn.rope.look;
	worn.pull = CreatureLeash::Worn::PullRopes {.toCollar = toCollar, .toTied = toTied};
}

void LeashSystem::EndPull(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* leashes = Find<CreatureLeash>(registry, creature); leashes != nullptr && leashes->worn.has_value())
	{
		leashes->worn->pull.reset();
	}
}

void LeashSystem::ActOnPoint(entt::entity creature, const glm::vec3& point)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* leashes = Find<CreatureLeash>(registry, creature);
	if (leashes == nullptr || !std::as_const(registry).AllOf<Creature, Transform>(creature))
	{
		return;
	}
	const auto taken = TakePointOrder(registry, creature, *leashes, point);
	// Taking the order, it acknowledges it. Sent onto a field it acknowledges it once more, as it is told to act on it
	if (leash::AcknowledgesOrder(taken.order))
	{
		AcknowledgeOrder(registry, creature);
	}
	switch (taken.order)
	{
	case leash::PointOrder::Refused:
		break;
	case leash::PointOrder::CantGetThere:
		TellHelp(creature, {.kind = creature_help::Kind::FailureExplanation, .parameter = creature_help::k_Inaccessible});
		break;
	case leash::PointOrder::ActOnField:
		ActOn(creature, taken.field);
		break;
	case leash::PointOrder::WalkThere:
		leashes->lastOrderPoint = point;
		if (auto* mind = MindOf(registry, creature); mind != nullptr && Locator::creatureLocomotionSystem::has_value())
		{
			// Unless it already walks where its player sent it, it first stops what it does
			if (!WalksAsOrdered(registry, creature, *leashes))
			{
				std::ignore = StopWhatItDoes(registry, creature);
			}
			// Sent to act, it has something to do with its player again: its turns since are counted from none
			mind->turnsSinceMadeToAct = 0;
			// Sent again while it obeys, on a walk it was sent on or carrying out another order of its player's, it runs,
			// and waits there a moment once it arrives
			const bool again = mind->leash.obeying || mind->commanded;
			// sent on again so that it runs, the player has shown they know how to make it run
			if (again)
			{
				PromptKnown(creature, creature_help::Prompt::Run);
			}
			const auto pace =
			    again ? CreatureLocomotionSystemInterface::Pace::Run : CreatureLocomotionSystemInterface::Pace::Walk;
			const auto arrival =
			    leash::PointOrderArrival(CreatureHeight(std::as_const(registry).Get<const Creature>(creature)));
			auto& locomotion = Locator::creatureLocomotionSystem::value();
			if (locomotion.MoveTo(creature, taken.walkTo, pace, 0.0f, arrival) !=
			    CreatureLocomotionSystemInterface::MoveResult::Started)
			{
				// as the failed move's end of the action: a walk brakes to a stop
				locomotion.BrakeToStop(creature);
			}
			// It walks to the place found, and remembers the point tapped as where it walks to
			SendWalking(registry, creature, *leashes, point);
			if (again)
			{
				leashes->waitAfterWalk = leash::SecondOrderWaitTurns(game_random::GameFloatRand(leash::k_SecondOrderWaitDraw),
				                                                     game_clock::MsPerTurn());
			}
			const auto& position = std::as_const(registry).Get<const Transform>(creature).position;
			if (game_math::GetDistanceInMetres(point, position) > leash::k_PointOrderTellBeyond)
			{
				TellHelp(creature, {.kind = creature_help::Kind::CurrentDesire,
				                    .parameter = static_cast<uint32_t>(creature_desires::Desire::ObeyPlayer),
				                    .thing = creature,
				                    .position = point});
			}
		}
		break;
	case leash::PointOrder::CarryAndPutDown:
		leashes->lastOrderPoint = point;
		if (Locator::creatureLocomotionSystem::has_value())
		{
			// Holding something, it stops what it does, walks to within its height of the point, never running and with
			// no wait after, and puts the thing down there; its turns since it was made to act go on, and it tells the
			// player nothing
			std::ignore = StopWhatItDoes(registry, creature);
			auto& locomotion = Locator::creatureLocomotionSystem::value();
			const auto arrival = CreatureHeight(std::as_const(registry).Get<const Creature>(creature));
			const bool started = locomotion.MoveTo(creature, taken.walkTo, CreatureLocomotionSystemInterface::Pace::Walk, 0.0f,
			                                       arrival) == CreatureLocomotionSystemInterface::MoveResult::Started;
			if (!started)
			{
				// as the failed move's end of the action: a walk brakes to a stop
				locomotion.BrakeToStop(creature);
			}
			SendWalking(registry, creature, *leashes, point);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {}'s walk to put down: {}", entt::to_integral(creature),
			                   started ? "started" : "refused");
			// A walk refused ends what it was sent to do, the put-down with it
			if (started)
			{
				leashes->putDownAfterWalk = CreatureLeash::PutDownAfterWalk::Due;
			}
		}
		break;
	case leash::PointOrder::CarryOtherwise:
		leashes->lastOrderPoint = point;
		if (Locator::creatureLocomotionSystem::has_value())
		{
			// Holding something on the aggression leash, or sent where it can't stand, it stops what it does, walks to
			// where it throws from, eight times its height from the point, and throws the thing at the point on the ground;
			// its turns since it was made to act go on, and it tells the player nothing
			std::ignore = StopWhatItDoes(registry, creature);
			auto& locomotion = Locator::creatureLocomotionSystem::value();
			const auto& position = std::as_const(registry).Get<const Transform>(creature).position;
			const auto walk = leash::WalkToThrowAt({position.x, position.z}, taken.walkTo,
			                                       CreatureHeight(std::as_const(registry).Get<const Creature>(creature)));
			bool started = true;
			if (walk.has_value())
			{
				started = !walk->refused && locomotion.MoveTo(creature, walk->to, CreatureLocomotionSystemInterface::Pace::Walk,
				                                              walk->minDistance, walk->maxDistance) ==
				                                CreatureLocomotionSystemInterface::MoveResult::Started;
				if (!started)
				{
					// as the failed move's end of the action: a walk brakes to a stop
					locomotion.BrakeToStop(creature);
				}
			}
			SendWalking(registry, creature, *leashes, point);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {}'s walk to throw: {}", entt::to_integral(creature),
			                   !walk.has_value() ? "none needed"
			                   : started         ? "started"
			                                     : "refused");
			// A walk refused ends what it was sent to do, the throw with it
			if (started)
			{
				leashes->putDownAfterWalk = CreatureLeash::PutDownAfterWalk::Due;
				leashes->throwAt = glm::vec3(taken.walkTo.x, GroundAt(taken.walkTo), taken.walkTo.y);
			}
		}
		break;
	case leash::PointOrder::DrinkFromSea:
	case leash::PointOrder::LookAtReflection:
	case leash::PointOrder::Sleep:
		if (taken.order == leash::PointOrder::DrinkFromSea)
		{
			// sent to drink from the sea, the player has shown they know how to make it drink
			PromptKnown(creature, creature_help::Prompt::Drink);
		}
		if (const auto plan = leash::PlanForPointOrder(taken.order))
		{
			OrderPlanAtPoint(registry, creature, *leashes, *plan, taken.order != leash::PointOrder::Sleep, point);
		}
		break;
	}
	// An order taken is marked where it was sent
	if (leash::AcknowledgesOrder(taken.order))
	{
		ShowTargetMarker(creature, *leashes, taken.walkTo);
	}
	// Whatever came of the order, it is doing what the leash sent it to do; onto a field, the sea or to sleep, until its
	// mind is done with the plan it was made to carry out, as with any thing it is told to act on
	leashes->control = CreatureLeash::Control::Sent;
	leashes->sentToThing = taken.order == leash::PointOrder::ActOnField || leash::PlanForPointOrder(taken.order).has_value();
}

void LeashSystem::MarkTargetAt(entt::entity creature, entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* leashes = Find<CreatureLeash>(registry, creature);
	const auto* transform = registry.Valid(thing) ? std::as_const(registry).TryGet<const Transform>(thing) : nullptr;
	if (leashes == nullptr || transform == nullptr)
	{
		return;
	}
	CloseTargetMarker(*leashes);
	if (Locator::particleSystem::has_value())
	{
		leashes->targetMarker = Locator::particleSystem::value().StartSpotVisual(
		    SpotVisualType::CreatureTarget, transform->position, leash::k_TargetMarkerTurns, creature,
		    leash::k_TargetMarkerMagnitude);
	}
	leashes->targetThing = thing;
	leashes->targetPoint = leash::TargetAbove(transform->position, ecs::object::GetHeight(thing));
	leashes->hasTarget = true;
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {}'s leash target: thing {}", entt::to_integral(creature),
	                   entt::to_integral(thing));
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
		// kept as the game keeps a home, in whole map units
		leashes.home = glm::vec3(map_coords::Quantise(leashes.home->x), leashes.home->y, map_coords::Quantise(leashes.home->z));
	}
	leashes.confinementCentre = *leashes.home;
	leashes.confinementRadius = radius;
}

void LeashSystem::ReleaseUnleashed(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* leashes = Find<CreatureLeash>(registry, creature);
	if (leashes == nullptr || leashes->worn.has_value())
	{
		return;
	}
	// As the leash comes off: the area it is kept in goes, and any leash picked but the learning leash lets go of the
	// desire made dominant
	leashes->confinementRadius = 0.0f;
	leashes->control = CreatureLeash::Control::Idle;
	// and the actions it gave up for walking it out of the area it was kept within may be taken again
	if (auto* mind = MindOf(registry, creature))
	{
		mind->unavailableWhenConfined.clear();
	}
	if (leashes->selected != LeashType::Rope)
	{
		EndDominance(registry, creature);
	}
	// With no leash to come off, it still wipes the gesture path of the owner's hand when they play at this machine
	if (const auto owner = OwnerOfCreature(registry, creature); owner.has_value())
	{
		ForgetLocalGesturePath(*owner);
	}
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

void LeashSystem::ReleaseFromHome(entt::entity creature, uint32_t developmentPhase)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* leashes = Find<CreatureLeash>(registry, creature);
	if (leashes != nullptr &&
	    creature_leash::ReleasedFromHome(developmentPhase, leashes->confinementRadius, leashes->worn.has_value()))
	{
		// Only the radius goes; whether it was walking back is left as it was
		leashes->confinementRadius = 0.0f;
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
	// The player's primary creature, the one creature they lead; without the player service, the same one found here.
	// The neutral player has a creature too, but leads none
	if (!leash::CanLead(player))
	{
		return std::nullopt;
	}
	if (Locator::playerSystem::has_value())
	{
		return Locator::playerSystem::value().GetPrimaryCreature(player);
	}
	if (const auto id = leash::LeashableOf(Claims(), player))
	{
		return static_cast<entt::entity>(*id);
	}
	return std::nullopt;
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
		ForgetGoneTarget(registry, leashes);
		// Fighting or knocked out, the leash doesn't move it about. Its moods, the area it is kept within and how it
		// warms to a creature it is tied to go on all the same, as the game's creature turn takes them whatever the
		// creature is doing. Strayed too far it is walked back all the same while it fights, though not knocked out
		const bool fighting = registry.AnyOf<CreatureFighting, CreatureKnockedOut>(entity);
		const bool knockedOut = registry.AllOf<CreatureKnockedOut>(entity);
		const bool moving =
		    Locator::creatureLocomotionSystem::has_value() && Locator::creatureLocomotionSystem::value().IsMoving(entity);

		// The walk the leash or its home sent it on, or what the leash sent it to do at a point, is over once it stops,
		// arrived or given up on the way: its mind takes over again
		// A wait it was given after the walk is counted first, a turn at a time, and the walk is over with it. A turn its
		// body plays an action, or its hands act on a thing or catch one, is not counted
		bool waiting = false;
		if (!fighting && leashes.returning && !moving && leashes.waitAfterWalk.has_value())
		{
			const auto* animation = registry.TryGet<const CreatureAnimation>(entity);
			const bool handling = Locator::creatureObjectActionSystem::has_value() &&
			                      (Locator::creatureObjectActionSystem::value().GetProgress(entity).has_value() ||
			                       Locator::creatureObjectActionSystem::value().IsCatching(entity));
			const bool bodyActing = creature_body::IsPerformingAction(
			    {.playing = animation != nullptr && creature_layers::IsPlaying(animation->body), .handling = handling});
			const auto step = leash::CountWait(*leashes.waitAfterWalk, bodyActing);
			leashes.waitAfterWalk = step.left;
			waiting = !step.over;
			if (step.over)
			{
				leashes.waitAfterWalk.reset();
			}
		}
		// Sent there holding something, it puts the thing down (or throws it) once it stops, and the walk is over once its
		// hands are free
		if (!fighting && leashes.returning && !moving && leashes.putDownAfterWalk != CreatureLeash::PutDownAfterWalk::None)
		{
			auto* hands =
			    Locator::creatureObjectActionSystem::has_value() ? &Locator::creatureObjectActionSystem::value() : nullptr;
			if (leashes.putDownAfterWalk == CreatureLeash::PutDownAfterWalk::Due)
			{
				const bool started = hands != nullptr && (leashes.throwAt.has_value() ? hands->Throw(entity, *leashes.throwAt)
				                                                                      : hands->PutDown(entity));
				leashes.putDownAfterWalk =
				    started ? CreatureLeash::PutDownAfterWalk::Started : CreatureLeash::PutDownAfterWalk::None;
				const auto& at = registry.Get<const Transform>(entity).position;
				const auto* mindNow = registry.TryGet<const CreatureMindState>(entity);
				SPDLOG_LOGGER_INFO(
				    spdlog::get("game"),
				    "Creature {} stopped {:.1f} from where it was sent: it {} {}; plan action {}, forced {}, step {} of {}, "
				    "obeying {}, by script {}",
				    entt::to_integral(entity),
				    glm::distance(glm::vec2(at.x, at.z), glm::vec2(leashes.returningTo.x, leashes.returningTo.z)),
				    leashes.throwAt.has_value() ? "throws" : "puts down", started ? "what it holds" : "nothing",
				    mindNow != nullptr && mindNow->planner.current.has_value()
				        ? static_cast<int>(mindNow->planner.current->action)
				        : -1,
				    mindNow != nullptr && mindNow->planner.current.has_value() && mindNow->planner.current->forced,
				    mindNow != nullptr ? mindNow->idle.step : 0, mindNow != nullptr ? mindNow->idle.agenda.size() : 0,
				    mindNow != nullptr && mindNow->leash.obeying, ControlledByScript(std::as_const(registry), entity));
			}
			else if (hands == nullptr || hands->GetState(entity) != CreatureObjectActionSystemInterface::State::Busy)
			{
				leashes.putDownAfterWalk = CreatureLeash::PutDownAfterWalk::None;
			}
			waiting = waiting || leashes.putDownAfterWalk != CreatureLeash::PutDownAfterWalk::None;
		}
		// Sent to act on a thing, it is doing that while its mind is still to take the thing up or carries out its plan
		const bool onThing = leashes.control == CreatureLeash::Control::Sent && leashes.sentToThing && mind != nullptr &&
		                     (mind->leash.ordered.has_value() || !mind->leash.actOn.empty() ||
		                      (mind->planActive && mind->idle.step < mind->idle.agenda.size()));
		if (!fighting && !waiting && !onThing && (leashes.control != CreatureLeash::Control::Idle || leashes.returning) &&
		    !moving)
		{
			// Done, its mark goes as it stops what it was doing
			CloseTargetMarker(leashes);
			leashes.control = CreatureLeash::Control::Idle;
			leashes.sentToThing = false;
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
			if (!knockedOut)
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
				    game_math::GetDistanceInMetres(otherAt, transform->position) <
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
		if (!knockedOut)
		{
			KeepWithin(registry, entity, leashes, true, worn.works);
		}
	}
}

void LeashSystem::Update(float seconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto hand = HandPoint();
	const auto handRope = HandRopePoint(hand);
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
		// The rope is laid from the hand's point and then swings from its wrist
		std::optional<glm::vec3> start;
		if (worn.tiedTo.has_value() && registry.Valid(*worn.tiedTo) &&
		    registry.TryGet<const Transform>(*worn.tiedTo) != nullptr)
		{
			start = TiedPoint(registry, *worn.tiedTo);
		}
		else
		{
			start = worn.ropeStarted ? handRope : hand;
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
		// one that could not be laid as it went on, tied or untied is laid now, from the same points
		if (!worn.ropeStarted)
		{
			const auto laidStart = worn.tiedTo.has_value() ? LaidStartPoint(registry, worn).value_or(*start) : *start;
			worn.rope = leash_rope::Create(laidStart, LaidCollarPoint(registry, entity), worn.rope.slackLength,
			                               worn.rope.maxLength, leash::LookFor(worn.type), GroundAt);
			worn.ropeStarted = true;
			continue;
		}
		worn.rope.look = leash::LookFor(worn.type);
		leash_rope::Step(worn.rope, *start, end, seconds, GroundAt);
		// the hand pulling the tied rope: its two ropes follow the hand, the collar and what the leash is tied to. Untied,
		// or with the tied thing gone, they go
		if (worn.pull.has_value())
		{
			if (!worn.tiedTo.has_value() || !registry.Valid(*worn.tiedTo) ||
			    registry.TryGet<const Transform>(*worn.tiedTo) == nullptr)
			{
				worn.pull.reset();
			}
			else if (hand.has_value())
			{
				leash_rope::Step(worn.pull->toCollar, *hand, end, seconds, GroundAt);
				leash_rope::Step(worn.pull->toTied, *hand, PullTiedPoint(registry, *worn.tiedTo), seconds, GroundAt);
			}
		}
	}
}
