/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <optional>
#include <unordered_map>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "3D/SkeletalAnimation.h"
#include "Common/Zoomer.h"
#include "Creature/CreatureEyes.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureSway.h"

namespace openblack::ecs::components
{

/// How a creature's body is drawn: its base mesh pulled towards its evil or good, thin or fat and weak or strong
/// meshes, as the creature has become
struct CreatureMorph
{
	/// The fatness the body shows, which follows the creature's own a little each turn
	float shownFatness {0.5f};
	/// How far along each axis the body is drawn
	creature_morph::Morph drawn {};
	/// Goes up each time the animations and rest pose have to follow the body anew
	uint32_t revision {0};
};

/// How a creature's body is posed. The animations follow the body's evil to good and thin to fat axes, built again
/// when they change.
struct CreatureAnimation
{
	/// What the body plays: standing and breathing, an action, or sitting
	creature_layers::BodyAction body {};
	/// Animations played together, each by its weight, in place of the body's own when there are any (as walking
	/// blends into running)
	struct Slot
	{
		size_t animation;
		float timeMs;
		float weight;
		bool mirrored;
		/// A turn on the spot's or a step off's two animations are drawn as one (skeletal_animation::BlendPair): the
		/// first of their two slots holds the second's animation, how far towards it, and the time through the blend's
		/// own length. Both slots stay as they are for all else, such as the sounds of their moments
		struct Pair
		{
			size_t to;
			float weight;
			float timeMs;
			uint32_t durationMs;
		};
		std::optional<Pair> pair {};
	};
	std::vector<Slot> slots;
	/// The face's expression and a gesture such as a nod, played on top of the body
	creature_layers::FaceLayer face {};
	creature_layers::GestureLayer gesture {};
	/// The wobble a blow sends through the body, played once on top of it
	creature_layers::GestureLayer wobble {};
	/// The upper and lower body swaying from things thrown at it, drawn leaning on top
	creature_sway::Sway sway {};
	/// Where the head turns to look, if anywhere, and how far it is turned right to left and down to up
	std::optional<glm::vec3> lookAt;
	creature_layers::LookAxis yaw {};
	creature_layers::LookAxis pitch {};

	/// How fast the body plays its animations and breathes, 1 as it should and 0 frozen still
	float playbackScale {1.0f};
	/// A script set real time (SET_ANIMATION_MODIFY): the body's animations step by the real time, not at the rate of
	/// its size, and each static action starts unmirrored
	bool realSpeed {false};
	/// How far through a breath the creature is, 0 to 1, and the seconds a breath takes now
	float breathPhase {0.0f};
	float breathPeriod {0.0f};

	/// The CreatureMorph revision the animations below were built for
	std::optional<uint32_t> builtRevision;
	/// The rest pose, blended as the body is, and the animations blended so far, by their index in the creature spec
	skeletal_animation::Skeleton skeleton;
	std::unordered_map<size_t, skeletal_animation::Animation> animations;
	/// The animation a turn or a step draws its two as, kept while the same two play by the same weight and length
	struct PairBlend
	{
		size_t from;
		size_t to;
		float weight;
		uint32_t durationMs;
		skeletal_animation::Animation animation;
	};
	std::optional<PairBlend> pairBlend;
	/// Each bone's mirror bone, for playing actions left to right
	std::vector<uint32_t> mirror;

	/// The bones' matrices in the mesh's space as posed this frame, which the body is drawn with when it has no drawn
	/// bones
	std::vector<glm::mat4> boneMatrices;
	/// The bones as the body is drawn this frame, between its last two turn poses (components::CreatureTurnPose), seen
	/// from the body's drawn matrix (ecs::DrawnBodyModel); empty when the turn poses are not drawn
	std::vector<glm::mat4> drawnBones;
	/// The same blend in the world, as the body was last drawn, which a footstep's print reads; empty when the turn
	/// poses are not drawn
	std::vector<glm::mat4> drawnInWorld;
};

/// What a body is set to play a share of the way through a turn, as the frames set CreatureAnimation::body and ::slots:
/// its own action, and the animations blended in place of it. A value, not a component.
struct CreatureAnimationInputs
{
	creature_layers::BodyAction body {};
	std::vector<CreatureAnimation::Slot> slots;
};

/// A creature's eyes: where they look and blink, and where each eyeball and eyelid is drawn this frame
struct CreatureEyes
{
	/// The eyeball and eyelid every creature's eyes are drawn with, Data/Eyeball.l3d and Data/Eyelid.l3d
	static constexpr entt::id_type k_EyeballMeshId = entt::hashed_string("creature/eyeball");
	static constexpr entt::id_type k_EyelidMeshId = entt::hashed_string("creature/eyelid");

	creature_eyes::Mode mode {creature_eyes::Mode::Calm};
	/// How open the eyes are, -1 to about 0.75: sleepy eyes turn slowly and droop
	float openness {0.0f};
	creature_eyes::Blink blink {};
	/// Where the creature looks, when it looks at something rather than ahead
	std::optional<glm::vec3> lookAt;
	/// Which way the back of each eye faces as it eases round to where it looks
	std::array<Zoomer3, 2> look;
	std::array<bool, 2> lookStarted {};

	struct Eye
	{
		std::optional<glm::mat4> eyeball;
		std::optional<glm::mat4> eyelid;
	};
	std::array<Eye, 2> drawn {};
	/// The skin's colour under the eyelids, opaque 0xAARRGGBB, which the body's colour multiplies once for the first lid
	/// drawn and twice for the second
	uint32_t lidColour {0xFFFFFFFFu};
};

} // namespace openblack::ecs::components
