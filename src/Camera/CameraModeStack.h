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

#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

/// The camera's stack of modes (docs/wiki/camera/game-camera.md, docs/wiki/camera/script-camera.md#dual-camera): the
/// pure rules of the stack the script camera keeps over the player's own mode. The script camera (Camera/ScriptCamera.h)
/// drives it
namespace openblack::camera_stack
{

/// A dual camera's heading pi / 4, pitch pi / 8 and distance factor 1 with two things
constexpr float k_DualHeading = 0.785398185f;
constexpr float k_DualPitch = 0.392699093f;
constexpr float k_DualDistanceFactor = 1.0f;

/// A dual camera ("Dual Cam") on the camera's mode stack
struct DualMode
{
	entt::entity a = entt::null;
	entt::entity b = entt::null;   ///< Not written for a point camera: only read with twoObjects
	glm::vec3 point {0.0f};        ///< The point of a point camera
	bool twoObjects = true;        ///< Set by UpdateDual
	float heading = k_DualHeading; ///< Radians
	float pitch = k_DualPitch;     ///< Radians, kept >= 0.241661 by the update
	float distanceFactor = k_DualDistanceFactor;
	bool alive = true; ///< Cleared when deleted
};

/// One of the modes on the camera's stack of modes the script camera keeps: the script mode, a dual camera, or the creature
/// interaction's camera, which another system drives and which cannot be left
struct StackedMode
{
	enum class Kind : uint8_t
	{
		Script,
		Dual,
		Interaction,
		/// A new player's mode put on top of the stack by a script's end: the modes under it stay there, never current
		/// again unless it is popped (the end of a creature hold pops whatever mode is current)
		Player,
		/// A path a miracle placed in the world (its owner's key in `path`), which the camera path system plays: one of
		/// the other systems' entries (ModeStack::OtherEntries)
		Path,
		/// Creature Mode's follow, which the creature mode system drives: one of the other systems' entries
		Follow,
		/// The dance camera (its dance in `dance`), which a dance's key frame calls in once the hand has stayed near the
		/// dance: it holds the view where it is. One of the other systems' entries, the camera checks its owner
		Dance,
	};
	Kind kind = Kind::Script;
	DualMode dual; ///< Only read for a dual camera
	/// A new player's mode: its number on the stack, from 1, which the camera's model it went over is kept by (0 for the
	/// other kinds)
	uint32_t id = 0;
	/// A new player's mode: the camera's model it went over (the player's mode under it, as that mode left the view) is
	/// kept until this mode is popped, which gives it back. False once that mode is gone, or when there was none to keep
	bool keepsUnder = false;
	/// A script mode whose script's camera ended: no longer valid, it keeps its place of the stack under the new player's
	/// mode until the turn's check deletes it. A dance camera ended (the hand gripped the land, or its dance went): it
	/// stays where it is until the turn's check deletes it
	bool dead = false;
	/// The dance camera: the dance it is kept by
	entt::entity dance = entt::null;
	/// A placed path: its owner's key, by which the camera path system keeps the path's playback
	uint64_t path = 0;
	/// Creature Mode's follow: its key, by which the creature mode system keeps that follow (each follow is a mode of its
	/// own on the stack)
	uint64_t follow = 0;
	/// A placed path: the number of the new player's mode put over it, 0 while none is. None of the paths under one has
	/// the camera until that mode is popped
	uint32_t under = 0;
	/// Its place in the stack's push order, whatever its kind: a mode pushed later is over every mode pushed before it
	/// (set by the push)
	uint64_t pushed = 0;
};

/// The camera's stack of modes, as the game stacks them over the player's own mode (the bottom of the stack, kept outside
/// it): the script camera's modes, and the other systems' entries (the placed paths, Creature Mode's follow), in one
/// push order whatever their kind. Each new mode goes on top of whatever mode is current, and only the current one, the
/// top, is updated. The stack has 12 places, the player's own mode and the other systems' modes counted: a mode pushed
/// onto a full stack deletes the bottom one first. The pure rules of the stack; the script camera's functions
/// (Camera/ScriptCamera.h) drive its own modes
class ModeStack
{
public:
	/// The places of the camera's stack of modes
	static constexpr std::size_t k_Places = 12;
	/// The mode a push onto a full stack deleted
	enum class Dropped : uint8_t
	{
		None,
		/// The player's own mode, the bottom: once the stack empties, the game makes a new one (NewPlayersModeIfGone)
		PlayersMode,
		/// The lowest of the other systems' modes (a follow of Creature Mode's, a placed path), which its own system deletes
		/// (TakeOtherSystemsDrops)
		OtherSystemsMode,
		/// The lowest of the script camera's modes, a dual camera marked deleted
		Own,
	};
	[[nodiscard]] bool Empty() const { return _modes.empty(); }
	[[nodiscard]] std::size_t Size() const { return _modes.size(); }
	[[nodiscard]] const std::vector<StackedMode>& Modes() const { return _modes; }
	/// The current mode, the top, when it is one of the script camera's; none when the player's own mode or another
	/// system's entry (OtherCurrent) is current
	[[nodiscard]] const StackedMode* Current() const { return OwnCurrent() ? &_modes.back() : nullptr; }
	/// Another system's entry (a placed path, Creature Mode's follow) is the current mode: pushed after every mode of the
	/// script camera's still on the stack
	[[nodiscard]] bool OtherCurrent() const
	{
		return !_others.empty() && (_modes.empty() || _others.back().pushed > _modes.back().pushed);
	}
	[[nodiscard]] bool ScriptCurrent() const;
	[[nodiscard]] bool DualCurrent() const;
	/// The creature interaction's camera is the current mode, or is anywhere on the stack
	[[nodiscard]] bool InteractionCurrent() const;
	[[nodiscard]] bool InteractionOnStack() const;
	/// A new player's mode a script's end put over the creature interaction's camera is the current mode
	[[nodiscard]] bool PlayerCurrent() const;
	/// None of the script camera's own modes but a player's mode is current: the bottom one, one a script's end put on
	/// top, or another system's entry (OtherCurrent), which moves the camera's own zoomers as the player's mode does
	[[nodiscard]] bool PlayersModeCurrent() const { return _modes.empty() || PlayerCurrent() || OtherCurrent(); }
	/// The camera's zoomers are not the script camera's: the player's mode or the creature interaction's camera is current
	[[nodiscard]] bool OverAnotherCamera() const { return PlayersModeCurrent() || InteractionCurrent(); }
	/// The current dual camera, none when the current mode is not one
	[[nodiscard]] DualMode* CurrentDual();
	/// A script mode is on the stack over the newest player's mode, current or not (one under a new player's mode is
	/// current again only if that mode is popped)
	[[nodiscard]] bool ScriptOnStack() const;
	/// Whether the current mode can be left for a new one: the player's mode and a dual camera can, a living script mode
	/// and the creature interaction's camera cannot
	[[nodiscard]] bool CanLeaveCurrent() const
	{
		// a script mode can be left only once it is no longer valid
		return !(ScriptCurrent() && !_modes.back().dead) && !InteractionCurrent();
	}
	/// The places the script camera's modes take, and the player's own mode while it is there
	[[nodiscard]] std::size_t Places() const;
	/// A new mode on top: the current one from now on. `under` is the number of the other systems' modes on the stack
	/// (Creature Mode's follows, the placed paths: OtherCount), a follow the new mode goes over among them. On a full stack
	/// the bottom mode is deleted first and the rest move down a place: the player's mode while it is there, else the one
	/// pushed first of the lowest other entry and the script camera's lowest mode (other modes counted in `under` with no
	/// entry are taken as pushed before any of the script camera's)
	Dropped Push(const StackedMode& mode, std::size_t under = 0);
	/// The player's own mode was deleted by a push onto a full stack and no new one has been made since
	[[nodiscard]] bool PlayersModeGone() const { return _playersModeGone; }
	/// The other systems' modes (`under` of Push) pushes deleted from the bottom since the last call, which their own
	/// systems are to delete, the lowest first
	std::size_t TakeOtherSystemsDrops() { return std::exchange(_otherSystemsDrops, 0); }
	/// The stack emptied with the player's own mode deleted: the game makes a new player's mode, which is the bottom from
	/// then on. True when that is the case (`under`: as Push's)
	bool NewPlayersModeIfGone(std::size_t under = 0);
	/// The current mode, when it is a dual camera, deleted and popped: the mode under it is current again. False when it is
	/// not one
	bool PopDual();
	/// What a pop of the script mode or of the creature interaction's camera did
	enum class Popped : uint8_t
	{
		/// Nothing: the current mode is not the one asked for
		None,
		/// Popped: the mode under it, or the player's own one, is current again
		ModeUnder,
		/// Popped off a stack it leaves empty with the player's own mode deleted by a full stack: the game makes a new
		/// player's mode, the bottom from then on (as NewPlayersModeIfGone)
		NewPlayersMode,
	};
	/// The current mode, when it is the script mode, deleted and popped (`under`: as Push's)
	Popped PopScript(std::size_t under = 0);
	/// The current mode, when it is the creature interaction's camera, popped (`under`: as Push's)
	Popped PopInteraction(std::size_t under = 0);
	/// The current mode, when it is a new player's mode over the creature interaction's camera, popped. False when it is
	/// not
	bool PopPlayer();
	/// A new player's mode on top (Kind::Player), as Push (`under` as Push's). `keepsUnder`: the camera's model it goes
	/// over is kept for it. Returns its number
	uint32_t PushPlayer(bool keepsUnder, std::size_t under = 0);
	/// The player's mode that a pop of the current one took off
	struct PlayerPop
	{
		/// Its number; 0 for the bottom one, outside the stack
		uint32_t id = 0;
		/// The camera's model it went over was kept for it
		bool keptUnder = false;
	};
	/// The current mode, when it is the player's, deleted and popped: a new player's mode (Kind::Player), or the bottom
	/// one, which leaves nothing under it (the game then makes a new one, the bottom from then on). None when the current
	/// mode is not the player's
	std::optional<PlayerPop> PopPlayersMode();
	/// The camera's model the player's mode `id` went over is still kept for it
	[[nodiscard]] bool KeepsModel(uint32_t id) const;
	/// The player's mode `id` is on the stack
	[[nodiscard]] bool HasPlayer(uint32_t id) const;
	/// The mode the player's mode `id` went over was deleted from under it, and the player's mode under that one is what
	/// it has under it from now on: kept for it while that one is still on the stack (the bottom one, or another new
	/// player's mode under it). False when it keeps nothing
	bool KeepUnder(uint32_t id);
	/// The end of a script's control with the script mode current: the script mode is deleted and a new player's mode
	/// goes on top of the whole stack (PushPlayer, `keepsUnder` and `under` as its). The modes under it stay, never
	/// current again unless the new mode is popped. Returns the new mode's number; 0 (nothing done) when the script mode
	/// is not the current one
	uint32_t EndScript(bool keepsUnder = false, std::size_t under = 0);
	/// The turn's validity check, from the bottom of the stack: every dual camera `valid` says is no longer valid is
	/// deleted (the script mode stays while it is on the stack). True when the current mode was among them
	bool RemoveInvalid(const std::function<bool(const DualMode&)>& valid);
	/// The turn's validity check runs over the whole stack once: from BeginValidityPass to EndValidityPass the current
	/// mode is the one that was current as the pass began, whatever the pass deletes before reaching it. The removals
	/// answer "it was the current mode" for that one only, so only its cleanup runs, as the original's pass from the
	/// bottom, which meets the current mode last, after every mode under it
	void BeginValidityPass();
	void EndValidityPass() { _passTop.reset(); }
	/// The script camera's modes go. The other systems' entries stay: their own systems take them off
	void Clear()
	{
		_modes.clear();
		_playersModeGone = false;
		_otherSystemsDrops = 0;
	}

