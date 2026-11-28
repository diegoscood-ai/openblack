/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandLightFrame.h"

#include "3D/LandLight.h"
#include "3D/LandLightTable.h"

uint32_t openblack::FrameLandLight(uint8_t level)
{
	// The table's colours are 0xAARRGGBB
	return land_light::CurrentTable().GetRaw(level) & 0x00FFFFFFu;
}
