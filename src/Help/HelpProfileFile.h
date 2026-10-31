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

#include <HelpStatsFile.h>

#include "Help/HelpProfile.h"

/// The help record as a player's profile stores it in helpstats.dat
namespace openblack::help::profile
{

/// Every event's count as the profile stores it. The clock is not stored: each game starts it again.
[[nodiscard]] helpstats::HelpStats ToHelpStats(const HelpProfile& profile);

/// Every event's count from the profile's record. The clock stays as it is.
void FromHelpStats(HelpProfile& profile, const helpstats::HelpStats& stats);

/// The file a player's profile keeps its help record in, in the profile's own folder
constexpr std::string_view k_FileName = "helpstats.dat";

/// The bytes of the profile's file for the record
[[nodiscard]] std::vector<uint8_t> FileOf(const HelpProfile& profile);

/// Takes the record from a profile's file, as the game reads it: as many bytes as the file holds, up to the record's
/// size, over the record kept; with no file the record stays as it is. Whether there was a file
bool TakeFile(HelpProfile& profile, const std::optional<std::vector<uint8_t>>& file);

/// The help record is written into the file of the profile played (Locator::playerProfileSystem), as the game does each
/// time it writes the profile back; with no profile kept (a run of the checks) nothing is written
void KeepInProfile();
/// The help record is read again from the file of the profile played; with no file it stays as it is
void TakeFromProfile();

} // namespace openblack::help::profile
