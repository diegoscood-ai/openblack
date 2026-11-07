/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <chrono>
#include <optional>
#include <utility>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureLayers.h"
#include "ECS/Components/CreatureBody.h"

namespace openblack::creature_hand_glow
{
enum class Place : uint8_t;
}

namespace openblack::ecs::systems
{

/// Brings the creatures' bodies to life: their shape follows what each creature has become, they are posed by their
/// animations, and their eyes look about and blink (see components::CreatureMorph, CreatureAnimation, CreatureEyes)
class CreatureAnimationSystemInterface
{
public:
	virtual ~CreatureAnimationSystemInterface() = default;

	/// Once a game turn: the fatness each body shows follows its creature's a step
	virtual void ProcessTurn() = 0;
	/// Once a frame, by the game time, which stops while the game is paused: the bodies are reshaped where they have
	/// changed enough, posed as their animations stand (nothing is moved on), drawn `turnFraction` of the way from their
	/// last turn pose but one to their last (components::CreatureAnimation::drawnBones), and their eyes placed on the
	/// body as it is drawn
	virtual void Update(float turnFraction, std::chrono::duration<float, std::milli> gameTime) = 0;
	/// Once a game turn, at its end just before PoseTurn: each body's face, gesture, wobble, breath, action and head move
	/// on by the whole milliseconds the creature lives in the turn (creature_turn_pose::ClockOfTurn, by the size it is
	/// drawn at), as the original moves them once a creature turn. A body whose rest pose is not built yet for its shape,
	/// or whose species has no animations, moves nothing. Gives, by creature, what each body's layers passed, over which
	/// the turn's sounds are looked for (a body that moved nothing is left out)
	using TurnMoments = std::vector<std::pair<entt::entity, creature_layers::TurnPassed>>;
	virtual TurnMoments AdvanceTurn() = 0;
	/// What each creature's body is set to play at the end of the turn, by creature
	using TurnInputs = std::vector<std::pair<entt::entity, components::CreatureAnimationInputs>>;
	/// At the end of a game turn: each body's pose becomes its pose for the turn (components::CreatureTurnPose), in the
	/// world. The body is posed as it plays `inputs` at the turn's end (a creature without any plays what it is set to),
	/// with its face, gestures and head as the turn's advance (AdvanceTurn) left them, and nothing is moved on. A body
	/// that has not been posed yet gets none. The original can pose a body more than once in a turn, and each pose moves
	/// the current one to the previous; only this end-of-turn pose is made so far. Before the pose, the eyes blink by the
	/// time the creature lives this turn, on the game's local random stream
	virtual void PoseTurn(const TurnInputs& inputs) = 0;
	/// A force kicks a creature's body at a point, swaying its upper body when the point is high above the land and its
	/// lower body otherwise, unless what the body is doing takes no kick. The spring is stepped and drawn leaning once a
	/// turn, in PoseTurn. A body that cannot sway, as the tests' fakes, ignores it
	virtual void KickSway(entt::entity /*creature*/, glm::vec3 /*force*/, glm::vec3 /*point*/) {}

