/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CameraModeStack.h"

#include <algorithm>
#include <iterator>
#include <ranges>

namespace openblack::camera_stack
{

bool ModeStack::ScriptCurrent() const
{
	return OwnCurrent() && _modes.back().kind == StackedMode::Kind::Script;
}

bool ModeStack::DualCurrent() const
{
	return OwnCurrent() && _modes.back().kind == StackedMode::Kind::Dual;
}

bool ModeStack::InteractionCurrent() const
{
	return OwnCurrent() && _modes.back().kind == StackedMode::Kind::Interaction;
}

bool ModeStack::PlayerCurrent() const
{
	return OwnCurrent() && _modes.back().kind == StackedMode::Kind::Player;
}

bool ModeStack::InteractionOnStack() const
{
	return std::ranges::any_of(_modes, [](const StackedMode& mode) { return mode.kind == StackedMode::Kind::Interaction; });
}

DualMode* ModeStack::CurrentDual()
{
	return DualCurrent() ? &_modes.back().dual : nullptr;
}

bool ModeStack::ScriptOnStack() const
{
	// From the top down to the newest player's mode: a script mode under it has the camera again only if it is popped
	for (const auto& mode : std::ranges::reverse_view(_modes))
	{
		if (mode.kind == StackedMode::Kind::Player)
		{
			return false;
		}
		if (mode.kind == StackedMode::Kind::Script)
		{
			return true;
		}
	}
	return false;
}

std::size_t ModeStack::Places() const
{
	return (_playersModeGone ? 0 : 1) + _modes.size();
}

void ModeStack::LostUnder(std::size_t from)
{
	const auto next = std::ranges::find_if(_modes.begin() + static_cast<std::ptrdiff_t>(from), _modes.end(),
	                                       [](const StackedMode& mode) { return mode.kind == StackedMode::Kind::Player; });
	if (next != _modes.end())
	{
		next->keepsUnder = false;
	}
}

ModeStack::Room ModeStack::MakeRoom(std::size_t under)
{
	Room room;
	if (Places() + under + 1 > k_Places)
	{
		// The stack is full: the bottom mode is deleted and the rest move down a place
		if (!_playersModeGone)
		{
			_playersModeGone = true;
			room.dropped = Dropped::PlayersMode;
			room.playersModeLost = true;
		}
		else if (under != 0 && (_others.empty() || _modes.empty() || _others.front().pushed < _modes.front().pushed))
		{
			++_otherSystemsDrops; // its own system deletes it
			room.dropped = Dropped::OtherSystemsMode;
		}
		else if (!_modes.empty())
		{
			room.playersModeLost = _modes.front().kind == StackedMode::Kind::Player;
			_modes.front().dual.alive = false; // deleted
			_modes.erase(_modes.begin());
			room.dropped = Dropped::Own;
		}
	}
	return room;
}

ModeStack::Dropped ModeStack::Push(const StackedMode& mode, std::size_t under)
{
	const auto room = MakeRoom(under);
	_modes.push_back(mode);
	_modes.back().pushed = NextPushed();
	// A player's mode deleted from the bottom: the new player's mode over it, the lowest one, no longer keeps it
	if (room.playersModeLost)
	{
		LostUnder(0);
	}
	return room.dropped;
}

uint32_t ModeStack::PushPlayer(bool keepsUnder, std::size_t under)
{
	// Only a player's mode still on the stack is kept: the bottom one, or a new one put on top before
	const bool playersModeUnder = !_playersModeGone || std::ranges::any_of(_modes, [](const StackedMode& mode) {
		return mode.kind == StackedMode::Kind::Player;
	});
	const auto id = _nextPlayerId++;
	Push({.kind = StackedMode::Kind::Player, .id = id, .keepsUnder = keepsUnder && playersModeUnder}, under);
	return id;
}

std::optional<ModeStack::PlayerPop> ModeStack::PopPlayersMode()
{
	if (_modes.empty())
	{
		return PlayerPop {}; // the bottom one: nothing under it
	}
	if (!PlayerCurrent())
	{
		return std::nullopt;
	}
	const PlayerPop popped {.id = _modes.back().id, .keptUnder = _modes.back().keepsUnder};
	_modes.pop_back();
	return popped;
}

bool ModeStack::HasPlayer(uint32_t id) const
{
	return std::ranges::any_of(
	    _modes, [id](const StackedMode& mode) { return mode.kind == StackedMode::Kind::Player && mode.id == id; });
}

bool ModeStack::KeepUnder(uint32_t id)
{
	bool playersModeUnder = !_playersModeGone;
	for (auto& mode : _modes)
	{
		if (mode.kind != StackedMode::Kind::Player)
		{
			continue;
		}
		if (mode.id == id)
		{
			mode.keepsUnder = playersModeUnder;
			return mode.keepsUnder;
		}
		playersModeUnder = true;
	}
	return false;
}

bool ModeStack::KeepsModel(uint32_t id) const
{
	return std::ranges::any_of(_modes, [id](const StackedMode& mode) {
		return mode.kind == StackedMode::Kind::Player && mode.id == id && mode.keepsUnder;
	});
}

bool ModeStack::NewPlayersModeIfGone(std::size_t under)
{
	if (!_playersModeGone || under != 0 || !_modes.empty())
	{
		return false;
	}
	_playersModeGone = false;
	return true;
}

bool ModeStack::PopDual()
{
	if (!DualCurrent())
	{
		return false;
	}
	_modes.back().dual.alive = false; // deleted
	_modes.pop_back();                // popped off the stack
	return true;
}

ModeStack::Popped ModeStack::PopScript(std::size_t under)
{
	if (!ScriptCurrent())
	{
		return Popped::None;
	}
	_modes.pop_back();
	return NewPlayersModeIfGone(under) ? Popped::NewPlayersMode : Popped::ModeUnder;
}

ModeStack::Popped ModeStack::PopInteraction(std::size_t under)
{
	if (!InteractionCurrent())
	{
		return Popped::None;
	}
	_modes.pop_back();
	return NewPlayersModeIfGone(under) ? Popped::NewPlayersMode : Popped::ModeUnder;
}

bool ModeStack::PopPlayer()
{
	if (!PlayerCurrent())
	{
		return false;
	}
	_modes.pop_back();
	return true;
}

uint32_t ModeStack::EndScript(bool keepsUnder, std::size_t under)
{
	if (!ScriptCurrent())
	{
		return 0;
	}
	// The new player's mode goes on top while the script mode, no longer valid, still takes its places, which the turn's
	// check then frees: the rest stay under the new mode
	const auto id = PushPlayer(keepsUnder, under);
	const auto script = std::ranges::find_if(_modes.rbegin() + 1, _modes.rend(), [](const StackedMode& mode) {
		return mode.kind == StackedMode::Kind::Script && !mode.dead;
	});
	if (script != _modes.rend())
	{
		script->dead = true;
	}
	return id;
}

bool ModeStack::RemoveInvalid(const std::function<bool(const DualMode&)>& valid)
{
	bool current = false;
	// only the top of the script camera's modes, and only while no other system's entry is over it
	const bool ownCurrent = OwnCurrent();
	for (std::size_t i = 0; i < _modes.size();) // from the bottom of the stack
	{
		// a dual camera whose things went, and a script mode whose script's camera ended
		const bool gone = (_modes[i].kind == StackedMode::Kind::Dual && !valid(_modes[i].dual)) ||
		                  (_modes[i].kind == StackedMode::Kind::Script && _modes[i].dead);
		if (gone)
		{
			current = current || WasCurrent(_modes[i], ownCurrent && i + 1 == _modes.size());
			_modes.erase(_modes.begin() + static_cast<std::ptrdiff_t>(i));
		}
		else
		{
			++i;
		}
	}
	return current;
}

ModeStack::Dropped ModeStack::PushPath(uint64_t path)
{
	// The one push of every mode: the other entries already on the stack are counted under it
	const auto room = MakeRoom(_others.size());
	_others.push_back({.kind = StackedMode::Kind::Path, .path = path, .pushed = NextPushed()});
	if (room.playersModeLost)
	{
		LostUnder(0);
	}
	return room.dropped;
}

void ModeStack::BeginValidityPass()
{
	// the top of the push order, whatever its owner; none on the stack: the player's own mode
	uint64_t top = 0;
	if (!_modes.empty())
	{
		top = _modes.back().pushed;
	}
	if (!_others.empty())
	{
		top = std::max(top, _others.back().pushed);
	}
	_passTop = top;
}

bool ModeStack::RemovePath(uint64_t path)
{
	const auto entry = std::ranges::find_if(
	    _others, [path](const StackedMode& other) { return other.kind == StackedMode::Kind::Path && other.path == path; });
	if (entry == _others.end())
	{
		return false;
	}
	const bool current = WasCurrent(*entry, OtherCurrent() && std::next(entry) == _others.end());
	_others.erase(entry);
	return current;
}

ModeStack::Dropped ModeStack::PushFollow(uint64_t follow)
{
	const auto room = MakeRoom(_others.size());
	_others.push_back({.kind = StackedMode::Kind::Follow, .follow = follow, .pushed = NextPushed()});
	if (room.playersModeLost)
	{
		LostUnder(0);
	}
	return room.dropped;
}

bool ModeStack::RemoveFollow(uint64_t follow)
{
	const auto entry = std::find_if(_others.rbegin(), _others.rend(), [follow](const StackedMode& other) {
		return other.kind == StackedMode::Kind::Follow && other.follow == follow;
	});
	if (entry == _others.rend())
	{
		return false;
	}
	const bool current = WasCurrent(*entry, OtherCurrent() && entry == _others.rbegin());
	_others.erase(std::next(entry).base());
	return current;
}

bool ModeStack::FollowCurrent() const
{
	return CurrentFollow().has_value();
}

std::optional<uint64_t> ModeStack::CurrentFollow() const
{
	if (!OtherCurrent() || _others.back().kind != StackedMode::Kind::Follow)
	{
		return std::nullopt;
	}
	return _others.back().follow;
}

bool ModeStack::FollowOverPath(uint64_t path) const
{
	const auto entry = std::ranges::find_if(
	    _others, [path](const StackedMode& other) { return other.kind == StackedMode::Kind::Path && other.path == path; });
	return entry != _others.end() && std::any_of(std::next(entry), _others.end(), [](const StackedMode& other) {
		       return other.kind == StackedMode::Kind::Follow;
	       });
}

bool ModeStack::OverPath(uint64_t path) const
{
	const auto entry = std::ranges::find_if(
	    _others, [path](const StackedMode& other) { return other.kind == StackedMode::Kind::Path && other.path == path; });
	return entry != _others.end() && std::any_of(std::next(entry), _others.end(), [](const StackedMode& other) {
		       return other.kind == StackedMode::Kind::Follow || other.kind == StackedMode::Kind::Dance;
	       });
}

ModeStack::Dropped ModeStack::PushDance(entt::entity dance)
{
	const auto room = MakeRoom(_others.size());
	_others.push_back({.kind = StackedMode::Kind::Dance, .dance = dance, .pushed = NextPushed()});
	if (room.playersModeLost)
	{
		LostUnder(0);
	}
	return room.dropped;
}

bool ModeStack::RemoveDance(entt::entity dance)
{
	const auto entry = std::ranges::find_if(
	    _others, [dance](const StackedMode& other) { return other.kind == StackedMode::Kind::Dance && other.dance == dance; });
	if (entry == _others.end())
	{
		return false;
	}
	const bool current = OtherCurrent() && std::next(entry) == _others.end();
	_others.erase(entry);
	return current;
}

void ModeStack::EndDance(entt::entity dance)
{
	for (auto& other : _others)
	{
		if (other.kind == StackedMode::Kind::Dance && other.dance == dance)
		{
			other.dead = true;
		}
	}
}

std::optional<entt::entity> ModeStack::CurrentDance() const
{
	if (!OtherCurrent() || _others.back().kind != StackedMode::Kind::Dance)
	{
		return std::nullopt;
	}
	return _others.back().dance;
}

bool ModeStack::RemoveInvalidDances(const std::function<bool(entt::entity)>& there)
{
	bool current = false;
	const bool otherCurrent = OtherCurrent();
	for (std::size_t i = 0; i < _others.size();) // from the bottom of the stack
	{
		const auto& other = _others[i];
		if (other.kind == StackedMode::Kind::Dance && (other.dead || !there(other.dance)))
		{
			current = current || WasCurrent(other, otherCurrent && i + 1 == _others.size());
			_others.erase(_others.begin() + static_cast<std::ptrdiff_t>(i));
		}
		else
		{
			++i;
		}
	}
	return current;
}

void ModeStack::ClearDances()
{
	std::erase_if(_others, [](const StackedMode& other) { return other.kind == StackedMode::Kind::Dance; });
}

bool ModeStack::FollowOnStack() const
{
	return std::ranges::any_of(_others, [](const StackedMode& other) { return other.kind == StackedMode::Kind::Follow; });
}

std::optional<StackedMode> ModeStack::LowestOther() const
{
	return _others.empty() ? std::nullopt : std::optional(_others.front());
}

bool ModeStack::CoverPaths(uint32_t playersMode)
{
	bool covered = false;
	for (auto& other : _others)
	{
		if (other.kind == StackedMode::Kind::Path && other.under == 0)
		{
			other.under = playersMode;
			covered = true;
		}
	}
	return covered;
}

void ModeStack::UncoverPaths(uint32_t playersMode)
{
	if (playersMode == 0)
	{
		return;
	}
	for (auto& other : _others)
	{
		if (other.kind == StackedMode::Kind::Path && other.under == playersMode)
		{
			other.under = 0;
		}
	}
}

uint32_t ModeStack::PathUnder(uint64_t path) const
{
	const auto entry = std::ranges::find_if(
	    _others, [path](const StackedMode& other) { return other.kind == StackedMode::Kind::Path && other.path == path; });
	return entry != _others.end() ? entry->under : 0;
}

} // namespace openblack::camera_stack
