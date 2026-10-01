/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::physics
{
struct PhysicsObject;

/// The collision sounds of the physics (PhysicsObject::AttemptToAddSoundEvent 0x6464F0 and the editor.sad animation
/// table it looks up through SamplePlayAnimEffect), plus the ground dust and the water splash of a landing.
class CollisionSounds
{
public:
	/// SOUND_COLLISION_TYPE of an object (info collideSound; a Fragment is always BUSH, DeadTree mesh 406 HOLLOW_WOOD).
	[[nodiscard]] static int TypeOf(entt::entity entity);
	/// AttemptToAddSoundEvent for a body at the end of a turn (its impact and what hit it are set).
	static void AttemptToAddSoundEvent(const PhysicsObject& po);
	/// One of editor.sad samples first..last at random, 3D at the point: GAudio::SamplePlayAnimEffect 0x42A4B0 with the
	/// object as the channel's owner (so the .sad play mode applies per object: G_BigSplash's mode 2 plays nothing while
	/// the same object's splash plays) and `track` (options +0x0C: the sound follows the object every turn).
	static void PlayEditorSample(int first, int last, glm::vec3 at, entt::entity owner = entt::null, bool track = false);
	/// One sample of a bank, 2D.
	static void PlaySample2D(const char* bank, int sample);
	/// The pair list ages one turn (pairs stay listed for the turn they were added and the next).
	static void EndTurn();
	CollisionSounds() = delete;
};
} // namespace openblack::ecs::physics
