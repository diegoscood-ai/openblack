/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BookmarkSteps.h"

using namespace openblack;

std::optional<uint8_t> bookmark_steps::Next(uint8_t current, std::span<const bool, k_SteppedSlots> isSet)
{
	for (size_t step = 1; step < k_Slots + 1; ++step)
	{
		const size_t slot = (current + step) % k_Slots;
		if (slot < k_SteppedSlots && isSet[slot])
		{
			return static_cast<uint8_t>(slot);
		}
	}
	return std::nullopt;
}

std::optional<uint8_t> bookmark_steps::Previous(uint8_t current, std::span<const bool, k_SteppedSlots> isSet)
{
	for (size_t step = k_Slots - 1; step > 0; --step)
	{
		const size_t slot = (current + step) % k_Slots;
		if (slot < k_SteppedSlots && isSet[slot])
		{
			return static_cast<uint8_t>(slot);
		}
	}
	return std::nullopt;
}

std::optional<size_t> bookmark_steps::NextTown(std::span<const Town> headToTail, std::optional<uint32_t> lastFlownTo)
{
	if (headToTail.empty())
	{
		return std::nullopt;
	}
	bool searchAny = !lastFlownTo.has_value();
	for (;;)
	{
		// from the tail towards the head
		for (size_t at = headToTail.size(); at-- > 0;)
		{
			if (!searchAny && headToTail[at].id != *lastFlownTo)
			{
				continue;
			}
			// the town before it in the list, the tail after the head
			const size_t next = at > 0 ? at - 1 : headToTail.size() - 1;
			if (headToTail[next].hasPlace)
			{
				return next;
			}
		}
		if (searchAny)
		{
			return std::nullopt;
		}
		searchAny = true;
	}
}
