/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <array>
#include <span>
#include <utility>

/// The C runtime's quicksort as the game's runtime sorts: not stable, so where two elements compare equal their order
/// comes from the algorithm itself, which this reproduces step for step. Ranges of 8 or fewer elements are sorted by
/// picking the largest into the last place each time; larger ones take their middle element as the pivot, partition
/// around it and sort the smaller part next, keeping the larger one for later. Wiki:
/// docs/wiki/engine/game-clock.md#the-in-loop-logo.
namespace openblack::msvc_qsort
{

namespace detail
{
/// The largest of lo..hi goes to hi, then hi moves down
template <typename T, typename Compare>
void ShortSort(std::span<T> items, std::ptrdiff_t lo, std::ptrdiff_t hi, Compare& compare)
{
	while (hi > lo)
	{
		std::ptrdiff_t max = lo;
		for (std::ptrdiff_t p = lo + 1; p <= hi; ++p)
		{
			if (compare(items[static_cast<size_t>(p)], items[static_cast<size_t>(max)]) > 0)
			{
				max = p;
			}
		}
		std::swap(items[static_cast<size_t>(max)], items[static_cast<size_t>(hi)]);
		--hi;
	}
}
} // namespace detail

/// Sorts `items` with `compare(a, b)`, which returns less than 0, 0 or more than 0 as the runtime's comparators do
template <typename T, typename Compare>
void Sort(std::span<T> items, Compare compare)
{
	constexpr std::ptrdiff_t k_Cutoff = 8;
	constexpr size_t k_StackSize = 30;
	if (items.size() < 2)
	{
		return;
	}
	const auto at = [&items](std::ptrdiff_t i) -> T& { return items[static_cast<size_t>(i)]; };
	std::array<std::ptrdiff_t, k_StackSize> loStack {};
	std::array<std::ptrdiff_t, k_StackSize> hiStack {};
	size_t stack = 0;
	std::ptrdiff_t lo = 0;
	auto hi = static_cast<std::ptrdiff_t>(items.size()) - 1;
	for (;;)
	{
		const std::ptrdiff_t size = hi - lo + 1;
		bool sortedHere = true;
		if (size <= k_Cutoff)
		{
			detail::ShortSort(items, lo, hi, compare);
		}
		else
		{
			// the middle swapped to lo as the pivot
			std::swap(at(lo + size / 2), at(lo));
			std::ptrdiff_t loGuy = lo;
			std::ptrdiff_t hiGuy = hi + 1;
			for (;;)
			{
				do
				{
					++loGuy;
				} while (loGuy <= hi && compare(at(loGuy), at(lo)) <= 0);
				do
				{
					--hiGuy;
				} while (hiGuy > lo && compare(at(hiGuy), at(lo)) >= 0);
				if (hiGuy < loGuy)
				{
					break;
				}
				std::swap(at(loGuy), at(hiGuy));
			}
			std::swap(at(lo), at(hiGuy));
			// the smaller part is sorted next, the larger one kept for later
			if (hiGuy - lo > hi - loGuy)
			{
				if (lo + 1 < hiGuy)
				{
					loStack.at(stack) = lo;
					hiStack.at(stack) = hiGuy - 1;
					++stack;
				}
				if (loGuy < hi)
				{
					lo = loGuy;
					sortedHere = false;
				}
			}
			else
			{
				if (loGuy < hi)
				{
					loStack.at(stack) = loGuy;
					hiStack.at(stack) = hi;
					++stack;
				}
				if (lo + 1 < hiGuy)
				{
					hi = hiGuy - 1;
					sortedHere = false;
				}
			}
		}
		if (!sortedHere)
		{
			continue;
		}
		if (stack == 0)
		{
			return;
		}
		--stack;
		lo = loStack.at(stack);
		hi = hiStack.at(stack);
	}
}

} // namespace openblack::msvc_qsort
