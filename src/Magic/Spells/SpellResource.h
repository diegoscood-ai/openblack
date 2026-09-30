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

namespace openblack
{
struct GMagicResourceInfo;
} // namespace openblack

// SpellResource (0xF0 bytes, vtable 0x8F6AD8, GMagicResourceInfo::AllocSpell 0x5FAC20): the food and wood miracles
// (MAGIC_TYPE 14, 15 and 21). Their PSys (SF_Food / SF_Wood) sprinkles grains or logs from the hand; each one that lands
// is a SpellEvent 3 that pays for it and puts the resource down (Pot::AddResourceToPos). Wiki: docs/bw1-notes/magic.md.

namespace openblack::magic
{
/// SpellResource +0xEC: the first event is done (fn_00724C80 clears it at allocation; nothing sets it back)
struct SpellResourceData
{
	bool firstDone {false};
};

/// What one landing grain is worth (SpellResource::SpellEvent 0x724D80): n = resourceAmountFirstEvent the first time,
/// resourceAmountPerEvent after it; the chants paid are costPerUnit x n
struct ResourceEventCost
{
	uint32_t units;
	float chants;
};
[[nodiscard]] ResourceEventCost ResourceEvent(const GMagicResourceInfo& info, bool firstDone);

/// SpellResource::HasEnoughChantsAndLifeForRecast 0x724C90: costPerUnit x resourceAmountFirstEvent <= chants
[[nodiscard]] bool HasEnoughChantsForResourceRecast(const GMagicResourceInfo& info, float chants);
} // namespace openblack::magic
