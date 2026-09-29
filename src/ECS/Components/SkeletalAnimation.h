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
	/// Living flags +0xE0 of the original: 0x800 an into / out-of clip plays, 0x1000 an out-of clip plays first
	uint16_t transitionFlags {0};
	/// test hook OPENBLACK_TEST_ANIM: the clip stays whatever the villager does
	bool locked {false};
	/// the bones' model matrices for the current time (empty until the first update)
	std::vector<glm::mat4> pose;
};

} // namespace openblack::ecs::components
