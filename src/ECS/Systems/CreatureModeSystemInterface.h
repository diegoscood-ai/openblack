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

namespace openblack
{
class Camera;
} // namespace openblack

namespace openblack::ecs::systems
{

/// Creature Mode: the camera locks onto a creature and follows it (see creature_mode and creature_follow). C locks onto
/// the player's own creature and lets go of it again; a double click on a creature locks onto it, another god's too.
/// While locked on, Shift and the cursor keys turn the camera round the creature, and Ctrl and Shift together swing it
/// clear of the land in the way. The cursor keys alone or a grip of the land put a new player's camera over the
/// follow, which stays under it; the temple or a hand demo give the camera back; a script's cinema bars hold the keys
/// back.
///
/// It takes the camera over from the player's camera and hands it back as it was, waiting while the temple or the
/// editor has the camera. Every follow is a mode of its own on the camera's stack of modes (each a Kind::Follow entry,
/// by its key), which this system pushes and takes off, keeping each follow's model: a new follow goes over the one
/// before, which stays under it, and a script's camera mode goes over a follow, which stays under it with its model and
/// is not updated until it is the current mode again.
class CreatureModeSystemInterface
{
public:
	/// What the game tells it each frame
	struct Frame
	{
		/// The interface's hand state as its last pass left it (3, Normal, when it is reset), which tells the hand
		/// gripping the land
		int32_t interfaceHandState {3};
	};

	virtual ~CreatureModeSystemInterface() = default;

	/// Each frame, before the camera moves: reads C and the double click, keeps the camera on the creature, and ends the
	/// mode when it must
	virtual void Update(std::chrono::microseconds dt, const Frame& frame) = 0;

	/// As C does: locks onto the player's creature, or lets go of it when already on it
	virtual void PressCreatureKey() = 0;
	/// The frames a script's camera mode or a dual camera drives the camera, when the update does not run: C is read as
	/// the update reads it, whatever mode is current, so it locks on over a dual camera (the double click is the player's
	/// camera's own, read only in that camera's update)
	virtual void ReadCreatureKey() {}
	/// Locks onto a creature, any player's; false when the camera can't be taken now
	virtual bool Enter(entt::entity creature) = 0;
	/// A load of a game saved with the camera following a thing (Creature Mode, or a script's follow saved in a cut
	/// scene, which the game makes again as the player's follow): locks onto it as Enter does, a thing that is not a
	/// creature too, with the view and the style the save kept and no button held from a double click; false when the
	/// camera can't be taken now or the thing can't be followed
	virtual bool Restore(entt::entity thing, const creature_follow::View& view, const creature_follow::Style& style) = 0;
	/// The newest follow goes, giving the camera back as soon as it has it again: to the player's camera it took over from,
	/// or to the follow under it, which is the current mode again if nothing else is over it
	virtual void Leave() = 0;
	/// The follows' part of the turn's camera checks (camera_checks::CheckModes): from the bottom of the stack, every
	/// follow whose thing is gone comes off the camera's stack, and when the camera had its model, the camera's model goes
	/// back to the one under it (the player's, or the follow's under it), as its cleanup; nothing is restarted here.
	/// Meanwhile, from the frame the thing went, the follow reads nothing and the camera eases on to where it was going.
	/// True when a follow whose model the camera had went
	virtual bool RemoveInvalid() { return false; }
	/// The new player's mode `playersMode`, put over a follow (its place left the camera fence, the cursor keys alone or a
	/// fresh grip of the land, C let go of a follow over it, a script's camera over it ended), is popped: that follow has
	/// the camera again, from where the camera is, and is under no mode from then on. False when it keeps no follow under
	/// that mode
	virtual bool TakeBackFromUnderPlayersCamera(uint32_t /*playersMode*/) { return false; }
	/// A new player's mode `playersMode` went on top of the whole stack while the camera has a follow's model (a script's
	/// camera that went over the follow ended): the follow stays under it with its model, and the camera has a new
	/// player's camera from the camera as it is. False when the camera has no follow's model
	virtual bool KeepUnderPlayersMode(uint32_t /*playersMode*/) { return false; }
	/// A new player's mode goes on top of the whole stack, over whatever mode is current, while the camera has the newest
	/// follow's model (that follow the current mode, or under a dual camera or a placed path pushed over it): the follow
	/// stays under it with its model, the camera has a new player's camera from the camera as it is, and every placed
	/// path not yet under a new player's mode goes under this one (camera_checks::PutNewPlayersModeOver). False, with
	/// nothing done, when the camera has no follow's model
	virtual bool GoUnderNewPlayersMode() { return false; }
	/// A push onto a full stack deleted the follow `follow` from the bottom: it goes, and when the camera has its model,
	/// the camera's model goes back to the one under it. A system with one follow leaves it
	virtual void DeleteFollow(uint64_t /*follow*/) { Leave(); }
	/// Every follow goes (the map's clear): the camera's model back to the player's as soon as it has it
	virtual void LeaveAll() { Leave(); }
	/// The map's clear inside the citadel, where the island's camera behind the temple (`world`) holds the current mode's
	/// model: every follow goes as LeaveAll has them go, but the follow whose model the island's camera has gives that
	/// camera the model under it at once (the player's camera it took over from), as the clear makes a new player's mode
	/// there that the way out starts again
	virtual void LeaveAllBehindTemple(Camera& /*world*/) { LeaveAll(); }
	/// The player's camera took a double click on a creature: it locks onto it in the next update, before the camera
	/// moves, as the game builds the creature's camera once the camera's update that took it is over
	virtual void LockOnNext(entt::entity creature) = 0;
	[[nodiscard]] virtual bool IsActive() const = 0;
	/// The creature the newest follow is locked onto
	[[nodiscard]] virtual std::optional<entt::entity> GetCreature() const = 0;
	/// The creature the newest follow is locked onto while that follow is the current mode with the camera, the only time
	/// its update runs: the creature's status panel shows it then. None while another mode is over it
	[[nodiscard]] virtual std::optional<entt::entity> GetCreatureInView() const
	{
		return GetView().has_value() ? GetCreature() : std::nullopt;
	}
	/// The player's own creature, which C locks onto
	[[nodiscard]] virtual std::optional<entt::entity> PlayersCreature() const = 0;
	/// The camera's view of the creature, while it has the camera
	[[nodiscard]] virtual std::optional<creature_follow::View> GetView() const = 0;
	/// How the follow eases and turns, while it has the camera
	[[nodiscard]] virtual std::optional<creature_follow::Style> GetStyle() const = 0;
	/// The newest follow as the camera's current mode, which a save writes
	struct CurrentFollow
	{
		creature_follow::View view;
		creature_follow::Style style;
	};
	/// The newest follow's view and style while it is the camera's current mode: while it has the camera, and inside the
	/// citadel while the temple keeps its model aside, as the way in leaves the camera's modes as they are. None otherwise
	[[nodiscard]] virtual std::optional<CurrentFollow> GetCurrentFollow() const
	{
		const auto view = GetView();
		const auto style = GetStyle();
		if (!view.has_value() || !style.has_value())
		{
			return std::nullopt;
		}
		return CurrentFollow {.view = *view, .style = *style};
	}

	/// As if Ctrl and Shift were pressed together: swings the camera clear of the land in the way
	virtual void ClearView() = 0;
};

} // namespace openblack::ecs::systems
