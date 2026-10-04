/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// help::spirits (src/Help/Spirits.h): HelpDudeControl 0x5C2A40..0x5C3D82, the HelpSpirit calls 0x5C4BD0..0x5C52E4 and
// HelpDude's hover motion (Sethoverx 0x5B96D0, SetState 0x5BD4A0, Update1 0x5BDDA0) as pure logic, with the random
// streams and the camera given by the test. No .hd is needed: the dudes have no clips, so only the motion runs.

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "Help/HelpSystem.h"
#include "Help/Spirits.h"

using namespace openblack;
using namespace openblack::help::spirits;

namespace
{
constexpr Screen k_Screen {640, 480};

struct Fixture
{
	bool randomMax {false};
	std::vector<int32_t> randCalls;
	std::vector<glm::vec2> randomCalls; ///< the (a, b) of every crt Random
	DudeData good;
	DudeData evil;
	HelpDudeControl control;

	Fixture()
	    : control(good, evil, MakeQueries(), k_Screen)
	{
	}

	Queries MakeQueries()
	{
		Queries q;
		q.localRand = [this](int32_t n) {
			randCalls.push_back(n);
			return randomMax && n > 0 ? static_cast<uint32_t>(n - 1) : 0u;
		};
		q.localFloatRand = [](float) { return 0.0f; };
		q.random = [this](float a, float b) {
			randomCalls.emplace_back(a, b);
			return a;
		};
		return q;
	}

	void Frames(int count, float dt = 0.1f)
	{
		FrameInput input;
		input.dt = dt;
		input.frameMs = static_cast<int32_t>(dt * 1000.0f + 0.5f);
		input.screen = k_Screen;
		input.mouse = {320, 240};
		for (int i = 0; i < count; ++i)
		{
			input.tickMs += static_cast<uint32_t>(input.frameMs);
			control.Update(input);
		}
	}
};

// hover space: hx = (px - W/2) / (W/2), hy = (py - H/2) / (W/2)
float Hx(int px)
{
	return static_cast<float>(px - 320) / 320.0f;
}
float Hy(int py)
{
	return static_cast<float>(py - 240) / 320.0f;
}
} // namespace

TEST(Spirits, ConvertScriptSpiritToHelpSpiritThenDude)
{
	// GScript 0x710350 (HelpSystem.h, reused), then fn_005C5250: type 1 -> dude 0, any other -> 1
	auto rand = [] { return 51; };
	EXPECT_EQ(HelpDudeControl::DudeOf(help::ConvertScriptSpiritToHelpSpirit(1, 0, rand)), k_GoodDude);
	EXPECT_EQ(HelpDudeControl::DudeOf(help::ConvertScriptSpiritToHelpSpirit(2, 0, rand)), k_EvilDude);
	EXPECT_EQ(HelpDudeControl::DudeOf(help::ConvertScriptSpiritToHelpSpirit(3, 2, rand)), k_EvilDude);
	EXPECT_EQ(HelpDudeControl::DudeOf(help::ConvertScriptSpiritToHelpSpirit(4, 2, rand)), k_GoodDude);
	EXPECT_EQ(HelpDudeControl::DudeOf(help::ConvertScriptSpiritToHelpSpirit(5, 0, rand)), k_EvilDude);
	EXPECT_EQ(HelpDudeControl::DudeOf(help::ConvertScriptSpiritToHelpSpirit(0, 0, rand)), k_GoodDude);
}

TEST(Spirits, StartAtHome)
{
	Fixture f;
	for (int d = 0; d < k_Dudes; ++d)
	{
		EXPECT_EQ(f.control.State(d), ControlState::Home);
		EXPECT_EQ(f.control.Dude(d).State(), dude_state::Hover);
	}
	// the anchors of fn_005BD440
	EXPECT_EQ(f.control.Anchor(0), glm::ivec2(320, 720));
	EXPECT_EQ(f.control.Anchor(1), glm::ivec2(-320, 240));
	EXPECT_EQ(f.control.Anchor(2), glm::ivec2(320, -240));
	EXPECT_EQ(f.control.Anchor(3), glm::ivec2(960, 240));
}

