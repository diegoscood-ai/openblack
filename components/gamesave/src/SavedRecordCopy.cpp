/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A saved record's copies, apart from its other operations: each is a switch over every record class
// (docs/wiki/engine/saved-games.md#openblack)

#include "SavedRecords.h"

openblack::gamesave::SavedRecord::SavedRecord(const SavedRecord& other) = default;
openblack::gamesave::SavedRecord& openblack::gamesave::SavedRecord::operator=(const SavedRecord& other) = default;
