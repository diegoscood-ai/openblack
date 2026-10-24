/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The particles' exit guard: while the program runs, watching it never makes it say the program is exiting

#include <gtest/gtest.h>

#include "Particles/ParticleExitGuard.h"

namespace exit_guard = openblack::particles::exit_guard;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ParticleExitGuard, NotExitingWhileTheProgramRuns)
{
	EXPECT_FALSE(exit_guard::Exiting());
	exit_guard::Watch();
	exit_guard::Watch();
	EXPECT_FALSE(exit_guard::Exiting());
}