TEST(Spirits, EjectFromHomeFliesIntoTheMiddle)
{
	for (const bool high : {false, true})
	{
		Fixture f;
		f.randomMax = high;
		f.control.SpiritEject(1, false); // CHL 7 from a challenge script: Eject 0x5C32C0
		EXPECT_EQ(f.control.State(0), ControlState::Out);
		EXPECT_EQ(f.control.Timer(0), 0.0f);
		// LocalRand(W/2) + W/4 then LocalRand(H/2) + H/4, in that order
		ASSERT_GE(f.randCalls.size(), 2u);
		EXPECT_EQ(f.randCalls[0], 320);
		EXPECT_EQ(f.randCalls[1], 240);
		const HelpDude& d = f.control.Dude(0);
		const float x = d.HoverX().destination;
		const float y = d.HoverY().destination;
		EXPECT_GE(x, Hx(160));
		EXPECT_LE(x, Hx(160 + 319));
		EXPECT_GE(y, Hy(120));
		EXPECT_LE(y, Hy(120 + 239));
		EXPECT_FLOAT_EQ(x, high ? Hx(479) : Hx(160));
		EXPECT_FLOAT_EQ(y, high ? Hy(359) : Hy(120));
		EXPECT_FLOAT_EQ(d.HoverX().duration, 1.0f);
		EXPECT_EQ(d.AlphaTarget(), 1.0f);
		EXPECT_EQ(d.EmotionTarget(), 0u);
		EXPECT_EQ(d.EmotionPeak(), 1.0f);
	}
}

TEST(Spirits, EjectFromClingStaysInTheMiddleThird)
{
	Fixture f;
	f.control.SpiritCling(1, 0.0f, 0.0f);
	ASSERT_EQ(f.control.State(0), ControlState::Clinging);
	f.randCalls.clear();
	f.randomMax = true;
	f.control.SpiritEject(1, false);
	EXPECT_EQ(f.control.State(0), ControlState::Clinging); // Eject does not change a clinging state
	ASSERT_GE(f.randCalls.size(), 2u);
	EXPECT_EQ(f.randCalls[0], 213); // W/3
	EXPECT_EQ(f.randCalls[1], 160); // H/3
}

TEST(Spirits, AppearSpots)
{
	Fixture f;
	f.control.SpiritEject(1, true); // CHL 300 / CHL 7 from a help script: Appear 0x5C3400
	f.control.SpiritEject(2, true);
	const HelpDude& good = f.control.Dude(0);
	const HelpDude& evil = f.control.Dude(1);
	EXPECT_EQ(f.control.State(0), ControlState::Out);
	EXPECT_EQ(f.control.State(1), ControlState::Out);
	EXPECT_FLOAT_EQ(good.Hover().x, Hx(160)); // (W/4, H/2)
	EXPECT_FLOAT_EQ(good.Hover().y, 0.0f);
	EXPECT_FLOAT_EQ(evil.Hover().x, Hx(480)); // (3W/4, H/2)
	EXPECT_FLOAT_EQ(evil.Hover().y, 0.0f);
	EXPECT_EQ(good.Alpha(), 0.0f);
	EXPECT_EQ(good.AlphaTarget(), 1.0f);
	EXPECT_TRUE(good.PuffRunning());
	// fades in at 3/s (fn_005C0700 0x5C08C8)
	f.Frames(1);
	EXPECT_NEAR(f.control.Dude(0).Alpha(), 0.3f, 1e-5f);
}

