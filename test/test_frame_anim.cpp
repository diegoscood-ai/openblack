/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The frame clocks of src/3D/FrameAnim.h against the original's formulas: every one picks a whole frame (no blend),
// with its own constants and edges (the cell-32 flame, the mist's skipped cells 14-15, the fish's cell before its wrap,
// the icon's negative fmod, the hand flow's rounding), and the loaders for mods.

#include <cmath>

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "3D/Billboard.h"
#include "3D/FrameAnim.h"

using namespace openblack::graphics;

TEST(FrameAnim, spriteCellAndOffset)
{
	// LH3DSprite +0x28 & 0x3F, the UVs of billboard::CellUv
	EXPECT_EQ(frame_anim::SpriteCell(64 + 9), 9);
	EXPECT_EQ(frame_anim::SpriteCell(-1), 63);
	const auto uv = frame_anim::SpriteCellUv(9, 8);
	EXPECT_FLOAT_EQ(uv[0].x, 0.125f);
	EXPECT_FLOAT_EQ(uv[0].y, 0.125f);
	EXPECT_FLOAT_EQ(uv[2].x, 0.25f);
	// SetAnimatedUV_1 / DrawTriangle: (0, 0) is off; a fixed material (bit 0x10) keeps its UVs
	EXPECT_FALSE(frame_anim::IsAnimatedUv({0.0f, 0.0f}));
	EXPECT_TRUE(frame_anim::IsAnimatedUv({0.0f, -0.5f}));
	EXPECT_FLOAT_EQ(frame_anim::OffsetUv({0.1f, 0.2f}, {0.25f, 0.5f}, false).y, 0.7f);
	EXPECT_FLOAT_EQ(frame_anim::OffsetUv({0.1f, 0.2f}, {0.25f, 0.5f}, true).y, 0.2f);
	// PackUvOffset is exact for the icons' 32/256 steps: v + 4 x 32 col
	for (int col = 0; col < 8; ++col)
	{
		const float u = static_cast<float>(col) * 0.00390625f * 32.0f;
		const float packed = frame_anim::PackUvOffset(u, 0.375f);
		const float steps = std::floor((packed + 2.0f) / 4.0f);
		EXPECT_FLOAT_EQ(steps / 256.0f, u);
		EXPECT_FLOAT_EQ(packed - steps * 4.0f, 0.375f);
	}
}

TEST(FrameAnim, animTexturedCell)
{
	// 0x67A530: cells of 64 x 32 in rows of 256 / 64 = 4
	frame_anim::AnimTexturedSheet sheet {64, 32, false, false, 16};
	const auto uv = frame_anim::AnimTexturedCell(6, sheet);
	EXPECT_FLOAT_EQ(uv.x, 0.5f);   // 64 / 256 x (6 % 4)
	EXPECT_FLOAT_EQ(uv.y, 0.125f); // 32 / 256 x (6 / 4)
	// the slide: H f / (N 256)
	sheet.slideV = true;
	sheet.frames = 1000;
	EXPECT_FLOAT_EQ(frame_anim::AnimTexturedCell(500, sheet).y, 32.0f * 500.0f / 256000.0f);
	EXPECT_FLOAT_EQ(frame_anim::AnimTexturedCell(500, sheet).x, 0.0f);
}

TEST(FrameAnim, oneOffAndSpellIcon)
{
	// 0x72A570: 18 frames a second over 16, 4 x 4
	float phase = 0.0f;
	auto uv = frame_anim::OneOffFrame(phase, 500.0f); // 9 frames
	EXPECT_FLOAT_EQ(phase, 9.0f);
	EXPECT_FLOAT_EQ(uv.x, 0.25f);
	EXPECT_FLOAT_EQ(uv.y, 0.5f);
	uv = frame_anim::OneOffFrame(phase, 500.0f); // 18 -> 2
	EXPECT_FLOAT_EQ(phase, 2.0f);
	EXPECT_FLOAT_EQ(uv.x, 0.5f);
	EXPECT_FLOAT_EQ(uv.y, 0.0f);
	// 0x519AD0: -15 a second, fmod 32 and + 32 when negative; 8 x 4 in 32/256 steps
	float icon = 0.0f;
	uv = frame_anim::SpellIconFrame(icon, 0.01f); // -0.15 -> 31.85: frame 31
	EXPECT_NEAR(icon, 31.85f, 1e-5f);
	EXPECT_FLOAT_EQ(uv.x, 0.875f);
	EXPECT_FLOAT_EQ(uv.y, 0.375f);
	uv = frame_anim::SpellIconFrame(icon, 1.0f); // 16.85: frame 16
	EXPECT_FLOAT_EQ(uv.x, 0.0f);
	EXPECT_FLOAT_EQ(uv.y, 0.25f);
}

