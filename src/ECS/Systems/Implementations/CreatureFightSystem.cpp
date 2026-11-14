/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureFightSystem.h"

#include <cmath>

#include <algorithm>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/mat3x3.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Audio/Audio.h"
#include "Audio/Services/Guidance.h"
#include "Camera/Camera.h"
#include "Camera/CameraModel.h"
#include "Camera/FightWatch.h"
#include "Camera/ScriptCamera.h"
#include "Common/GameDistance.h"
#include "Common/GameRandom.h"
#include "Creature/CaveCopyPick.h"
#include "Creature/CreatureArenaRing.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFizz.h"
#include "Creature/CreatureHelp.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureMarks.h"
#include "Creature/CreatureMode.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSkinRay.h"
#include "Creature/CreatureTurnPose.h"
#include "Creature/FightMarks.h"
#include "Creature/LeashRules.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/CreatureTurnPose.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureHome.h"
#include "ECS/CreatureNaming.h"
#include "ECS/CreaturePose.h"
#include "ECS/CreatureSkin.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/HandMeshRay.h"
#include "ECS/Life.h"
#include "ECS/MobileDrawing.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureFizzSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Input/InterfaceActive.h"
#include "Input/LocalPlayer.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Particles/LightSheet.h"
#include "Physics/Body.h"
#include "Physics/BodyShapes.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature::CreatureRig;
namespace fight = openblack::creature_fight;
namespace feedback = openblack::creature_feedback;
using Pace = CreatureLocomotionSystemInterface::Pace;

namespace
{
constexpr float k_TurnSeconds = game_clock::k_TurnSeconds;
/// The creature action table's row for fighting, whose strength gain a fight given up adds to each fighter
constexpr size_t k_FightActionRow = 21;

/// The strength a fight given up adds to a fighter, from the action table's fight row; none without the tables
std::optional<float> FightStrengthGain()
{
	if (!Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto& actions = Locator::infoConstants::value().creatureAction;
	return k_FightActionRow < actions.size() ? std::optional(actions[k_FightActionRow].strengthGain) : std::nullopt;
}

/// A game turn's milliseconds, as the game's tick time sets them
float TurnMs()
{
	return static_cast<float>(game_clock::MsPerTurn());
}
/// The game's synchronised random numbers, as the fight rules draw them
uint32_t GameRandom(uint32_t range)
{
	return game_random::GameRand(range);
}
/// A blow is aimed by rays spread across this share of the opponent's height to each side of the way to it, this many
constexpr float k_BlowSweepShare = 0.15f;
constexpr int k_BlowSweepSamples = 11;
/// A blow whose file gives it a moment earlier than this, in its animation's milliseconds, is not measured
constexpr int32_t k_EarliestHitMs = 50;
/// A face is due again once its wait is within this much of over
constexpr float k_FaceDue = 1e-3f;
/// The stages before the duel give up waiting after this long
constexpr float k_ApproachSeconds = 30.0f;
constexpr float k_PooApproachSeconds = 20.0f;
/// A blow leaves a trail of this many drops of blood down the skin
constexpr int k_BloodDrops = 12;
/// Wounds are placed at random on the skin this far from its edges
constexpr uint32_t k_WoundEdge = 16;
/// The miscellaneous help message that the creature is being taken home, as a Die action's steps are laid
constexpr uint32_t k_TransportedHomeHelp = 6;
/// The help: "the creature has fainted"
constexpr uint32_t k_HasFaintedHelp = 3;

float HeightOf(float size)
{
	return feedback::k_HeightAtSizeOne * std::max(size, 0.05f);
}

/// The height of a creature's body as it is drawn: by the pen's size in its temple's pen, else by its own
float DrawnHeightOf(const ecs::Registry& registry, entt::entity creature)
{
	return HeightOf(ecs::creature_pose::DrawnSize(registry, creature));
}

/// The scale a creature's body is drawn at, along x: the pen's in its temple's pen, else its Transform's
float DrawnScaleOf(const ecs::Registry& registry, entt::entity creature)
{
	return std::abs(ecs::creature_pose::DrawnScale(registry, creature).x);
}

/// The strength that a creature's body shows, from -1 (weak) to 1 (strong): its own strength with a fifth of its
/// species'. The blows compare these strengths.
float ShownStrengthOf(const ecs::Registry& registry, entt::entity creature)
{
	return ecs::creature_pose::TargetMorph(registry, creature).weakStrong;
}

glm::vec2 Flat(const glm::vec3& point)
{
	return {point.x, point.z};
}

float GroundAt(glm::vec2 point)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
}

float LifeOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto* needs = registry.TryGet<const CreatureNeeds>(creature);
	return needs != nullptr ? needs->needs.life : 1.0f;
}

const CreatureRig* RigOf(CreatureType species)
{
	auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto id = creature::GetRigId(species);
	return rigs.Contains(id) ? &*rigs.Handle(id) : nullptr;
}

/// How far one of the species' animations carries the root, in its mesh's units, ahead being -z
glm::vec2 DisplacementOf(CreatureType species, size_t animation)
{
	const auto* rig = RigOf(species);
	const auto* played = rig != nullptr ? rig->GetAnimation(CreatureRig::Mesh::Base, animation) : nullptr;
	return played != nullptr && !played->frames.empty() ? glm::vec2(played->displacement.x, played->displacement.z)
	                                                    : glm::vec2(0.0f);
}

float DurationOf(entt::entity creature, size_t animation)
{
	if (!Locator::creatureAnimationSystem::has_value())
	{
		return 0.0f;
	}
	return Locator::creatureAnimationSystem::value().AnimationDuration(creature, animation).value_or(0.0f);
}

bool Has(entt::entity creature, size_t animation)
{
	return DurationOf(creature, animation) > 0.0f;
}

/// A point in the mesh's space of a creature facing a heading, in the world's directions
glm::vec2 MeshToWorld(glm::vec2 mesh, float heading)
{
	const auto turned = glm::mat3(glm::eulerAngleY(heading)) * glm::vec3(mesh.x, 0.0f, mesh.y);
	return {turned.x, turned.z};
}

/// A direction in the world as seen by a creature facing a heading: x to its right, y ahead
glm::vec2 WorldToLocal(glm::vec2 world, float heading)
{
	const auto ahead = creature_locomotion::DirectionOf(heading);
	const glm::vec2 right {-ahead.y, ahead.x};
	return {glm::dot(world, right), glm::dot(world, ahead)};
}

/// Moves a creature where it stands by the end of this turn; it is drawn moving there from where the turn started
void Shift(CreatureLocomotion& locomotion, glm::vec2 by)
{
	auto& point = locomotion.toPosition;
	point.x += by.x;
	point.z += by.y;
	point.y = GroundAt(Flat(point));
}

/// How far a fighter's body reaches ahead of where it stands at its fight animation's time: its bones as that animation
/// poses them, at the size it is drawn, ahead along the way it faces
float ForwardExtentOf(const ecs::Registry& registry, entt::entity creature, const fight::Fighter& fighter)
{
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	if (animation == nullptr || !Locator::creatureAnimationSystem::has_value())
	{
		return 0.0f;
	}
	auto& animations = Locator::creatureAnimationSystem::value();
	const auto scale = DrawnScaleOf(registry, creature);
	std::vector<float> ahead;
	ahead.reserve(animation->skeleton.parents.size());
	for (uint32_t bone = 0; bone < animation->skeleton.parents.size(); ++bone)
	{
		if (const auto point = animations.BoneInAnimation(creature, fighter.animation, fighter.timeMs, bone, fighter.mirrored))
		{
			ahead.push_back(-point->z * scale);
		}
	}
	return fight::ForwardExtent(ahead);
}

/// Puts a creature at a point at once, out of its turn's walk: it is drawn there with no slide
void PlaceAt(entt::entity creature, CreatureLocomotion& locomotion, glm::vec3 at)
{
	at.y = GroundAt(Flat(at));
	locomotion.fromPosition = at;
	locomotion.toPosition = at;
	ecs::creature_pose::CommitTurnPose(creature, at, locomotion.heading);
	ecs::NotifyTeleported(creature);
}

/// Sets a creature fizzing out of sight or back in
void SetFizz(entt::entity creature, float target, float seconds)
{
	if (Locator::creatureFizzSystem::has_value())
	{
		Locator::creatureFizzSystem::value().SetFizz(creature, target, seconds, false);
	}
}

/// Carried home and stopped part way, a creature is back in full sight at once
void StopCarryingHome(entt::entity creature, const CreatureKnockedOut& knockedOut)
{
	if (knockedOut.stage == CreatureKnockedOut::Stage::FadingOut || knockedOut.stage == CreatureKnockedOut::Stage::FadingIn)
	{
		SetFizz(creature, 0.0f, 0.0f);
	}
}

/// Brings the spells on a creature lying knocked out, then those on the creature its action is done to (itself when
/// none), to their end: whether neither has any left. Done to a thing that is not a creature, it is never over
bool SpellsOver(ecs::Registry& registry, entt::entity creature, std::optional<entt::entity> actedOn)
{
	const auto finish = [&registry](entt::entity entity) {
		return std::as_const(registry).TryGet<const CreatureSpells>(entity) == nullptr ||
		       creature_spells::TryFinishAll(registry.Get<CreatureSpells>(entity).spells);
	};
	if (!finish(creature))
	{
		return false;
	}
	const auto other = actedOn.value_or(creature);
	if (!registry.Valid(other) || !std::as_const(registry).AllOf<Creature>(other))
	{
		return false;
	}
	return finish(other);
}

/// The game turns a carry home's fade out, or its fade back in, lasts
uint16_t CarryHomeTurns()
{
	return creature_fizz::TurnsOf(creature_fizz::k_CarryHomeSeconds, game_clock::MsPerTurn());
}

/// Turns a creature to face a heading outright, by the end of this turn, the heading wrapped once within -pi to pi
void FaceHeading(CreatureLocomotion& locomotion, float heading)
{
	const auto faced = affine::WrapAngle(heading);
	locomotion.heading = faced;
	locomotion.targetHeading = faced;
	locomotion.toHeading = faced;
}

/// The body plays a fight animation at the fighter's time, looping until the fight moves it on. The fight alone moves
/// the time on, at the move's own speed, so that the body shows exactly where the move is: blows land and sound as drawn,
/// whatever the frame rate.
creature_layers::BodyAction ShownBody(const fight::Fighter& fighter, bool poweringUp, float durationMs, float timeMs)
{
	const auto played = fighter.state == fight::State::Stance && poweringUp ? fight::animations::k_PowerUp : fighter.animation;
	return {
	    .kind = creature_layers::BodyAction::Kind::Sequence,
	    .phase = creature_layers::BodyAction::Phase::Loop,
	    .animations = {played, played, played},
	    .timeMs = std::clamp(timeMs, 0.0f, std::max(durationMs - 1.0f, 0.0f)),
	    .mirrored = fighter.mirrored,
	    .holdLoop = fighter.state == fight::State::Lying,
	    .timedByPlayer = true,
	};
}

/// The body set to play that, in place of any slots
void Show(CreatureAnimation& animation, const fight::Fighter& fighter, bool poweringUp, float durationMs, float timeMs)
{
	animation.body = ShownBody(fighter, poweringUp, durationMs, timeMs);
	animation.slots.clear();
}

void PlayOnce(CreatureAnimation& animation, size_t played)
{
	animation.body = {};
	if (const auto once = creature_layers::PlayOnce(animation.body, played, false))
	{
		animation.body = *once;
	}
}

void SetEyes(ecs::Registry& registry, entt::entity creature, creature_eyes::Mode mode)
{
	if (auto* eyes = registry.TryGet<CreatureEyes>(creature))
	{
		eyes->mode = mode;
	}
}

std::vector<feedback::Capsule> BodyOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (body == nullptr || animation == nullptr || transform == nullptr)
	{
		return {};
	}
	// The body as it is drawn, at the turn's place
	return feedback::BodyCapsules(
	    animation->skeleton.parents, animation->boneMatrices,
	    creature::PlacementMatrix(transform->position, transform->rotation, ecs::creature_pose::DrawnScale(registry, creature)),
	    feedback::k_BodyRadiusShare * DrawnHeightOf(registry, creature));
}

/// The bones of a creature as the ellipsoids that fight blows strike: each bone's box about the vertices it moves in the
/// species' base model, placed by the bones as the game posed them this turn, as they are drawn until it has been posed
std::vector<openblack::physics::Ellipsoid> BonesOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (body == nullptr || animation == nullptr || transform == nullptr || !Locator::resources::has_value())
	{
		return {};
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto meshId = creature::GetIdFromType(body->species, creature::CreatureBody::Appearance::Base);
	if (!meshes.Contains(meshId))
	{
		return {};
	}
	std::span<const glm::mat4> boneMatrices = animation->boneMatrices;
	auto placement =
	    creature::PlacementMatrix(transform->position, transform->rotation, ecs::creature_pose::DrawnScale(registry, creature));
	if (const auto* pose = registry.TryGet<const CreatureTurnPose>(creature);
	    pose != nullptr && creature_turn_pose::HasPose(*pose))
	{
		boneMatrices = creature_turn_pose::Safe(*pose);
		placement = glm::mat4(1.0f);
	}
	if (boneMatrices.empty())
	{
		return {};
	}
	std::vector<glm::vec3> positions;
	std::vector<uint16_t> bones;
	for (const auto& subMesh : meshes.Handle(meshId)->GetSubMeshes())
	{
		const auto& local = subMesh->GetSkinLocalPositions();
		const auto& skin = subMesh->GetSkinBones();
		positions.insert(positions.end(), local.begin(), local.end());
		bones.insert(bones.end(), skin.begin(), skin.end());
		bones.resize(positions.size(), uint16_t {0});
	}
	const auto boxes = openblack::physics::shapes::BoneBoxes(positions, bones, boneMatrices.size());
	std::vector<openblack::physics::Ellipsoid> skeleton;
	skeleton.reserve(boneMatrices.size());
	for (uint32_t bone = 0; bone < boneMatrices.size(); ++bone)
	{
		const auto posed = creature::PosedBone(bone, boneMatrices, placement);
		const auto box = bone < boxes.size() ? boxes[bone] : openblack::physics::shapes::BoneBox {};
		skeleton.push_back({.boxMin = box.min, .boxMax = box.max, .localToWorld = posed, .worldToLocal = glm::inverse(posed)});
	}
	return skeleton;
}

