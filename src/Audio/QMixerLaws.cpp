/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "QMixerLaws.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>

using namespace openblack::audio;

float qmixer::Gain(int volume, int master)
{
	const int v = std::clamp(volume, 0, k_MaxVolume);
	const int m = std::clamp(master, 0, k_MaxVolume);
	// floor(m * v / 127) * 258 (0x100133C1..0x100133E3, 0x1001511D..0x10015143), / 32767 (0x18007AE8)
	return static_cast<float>(m * v / 127 * 258) / 32767.0f;
}

float qmixer::DistanceGain(float minDistance, float maxDistance, float scale, float distance)
{
	if (distance > maxDistance)
	{
		return 0.0f;
	}
	if (distance <= minDistance || scale == 0.0f)
	{
		return 1.0f;
	}
	if (scale == 1.0f)
	{
		return minDistance / distance;
	}
	return minDistance / ((distance - minDistance) * scale + minDistance);
}

glm::vec3 qmixer::PolarRelative(glm::vec3 position)
{
	// 0x100122BC..0x10012522 in doubles, the angles in degrees with LHaudio's 180 * 0.318471 (not 180 / pi)
	constexpr double k_Degrees = 180.0 * 0.318471;
	const double x = position.x;
	const double y = position.y;
	const double z = position.z;
	const double range = std::sqrt(x * x + y * y + z * z);
	double elevation = 0.0;
	if (z != 0.0)
	{
		if (y == 0.0 && x == 0.0)
		{
			elevation = z < 0.0 ? -90.0 : 90.0;
		}
		else
		{
			elevation = std::atan(z / std::sqrt(x * x + y * y)) * k_Degrees;
		}
	}
	const auto ftolAbs = [](double v) { return static_cast<double>(std::abs(static_cast<int32_t>(v))); };
	double azimuth = 0.0;
	if (x == 0.0)
	{
		azimuth = y < 0.0 ? 180.0 : 0.0;
	}
	else if (y == 0.0)
	{
		azimuth = x > 0.0 ? 90.0 : 270.0;
	}
	else if (x > 0.0)
	{
		azimuth = y > 0.0 ? 90.0 - std::atan(y / x) * k_Degrees : 90.0 + std::atan(ftolAbs(y) / x) * k_Degrees;
	}
	else
	{
		azimuth = y < 0.0 ? 270.0 - std::atan(ftolAbs(y) / ftolAbs(x)) * k_Degrees
		                  : 270.0 + std::atan(y / ftolAbs(x)) * k_Degrees;
	}
	// QMixer 0x1800AA85: the angles * pi / 180 (the exact pi)
	constexpr double k_Radians = 3.14159265358979323846 / 180.0;
	const double az = azimuth * k_Radians;
	const double el = elevation * k_Radians;
	const double flat = std::cos(el) * range;
	return {static_cast<float>(std::sin(az) * flat), static_cast<float>(std::sin(el) * range),
	        static_cast<float>(std::cos(az) * flat)};
}

float qmixer::FrequencyRatio(int sampleRate, int percent)
{
	if (sampleRate <= 0)
	{
		return static_cast<float>(percent) / 100.0f;
	}
	const auto frequency = static_cast<uint32_t>(sampleRate) * static_cast<uint32_t>(std::max(percent, 0)) / 100u;
	return static_cast<float>(frequency) / static_cast<float>(sampleRate);
}

int qmixer::StartPitch(int pitch, int deviation, int rand15)
{
	auto p = static_cast<uint32_t>(pitch > 0 ? pitch : 100);
	const uint32_t d = static_cast<uint32_t>(std::max(deviation, 0)) * p / 100;
	p -= d;
	p += static_cast<uint32_t>(std::clamp(rand15, 0, 32767)) * (2 * d) / 32767;
	return p == 0 ? 100 : static_cast<int>(p);
}