TEST(Spirits, PuffParticlesDrawRandomInTheExeOrder)
{
	// grey, vx, vy, vz, size, spin (fn_005C1D20 0x5C1D3E..0x5C1DCD, the creation 0x5C0B62..0x5C0BEC)
	const std::array<glm::vec2, 6> order = {glm::vec2(116.0f, 250.0f), glm::vec2(-0.8f, 0.8f), glm::vec2(-1.9f, 1.5f),
	                                        glm::vec2(-1.0f, 1.0f),     glm::vec2(4.0f, 8.0f),   glm::vec2(-2.0f, 2.0f)};
	Fixture f;
	HelpDude& d = f.control.Dude(0);
	f.control.SpiritEject(1, true); // Appear: StartPuff before the particles exist draws nothing (0x5C1D2E)
	EXPECT_TRUE(d.PuffRunning());
	EXPECT_FALSE(d.PuffParticles().has_value());
	f.randomCalls.clear();
	f.Frames(1); // the first draw of the puff makes them, after Process (fn_005C0700 0x5C0B24..0x5C0C06)
	ASSERT_TRUE(d.PuffParticles().has_value());
	ASSERT_GE(f.randomCalls.size(), 6 * k_PuffParticles);
	const size_t first = f.randomCalls.size() - 6 * k_PuffParticles;
	for (size_t i = 0; i < 6 * k_PuffParticles; ++i)
	{
		EXPECT_EQ(f.randomCalls[first + i], order.at(i % 6)) << i;
	}
	const PuffParticle& p = d.PuffParticles()->at(0);
	EXPECT_EQ(p.grey, 0x747474u); // ftol(116), three bytes
	// drawVelocityY is the vy before this frame's drift, which has already moved velocity.y (UpdateDraw)
	EXPECT_FLOAT_EQ(p.drawVelocityY, -1.9f);
	EXPECT_EQ(p.k, (1.0f - -1.9f) + 1.0f);
	EXPECT_GT(p.age, 0.0f); // stepped by this frame's draw
	// a second puff draws them again at once, in fn_005C1D20 itself
	f.randomCalls.clear();
	d.StartPuff();
	ASSERT_EQ(f.randomCalls.size(), 6 * k_PuffParticles);
	for (size_t i = 0; i < f.randomCalls.size(); ++i)
	{
		EXPECT_EQ(f.randomCalls[i], order.at(i % 6)) << i;
	}
	EXPECT_EQ(d.PuffParticles()->at(0).age, 0.0f);
}

TEST(Spirits, HomeTakesOneSecond)
{
	Fixture f;
	f.control.SpiritEject(1, false);
	f.control.SpiritHome(1, false); // CHL 8 from a challenge script: Home 0x5C3590
	EXPECT_EQ(f.control.State(0), ControlState::GoingHome);
	f.Frames(9);
	EXPECT_EQ(f.control.State(0), ControlState::GoingHome);
	f.Frames(2);
	EXPECT_EQ(f.control.State(0), ControlState::Home);
	EXPECT_EQ(f.control.Dude(0).State(), dude_state::Hover);
}

TEST(Spirits, VanishWaitsForThePuff)
{
	Fixture f;
	f.control.SpiritEject(2, false);
	f.control.SpiritHome(2, true); // CHL 301: Vanish 0x5C3540
	const HelpDude& d = f.control.Dude(1);
	EXPECT_EQ(f.control.State(1), ControlState::GoingHome);
	EXPECT_EQ(d.AlphaTarget(), 0.0f);
	EXPECT_TRUE(d.PuffRunning());
	f.Frames(5);
	EXPECT_NEAR(d.Alpha(), 0.0f, 1e-6f); // fades out at 2/s
	f.Frames(6); // 1.1 s: the timer is past 1 s but the puff still runs (0x5C3B25)
	EXPECT_EQ(f.control.State(1), ControlState::GoingHome);
	f.Frames(20); // the puff ends after (255 / 32) / 0.0032 ms of time units
	EXPECT_FALSE(d.PuffRunning());
	EXPECT_EQ(f.control.State(1), ControlState::Home);
}

TEST(Spirits, ClingEdges)
{
	Fixture f;
	f.control.SpiritCling(1, 0.0f, 0.0f); // (0, 0): the default edge, good on the right
	f.control.SpiritCling(2, 0.0f, 0.0f); // evil on the left
	const HelpDude& good = f.control.Dude(0);
	const HelpDude& evil = f.control.Dude(1);
	EXPECT_EQ(f.control.State(0), ControlState::Clinging);
	EXPECT_EQ(good.ClingEdge(), Edge::Right);
	EXPECT_FLOAT_EQ(good.ClingTarget().x, 1.04f);
	EXPECT_EQ(evil.ClingEdge(), Edge::Left);
	EXPECT_FLOAT_EQ(evil.ClingTarget().x, -1.04f);
	// from home: snapped to the edge's off-screen anchor first, then arriving (0x110)
	EXPECT_FLOAT_EQ(good.Hover().x, Hx(960));
	EXPECT_FLOAT_EQ(evil.Hover().x, Hx(-320));
	EXPECT_EQ(good.State(), dude_state::ClingArrive);
	EXPECT_FLOAT_EQ(good.HoverX().destination, 1.04f);

	// |x| > 1.28205 |y| -> left / right, else bottom / top
	f.control.SpiritCling(1, 0.5f, 1.0f); // hx 0, hy 0.75
	EXPECT_EQ(good.ClingEdge(), Edge::Bottom);
	EXPECT_FLOAT_EQ(good.ClingTarget().y, 0.78f);
	f.control.SpiritCling(1, 0.2f, 1.0f); // hx -0.6, hy 0.75
	EXPECT_EQ(good.ClingEdge(), Edge::Bottom);
	f.control.SpiritCling(1, 0.0f, 0.5f); // hx -1, hy 0
	EXPECT_EQ(good.ClingEdge(), Edge::Left);
	f.control.SpiritCling(1, 0.5f, 0.0f); // hx 0, hy -0.75
	EXPECT_EQ(good.ClingEdge(), Edge::Top);
	EXPECT_FLOAT_EQ(good.ClingTarget().y, -0.78f);
}

