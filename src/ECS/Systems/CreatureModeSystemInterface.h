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

#include <chrono>
#include <optional>

#include <entt/entity/entity.hpp>

#include "Camera/CreatureFollow.h"

namespace openblack::ecs::systems
{

/// Creature Mode: the camera locks onto a creature and follows it (see creature_mode and creature_follow). C locks onto
/// the player's own creature and lets go of it again; a double click on a creature locks onto it, another god's too.
/// While locked on, Shift and the cursor keys turn the camera round the creature, and Ctrl and Shift together swing it
/// clear of the land in the way. The cursor keys alone, a fresh grip of the land, the temple, a script's camera, a hand
/// demo or a script's cinema bars give the camera back.
///
/// It takes the camera over from the player's camera and hands it back as it was, waiting while the temple or the
/// editor has the camera.
class CreatureModeSystemInterface
{
public:
	/// What the game tells it each frame
	struct Frame
	{
		/// The hand is gripping the land to drag or turn the camera
		bool handGripping {false};
	};

	virtual ~CreatureModeSystemInterface() = default;

	/// Each frame, before the camera moves: reads C and the double click, keeps the camera on the creature, and ends the
	/// mode when it must
	virtual void Update(std::chrono::microseconds dt, const Frame& frame) = 0;

	/// As C does: locks onto the player's creature, or lets go of it when already on it
	virtual void PressCreatureKey() = 0;
	/// Locks onto a creature, any player's; false when the camera can't be taken now
	virtual bool Enter(entt::entity creature) = 0;
	/// A load of a game saved with the camera following a thing (Creature Mode, or a script's follow saved in a cut
	/// scene, which the game makes again as the player's follow): locks onto it as Enter does, a thing that is not a
	/// creature too, with the view and the style the save kept and no button held from a double click; false when the
	/// camera can't be taken now or the thing can't be followed
	virtual bool Restore(entt::entity thing, const creature_follow::View& view, const creature_follow::Style& style) = 0;
	/// Gives the camera back, as soon as it has it again
	virtual void Leave() = 0;
	/// The turn's camera checks: a follow whose thing is gone comes off the camera, and, when it had the view, the
	/// player's camera has it again and flies back to the view it had. Meanwhile, from the frame the thing went, the
	/// follow reads nothing and the camera eases on to where it was going
	virtual void CheckFollow() {}
	/// The new player's mode `playersMode` that went over the follow as its place left the camera fence is popped: the
	/// follow has the camera again, from where the camera is. False when the follow is not under that mode
	virtual bool UncoverFromPlayersCamera(uint32_t /*playersMode*/) { return false; }
	/// While locked on: the follow's number in the order of the camera's other modes' pushes
	/// (script_camera::OtherModePushed), which tells whether it is the lowest of them
	[[nodiscard]] virtual std::optional<uint32_t> PushedAt() const { return std::nullopt; }
	/// The player's camera took a double click on a creature: it locks onto it in the next update, before the camera
	/// moves, as the game builds the creature's camera once the camera's update that took it is over
	virtual void LockOnNext(entt::entity creature) = 0;
	[[nodiscard]] virtual bool IsActive() const = 0;
	/// The creature locked onto
	[[nodiscard]] virtual std::optional<entt::entity> GetCreature() const = 0;
	/// The player's own creature, which C locks onto
	[[nodiscard]] virtual std::optional<entt::entity> PlayersCreature() const = 0;
	/// The camera's view of the creature, while it has the camera
	[[nodiscard]] virtual std::optional<creature_follow::View> GetView() const = 0;
	/// How the follow eases and turns, while it has the camera
	[[nodiscard]] virtual std::optional<creature_follow::Style> GetStyle() const = 0;

	/// As if Ctrl and Shift were pressed together: swings the camera clear of the land in the way
	virtual void ClearView() = 0;
};

} // namespace openblack::ecs::systems
