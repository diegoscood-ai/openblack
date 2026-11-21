/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A saved record's making, destruction and comparison, apart from its copies and moves: each is a switch over
// every record class (docs/wiki/engine/saved-games.md#openblack)

#include "SavedRecords.h"

openblack::gamesave::SavedRecord::SavedRecord() = default;
openblack::gamesave::SavedRecord::~SavedRecord() = default;

bool openblack::gamesave::operator==(const SavedRecord& left, const SavedRecord& right)
{
	return static_cast<const SavedRecordVariant&>(left) == static_cast<const SavedRecordVariant&>(right);
}
