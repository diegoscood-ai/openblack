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

#include <optional>
#include <string_view>
#include <vector>

#include <LHVMTypes.h>
#include <entt/entity/entity.hpp>

#include "Help/ScriptControl.h"
#include "ScriptHeaders/NativeStubCalls.h"

namespace openblack::chlapi
{

using openblack::lhvm::DataType;
using openblack::lhvm::VMValue;

class CHLApi
{
public:
	CHLApi();

	[[nodiscard]] const std::vector<lhvm::NativeFunction>& GetFunctionsTable();
	/// The help system starts a help script by name, unless a script holds the dialogue: a help script holding it is
	/// stopped first. True when it started.
	static bool StartHelpScript(std::string_view name);
	/// A challenge's scroll tapped again: its record's reminder script starts with its arguments (StartChallengeScript,
	/// the camera left as it is), unless a script holds the dialogue (a help script holding it is stopped first). True
	/// when it started; false too before the challenge has a record, or when the record has no reminder script.
	static bool StartChallengeReminder(uint32_t challenge);
	/// A challenge's reminder script starts with its record's arguments, as the temple's challenge room replays it and a
	/// started scroll's tap does. With moveCamera (the room's replay), the camera is first put where the challenge's
	/// picture was taken, unless a script has taken the player's keys. False before the challenge has a record, or when
	/// the record has no reminder script (nothing is pushed or started then; the camera still moves).
	static bool StartChallengeScript(uint32_t challenge, bool moveCamera);
	/// The player clicked the door of a temple room that is not open: the help that says so starts
	static void TellRoomNotAvailable();

	/// The scripts call a native: the one an unwritten native reports itself as
	void EnterNative(uint32_t native) { _currentNative = native; }
	/// The native being run isn't written (or this case of it isn't): logged the first time only, every call counted
	void NotImplemented(std::optional<int32_t> detail = std::nullopt);
	[[nodiscard]] const script::NativeStubCalls& GetStubCalls() const { return _stubCalls; }
	/// Logs how often each unwritten native was called, the most called first
	void LogStubCalls() const;

private:
	void InitFunctionsTable0();
	void InitFunctionsTable1();
	void InitFunctionsTable2();
	void InitFunctionsTable3();
	void InitFunctionsTable4();

	std::vector<lhvm::NativeFunction> _functionsTable;
	uint32_t _currentNative {0};
	script::NativeStubCalls _stubCalls;
};

/// The script VM as the game's script control asks it (task number, current task's script type, script type, stop
/// tasks of a type, push, start script) on Locator::vm
[[nodiscard]] help::script_control::Vm ScriptVm();

/// A thing's player as GET_PROPERTY's PLAYER answers it (1 for the first player, 0 for none or the neutral one, 1 for
/// a town none of whose buildings stands); nothing for a kind of thing whose player is not worked out yet
[[nodiscard]] std::optional<float> ThingPlayerProperty(entt::entity thing);

/// (debug) The last IS_OF_TYPE a script asked and its answer, kept in the debug hooks' store for the LHVM viewer's
/// Natives tab (nothing of the game's state)
struct IsOfTypeReadout
{
	uint32_t calls {0};
	uint32_t object {0};
	int32_t type {0};
	uint32_t subtype {0};
	bool answer {false};
};

} // namespace openblack::chlapi
