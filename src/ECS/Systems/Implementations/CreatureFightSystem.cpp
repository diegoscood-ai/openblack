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
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/mat3x3.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandAvoid.h"
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
#include "Common/X87Vector.h"
#include "Creature/CaveCopyPick.h"
#include "Creature/CreatureActionAgendas.h"
#include "Creature/CreatureArena.h"
#include "Creature/CreatureArenaRing.h"
#include "Creature/CreatureBodyActivity.h"
#include "Creature/CreatureCastMoves.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFizz.h"
#include "Creature/CreatureFriendship.h"
#include "Creature/CreatureHelp.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureMarks.h"
#include "Creature/CreatureMode.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureRestAndHome.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSkinRay.h"
#include "Creature/CreatureTraining.h"
#include "Creature/CreatureTurnPose.h"
#include "Creature/CreatureTurnToFace.h"
#include "Creature/FightMarks.h"
#include "CreatureMindSystemDetail.h"
#include "Debug/DebugEnv.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreaturePartner.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/CreatureTurnPose.h"
#include "ECS/Components/MapSimData.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/CreatureHome.h"
#include "ECS/CreatureNaming.h"
#include "ECS/CreaturePose.h"
#include "ECS/CreatureSkin.h"
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
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/ToBeDeleted.h"
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