/// The body as the game posed it this turn, in the world, which blows are aimed at and land on; as it is drawn until
/// it has been posed
std::vector<feedback::Capsule> BodyThisTurn(const ecs::Registry& registry, entt::entity creature)
{
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	const auto* pose = registry.TryGet<const CreatureTurnPose>(creature);
	if (animation == nullptr || pose == nullptr || !creature_turn_pose::HasPose(*pose))
	{
		return BodyOf(registry, creature);
	}
	return feedback::BodyCapsules(animation->skeleton.parents, creature_turn_pose::Safe(*pose), glm::mat4(1.0f),
	                              feedback::k_BodyRadiusShare * DrawnHeightOf(registry, creature));
}

/// Where a creature is carried when its player has no temple and it passed out outside a fight, as on the testbed: the
/// middle of the land, where the testbed puts its creatures
std::optional<glm::vec3> NoPen()
{
	if (!Locator::terrainSystem::has_value())
	{
		return std::nullopt;
	}
	const auto& land = Locator::terrainSystem::value();
	const auto middle = (land.GetExtent().minimum + land.GetExtent().maximum) * 0.5f;
	return glm::vec3(middle.x, land.GetHeightAt(middle), middle.y);
}

/// Who is in a duel with whom, both of them still at it
/// As the start of a cast in a fight ends, the creature tries its spell: an aggressive one at its opponent, a defensive
/// one at itself, and none without an opponent. What becomes of the try changes nothing in the fight
void CastFightSpell(entt::entity creature, const CreatureFighting& fighting, bool hasOpponent)
{
	if (!fighting.fighter.spell.has_value() || !Locator::creatureMindSystem::has_value())
	{
		return;
	}
	const auto type = static_cast<MagicType>(*fighting.fighter.spell);
	const auto& effect = magic::GetMagicEffectInfo(Locator::infoConstants::value(), type);
	const auto at = fight::FightSpellTarget(effect.isAggressiveSpellWhichIsUsedInCreatureFightArena != 0,
	                                        effect.isDefensiveSpellWhichIsUsedInCreatureFightArena != 0, hasOpponent);
	if (at != fight::FightSpellAt::Nothing)
	{
		[[maybe_unused]] const bool cast = Locator::creatureMindSystem::value().TryMiracle(
		    creature, type, at == fight::FightSpellAt::Opponent ? fighting.opponent : creature);
	}
}

bool InDuel(const ecs::Registry& registry, entt::entity creature)
{
	const auto* fighting = registry.TryGet<const CreatureFighting>(creature);
	return fighting != nullptr && fighting->stage == CreatureFighting::Stage::Duel;
}

/// The player's own camera's fight watching, if it has the camera: not while a script camera mode is on or the script
/// has the screen
camera::FightWatch* PlayersFightWatch()
{
	if (!Locator::camera::has_value() || script_camera::HasMode() || !interface_active::IsActive())
	{
		return nullptr;
	}
	return Locator::camera::value().GetModel().GetFightWatch();
}

/// The fight is over: the player's camera lingers on it, then ends its watch
void TellCameraFightIsOver()
{
	if (auto* watch = PlayersFightWatch())
	{
		watch->EndFight();
	}
}
} // namespace

CreatureFightSystemInterface::StartResult CreatureFightSystem::StartFight(entt::entity creature, entt::entity opponent)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto ready = [&registry](entt::entity entity) {
		return registry.Valid(entity) && registry.AllOf<Creature, Transform, CreatureAnimation, CreatureLocomotion>(entity);
	};
	if (creature == opponent || !ready(creature) || !ready(opponent))
	{
		return StartResult::NoOpponent;
	}
	if (registry.AnyOf<CreatureFighting, CreatureKnockedOut>(creature) ||
	    registry.AnyOf<CreatureFighting, CreatureKnockedOut>(opponent))
	{
		return StartResult::Busy;
	}
	if (!fight::HealthyEnoughToFight(LifeOf(registry, creature)))
	{
		return StartResult::TooWeak;
	}
	// Every fight starts by asking the other creature, which may refuse
	if (!fight::AgreesToFight(DrawnHeightOf(registry, opponent), DrawnHeightOf(registry, creature),
	                          registry.AllOf<CreatureKnockedOut>(opponent), ecs::script_held::IsControlledByScript(opponent),
	                          ecs::script_held::IsControlledByScript(creature)))
	{
		return StartResult::Refused;
	}
	const auto& from = registry.Get<const Transform>(creature).position;
	const auto& to = registry.Get<const Transform>(opponent).position;
	// sized by the bigger body as drawn, smaller in a temple's pen
	auto arena = fight::MakeArena(Flat(from), Flat(to), ecs::creature_pose::DrawnSize(registry, creature),
	                              ecs::creature_pose::DrawnSize(registry, opponent));
	arena.serial = ++_arenasMade;

	for (const auto& [self, other] : {std::pair(creature, opponent), std::pair(opponent, creature)})
	{
		const auto& body = registry.Get<const Creature>(self);
		if (Locator::creatureLocomotionSystem::has_value())
		{
			Locator::creatureLocomotionSystem::value().Stop(self);
		}
		if (Locator::creatureObjectActionSystem::has_value())
		{
			Locator::creatureObjectActionSystem::value().Cancel(self);
			Locator::creatureObjectActionSystem::value().Drop(self);
		}
		registry.Get<CreatureAnimation>(self).body = {};

		// Starting a fight adds nothing to the creature's battles fought
		auto& record = registry.AllOf<CreatureFightRecord>(self) ? registry.Get<CreatureFightRecord>(self)
		                                                         : registry.AssignState<CreatureFightRecord>(self);
		// The creature that starts the fight chooses its moves by what it has learnt; the other by the lean it chose by
		// in the last fight it started
		if (self == creature)
		{
			record.tendency = fight::TendencyAtStart(record.foughtBefore, record.tendency, body.alignment);
			record.choosingTendency = record.tendency;
		}
		const auto* needs = registry.TryGet<const CreatureNeeds>(self);
		const auto bodyNeeds = needs != nullptr ? needs->needs : creature_physiology::Needs {};

		CreatureFighting fighting {
		    .stage = CreatureFighting::Stage::Approach,
		    .opponent = other,
		    .arena = arena,
		    .madeArena = self == creature,
		    .startPosition = registry.Get<const Transform>(self).position,
		};
		auto& fighter = fighting.fighter;
		fighter.health = fight::FightHealthAtStart(bodyNeeds.life);
		fighter.stamina = fight::StaminaAtStart(bodyNeeds.energy, bodyNeeds.exhaustion);
		// The human player directs their own creature; any other fights by itself
		fighter.control = body.owner == creature::LocalPlayer() ? fight::Control::Player : fight::Control::Computer;
		fighter.computerWaitMs = fight::k_ComputerWaitsAtStartMs;
		fighter.tendency = record.choosingTendency;
		registry.AssignOrReplaceState<CreatureFighting>(self, std::move(fighting));
	}

	// The fight takes its arena with the game's sound at its middle, on the land there, not following anything
	if (Locator::audio::has_value())
	{
		const auto soundAt = fight::ArenaSoundPoint(arena);
		audio::PlayOptions sound;
		sound.sample = {audio::Bank(audio::SfxBank::InGame), fight::k_ArenaTakenSample};
		sound.is3D = true;
		sound.track = false;
		sound.position = glm::vec3(soundAt.x, GroundAt(soundAt), soundAt.y);
		audio::PlaySoundEffect(sound);
	}

	// Each fighter's start makes a fight reaction about it, for its player, at once, which those who see it may take
	// notice of
	if (Locator::reactionSystem::has_value())
	{
		for (const auto fighter : {creature, opponent})
		{
			const auto owner = std::as_const(registry).Get<const Creature>(fighter).owner;
			static_cast<void>(ecs::effects::reactions::CreateReaction(fighter, openblack::Reaction::ReactToFight, owner, true));
		}
	}
	WatchFightStart(creature, opponent, arena);
	return StartResult::Started;
}

void CreatureFightSystem::WatchFightStart(entt::entity fighterA, entt::entity fighterB, const fight::Arena& arena) const
{
	// Only the player's own camera watches fights
	auto* watch = PlayersFightWatch();
	if (watch == nullptr)
	{
		return;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto local = creature::LocalPlayer();
	const bool localFighter =
	    registry.Get<const Creature>(fighterA).owner == local || registry.Get<const Creature>(fighterB).owner == local;
	const bool scriptControlled =
	    ecs::script_held::IsControlledByScript(fighterA) || ecs::script_held::IsControlledByScript(fighterB);
	// The camera reads the arena's place as a map position keeps it, for its test and for the watch
	const glm::vec2 place(map_coords::Quantise(arena.centre.x), map_coords::Quantise(arena.centre.y));
	const glm::vec3 centre(place.x, GroundAt(place), place.y);
	switch (fight::ViewAtFightStart(localFighter, scriptControlled, !watch->WantToQuitFight(centre, arena.radius)))
	{
	case fight::FightView::Watch:
		if (_cameraWatches)
		{
			watch->StartFight(fighterA, fighterB, centre, arena.radius);
		}
		break;
	case fight::FightView::Remark:
		openblack::audio::guidance::RemarkCreatureFight();
		break;
	case fight::FightView::None:
		break;
	}
}

void CreatureFightSystem::AbortFight(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = registry.TryGet<CreatureFighting>(creature);
	if (fighting == nullptr)
	{
		return;
	}
	const auto opponent = fighting->opponent;
	for (const auto self : {creature, opponent})
	{
		auto* side = registry.Valid(self) ? registry.TryGet<CreatureFighting>(self) : nullptr;
		if (side == nullptr)
		{
			continue;
		}
		const auto* other = registry.Valid(side->opponent) ? registry.TryGet<const CreatureFighting>(side->opponent) : nullptr;
		if (side->stage == CreatureFighting::Stage::Duel)
		{
			EndFightFor(self, other == nullptr || side->fighter.health >= other->fighter.health);
			side->stage = CreatureFighting::Stage::Respond;
			side->played = false;
			fight::Enter(side->fighter, fight::State::Finish);
		}
		else if (side->stage == CreatureFighting::Stage::Approach || side->stage == CreatureFighting::Stage::Taunt ||
		         side->stage == CreatureFighting::Stage::Ready)
		{
			Leave(self);
		}
	}
}

void CreatureFightSystem::CallOff(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = registry.TryGet<CreatureFighting>(creature);
	if (fighting == nullptr)
	{
		return;
	}
	const auto opponent = fighting->opponent;
	for (const auto self : {creature, opponent})
	{
		auto* side = registry.Valid(self) ? registry.TryGet<CreatureFighting>(self) : nullptr;
		if (side == nullptr)
		{
			continue;
		}
		if (side->stage == CreatureFighting::Stage::Duel)
		{
			// Neither wins: each finishes what it is doing and stands, and its fight is over with nothing more to it
			EndFightFor(self, std::nullopt);
			side->stage = CreatureFighting::Stage::Respond;
			side->played = true;
			side->fighter.stopping = true;
			side->fighter.queue.Clear();
		}
		else if (side->stage == CreatureFighting::Stage::Approach || side->stage == CreatureFighting::Stage::Taunt ||
		         side->stage == CreatureFighting::Stage::Ready)
		{
			Leave(self);
		}
	}
}

void CreatureFightSystem::EndOwnFight(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* side = registry.Valid(creature) ? registry.TryGet<CreatureFighting>(creature) : nullptr;
	if (side == nullptr || side->stage != CreatureFighting::Stage::Duel || !fight::CameraLingersOnEnd(side->fighter.state))
	{
		return;
	}
	// Neither wins; it finishes what it is doing and stands, and its opponent is not touched
	EndFightFor(creature, std::nullopt);
	side->stage = CreatureFighting::Stage::Respond;
	side->played = true;
	side->fighter.stopping = true;
	side->fighter.queue.Clear();
}

void CreatureFightSystem::Hurry(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* side = registry.Valid(creature) ? registry.TryGet<CreatureFighting>(creature) : nullptr;
	if (side != nullptr && fight::HurriedByForcedFinish(side->fighter.state))
	{
		side->fighter.hurried = true;
		// A readout at run time (the game channel at trace level), so a player's log shows the forced finish's pace
		if (const auto logger = spdlog::get("game"))
		{
			logger->trace("Creature {} forced to finish in its fight's {}: its moves play at twice the pace",
			              entt::to_integral(creature), fight::Name(side->fighter.state));
		}
	}
}

void CreatureFightSystem::Withdraw(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* fighting = registry.TryGet<const CreatureFighting>(creature);
	if (fighting == nullptr)
	{
		return;
	}
	const auto opponent = fighting->opponent;
	AbortFight(creature);
	// It leaves at once, rather than finishing and standing; an arena it made goes with its fight
	Leave(creature);
	// What it fought forgets it: the opponent plays out the end of its fight alone
	if (auto* other = registry.Valid(opponent) ? registry.TryGet<CreatureFighting>(opponent) : nullptr;
	    other != nullptr && other->opponent == creature)
	{
		other->opponent = entt::null;
	}
}

bool CreatureFightSystem::IsFighting(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(creature) && registry.AllOf<CreatureFighting>(creature);
}

bool CreatureFightSystem::IsDuelling(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(creature) && InDuel(registry, creature);
}

std::optional<entt::entity> CreatureFightSystem::OpponentOf(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* fighting = registry.Valid(creature) ? registry.TryGet<const CreatureFighting>(creature) : nullptr;
	return fighting != nullptr ? std::optional(fighting->opponent) : std::nullopt;
}

std::vector<CreatureFightSystemInterface::ArenaView> CreatureFightSystem::GetArenas() const
{
	// Read through the const registry, so that asking makes no storage. The middle is given as the original's arena
	// keeps it, as a map position
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	using Made = std::pair<uint32_t, ArenaView>;
	std::vector<Made> made;
	registry.Each<const CreatureFighting>([&made](entt::entity entity, const CreatureFighting& fighting) {
		if (fighting.madeArena)
		{
			made.emplace_back(fighting.arena.serial, ArenaView {.maker = entity,
			                                                    .opponent = fighting.opponent,
			                                                    .centre = {map_coords::Quantise(fighting.arena.centre.x),
			                                                               map_coords::Quantise(fighting.arena.centre.y)},
			                                                    .radius = fighting.arena.radius,
			                                                    .serial = fighting.arena.serial});
		}
	});
	std::ranges::sort(made, std::ranges::greater {}, &Made::first);
	std::vector<ArenaView> arenas;
	arenas.reserve(made.size());
	std::ranges::transform(made, std::back_inserter(arenas), &Made::second);
	return arenas;
}

std::optional<CreatureFightSystemInterface::ArenaView> CreatureFightSystem::ArenaOf(entt::entity fighter) const
{
	if (fighter == entt::null)
	{
		return std::nullopt;
	}
	const auto arenas = GetArenas();
	const auto it = std::ranges::find_if(
	    arenas, [fighter](const ArenaView& arena) { return arena.maker == fighter || arena.opponent == fighter; });
	return it != arenas.end() ? std::optional(*it) : std::nullopt;
}

bool CreatureFightSystem::QueueMove(entt::entity creature, const fight::Move& move, bool replace)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = registry.Valid(creature) ? registry.TryGet<CreatureFighting>(creature) : nullptr;
	if (fighting == nullptr)
	{
		return false;
	}
	const bool queued = fight::PlayerMove(fighting->fighter, move, replace);
	// a move in place of those queued leaves only the newest press's mark, now the first
	if (replace)
	{
		fighting->marks.KeepNewest();
	}
	if (fighting->stage == CreatureFighting::Stage::Duel)
	{
		CheckQueue(creature);
	}
	return queued;
}

