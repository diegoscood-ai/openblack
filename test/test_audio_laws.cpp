/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Audio/LH/SamplePlay.h"

using namespace openblack::audio;

// The expected values come from a Unicorn emulation of the original DLLs (dev\tmp_dis\agua\re\emu_qmixer.py and
// emu_polar.py): QMixer 0x1802CE50, LHaudiodllR 0x100133C1 and 0x100122BC..0x10012522 + QMixer 0x1800AA85.

TEST(AudioLaws, QMixerVolume)
{
	// floor(127 * v / 127) * 258 / 32767
	EXPECT_NEAR(sample_play::QMixerGain(127), 32766.0f / 32767.0f, 1e-6f);
	EXPECT_NEAR(sample_play::QMixerGain(60), 15480.0f / 32767.0f, 1e-6f);
	EXPECT_NEAR(sample_play::QMixerGain(20), 5160.0f / 32767.0f, 1e-6f);
	EXPECT_NEAR(sample_play::QMixerGain(1), 258.0f / 32767.0f, 1e-6f);
	EXPECT_EQ(sample_play::QMixerGain(0), 0.0f);
	EXPECT_NEAR(sample_play::QMixerGain(200), 32766.0f / 32767.0f, 1e-6f);
}

TEST(AudioLaws, QMixerDistance)
{
	EXPECT_NEAR(sample_play::DistanceGain(50, 160, 4, 0), 1.0f, 1e-6f);
	EXPECT_NEAR(sample_play::DistanceGain(50, 160, 4, 50), 1.0f, 1e-6f);
	EXPECT_NEAR(sample_play::DistanceGain(50, 160, 4, 60), 0.5555556f, 1e-6f);
	EXPECT_NEAR(sample_play::DistanceGain(50, 160, 4, 100), 0.2f, 1e-6f);
	EXPECT_NEAR(sample_play::DistanceGain(50, 160, 4, 160), 0.1020408f, 1e-6f);
	EXPECT_EQ(sample_play::DistanceGain(50, 160, 4, 161), 0.0f);
	EXPECT_NEAR(sample_play::DistanceGain(1, 9999, 2, 2.828427f), 0.2147372f, 1e-6f);
	EXPECT_NEAR(sample_play::DistanceGain(1, 9999, 1, 5), 0.2f, 1e-6f);
	EXPECT_NEAR(sample_play::DistanceGain(1, 9999, 0, 10), 1.0f, 1e-6f);
	EXPECT_NEAR(sample_play::DistanceGain(60, 180, 0.3f, 200), 0.0f, 1e-6f);
}

TEST(AudioLaws, RelativeAxes)
{
	// LHaudio's relative (x, y, z) -> QMixer's (right, up, ahead)
	struct Case
	{
		glm::vec3 lh;
		glm::vec3 heard;
	};
	const Case cases[] = {
	    {{2, 2, 0}, {1.9992f, 0.0f, 2.0008f}},  {{-2, 1, 0}, {-1.9998f, 0.0f, 1.0005f}}, {{0, 4, 0}, {0, 0, 4}},
	    {{0, -4, 0}, {0, 0, -4}},               {{4, 0, 0}, {4, 0, 0}},                   {{-4, 0, 0}, {-4, 0, 0}},
	    {{-5, -5, 0}, {-4.9980f, 0.0f, -5.0020f}}, {{1, -2, 0}, {0.9989f, 0.0f, -2.0006f}},
	    {{2, -1.5f, 0}, {2.2358f, 0.0f, -1.1186f}}, {{0, 0, 3}, {0, 3, 0}}, {{1, 1, 1}, {0.9994f, 1.0004f, 1.0002f}},
	};
	for (const auto& c : cases)
	{
		const auto p = sample_play::PolarRelative(c.lh);
		EXPECT_NEAR(p.x, c.heard.x, 2e-4f) << c.lh.x << "," << c.lh.y << "," << c.lh.z;
		EXPECT_NEAR(p.y, c.heard.y, 2e-4f) << c.lh.x << "," << c.lh.y << "," << c.lh.z;
		EXPECT_NEAR(p.z, c.heard.z, 2e-4f) << c.lh.x << "," << c.lh.y << "," << c.lh.z;
	}
}
