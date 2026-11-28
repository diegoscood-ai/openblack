/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VortexPullRules.h"

namespace openblack::vortex
{

bool CreatureMayBeTaken(bool letThrough, bool leashed, bool controlledByScript)
{
	return (letThrough && leashed) || controlledByScript;
}

bool TakesWhatStrikesIt(VortexType type, VortexStateType state, bool hitterFlying, bool hitterMayBeTaken)
{
	return type == VortexType::In && state == VortexStateType::Active && hitterFlying && hitterMayBeTaken;
}

} // namespace openblack::vortex