void CreatureFightSystem::ReleaseCharge(entt::entity creature, float heldMs)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* fighting = registry.Valid(creature) ? registry.TryGet<CreatureFighting>(creature) : nullptr)
	{
		fighting->fighter.queue.Release(heldMs);
		if (fighting->stage == CreatureFighting::Stage::Duel)
		{
			CheckQueue(creature);
		}
	}
}

bool CreatureFightSystem::QueueScriptMove(entt::entity creature, const fight::Move& move)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = registry.Valid(creature) ? registry.TryGet<CreatureFighting>(creature) : nullptr;
	if (fighting == nullptr)
	{
		return false;
	}
	const bool queued = fight::ScriptQueueMove(fighting->fighter, move);
	if (fighting->stage == CreatureFighting::Stage::Duel)
	{
		CheckQueue(creature);
	}
	return queued;
}

uint32_t CreatureFightSystem::QueuedBlows(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* fighting = registry.Valid(creature) ? registry.TryGet<const CreatureFighting>(creature) : nullptr;
	return fighting != nullptr ? fight::QueuedBlows(fighting->fighter.queue.Moves()) : 0;
}

fight::FightAction CreatureFightSystem::CurrentFightAction(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return fight::FightAction::Other;
	}
	if (const auto* fighting = registry.TryGet<const CreatureFighting>(creature))
	{
		return fight::FightActionOf(fighting->fighter.actionAnimation);
	}
	// Knocked out, the faint is the last thing it did, until it gets up
	const auto* knockedOut = registry.TryGet<const CreatureKnockedOut>(creature);
	return knockedOut != nullptr && knockedOut->stage != CreatureKnockedOut::Stage::GettingUp ? fight::FightAction::Fainting
	                                                                                          : fight::FightAction::Other;
}

void CreatureFightSystem::SetAutoFighting(entt::entity creature, bool autoFight)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* fighting = registry.Valid(creature) ? registry.TryGet<CreatureFighting>(creature) : nullptr)
	{
		fighting->fighter.autoFight = autoFight;
		// On, the computer fights for it; off, nobody does
		fighting->fighter.control = autoFight ? fight::Control::Computer : fight::Control::None;
	}
}

bool CreatureFightSystem::IsAutoFighting(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* fighting = registry.Valid(creature) ? registry.TryGet<const CreatureFighting>(creature) : nullptr;
	return fighting != nullptr && (fighting->fighter.autoFight || fighting->fighter.control == fight::Control::Computer);
}

size_t CreatureFightSystem::QueuedMoves(entt::entity creature) const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* fighting = registry.Valid(creature) ? registry.TryGet<const CreatureFighting>(creature) : nullptr;
	return fighting != nullptr ? fighting->fighter.queue.Size() : 0;
}

namespace
{
/// How high a creature's head is over the land under it, as the game's logic poses the body: its species' head bone
/// in the turn's pose; none without the bone or a pose
std::optional<float> HeadHeightOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* pose = registry.TryGet<const CreatureTurnPose>(creature);
	const auto* rig = body != nullptr && Locator::resources::has_value() ? RigOf(body->species) : nullptr;
	if (pose == nullptr || rig == nullptr || !rig->actionPoints.has_value())
	{
		return std::nullopt;
	}
	const auto bones = creature_turn_pose::Safe(*pose);
	if (rig->actionPoints->head >= bones.size())
	{
		return std::nullopt;
	}
	const auto head = glm::vec3(bones[rig->actionPoints->head][3]);
	return head.y - GroundAt(Flat(head));
}
} // namespace

std::optional<float> CreatureFightSystem::HeadHeight(entt::entity creature) const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!registry.Valid(creature))
	{
		return std::nullopt;
	}
	return HeadHeightOf(registry, creature);
}

std::optional<CreatureFightSystemInterface::BodyPoint>
CreatureFightSystem::BodyHit(entt::entity creature, const glm::vec3& rayOrigin, const glm::vec3& rayDirection) const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!registry.Valid(creature) || !registry.AllOf<Creature, Transform>(creature))
	{
		return std::nullopt;
	}
	const auto hit = creature_skin_ray::Nearest(ecs::creature_skin::DrawnSkin(registry, creature), rayOrigin, rayDirection);
	const auto headHeight = HeadHeightOf(registry, creature);
	if (!hit.has_value() || !headHeight.has_value())
	{
		return std::nullopt;
	}
	const auto& at = registry.Get<const Transform>(creature).position;
	return BodyPoint {.point = hit->point, .heightShare = (hit->point.y - at.y) / *headHeight, .spot = hit->spot};
}

size_t CreatureFightSystem::MarkCount(entt::entity creature) const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* fighting = registry.Valid(creature) ? registry.TryGet<const CreatureFighting>(creature) : nullptr;
	return fighting != nullptr ? fighting->marks.count : 0;
}

void CreatureFightSystem::MarkOnSkin(entt::entity creature, bool own, const creature_skin_ray::Spot& spot)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* fighting = registry.Valid(creature) ? registry.TryGet<CreatureFighting>(creature) : nullptr)
	{
		fighting->marks.AddOnSkin(own ? creature_fight_marks::Mark::Kind::Own : creature_fight_marks::Mark::Kind::Opponent,
		                          spot);
	}
}

void CreatureFightSystem::MarkOnGround(entt::entity creature, const glm::vec3& point)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = registry.Valid(creature) ? registry.TryGet<CreatureFighting>(creature) : nullptr;
	if (fighting == nullptr || !registry.AllOf<Transform, CreatureLocomotion>(creature))
	{
		return;
	}
	const auto& at = std::as_const(registry).Get<const Transform>(creature).position;
	const auto heading = std::as_const(registry).Get<const CreatureLocomotion>(creature).heading;
	fighting->marks.AddOnGround(point, creature_fight_marks::LocalOf(point, at, heading));
}

void CreatureFightSystem::LetGoOfMark(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* fighting = registry.Valid(creature) ? registry.TryGet<CreatureFighting>(creature) : nullptr)
	{
		fighting->marks.LetGo();
	}
}

void CreatureFightSystem::TurnMarks()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> marked;
	std::as_const(registry).Each<const CreatureFighting>([&marked](entt::entity entity, const CreatureFighting& fighting) {
		if (fighting.marks.count > 0)
		{
			marked.push_back(entity);
		}
	});
	const auto turnMs = static_cast<uint32_t>(TurnMs());
	for (const auto entity : marked)
	{
		auto& fighting = registry.Get<CreatureFighting>(entity);
		// the held press's mark counts the creature's turn
		fighting.marks.Hold(turnMs);
		// the local player's creature in its duel turns and moves its ground marks with it
		if (!registry.Valid(fighting.opponent) || !registry.AllOf<Transform, CreatureLocomotion>(entity) ||
		    std::as_const(registry).Get<const Creature>(entity).owner != creature::LocalPlayer())
		{
			continue;
		}
		const auto& at = std::as_const(registry).Get<const Transform>(entity).position;
		const auto heading = std::as_const(registry).Get<const CreatureLocomotion>(entity).heading;
		for (size_t i = 0; i < fighting.marks.count; ++i)
		{
			auto& mark = fighting.marks.marks.at(i);
			if (mark.kind == creature_fight_marks::Mark::Kind::Ground)
			{
				mark.world = creature_fight_marks::WorldOf(mark.local, at, heading);
			}
		}
	}
}

void CreatureFightSystem::UpdateSparkles()
{
	_sparkles.clear();
	auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::camera::has_value())
	{
		return;
	}
	std::vector<entt::entity> marked;
	std::as_const(registry).Each<const CreatureFighting>([&marked](entt::entity entity, const CreatureFighting& fighting) {
		if (fighting.marks.count > 0)
		{
			marked.push_back(entity);
		}
	});
	const auto camera = Locator::camera::value().GetOrigin();
	const auto turnFraction = game_clock::TurnFraction();
	for (const auto entity : marked)
	{
		const auto& lookup = std::as_const(registry);
		const auto opponent = lookup.Get<const CreatureFighting>(entity).opponent;
		// only while it fights an opponent, as each frame draws them
		if (!registry.Valid(opponent) || !registry.AllOf<Transform, CreatureLocomotion>(entity))
		{
			continue;
		}
		const auto ownSkin = ecs::creature_skin::DrawnSkin(lookup, entity);
		const auto opponentSkin = ecs::creature_skin::DrawnSkin(lookup, opponent);
		const auto& at = lookup.Get<const Transform>(entity).position;
		const auto heading = lookup.Get<const CreatureLocomotion>(entity).heading;
		const auto size = ecs::creature_pose::DrawnSize(lookup, entity);
		auto& marks = registry.Get<CreatureFighting>(entity).marks;
		for (size_t i = 0; i < marks.count; ++i)
		{
			auto& mark = marks.marks.at(i);
			std::optional<glm::vec3> point;
			uint32_t frame = 0;
			switch (mark.kind)
			{
			case creature_fight_marks::Mark::Kind::Opponent:
				point = creature_skin_ray::PointOf(opponentSkin, mark.spot);
				// a mark on the opponent flickers the faster the longer its press was held
				mark.frame = creature_fight_marks::NextFrame(mark.frame, mark.heldMs);
				frame = mark.frame;
				break;
			case creature_fight_marks::Mark::Kind::Own:
				point = creature_skin_ray::PointOf(ownSkin, mark.spot);
				break;
			case creature_fight_marks::Mark::Kind::Ground:
				point = creature_fight_marks::DrawnOnGround(mark.world, creature_fight_marks::WorldOf(mark.local, at, heading),
				                                            turnFraction);
				break;
			}
			if (!point.has_value())
			{
				continue;
			}
			_sparkles.push_back({.position = creature_fight_marks::TowardsCamera(*point, camera),
			                     .halfSize = size,
			                     .argb = creature_fight_marks::Colour(i),
			                     .frame = frame});
		}
	}
}

void CreatureFightSystem::GroundPress(entt::entity creature, const glm::vec3& point, bool replace)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!registry.Valid(creature) || !registry.AllOf<Transform, CreatureLocomotion>(creature))
	{
		return;
	}
	const auto& at = registry.Get<const Transform>(creature).position;
	const auto heading = registry.Get<const CreatureLocomotion>(creature).heading;
	QueueMove(creature, fight::StepMove(fight::GroundStep(Flat(point), Flat(at), heading)), replace);
}

bool CreatureFightSystem::IsBlocking(entt::entity creature) const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* fighting = registry.Valid(creature) ? registry.TryGet<const CreatureFighting>(creature) : nullptr;
	return fighting != nullptr && fight::IsBlocking(fighting->fighter.state, fighting->fighter.timeMs,
	                                                DurationOf(creature, fight::animations::k_StartBlock));
}

void CreatureFightSystem::Recoil(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || std::as_const(registry).TryGet<const CreatureFighting>(creature) == nullptr)
	{
		return;
	}
	auto& fighter = registry.Get<CreatureFighting>(creature).fighter;
	// Reeling already from a blow, recoiling in its block or fainting, it takes no more notice
	using namespace fight::animations;
	const bool reeling = fighter.animation >= k_RecoilHigh && fighter.animation < k_WobbleHighSide;
	if (reeling || fighter.state == fight::State::BlockRecoil || fighter.state == fight::State::Faint)
	{
		return;
	}
	if (IsBlocking(creature))
	{
		fight::Enter(fighter, fight::State::BlockRecoil);
	}
	else if (fighter.state == fight::State::Stance || fighter.state == fight::State::Action ||
	         fighter.state == fight::State::CastStart || fighter.state == fight::State::Cast ||
	         fighter.state == fight::State::CastEnd)
	{
		fight::Enter(fighter, fight::State::Action, k_RecoilMid);
	}
}

void CreatureFightSystem::TakeMiracle(entt::entity creature, float heal, float harm)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = registry.Valid(creature) ? registry.TryGet<CreatureFighting>(creature) : nullptr;
	if (fighting == nullptr)
	{
		return;
	}
	const auto taken = fight::TakeMiracle(fighting->fighter.health, heal, harm);
	fighting->fighter.health = taken.health;
	if (taken.knockedOut)
	{
		KnockOut(creature);
	}
}