TEST(Spirits, FlyReachesItsTargetInOneSecond)
{
	Fixture f;
	f.control.SpiritEject(1, false);
	f.control.SpiritEject(2, false);
	// Land 1 FollowUs (rt_chl_code.txt:51735 / 51739)
	f.control.SpiritFly(1, 0.8f, 0.75f);
	f.control.SpiritFly(2, 0.2f, 0.75f);
	const glm::vec2 goodStart = f.control.Dude(0).Hover();
	f.Frames(5);
	const glm::vec2 goodMid = f.control.Dude(0).Hover();
	EXPECT_GT(goodMid.x, std::min(goodStart.x, Hx(512)));
	EXPECT_LT(goodMid.x, std::max(goodStart.x, Hx(512)));
	f.Frames(5);
	EXPECT_FLOAT_EQ(f.control.Dude(0).Hover().x, Hx(512)); // 0.8 W
	EXPECT_FLOAT_EQ(f.control.Dude(0).Hover().y, Hy(360)); // 0.75 H
	EXPECT_FLOAT_EQ(f.control.Dude(1).Hover().x, Hx(128));
	EXPECT_FLOAT_EQ(f.control.Dude(1).Hover().y, Hy(360));
	EXPECT_EQ(f.control.Dude(0).HoverX().speed, 0.0f);
}

TEST(Spirits, FlyIgnoredAtHome)
{
	Fixture f;
	const glm::vec2 before = f.control.Dude(0).Hover();
	f.control.SpiritFly(1, 0.8f, 0.75f);
	EXPECT_EQ(f.control.Dude(0).HoverX().destination, before.x);
}

TEST(Spirits, HoverClamps)
{
	Fixture f;
	HelpDude& d = f.control.Dude(0);
	d.SetHoverX(2.0f, 1.0f, true);
	EXPECT_FLOAT_EQ(d.HoverX().destination, 0.75f);
	d.SetHoverX(-2.0f, 1.0f, true);
	EXPECT_FLOAT_EQ(d.HoverX().destination, -0.75f);
	d.SetHoverY(1.0f, 1.0f, true);
	EXPECT_FLOAT_EQ(d.HoverY().destination, 0.6f);
	d.SetHoverY(-1.0f, 1.0f, true);
	EXPECT_FLOAT_EQ(d.HoverY().destination, -0.6f);
	d.SetHoverX(2.0f, 1.0f, false);
	EXPECT_FLOAT_EQ(d.HoverX().destination, 2.0f);
	// below 0.001 s the spline snaps
	d.SetHoverY(0.25f, 0.0005f, true);
	EXPECT_FLOAT_EQ(d.HoverY().value, 0.25f);
}

TEST(Spirits, FocusScale)
{
	Fixture f;
	f.Frames(1);
	// 0.8 e^(-+0.5 (0.4 - 0.5)) (0x5C3A9E..0x5C3AFF)
	EXPECT_NEAR(f.control.Dude(0).ModelScale(), 0.841, 5e-4);
	EXPECT_NEAR(f.control.Dude(1).ModelScale(), 0.761, 5e-4);
	// the exe's values with the FPU at 24 bits (the inline exp rounded per step): 0x3F574CE2 / 0x3F42CFD2; the double
	// 0.8 e^-0.05 would be one ulp below for dude 1
	EXPECT_EQ(f.control.Dude(0).ModelScale(), 0.841016889f);
	EXPECT_EQ(f.control.Dude(1).ModelScale(), 0.760983586f);
	EXPECT_EQ(f.control.Focus(), 0);
}

