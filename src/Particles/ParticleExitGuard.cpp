/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ParticleExitGuard.h"

namespace openblack::particles::exit_guard
{
namespace
{
bool g_Exiting = false;

struct Guard
{
	Guard() = default;
	Guard(const Guard&) = delete;
	Guard& operator=(const Guard&) = delete;
	Guard(Guard&&) = delete;
	Guard& operator=(Guard&&) = delete;
	~Guard() { g_Exiting = true; }
};
} // namespace

void Watch()
{
	static Guard guard;
}

bool Exiting()
{
	return g_Exiting;
}
} // namespace openblack::particles::exit_guard