	/// The other systems' entries, Creature Mode's follows (Kind::Follow), the placed paths (Kind::Path) and the dance
	/// cameras (Kind::Dance), in the order
	/// they were pushed, the first the lowest. Each takes its place in the stack's one push order (StackedMode::pushed)
	/// with the script camera's modes: one pushed after them is the current mode (OtherCurrent), one pushed before them
	/// is under them. They are counted in the `under` of Push and the rest, and a push onto a full stack deletes the
	/// lowest of them when it was pushed before the script camera's lowest (TakeOtherSystemsDrops, LowestOther). Their
	/// own systems push and take off their entries, and keep their own data by them (a path's playback by its key, a
	/// follow's model by its key)
	[[nodiscard]] const std::vector<StackedMode>& OtherEntries() const { return _others; }
	[[nodiscard]] std::size_t OtherCount() const { return _others.size(); }
	/// A placed path's entry, on top of the whole stack. On a full stack the bottom mode is deleted first, as Push's
	/// (the other entries under it counted), whatever its kind; returns what was deleted
	Dropped PushPath(uint64_t path);
	/// The placed path's entry goes; nothing when it has none. True when it was the current mode
	bool RemovePath(uint64_t path);
	/// A follow's entry, by its key, on top of the whole stack: every follow is a mode of its own, a follow already on
	/// the stack (of the same thing or another) staying where it is, under it. On a full stack the bottom mode is deleted
	/// first, as PushPath's; returns what was deleted
	Dropped PushFollow(uint64_t follow = 0);
	/// The follow's entry goes (the newest one with that key); nothing when it has none. True when it was the current mode
	bool RemoveFollow(uint64_t follow = 0);
	/// A follow is on the stack, current or not
	[[nodiscard]] bool FollowOnStack() const;
	/// A follow's entry is the current mode
	[[nodiscard]] bool FollowCurrent() const;
	/// The key of the follow that is the current mode; none when the current mode is not a follow
	[[nodiscard]] std::optional<uint64_t> CurrentFollow() const;
	/// A follow's entry was pushed after the placed path's, so the path is under it; false when either has none
	[[nodiscard]] bool FollowOverPath(uint64_t path) const;
	/// A follow's or a dance camera's entry was pushed after the placed path's, so the path is under it; false when the
	/// path has no entry
	[[nodiscard]] bool OverPath(uint64_t path) const;
	/// The dance camera of `dance`, on top of the whole stack: the current mode. On a full stack the bottom mode is
	/// deleted first, as PushPath's; returns what was deleted
	Dropped PushDance(entt::entity dance);
	/// The dance camera of `dance` goes; nothing when it has none. True when it was the current mode
	bool RemoveDance(entt::entity dance);
	/// The dance camera of `dance` ends: it stays on the stack, no longer valid, until the turn's check deletes it
	/// (RemoveInvalidDances)
	void EndDance(entt::entity dance);
	/// The dance whose camera is the current mode; none when the current mode is not a dance camera
	[[nodiscard]] std::optional<entt::entity> CurrentDance() const;
	/// The turn's validity check of the dance cameras, from the bottom of the stack: every one that has ended, or whose
	/// dance `there` says is gone, is deleted. True when the current mode was among them
	bool RemoveInvalidDances(const std::function<bool(entt::entity)>& there);
	/// Every dance camera goes (the map's clear)
	void ClearDances();
	/// The lowest of the other entries, the first a push onto a full stack deletes; none when there is none
	[[nodiscard]] std::optional<StackedMode> LowestOther() const;
	/// A new player's mode `playersMode` goes over every placed path not under one already. True when one was
	bool CoverPaths(uint32_t playersMode);
	/// The new player's mode `playersMode` is popped: the placed paths it went over are on the stack again (none for 0)
	void UncoverPaths(uint32_t playersMode);
	/// The number of the new player's mode put over the placed path, 0 while none is or the path has no entry
	[[nodiscard]] uint32_t PathUnder(uint64_t path) const;

private:
	/// A mode deleted from the bottom of a full stack: a new player's mode over it no longer keeps it
	void LostUnder(std::size_t from);
	/// What making room for a new mode deleted from the bottom of a full stack
	struct Room
	{
		Dropped dropped = Dropped::None;
		/// A player's mode was deleted: the lowest new player's mode left over it no longer keeps it (LostUnder)
		bool playersModeLost = false;
	};
	/// Room for one more mode (`under`: as Push's): on a full stack the bottom mode is deleted and the rest move down a
	/// place, whatever the kind of the mode about to go on top
	Room MakeRoom(std::size_t under);
	/// The top of the script camera's modes is the current mode: pushed after every other system's entry
	[[nodiscard]] bool OwnCurrent() const
	{
		return !_modes.empty() && (_others.empty() || _modes.back().pushed > _others.back().pushed);
	}
	/// The next place in the push order
	uint64_t NextPushed() { return ++_lastPushed; }
	/// Whether `mode`, about to be deleted, is the current mode: during a validity pass the one current as it began,
	/// else `liveCurrent`, the stack's own answer as it is
	[[nodiscard]] bool WasCurrent(const StackedMode& mode, bool liveCurrent) const
	{
		return _passTop.has_value() ? *_passTop != 0 && mode.pushed == *_passTop : liveCurrent;
	}

	std::vector<StackedMode> _modes; ///< The last the current one
	/// The last place in the push order given (StackedMode::pushed)
	uint64_t _lastPushed = 0;
	/// During a validity pass, the push number of the mode current as it began (0: the player's own mode, the bottom)
	std::optional<uint64_t> _passTop;
	bool _playersModeGone = false;
	/// The other systems' modes deleted from the bottom and not yet taken (TakeOtherSystemsDrops)
	std::size_t _otherSystemsDrops = 0;
	uint32_t _nextPlayerId = 1;
	/// The other systems' entries, the first the lowest (OtherEntries)
	std::vector<StackedMode> _others;
};

} // namespace openblack::camera_stack