TEST(FrameAnim, handFlowRounds)
{
	// 0x68D0C0: -20 a second, into [0, 64) from below; fistp rounds to the nearest
	float phase = 0.0f;
	auto uv = frame_anim::HandFlowFrame(phase, 0.03f); // -0.6 -> 63.4 -> 63 % 32 = 31
	EXPECT_NEAR(phase, 63.4f, 1e-5f);
	EXPECT_FLOAT_EQ(uv.x, 0.875f);
	EXPECT_FLOAT_EQ(uv.y, 0.375f);
	phase = 10.6f;
	uv = frame_anim::HandFlowFrame(phase, 0.0f); // rounded to 11, not truncated to 10
	EXPECT_FLOAT_EQ(uv.x, 0.375f);
	EXPECT_FLOAT_EQ(uv.y, 0.125f);
}

TEST(FrameAnim, psysFrames)
{
	// fn_00679920: looped, fmod then + N when negative; t' up to 5
	EXPECT_EQ(frame_anim::PSysFrameIndex(17.5f, 16, true), 1);
	EXPECT_EQ(frame_anim::PSysFrameIndex(-0.5f, 16, true), 15);
	// not looped: clamped to 0..N - 1
	EXPECT_EQ(frame_anim::PSysFrameIndex(-3.0f, 16, false), 0);
	EXPECT_EQ(frame_anim::PSysFrameIndex(40.0f, 16, false), 15);
	EXPECT_FLOAT_EQ(frame_anim::PSysFrameLerp(2.0f, 4.0f, 0.5f, true), 3.0f);
	EXPECT_FLOAT_EQ(frame_anim::PSysFrameLerp(2.0f, 4.0f, 3.0f, true), 8.0f);
	EXPECT_FLOAT_EQ(frame_anim::PSysFrameLerp(2.0f, 4.0f, 3.0f, false), 4.0f);
	EXPECT_FLOAT_EQ(frame_anim::PSysFrameLerp(2.0f, 4.0f, 9.0f, true), 12.0f);
	// fn_00673EA0: both kept in [0, 2N): above 2 N down by N, below 0 up by 2 N
	float previous = 0.0f;
	float current = 31.5f;
	frame_anim::PSysFrameAdvance(previous, current, 0.1f, 10.0f, 16, true); // 32.5, previous 31.5: not both above 32
	EXPECT_FLOAT_EQ(current, 32.5f);
	frame_anim::PSysFrameAdvance(previous, current, 0.1f, 10.0f, 16, true); // 33.5 and 32.5 -> 17.5 and 16.5
	EXPECT_FLOAT_EQ(current, 17.5f);
	EXPECT_FLOAT_EQ(previous, 16.5f);
	current = 0.5f;
	frame_anim::PSysFrameAdvance(previous, current, 0.1f, -10.0f, 16, true); // -0.5 -> 31.5, previous 32.5
	EXPECT_FLOAT_EQ(current, 31.5f);
	EXPECT_FLOAT_EQ(previous, 32.5f);
	// 0x673FC6..0x673FDE: without PlayAnim no step and no wrap, a negative frame stays negative
	current = -3.0f;
	frame_anim::PSysFrameAdvance(previous, current, 0.1f, 10.0f, 16, false);
	EXPECT_FLOAT_EQ(current, -3.0f);
	EXPECT_FLOAT_EQ(previous, -3.0f);
	EXPECT_EQ(frame_anim::PSysFrameIndex(current, 16, false), 0);
	// PlayAnim with rate 0 still wraps: -3 + 32
	frame_anim::PSysFrameAdvance(previous, current, 0.1f, 0.0f, 16, true);
	EXPECT_FLOAT_EQ(current, 29.0f);
}

