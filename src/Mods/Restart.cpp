/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Restart.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace openblack::mods::restart
{
namespace
{
bool g_requested = false;
}

void Request()
{
	g_requested = true;
}

bool Requested()
{
	return g_requested;
}

bool Relaunch([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
#if defined(_WIN32)
	// the same command line (wide, as Windows gave it) and the same working folder
	wchar_t exe[MAX_PATH] = {};
	if (GetModuleFileNameW(nullptr, exe, MAX_PATH) == 0)
	{
		return false;
	}
	STARTUPINFOW startup {};
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION process {};
	if (CreateProcessW(exe, GetCommandLineW(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process) == 0)
	{
		return false;
	}
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	return true;
#else
	if (argc < 1 || argv == nullptr || argv[0] == nullptr)
	{
		return false;
	}
	execv("/proc/self/exe", argv); // returns only on failure
	execvp(argv[0], argv);
	return false;
#endif
}

} // namespace openblack::mods::restart
