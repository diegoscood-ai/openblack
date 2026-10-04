/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <gtest/gtest.h>

#include "ECS/ScriptHighlight.h"

// ScriptHighlight's pure rules (runblack.exe W120, ScriptHighlight.cpp 0x7095D0..0x70AE60), against the values read
// from the binary (dev\documentacion\intro\spec_highlight.md).

using namespace openblack;
namespace sh = openblack::ecs::script_highlight;

TEST(ScriptHighlight, DidYouKnowIsRowOne)
{
	// IsDidYouKnow 0x70AC20: (info - 0xD96390) / 272 == 1
	EXPECT_FALSE(sh::IsDidYouKnowInfo(0));
	EXPECT_TRUE(sh::IsDidYouKnowInfo(1));
	EXPECT_FALSE(sh::IsDidYouKnowInfo(2));
	EXPECT_FALSE(sh::IsDidYouKnowInfo(3));
}

TEST(ScriptHighlight, SpriteAlphaByRow)
{
	// Draw 0x70A3E2..0x70A416
	EXPECT_EQ(sh::SpriteAlpha(0), 0x00);
	EXPECT_EQ(sh::SpriteAlpha(1), 0x32);
	EXPECT_EQ(sh::SpriteAlpha(2), 0x96);
	EXPECT_EQ(sh::SpriteAlpha(3), 0x64);
	EXPECT_EQ(sh::SpriteAlpha(4), 0x00);
}

TEST(ScriptHighlight, ScrollsGrowWithTheCameraDistance)
{
	// Draw 0x709E5D..0x709EA6: the whole metres bound to [10, 30], x 1 / 30
	EXPECT_FLOAT_EQ(sh::DistanceScale(1.0f, 3.0f, false), 10.0f * 0.0333333f);
	EXPECT_FLOAT_EQ(sh::DistanceScale(1.0f, 20.9f, false), 20.0f * 0.0333333f);
	EXPECT_FLOAT_EQ(sh::DistanceScale(2.0f, 500.0f, false), 30.0f * 2.0f * 0.0333333f);
	// a did-you-know keeps its scale
	EXPECT_FLOAT_EQ(sh::DistanceScale(1.5f, 500.0f, true), 1.5f);
}

TEST(ScriptHighlight, SpinWrapsByTruncation)
{
	// pi / 1000 a game ms; minus ftol(a / 2 pi) turns
	EXPECT_NEAR(sh::SpinAngle(0.0f, 100), 0.314159f, 1e-5f);
	EXPECT_NEAR(sh::SpinAngle(6.2f, 100), 6.2f + 0.314159f - 6.28319f, 1e-4f);
	// a negative angle is not brought into [0, 2 pi) (ftol truncates towards 0)
	EXPECT_NEAR(sh::SpinAngle(-1.0f, 0), -1.0f, 1e-6f);
}

TEST(ScriptHighlight, ChallengeIdsThatSave)
{
	// fn_0070AC50
	for (uint32_t id = 0; id < 0x50; ++id)
	{
		const bool expected = id == 0x38 || id == 0x3B || id == 0x3C || id == 0x3D;
		EXPECT_EQ(sh::SavesGameWhenClicked(id), expected) << id;
	}
}

TEST(ScriptHighlight, ValidToTap)
{
	// InterfaceValidToTap 0x70ADD0
	EXPECT_TRUE(sh::ValidToTap(true, false, 4143));
	EXPECT_FALSE(sh::ValidToTap(true, true, 0));
	EXPECT_FALSE(sh::ValidToTap(false, false, 53));
	EXPECT_TRUE(sh::ValidToTap(false, true, 53));
	EXPECT_FALSE(sh::ValidToTap(false, true, 0));
}

TEST(ScriptHighlight, TapToolTip)
{
	// GetOverwriteTapToolTip 0x70AE10
	EXPECT_EQ(sh::OverwriteTapToolTip(ObjectType::ScriptHighlight), 0u);
	EXPECT_EQ(sh::OverwriteTapToolTip(ObjectType::Creature), 0xEF2u);
}

TEST(ScriptHighlight, PulseOfProcessHighlights)
{
	// 0x70A463..0x70A4D6 with the 100 ms turn: 0.5 rad a turn
	sh::Pulse pulse;
	sh::StepPulse(pulse, 100);
	EXPECT_NEAR(pulse.phase, 0.5f, 1e-6f);
	EXPECT_FLOAT_EQ(pulse.previous, 0.0f);
	EXPECT_NEAR(pulse.value, (1.0f - std::cos(0.5f)) * 0.5f, 1e-6f);
	const float first = pulse.value;
	sh::StepPulse(pulse, 100);
	EXPECT_FLOAT_EQ(pulse.previous, first);
	// the phase loses 2 pi once it is above
	sh::Pulse wrap {6.2f, 0.0f, 0.0f};
	sh::StepPulse(wrap, 100);
	EXPECT_NEAR(wrap.phase, 6.7f - 6.28319f, 1e-5f);
}

TEST(ScriptHighlight, ActivePulseAndGlow)
{
	// fn_0070A510: lerp, bound to [0, 1], x 0.6 + 0.4
	const sh::Pulse pulse {0.0f, 1.0f, 0.0f};
	EXPECT_NEAR(sh::ActivePulse(pulse, 0.5f), 0.7f, 1e-6f);
	EXPECT_NEAR(sh::ActivePulse(pulse, 0.0f), 0.4f, 1e-6f);
	const sh::Pulse over {0.0f, 3.0f, 0.0f};
	EXPECT_NEAR(sh::ActivePulse(over, 0.9f), 1.0f, 1e-6f);
	// Draw 0x70A0BD..0x70A0E6: silver's fixed colour, the others 0x50 alpha only at a pulse of 1
	EXPECT_EQ(sh::ActiveGlowArgb(2, 0.7f), 0x14B4DCFFu);
	EXPECT_EQ(sh::ActiveGlowArgb(3, 0.7f), 0x00FFFF00u);
	EXPECT_EQ(sh::ActiveGlowArgb(3, 1.0f), 0x50FFFF00u);
	EXPECT_EQ(sh::ActiveGlowArgb(0, 1.0f), 0x50FFFF00u);
}

TEST(ScriptHighlight, DidYouKnowReadLists)
{
	sh::dyk_read::Clear();
	EXPECT_FALSE(sh::dyk_read::IsRead(4143, DykCategory::Misc));
	sh::dyk_read::MarkRead(4143, DykCategory::Misc);
	EXPECT_TRUE(sh::dyk_read::IsRead(4143, DykCategory::Misc));
	EXPECT_FALSE(sh::dyk_read::IsRead(4143, DykCategory::Creature)); // one list a category
	EXPECT_EQ(sh::dyk_read::Total(), 1u);
	// at most 48 a category
	for (uint32_t text = 0; text < 60; ++text)
	{
		sh::dyk_read::MarkRead(10000 + text, DykCategory::Navigation);
	}
	EXPECT_EQ(sh::dyk_read::Total(), 1u + 48u);
	EXPECT_FALSE(sh::dyk_read::IsRead(10055, DykCategory::Navigation));
	// past DYK_CATEGORY 4: never read (fn_0078CC10 `cmp eax, 4; ja`)
	EXPECT_FALSE(sh::dyk_read::IsRead(4143, static_cast<DykCategory>(7)));
	sh::dyk_read::Clear();
	EXPECT_EQ(sh::dyk_read::Total(), 0u);
}