TEST(Spirits, PointSide)
{
	Fixture f;
	f.control.SpiritEject(1, false);
	HelpDude& d = f.control.Dude(0);
	d.SetPosition({480, 240}); // hx 0.5
	// LocalRand(2) = 1 gives the random default side 1 (0x5BD782..0x5BD792): the rule must override it
	f.randomMax = true;
	f.randCalls.clear();
	d.ScreenPoint({320, 240}); // target hx 0, left of the dude: side 2, the right arm, x + 0.2
	// the LocalRand(2) is drawn before the rules (0x5BD784)
	EXPECT_NE(std::find(f.randCalls.begin(), f.randCalls.end(), 2), f.randCalls.end());
	EXPECT_EQ(d.State(), dude_state::PointIntroR);
	EXPECT_FLOAT_EQ(d.HoverX().destination, 0.2f);
	d.SetState(dude_state::Hover, true);
	// LocalRand(2) = 0 gives the random default side 2: the rule must override it again
	f.randomMax = false;
	d.SetPosition({160, 240}); // hx -0.5: target on its right: side 1, x - 0.2
	d.ScreenPoint({320, 240});
	EXPECT_EQ(d.State(), dude_state::PointIntroL);
	EXPECT_FLOAT_EQ(d.HoverX().destination, -0.2f);
}

TEST(Spirits, PlayAnimAndPlayed)
{
	Fixture f;
	EXPECT_FALSE(f.control.SpiritPlayingAnim(1));
	f.control.SpiritPlayAnim(1, 0.5f, 0.5f, 24, 1.0f); // ejects first
	EXPECT_EQ(f.control.State(0), ControlState::Out);
	EXPECT_EQ(f.control.Dude(0).State(), dude_state::FlyToAnim);
	EXPECT_TRUE(f.control.SpiritPlayingAnim(1)); // SPIRIT_PLAYED pushes the negation
	f.control.SpiritPlayAnim(1, 0.5f, 0.5f, 0, 1.0f); // anim 0 stops it
	EXPECT_FALSE(f.control.SpiritPlayingAnim(1));
}

TEST(Spirits, AudioTags)
{
	int errors = 0;
	auto tags = ParseAudioTags("[TE pleased TA knockscreen]", 1.5f, &errors);
	ASSERT_EQ(tags.size(), 2u);
	EXPECT_EQ(errors, 0);
	EXPECT_EQ(tags[0].who, 0);
	EXPECT_EQ(tags[0].action, 4);
	EXPECT_EQ(tags[0].index, 1); // Pleased
	EXPECT_EQ(tags[0].value, 100);
	EXPECT_EQ(tags[0].time, 1.5f);
	EXPECT_EQ(tags[1].action, 1);
	EXPECT_EQ(tags[1].index, 58); // KnockScreen
	tags = ParseAudioTags("[TA pray OA dismiss]", 0.0f, &errors);
	ASSERT_EQ(tags.size(), 2u);
	EXPECT_EQ(tags[0].index, 51);
	EXPECT_EQ(tags[1].who, 1);
	EXPECT_EQ(tags[1].index, 55);
	tags = ParseAudioTags("[TE sad40]", 0.0f, &errors);
	ASSERT_EQ(tags.size(), 1u);
	EXPECT_EQ(tags[0].index, 3);
	EXPECT_EQ(tags[0].value, 40);
	tags = ParseAudioTags("[GLS camera]", 0.0f, &errors);
	ASSERT_EQ(tags.size(), 1u);
	EXPECT_EQ(tags[0].who, 2);
	EXPECT_EQ(tags[0].action, 6);
	EXPECT_EQ(tags[0].index, 2);
	EXPECT_EQ(errors, 0);

	// fn_0042ACC0: an emotion tag on the speaker
	Fixture f;
	HelpDude& d = f.control.Dude(1);
	d.FireTag(ParseAudioTags("[TE furious50]", 0.0f)[0], true, true);
	EXPECT_EQ(d.EmotionTarget(), 7u);
	EXPECT_FLOAT_EQ(d.EmotionPeak(), 0.5f);
	// "G" from the evil spirit goes to its partner
	d.partner = &f.control.Dude(0);
	d.FireTag(ParseAudioTags("[GA shrug]", 0.0f)[0], true, true);
	EXPECT_EQ(f.control.Dude(0).SlotMode(64), 1);
	EXPECT_EQ(d.SlotMode(64), 0);
}