	/// Where a bone of a creature is in one of its animations at a time, played left to right or not, in its mesh's space,
	/// the animation blended as the body is drawn; nothing when the creature has no such animation or bone, or hasn't been
	/// posed yet
	[[nodiscard]] virtual std::optional<glm::vec3> BoneInAnimation(entt::entity creature, size_t animation, float timeMs,
	                                                               uint32_t bone, bool mirrored) = 0;
	/// How long one of a creature's animations lasts, in milliseconds, if it has it
	[[nodiscard]] virtual std::optional<float> AnimationDuration(entt::entity creature, size_t animation) = 0;
	/// How far one of a creature's animations carries its body over the whole of it, in its mesh's units, if it has it
	[[nodiscard]] virtual std::optional<glm::vec3> AnimationTravel(entt::entity /*creature*/, size_t /*animation*/)
	{
		return std::nullopt;
	}
	/// Where a hand grasps in one of a creature's animations at a time: the mean place of the bones at the ends of the
	/// body's tree from the hand bone, in its mesh's space; nothing when the creature has no such animation or bone, or
	/// hasn't been posed yet
	[[nodiscard]] virtual std::optional<glm::vec3> GraspInAnimation(entt::entity /*creature*/, size_t /*animation*/,
	                                                                float /*timeMs*/, uint32_t /*hand*/)
	{
		return std::nullopt;
	}
	/// The creature's rest pose and animations built for its body as it is drawn now, once for each change of its
	/// body, as a frame builds them, so that its animations can be asked about before it is first drawn. False when its
	/// species' meshes are not loaded
	virtual bool EnsurePosed(entt::entity creature) = 0;
	/// Each frame the Creature Cave is drawn, by the frame's real milliseconds (held to 200): the copy of the creature
	/// by its fire (components::CreatureCopy) starts its rest the first time, may yawn while it sits, moves on and is
	/// posed, and its eyes are placed. The world stands still in the temple, so the frame's game time does not move it
	virtual void UpdateCopies(uint32_t frameMs) = 0;
	/// As the falling spell's film starts, the copy of the creature it shows (components::FallingCreature), just made: a
	/// millisecond of its time at its size's rate, then its time becomes the film's, its look and body are reset to
	/// standing, and it starts the falling action, played once, when its species has it. It is posed after the step and
	/// again after the reset. Nothing for a copy whose species' body is not loaded. A body that cannot be posed, as the
	/// tests' fakes, ignores it
	virtual void StartFallingCopy(entt::entity /*copy*/) {}
	/// Each update of the falling spell's film, its copy of the creature moves on by the film's milliseconds since the
	/// last update: its breath, its sparkles' draws while it is the body that sparkles, its blink on the local stream,
	/// then what it plays, and it is posed. The game is paused by the film, so the frame's game time does not move it
	virtual void AdvanceFallingCopy(entt::entity /*copy*/, uint32_t /*milliseconds*/) {}
	/// Each frame the falling spell draws, hidden or not, its copy of the creature is made ready to be drawn: its body
	/// between the last two poses its steps gave it, the share of the game's turn that the pause keeps of the way from
	/// the one before to the last. No random number is drawn
	virtual void PrepareFallingCopy(entt::entity /*copy*/) {}
	/// Each frame the copy of the creature is drawn (not while it is hidden), its eyes are placed on its body as it is
	/// drawn, their look moving on by its last step's milliseconds
	virtual void PlaceFallingCopyEyes(entt::entity /*copy*/) {}

	/// A script gives a creature a creed power: a glow of a scale and a power at a place on its body, a power of 0 to
	/// remove the glow (components::CreatureHandGlows). The creature hums while its glows have power (see
	/// creature_hand_glow::SetCreedPower). A body that cannot glow, as the tests' fakes, ignores it
	virtual void SetCreedPower(entt::entity /*creature*/, creature_hand_glow::Place /*place*/, float /*scale*/, float /*power*/)
	{
	}
	/// A creature, or the falling film's copy of one, is drawn: the glows on its body are placed where they are drawn
	/// (components::CreatureHandGlows::placed), their textures sliding on by the frame's real milliseconds and the
	/// circling glow turning by its game milliseconds, once for each draw. A body not drawn in a frame has none placed
	/// and its glows stand still. Nothing for a body without glows, a pose or its species' rig, as the tests' fakes
	virtual void PlaceHandGlows(entt::entity /*body*/, uint32_t /*realMs*/, uint32_t /*gameMs*/) {}
	/// A moment of a creature's animation is played: when its action sets a creed glow (creature_hand_glow::CreedEventOf),
	/// the glow at its place gets the moment's power at a scale of 1, a power of 0 removing it. The creature's glows are
	/// made on the first such moment, and its hum does not change
	virtual void PlayCreedEvent(entt::entity /*creature*/, int32_t /*event*/) {}

	/// Whether the creatures that look good or evil make their motes or smoke each turn. On unless turned off for
	/// testing; while off none are made and no random numbers are drawn for them
	[[nodiscard]] virtual bool AreSparklesShown() const = 0;
	virtual void SetSparklesShown(bool shown) = 0;
};

} // namespace openblack::ecs::systems
