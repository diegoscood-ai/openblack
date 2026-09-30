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

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "PSysFile.h"

// The SOUND_ACTION property of the spell files and its value (docs/bw1-notes/magic.md, "Sonido de las partículas").

namespace openblack::psys
{

/// PSysSoundAction (0x18 bytes): the SoundAction.h value and the attribute slots handed to the .sad anim effect table
struct SoundAction
{
	enum Flag : uint8_t
	{
		Looping = 0x01,      ///< LOOPING: the update keeps re-issuing it while the atom lives
		Delayed = 0x02,      ///< set by code (thunder): starts distance / 347 s later
		SoftRelease = 0x04,  ///< SOFTRELEASE: when the atom goes, the loop ends its pass instead of being cut
		UseSurface = 0x08,   ///< USESURFACE: the surface slot is GSoundMap::GetSurfaceType of the atom
		SnapToGround = 0x20, ///< set by code (thunder): the position's height is the land's
	};

	int32_t action {-1};    ///< +0x00 LHSoundAction, -1 = NO_SOUND
	int32_t surface {1};    ///< +0x04
	int32_t size {2};       ///< +0x08 size class: 1 large, 2 medium, 3 small (set by the rule that plays it)
	int32_t alignment {2};  ///< +0x0C replaced by the owner player's discrete alignment when there is one
	int32_t fadeStep {0};   ///< +0x10 volume taken off per turn once the atom is gone (0..127 units)
	uint8_t flags {0};      ///< +0x14
};

/// SoundActionProperty::ReadProperty 0x585A70: the action from its name (unknown or NO_SOUND -> -1) and the LOOPING,
/// SOFTRELEASE and USESURFACE bits; ONLYONE is read and dropped. The slots keep the PSysSoundAction defaults
/// (inline ctor, e.g. CreateRuleAnAtom 0x69F350). A missing property is the default: NO_SOUND.
[[nodiscard]] SoundAction ReadSoundAction(const Object& object, std::string_view key);

/// LHParseFile::FindEnumVal 0x7BE530 on Data\SoundAction.h, parsed once (fn_00585590); -1 if the name is not there
[[nodiscard]] int32_t SoundActionValue(std::string_view name);
/// The name of a value, for the logs ("?" if none)
[[nodiscard]] std::string SoundActionName(int32_t value);
/// The enum of a C header: each NAME with its "= value", or the previous value + 1
[[nodiscard]] std::vector<std::pair<std::string, int32_t>> ParseEnumHeader(std::string_view text);

} // namespace openblack::psys
