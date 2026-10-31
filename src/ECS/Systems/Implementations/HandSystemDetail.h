/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// Helpers shared by the HandSystem translation units:
//   HandSystem.cpp      input state machine, animation, public interface
//   HandPlacement.cpp   hand geometry, placement, cursor pick (ObtainRequiredHandPosition)
//   HandHolding.cpp     pick up, hold poses, spring, drop / throw
//   HandResources.cpp   piles, pots, multi pick-up, put down, stores
//   HandTrees.cpp       tug, uproot, roots, replant, dead trees
//   HandEffects.cpp     grip dust, multi pick-up particles
//   HandFish.cpp        splash of gripping the water, catching fish
//   HandPhysics.cpp     trees, pots and stores in the physics system (ECS/Physics)
//   HandDebugHooks.cpp  environment-variable test hooks

#include <cmath>
#include <cstdint>

#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Audio/Audio.h"
#include "Audio/Device/Sound.h"
#include "Common/Zoomer.h"
#include "Enums.h"
#include "Magic/HandHoldPose.h"

namespace openblack::ecs::systems::hand_detail
{
/// A sound effect (NULL, sample, 3, 0, 0, 0, bank), a 2D one-shot without owner: the hand's failure (FailApply:
/// G_SpellCastFailure). The sample is a "<bank>.sad/<n>" SoundId.
void PlaySample(audio::SoundId id);
/// A sound effect from play options with is3D 1, track 0, no owner, at a world point (the options form of the hand's
/// 3D sites, e.g. G_HandInWater when gripping the water): not started beyond the sample's max
/// distance from the camera. Returns the channel as an entity (entt::null when it did not start).
entt::entity PlaySample3D(audio::SoundId id, glm::vec3 point);
/// MapCoords::IsLand: the landscape cell under the point does not have the water bit.
bool IsLand(glm::vec3 point);
/// A pot's end of physics: a hand's pile of wood or food (HandWood / HandFood) that ends out of the water is put down
/// as a land pile; any other pot, or one in the water, stays as it is
[[nodiscard]] constexpr bool HandPotBecomesPile(PotInfo type, bool onLand)
{
	return (type == PotInfo::HandWood || type == PotInfo::HandFood) && onLand;
}
/// The held seed is ready and can be cast on the object (HandSpellSeed.cpp, the press's test)
[[nodiscard]] bool SeedValidToApplyToObject(entt::entity seed, entt::entity target);
/// The held seed is ready and can be cast at the map point (HandSpellSeed.cpp, the press's test)
[[nodiscard]] bool SeedValidToApplyToMapCoord(entt::entity seed, const glm::vec3& mapPosition);
/// OPENBLACK_DUMP_ENTITY_COUNTS (HandDebugHooks.cpp): entity counts per kind, once.
void DumpEntityCounts();

/// One turn of the multi pick-up from a pile: t = clamp(pickTurns / rampTicks, 0, 1) (<= 0 or NaN gives 0) and the
/// amount (perTurn + (perTurnEnd - perTurn) x t^2, truncated toward zero), before the source and room limits
struct PickUpStep
{
	float t;
	uint32_t amount;
};
[[nodiscard]] PickUpStep PickUpAmount(uint32_t perTurn, uint32_t perTurnEnd, float rampTicks, uint32_t pickTurns);

/// The hand model's scale: 3.2 x the hand scale / the hand model's height (the bind joint Y extent of Hand_Boned_Base2),
/// so the hand is 3.2 units long. The mesh is a left hand, which a right-handed hand mirrors along its x axis
[[nodiscard]] constexpr glm::vec3 HandModelScale(float handScale, bool rightHanded)
{
	const float scale = 3.2f * handScale / magic::hand_hold::k_HandModelHeight;
	return {rightHanded ? -scale : scale, scale, scale};
}

/// Whether the hand's state draws the object it holds: only while no box has the mouse (the escape menu, a dialog), the
/// interface is active and the state is the tug, holding, multiple pick-up or grain state (3, 4, 6 and 8)
[[nodiscard]] constexpr bool DrawsHeldObject(int32_t handState, bool interfaceActive, bool boxHasMouse)
{
	const bool drawingState = handState == 3 || handState == 4 || handState == 6 || handState == 8;
	return !boxHasMouse && interfaceActive && drawingState;
}

/// Whether the hand's draw preparation stops once its state has changed: in the INVISIBLE state (0), while a box has
/// the mouse (the escape menu, a dialog) or with the interface inactive. Then the mouse smoothing, the state's update,
/// the blend and the hand's pose and place do not run, and the hand stays drawn as last placed unless it is INVISIBLE
[[nodiscard]] constexpr bool DrawPreparationStops(int32_t handState, bool interfaceActive, bool boxHasMouse)
{
	return handState == 0 || boxHasMouse || !interfaceActive;
}

/// The hand's distance from the eye along the mouse ray, eased by a zoomer, and its start
struct HandDistance
{
	Zoomer zoomer;
	/// The zoomer has ever moved: the very first time it moves it starts at 0
	bool started {false};
};

/// The normal state's entry puts the hand's distance zoomer at rest at the hand's view distance (where its scale was
/// last taken). If the zoomer has never moved, its first move still starts it at 0
[[nodiscard]] HandDistance RestartHandDistance(HandDistance distance, float viewDistance);

/// Whether the hand's state update moves its distance zoomer: the normal state's (1) always, the holding state's (4)
/// unless a pile, field or fish farm holds the hand in its interaction, and the grain state's (8) the same way while
/// the hand holds its object, as the grain state then runs the holding state's update. Every other state places the
/// hand its own way and leaves the zoomer as it is
[[nodiscard]] constexpr bool StateMovesHandDistance(int32_t handState, bool holdsObject, bool lockedInInteraction)
{
	switch (handState)
	{
	case 1:
		return true;
	case 4:
		return !lockedInInteraction;
	case 8:
		return holdsObject && !lockedInInteraction;
	default:
		return false;
	}
}

/// Whether the hand's state takes its view distance, and so its scale, from where the hand is placed each frame: every
/// state but the tug (3), the totem (5), the creature (7), the play-anim (9) and the temple (10) states. The tug and
/// play-anim states keep the last one, the totem's is taken once, as it is entered (TotemViewDistance), and the
/// creature's and the temple's come from their own distance zoomers (StepCreatureDistance, StepCitadelDistance)
[[nodiscard]] constexpr bool StateTakesViewDistance(int32_t handState)
{
	return handState != 3 && handState != 5 && handState != 7 && handState != 9 && handState != 10;
}

/// The temple (citadel) state's aim: the distance from the eye to where the cursor meets the room, summed z, y, x, less
/// 5, at least 1
[[nodiscard]] float CitadelRoomDistance(const glm::vec3& eye, const glm::vec3& roomPoint);

/// One frame of the temple state's distance zoomer and the distance it leaves, which is the hand's view distance and
/// the distance along the mouse ray the hand is drawn at
struct CitadelDistanceStep
{
	Zoomer zoomer;
	float distance;
};
/// The zoomer is sent to the eye's distance to `along` (the point on the mouse ray at CitadelRoomDistance), summed z,
/// y, x and kept within 4 and 300, in 0.2 s when that is nearer than where it was going, else 0.4 s, then moves on by
/// `seconds` (Zoomer::UpdateInlineStartSpeedFirst)
[[nodiscard]] CitadelDistanceStep StepCitadelDistance(Zoomer zoomer, const glm::vec3& eye, const glm::vec3& along,
                                                      float seconds);

/// The creature state's distance zoomer as the state is entered: the distance from the eye to the creature's position,
/// summed z, y, x, less 0.825 times the size its body is drawn at, at rest
[[nodiscard]] float CreatureEntryDistance(const glm::vec3& eye, const glm::vec3& creature, float drawnSize);

/// One frame of the creature state's distance zoomer, and the distance it gives the hand's view distance (and so its
/// scale and the place along the mouse ray it is drawn at) before it moves on
struct CreatureDistanceStep
{
	Zoomer zoomer;
	float viewDistance;
};
/// The zoomer is put at once no further than the body's bottom less a quarter of its height (`groin`, the bottom bone
/// in the pose the game reads; none when the species has none). With the hand on the body (`touch`, where the line of
/// sight meets it) it is put no further than the touch less 0.085 of the height, and sent to the touch less 0.055 of
/// the height in 0.3 s. The view distance is the zoomer's value then; the zoomer then moves on by `seconds`
/// (Zoomer::UpdateInlineByTime)
[[nodiscard]] CreatureDistanceStep StepCreatureDistance(Zoomer zoomer, const glm::vec3& eye, std::optional<glm::vec3> groin,
                                                        std::optional<glm::vec3> touch, float height, float seconds);

/// The view distance the totem state takes as it is entered: the distance from the eye to the totem's position, summed
/// z, y, x, less the totem's 2D radius
[[nodiscard]] inline float TotemViewDistance(const glm::vec3& eye, const glm::vec3& totem, float radius)
{
	const float dx = eye.x - totem.x;
	const float dy = eye.y - totem.y;
	const float dz = eye.z - totem.z;
	return std::sqrt((dz * dz + dy * dy) + dx * dx) - radius;
}

/// The seconds of the hand's state update from its whole milliseconds (the camera's time step, the real frame time
/// inside the citadel): one float product by 0.001
[[nodiscard]] constexpr float StateUpdateSeconds(uint32_t frameMs)
{
	return static_cast<float>(frameMs) * 0.001f;
}

/// How long the hand's distance takes to come closer, to move away, and to move away slowly
inline constexpr float k_HandDistanceCloserSeconds = 0.1f;
inline constexpr float k_HandDistanceAwaySeconds = 0.28f;
inline constexpr float k_HandDistanceSlowAwaySeconds = 2.0f;
/// While the distance already comes closer at this speed or faster, a slow move away takes the usual time
inline constexpr float k_HandDistanceSlowAwaySpeedLimit = -1.0f;

/// One frame of the hand's distance: the zoomer starts at 0 the very first time, moves towards the target in 0.1 s when coming
/// closer and 0.28 s when moving away (stepped as Zoomer::UpdateInlineSpeed sums it), and stays at least 1. Moving away
/// slowly (while the hand holds something to give to a creature, or the camera watches a fight) takes 2 s instead,
/// unless the distance is coming closer at a speed of 1 or more. It belongs to the state's update, so a skipped frame
/// (the draw preparation stopped: hidden, a box with the mouse, the interface inactive; or a state whose update does
/// not move it, StateMovesHandDistance) neither starts nor moves it
[[nodiscard]] HandDistance AdvanceHandDistance(HandDistance distance, float target, float seconds, bool skipped,
                                               bool slowAway = false);
} // namespace openblack::ecs::systems::hand_detail
