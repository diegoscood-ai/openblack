/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec3.hpp>

namespace openblack::ecs
{

/// The puff an object leaves when it goes (Object::CreateSmokyStuff 0x63A810 -> SmokyStuff::Create 0x823C90; research
/// dev\tmp_dis\animals\misc.md §3): 15 sprites of Data\Textures\smoke.raw (a 4 x 4 sheet) around the point, each with a
/// random direction at 0.3..1 x size, grey (0x808080) fading over 3 seconds while they grow from 0.5 to 1.5 x size.
/// Used by the animal corpses that time out; the original also uses it for villagers, buildings and spells.
class SmokyStuff
{
public:
	/// `size` is the original's size argument (the corpse: 1); the point is usually half the object's height up
	static void Create(glm::vec3 at, float size);
	/// game seconds (it stops while paused)
	static void Update(float seconds);
	static void Clear();
	SmokyStuff() = delete;
};

} // namespace openblack::ecs