namespace
{
/// The fight of a creature that has an opponent: from the start of its duel until its fight has ended for it
CreatureFighting* WithAnOpponent(ecs::Registry& registry, entt::entity creature)
{
	auto* fighting = registry.Valid(creature) ? registry.TryGet<CreatureFighting>(creature) : nullptr;
	return fighting != nullptr && fighting->stage == CreatureFighting::Stage::Duel && !fighting->ended ? fighting : nullptr;
}
} // namespace

void CreatureFightSystem::LoseFightHealth(entt::entity creature, float lost)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = WithAnOpponent(registry, creature);
	if (fighting == nullptr)
	{
		return;
	}
	fighting->fighter.health = fight::HealthAfterLifeLost(fighting->fighter.health, lost);
	if (fighting->fighter.health <= 0.0f)
	{
		KnockOut(creature);
	}
}

void CreatureFightSystem::GainFightHealth(entt::entity creature, float gained)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* fighting = WithAnOpponent(registry, creature))
	{
		fighting->fighter.health = fight::HealthAfterLifeGained(fighting->fighter.health, gained);
	}
}

bool CreatureFightSystem::IsCameraOnFight() const
{
	if (!Locator::camera::has_value())
	{
		return false;
	}
	const auto* watch = std::as_const(Locator::camera::value()).GetModel().GetFightWatch();
	return watch != nullptr && watch->IsWatchingFight();
}

void CreatureFightSystem::KnockOut(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature, CreatureAnimation>(creature) ||
	    registry.AllOf<CreatureKnockedOut>(creature))
	{
		return;
	}
	// In a duel, its opponent wins it
	std::optional<glm::vec3> start;
	if (auto* fighting = registry.TryGet<CreatureFighting>(creature))
	{
		if (fighting->stage == CreatureFighting::Stage::Duel && InDuel(registry, fighting->opponent))
		{
			Win(fighting->opponent, creature);
			return;
		}
		start = fighting->startPosition;
		Leave(creature);
	}
	Faint(creature, start);
}

void CreatureFightSystem::PartnerStops(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* fighting =
	    registry.Valid(creature) ? std::as_const(registry).TryGet<const CreatureFighting>(creature) : nullptr;
	if (fighting == nullptr)
	{
		return;
	}
	const auto opponent = fighting->opponent;
	auto* other = registry.Valid(opponent) ? registry.TryGet<CreatureFighting>(opponent) : nullptr;
	if (other == nullptr || other->opponent != creature || other->stage != CreatureFighting::Stage::Duel)
	{
		return;
	}
	if (const auto gain = FightStrengthGain(); gain.has_value() && Locator::creaturePhysiologySystem::has_value())
	{
		Locator::creaturePhysiologySystem::value().ModifyStrength(opponent, *gain);
	}
	EndOwnFight(opponent);
	registry.Get<CreatureFighting>(opponent).opponent = entt::null;
}

void CreatureFightSystem::FailsWhileDueling(entt::entity creature)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* fighting = registry.Valid(creature) ? registry.TryGet<const CreatureFighting>(creature) : nullptr;
	if (fighting == nullptr || fighting->stage != CreatureFighting::Stage::Duel || !registry.Valid(fighting->opponent))
	{
		return;
	}
	if (const auto gain = FightStrengthGain(); gain.has_value() && Locator::creaturePhysiologySystem::has_value())
	{
		Locator::creaturePhysiologySystem::value().ModifyStrength(creature, *gain);
	}
}

void CreatureFightSystem::ForceFaint(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	if (!registry.Valid(creature) || !lookup.AllOf<Creature, CreatureAnimation>(creature) ||
	    lookup.AllOf<CreatureKnockedOut>(creature))
	{
		return;
	}
	std::optional<glm::vec3> start;
	if (const auto* fighting = lookup.TryGet<const CreatureFighting>(creature))
	{
		start = fighting->startPosition;
		// Its fight is given up as any action is: it gains a fight's strength, and so does the one it was fighting, whose
		// fight ends the same turn, first, with no winner
		const auto gain = FightStrengthGain();
		if (gain.has_value() && Locator::creaturePhysiologySystem::has_value())
		{
			Locator::creaturePhysiologySystem::value().ModifyStrength(creature, *gain);
		}
		PartnerStops(creature);
		EndOwnFight(creature);
		Leave(creature);
	}
	Faint(creature, start);
}

void CreatureFightSystem::Faint(entt::entity creature, std::optional<glm::vec3> start, bool forcedDeath)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().Stop(creature);
	}
	if (Locator::creatureObjectActionSystem::has_value())
	{
		Locator::creatureObjectActionSystem::value().Cancel(creature);
		Locator::creatureObjectActionSystem::value().Drop(creature);
	}
	// Fainting drops the leash; a Die action forced on it leaves the leash alone
	if (!forcedDeath && Locator::leashSystem::has_value() && Locator::leashSystem::value().IsLeashed(creature))
	{
		Locator::leashSystem::value().TakeOff(creature);
	}
	// Its Die action's steps, laid as it is forced, say it is being taken home; a faint then says it has fainted, a Die
	// action forced on it by a script does not
	if (Locator::creatureHelpSystem::has_value())
	{
		auto& help = Locator::creatureHelpSystem::value();
		help.Raise(creature, {.kind = creature_help::Kind::MiscellaneousStacked, .parameter = k_TransportedHomeHelp});
		if (!forcedDeath)
		{
			help.Raise(creature, {.kind = creature_help::Kind::MiscellaneousStacked, .parameter = k_HasFaintedHelp});
		}
	}
	auto& animation = registry.Get<CreatureAnimation>(creature);
	// The side it falls to is drawn even when it already lies or falls, and then it does not fall again
	const bool mirrored = game_random::GameRand(2) != 0;
	const auto* lying = std::as_const(registry).TryGet<const CreatureKnockedOut>(creature);
	if (lying == nullptr || lying->stage == CreatureKnockedOut::Stage::GettingUp)
	{
		animation.body = {
		    .kind = creature_layers::BodyAction::Kind::Sequence,
		    .phase = creature_layers::BodyAction::Phase::Start,
		    .animations = {fight::animations::k_Faint, fight::animations::k_Faint, fight::animations::k_GetUp},
		    .mirrored = mirrored,
		    .holdLoop = true,
		};
	}
	animation.face = creature_layers::RelaxFace(animation.face);
	SetEyes(registry, creature, creature_eyes::Mode::Closed);
	if (auto* needs = registry.TryGet<CreatureNeeds>(creature))
	{
		needs->rest = CreatureNeeds::Rest::Unconscious;
	}
	// The spells on it are brought to their end; a forced Die action brings them to their end only at home
	if (!forcedDeath && std::as_const(registry).TryGet<const CreatureSpells>(creature) != nullptr)
	{
		creature_spells::TryFinishAll(registry.Get<CreatureSpells>(creature).spells);
	}
	// It is taken to its pen, or else back to where it stood as its fight started; passing out outside a fight with no
	// temple for a pen, to the middle of the land
	const auto here = registry.Get<const Transform>(creature).position;
	const auto fallback = start.has_value() ? start : NoPen();
	auto& knockedOut = registry.AssignOrReplaceState<CreatureKnockedOut>(
	    creature,
	    CreatureKnockedOut {.home = ecs::creature_home::HomeOf(registry, creature).value_or(fallback.value_or(here))});
	// Every faint is its Die action: it lies the turns of the size it is drawn at, counted from the turn after it is
	// first seen lying, and those near it react as to a death
	knockedOut.lieTurns =
	    game_clock::TicksForSeconds(fight::FaintSeconds(ecs::creature_pose::DrawnSize(std::as_const(registry), creature)));
	// A faint lays its Die action's steps at once, so whether it rests once home goes by how it is as it faints (a Die
	// action forced by a script decides it in ForceDie)
	if (!forcedDeath)
	{
		const auto* now = std::as_const(registry).TryGet<const CreatureNeeds>(creature);
		knockedOut.rests = now != nullptr && fight::NeedsRest(now->needs.life, now->needs.exhaustion);
	}
	// no player known for the death
	if (Locator::reactionSystem::has_value())
	{
		static_cast<void>(
		    ecs::effects::reactions::CreateReaction(creature, openblack::Reaction::ReactToDeath, std::nullopt, false));
	}
}

void CreatureFightSystem::ForceDie(entt::entity creature, entt::entity actedOn)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	if (!registry.Valid(creature) || !lookup.AllOf<Creature, CreatureAnimation>(creature))
	{
		return;
	}
	// As its steps are laid: whether it rests at home goes by how it is now (how long it lies and the help that it is
	// being taken home are the faint's)
	const auto* needs = lookup.TryGet<const CreatureNeeds>(creature);
	const bool rests = needs != nullptr && fight::NeedsRest(needs->needs.life, needs->needs.exhaustion);
	// A fight it is in is left, not lost; a knock-out under way starts again from its faint, back in full sight at once
	// if it was being carried home
	std::optional<glm::vec3> start;
	if (const auto* fighting = lookup.TryGet<const CreatureFighting>(creature))
	{
		start = fighting->startPosition;
		Leave(creature);
	}
	if (const auto* knockedOut = lookup.TryGet<const CreatureKnockedOut>(creature))
	{
		StopCarryingHome(creature, *knockedOut);
	}
	Faint(creature, start, true);
	auto& knockedOut = registry.Get<CreatureKnockedOut>(creature);
	knockedOut.rests = rests;
	knockedOut.actedOn = actedOn;
}

void CreatureFightSystem::KillPermanently(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return;
	}
	if (registry.AllOf<CreatureFighting>(creature))
	{
		AbortFight(creature);
		Leave(creature);
	}
	KnockOut(creature);
	if (auto* knockedOut = registry.TryGet<CreatureKnockedOut>(creature))
	{
		StopCarryingHome(creature, *knockedOut);
		knockedOut->permanent = true;
		knockedOut->stage = CreatureKnockedOut::Stage::Lying;
	}
	if (auto* needs = registry.TryGet<CreatureNeeds>(creature))
	{
		needs->needs.life = 0.0f;
	}
}

void CreatureFightSystem::Resurrect(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* knockedOut = registry.Valid(creature) ? registry.TryGet<CreatureKnockedOut>(creature) : nullptr;
	if (knockedOut == nullptr)
	{
		return;
	}
	if (auto* needs = registry.TryGet<CreatureNeeds>(creature))
	{
		needs->needs.life = fight::k_GetUpLife;
	}
	StopCarryingHome(creature, *knockedOut);
	knockedOut->permanent = false;
	knockedOut->stage = CreatureKnockedOut::Stage::GettingUp;
	knockedOut->seconds = 0.0f;
	auto& animation = registry.Get<CreatureAnimation>(creature);
	animation.body = creature_layers::EndLoop(animation.body);
	SetEyes(registry, creature, creature_eyes::Mode::Calm);
}

bool CreatureFightSystem::IsKnockedOut(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(creature) && registry.AllOf<CreatureKnockedOut>(creature);
}

std::optional<size_t> CreatureFightSystem::ActionAnimation(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return std::nullopt;
	}
	if (const auto* fighting = registry.TryGet<const CreatureFighting>(creature))
	{
		return fighting->fighter.actionAnimation;
	}
	// Knocked out, it is named by its faint until it gets up, then by getting up
	if (const auto* knockedOut = registry.TryGet<const CreatureKnockedOut>(creature))
	{
		return knockedOut->stage == CreatureKnockedOut::Stage::GettingUp ? fight::animations::k_GetUp
		                                                                 : fight::animations::k_Faint;
	}
	return std::nullopt;
}

std::optional<creature_fight_hud::Values> CreatureFightSystem::GetPanel() const
{
	// Only the fight the player's camera watches, lingering on it after its end too, while its arena is there: the
	// creature that made the arena first, then its opponent
	if (!Locator::camera::has_value())
	{
		return std::nullopt;
	}
	const auto* watch = std::as_const(Locator::camera::value()).GetModel().GetFightWatch();
	if (watch == nullptr || !watch->IsWatchingFight())
	{
		return std::nullopt;
	}
	const auto fighter = watch->GetWatchedFighter();
	const auto arena = fighter.has_value() ? ArenaOf(*fighter) : std::nullopt;
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto isCreature = [&registry](entt::entity entity) {
		return registry.Valid(entity) && registry.AllOf<Creature>(entity);
	};
	if (!arena.has_value() || !isCreature(arena->maker) || !isCreature(arena->opponent))
	{
		return std::nullopt;
	}
	creature_fight_hud::Values values;
	for (size_t i = 0; i < values.sides.size(); ++i)
	{
		const auto entity = i == 0 ? arena->maker : arena->opponent;
		auto& side = values.sides.at(i);
		side.name = creature_naming::NameOf(registry, entity);
		if (const auto* fighting = registry.TryGet<const CreatureFighting>(entity))
		{
			side.health = fighting->fighter.health;
			side.stamina = fighting->fighter.stamina;
		}
		else
		{
			side.health = 0.0f;
			side.stamina = 0.0f;
		}
	}
	return values;
}

void CreatureFightSystem::ProcessTurn()
{
	// The fight animations play on by a turn first, as they did by the frame
	StepFighters(TurnMs());
	TurnMarks();
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> records;
	std::as_const(registry).Each<const CreatureFightRecord>(
	    [&records](entt::entity entity, const CreatureFightRecord& /*record*/) { records.push_back(entity); });
	for (const auto entity : records)
	{
		registry.Get<CreatureFightRecord>(entity).secondsSinceFight += k_TurnSeconds;
	}
	StartFightsFromMinds();
	ProcessStages();
	ProcessDuels();
	ProcessKnockedOut();
}