TEST(FrameAnim, mistSkipsCells14And15)
{
	// fn_007FA300: (c x 45 / 900) & 15, c up to 900 (kept), so cells 14 and 15 are not reached in the third turn
	EXPECT_EQ(frame_anim::MistCell(0), 0);
	EXPECT_EQ(frame_anim::MistCell(319), 15);
	EXPECT_EQ(frame_anim::MistCell(320), 0);
	EXPECT_EQ(frame_anim::MistCell(899), 12);
	EXPECT_EQ(frame_anim::MistCell(900), 13);
	bool seen14 = false;
	for (int c = 640; c <= 900; ++c)
	{
		seen14 = seen14 || frame_anim::MistCell(c) >= 14;
	}
	EXPECT_FALSE(seen14);
	// the wrap only past 900
	frame_anim::MistClock clock {896, 0.0f};
	frame_anim::MistAdvanceExact(clock, 16); // + ftol(4.08) = 900: kept
	EXPECT_EQ(clock.counter, 900);
	frame_anim::MistAdvanceExact(clock, 16); // 904 -> 4
	EXPECT_EQ(clock.counter, 4);
	// openblack keeps the fraction: 4 frames of 1 ms make one count
	frame_anim::MistClock fine {0, 0.0f};
	for (int i = 0; i < 4; ++i)
	{
		frame_anim::MistAdvance(fine, 1.0f);
	}
	EXPECT_EQ(fine.counter, 1);
	// rows 2-3 in the effect branch
	EXPECT_FLOAT_EQ(frame_anim::MistCellUv(9, true).x, 0.125f);
	EXPECT_FLOAT_EQ(frame_anim::MistCellUv(9, true).y, 0.375f);
	EXPECT_FLOAT_EQ(frame_anim::MistCellUv(9, false).y, 0.125f);
	EXPECT_EQ(frame_anim::MistStartCounter(15.9f), 15);
	// the smoke's age step: dt x 255 with the fraction kept, dt at most 100 s
	float remainder = 0.0f;
	EXPECT_EQ(frame_anim::SmokeAgeStep(remainder, 16.0f), 4);
	EXPECT_NEAR(remainder, 0.08f, 1e-4f);
}

TEST(FrameAnim, fireCell32)
{
	// fn_007321B0: ftol(fmod(-25 age, 32) + 32); at age 0 the cell is 32
	EXPECT_EQ(frame_anim::FireCell(0.0f), 32);
	EXPECT_EQ(frame_anim::FireCell(0.04f), 31);
	// 0x7321E0: ftol(fmod(25 age, 32))
	EXPECT_EQ(frame_anim::SteamCell(0.0f), 0);
	EXPECT_EQ(frame_anim::SteamCell(1.3f), 0); // 32.5 -> 0.5
	EXPECT_EQ(frame_anim::SteamCell(0.5f), 12);
}

TEST(FrameAnim, fishCellBeforeWrap)
{
	// fn_008248E0: dt at most 0.1; the cell is taken before the frame wraps
	float frame = 14.5f;
	const auto cell = frame_anim::FishFrame(frame, 0.04f, 1.0f); // 15.5: cell 8 + 15, then 0.5
	EXPECT_EQ(cell, 23);
	EXPECT_NEAR(frame, 0.5f, 1e-5f);
	frame = 0.0f;
	(void)frame_anim::FishFrame(frame, 1.0f, 1.0f); // dt 0.1: 2.5 frames
	EXPECT_FLOAT_EQ(frame, 2.5f);
	EXPECT_FLOAT_EQ(frame_anim::FishDt(0.5f), 0.1f);
}

TEST(FrameAnim, lanterns)
{
	// fn_00823570: one clock, 31 steps in 700 ms; the flames start at the global table 0xC383BC ({0, 13} in the file)
	int clock = 0;
	EXPECT_EQ(frame_anim::LanternAdvance(clock, 350), 15);
	EXPECT_EQ(frame_anim::LanternAdvance(clock, 350), 31); // 700 is kept
	EXPECT_EQ(clock, 700);
	EXPECT_EQ(frame_anim::LanternAdvance(clock, 10), 0); // 710 -> 10
	const auto& starts = frame_anim::k_LanternFileStarts;
	EXPECT_EQ(frame_anim::LanternCell(0, 0, starts), 31);
	EXPECT_EQ(frame_anim::LanternCell(0, 1, starts), (10 + 31 - 13) & 31);
	EXPECT_EQ(frame_anim::LanternCell(5, 0, starts), 26);
	// fn_00823240 rewrites the table with ftol(Random(0, 31)) for every new light
	const frame_anim::LanternStarts written = {frame_anim::LanternStart(30.9f), frame_anim::LanternStart(7.2f), 0};
	EXPECT_EQ(written[0], 30);
	EXPECT_EQ(frame_anim::LanternCell(0, 1, written), (10 + 31 - 7) & 31);
}

