/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CameraHelp.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "Locator.h"

namespace openblack::camera_help
{
namespace
{
/// The game's camera help (Locator::cameraHelpSystem). Release builds have no entt assert, so a missing slot stops the
/// game with its name rather than a null dereference
CameraHelp& CameraHelpData()
{
	if (!Locator::cameraHelpSystem::has_value())
	{
		std::fputs("camera_help: no camera help system in the locator (Locator::cameraHelpSystem)\n", stderr);
		std::abort();
	}
	return Locator::cameraHelpSystem::value().Get();
}

/// Nothing tilts within this of the way there; the tilt is a fifth of the way, at most the frame's seconds, and
/// becomes pitch input at this rate
constexpr float k_CloseEnough = 0.01f;
constexpr float k_ShareOfTheWay = 0.2f;
constexpr float k_InputPerTilt = -150.0f;
} // namespace

void CameraHelp::SetAutoPitch(float pitch, float height, bool on)
{
	// features = on ? 0x40 : 0, mask = on ? 0 : 0x40
	Enable(on ? Bit(Feature::AutoPitch) : 0, on ? 0 : Bit(Feature::AutoPitch));
	autoPitch = pitch;
	autoPitchHeight = height;
}

void CameraHelp::Reset()
{
	features = k_NormalFeatures;
	autoPitch = k_DefaultAutoPitchAngle;
	autoPitchHeight = k_DefaultAutoPitchDistance;
}

void EnableCameraFeatures(int32_t features, int32_t mask)
{
	CameraHelpData().Enable(features, mask);
}

void SetAutoPitch(float angle, float distance, bool on)
{
	CameraHelpData().SetAutoPitch(angle, distance, on);
}

int32_t GetEnabledFeatures()
{
	return CameraHelpData().features;
}

bool IsFeatureEnabled(Feature feature)
{
	return CameraHelpData().IsFeatureEnabled(feature);
}

float GetAutoPitchAngle()
{
	return CameraHelpData().autoPitch;
}

float GetAutoPitchDistance()
{
	return CameraHelpData().autoPitchHeight;
}

void Reset()
{
	CameraHelpData().Reset();
}

std::optional<float> AutoPitchInput(float targetPitch, float pitch, float deltaSeconds)
{
	const auto tilt = (targetPitch - pitch) * k_ShareOfTheWay;
	if (std::abs(tilt) <= k_CloseEnough)
	{
		return std::nullopt;
	}
	auto limited = tilt;
	if (limited <= -deltaSeconds)
	{
		limited = -deltaSeconds;
	}
	else if (deltaSeconds <= limited)
	{
		limited = deltaSeconds;
	}
	return limited * k_InputPerTilt;
}

} // namespace openblack::camera_help