void CreatureFightSystem::StartFightsFromMinds()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto isCreature = [&registry](entt::entity entity) {
		return registry.Valid(entity) && registry.AllOf<Creature, Transform>(entity);
	};
	// Sent with the aggression leash to act on another creature close enough, the creature fights it; on any other leash,
	// or further away, its mind looks for what to do with that creature as with any other thing
	const auto fightsOnLeash = [&registry](entt::entity entity, entt::entity target) {
		const auto* leash = registry.TryGet<const CreatureLeash>(entity);
		if (leash == nullptr || !leash->worn.has_value())
		{
			return false;
		}
		const auto distance = game_math::GetDistanceInMetres(registry.Get<const Transform>(entity).position,
		                                                     registry.Get<const Transform>(target).position);
		return creature_leash::LeashedFightsCreature(leash->worn->type, distance, DrawnHeightOf(registry, entity));
	};
	std::vector<std::pair<entt::entity, entt::entity>> wanted;
	std::vector<entt::entity> minds;
	std::as_const(registry).Each<const CreatureMindState>(
	    [&minds](entt::entity entity, const CreatureMindState& /*mind*/) { minds.push_back(entity); });
	for (const auto entity : minds)
	{
		auto& actOn = registry.Get<CreatureMindState>(entity).leash.actOn;
		for (auto it = actOn.begin(); it != actOn.end();)
		{
			const auto target = static_cast<entt::entity>(*it);
			if (target != entity && isCreature(target) && registry.AllOf<Transform>(entity) && fightsOnLeash(entity, target))
			{
				wanted.emplace_back(entity, target);
				it = actOn.erase(it);
				// A fight forced on a creature, like any forced action, makes every free creature agree to joint
				// actions from now on
				registry.Context().partnersAlwaysAgree = true;
			}
			else
			{
				++it;
			}
		}
	}

	// A creature angry enough picks a fight with the nearest creature in reach
	if (_angerStartsFights)
	{
		std::as_const(registry).Each<const CreatureMindState, const Creature, const Transform>(
		    [&](entt::entity entity, const CreatureMindState& mind, const Creature& creature, const Transform& transform) {
			    if (!mind.desires.has_value() || mind.paused || registry.AnyOf<CreatureFighting, CreatureKnockedOut>(entity))
			    {
				    return;
			    }
			    const auto& anger = (*mind.desires)[creature_desires::Desire::Anger];
			    if (!anger.activated || anger.value < fight::k_AngerToFight)
			    {
				    return;
			    }
			    std::optional<entt::entity> nearest;
			    float best = std::numeric_limits<float>::max();
			    std::as_const(registry).Each<const Creature, const Transform>(
			        [&](entt::entity other, const Creature& /*body*/, const Transform& at) {
				        if (other == entity || registry.AnyOf<CreatureFighting, CreatureKnockedOut>(other))
				        {
					        return;
				        }
				        const auto distance = glm::distance(Flat(at.position), Flat(transform.position));
				        if (distance < best)
				        {
					        best = distance;
					        nearest = other;
				        }
			        });
			    const auto* record = registry.TryGet<const CreatureFightRecord>(entity);
			    const auto since = record != nullptr ? record->secondsSinceFight : fight::k_SecondsBetweenFights;
			    if (nearest.has_value() &&
			        fight::WantsToFight(anger.value, LifeOf(registry, entity), best, creature.size, since))
			    {
				    wanted.emplace_back(entity, *nearest);
			    }
		    });
	}
	for (const auto& [creature, opponent] : wanted)
	{
		StartFight(creature, opponent);
	}
}

void CreatureFightSystem::ProcessStages()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> fighters;
	std::as_const(registry).Each<const CreatureFighting>(
	    [&fighters](entt::entity entity, const CreatureFighting& /*fighting*/) { fighters.push_back(entity); });
	auto* locomotion = Locator::creatureLocomotionSystem::has_value() ? &Locator::creatureLocomotionSystem::value() : nullptr;
	const auto moving = [locomotion](entt::entity entity) { return locomotion != nullptr && locomotion->IsMoving(entity); };

	for (const auto entity : fighters)
	{
		auto* fighting = registry.TryGet<CreatureFighting>(entity);
		if (fighting == nullptr)
		{
			continue;
		}
		fighting->stageSeconds += k_TurnSeconds;
		// Squaring up to the opponent and fighting it, it shows its anger as it starts and every few seconds after
		if (fighting->stage <= CreatureFighting::Stage::Duel && Locator::creatureMindSystem::has_value())
		{
			fighting->faceSeconds -= k_TurnSeconds;
			if (fighting->faceSeconds < k_FaceDue)
			{
				Locator::creatureMindSystem::value().ShowFeeling(entity, creature_face::Cue::Anger);
				fighting->faceSeconds = creature_mind::k_FaceRepeatSeconds;
			}
		}
		const auto opponent = fighting->opponent;
		const bool opponentHere = registry.Valid(opponent) && registry.AllOf<Creature, Transform>(opponent);
		const auto& body = registry.Get<const Creature>(entity);
		auto& animation = registry.Get<CreatureAnimation>(entity);
		const auto next = [fighting](CreatureFighting::Stage stage) {
			fighting->stage = stage;
			fighting->stageSeconds = 0.0f;
			fighting->played = false;
		};

		switch (fighting->stage)
		{
		case CreatureFighting::Stage::Approach:
			if (!opponentHere || !registry.AllOf<CreatureFighting>(opponent))
			{
				Leave(entity);
				break;
			}
			if (!fighting->played)
			{
				fighting->played = true;
				if (locomotion != nullptr)
				{
					const auto spot = fight::ArenaSpot(fighting->arena, body.size, fighting->madeArena);
					locomotion->MoveTo(entity, spot, Pace::Walk, 0.0f, fight::ArrivalDistance(body.size));
				}
			}
			else if (!moving(entity) || fighting->stageSeconds > k_ApproachSeconds)
			{
				next(CreatureFighting::Stage::Taunt);
			}
			break;
		case CreatureFighting::Stage::Taunt:
			if (!opponentHere)
			{
				Leave(entity);
				break;
			}
			if (!fighting->played)
			{
				fighting->played = true;
				if (locomotion != nullptr)
				{
					locomotion->Stop(entity);
					locomotion->TurnToFace(entity, Flat(registry.Get<const Transform>(opponent).position));
				}
			}
			else if (!fighting->taunted && (!moving(entity) || fighting->stageSeconds > k_ApproachSeconds))
			{
				// The game picks between a taunt and an action past the end of its list, which plays nothing, half and half
				fighting->taunted = true;
				if (game_random::GameRand(2) != 0)
				{
					PlayOnce(animation, fight::animations::k_Taunt);
				}
			}
			else if (fighting->taunted && !creature_layers::IsPlaying(animation.body))
			{
				next(CreatureFighting::Stage::Ready);
			}
			break;
		case CreatureFighting::Stage::Ready:
		{
			const auto* other = opponentHere ? registry.TryGet<const CreatureFighting>(opponent) : nullptr;
			if (other == nullptr || other->opponent != entity)
			{
				Leave(entity);
			}
			else if (other->stage == CreatureFighting::Stage::Ready || other->stage == CreatureFighting::Stage::Duel)
			{
				// Both start together, so neither finds the other not yet duelling
				const bool otherReady = other->stage == CreatureFighting::Stage::Ready;
				BeginDuel(entity);
				if (otherReady)
				{
					BeginDuel(opponent);
				}
			}
			else if (fighting->stageSeconds > k_ApproachSeconds)
			{
				AbortFight(entity);
			}
			break;
		}
		case CreatureFighting::Stage::Duel:
			break;
		case CreatureFighting::Stage::Celebrate:
			if (fighting->fighter.state != fight::State::Idle)
			{
				break;
			}
			if (!fighting->played)
			{
				fighting->played = true;
				// Evil winners have a poo on the loser; the rest show off
				if (fight::WinnerPoos(body.alignment) && opponentHere)
				{
					next(CreatureFighting::Stage::PooOnLoser);
				}
				else
				{
					PlayOnce(animation, fight::animations::k_Impress);
				}
			}
			else if (!creature_layers::IsPlaying(animation.body))
			{
				Leave(entity);
			}
			break;
		case CreatureFighting::Stage::PooOnLoser:
			if (!fighting->played)
			{
				fighting->played = true;
				if (locomotion != nullptr && opponentHere)
				{
					locomotion->MoveToObject(entity, opponent, Pace::Walk, 0.0f);
				}
			}
			else if (!moving(entity) || fighting->stageSeconds > k_PooApproachSeconds)
			{
				Leave(entity);
				if (Locator::creatureMindSystem::has_value())
				{
					Locator::creatureMindSystem::value().Poo(entity);
				}
			}
			break;
		case CreatureFighting::Stage::Respond:
			if (fighting->fighter.state != fight::State::Idle)
			{
				break;
			}
			if (!fighting->played)
			{
				fighting->played = true;
				const auto life = LifeOf(registry, entity);
				const auto otherLife = opponentHere ? LifeOf(registry, opponent) : 0.0f;
				PlayOnce(animation, fight::Response(life, otherLife));
			}
			else if (!creature_layers::IsPlaying(animation.body))
			{
				Leave(entity);
			}
			break;
		}
	}
}

void CreatureFightSystem::BeginDuel(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& fighting = registry.Get<CreatureFighting>(creature);
	fighting.stage = CreatureFighting::Stage::Duel;
	fighting.stageSeconds = 0.0f;
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().Stop(creature);
	}
	MeasureBlows(creature);
	fight::Enter(fighting.fighter, fight::State::Start);
}

void CreatureFightSystem::MeasureBlows(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& fighting = registry.Get<CreatureFighting>(creature);
	if (fighting.measured || !Locator::creatureAnimationSystem::has_value())
	{
		return;
	}
	const auto& body = registry.Get<const Creature>(creature);
	const auto* rig = RigOf(body.species);
	if (rig == nullptr)
	{
		return;
	}
	auto& animations = Locator::creatureAnimationSystem::value();
	const auto scale = DrawnScaleOf(registry, creature);
	fighting.reaches.clear();
	for (size_t i = 0; i < fight::animations::k_AttackCount; ++i)
	{
		const auto animation = fight::animations::k_FirstAttack + i;
		const auto duration = DurationOf(creature, animation);
		if (duration <= 0.0f)
		{
			continue;
		}
		// The blow lands at the moment and with the bone the species' file gives it, where that bone is then, the body
		// carried along by its root to that moment; a blow whose moment is too early to have come from anywhere is none
		const auto& attack = rig->attackPoints.at(i);
		if (attack.hitTimeMs < k_EarliestHitMs)
		{
			continue;
		}
		const auto time = static_cast<float>(attack.hitTimeMs);
		const auto point = animations.BoneInAnimation(creature, animation, time, attack.bone, false);
		if (!point.has_value())
		{
			continue;
		}
		const auto rootAhead = -DisplacementOf(body.species, animation).y * scale;
		fighting.reaches.push_back(fight::Reach {
		    .animation = animation, .reach = (-point->z * scale) + (rootAhead * time / duration), .height = point->y * scale});
		fighting.hitTimesMs.at(i) = time;
	}
	fighting.measured = true;
}

void CreatureFightSystem::ProcessDuels()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> duelling;
	std::as_const(registry).Each<const CreatureFighting>([&duelling](entt::entity entity, const CreatureFighting& fighting) {
		if (fighting.stage == CreatureFighting::Stage::Duel)
		{
			duelling.push_back(entity);
		}
	});
	const fight::Random random = &GameRandom;
	for (const auto entity : duelling)
	{
		auto* fighting = registry.TryGet<CreatureFighting>(entity);
		if (fighting == nullptr || fighting->stage != CreatureFighting::Stage::Duel)
		{
			continue;
		}
		// Its opponent gone or out of the fight, it finishes; for the player's creature the advisors remark it went badly
		// and the player's camera is told so
		const auto opponent = fighting->opponent;
		if (!registry.Valid(opponent) || !InDuel(registry, opponent) ||
		    registry.Get<const CreatureFighting>(opponent).opponent != entity)
		{
			if (std::as_const(registry).Get<const Creature>(entity).owner == creature::LocalPlayer())
			{
				if (const auto* at = registry.TryGet<const Transform>(entity))
				{
					openblack::audio::guidance::RemarkGeneralBad(at->position);
				}
			}
			EndFightFor(entity, true);
			if (std::as_const(registry).Get<const Creature>(entity).owner == creature::LocalPlayer())
			{
				TellCameraFightIsOver();
			}
			fighting->stage = CreatureFighting::Stage::Respond;
			fighting->played = false;
			fight::Enter(fighting->fighter, fight::State::Finish);
			continue;
		}
		auto& fighter = fighting->fighter;
		fighter.stamina = fight::RegainStamina(fighter.stamina);
		const auto& other = registry.Get<const CreatureFighting>(opponent).fighter;

		// Left alone long enough, the computer takes over the player's creature
		fight::CountComputerWait(fighter, TurnMs());
		const bool computer = fighter.autoFight || fighter.control == fight::Control::Computer;
		// The arena's keep-in rule: with either of the two at or past its edge fraction, a block ends and from its
		// stance it steps back towards the middle, and it takes no move from the queue and chooses none this turn
		if ((fighter.state == fight::State::Stance || fighter.state == fight::State::Block) &&
		    !fight::BothKeptIn(fighting->arena, Flat(registry.Get<const Transform>(entity).position),
		                       Flat(registry.Get<const Transform>(opponent).position)))
		{
			if (fighter.state == fight::State::Block)
			{
				fight::Enter(fighter, fight::State::BlockEnd);
			}
			else if (const auto* locomotion = registry.TryGet<const CreatureLocomotion>(entity))
			{
				const glm::vec3 middle {fighting->arena.centre.x, GroundAt(fighting->arena.centre), fighting->arena.centre.y};
				if (const auto step = fight::KeepInStep(middle, registry.Get<const Transform>(entity).position,
				                                        locomotion->heading, ecs::object::Get2DRadius(entity)))
				{
					// A step the rule makes is taken wherever the creature stands
					fight::Enter(fighter, fight::State::Action, fight::StepAnimation(*step));
				}
			}
			continue;
		}
		// It faced its opponent outright as its turn began, so it makes its moves whatever way it faces now
		if (!computer)
		{
			// The computer does not look at the queue: it chooses its own moves
			if (!fighter.queue.Empty())
			{
				CheckQueue(entity);
			}
		}
		else if (fighter.state == fight::State::Stance)
		{
			const auto choice =
			    fight::ChooseMove(fight::TierOf(fighter.tendency), fight::OpponentOf(other.state, other.animation), random);
			switch (choice.kind)
			{
			case fight::Choice::Kind::Blow:
				AttemptBlow(entity, choice.band, fight::k_AiBlowSpeed);
				break;
			case fight::Choice::Kind::Block:
				fight::Enter(fighter, fight::State::BlockStart);
				break;
			case fight::Choice::Kind::Step:
				if (fight::CanStep(fighting->arena, Flat(registry.Get<const Transform>(entity).position), choice.step))
				{
					fight::Enter(fighter, fight::State::Action, fight::StepAnimation(choice.step));
				}
				break;
			case fight::Choice::Kind::None:
				break;
			}
		}
		else if (fighter.state == fight::State::Block && fight::ComputerEndsBlock(random))
		{
			fight::Enter(fighter, fight::State::BlockEnd);
		}
	}
}