TEST(FrameAnim, otherCellClocks)
{
	// fn_00466730: 10 a second over 15, after the wrap
	float leash = 14.0f;
	EXPECT_EQ(frame_anim::LeashCell(leash, 0.15f), 0); // 15.5 -> 0.5
	float u = 0.9f;
	EXPECT_NEAR(frame_anim::LeashScroll(u, 0.4f), 0.1f, 1e-5f);
	// fn_006CA990: (t / 50 + base + drop) % 32
	EXPECT_EQ(frame_anim::GoldenShowerCell(1000, 3, 10), 1);
	// CreatureRoom::DrawAdditional: 31 - (((tick >> 5) + i) & 31)
	EXPECT_EQ(frame_anim::CreatureRoomCell(32u * 5u, 1), 25);
	// CameraModeNew3: (tick / 50) & 15
	EXPECT_EQ(frame_anim::CursorCell(850), 1);
	// fn_005C0700: (clock / 200) & 15
	int32_t help = 0;
	EXPECT_EQ(frame_anim::HelpSystemCell(help, 3300), 0);
	EXPECT_EQ(frame_anim::HelpSystemCell(help, 100), 1);
	// fn_00828A70: + ms x 0.01, restarted at 0 past 15
	float jc = 14.0f;
	EXPECT_EQ(frame_anim::JCSpecialCell(jc, 100.0f), 15);
	EXPECT_EQ(frame_anim::JCSpecialCell(jc, 10.0f), 0);
	EXPECT_FLOAT_EQ(jc, 0.0f);
	// PlayerSymbolSprite::Draw: -= ms x 0.02 (layer 0) or 0.023 (layer 1), + 32 while negative
	float symbol = 0.0f;
	EXPECT_EQ(frame_anim::PlayerSymbolCell(symbol, 50.0f, 0), 31);
	float symbolB = 0.0f;
	EXPECT_EQ(frame_anim::PlayerSymbolCell(symbolB, 100.0f, 1), 29); // 32 - 2.3
	float spin = 6.0f;
	EXPECT_NEAR(frame_anim::PlayerSymbolSpin(spin, 200.0f), 6.4f - 6.28318548f, 1e-5f);
	EXPECT_EQ(frame_anim::SmokyStuffCell(0.99f), 14);
	EXPECT_EQ(frame_anim::DustCell(3, 7.0f), 16 + 1);
}

TEST(FrameAnim, scrolls)
{
	// 0x5E3972: V -= 0.5 dt, minus its whole part: in -1..0
	float v = 0.0f;
	EXPECT_FLOAT_EQ(frame_anim::WaterfallScroll(v, 1.0f), -0.5f);
	EXPECT_FLOAT_EQ(frame_anim::WaterfallScroll(v, 1.0f), 0.0f);
	// fn_005E6390 at t = 500: x = 1
	const auto ghost = frame_anim::GoolooFrame(500.0f);
	EXPECT_NEAR(ghost.uv.x, 2.0f * std::cos(1.0f), 1e-5f);
	EXPECT_NEAR(ghost.uv.y, 1.7f * std::sin(0.7f), 1e-5f);
	EXPECT_EQ(ghost.materialByte, 0);
	EXPECT_EQ(frame_anim::GoolooFrame(0.0f).materialByte, 255);
	// 0x67CBA0: lerp, then the period taken off while above it (nothing added below 0)
	const auto rotating = frame_anim::RotatingUv({0.5f, -0.25f}, {2.5f, -0.75f}, 0.5f, {1.0f, 1.0f});
	EXPECT_FLOAT_EQ(rotating.x, 0.5f);
	EXPECT_FLOAT_EQ(rotating.y, -0.5f);
	// fn_0067B3F0: + ms x rate x 0.001, kept in FrameHeight / 256
	float scroll = 0.0f;
	EXPECT_NEAR(frame_anim::ChainScroll(scroll, 1000.0f, 0.3f, 64), 0.05f, 1e-6f);
	EXPECT_NEAR(frame_anim::ChainScroll(scroll, 1000.0f, -0.1f, 64), 0.2f, 1e-6f);
	float none = 0.0f;
	EXPECT_FLOAT_EQ(frame_anim::ChainScroll(none, 1000.0f, 0.0f, 64), 0.0f);
	// fn_006C8920: segment 1 of 4 over 2 textures: the second half of the first texture
	frame_anim::ChainSheet sheet;
	sheet.textures = 2;
	sheet.frameOfTail = 1;
	const auto uv = frame_anim::ChainSegmentUv(1, 4, sheet, 0.0f);
	EXPECT_FLOAT_EQ(uv[0].x, 0.125f); // frame 1 x 32 / 256
	EXPECT_FLOAT_EQ(uv[1].x, 0.25f);
	EXPECT_FLOAT_EQ(uv[0].y, 0.125f); // 64 x 1/2 / 256
	EXPECT_FLOAT_EQ(uv[3].y, 0.25f);
}

