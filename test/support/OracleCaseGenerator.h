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

namespace openblack::test
{

/// The generator that made the inputs of a golden table run in the original executable, so that a test makes the same
/// inputs again and the table holds only the original's results: state = state x 1664525 + 1013904223 (mod 2^32). Every
/// value it gives is an exact float
struct OracleCaseGenerator
{
	uint32_t state;

	uint32_t Next()
	{
		state = state * 1664525U + 1013904223U;
		return state;
	}

	/// The top `bits` bits of the next value over 2^bits: in [0, 1)
	float Unit(int bits) { return static_cast<float>(Next() >> (32 - bits)) / static_cast<float>(uint32_t {1} << bits); }

	/// The top `bits` bits of the next value, less 2^(bits - 1), over 2^(bits - 1): in [-1, 1)
	float Signed(int bits)
	{
		const auto half = static_cast<int32_t>(uint32_t {1} << (bits - 1));
		return static_cast<float>(static_cast<int32_t>(Next() >> (32 - bits)) - half) / static_cast<float>(half);
	}
};

} // namespace openblack::test