void CreatureFightSystem::CheckQueue(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& fighting = registry.Get<CreatureFighting>(creature);
	if (fighting.stage != CreatureFighting::Stage::Duel || !registry.Valid(fighting.opponent))
	{
		if (const auto logger = spdlog::get("game"); logger != nullptr && fighting.fighter.queue.Front().has_value())
		{
			logger->trace("Fight: creature {} holds its queued move: its fight is not a duel yet (stage {})",
			              entt::to_integral(creature), static_cast<int>(fighting.stage));
		}
		return;
	}
	const auto opponentAt = Flat(registry.Get<const Transform>(fighting.opponent).position);
	auto& fighter = fighting.fighter;
	const bool inRange = fight::WithinRange(fighting.arena, opponentAt);
	// a trace at run time (the game channel at trace level), not compiled out of release builds
	if (const auto logger = spdlog::get("game"); logger != nullptr && !inRange && fighter.queue.Front().has_value())
	{
		logger->trace("Fight: creature {} holds its queued move: its opponent at ({:.1f}, {:.1f}) is {:.1f} from the arena's "
		              "centre ({:.1f}, {:.1f}), radius {:.1f}",
		              entt::to_integral(creature), opponentAt.x, opponentAt.y, glm::distance(opponentAt, fighting.arena.centre),
		              fighting.arena.centre.x, fighting.arena.centre.y, fighting.arena.radius);
	}
	// The help record counts the moves of the local player's creature
	if (const auto front = fighter.queue.Front();
	    fight::CountsHelpEvent(fighter.state, inRange, front.has_value()) && Locator::helpProfileSystem::has_value() &&
	    Locator::playerSystem::has_value() &&
	    registry.Get<const Creature>(creature).owner == Locator::playerSystem::value().GetLocalPlayer())
	{
		Locator::helpProfileSystem::value().Trigger(fight::HelpEventOf(front->move.kind));
	}
	// Each look at the front of the queue from the stance teaches the creature to lean, whoever queued the move, a blow
	// still waiting to be let go too
	if (const auto front = fighter.queue.Front(); fight::CountsHelpEvent(fighter.state, inRange, front.has_value()))
	{
		const bool local = std::as_const(registry).Get<const Creature>(creature).owner == creature::LocalPlayer();
		if (auto* record = registry.TryGet<CreatureFightRecord>(creature);
		    record != nullptr && fight::LearnsFromMove(front->move.kind, local))
		{
			record->tendency = fight::LearnTendency(record->tendency, front->move.kind);
		}
	}
	const auto waiting = fighter.queue.Size();
	const auto order = fight::TakeOrder(fighter, inRange);
	// the move taken from the front of the queue takes its press's mark with it
	if (fighter.queue.Size() < waiting)
	{
		fighting.marks.Shunt();
	}
	if (!order.has_value())
	{
		return;
	}
	if (const auto logger = spdlog::get("game"))
	{
		logger->trace("Fight: creature {} takes its queued move (order {})", entt::to_integral(creature),
		              static_cast<int>(order->kind));
	}
	switch (order->kind)
	{
	case fight::Order::Kind::EndBlock:
		fight::Enter(fighter, fight::State::BlockEnd);
		break;
	case fight::Order::Kind::Animation:
		if (const auto step = fight::StepOf(order->value))
		{
			if (fight::CanStep(fighting.arena, Flat(registry.Get<const Transform>(creature).position), *step))
			{
				fight::Enter(fighter, fight::State::Action, order->value);
			}
		}
		else if (Has(creature, order->value))
		{
			fight::Enter(fighter, fight::State::Action, order->value);
		}
		break;
	case fight::Order::Kind::Cast:
		// The spell is cast as the cast's start ends
		fight::Enter(fighter, fight::State::CastStart);
		fighter.spell = order->value;
		break;
	case fight::Order::Kind::Block:
		fight::Enter(fighter, fight::State::BlockStart);
		break;
	case fight::Order::Kind::Special:
		fight::Enter(fighter, fight::State::Action, fight::animations::k_AttackSpecial, order->speed);
		break;
	case fight::Order::Kind::Blow:
		// The queued blow leaves the queue only once it is thrown
		if (AttemptBlow(creature, order->band, order->speed))
		{
			fighter.queue.Pop();
			fighting.marks.Shunt();
		}
		break;
	}
}

bool CreatureFightSystem::AttemptBlow(entt::entity creature, fight::Band band, float speed)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& fighting = registry.Get<CreatureFighting>(creature);
	MeasureBlows(creature);
	const auto& body = registry.Get<const Creature>(creature);
	const auto opponent = fighting.opponent;
	const auto selfAt = Flat(registry.Get<const Transform>(creature).position);
	const auto opponentAt = Flat(registry.Get<const Transform>(opponent).position);
	const auto* opponentMoves = registry.TryGet<const CreatureLocomotion>(opponent);
	const auto opponentRadius = opponentMoves != nullptr ? opponentMoves->radius : 5.0f;
	const auto distance = glm::distance(selfAt, opponentAt);
	const auto gap = distance - opponentRadius;
	const auto scale = DrawnScaleOf(registry, creature);
	const auto stepLength = glm::length(DisplacementOf(body.species, fight::animations::k_StepForward)) * scale;
	const auto opponentHeight = DrawnHeightOf(registry, opponent);
	// Each blow is measured against the opponent's body as it is drawn: rays from the attacker towards it at the blow's
	// height, spread across 0.15 of its height to each side, the nearest of them meeting it. A blow none of them meets is
	// not used. Without a drawn body, the opponent's walking radius stands for it
	const auto capsules = BodyThisTurn(registry, opponent);
	std::vector<std::optional<float>> gaps(fighting.reaches.size(), gap);
	if (!capsules.empty() && distance > 0.0f)
	{
		const auto toward = (opponentAt - selfAt) / distance;
		const glm::vec3 direction {toward.x, 0.0f, toward.y};
		const glm::vec3 side {-toward.y, 0.0f, toward.x};
		const auto halfWidth = k_BlowSweepShare * opponentHeight;
		const auto ground = registry.Get<const Transform>(creature).position.y;
		for (size_t i = 0; i < fighting.reaches.size(); ++i)
		{
			std::optional<float> nearest;
			const glm::vec3 origin {selfAt.x, ground + fighting.reaches[i].height, selfAt.y};
			for (int sample = 0; sample < k_BlowSweepSamples; ++sample)
			{
				const auto across =
				    -halfWidth + (2.0f * halfWidth * static_cast<float>(sample) / static_cast<float>(k_BlowSweepSamples - 1));
				const auto entry =
				    feedback::EntryAlong(origin + (side * across), direction, capsules, distance + opponentHeight);
				if (entry.has_value() && (!nearest.has_value() || *entry < *nearest))
				{
					nearest = entry;
				}
			}
			gaps[i] = nearest;
		}
	}
	const auto choice = fight::ChooseAttack(fighting.reaches, band, gaps, stepLength,
	                                        ecs::creature_pose::DrawnSize(registry, creature), opponentHeight);
	// a trace at run time (the game channel at trace level), not compiled out of release builds
	if (const auto logger = spdlog::get("game"))
	{
		std::optional<float> nearest;
		for (const auto& g : gaps)
		{
			if (g.has_value() && (!nearest.has_value() || *g < *nearest))
			{
				nearest = g;
			}
		}
		logger->trace("Fight: creature {} tries a blow: walking gap {:.2f}, nearest body gap {:.2f}, step {:.2f}, {} reaches, "
		              "size {:.2f}, opponent height {:.2f} -> blow {}, step {}",
		              entt::to_integral(creature), gap, nearest.value_or(-1.0f), stepLength, fighting.reaches.size(),
		              ecs::creature_pose::DrawnSize(registry, creature), opponentHeight,
		              choice.animation.has_value() ? static_cast<int>(*choice.animation) : -1,
		              choice.step.has_value() ? static_cast<int>(*choice.step) : -1);
	}
	if (choice.animation.has_value())
	{
		fight::Enter(fighting.fighter, fight::State::Action, *choice.animation, speed);
		return true;
	}
	if (choice.step.has_value() && fight::CanStep(fighting.arena, selfAt, *choice.step))
	{
		fight::Enter(fighting.fighter, fight::State::Action, fight::StepAnimation(*choice.step));
	}
	return false;
}

void CreatureFightSystem::Update(float turnFraction, float /*frameMs*/)
{
	auto& registry = Locator::entitiesRegistry::value();
	// The body is drawn between where the fighter's animation was as the turn started and where it is at its end
	const auto f = std::clamp(turnFraction, 0.0f, 1.0f);
	std::vector<entt::entity> fighters;
	std::as_const(registry).Each<const CreatureFighting, const CreatureAnimation>(
	    [&fighters](entt::entity entity, const CreatureFighting& fighting, const CreatureAnimation& /*animation*/) {
		    if (fighting.fighter.state != fight::State::Idle)
		    {
			    fighters.push_back(entity);
		    }
	    });
	for (const auto entity : fighters)
	{
		const auto& fighting = std::as_const(registry).Get<CreatureFighting>(entity);
		auto& animation = registry.Get<CreatureAnimation>(entity);
		animation.body = BodyAt(fighting, DurationOf(entity, fighting.fighter.animation), f);
		animation.slots.clear();
	}
	UpdateRings();
	UpdateSparkles();
}

void CreatureFightSystem::UpdateRings()
{
	namespace ring = creature_arena_ring;
	const auto arenas = GetArenas();
	// The arenas no longer in use let their rings go, which the particles then stop drawing
	std::erase_if(_rings, [&arenas](const auto& kept) {
		return std::ranges::none_of(arenas, [&kept](const ArenaView& arena) { return arena.serial == kept.first; });
	});
	if (!Locator::particleSystem::has_value())
	{
		return;
	}
	for (const auto& arena : arenas)
	{
		if (_rings.contains(arena.serial))
		{
			continue;
		}
		// Each point on the land under it, as the land's map position there finds it
		std::vector<glm::vec3> points;
		for (const auto& point : ring::Points(arena.centre, arena.radius))
		{
			const glm::vec2 cell {map_coords::ToMetres(map_coords::MetresToFixedForHandLookup(point.x)),
			                      map_coords::ToMetres(map_coords::MetresToFixedForHandLookup(point.y))};
			points.emplace_back(point.x, GroundAt(cell), point.y);
		}
		auto sheet = std::make_shared<particles::LightSheet>();
		sheet->Start(std::move(points), ring::k_Rgb, ring::Height(arena.radius), ring::k_ShiftSeconds);
		sheet->SetSpread(ring::k_Spread);
		sheet->SetStrength(ring::k_Strength);
		// its memory starts cleared, so until it is first built it sorts at the world's origin
		sheet->SetUnbuiltMiddle(glm::vec3(0.0f));
		Locator::particleSystem::value().AddLightSheet(sheet);
		_rings.emplace(arena.serial, std::move(sheet));
	}
}

void CreatureFightSystem::AnimationAt(entt::entity creature, float turnFraction, CreatureAnimationInputs& inputs) const
{
	const auto& lookup = std::as_const(Locator::entitiesRegistry::value());
	const auto* fighting = lookup.TryGet<const CreatureFighting>(creature);
	if (fighting == nullptr || !lookup.AllOf<CreatureAnimation>(creature) || fighting->fighter.state == fight::State::Idle)
	{
		return;
	}
	inputs.body = BodyAt(*fighting, DurationOf(creature, fighting->fighter.animation), std::clamp(turnFraction, 0.0f, 1.0f));
	inputs.slots.clear();
}

creature_layers::BodyAction CreatureFightSystem::BodyAt(const CreatureFighting& fighting, float durationMs, float share)
{
	const auto& fighter = fighting.fighter;
	return ShownBody(fighter, fighter.queue.AwaitsRelease(), durationMs,
	                 fight::DrawnTime(fighting.drawFromMs, fighter.timeMs, share));
}

