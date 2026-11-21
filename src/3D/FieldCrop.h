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

/// A field's crop: once sown, it ages and fills with food, until it is ripe. It shows once it has grown a little,
/// rising out of the ground as it fills, green while young, turning to its own colour as it ripens, and swaying once
/// ripe.
namespace openblack::field_crop
{

/// How the crop looks while it shows
struct Look
{
	/// The colour, 0xRRGGBB, the land's light on it is multiplied by
	uint32_t tint;
	/// How far it stands out of the ground, from -1 when empty to 0 when full
	float height;
	/// Ripe crops sway in the wind
	bool sways;
};

} // namespace openblack::field_crop