/// The creature gains a fight's strength this many times, one after the other
void GainFightStrength(entt::entity creature, uint32_t times)
{
	const auto gain = FightStrengthGain();
	if (!gain.has_value() || !Locator::creaturePhysiologySystem::has_value())
	{
		return;
	}
	for (uint32_t time = 0; time < times; ++time)
	{
		Locator::creaturePhysiologySystem::value().ModifyStrength(creature, *gain);
	}
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
/// A blow whose file gives it a moment earlier than this, in its animation's milliseconds, is not measured: its striking
/// bone's move into the moment is measured over this long
constexpr int32_t k_EarliestHitMs = 50;
/// That move, scaled as the game keeps it
constexpr float k_HitMoveScale = 20.0f;
/// A face is due again once its wait is within this much of over
constexpr float k_FaceDue = 1e-3f;
/// The miscellaneous help message that the creature is being taken home, as a Die action's steps are laid
constexpr uint32_t k_TransportedHomeHelp = 6;
/// The help: "the creature has fainted"
constexpr uint32_t k_HasFaintedHelp = 3;
/// A faint's plan to lie dead is forced at this priority, every part of the plan alike
constexpr float k_FaintPlanPriority = 1.0f;

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

/// How many fights the creature has performed, as its mind counts them; none without the minds
uint32_t FightsPerformed(entt::entity creature)
{
	return Locator::creatureMindSystem::has_value()
	           ? Locator::creatureMindSystem::value().GetActionCount(creature, creature_training::k_FightAction)
	           : 0;
}

/// Its response after the fight over, the creature that set out to fight as its mind's plan to fight (the evil leash's
/// too, as the leash forces that plan; not by telling the other to go away) counts the fight as performed when it is left
/// the stronger. Its opponent gone, it is not counted
void CountFightPerformed(const ecs::Registry& registry, entt::entity creature, const CreatureFighting& fighting)
{
	if (!fighting.madeArena || !Locator::creatureMindSystem::has_value())
	{
		return;
	}
	auto& minds = Locator::creatureMindSystem::value();
	const auto opponent = fighting.opponent;
	if (!minds.PlansAction(creature, "Fight") || !registry.Valid(opponent) || !registry.AllOf<Creature>(opponent))
	{
		return;
	}
	if (fight::FightPerformed(LifeOf(registry, creature), LifeOf(registry, opponent)))
	{
		minds.PerformedFight(creature);
	}
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

/// The length of the animation a fighter's clock goes round: in its stance the power-up's while a blow waits to be let
/// go, when its body has one, else its state's own
float ClockDurationOf(entt::entity creature, const fight::Fighter& fighter)
{
	if (fighter.state == fight::State::Stance && fighter.queue.AwaitsRelease())
	{
		if (const auto powerUp = DurationOf(creature, fight::animations::k_PowerUp); powerUp > 0.0f)
		{
			return powerUp;
		}
	}
	return DurationOf(creature, fighter.animation);
}

/// Moves a creature where it stands by the end of this turn; it is drawn moving there from where the turn started
void Shift(CreatureLocomotion& locomotion, glm::vec2 by)
{
	auto& point = locomotion.toPosition;
	point.x += by.x;
	point.z += by.y;
	point.y = GroundAt(Flat(point));
}

/// How far a fighter's body reaches ahead of where it stands now, along the way it faces now: its bones as the game's
/// logic last posed them, at the end of the turn before, so a move since then (its own carry or a push) changes it.
/// Before it was ever posed, its bones as its fight animation poses them at its time, at the size it is drawn.
float ForwardExtentOf(const ecs::Registry& registry, entt::entity creature, const fight::Fighter& fighter)
{
	const auto* pose = registry.TryGet<const CreatureTurnPose>(creature);
	const auto* locomotion = registry.TryGet<const CreatureLocomotion>(creature);
	if (pose != nullptr && locomotion != nullptr && creature_turn_pose::HasPose(*pose))
	{
		const auto posed = creature_turn_pose::Safe(*pose);
		std::vector<glm::vec2> bones;
		bones.reserve(posed.size());
		for (const auto& bone : posed)
		{
			bones.emplace_back(bone[3].x, bone[3].z);
		}
		return fight::ForwardExtentFrom(bones, Flat(locomotion->toPosition),
		                                creature_locomotion::DirectionOf(locomotion->heading));
	}
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
/// the time on, at the fight speed, so that the body shows exactly where the move is: blows land and sound as drawn,
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

/// A creature taking its part in a fight stops what it does: its walk, what its hands do and hold, and its body's
/// action
void StopForTheFight(ecs::Registry& registry, entt::entity creature)
{
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().Stop(creature);
	}
	if (Locator::creatureObjectActionSystem::has_value())
	{
		Locator::creatureObjectActionSystem::value().Cancel(creature);
		Locator::creatureObjectActionSystem::value().Drop(creature);
	}
	registry.Get<CreatureAnimation>(creature).body = {};
}

void PlayOnce(CreatureAnimation& animation, size_t played, bool mirrored = false)
{
	animation.body = {};
	if (const auto once = creature_layers::PlayOnce(animation.body, played, mirrored))
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

/// Whether the creature still has the opponent its duel is with: one past its fight's set-up, in its own duel or with
/// its own side of the fight ended while this one still fights it. Only the creature's own duel ends it, so an
/// opponent whose body ended its fight is still fought and struck
bool HasItsOpponent(const ecs::Registry& registry, entt::entity opponent)
{
	const auto* other = registry.Valid(opponent) ? registry.TryGet<const CreatureFighting>(opponent) : nullptr;
	return other != nullptr && !IsLeadIn(other->stage);
}

/// Whether the creature's mind has the other creature as its partner in a joint action
bool MindsPartner(const ecs::Registry& registry, entt::entity creature, entt::entity other)
{
	const auto* link = registry.Valid(creature) ? registry.TryGet<const CreaturePartner>(creature) : nullptr;
	return link != nullptr && link->partner == other;
}

/// Whether the opponent of a fighter has its order to start its duel and starts it when its body lets it: it is in the
/// fight
bool OpponentStartsDuel(const ecs::Registry& registry, entt::entity creature, const CreatureFighting& fighting)
{
	const auto* other =
	    registry.Valid(fighting.opponent) ? registry.TryGet<const CreatureFighting>(fighting.opponent) : nullptr;
	return other != nullptr && other->opponent == creature && other->stage == CreatureFighting::Stage::Square && other->played;
}

/// Whether the body of the creature is busy: it moves or turns, it plays an action, it does an action on a thing, or its
/// fighter is in a fight state
bool BodyBusy(const ecs::Registry& registry, entt::entity creature, const CreatureLocomotionSystemInterface* locomotion)
{
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	const auto* fighting = registry.TryGet<const CreatureFighting>(creature);
	const auto* hands =
	    Locator::creatureObjectActionSystem::has_value() ? &Locator::creatureObjectActionSystem::value() : nullptr;
	return creature_body::IsPerformingAction({
	    .playing = animation != nullptr && creature_layers::IsPlaying(animation->body),
	    .moving = locomotion != nullptr && locomotion->IsMoving(creature),
	    .handling = hands != nullptr && (hands->GetProgress(creature).has_value() || hands->IsCatching(creature)),
	    .fighting = fighting != nullptr && fighting->fighter.state != fight::State::Idle,
	});
}

/// Whether the creature can start its duel on this turn: its body refuses while it is busy, and a body with no animations
/// for the start, the stance and the finish of a fight refuses always. With no bodies loaded, nothing plays and the duel
/// starts
bool CanStartDuel(const ecs::Registry& registry, entt::entity creature, const CreatureLocomotionSystemInterface* locomotion)
{
	if (BodyBusy(registry, creature, locomotion))
	{
		return false;
	}
	using namespace fight::animations;
	return !Locator::creatureAnimationSystem::has_value() ||
	       (Has(creature, k_Start) && Has(creature, k_Stance) && Has(creature, k_Finish));
}

/// One game turn of the creature's turn to face its opponent in the face-off or in the arena, from where it stands to
/// where the opponent is now
void FaceOpponentTurn(const ecs::Registry& registry, entt::entity creature, CreatureFighting& fighting,
                      CreatureLocomotionSystemInterface* locomotion)
{
	using Phase = creature_turn_to_face::Turn::Phase;
	if (fighting.faceTurn.phase == Phase::Idle || fighting.faceTurn.phase == Phase::Done)
	{
		return;
	}
	// With no legs, nothing turns
	const auto* legs = registry.TryGet<const CreatureLocomotion>(creature);
	if (locomotion == nullptr || legs == nullptr)
	{
		fighting.faceTurn.phase = Phase::Done;
		return;
	}
	const auto here = Flat(registry.Get<const Transform>(creature).position);
	const auto there = Flat(registry.Get<const Transform>(fighting.opponent).position);
	const creature_turn_to_face::Senses senses {
	    .onTop = glm::distance(here, there) < creature_cast_moves::k_OnTopDistance,
	    .facing = creature_cast_moves::Facing(here, legs->heading, there),
	    .bodyBusy = BodyBusy(registry, creature, locomotion),
	};
	const auto hold = static_cast<uint32_t>(std::max(game_clock::TicksForSeconds(fight::k_FaceOffHoldSeconds), 0));
	creature_turn_to_face::Step(fighting.faceTurn, senses, hold,
	                            [locomotion, creature, there] { return locomotion->TurnToFace(creature, there); });
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

CreatureFightSystemInterface::StartResult CreatureFightSystem::StartFight(entt::entity creature, entt::entity opponent,
                                                                          std::optional<creature_fight::Taunts> taunts)
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
	// Every fight starts by asking the other creature, which may refuse: its partner already agrees, one with another
	// partner refuses
	const auto* partner = std::as_const(registry).TryGet<const CreaturePartner>(opponent);
	const auto availability = creature_friendship::AvailabilityOf({
	    .dead = registry.AllOf<CreatureKnockedOut>(opponent),
	    .scriptControlled = ecs::script_held::IsControlledByScript(opponent),
	    .askerScriptControlled = ecs::script_held::IsControlledByScript(creature),
	    .partner = partner != nullptr ? std::optional(entt::to_integral(partner->partner)) : std::nullopt,
	    .asker = entt::to_integral(creature),
	});
	// A fight its creature's mind planned has the taunts its agenda drew as it was made; any other draws each creature's
	// taunt now, before it asks the other creature: the other creature's first, any draw but 0 the rude gesture
	if (!taunts.has_value())
	{
		const bool otherDraw = game_random::GameRand(2) != 0;
		const bool ownDraw = game_random::GameRand(2) != 0;
		taunts = fight::Taunts {.otherRude = otherDraw, .ownRude = ownDraw};
	}
	const bool otherRude = taunts->otherRude;
	const bool ownRude = taunts->ownRude;
	if (!fight::AgreesToFight(DrawnHeightOf(registry, opponent), DrawnHeightOf(registry, creature), availability))
	{
		return StartResult::Refused;
	}

	for (const auto& [self, other] : {std::pair(creature, opponent), std::pair(opponent, creature)})
	{
		const auto& body = registry.Get<const Creature>(self);
		// The first creature stops what it does at once; the other carries on until its first order
		if (self == creature)
		{
			StopForTheFight(registry, self);
		}

		// Starting a fight adds nothing to the creature's battles fought; its duel start does
		auto& record = registry.AllOf<CreatureFightRecord>(self) ? registry.Get<CreatureFightRecord>(self)
		                                                         : registry.AssignState<CreatureFightRecord>(self);
		// The creature that starts the fight chooses its moves by what it has learnt; the other by the lean it chose by
		// in the last fight it started
		if (self == creature)
		{
			record.tendency = fight::TendencyAtStart(FightsPerformed(self), record.tendency, body.alignment);
			record.choosingTendency = record.tendency;
		}
		const auto* needs = registry.TryGet<const CreatureNeeds>(self);
		const auto bodyNeeds = needs != nullptr ? needs->needs : creature_physiology::Needs {};

		CreatureFighting fighting {
		    .stage = CreatureFighting::Stage::FaceOff,
		    .opponent = other,
		    .madeArena = self == creature,
		    .ordered = self == creature,
		    .awaitsArena = true,
		    .rude = self == creature ? ownRude : otherRude,
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
	return StartResult::Started;
}

bool CreatureFightSystem::TakeArena(entt::entity creature, entt::entity opponent)
{
	auto& registry = Locator::entitiesRegistry::value();
	// The arena is found where the two stand as the first creature's taunt ends
	const auto& from = registry.Get<const Transform>(creature).position;
	const auto& to = registry.Get<const Transform>(opponent).position;
	// sized by the bigger body as drawn, smaller in a temple's pen
	auto arena = fight::MakeArena(Flat(from), Flat(to), ecs::creature_pose::DrawnSize(registry, creature),
	                              ecs::creature_pose::DrawnSize(registry, opponent));
	// It takes the nearest arena already on the game's list, another fight's or the land's, strictly closer to the middle
	// of the two (as a map position) than its reach, at the smaller of the two radii, which a land's arena keeps from
	// then on; else it makes its own on the nearest clear ground, smaller if it must, and the fight fails without any
	const auto taken = ArenaToTake(registry, creature, arena);
	entt::entity tookArenaOf = entt::null;
	if (taken.has_value())
	{
		tookArenaOf = taken->owner;
		arena.centre = taken->centre;
		arena.radius = std::min(taken->radius, arena.radius);
		arena.serial = taken->serial;
		if (auto* land = registry.TryGet<Arena>(taken->owner))
		{
			land->radius = arena.radius;
			arena.serial = ++_arenasMade;
		}
	}
	else
	{
		const auto start = arena.centre;
		const auto placed = creature_arena::Place(arena.radius, [&registry, creature, start](float width) {
			return creature_cast_moves::FindClearArea(
			    start, width,
			    [&registry, creature](int32_t x, int32_t z) {
				    return land_avoid::At(x, z) == land_avoid::k_Land &&
				           !mind_detail::CellBlocked(registry, creature, x, z, false, true);
			    },
			    creature_cast_moves::ClearAreaCellsFor(creature_arena::k_SearchRange));
		});
		if (!placed.has_value())
		{
			return false;
		}
		arena.centre = placed->centre;
		arena.radius = placed->radius;
		arena.serial = ++_arenasMade;
	}
	const auto startCount = ++_fightsStarted;
	// The arena as a thing of the game's list: the land's it took, the one made by the fight it took it from, or a new one
	// this fight makes, which starts its fight reaction for no player and not started, as every arena does as it is made
	entt::entity arenaThing = entt::null;
	if (tookArenaOf != entt::null)
	{
		if (registry.AllOf<Arena>(tookArenaOf))
		{
			arenaThing = tookArenaOf;
		}
		else if (const auto* holder = std::as_const(registry).TryGet<const CreatureFighting>(tookArenaOf))
		{
			arenaThing = holder->arenaThing;
		}
	}
	else
	{
		arenaThing = registry.Create();
		registry.Assign<Transform>(arenaThing, glm::vec3(arena.centre.x, GroundAt(arena.centre), arena.centre.y),
		                           glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<CreatureFightArena>(
		    arenaThing, CreatureFightArena {
		                    .maker = creature,
		                    .place = {map_coords::ToFixed(arena.centre.x), map_coords::ToFixed(arena.centre.y)},
		                    .radius = arena.radius,
		                });
		if (Locator::reactionSystem::has_value())
		{
			static_cast<void>(
			    ecs::effects::reactions::CreateReaction(arenaThing, openblack::Reaction::ReactToFight, std::nullopt, false));
		}
	}
	for (const auto self : {creature, opponent})
	{
		auto& fighting = registry.Get<CreatureFighting>(self);
		fighting.arena = arena;
		fighting.awaitsArena = false;
		fighting.tookArenaOf = tookArenaOf;
		fighting.arenaThing = arenaThing;
		fighting.startCount = startCount;
	}

	// The fight takes its arena with the game's sound at its middle, on the land there, not following anything; the
	// arena owns it
	if (Locator::audio::has_value())
	{
		const auto soundAt = fight::ArenaSoundPoint(arena);
		audio::PlayOptions sound;
		sound.sample = {audio::Bank(audio::SfxBank::InGame), fight::k_ArenaTakenSample};
		sound.is3D = true;
		sound.track = false;
		if (arenaThing != entt::null)
		{
			sound.owner = audio::Owner::Thing(arenaThing);
		}
		sound.position = glm::vec3(soundAt.x, GroundAt(soundAt), soundAt.y);
		audio::PlaySoundEffect(sound);
	}
	return true;
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

namespace
{
void TraceFightStage(const ecs::Registry& registry, entt::entity entity, const CreatureFighting& fighting,
                     std::string_view part);
} // namespace

void CreatureFightSystem::AbortFight(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = registry.TryGet<CreatureFighting>(creature);
	if (fighting == nullptr)
	{
		return;
	}
	TraceFightStage(registry, creature, *fighting, "AbortFight");
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
		else if (IsLeadIn(side->stage))
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
	TraceFightStage(registry, creature, *fighting, "CallOff");
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
		else if (IsLeadIn(side->stage))
		{
			Leave(self);
		}
	}
}

void CreatureFightSystem::EndOwnFight(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* side = registry.Valid(creature) ? registry.TryGet<CreatureFighting>(creature) : nullptr;
	// Its duel already ended this turn by its partner's stop, its body ends its fight again: still in the midst of the
	// fight, its life pays once more
	if (side != nullptr && side->stage != CreatureFighting::Stage::Duel && creature == _endedByPartner &&
	    _endedByPartnerTurn == game_clock::Turn())
	{
		_endedByPartner = entt::null;
		PayLifeAgain(creature, 1);
		return;
	}
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
			logger->trace("Creature {} forced to finish in its fight's {}: its turn's span doubles",
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

std::optional<CreatureFightSystem::TakenArena>
CreatureFightSystem::ArenaToTake(const ecs::Registry& registry, entt::entity creature, const fight::Arena& wanted) const
{
	// the game's list, the newest first: the fights' own arenas, then the land's, the last made first
	std::vector<glm::ivec2> listed;
	std::vector<TakenArena> owners;
	for (const auto& made : GetArenas())
	{
		const auto* fighting = registry.TryGet<const CreatureFighting>(made.maker);
		if (fighting != nullptr && fighting->tookArenaOf != entt::null)
		{
			continue; // an arena it took is listed as its own owner's
		}
		listed.emplace_back(map_coords::ToFixed(made.centre.x), map_coords::ToFixed(made.centre.y));
		owners.push_back({.owner = made.maker, .centre = made.centre, .radius = made.radius, .serial = made.serial});
	}
	std::vector<std::pair<uint32_t, entt::entity>> land;
	registry.Each<const Arena>([&land](entt::entity entity, const Arena& arena) { land.emplace_back(arena.number, entity); });
	std::ranges::sort(land, std::ranges::greater {}, &std::pair<uint32_t, entt::entity>::first);
	for (const auto& [number, entity] : land)
	{
		const auto& arena = registry.Get<const Arena>(entity);
		listed.emplace_back(map_coords::ToFixed(arena.position.x), map_coords::ToFixed(arena.position.z));
		owners.push_back({.owner = entity,
		                  .centre = {map_coords::Quantise(arena.position.x), map_coords::Quantise(arena.position.z)},
		                  .radius = arena.radius,
		                  .serial = 0});
	}
	// the reach: ten times the pace it runs at, from its species' running share and its size
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr || !Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto& infos = Locator::infoConstants::value().creature;
	const auto row = creature::InfoRow(body->species);
	if (row >= infos.size())
	{
		return std::nullopt;
	}
	const float reach =
	    creature_arena::ReuseDistance(infos.at(row).runSpeed, ecs::creature_pose::GivenSize(registry, creature));
	const glm::ivec2 middle {map_coords::ToFixed(wanted.centre.x), map_coords::ToFixed(wanted.centre.y)};
	if (const auto nearest = creature_arena::Nearest(listed, middle, reach); nearest.has_value())
	{
		return owners.at(*nearest);
	}
	return std::nullopt;
}

std::vector<CreatureFightSystemInterface::ArenaView> CreatureFightSystem::GetArenas() const
{
	// Read through the const registry, so that asking makes no storage. The middle is given as the original's arena
	// keeps it, as a map position
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	using Made = std::pair<uint32_t, ArenaView>;
	std::vector<Made> made;
	registry.Each<const CreatureFighting>([&made](entt::entity entity, const CreatureFighting& fighting) {
		if (fighting.madeArena && !fighting.awaitsArena)
		{
			made.emplace_back(fighting.arena.serial, ArenaView {.maker = entity,
			                                                    .opponent = fighting.opponent,
			                                                    .centre = {map_coords::Quantise(fighting.arena.centre.x),
			                                                               map_coords::Quantise(fighting.arena.centre.y)},
			                                                    .radius = fighting.arena.radius,
			                                                    .serial = fighting.arena.serial,
			                                                    .thing = fighting.arenaThing});
		}
	});
	std::ranges::sort(made, std::ranges::greater {}, &Made::first);
	std::vector<ArenaView> arenas;
	arenas.reserve(made.size());
	std::ranges::transform(made, std::back_inserter(arenas), &Made::second);
	return arenas;
}

std::optional<CreatureFightSystemInterface::ArenaView> CreatureFightSystem::FightIn(entt::entity thing) const
{
	if (thing == entt::null)
	{
		return std::nullopt;
	}
	// of the fights on that hold it, the one that started last, by the creature that started it
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	std::optional<ArenaView> latest;
	uint32_t latestStart = 0;
	registry.Each<const CreatureFighting>([&](entt::entity entity, const CreatureFighting& fighting) {
		if (fighting.madeArena && !fighting.ended && fighting.arenaThing == thing && fighting.startCount >= latestStart)
		{
			latestStart = fighting.startCount;
			latest = ArenaView {
			    .maker = entity,
			    .opponent = fighting.opponent,
			    .centre = {map_coords::Quantise(fighting.arena.centre.x), map_coords::Quantise(fighting.arena.centre.y)},
			    .radius = fighting.arena.radius,
			    .serial = fighting.arena.serial,
			    .thing = thing};
		}
	});
	return latest;
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
	for (const auto entity : marked)
	{
		TurnMarksOf(entity);
	}
}

void CreatureFightSystem::TurnMarksOf(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto turnMs = static_cast<uint32_t>(TurnMs());
	{
		auto& fighting = registry.Get<CreatureFighting>(entity);
		// the held press's mark counts the creature's turn
		fighting.marks.Hold(turnMs);
		// the local player's creature in its duel turns and moves its ground marks with it
		if (!registry.Valid(fighting.opponent) || !registry.AllOf<Transform, CreatureLocomotion>(entity) ||
		    std::as_const(registry).Get<const Creature>(entity).owner != creature::LocalPlayer())
		{
			return;
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
	// the last frame's drawing held the hand at its point: the hand reads it this frame
	_handPin.BeginFrame();
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
				// while a press is held, the hand is held at the mark: the last of the list's marks on the opponent
				if (point.has_value())
				{
					_handPin.Drawn(*point, marks.pending);
				}
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
	if (const auto logger = spdlog::get("game"))
	{
		const auto& fighter = fighting->fighter;
		const bool reels = fighter.state == fight::State::Action && fighter.animation >= fight::animations::k_RecoilHigh &&
		                   fighter.animation < fight::animations::k_WobbleHighSide;
		logger->trace("Fight: creature {} takes a miracle in its fight: heal {:.4f}, harm {:.4f}, fight health {:.4f}, {}",
		              entt::to_integral(creature), heal, harm, taken.health,
		              fighter.state == fight::State::BlockRecoil ? "recoils its block"
		              : reels                                    ? "reels"
		                                                         : "does not reel");
	}
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
			Win(fighting->opponent, creature, false);
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
	// The stop reaches the opponent only while the creature's own duel stands, whatever the opponent's state: an opponent
	// whose own fight already ended still gains, has its duel cleared and fails
	const auto opponent = fighting->opponent;
	const bool duels = fighting->stage == CreatureFighting::Stage::Duel && !fighting->ended;
	auto* other = registry.Valid(opponent) ? registry.TryGet<CreatureFighting>(opponent) : nullptr;
	if (!duels || other == nullptr)
	{
		return;
	}
	const bool partners = MindsPartner(registry, creature, opponent) && MindsPartner(registry, opponent, creature);
	if (const auto gain = FightStrengthGain(); gain.has_value() && Locator::creaturePhysiologySystem::has_value())
	{
		Locator::creaturePhysiologySystem::value().ModifyStrength(opponent, *gain);
	}
	EndOwnFight(opponent);
	registry.Get<CreatureFighting>(opponent).opponent = entt::null;
	// Between partners in each other's duel, the opponent's failure fails its partner, the creature, again before the
	// creature's own body ends its fight: the creature, still dueling, gains a fight's strength and stops again, which
	// gains the opponent one more and fails it once more, then the creature's body ends its fight, and its mind fails
	// the opponent a last time. Neither body changes its state meanwhile, so the opponent's body ends its fight two times
	// more and the creature's once, each paying the life again in the midst of the fight; the creature's own end later
	// in its stop pays once more
	if (partners)
	{
		GainFightStrength(creature, 1);
		GainFightStrength(opponent, 1);
		PayLifeAgain(opponent, 2);
		EndOwnFight(creature);
		_endedByPartner = creature;
		_endedByPartnerTurn = game_clock::Turn();
	}
}

void CreatureFightSystem::StopsAsAFailure(entt::entity creature)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* fighting = registry.Valid(creature) ? registry.TryGet<const CreatureFighting>(creature) : nullptr;
	if (fighting == nullptr)
	{
		return;
	}
	const bool settingUp = IsLeadIn(fighting->stage);
	FailsWhileDueling(creature);
	PartnerStops(creature);
	EndOwnFight(creature);
	if (settingUp)
	{
		CallOff(creature);
	}
}

void CreatureFightSystem::WalksBack(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* fighting =
	    registry.Valid(creature) ? std::as_const(registry).TryGet<const CreatureFighting>(creature) : nullptr;
	if (fighting == nullptr)
	{
		return;
	}
	const auto opponent = fighting->opponent;
	const auto* other = registry.Valid(opponent) ? std::as_const(registry).TryGet<const CreatureFighting>(opponent) : nullptr;
	const bool opponentDuels = other != nullptr && other->opponent == creature && InDuel(registry, opponent);
	// The chain's calls with the two as they stand: each body end pays the life again in the midst of the fight, each
	// gain a fight's strength
	const auto chain = fight::RunStopChain(
	    {.duels = {InDuel(registry, creature), opponentDuels},
	     .partners = {MindsPartner(registry, creature, opponent), MindsPartner(registry, opponent, creature)}},
	    fight::WalkBackSteps());
	const auto endSide = [this, &registry](entt::entity side, uint32_t ends) {
		if (ends == 0)
		{
			return;
		}
		// the first end ends its duel; any more pay again
		const bool dueling = InDuel(registry, side);
		EndOwnFight(side);
		PayLifeAgain(side, dueling ? ends - 1 : ends);
	};
	endSide(creature, chain.bodyEnds[0]);
	GainFightStrength(creature, chain.gains[0]);
	if (opponentDuels)
	{
		endSide(opponent, chain.bodyEnds[1]);
		GainFightStrength(opponent, chain.gains[1]);
		if (!chain.after.duels[1])
		{
			registry.Get<CreatureFighting>(opponent).opponent = entt::null;
		}
	}
}

void CreatureFightSystem::PayLifeAgain(entt::entity creature, uint32_t times)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* side = registry.Valid(creature) ? std::as_const(registry).TryGet<const CreatureFighting>(creature) : nullptr;
	if (side == nullptr || !fight::CameraLingersOnEnd(side->fighter.state) ||
	    !std::as_const(registry).AllOf<CreatureNeeds>(creature))
	{
		return;
	}
	for (uint32_t time = 0; time < times; ++time)
	{
		ecs::life::SetLife(creature, fight::LifeAfterFight(ecs::life::LifeOf(creature), side->fighter.health));
	}
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
		// Its stop stops the one dueling it, between partners failing each other again (PartnerStops), then its own body
		// ends its fight
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
	// A faint is a plan forced on it as its player's order is: to obey the player by dying, done to itself. What it was
	// doing stops as a failure first. The knock-out carries out the plan's steps, so the plan's one step waits until it is
	// on its feet again; until then its mind chooses nothing for itself. A Die action forced on it already has the plan
	if (!forcedDeath && Locator::creatureMindSystem::has_value())
	{
		const bool planned =
		    Locator::creatureMindSystem::value().ForceActivity(creature, {.desire = creature_desires::Desire::ObeyPlayer,
		                                                                  .action = creature_friendship::k_DieAction,
		                                                                  .actionObject = creature,
		                                                                  .score = k_FaintPlanPriority,
		                                                                  .agenda = creature_mind::Die()});
		if (auto* mind = registry.TryGet<CreatureMindState>(creature); planned && mind != nullptr)
		{
			mind->commanded = true;
		}
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
		StartFaintPose(creature, mirrored);
	}
	else
	{
		animation.face = creature_layers::RelaxFace(animation.face);
		SetEyes(registry, creature, creature_eyes::Mode::Closed);
	}
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

void CreatureFightSystem::StartFaintPose(entt::entity creature, bool mirrored)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& animation = registry.Get<CreatureAnimation>(creature);
	animation.body = {
	    .kind = creature_layers::BodyAction::Kind::Sequence,
	    .phase = creature_layers::BodyAction::Phase::Start,
	    .animations = {fight::animations::k_Faint, fight::animations::k_Faint, fight::animations::k_GetUp},
	    .mirrored = mirrored,
	    .holdLoop = true,
	};
	animation.face = creature_layers::RelaxFace(animation.face);
	SetEyes(registry, creature, creature_eyes::Mode::Closed);
}

void CreatureFightSystem::RestoreKnockedOut(entt::entity creature, const KeptKnockOut& kept)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !std::as_const(registry).AllOf<Creature, CreatureAnimation, Transform>(creature))
	{
		return;
	}
	// taken to its pen, or with no temple for a pen to the middle of the land, as a faint takes it
	const auto here = std::as_const(registry).Get<const Transform>(creature).position;
	auto& knockedOut = registry.AssignOrReplaceState<CreatureKnockedOut>(
	    creature, CreatureKnockedOut {.home = ecs::creature_home::HomeOf(registry, creature).value_or(NoPen().value_or(here))});
	knockedOut.lieTurns = kept.lieTurns;
	knockedOut.rests = kept.rests;
	knockedOut.actedOn = kept.actedOn;
	using Step = KeptKnockOut::Step;
	if (kept.step == Step::Faint)
	{
		knockedOut.restartFaint = true;
		return;
	}
	// past the faint it was seen lying, and its lie counted down
	knockedOut.seenLying = true;
	knockedOut.lieCounting = true;
	switch (kept.step)
	{
	case Step::Faint:
	case Step::Lie:
		break;
	case Step::CarryHome:
		// it fizzes out of sight again where it is
		knockedOut.lieTurns = 0;
		knockedOut.stage = CreatureKnockedOut::Stage::FadingOut;
		knockedOut.fizzTurns = CarryHomeTurns();
		SetFizz(creature, 1.0f, creature_fizz::k_CarryHomeSeconds);
		break;
	case Step::HomeWait:
		knockedOut.lieTurns = 0;
		knockedOut.stage = CreatureKnockedOut::Stage::Waiting;
		knockedOut.homeWaitTurns = kept.homeWaitTurns;
		break;
	case Step::WaitForSpells:
		knockedOut.lieTurns = 0;
		knockedOut.stage = CreatureKnockedOut::Stage::Waiting;
		knockedOut.homeWaitTurns = 0;
		knockedOut.homeWaited = true;
		break;
	case Step::Rest:
		// its rest from the first try to lie down, its turns of rest counted again
		knockedOut.lieTurns = 0;
		knockedOut.homeWaitTurns = 0;
		knockedOut.homeWaited = true;
		knockedOut.stage = CreatureKnockedOut::Stage::Resting;
		break;
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

void CreatureFightSystem::BeginCreatureTurns() {}

void CreatureFightSystem::ProcessCreatureTurn(entt::entity creature)
{
	// One creature's fight, every part of its turn before the next creature's (issue #6): its fight animations play on
	// by a turn, its marks, the time since its last fight, a fight its anger picks, then its fight's stage, its duel and
	// its lying knocked out
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	if (!registry.Valid(creature))
	{
		return;
	}
	if (const auto* fighting = lookup.TryGet<const CreatureFighting>(creature);
	    fighting != nullptr && fighting->fighter.state != fight::State::Idle)
	{
		_fightersMoved = false;
		StepFighter(creature, TurnMs());
		if (std::exchange(_fightersMoved, false))
		{
			registry.SetDirty();
		}
	}
	if (const auto* fighting = lookup.TryGet<const CreatureFighting>(creature);
	    fighting != nullptr && fighting->marks.count > 0)
	{
		TurnMarksOf(creature);
	}
	if (auto* record = registry.TryGet<CreatureFightRecord>(creature))
	{
		record->secondsSinceFight += k_TurnSeconds;
	}
	if (const auto opponent = FightWantedBy(creature); opponent.has_value())
	{
		static_cast<void>(StartFight(creature, *opponent));
	}
	if (lookup.AllOf<CreatureFighting>(creature))
	{
		ProcessStageOf(creature);
	}
	if (const auto* fighting = lookup.TryGet<const CreatureFighting>(creature);
	    fighting != nullptr && fighting->stage == CreatureFighting::Stage::Duel)
	{
		ProcessDuelOf(creature);
	}
	if (lookup.AllOf<CreatureKnockedOut>(creature))
	{
		ProcessKnockedOutOf(creature);
	}
}

void CreatureFightSystem::ProcessCreatureSubAction(entt::entity creature)
{
	// The fight's part of the creature's sub-action: the time since its last fight, a fight its anger picks, then its
	// fight's stage
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return;
	}
	if (auto* record = registry.TryGet<CreatureFightRecord>(creature))
	{
		record->secondsSinceFight += k_TurnSeconds;
	}
	if (const auto opponent = FightWantedBy(creature); opponent.has_value())
	{
		static_cast<void>(StartFight(creature, *opponent));
	}
	// then its fight's stage, as the original's fight sub-actions lead it in and follow it: the face-off, the taunt, the
	// point at the arena, the walk to it, squaring up, the duel's own turn and what follows it
	if (std::as_const(registry).AllOf<CreatureFighting>(creature))
	{
		ProcessStageOf(creature, false);
	}
}

void CreatureFightSystem::ProcessCreatureBody(entt::entity creature)
{
	// The fight's part of the body's update: its fight animations by the turn, its marks, its duel's moves and blows and
	// its lying knocked out, as the original's body steps its fight states
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	if (!registry.Valid(creature))
	{
		return;
	}
	if (const auto* fighting = lookup.TryGet<const CreatureFighting>(creature);
	    fighting != nullptr && fighting->fighter.state != fight::State::Idle)
	{
		_fightersMoved = false;
		StepFighter(creature, TurnMs());
		if (std::exchange(_fightersMoved, false))
		{
			registry.SetDirty();
		}
	}
	if (const auto* fighting = lookup.TryGet<const CreatureFighting>(creature);
	    fighting != nullptr && fighting->marks.count > 0)
	{
		TurnMarksOf(creature);
	}
	// its anger shown as the duel starts and every few seconds of it, then the duel's moves and blows
	DuelAngerOf(creature);
	if (const auto* fighting = lookup.TryGet<const CreatureFighting>(creature);
	    fighting != nullptr && fighting->stage == CreatureFighting::Stage::Duel)
	{
		ProcessDuelOf(creature);
	}
	if (lookup.AllOf<CreatureKnockedOut>(creature))
	{
		ProcessKnockedOutOf(creature);
	}
}

void CreatureFightSystem::EndCreatureTurns() {}

void CreatureFightSystem::StartFightsFromMinds()
{
	auto& registry = Locator::entitiesRegistry::value();
	// A fight the leash sends the creature to is its mind's: the act-on forces the fight plan on it
	std::vector<std::pair<entt::entity, entt::entity>> wanted;
	// A creature angry enough picks a fight with the nearest creature in reach
	if (_angerStartsFights)
	{
		std::as_const(registry).Each<const CreatureMindState, const Creature, const Transform>(
		    [&](entt::entity entity, const CreatureMindState& /*mind*/, const Creature& /*creature*/, const Transform& /*at*/) {
			    if (const auto opponent = FightWantedBy(entity); opponent.has_value())
			    {
				    wanted.emplace_back(entity, *opponent);
			    }
		    });
	}
	for (const auto& [creature, opponent] : wanted)
	{
		static_cast<void>(StartFight(creature, opponent));
	}
}

std::optional<entt::entity> CreatureFightSystem::FightWantedBy(entt::entity entity) const
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	if (!_angerStartsFights || !lookup.Valid(entity) || !lookup.AllOf<CreatureMindState, Creature, Transform>(entity) ||
	    !ecs::IsAvailable(entity))
	{
		return std::nullopt;
	}
	const auto& mind = lookup.Get<const CreatureMindState>(entity);
	const auto& creature = lookup.Get<const Creature>(entity);
	const auto& transform = lookup.Get<const Transform>(entity);
	std::optional<entt::entity> wanted;
	[&]() {
		if (!mind.desires.has_value() || mind.paused || registry.AnyOf<CreatureFighting, CreatureKnockedOut>(entity))
		{
			return;
		}
		const auto& anger = (*mind.desires)[creature_desires::Desire::Anger];
		if (!mind.desires->IsActivated(creature_desires::Desire::Anger) || anger.value < fight::k_AngerToFight)
		{
			return;
		}
		// the others are the game's list of creatures, which a creature waiting on the dead list has left
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
		    },
		    entt::exclude<Unavailable>);
		const auto* record = registry.TryGet<const CreatureFightRecord>(entity);
		const auto since = record != nullptr ? record->secondsSinceFight : fight::k_SecondsBetweenFights;
		if (nearest.has_value() && fight::WantsToFight(anger.value, LifeOf(registry, entity), best, creature.size, since))
		{
			wanted = *nearest;
		}
	}();
	return wanted;
}

void CreatureFightSystem::ProcessStages()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> fighters;
	std::as_const(registry).Each<const CreatureFighting>(
	    [&fighters](entt::entity entity, const CreatureFighting& /*fighting*/) { fighters.push_back(entity); });
	for (const auto entity : fighters)
	{
		ProcessStageOf(entity);
	}
}

namespace
{
/// (readout, OPENBLACK_FIGHT_STAGE_TRACE) a fighter's stage, its turn to face's phase, its heading and place, as its
/// stage or its duel runs
void TraceFightStage(const ecs::Registry& registry, entt::entity entity, const CreatureFighting& fighting,
                     std::string_view part)
{
	static const bool k_Trace = debug_env::FightStageTrace();
	if (!k_Trace)
	{
		return;
	}
	const auto* transform = registry.TryGet<const Transform>(entity);
	const auto* legs = registry.TryGet<const CreatureLocomotion>(entity);
	if (auto logger = spdlog::get("game"); logger != nullptr && transform != nullptr)
	{
		logger->info(
		    "Fight stage trace: creature {} {} turn {}: stage {}, fighter state {}, turn to face {}, heading {:.4f}, at "
		    "({:.2f}, {:.2f}), opponent {}",
		    entt::to_integral(entity), part, game_clock::Turn(), static_cast<int>(fighting.stage),
		    static_cast<int>(fighting.fighter.state), static_cast<int>(fighting.faceTurn.phase),
		    legs != nullptr ? legs->heading : 0.0f, transform->position.x, transform->position.z,
		    entt::to_integral(fighting.opponent));
	}
}
} // namespace

void CreatureFightSystem::DuelAngerOf(entt::entity entity)
{
	// Fighting its opponent, it shows its anger as it starts and every few seconds after
	auto* fighting = Locator::entitiesRegistry::value().TryGet<CreatureFighting>(entity);
	if (fighting != nullptr && fighting->stage == CreatureFighting::Stage::Duel && Locator::creatureMindSystem::has_value())
	{
		fighting->faceSeconds -= k_TurnSeconds;
		if (fighting->faceSeconds < k_FaceDue)
		{
			Locator::creatureMindSystem::value().ShowFeeling(entity, creature_face::Cue::Anger);
			fighting->faceSeconds = creature_mind::k_FaceRepeatSeconds;
		}
	}
}

void CreatureFightSystem::ProcessStageOf(entt::entity entity, bool withDuelAnger)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* locomotion = Locator::creatureLocomotionSystem::has_value() ? &Locator::creatureLocomotionSystem::value() : nullptr;
	const auto moving = [locomotion](entt::entity entity) { return locomotion != nullptr && locomotion->IsMoving(entity); };
	{
		auto* fighting = registry.TryGet<CreatureFighting>(entity);
		if (fighting == nullptr)
		{
			return;
		}
		TraceFightStage(registry, entity, *fighting, "stage");
		fighting->stageSeconds += k_TurnSeconds;
		if (withDuelAnger)
		{
			DuelAngerOf(entity);
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
		// The creature that started the fight tells the other creature when to do each step before the duel
		const auto order = [&registry](entt::entity who, CreatureFighting::Stage stage) {
			auto& side = registry.Get<CreatureFighting>(who);
			side.stage = stage;
			side.stageSeconds = 0.0f;
			side.played = false;
		};
		const auto* partner = opponentHere ? registry.TryGet<const CreatureFighting>(opponent) : nullptr;
		const bool together = partner != nullptr && partner->opponent == entity;
		const bool starter = fighting->madeArena;
		if (IsLeadIn(fighting->stage) && !together)
		{
			Leave(entity);
			return;
		}
		// Before the duel only the creature that started the fight shows anger in its face; the other takes its orders
		// with no face. A step with anger pulls the face on its first turn and again each time the face is due; an order
		// with anger pulls it only on the turn of the order
		const auto showAnger = [entity]() {
			if (Locator::creatureMindSystem::has_value())
			{
				Locator::creatureMindSystem::value().ShowFeeling(entity, creature_face::Cue::Anger);
			}
		};
		const auto angerOnOrder = [fighting, &showAnger]() {
			showAnger();
			fighting->angerShown = false;
		};
		const auto angerStarts = [fighting, &showAnger]() {
			showAnger();
			fighting->angerShown = true;
			fighting->faceSeconds = creature_mind::k_FaceRepeatSeconds;
		};
		const auto angerTurn = [fighting, &showAnger]() {
			if (!fighting->angerShown)
			{
				return;
			}
			fighting->faceSeconds -= k_TurnSeconds;
			if (fighting->faceSeconds < k_FaceDue)
			{
				showAnger();
				fighting->faceSeconds = creature_mind::k_FaceRepeatSeconds;
			}
		};

		using Phase = creature_turn_to_face::Turn::Phase;
		switch (fighting->stage)
		{
		case CreatureFighting::Stage::FaceOff:
		{
			++fighting->faceOffTurns;
			// The first creature asks the other creature, tells its mind to stand still, then tells it to turn to face it,
			// and on the next turn turns to face the other creature itself. It does not wait for the other's turn: on the
			// turn after its own turn is done, it tells the other creature to taunt and taunts itself from the next turn
			if (starter)
			{
				// The request and the order to turn show anger, the order to stand still does not; its own turn shows
				// anger until it is done
				if (fighting->faceOffTurns == fight::k_FaceOffRequestTurn ||
				    fighting->faceOffTurns == fight::k_FaceOffOrderTurn)
				{
					angerOnOrder();
				}
				else if (fighting->faceOffTurns == fight::k_FaceOffOwnTurn)
				{
					angerStarts();
				}
				else if (fighting->faceOffTurns > fight::k_FaceOffOwnTurn && fighting->faceTurn.phase != Phase::Done)
				{
					angerTurn();
				}
				else
				{
					fighting->angerShown = false;
				}
				auto& other = registry.Get<CreatureFighting>(opponent);
				if (fighting->faceOffTurns == fight::k_FaceOffOrderTurn && other.stage == CreatureFighting::Stage::FaceOff &&
				    other.faceTurn.phase == Phase::Idle)
				{
					// Its first order: the other creature stops what it does and obeys, its mind told so
					if (!other.ordered)
					{
						other.ordered = true;
						StopForTheFight(registry, opponent);
						if (Locator::creatureMindSystem::has_value())
						{
							Locator::creatureMindSystem::value().FightOrdered(opponent, entity);
						}
					}
					other.faceTurn.phase = Phase::Start;
				}
				if (fighting->faceOffTurns == fight::k_FaceOffOwnTurn)
				{
					fighting->faceTurn.phase = Phase::Start;
				}
				else if (fighting->faceTurn.phase == Phase::Done)
				{
					order(opponent, CreatureFighting::Stage::Taunt);
					next(CreatureFighting::Stage::Taunt);
					break;
				}
			}
			FaceOpponentTurn(registry, entity, *fighting, locomotion);
			break;
		}
		case CreatureFighting::Stage::Taunt:
			if (!fighting->played)
			{
				// Each try to start the taunt draws whether it is mirrored first. A body that plays an action or moves, or
				// that has no animation for it, refuses the start, and it tries again on the next turn. With no bodies
				// loaded, nothing plays
				const auto played = fight::FaceOffAnimation(fighting->rude);
				const bool mirrored = game_random::GameRand(2) != 0;
				const bool loaded = Locator::creatureAnimationSystem::has_value();
				if (creature_layers::IsPlaying(animation.body) || moving(entity) || (loaded && !Has(entity, played)))
				{
					break;
				}
				fighting->played = true;
				fighting->taunted = loaded;
				if (loaded)
				{
					PlayOnce(animation, played, mirrored);
				}
			}
			else if (!creature_layers::IsPlaying(animation.body))
			{
				// The other creature waits for its next order. At the end of the first creature's taunt the arena
				// appears; with no arena the fight stops
				if (!starter)
				{
					next(CreatureFighting::Stage::Ready);
				}
				else if (TakeArena(entity, opponent))
				{
					next(CreatureFighting::Stage::Point);
				}
				else
				{
					Leave(opponent);
					Leave(entity);
				}
			}
			break;
		case CreatureFighting::Stage::Point:
		{
			auto* hands =
			    Locator::creatureObjectActionSystem::has_value() ? &Locator::creatureObjectActionSystem::value() : nullptr;
			if (!fighting->played)
			{
				// It points at the middle of the arena on the ground once its body is free, for a set number of turns
				if (BodyBusy(registry, entity, locomotion))
				{
					break;
				}
				fighting->played = true;
				fighting->pointTurns = fight::k_PointAtArenaTurns;
				if (hands != nullptr)
				{
					const auto centre = fighting->arena.centre;
					hands->PointUntilStopped(entity, {centre.x, GroundAt(centre), centre.y});
				}
			}
			else if (--fighting->pointTurns == 0)
			{
				if (hands != nullptr)
				{
					hands->StopPointing(entity);
				}
				// The order to walk to the arena shows anger
				angerOnOrder();
				order(opponent, CreatureFighting::Stage::Approach);
				next(CreatureFighting::Stage::Approach);
			}
			break;
		}
		case CreatureFighting::Stage::Approach:
			// The walk of the first creature to its place shows anger
			if (starter)
			{
				if (fighting->played)
				{
					angerTurn();
				}
				else
				{
					angerStarts();
				}
			}
			if (!fighting->played)
			{
				fighting->played = true;
				if (locomotion != nullptr)
				{
					// by the size it is drawn at (the pen's in its temple's pen), kept within its limits
					const auto drawn = creature_morph::ClampScale(ecs::creature_pose::DrawnSize(registry, entity));
					const auto spot = fight::ArenaSpot(fighting->arena, drawn, fighting->madeArena);
					locomotion->MoveTo(entity, spot, Pace::Walk, 0.0f, fight::ArrivalDistance(drawn));
				}
			}
			// The walk has no time limit: it ends when the body stops
			else if (!moving(entity))
			{
				next(CreatureFighting::Stage::Ready);
			}
			break;
		case CreatureFighting::Stage::Ready:
			// The other creature waits for its next order; the first creature, at its place, waits for the other to arrive
			if (!starter)
			{
				break;
			}
			// The wait shows anger; in this stage the played flag shows that the wait started
			if (fighting->played)
			{
				angerTurn();
			}
			else
			{
				fighting->played = true;
				angerStarts();
			}
			// With the other creature ready, it turns to face it in the arena from the next turn. The wait has no time
			// limit
			if (partner->stage == CreatureFighting::Stage::Ready)
			{
				next(CreatureFighting::Stage::Square);
				fighting->faceOffTurns = 0;
				fighting->faceTurn = {};
			}
			break;
		case CreatureFighting::Stage::Square:
		{
			// The first creature tells the other creature to turn to face it, then turns to face the other creature. It
			// does not wait for the other's turn: on the turn after its own turn is done, it tells the other creature to
			// start its duel, and tries to start its own from the next turn. In this stage the played flag shows that the
			// creature has its order to start its duel
			if (starter)
			{
				++fighting->faceOffTurns;
				// The order to turn, its own turn, the order to start the duel and its own duel show anger
				if (fighting->faceOffTurns == fight::k_SquareOrderTurn)
				{
					angerOnOrder();
				}
				else if (fighting->faceOffTurns == fight::k_SquareOwnTurn || (fighting->played && !fighting->angerShown))
				{
					angerStarts();
				}
				else if (!fighting->played && fighting->faceTurn.phase == Phase::Done)
				{
					angerOnOrder();
				}
				else
				{
					angerTurn();
				}
				if (fighting->faceOffTurns == fight::k_SquareOrderTurn &&
				    registry.Get<const CreatureFighting>(opponent).stage == CreatureFighting::Stage::Ready)
				{
					order(opponent, CreatureFighting::Stage::Square);
					registry.Get<CreatureFighting>(opponent).faceTurn = {.phase = Phase::Start};
				}
				if (fighting->faceOffTurns == fight::k_SquareOwnTurn)
				{
					fighting->faceTurn = {.phase = Phase::Start};
				}
				else if (!fighting->played && fighting->faceTurn.phase == Phase::Done)
				{
					registry.Get<CreatureFighting>(opponent).played = true;
					fighting->played = true;
					break;
				}
			}
			// Told to start its duel, it starts it when its body lets it
			if (fighting->played)
			{
				if (CanStartDuel(registry, entity, locomotion))
				{
					BeginDuel(entity);
				}
				break;
			}
			FaceOpponentTurn(registry, entity, *fighting, locomotion);
			break;
		}
		case CreatureFighting::Stage::Duel:
			break;
		case CreatureFighting::Stage::Celebrate:
			// Its finish played, it leaves the fight; its mind then carries out what the win made it do to the loser
			if (fighting->fighter.state == fight::State::Idle)
			{
				Leave(entity);
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
				fighting->responded = true;
				const auto life = LifeOf(registry, entity);
				const auto otherLife = opponentHere ? LifeOf(registry, opponent) : 0.0f;
				PlayOnce(animation, fight::Response(life, otherLife));
			}
			else if (!creature_layers::IsPlaying(animation.body))
			{
				CountFightPerformed(registry, entity, *fighting);
				// Its response over, left with no more life than its opponent it is made afraid of it: the fight's plan
				// fails and it plans its fear of the opponent at once, choosing what to do itself
				const bool afraid = fighting->responded && opponentHere &&
				                    !fight::FightPerformed(LifeOf(registry, entity), LifeOf(registry, opponent));
				Leave(entity);
				if (afraid && Locator::creatureMindSystem::has_value())
				{
					auto& minds = Locator::creatureMindSystem::value();
					// Its plan failing fails its opponent's too: an opponent still responding to it stops, and so, of two
					// left with the same life, only the first to finish its response is afraid
					if (const auto* other = registry.TryGet<const CreatureFighting>(opponent);
					    other != nullptr && other->opponent == entity && other->stage == CreatureFighting::Stage::Respond)
					{
						Leave(opponent);
						minds.AbandonAction(opponent);
					}
					static_cast<void>(minds.ForceDesireOn(entity, creature_desires::Desire::Fear, opponent));
				}
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
	// It first turns on the spot to the way to its opponent, however small the turn, before its start animation plays.
	// Right on top of it, the way is heading 0.
	const auto opponent = fighting.opponent;
	if (Locator::creatureLocomotionSystem::has_value() && registry.Valid(opponent) &&
	    std::as_const(registry).AllOf<Transform>(opponent) && std::as_const(registry).AllOf<Transform>(creature))
	{
		const auto towards =
		    Flat(registry.Get<const Transform>(opponent).position) - Flat(registry.Get<const Transform>(creature).position);
		fighting.fighter.turning = Locator::creatureLocomotionSystem::value().TurnToHeading(
		    creature, creature_locomotion::HeadingFromDirection(towards));
	}

	// Each fighter's duel start makes a fight reaction about it, for its player and started at once, which those who see
	// it may take notice of, counts one more battle fought, and tells the player's camera of the fight in its arena
	const bool madeArena = fighting.madeArena;
	const auto arena = fighting.arena;
	const auto owner = std::as_const(registry).Get<const Creature>(creature).owner;
	if (Locator::reactionSystem::has_value())
	{
		static_cast<void>(ecs::effects::reactions::CreateReaction(creature, openblack::Reaction::ReactToFight, owner, true));
	}
	if (auto* record = registry.TryGet<CreatureFightRecord>(creature))
	{
		++record->fights;
	}
	if (registry.Valid(opponent) && std::as_const(registry).AllOf<Creature>(opponent))
	{
		// The camera is told of the arena's maker first
		WatchFightStart(madeArena ? creature : opponent, madeArena ? opponent : creature, arena);
	}
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
		const auto rootMove = DisplacementOf(body.species, animation).y;
		const auto rootAhead = -rootMove * scale;
		fighting.reaches.push_back(fight::Reach {.animation = animation,
		                                         .reach = (-point->z * scale) + (rootAhead * time / duration),
		                                         .height = point->y * scale,
		                                         .point = {point->x, point->y, point->z + (time * rootMove / duration)}});
		fighting.hitTimesMs.at(i) = time;
		const auto before =
		    animations.BoneInAnimation(creature, animation, time - static_cast<float>(k_EarliestHitMs), attack.bone, false);
		fighting.hitMoves.at(i) = before.has_value() ? (*point - *before) * k_HitMoveScale : glm::vec3(0.0f);
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
	for (const auto entity : duelling)
	{
		ProcessDuelOf(entity);
	}
}

void CreatureFightSystem::ProcessDuelOf(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const fight::Random random = &GameRandom;
	{
		auto* fighting = registry.TryGet<CreatureFighting>(entity);
		if (fighting == nullptr || fighting->stage != CreatureFighting::Stage::Duel)
		{
			return;
		}
		TraceFightStage(registry, entity, *fighting, "duel");
		// Its opponent gone (a stop of the opponent's in its own duel clears it), it finishes; for the player's creature
		// the advisors remark it went badly and the player's camera is told so. An opponent whose own side of the fight
		// ended is fought on
		const auto opponent = fighting->opponent;
		if (!OpponentStartsDuel(registry, entity, *fighting) && !HasItsOpponent(registry, opponent))
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
			// The first creature's duel over, it tells the other creature to respond to the fight, its mind told so
			if (fighting->madeArena && registry.Valid(opponent) && registry.AllOf<CreatureFighting>(opponent) &&
			    Locator::creatureMindSystem::has_value())
			{
				Locator::creatureMindSystem::value().FightOrdered(opponent, entity);
			}
			return;
		}
		auto& fighter = fighting->fighter;
		fighter.stamina = fight::RegainStamina(fighter.stamina);
		const auto& other = registry.Get<const CreatureFighting>(opponent).fighter;

		// Left alone long enough, the computer takes over the player's creature: the wait counts the milliseconds the
		// creature lives through in the turn, fewer while it is frozen
		const auto* animation = std::as_const(registry).TryGet<const CreatureAnimation>(entity);
		fight::CountComputerWait(
		    fighter, static_cast<float>(creature_turn_pose::LivedMs(animation != nullptr ? animation->playbackScale : 1.0f,
		                                                            static_cast<int32_t>(game_clock::MsPerTurn()))));
		const bool computer = fighter.autoFight || fighter.control == fight::Control::Computer;
		// The arena's keep-in rule: with either of the two at or past its edge fraction, a block ends and from its
		// stance it steps back towards the middle, and it takes no move from the queue and chooses none this turn
		if ((fighter.state == fight::State::Stance || fighter.state == fight::State::Block) &&
		    !fight::BothKeptIn(fighting->arena, Flat(registry.Get<const Transform>(entity).position),
		                       Flat(registry.Get<const Transform>(opponent).position)))
		{
			std::string outcome = "stays";
			if (fighter.state == fight::State::Block)
			{
				fight::Enter(fighter, fight::State::BlockEnd);
				outcome = "ends its block";
			}
			else if (const auto* locomotion = registry.TryGet<const CreatureLocomotion>(entity))
			{
				const glm::vec3 middle {fighting->arena.centre.x, GroundAt(fighting->arena.centre), fighting->arena.centre.y};
				if (const auto step = fight::KeepInStep(middle, registry.Get<const Transform>(entity).position,
				                                        locomotion->heading, ecs::object::Get2DRadius(entity)))
				{
					// A step the rule makes is taken wherever the creature stands
					fight::Enter(fighter, fight::State::Action, fight::StepAnimation(*step));
					outcome = "steps " + std::to_string(fight::StepAnimation(*step)) + " towards the middle";
				}
			}
			if (const auto logger = spdlog::get("game"))
			{
				const auto& centre = fighting->arena.centre;
				logger->trace("Fight: creature {} keeps in: {:.1f} and its opponent {:.1f} from the arena's centre ({:.1f}, "
				              "{:.1f}), limit {:.1f}: {}",
				              entt::to_integral(entity),
				              glm::distance(Flat(registry.Get<const Transform>(entity).position), centre),
				              glm::distance(Flat(registry.Get<const Transform>(opponent).position), centre), centre.x, centre.y,
				              fight::k_RangeShare * fighting->arena.radius, outcome);
			}
			return;
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
	const auto scale = DrawnScaleOf(registry, creature);
	const auto opponentHeight = DrawnHeightOf(registry, opponent);
	const auto headHeight = HeadHeightOf(registry, opponent).value_or(opponentHeight);
	// Lines from where each blow lands, towards the opponent, find how far its body is ahead of the blow. The body is
	// placed as the blow's test places it: where its turn takes it, facing its turn's heading, leaned onto the slope it
	// stands on there
	const auto& locomotion = registry.Get<const CreatureLocomotion>(creature);
	const auto* opponentMoves = registry.TryGet<const CreatureLocomotion>(opponent);
	const auto opponentPosition =
	    opponentMoves != nullptr ? opponentMoves->toPosition : registry.Get<const Transform>(opponent).position;
	auto towards = opponentPosition - locomotion.toPosition;
	if (towards != glm::vec3(0.0f))
	{
		towards *= 1.0f / game_math::x87::LengthZyx(towards);
	}
	const auto placement =
	    ecs::creature_pose::LeanedModel(registry, creature, locomotion.toPosition, affine::AngleY(-locomotion.toHeading),
	                                    ecs::creature_pose::DrawnScale(registry, creature));
	const auto bones = BonesOf(registry, opponent);
	const auto opponentSize = ecs::creature_pose::DrawnSize(registry, opponent);
	// Without bones to meet (no body loaded), the opponent's walking radius stands for its body
	const auto opponentRadius = opponentMoves != nullptr ? opponentMoves->radius : 5.0f;
	const auto gap = glm::distance(selfAt, Flat(registry.Get<const Transform>(opponent).position)) - opponentRadius;
	std::vector<fight::BlowDepth> blows;
	blows.reserve(fighting.reaches.size());
	for (const auto& reach : fighting.reaches)
	{
		fight::BlowDepth blow {.animation = reach.animation, .height = reach.height, .depth = std::nullopt};
		if (reach.animation != fight::animations::k_AttackSpecial && reach.height <= headHeight)
		{
			if (bones.empty())
			{
				blow.depth = gap - reach.reach;
			}
			else
			{
				const auto point = glm::vec3(placement * glm::vec4(0.0f, reach.point.y, reach.point.z, 1.0f));
				blow.depth = fight::DepthToBody(bones, point, towards, opponentSize);
			}
		}
		blows.push_back(blow);
	}
	// Each step carries the creature as far as its animation's file says, at the size it is drawn
	const auto strideOf = [&body, scale](size_t animation) {
		const auto* rig = RigOf(body.species);
		const auto* played = rig != nullptr ? rig->GetAnimation(CreatureRig::Mesh::Base, animation) : nullptr;
		return played != nullptr ? played->stride * scale : 0.0f;
	};
	const auto steps = fight::StepLengths {.forward = strideOf(fight::animations::k_StepForward),
	                                       .back = strideOf(fight::animations::k_StepBack)};
	const auto choice = fight::ChooseAttack(blows, band, headHeight, steps, ecs::creature_pose::DrawnSize(registry, creature));
	// a trace at run time (the game channel at trace level), not compiled out of release builds
	if (const auto logger = spdlog::get("game"))
	{
		float nearest = fight::k_NoDepth;
		for (const auto& blow : blows)
		{
			nearest = std::min(nearest, blow.depth.value_or(fight::k_NoDepth));
		}
		logger->trace("Fight: creature {} tries a blow: nearest depth {:.2f}, steps {:.2f} forward {:.2f} back, {} reaches, "
		              "size {:.2f}, opponent's head {:.2f} -> blow {}, step {}",
		              entt::to_integral(creature), nearest, steps.forward, steps.back, fighting.reaches.size(),
		              ecs::creature_pose::DrawnSize(registry, creature), headHeight,
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
		    // turning on the spot in its start, the turn's own animation plays
		    if (fighting.fighter.state != fight::State::Idle && !fighting.fighter.turning)
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
	// an arena whose fighters went without leaving their fight goes too
	ReleaseArenaThings();
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
	if (fighting == nullptr || !lookup.AllOf<CreatureAnimation>(creature) || fighting->fighter.state == fight::State::Idle ||
	    fighting->fighter.turning)
	{
		return;
	}
	inputs.body = BodyAt(*fighting, ClockDurationOf(creature, fighting->fighter), std::clamp(turnFraction, 0.0f, 1.0f));
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
	_fightersMoved = false;
	for (const auto entity : fighters)
	{
		StepFighter(entity, milliseconds);
	}
	if (std::exchange(_fightersMoved, false))
	{
		registry.SetDirty();
	}
}

void CreatureFightSystem::StepFighter(entt::entity entity, float milliseconds)
{
	auto& registry = Locator::entitiesRegistry::value();
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
			return;
		}
		auto& fighter = fighting->fighter;
		const auto& body = registry.Get<const Creature>(entity);
		// An opponent about to start its duel is in the fight
		const bool hasOpponent =
		    fighting->stage == CreatureFighting::Stage::Duel &&
		    (HasItsOpponent(registry, fighting->opponent) || OpponentStartsDuel(registry, entity, *fighting));
		// In its start, while it turns on the spot to its opponent the turn's own animation plays and nothing else moves
		// it; the turn over, its start animation plays from its beginning this same turn
		if (fight::WaitsForStartTurn(fighter, Locator::creatureLocomotionSystem::has_value() &&
		                                          Locator::creatureLocomotionSystem::value().IsMoving(entity)))
		{
			return;
		}
		// A forced finish doubles only the span of the turn; the fight speed's advances keep the turn's milliseconds
		auto spanMs = fight::TestSpanMs(milliseconds, ecs::creature_pose::DrawnSize(registry, entity), false);
		if (fighter.hurried)
		{
			spanMs = static_cast<int32_t>(static_cast<float>(spanMs) * fight::k_HurriedPace);
		}
		auto duration = ClockDurationOf(entity, fighter);
		// The start, the finish, the start and end of a block or a cast, a move and a block's reel test their end before
		// their animation moves on; a fighter called off leaves its stance, its block or its cast at once. The next state
		// then moves on in the same turn.
		bool entered = false;
		{
			const auto ending = fighter.state;
			bool ends = false;
			if (ending == fight::State::Cast)
			{
				// The cast is held by the update, not by its animation: past the hold it lets go and ends the cast
				ends = fight::HoldFightCast(fighter, milliseconds);
			}
			else if (fight::EndsByOwnTest(ending))
			{
				const auto advanceMs = fight::TurnAdvanceMs(milliseconds, fighter.animation);
				ends = fight::OwnTestEnds(fighter.timeMs, fight::EndTestMs(ending, advanceMs, spanMs), duration);
			}
			else if (fight::EndsAsItsTurnBegins(ending) && duration > 0.0f)
			{
				// A blow, a step, a reel and a block's reel end once their time and the turn's advance reach their end,
				// before they move on, and are never called off
				ends = fight::OwnTestEnds(fighter.timeMs, fight::TurnAdvanceMs(milliseconds, fighter.animation), duration);
			}
			auto next = fighter.stopping ? fight::StopTransition(ending, ends) : std::nullopt;
			if (!next.has_value() && ends)
			{
				next = fight::AfterAnimation(ending, hasOpponent);
			}
			if (next.has_value())
			{
				if (ending == fight::State::Cast && Locator::magicSystem::has_value())
				{
					Locator::magicSystem::value().ReleaseCreatureCast(entity);
				}
				// A cast started by a fighter called off is never cast
				if (fighter.stopping)
				{
					fighter.spell.reset();
				}
				else if (ending == fight::State::CastStart && *next == fight::State::Cast)
				{
					CastFightSpell(entity, *fighting, hasOpponent);
					fighter.castMs = 0;
				}
				fight::Enter(fighter, *next);
				if (*next == fight::State::Idle)
				{
					fighting->drawFromMs = 0.0f;
					animation->body = {};
					ecs::creature_pose::CommitTurnPose(entity, locomotion->toPosition, locomotion->toHeading);
					return;
				}
				// Back in its stance from a blow, a step, a reel, a block or a cast, the player's creature takes its queued
				// move at once, and that move plays on in this same turn
				if (fight::ChecksQueueAsItEnds(ending, *next, fighter.autoFight || fighter.control == fight::Control::Computer))
				{
					CheckQueue(entity);
				}
				entered = true;
				duration = ClockDurationOf(entity, fighter);
			}
		}
		const auto before = fighter.timeMs;
		fighting->drawFromMs = before;
		// The stance's and the block's loops come round here
		bool cameRound = false;
		if (fight::MovesOnInWholeMs(fighter.state))
		{
			// Its animation goes round once at most, in whole milliseconds; the state ends only by its own test, or a
			// loop with no opponent once it comes round
			const auto advanceMs =
			    fight::StateAdvanceMs(fighter.state, fight::TurnAdvanceMs(milliseconds, fighter.animation), spanMs);
			// The power-up goes round from its start when the stance's time is past its end
			fighter.timeMs = static_cast<float>(
			    fight::StanceLoopFrom(fighter.state == fight::State::Stance && fighter.queue.AwaitsRelease() &&
			                              Has(entity, fight::animations::k_PowerUp),
			                          static_cast<int32_t>(fighter.timeMs), static_cast<int32_t>(duration)));
			cameRound = duration > 0.0f && static_cast<int32_t>(fighter.timeMs) + advanceMs >= static_cast<int32_t>(duration);
			fighter.timeMs = duration > 0.0f
			                     ? static_cast<float>(creature_layers::AdvanceLooping(static_cast<int32_t>(fighter.timeMs),
			                                                                          advanceMs, static_cast<int32_t>(duration))
			                                              .timeMs)
			                     : 0.0f;
		}
		else if (fighter.state == fight::State::Faint || fighter.state == fight::State::GetUp)
		{
			// The fall and the getting up play at the creature's own pace for its size, as its other actions do, not at
			// the fight's: the turn's whole step
			fighter.timeMs += static_cast<float>(spanMs);
		}
		else
		{
			fighter.timeMs = fight::AdvanceClock(fighter.timeMs, milliseconds, fighter.animation);
		}

		// It faces its opponent where the opponent is now. Right on top of it, it faces heading 0
		const auto faceOpponent = [&]() {
			const auto towards =
			    Flat(registry.Get<const Transform>(fighting->opponent).position) - Flat(locomotion->toPosition);
			FaceHeading(*locomotion, creature_locomotion::HeadingFromDirection(towards));
			_fightersMoved = true;
		};
		// With its opponent swinging no blow, the two are pulled apart along the way it faces when they stand nearer than
		// both their reaches ahead; each is moved only to a place strictly inside the range. In the stance only while the
		// keep-in rule lets the stance go on, both strictly inside the range; in a step or a reel whatever the range
		const auto pullApart = [&](bool keptInFirst) {
			auto* otherMoves = registry.TryGet<CreatureLocomotion>(fighting->opponent);
			const auto* otherFighting = registry.TryGet<const CreatureFighting>(fighting->opponent);
			if (otherMoves != nullptr && otherFighting != nullptr && !fight::IsAttack(otherFighting->fighter.animation) &&
			    (!keptInFirst ||
			     fight::BothKeptIn(fighting->arena, Flat(locomotion->toPosition), Flat(otherMoves->toPosition))))
			{
				const auto push = fight::PullApartOf(Flat(locomotion->toPosition), Flat(otherMoves->toPosition),
				                                     creature_locomotion::DirectionOf(locomotion->heading),
				                                     ForwardExtentOf(registry, entity, fighter),
				                                     ForwardExtentOf(registry, fighting->opponent, otherFighting->fighter));
				if (push.has_value())
				{
					if (fight::KeptIn(fighting->arena, Flat(locomotion->toPosition) - *push))
					{
						Shift(*locomotion, -*push);
					}
					if (fight::KeptIn(fighting->arena, Flat(otherMoves->toPosition) + *push))
					{
						Shift(*otherMoves, *push);
						ecs::creature_pose::CommitTurnPose(fighting->opponent, otherMoves->toPosition, otherMoves->toHeading);
					}
					_fightersMoved = true;
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
			pullApart(true);
		}

		// Each turn an action carries the creature by its animation's root travel at the turn's share, wherever that
		// takes it; a block's recoil only to a place inside the range
		if ((fighter.state == fight::State::Action || fighter.state == fight::State::BlockRecoil) && duration > 0.0f)
		{
			auto by = fight::TurnCarry(fighter.state, DisplacementOf(body.species, fighter.animation),
			                           fight::PlaybackSpeed(fighter.animation), static_cast<int32_t>(milliseconds), spanMs,
			                           DrawnScaleOf(registry, entity), static_cast<int32_t>(duration), locomotion->heading);
			const auto at = Flat(locomotion->toPosition);
			const auto to = fight::ActionMoveTo(fighting->arena, fighter.state, at, by);
			// Carried all the way: the stance's pull-apart and a blow's push keep the two apart
			by = to - at;
			if (glm::length(by) > 0.0f)
			{
				Shift(*locomotion, by);
				_fightersMoved = true;
			}
		}
		// In a step or a reel, once carried, it faces its opponent and the two are pulled apart
		if (hasOpponent && fighter.state == fight::State::Action &&
		    fight::FacesOpponentEachTurn(fighter.state, fighter.animation) && registry.Valid(fighting->opponent))
		{
			faceOpponent();
			pullApart(false);
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
				return;
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
					_fightersMoved = true;
				}
			}
			locomotion = moves != nullptr ? moves : locomotion;
		}
		auto& current = fighting->fighter;
		// Any other state moves on once its animation has played through, a loop once it came round, and a move with no
		// animation after its turn
		if (!entered && !fight::EndsByOwnTest(current.state) &&
		    (duration <= 0.0f || current.timeMs >= duration || (cameRound && fight::Loops(current.state))))
		{
			const auto next = fight::AfterAnimation(current.state, hasOpponent);
			if (next == current.state && fight::Loops(current.state))
			{
				current.timeMs =
				    current.state == fight::State::Lying || duration <= 0.0f ? duration : std::fmod(current.timeMs, duration);
			}
			else
			{
				fight::Enter(current, next);
				if (next == fight::State::Idle)
				{
					animation->body = {};
					ecs::creature_pose::CommitTurnPose(entity, locomotion->toPosition, locomotion->toHeading);
					return;
				}
			}
		}
		// A new animation, or a loop come round, is drawn from its start
		if (fighting->drawFromMs > current.timeMs)
		{
			fighting->drawFromMs = 0.0f;
		}
		Show(*animation, current, current.queue.AwaitsRelease(), ClockDurationOf(entity, current), current.timeMs);
		// The Transform moves once a turn, to where the turn ends
		ecs::creature_pose::CommitTurnPose(entity, locomotion->toPosition, locomotion->toHeading);
	}
}

void CreatureFightSystem::TestHit(entt::entity creature, float fromMs, float turnMs)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& fighting = registry.Get<CreatureFighting>(creature);
	const auto opponent = fighting.opponent;
	if (!HasItsOpponent(registry, opponent))
	{
		if (const auto logger = spdlog::get("game"))
		{
			logger->trace("Fight: creature {}'s blow is not tested: it has no opponent", entt::to_integral(creature));
		}
		return;
	}
	const auto& body = registry.Get<const Creature>(creature);
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
	// The bones are placed where the turn's move left the body, leaned onto the slope it stands on there, as the body
	// is posed for the turn
	const auto placement =
	    ecs::creature_pose::LeanedModel(registry, creature, locomotion.toPosition, affine::AngleY(-locomotion.toHeading),
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
	// How the blow lands comes from the blow as the creature's file measures it, not from where the line met the body
	const auto found = std::ranges::find_if(
	    fighting.reaches, [&attacker](const fight::Reach& reach) { return reach.animation == attacker.animation; });
	if (found == fighting.reaches.end())
	{
		return;
	}

	const bool blocked = fight::IsBlocking(victim.state, victim.timeMs, DurationOf(opponent, fight::animations::k_StartBlock));
	// The band goes by how high the striking bone stands in the attacker's body against the victim's head over the land
	// (a victim not posed yet, as in a test's world, by its drawn height), the way it reels by how that bone moves
	const auto heightShare = found->height / HeadHeightOf(registry, opponent).value_or(opponentHeight);
	const auto direction = fight::RecoilDirectionOf(fighting.hitMoves.at(blow));
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
	if (const auto logger = spdlog::get("game"))
	{
		logger->trace("Fight: creature {}'s blow lands on {}: animation {}, special {}, damage {:.4f}, fight health {:.4f}, "
		              "blocked {}, reel {}",
		              entt::to_integral(creature), entt::to_integral(opponent), attacker.animation, attacker.special,
		              result.damage, victim.health, blocked, Has(opponent, result.recoil) ? result.recoil : result.plainRecoil);
	}
	// The victim keeps the turn it was struck, which the villagers watching cheer or despair at
	{
		auto& struck = registry.AllOf<CreatureFightRecord>(opponent) ? registry.Get<CreatureFightRecord>(opponent)
		                                                             : registry.AssignState<CreatureFightRecord>(opponent);
		struck.struckTurn = game_clock::Turn();
	}
	// A landed blow plays no wobble on top of the victim: its sway animations move only with the forces on the body
	// Every landed blow, blocked or not, cuts the victim's skin where it landed, along the way the striking bone moved,
	// and some wounds bleed. The column is drawn first, then the kind, before the cut is looked for on the body
	{
		const fight::Random random = &GameRandom;
		const auto column = static_cast<uint8_t>(game_random::GameRand(creature_marks::k_CellsPerRow));
		const auto type = fight::WoundKind(rig->attackPoints.at(blow).cutType, random);
		// then what flies from where it landed: the victim's blood, unless it blocks, and the sparks
		if (Locator::dynamicsSystem::has_value())
		{
			const fight::LocalRandom local = [](int32_t n) { return static_cast<int32_t>(game_random::LocalRand(n)); };
			for (const auto& spray : fight::BlowSprays(landed->move, attackerSize, defenderSize, blocked, local))
			{
				Locator::dynamicsSystem::value().AddPuff(landed->point, spray.velocity, spray.size, spray.argb,
				                                         spray.drop ? openblack::physics::turn::PuffKind::Drop
				                                                    : openblack::physics::turn::PuffKind::Dust);
			}
		}
		const auto cut =
		    Locator::creatureSkinSystem::has_value()
		        ? ecs::creature_skin::NearestCut(std::as_const(registry), opponent, landed->point, glm::normalize(landed->move))
		        : std::nullopt;
		if (cut.has_value())
		{
			auto& skins = Locator::creatureSkinSystem::value();
			if (const auto place = ecs::creature_skin::PlaceOf(std::as_const(registry), opponent, cut->spot); place.has_value())
			{
				skins.AddWound(opponent,
				               {.u = place->u, .v = place->v, .skin = place->skin, .age = 0, .type = type, .column = column});
				// A readout for the log (the game channel at debug): the wound the blow left and where
				if (const auto logger = spdlog::get("game"); logger != nullptr && logger->should_log(spdlog::level::debug))
				{
					logger->debug("Fight: creature {}'s blow wounds {}: cut type {}, wound kind {}, column {}, skin {} at "
					              "({}, {}), bleeds {}",
					              entt::to_integral(creature), entt::to_integral(opponent), rig->attackPoints.at(blow).cutType,
					              static_cast<int>(type), static_cast<int>(column), static_cast<int>(place->skin),
					              static_cast<int>(place->u), static_cast<int>(place->v), fight::Bleeds(type));
				}
			}
			// and a wound that bleeds runs a trail of blood from where it was cut
			if (fight::Bleeds(type))
			{
				skins.Bleed(opponent, *cut);
			}
		}
	}

	// The blow is then a hit on the victim, applied by the attacker, as a thing striking it: still fighting, the victim
	// only comes to find the attacker nastier (no more than once in its creatures' shared wait between such changes);
	// with its own side of the fight ended, it is cut and loses life from it as from any hit
	{
		auto values = ecs::effects::FightBlowHit(creature, body.owner);
		static_cast<void>(ecs::effects::ApplyEffect(opponent, values));
	}
	if (!registry.Valid(opponent) || !registry.AllOf<CreatureFighting>(opponent))
	{
		return;
	}
	// what the hit did may have moved the fighters' storage
	auto& struckFighting = registry.Get<CreatureFighting>(opponent);
	auto& struck = struckFighting.fighter;
	// Getting hit takes away the blow added last while it waits to be let go, and makes it let go of a spell it holds;
	// the local player's creature loses the blow's mark with it
	if (struck.queue.TakeWaitingBlow() &&
	    std::as_const(registry).Get<const Creature>(opponent).owner == creature::LocalPlayer())
	{
		struckFighting.marks.DropWaiting();
	}
	if (Locator::magicSystem::has_value())
	{
		Locator::magicSystem::value().ReleaseCreatureCast(opponent);
	}

	if (struck.health <= 0.0f)
	{
		Win(creature, opponent, true);
		return;
	}
	if (blocked)
	{
		fight::Enter(struck, fight::State::BlockRecoil);
	}
	else if (struck.state == fight::State::Stance || struck.state == fight::State::Action ||
	         struck.state == fight::State::CastStart || struck.state == fight::State::Cast ||
	         struck.state == fight::State::CastEnd)
	{
		fight::Enter(struck, fight::State::Action, Has(opponent, result.recoil) ? result.recoil : result.plainRecoil);
	}
}

void CreatureFightSystem::Win(entt::entity winner, entt::entity loser, bool byBlow)
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
	// The winner gives up what it did for the plan the win forces on it, which stops the loser too, then the loser's
	// faint stops it again. Each stop ends the body's fight once more, and the body still in the midst of the fight pays
	// its life for it each time: with the two minds partners, the winner pays twice and the loser four times
	const auto chain =
	    fight::RunStopChain({.duels = {InDuel(registry, winner), InDuel(registry, loser)},
	                         .partners = {MindsPartner(registry, winner, loser), MindsPartner(registry, loser, winner)}},
	                        fight::KnockOutSteps());
	EndFightFor(winner, true, chain.bodyEnds[0]);
	EndFightFor(loser, false, chain.bodyEnds[1]);
	// and each of those failures in the duel gains a fight's strength for the one failing and the one its stop fails:
	// between partners two each, with none one each
	GainFightStrength(winner, chain.gains[0]);
	GainFightStrength(loser, chain.gains[1]);
	if (auto* record = registry.TryGet<CreatureFightRecord>(winner))
	{
		++record->wins;
	}
	const auto* lost = registry.TryGet<const CreatureFighting>(loser);
	const auto start = lost != nullptr ? std::optional(lost->startPosition) : std::nullopt;
	Leave(loser);
	Faint(loser, start);
	// A loser knocked out by a blow takes up no reaction until what it does next is over. Knocked out otherwise, its
	// faint clears that at once, as it stops what it was doing
	if (auto* lost = registry.TryGet<CreatureMindState>(loser); lost != nullptr && byBlow)
	{
		lost->fightEnded = true;
	}
	if (auto* fighting = registry.TryGet<CreatureFighting>(winner))
	{
		fighting->stage = CreatureFighting::Stage::Celebrate;
		fighting->stageSeconds = 0.0f;
		fighting->played = false;
		fight::Enter(fighting->fighter, fight::State::Finish);
		auto& won = registry.Get<CreatureAnimation>(winner);
		won.face = creature_layers::RelaxFace(won.face);
	}
	// The winner is made to do something to the loser: an evil winner has a poo on it, any other shows off at it. The
	// plan has the priority of a plan the winner could choose itself; its mind carries it out once the winner has left
	// the fight. Knocked out by a miracle or a loss of life, the loser gives the winner the plan as an order, so the
	// winner chooses nothing for itself until it is done.
	if (Locator::creatureMindSystem::has_value() && registry.Valid(loser))
	{
		const auto alignment = std::as_const(registry).Get<const Creature>(winner).alignment;
		const std::string_view action = fight::WinnerPoos(alignment) ? "Poo" : "ShowImpressiveAnimation";
		const bool planned =
		    Locator::creatureMindSystem::value().ForceActivity(winner, {.desire = creature_desires::Desire::ObeyPlayer,
		                                                                .activityObject = loser,
		                                                                .action = action,
		                                                                .actionObject = loser,
		                                                                .score = fight::k_WinPlanPriority});
		// Until that plan is over, the winner takes up no reaction
		if (auto* mind = registry.TryGet<CreatureMindState>(winner))
		{
			if (fight::WinPlanCommanded(byBlow))
			{
				mind->commanded = true;
			}
			mind->fightEnded = true;
		}
		if (const auto logger = spdlog::get("game"))
		{
			logger->trace("Fight: creature {} won {}, made to {} creature {}: {}", entt::to_integral(winner),
			              byBlow ? "by a blow" : "otherwise", action, entt::to_integral(loser),
			              planned ? "planned" : "no plan");
		}
	}
}

void CreatureFightSystem::EndFightFor(entt::entity creature, std::optional<bool> won, uint32_t bodyEnds)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = registry.TryGet<CreatureFighting>(creature);
	if (fighting == nullptr || fighting->ended)
	{
		return;
	}
	fighting->ended = true;
	// Only ending in the middle of the fight (starting, standing, in a move, a block or a cast) does anything more:
	// whoever's creature it is, the player's camera is told the fight is over, and a quarter of the fight health it lost
	// comes off its life, set as any life is (an indestructible creature keeps its life against one of a hundredth or
	// less). Ending in any other state of the fight, such as reeling or finishing, costs it nothing. A body that ends its
	// fight several times over in one stop, its state unchanged, pays each time
	if (fight::CameraLingersOnEnd(fighting->fighter.state) && bodyEnds > 0)
	{
		TellCameraFightIsOver();
		if (registry.AllOf<CreatureNeeds>(creature))
		{
			for (uint32_t end = 0; end < bodyEnds; ++end)
			{
				ecs::life::SetLife(creature, fight::LifeAfterFight(ecs::life::LifeOf(creature), fighting->fighter.health));
			}
		}
	}
	auto& record = registry.AllOf<CreatureFightRecord>(creature) ? registry.Get<CreatureFightRecord>(creature)
	                                                             : registry.AssignState<CreatureFightRecord>(creature);
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
	TraceFightStage(registry, creature, *fighting, "leave");
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
	ReleaseArenaThings();
}

void CreatureFightSystem::ReleaseArenaThings()
{
	// An arena a fight made goes, with its reaction, once its maker has left that fight: the fight's own arena is written
	// with its maker's fight
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> unheld;
	std::as_const(registry).Each<const CreatureFightArena>(
	    [&registry, &unheld](entt::entity thing, const CreatureFightArena& made) {
		    const auto* fighting =
		        registry.Valid(made.maker) ? std::as_const(registry).TryGet<const CreatureFighting>(made.maker) : nullptr;
		    if (fighting == nullptr || fighting->arenaThing != thing)
		    {
			    unheld.push_back(thing);
		    }
	    });
	for (const auto thing : unheld)
	{
		if (Locator::reactionSystem::has_value())
		{
			Locator::reactionSystem::value().RemoveFrom(thing);
		}
		registry.Destroy(thing);
	}
}

void CreatureFightSystem::ProcessKnockedOut()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> lying;
	std::as_const(registry).Each<const CreatureKnockedOut>(
	    [&lying](entt::entity entity, const CreatureKnockedOut& /*knockedOut*/) { lying.push_back(entity); });
	for (const auto entity : lying)
	{
		ProcessKnockedOutOf(entity);
	}
}

void CreatureFightSystem::ProcessKnockedOutOf(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
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
		// One turn of the wait at home: its turns are laid as it starts
		const auto countHomeWait = [&] {
			if (!knockedOut.homeWaitTurns.has_value())
			{
				knockedOut.homeWaitTurns = game_clock::TicksForSeconds(fight::k_WaitAfterHomeSeconds);
			}
			const bool liesDead = animation.body.holdLoop && animation.body.phase == creature_layers::BodyAction::Phase::Loop;
			const auto* locomotion =
			    Locator::creatureLocomotionSystem::has_value() ? &Locator::creatureLocomotionSystem::value() : nullptr;
			knockedOut.homeWaited =
			    fight::CountHomeWait(*knockedOut.homeWaitTurns, BodyBusy(registry, entity, locomotion), liesDead);
		};
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
			// A faint a load left at its start starts again: it stops, the side it falls to is drawn, it falls from the
			// start, and those near it react as to a death
			if (std::exchange(knockedOut.restartFaint, false))
			{
				if (Locator::creatureLocomotionSystem::has_value())
				{
					Locator::creatureLocomotionSystem::value().Stop(entity);
				}
				StartFaintPose(entity, game_random::GameRand(2) != 0);
				if (Locator::reactionSystem::has_value())
				{
					static_cast<void>(ecs::effects::reactions::CreateReaction(entity, openblack::Reaction::ReactToDeath,
					                                                          std::nullopt, false));
				}
			}
			// Its faint is over on the first turn it is seen lying, and from the next it lies its turns; a knock-out
			// with no turns laid lies the seconds of the size it is drawn at from the faint
			bool lain =
			    knockedOut.seconds >= fight::FaintSeconds(ecs::creature_pose::DrawnSize(std::as_const(registry), entity));
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
					// Not carried home, its wait at home counts from this turn, the one after its lie ended
					next(CreatureKnockedOut::Stage::Waiting);
					countHomeWait();
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
					// The move resets its look and ends its faint: its head straight and still, looking nowhere, its face
					// relaxed, and its body standing at home with no action; its eyes stay as they are
					animation.yaw = {};
					animation.pitch = {};
					animation.lookAt.reset();
					animation.face = {};
					animation.body = {};
					if (auto* eyes = registry.TryGet<CreatureEyes>(entity))
					{
						eyes->lookAt.reset();
					}
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
			// It waits its turns at home; from the turn after the wait is over, the spells on it, and on what it was done
			// to, are brought to their end until none is left
			if (!knockedOut.homeWaited)
			{
				countHomeWait();
			}
			else if (SpellsOver(registry, entity, knockedOut.actedOn))
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
			// Every turn of the rest counts, the tries to lie down too
			++knockedOut.restTurns;
			// First it lies down in the sleep pose, tried each turn until its body is free to start it: each try draws
			// for the pose's side, and the draw of the try that starts it plays it left to right on a one (never on a
			// body a script set to real time, the side drawn all the same). A body that still lies in its faint, as one
			// not carried home does, never starts it, and so never heals or gets up.
			if (!knockedOut.restPosed)
			{
				const bool mirrored = game_random::GameRand(2) == 1 && !animation.realSpeed;
				const bool moving = Locator::creatureLocomotionSystem::has_value() &&
				                    Locator::creatureLocomotionSystem::value().IsMoving(entity);
				const auto pose = creature_mind::RestStep().sequence;
				const auto posed =
				    moving ? std::optional<creature_layers::BodyAction> {}
				           : creature_layers::PlaySequence(animation.body, pose[0], pose[1], pose[2], false, mirrored);
				if (posed.has_value())
				{
					animation.body = *posed;
					knockedOut.restPosed = true;
				}
				break;
			}
			// Then each turn in the pose its temperature is first brought to the ground's where it stands, so a fire on it
			// stops, and the turn heals it and rests it; it gets up once its life is full and it is not exhausted at all,
			// after more than a few turns of rest. The tries to lie down, and the turn it lies down, leave its fire be
			if (Locator::fireSystem::has_value())
			{
				Locator::fireSystem::value().PutOut(entity);
			}
			if (needs == nullptr || (creature_mind::IsBetter(needs->needs.life, needs->needs.exhaustion) &&
			                         knockedOut.restTurns > creature_mind::k_FewestRestTurns))
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
				return;
			}
			break;
		}
		if (needs != nullptr)
		{
			// Resting in the sleep pose, its body heals; otherwise it is out cold until it gets up
			const bool resting = knockedOut.stage == CreatureKnockedOut::Stage::Resting && knockedOut.restPosed;
			needs->rest = resting                                                    ? CreatureNeeds::Rest::Resting
			              : knockedOut.stage == CreatureKnockedOut::Stage::GettingUp ? CreatureNeeds::Rest::Awake
			                                                                         : CreatureNeeds::Rest::Unconscious;
		}
	}
}
