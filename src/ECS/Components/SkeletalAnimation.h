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

#include <vector>

#include <entt/core/fwd.hpp>
#include <glm/mat4x4.hpp>

namespace openblack::ecs::components
{

/// A boned mesh playing a clip of AllAnims.anm (ecs/Animations.h). The renderer draws the entity with `pose` (one model
/// matrix per bone of its mesh) instead of the mesh's rest pose.
struct SkeletalAnimation
{
	/// the clip's id in the animation manager (ecs::ClipId of its index in AllAnims.anm) and that index (ANM_ enum)
	entt::id_type clip {0};
	int32_t clipIndex {-1};
	bool hasClip {false};
	/// milliseconds into the clip
	float time {0.0f};
	float speed {1.0f};
	/// > 0: the clip advances with the ground covered at this many m/s (a moving state, fn_0051AF00), not with time
	float distanceSpeed {0.0f};
	/// Villager +0xE0 (Villager::flags) bits 0x800 / 0x1000 of the original, kept here: 0x800 an into / out-of clip
	/// plays, 0x1000 an out-of clip plays first
	uint16_t transitionFlags {0};
	/// test hook OPENBLACK_TEST_ANIM: the clip stays whatever the villager does
	bool locked {false};
	/// Villager +0xF1, the CARRIED_OBJECT drawn in its hand (ECS/CarriedProps.h): 1 none, 2 axe ... 15 tree 3
	int32_t carriedObject {1};
	/// test hook OPENBLACK_TEST_CARRY: the carried object stays
	bool carriedLocked {false};
	/// Villager::Draw skips the body while the state's info.dat clip is -4 (ANM_DONT_DRAW: inside the house): the mesh
	/// is taken off meanwhile (then nothing draws or picks it) and kept here
	entt::id_type hiddenMesh {0};
	/// the bones' model matrices for the current time (empty until the first update)
	std::vector<glm::mat4> pose;
	/// The SuperVillager's clip cross-fade (ECS/SuperVillager.h; fn_00825530 0x8256C8..0x825755, 0x8257C4..0x825A09).
	/// crossFadeMs > 0: a clip change blends the clip drawn before it (frozen at its last time) into the new one over that
	/// many ms (300, [0xC383D8]); 0, every other villager: the instant cut of Living::SetAnim 0x5ECBA0. With
	/// DrawPosition::followSnap (SuperVillager +0x30 bit 2, 0x8257C4) the fade counts down but is not drawn. Set by
	/// ECS/SuperVillager every frame
	int32_t crossFadeMs {0};
	/// fn_00825400 0x82541D: CheckRegionOnScreen failed this frame, so fn_00825530 does not run: the fade state (and the
	/// last clip's time) stay as they are and only the plain pose is computed (the body is not drawn). Set by
	/// ECS/SuperVillager
	bool crossFadeFrozen {false};
	/// The pose the SuperVillager's body is drawn with while its fade is drawn (0x8257B6..0x825A09: the blend goes to
	/// the SuperVillager bone buffer [0xC37D9C] only); empty otherwise, for every other villager always. `pose` keeps
	/// the plain pose: (inferred) the carried prop and the foot shadows follow it, as Villager::Draw (vt+0x610) poses
	/// the object with its own GetPose before fn_00825530. Read through ecs::DrawnPose
	std::vector<glm::mat4> drawnPose;
	struct CrossFade
	{
		bool hasLast {false};       ///< SuperVillager +0xC != 0: a clip was drawn already
		entt::id_type lastClip {0}; ///< +0xC, the clip drawn last frame
		float lastTime {0.0f};      ///< +0x10, its time (obj+0x84 after the draw, 0x825E30)
		entt::id_type oldClip {0};  ///< LH3DObject +0x88
		float oldTime {0.0f};       ///< +0x8C, frozen during the fade
		int32_t leftMs {0};         ///< +0x94
		float weight {0.0f};        ///< +0x90, the old clip's weight: left / crossFadeMs
	} crossFade;
};

} // namespace openblack::ecs::components