void CreatureFightSystem::StepFighters(float milliseconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> fighters;
	std::as_const(registry).Each<const CreatureFighting>([&fighters](entt::entity entity, const CreatureFighting& fighting) {
		if (fighting.fighter.state != fight::State::Idle)
		{
			fighters.push_back(entity);
		}
	});
	bool moved = false;
	for (const auto entity : fighters)
	{
		auto* fighting = registry.TryGet<CreatureFighting>(entity);
		auto* locomotion = registry.TryGet<CreatureLocomotion>(entity);
		auto* animation = registry.TryGet<CreatureAnimation>(entity);
		if (fighting == nullptr || locomotion == nullptr || animation == nullptr ||
		    fighting->fighter.state == fight::State::Idle)
		{
			// Standing again, a forced finish's pace is over
			if (fighting != nullptr)
			{
				fighting->fighter.hurried = false;
			}
			continue;
		}
		auto& fighter = fighting->fighter;
		const auto& body = registry.Get<const Creature>(entity);
		const bool hasOpponent = fighting->stage == CreatureFighting::Stage::Duel && InDuel(registry, fighting->opponent);
		const auto duration = DurationOf(entity, fighter.animation);
		const auto before = fighter.timeMs;
		fighting->drawFromMs = before;
		fighter.timeMs = fight::AdvanceClock(fighter.timeMs, milliseconds * (fighter.hurried ? fight::k_HurriedPace : 1.0f),
		                                     fighter.speed, fighter.animation);

		// It faces its opponent where the opponent is now. Right on top of it, it faces heading 0
		const auto faceOpponent = [&]() {
			const auto towards =
			    Flat(registry.Get<const Transform>(fighting->opponent).position) - Flat(locomotion->toPosition);
			FaceHeading(*locomotion, creature_locomotion::HeadingFromDirection(towards));
			moved = true;
		};
		// With both within the range and its opponent swinging no blow, the two are pulled apart along the way it faces
		// when they stand nearer than both their reaches ahead; each is moved only if it stays within the range
		const auto pullApart = [&]() {
			auto* otherMoves = registry.TryGet<CreatureLocomotion>(fighting->opponent);
			const auto* otherFighting = registry.TryGet<const CreatureFighting>(fighting->opponent);
			if (otherMoves != nullptr && otherFighting != nullptr && !fight::IsAttack(otherFighting->fighter.animation) &&
			    fight::WithinRange(fighting->arena, Flat(locomotion->toPosition)) &&
			    fight::WithinRange(fighting->arena, Flat(otherMoves->toPosition)))
			{
				const auto push = fight::PullApartOf(Flat(locomotion->toPosition), Flat(otherMoves->toPosition),
				                                     creature_locomotion::DirectionOf(locomotion->heading),
				                                     ForwardExtentOf(registry, entity, fighter),
				                                     ForwardExtentOf(registry, fighting->opponent, otherFighting->fighter));
				if (push.has_value())
				{
					if (fight::WithinRange(fighting->arena, Flat(locomotion->toPosition) - *push))
					{
						Shift(*locomotion, -*push);
					}
					if (fight::WithinRange(fighting->arena, Flat(otherMoves->toPosition) + *push))
					{
						Shift(*otherMoves, *push);
						ecs::creature_pose::CommitTurnPose(fighting->opponent, otherMoves->toPosition, otherMoves->toHeading);
					}
					moved = true;
				}
			}
		};
		// In its start, its stance and its block it faces before anything moves it this turn, and in its stance it is
		// then pulled apart from its opponent
		const bool facesFirst =
		    fight::FacesOpponentEachTurn(fighter.state, fighter.animation) && fighter.state != fight::State::Action;
		if (hasOpponent && facesFirst && registry.Valid(fighting->opponent))
		{
			faceOpponent();
		}
		if (hasOpponent && fighter.state == fight::State::Stance)
		{
			pullApart();
		}

		// An action carries the creature as far as its animation's root moves, wherever that takes it; a block's recoil
		// moves it only to a place inside the range
		if ((fighter.state == fight::State::Action || fighter.state == fight::State::BlockRecoil) && duration > 0.0f)
		{
			const auto share = std::min(fighter.timeMs / duration, 1.0f);
			const auto scale = DrawnScaleOf(registry, entity);
			auto by = MeshToWorld(DisplacementOf(body.species, fighter.animation) * scale, locomotion->heading) *
			          (share - fighting->movedShare);
			fighting->movedShare = share;
			const auto at = Flat(locomotion->toPosition);
			const auto to = fight::ActionMoveTo(fighting->arena, fighter.state, at, by);
			// Carried all the way: the stance's pull-apart and a blow's push keep the two apart
			by = to - at;
			if (glm::length(by) > 0.0f)
			{
				Shift(*locomotion, by);
				moved = true;
			}
		}
		// In a step or a reel, once carried, it faces its opponent and the two are pulled apart
		if (hasOpponent && fighter.state == fight::State::Action &&
		    fight::FacesOpponentEachTurn(fighter.state, fighter.animation) && registry.Valid(fighting->opponent))
		{
			faceOpponent();
			pullApart();
		}
		// A blow is tested each turn near its moment, also after it landed (the victim reeling takes no other), while the
		// duel is on: a fight called off lands no more blows
		if (hasOpponent && fighter.state == fight::State::Action && fight::IsAttack(fighter.animation))
		{
			TestHit(entity, before, milliseconds);
			// The hit may have ended the fight
			fighting = registry.TryGet<CreatureFighting>(entity);
			if (fighting == nullptr)
			{
				continue;
			}
		}
		// Each turn of a blow, landed or not, the two are pushed apart along the way the attacker faces when they stand
		// nearer than its body's reach ahead and half the opponent's
		if (hasOpponent && fighting->fighter.state == fight::State::Action && fight::IsAttack(fighting->fighter.animation))
		{
			auto* moves = registry.TryGet<CreatureLocomotion>(entity);
			auto* otherMoves = registry.TryGet<CreatureLocomotion>(fighting->opponent);
			const auto* otherFighting = registry.TryGet<const CreatureFighting>(fighting->opponent);
			if (moves != nullptr && otherMoves != nullptr && otherFighting != nullptr)
			{
				const auto push = fight::BlowPushOf(Flat(moves->toPosition), Flat(otherMoves->toPosition),
				                                    creature_locomotion::DirectionOf(moves->heading),
				                                    ForwardExtentOf(registry, entity, fighting->fighter),
				                                    ForwardExtentOf(registry, fighting->opponent, otherFighting->fighter));
				if (push.has_value())
				{
					Shift(*moves, -*push);
					Shift(*otherMoves, *push);
					ecs::creature_pose::CommitTurnPose(fighting->opponent, otherMoves->toPosition, otherMoves->toHeading);
					moved = true;
				}
			}
			locomotion = moves != nullptr ? moves : locomotion;
		}
		auto& current = fighting->fighter;
		// Called off, it winds down: the move under way plays out, then it finishes and stands; a cast started is never
		// cast
		if (current.stopping)
		{
			if (const auto next = fight::StopTransition(current.state, duration <= 0.0f || current.timeMs >= duration))
			{
				if (current.state == fight::State::Cast && Locator::magicSystem::has_value())
				{
					Locator::magicSystem::value().ReleaseCreatureCast(entity);
				}
				current.spell.reset();
				fight::Enter(current, *next);
				fighting->movedShare = 0.0f;
				fighting->drawFromMs = 0.0f;
				Show(*animation, current, false, DurationOf(entity, current.animation), current.timeMs);
				ecs::creature_pose::CommitTurnPose(entity, locomotion->toPosition, locomotion->toHeading);
				continue;
			}
		}
		if (current.state == fight::State::Cast)
		{
			// The cast is held by the update, not by its animation: past the hold it lets go and ends the cast
			if (fight::HoldFightCast(current, milliseconds))
			{
				if (Locator::magicSystem::has_value())
				{
					Locator::magicSystem::value().ReleaseCreatureCast(entity);
				}
				fight::Enter(current, fight::State::CastEnd);
				fighting->movedShare = 0.0f;
			}
		}
		else if (duration <= 0.0f || current.timeMs >= duration)
		{
			const auto next = fight::AfterAnimation(current.state, hasOpponent);
			if (current.state == fight::State::CastStart && next == fight::State::Cast)
			{
				CastFightSpell(entity, *fighting, hasOpponent);
				current.castMs = 0;
			}
			if (next == current.state && fight::Loops(current.state))
			{
				current.timeMs =
				    current.state == fight::State::Lying || duration <= 0.0f ? duration : std::fmod(current.timeMs, duration);
			}
			else
			{
				fight::Enter(current, next);
				fighting->movedShare = 0.0f;
				if (next == fight::State::Idle)
				{
					animation->body = {};
					ecs::creature_pose::CommitTurnPose(entity, locomotion->toPosition, locomotion->toHeading);
					continue;
				}
			}
		}
		// A new animation, or a loop come round, is drawn from its start
		if (fighting->drawFromMs > current.timeMs)
		{
			fighting->drawFromMs = 0.0f;
		}
		Show(*animation, current, current.queue.AwaitsRelease(), DurationOf(entity, current.animation), current.timeMs);
		// The Transform moves once a turn, to where the turn ends
		ecs::creature_pose::CommitTurnPose(entity, locomotion->toPosition, locomotion->toHeading);
	}
	if (moved)
	{
		registry.SetDirty();
	}
}

void CreatureFightSystem::TestHit(entt::entity creature, float fromMs, float turnMs)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& fighting = registry.Get<CreatureFighting>(creature);
	const auto opponent = fighting.opponent;
	if (!InDuel(registry, opponent))
	{
		if (const auto logger = spdlog::get("game"))
		{
			logger->trace("Fight: creature {}'s blow is not tested: its opponent is not in the duel",
			              entt::to_integral(creature));
		}
		return;
	}
	const auto& body = registry.Get<const Creature>(creature);
	const auto& opponentTransform = registry.Get<const Transform>(opponent);
	auto& opponentFighting = registry.Get<CreatureFighting>(opponent);
	auto& victim = opponentFighting.fighter;
	auto& attacker = fighting.fighter;

	const auto blow = attacker.animation - fight::animations::k_FirstAttack;
	const auto* rig = RigOf(body.species);
	if (rig == nullptr || !rig->actionPoints.has_value() || blow >= rig->attackPoints.size() ||
	    !Locator::creatureAnimationSystem::has_value())
	{
		return;
	}
	// Sized by the bodies as they are drawn, the pen's in a temple's pen
	const auto attackerSize = ecs::creature_pose::DrawnSize(registry, creature);
	const auto defenderSize = ecs::creature_pose::DrawnSize(registry, opponent);
	const auto opponentHeight = HeightOf(defenderSize);

	// Whether the blow lands, as the game tests it: at thirty moments through a span of the animation, those near the
	// blow's moment, five lines from the attacker's groin to points round the bone the blow strikes with must meet the
	// opponent's bones between their ends; the last line that meets them gives the point. A victim that reels from a
	// blow already, recoils in its block or faints takes none.
	auto spanMs = fight::TestSpanMs(turnMs, attackerSize, false);
	if (attacker.hurried)
	{
		spanMs = static_cast<int32_t>(static_cast<float>(spanMs) * fight::k_HurriedPace);
	}
	const auto landMs = static_cast<int32_t>(fighting.hitTimesMs.at(blow));
	const auto times = fight::BlowTestTimes(static_cast<int32_t>(fromMs), spanMs, landMs);
	if (times.empty())
	{
		return;
	}
	auto& animations = Locator::creatureAnimationSystem::value();
	const auto& locomotion = registry.Get<const CreatureLocomotion>(creature);
	const auto placement = creature::PlacementMatrix(locomotion.toPosition, affine::AngleY(-locomotion.toHeading),
	                                                 ecs::creature_pose::DrawnScale(registry, creature));
	const auto worldBone = [&](uint32_t bone, int32_t timeMs) -> std::optional<glm::vec3> {
		const auto point =
		    animations.BoneInAnimation(creature, attacker.animation, static_cast<float>(timeMs), bone, attacker.mirrored);
		return point.has_value() ? std::optional(glm::vec3(placement * glm::vec4(*point, 1.0f))) : std::nullopt;
	};
	const auto strikes = rig->attackPoints.at(blow).bone;
	const auto groin = rig->actionPoints->groin;
	auto before = worldBone(strikes, static_cast<int32_t>(fromMs));
	if (!before.has_value())
	{
		return;
	}
	const bool reeling = victim.state == fight::State::Action && victim.animation >= fight::animations::k_RecoilHigh &&
	                     victim.animation < fight::animations::k_WobbleHighSide;
	const bool open = !reeling && victim.state != fight::State::BlockRecoil && victim.state != fight::State::Faint;
	std::vector<fight::BlowSample> samples;
	samples.reserve(times.size());
	for (const auto time : times)
	{
		const auto at = worldBone(strikes, time);
		// Each moment the striking bone is at puffs faintly there
		if (at.has_value() && Locator::dynamicsSystem::has_value())
		{
			Locator::dynamicsSystem::value().AddPuff(*at, glm::vec3(0.0f), fight::SwingPuffSize(attackerSize, attacker.special),
			                                         fight::k_SwingPuffColour);
		}
		samples.push_back({.strikes = at, .groin = at.has_value() ? worldBone(groin, time) : std::nullopt});
	}
	const auto landed = fight::BlowLandsAt(*before, samples, BonesOf(registry, opponent),
	                                       fight::BlowTestReach(attackerSize, attacker.special), open);
	if (!landed.has_value())
	{
		if (const auto logger = spdlog::get("game"))
		{
			logger->trace("Fight: creature {}'s blow does not land this turn", entt::to_integral(creature));
		}
		return;
	}
	// How high it lands and from where come from the blow as the creature's file measures it, not from where the line
	// met the body: where the blow reaches, ahead of the attacker and up
	const auto found = std::ranges::find_if(
	    fighting.reaches, [&attacker](const fight::Reach& reach) { return reach.animation == attacker.animation; });
	if (found == fighting.reaches.end())
	{
		return;
	}
	const auto ahead = creature_locomotion::DirectionOf(locomotion.heading);
	// The root has already carried the body part of the way the blow reaches
	const auto rootAhead = -DisplacementOf(body.species, attacker.animation).y * DrawnScaleOf(registry, creature);
	const auto blowReach = found->reach - (rootAhead * fighting.movedShare);
	const auto& from = locomotion.toPosition;
	const auto point = glm::vec3(from.x + (ahead.x * blowReach), from.y + found->height, from.z + (ahead.y * blowReach));

	const bool blocked = fight::IsBlocking(victim.state, victim.timeMs, DurationOf(opponent, fight::animations::k_StartBlock));
	const auto heightShare = std::clamp((point.y - opponentTransform.position.y) / opponentHeight, 0.0f, 1.0f);
	// Where it lands from the middle of the victim's body
	const auto* opponentMoves = registry.TryGet<const CreatureLocomotion>(opponent);
	const auto victimRadius = opponentMoves != nullptr ? std::max(opponentMoves->radius, 0.1f) : 5.0f;
	const auto local =
	    WorldToLocal(Flat(point) - Flat(opponentTransform.position), opponentMoves != nullptr ? opponentMoves->heading : 0.0f);
	const auto direction = fight::RecoilDirectionOf({local.x / victimRadius, (heightShare - 0.5f) * 2.0f});
	const auto result = fight::ResolveBlow(
	    {
	        .attackerSize = attackerSize,
	        .defenderSize = defenderSize,
	        .attackerStrength = ShownStrengthOf(registry, creature),
	        .defenderStrength = ShownStrengthOf(registry, opponent),
	        .speed = attacker.speed,
	        .special = attacker.special,
	        .heightShare = heightShare,
	        .blocked = blocked,
	    },
	    direction);
	victim.health = fight::HealthAfterBlow(victim.health, result.damage);
	// The blow is a hit from the attacker: the victim comes to find it nastier, no more than once in its creatures'
	// shared wait between such changes, which the leash's steps between two tied creatures also keep
	if (Locator::creatureMindSystem::has_value() && Locator::infoConstants::has_value())
	{
		auto& stamp = registry.AllOf<CreatureLeashAttitude>(opponent) ? registry.Get<CreatureLeashAttitude>(opponent)
		                                                              : registry.Assign<CreatureLeashAttitude>(opponent);
		if (const auto turn = game_clock::Turn(); creature_leash::AttitudeStepDue(stamp.lastStep, turn))
		{
			stamp.lastStep = turn;
			const auto weight = ecs::effects::alignment::AlignmentFactor(
			    Locator::infoConstants::value(), static_cast<size_t>(ecs::effects::EffectValues::Number::Hit),
			    AlignmentType::Creature);
			Locator::creatureMindSystem::value().ChangeHowNice(opponent, creature, fight::BlowNicenessChange(weight));
		}
	}
	// Getting hit takes away the blow added last while it waits to be let go, and makes it let go of a spell it holds;
	// the local player's creature loses the blow's mark with it
	if (victim.queue.TakeWaitingBlow() &&
	    std::as_const(registry).Get<const Creature>(opponent).owner == creature::LocalPlayer())
	{
		opponentFighting.marks.DropWaiting();
	}
	if (Locator::magicSystem::has_value())
	{
		Locator::magicSystem::value().ReleaseCreatureCast(opponent);
	}

	auto& victimAnimation = registry.Get<CreatureAnimation>(opponent);
	victimAnimation.wobble = {
	    .animation = fight::WobbleAnimation(result.band, direction == fight::RecoilDirection::Right ||
	                                                         direction == fight::RecoilDirection::Left),
	    .timeMs = 0.0f,
	};
	// An unblocked blow wounds the skin, and some wounds bleed. Where on the skin isn't found from the body yet.
	if (!blocked && Locator::creatureSkinSystem::has_value())
	{
		const fight::Random random = &GameRandom;
		auto& skins = Locator::creatureSkinSystem::value();
		// Drawn one at a time, in this order: where on the skin, u then v, then the kind, then the column
		constexpr auto k_TexelRange = creature_marks::k_AtlasSize - (2 * k_WoundEdge);
		const auto u = static_cast<uint8_t>(k_WoundEdge + game_random::GameRand(k_TexelRange));
		const auto v = static_cast<uint8_t>(k_WoundEdge + game_random::GameRand(k_TexelRange));
		const auto type = fight::WoundKind(fight::BlowWoundType(attacker.animation), random);
		const auto column = static_cast<uint8_t>(game_random::GameRand(creature_marks::k_CellsPerRow));
		creature_marks::Mark wound {.u = u, .v = v, .skin = 0, .age = 0, .type = type, .column = column};
		skins.AddWound(opponent, wound);
		if (fight::Bleeds(wound.type))
		{
			auto drop = wound;
			drop.type = 0;
			drop.column = 0;
			for (int i = 0; i < k_BloodDrops && drop.v < 255; ++i, ++drop.v)
			{
				skins.AddBlood(opponent, drop);
			}
		}
	}

	if (victim.health <= 0.0f)
	{
		Win(creature, opponent);
		return;
	}
	if (blocked)
	{
		fight::Enter(victim, fight::State::BlockRecoil);
		opponentFighting.movedShare = 0.0f;
	}
	else if (victim.state == fight::State::Stance || victim.state == fight::State::Action ||
	         victim.state == fight::State::CastStart || victim.state == fight::State::Cast ||
	         victim.state == fight::State::CastEnd)
	{
		fight::Enter(victim, fight::State::Action, Has(opponent, result.recoil) ? result.recoil : result.plainRecoil);
		opponentFighting.movedShare = 0.0f;
	}
}