TEST(FrameAnim, bandToEye)
{
	// fn_0051A830: columns -D, U, U x D, with D from the eye to the band
	const auto axes = billboard::BandToEye({0.0f, 0.0f, 10.0f}, {0.0f, 0.0f, 0.0f});
	EXPECT_NEAR(axes[0].z, -1.0f, 1e-6f);
	EXPECT_NEAR(axes[1].y, 1.0f, 1e-6f);
	EXPECT_NEAR(axes[2].x, 1.0f, 1e-6f); // U x D = (0, 1, 0) x (0, 0, 1)
	// straight above: pushed off the vertical, still a rotation
	const auto above = billboard::BandToEye({0.0f, 0.0f, 0.0f}, {0.0f, 10.0f, 0.0f});
	EXPECT_NEAR(glm::length(above[1]), 1.0f, 1e-4f);
	EXPECT_NEAR(glm::dot(above[0], above[1]), 0.0f, 1e-4f);
}

TEST(FrameAnim, stackedFrames)
{
	// GetBitmap 0x6A9D40: RGB when 3 bytes a pixel fit, else grey; nothing when short
	const std::vector<uint8_t> rgb(2 * 2 * 3 * 3, 100);
	const auto stacked = frame_anim::LoadStackedFrames(rgb, 2, 3);
	ASSERT_TRUE(stacked.has_value());
	EXPECT_EQ(stacked->channels, 3);
	EXPECT_FLOAT_EQ(frame_anim::SampleStackedFrame(*stacked, 2, 0.5f, 0.5f).g, 100.0f);
	const std::vector<uint8_t> grey(2 * 2 * 3, 7);
	EXPECT_EQ(frame_anim::LoadStackedFrames(grey, 2, 3)->channels, 1);
	EXPECT_FALSE(frame_anim::LoadStackedFrames(std::vector<uint8_t>(5, 0), 2, 3).has_value());
}

TEST(FrameAnim, gifAndDelayClock)
{
	// a 1 x 1 GIF of two frames, delays 10 cs and 1 cs (100 ms and 10 ms, shown as 100 ms)
	const std::vector<uint8_t> gif = {
	    'G', 'I', 'F', '8', '9', 'a', 0x01, 0x00, 0x01, 0x00, 0x80, 0x00, 0x00, // header, 2-colour table
	    0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF,                                     // black, white
	    0x21, 0xF9, 0x04, 0x00, 0x0A, 0x00, 0x00, 0x00,                         // delay 10
	    0x2C, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x02, 0x02, 0x44, 0x01, 0x00,
	    0x21, 0xF9, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, // delay 1
	    0x2C, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x02, 0x02, 0x44, 0x01, 0x00, 0x3B};
	const auto frames = frame_anim::LoadGif(gif);
	ASSERT_TRUE(frames.has_value());
	EXPECT_EQ(frames->frames, 2);
	EXPECT_EQ(frames->width, 1);
	ASSERT_EQ(frames->delaysMs.size(), 2u);
	EXPECT_EQ(frames->delaysMs[0], 100);
	EXPECT_EQ(frame_anim::GifDelayMs(frames->delaysMs[1]), 100);
	EXPECT_FALSE(frame_anim::LoadGif(std::vector<uint8_t>(8, 0)).has_value());

	const auto clock = frame_anim::DelayClock::FromDelays(frames->delaysMs);
	EXPECT_FLOAT_EQ(clock.Length(), 0.2f);
	EXPECT_EQ(clock.At(0.05f).frame, 0u);
	EXPECT_EQ(clock.At(0.15f).frame, 1u);
	EXPECT_EQ(clock.At(0.25f).frame, 0u); // looped
	EXPECT_EQ(clock.At(0.15f).next, 0u);
	EXPECT_NEAR(clock.At(0.15f).fraction, 0.5f, 1e-4f);

	// no blend unless asked for
	frame_anim::AnimatedSprite sprite;
	sprite.first = 4;
	sprite.clock = frame_anim::DelayClock::FixedRate(4, 10.0f);
	auto frame = sprite.At(0.25f);
	EXPECT_EQ(frame.cell, 6);
	EXPECT_EQ(frame.nextCell, 7);
	EXPECT_FLOAT_EQ(frame.weight, 0.0f);
	sprite.blend = true;
	frame = sprite.At(0.25f);
	EXPECT_NEAR(frame.weight, 0.5f, 1e-4f);
}
