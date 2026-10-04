/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CameraHelp.h"

namespace openblack::camera_help
{
namespace
{
int32_t s_enabledFeatures = k_NormalFeatures;          // [0x9CDD6C]
float s_autoPitchParam1 = k_DefaultAutoPitchParam1;   // [0x9CDD64]
float s_autoPitchParam2 = k_DefaultAutoPitchParam2;   // [0x9CDD68]
} // namespace

void EnableCameraFeatures(int32_t features, int32_t mask)
{
	// 0x447430: not mask; and old; or features
	s_enabledFeatures = (s_enabledFeatures & ~mask) | features;
}

void SetAutoPitch(float param1, float param2, bool on)
{
	// 0x4473F0: features = on ? 0x40 : 0 (neg; sbb; and 0x40), mask = on ? 0 : 0x40 (neg; sbb; and -0x40; add 0x40)
	EnableCameraFeatures(on ? Bit(Feature::AutoPitch) : 0, on ? 0 : Bit(Feature::AutoPitch));
	s_autoPitchParam1 = param1;
	s_autoPitchParam2 = param2;
}

int32_t GetEnabledFeatures()
{
	return s_enabledFeatures;
}

bool IsFeatureEnabled(Feature feature)
{
	return (s_enabledFeatures & Bit(feature)) != 0;
}

float GetAutoPitchParam1()
{
	return s_autoPitchParam1;
}

float GetAutoPitchParam2()
{
	return s_autoPitchParam2;
}

void Reset()
{
	s_enabledFeatures = k_NormalFeatures;
	s_autoPitchParam1 = k_DefaultAutoPitchParam1;
	s_autoPitchParam2 = k_DefaultAutoPitchParam2;
}

} // namespace openblack::camera_help