void CreatureFightSystem::Win(entt::entity winner, entt::entity loser)
{
	auto& registry = Locator::entitiesRegistry::value();
	// Its opponent knocked out, the local player's creature, unless a script controls it, has the advisors remark it
	// went well
	if (std::as_const(registry).Get<const Creature>(winner).owner == creature::LocalPlayer() &&
	    !ecs::script_held::IsControlledByScript(winner))
	{
		if (const auto* at = registry.TryGet<const Transform>(winner))
		{
			openblack::audio::guidance::RemarkGeneralGood(at->position);
		}
	}
	// a creature of a player (the neutral one too) impresses the villagers who see it win, for its player. Without the
	// reactions (a test's world) none is made
	if (const auto owner = std::as_const(registry).Get<const Creature>(winner).owner;
	    owner != PlayerNames::_COUNT && Locator::reactionSystem::has_value())
	{
		static_cast<void>(ecs::effects::reactions::CreateReaction(winner, openblack::Reaction::ReactToFightWon, owner, true));
	}
	EndFightFor(winner, true);
	EndFightFor(loser, false);
	if (auto* record = registry.TryGet<CreatureFightRecord>(winner))
	{
		++record->wins;
	}
	const auto* lost = registry.TryGet<const CreatureFighting>(loser);
	const auto start = lost != nullptr ? std::optional(lost->startPosition) : std::nullopt;
	Leave(loser);
	Faint(loser, start);
	if (auto* fighting = registry.TryGet<CreatureFighting>(winner))
	{
		fighting->stage = CreatureFighting::Stage::Celebrate;
		fighting->stageSeconds = 0.0f;
		fighting->played = false;
		fight::Enter(fighting->fighter, fight::State::Finish);
		auto& won = registry.Get<CreatureAnimation>(winner);
		won.face = creature_layers::RelaxFace(won.face);
	}
}

void CreatureFightSystem::EndFightFor(entt::entity creature, std::optional<bool> won)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = registry.TryGet<CreatureFighting>(creature);
	if (fighting == nullptr || fighting->ended)
	{
		return;
	}
	fighting->ended = true;
	// Ending in the middle of the fight, whoever's creature it is, the player's camera is told the fight is over
	if (fight::CameraLingersOnEnd(fighting->fighter.state))
	{
		TellCameraFightIsOver();
	}
	// A quarter of the fight health it lost comes off its life, set as any life is: an indestructible creature keeps its
	// life against one of a hundredth or less
	if (registry.AllOf<CreatureNeeds>(creature))
	{
		ecs::life::SetLife(creature, fight::LifeAfterFight(ecs::life::LifeOf(creature), fighting->fighter.health));
	}
	auto& record = registry.AllOf<CreatureFightRecord>(creature) ? registry.Get<CreatureFightRecord>(creature)
	                                                             : registry.AssignState<CreatureFightRecord>(creature);
	record.foughtBefore = true;
	record.secondsSinceFight = 0.0f;
	if (won.has_value() && Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().FoughtFight(creature, *won);
	}
}

void CreatureFightSystem::Leave(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = registry.TryGet<CreatureFighting>(creature);
	if (fighting == nullptr)
	{
		return;
	}
	if (!fighting->ended)
	{
		if (auto* record = registry.TryGet<CreatureFightRecord>(creature))
		{
			record->secondsSinceFight = 0.0f;
		}
	}
	if (auto* animation = registry.TryGet<CreatureAnimation>(creature))
	{
		if (fighting->fighter.state != fight::State::Idle)
		{
			animation->body = {};
		}
		animation->face = creature_layers::RelaxFace(animation->face);
	}
	// The fight health it leaves with, ended or not and set by a script or not, stays on it after the fight
	registry.AssignOrReplaceState<CreatureFightHealth>(creature, CreatureFightHealth {.health = fighting->fighter.health});
	registry.RemoveState<CreatureFighting>(creature);
}

void CreatureFightSystem::ProcessKnockedOut()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> lying;
	std::as_const(registry).Each<const CreatureKnockedOut>(
	    [&lying](entt::entity entity, const CreatureKnockedOut& /*knockedOut*/) { lying.push_back(entity); });
	for (const auto entity : lying)
	{
		auto& knockedOut = registry.Get<CreatureKnockedOut>(entity);
		auto* needs = registry.TryGet<CreatureNeeds>(entity);
		auto& animation = registry.Get<CreatureAnimation>(entity);
		const auto& body = registry.Get<const Creature>(entity);
		knockedOut.seconds += k_TurnSeconds;
		const auto next = [&knockedOut](CreatureKnockedOut::Stage stage) {
			knockedOut.stage = stage;
			knockedOut.seconds = 0.0f;
		};
		// Only a creature that lies is got up: one still falling, as a big one's slow fall can outlast the waits, waits
		// for its fall to end
		const bool falling = animation.body.holdLoop && animation.body.phase == creature_layers::BodyAction::Phase::Start;
		const auto getUp = [&] {
			if (falling)
			{
				return;
			}
			next(CreatureKnockedOut::Stage::GettingUp);
			animation.body = creature_layers::EndLoop(animation.body);
			SetEyes(registry, entity, creature_eyes::Mode::Calm);
		};

		switch (knockedOut.stage)
		{
		case CreatureKnockedOut::Stage::Lying:
		{
			// Its faint is over on the first turn it is seen lying, and from the next it lies its turns; a knock-out
			// with no turns laid lies the seconds of its size from the faint
			bool lain = knockedOut.seconds >= fight::FaintSeconds(body.size);
			if (knockedOut.lieTurns.has_value())
			{
				lain = false;
				if (!knockedOut.seenLying)
				{
					knockedOut.seenLying =
					    animation.body.holdLoop && animation.body.phase == creature_layers::BodyAction::Phase::Loop;
				}
				else if (*knockedOut.lieTurns > 0)
				{
					--*knockedOut.lieTurns;
					knockedOut.lieCounting = true;
				}
				else
				{
					lain = true;
				}
			}
			if (!knockedOut.permanent && lain)
			{
				// A player's creature is taken home; any other comes round where it lies
				if (body.owner != PlayerNames::NEUTRAL)
				{
					// It fizzes out of sight where it lies, with the teleport's sound
					next(CreatureKnockedOut::Stage::FadingOut);
					knockedOut.fizzTurns = CarryHomeTurns();
					SetFizz(entity, 1.0f, creature_fizz::k_CarryHomeSeconds);
				}
				else
				{
					next(CreatureKnockedOut::Stage::Waiting);
				}
			}
			break;
		}
		case CreatureKnockedOut::Stage::FadingOut:
			knockedOut.fizzTurns = knockedOut.fizzTurns > 0 ? static_cast<uint16_t>(knockedOut.fizzTurns - 1) : uint16_t {0};
			if (knockedOut.fizzTurns == 0)
			{
				// Out of sight, it is moved home and fizzes back into sight there
				auto* locomotion = registry.TryGet<CreatureLocomotion>(entity);
				if (knockedOut.home.has_value() && locomotion != nullptr)
				{
					PlaceAt(entity, *locomotion, *knockedOut.home);
					registry.SetDirty();
				}
				SetFizz(entity, 0.0f, creature_fizz::k_CarryHomeSeconds);
				// Home, it is no more exhausted or thirsty than it can bear, with a little energy, as it is moved
				if (Locator::creaturePhysiologySystem::has_value())
				{
					Locator::creaturePhysiologySystem::value().WakeFromFaint(entity);
				}
				next(CreatureKnockedOut::Stage::FadingIn);
				knockedOut.fizzTurns = CarryHomeTurns();
			}
			break;
		case CreatureKnockedOut::Stage::FadingIn:
			knockedOut.fizzTurns = knockedOut.fizzTurns > 0 ? static_cast<uint16_t>(knockedOut.fizzTurns - 1) : uint16_t {0};
			if (knockedOut.fizzTurns == 0)
			{
				// The game sets the fade back in once more as it ends, by then in full sight already
				SetFizz(entity, 0.0f, creature_fizz::k_CarryHomeSeconds);
				next(CreatureKnockedOut::Stage::Waiting);
			}
			break;
		case CreatureKnockedOut::Stage::Waiting:
			// Then the spells on it, and on what it was done to, are brought to their end until none is left
			if (knockedOut.seconds >= fight::k_WaitAfterHomeSeconds && SpellsOver(registry, entity, knockedOut.actedOn))
			{
				const bool rest =
				    knockedOut.rests.value_or(needs != nullptr && fight::NeedsRest(needs->needs.life, needs->needs.exhaustion));
				if (rest)
				{
					next(CreatureKnockedOut::Stage::Resting);
				}
				else
				{
					getUp();
				}
			}
			break;
		case CreatureKnockedOut::Stage::Resting:
			if (needs == nullptr || !fight::NeedsRest(needs->needs.life, needs->needs.exhaustion))
			{
				getUp();
			}
			break;
		case CreatureKnockedOut::Stage::GettingUp:
			if (!creature_layers::IsPlaying(animation.body))
			{
				if (needs != nullptr)
				{
					needs->rest = CreatureNeeds::Rest::Awake;
				}
				registry.RemoveState<CreatureKnockedOut>(entity);
				continue;
			}
			break;
		}
		if (needs != nullptr)
		{
			// Resting, its body heals; otherwise it is out cold until it gets up
			needs->rest = knockedOut.stage == CreatureKnockedOut::Stage::Resting     ? CreatureNeeds::Rest::Resting
			              : knockedOut.stage == CreatureKnockedOut::Stage::GettingUp ? CreatureNeeds::Rest::Awake
			                                                                         : CreatureNeeds::Rest::Unconscious;
		}
	}
}
