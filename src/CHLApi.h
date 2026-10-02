/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <LHVMTypes.h>

#include "Help/ScriptControl.h"

namespace openblack::chlapi
{

using openblack::lhvm::DataType;
using openblack::lhvm::VMValue;

class CHLApi
{
public:
	CHLApi();

	[[nodiscard]] const std::vector<lhvm::NativeFunction>& GetFunctionsTable();

private:
	void InitFunctionsTable0();
	void InitFunctionsTable1();
	void InitFunctionsTable2();
	void InitFunctionsTable3();
	void InitFunctionsTable4();

	std::vector<lhvm::NativeFunction> _functionsTable;
};

/// The script VM as GScript asks it (ScriptDLL::TaskNumber 0x6F69F0, GetCurrentTaskScriptType 0x6F6A90, GetScriptType
/// 0x6F6C50, StopTasksOfType 0x6F68F0, PUSH 0x6F6BA0, StartScript 0x6F6880) on Locator::vm
[[nodiscard]] help::script_control::Vm ScriptVm();

} // namespace openblack::chlapi
