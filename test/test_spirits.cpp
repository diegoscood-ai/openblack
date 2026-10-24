/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// help::spirits (src/Help/Spirits.h): AdvisorSpiritController, the spirit script calls and AdvisorSpirit's hover motion (the
// hover splines, SetState and the update) as pure logic, with the random streams and the camera given by the test. No .hd is
// needed: the dudes have no clips, so only the motion runs.

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Help/HelpSystem.h"
#include "Help/ScriptSpirits.h"
#include "Help/Spirits.h"
#include "Help/SpiritsRuntime.h"

using namespace openblack;
using namespace openblack::help::spirits;

namespace
{
constexpr Screen k_Screen {640, 480};

struct Fixture
{
	bool randomMax {false};
	std::vector<int32_t> randCalls;
	std::vector<glm::vec2> randomCalls;          ///< the (a, b) of every Random
	std::vector<int> sentenceUpdates;            ///< the advisor of every line update
	std::optional<LipSyncFrame> lip;             ///< what the voice gives every lip sync
	std::vector<std::pair<int, float>> lipCalls; ///< the advisor and the seconds of every lip sync
	/// Every line update ('s') and lip sync ('l'), with its advisor, in the order they were asked for
	std::vector<std::pair<char, int>> calls;
	bool soundBank {true};                        ///< whether the in-game bank is there
	std::vector<std::pair<int, AnimSound>> plays; ///< the advisor and the sound of every anim sound played
	DudeData good;
	DudeData evil;
	AdvisorSpiritController control;

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
		q.updateSentence = [this](int dude) {
			sentenceUpdates.push_back(dude);
			calls.emplace_back('s', dude);
		};
		q.lipSync = [this](int dude, float dt) {
			lipCalls.emplace_back(dude, dt);
			calls.emplace_back('l', dude);
			return lip;
		};
		q.soundBank = [this]() { return soundBank; };
		q.playAnimSound = [this](int dude, const AnimSound& sound) { plays.emplace_back(dude, sound); };
		return q;
	}

	void Frames(int count, float dt = 0.1f)
	{
		FrameInput input;
		input.dt = dt;
		input.frameMs = static_cast<int32_t>(dt * 1000.0f + 0.5f);
		input.trailDt = static_cast<float>(input.frameMs) * 0.01f;
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

TEST(Spirits, HelpSpiritOfThenDude)
{
	// HelpSpiritOf (Help/ScriptSpirits.h, reused), then DudeOf: type 1 -> dude 0, any other -> 1
	auto rand = [] { return 51U; };
	EXPECT_EQ(AdvisorSpiritController::DudeOf(help::script_spirits::HelpSpiritOf(1, 0, rand)), k_GoodDude);
	EXPECT_EQ(AdvisorSpiritController::DudeOf(help::script_spirits::HelpSpiritOf(2, 0, rand)), k_EvilDude);
	EXPECT_EQ(AdvisorSpiritController::DudeOf(help::script_spirits::HelpSpiritOf(3, 2, rand)), k_EvilDude);
	EXPECT_EQ(AdvisorSpiritController::DudeOf(help::script_spirits::HelpSpiritOf(4, 2, rand)), k_GoodDude);
	EXPECT_EQ(AdvisorSpiritController::DudeOf(help::script_spirits::HelpSpiritOf(5, 0, rand)), k_EvilDude);
	EXPECT_EQ(AdvisorSpiritController::DudeOf(help::script_spirits::HelpSpiritOf(0, 0, rand)), k_GoodDude);
}

TEST(Spirits, StartAtHome)
{
	Fixture f;
	for (int d = 0; d < k_Dudes; ++d)
	{
		EXPECT_EQ(f.control.State(d), ControlState::Home);
		EXPECT_EQ(f.control.Dude(d).State(), dude_state::k_Hover);
	}
	// the off-screen home anchors
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
		f.control.SpiritEject(1, false); // CHL 7 from a challenge script: Eject
		EXPECT_EQ(f.control.State(0), ControlState::Out);
		EXPECT_EQ(f.control.Timer(0), 0.0f);
		// LocalRand(W/2) + W/4 then LocalRand(H/2) + H/4, in that order
		ASSERT_GE(f.randCalls.size(), 2u);
		EXPECT_EQ(f.randCalls[0], 320);
		EXPECT_EQ(f.randCalls[1], 240);
		const AdvisorSpirit& d = f.control.Dude(0);
		const float x = d.HoverX().destination;
		const float y = d.HoverY().destination;
		EXPECT_GE(x, Hx(160));
		EXPECT_LE(x, Hx(160 + 319));
		EXPECT_GE(y, Hy(120));
		EXPECT_LE(y, Hy(120 + 239));
		EXPECT_FLOAT_EQ(x, high ? Hx(479) : Hx(160));
		EXPECT_FLOAT_EQ(y, high ? Hy(359) : Hy(120));
		EXPECT_FLOAT_EQ(d.HoverX().GetDuration(), 1.0f);
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
	f.control.SpiritEject(1, true); // CHL 300 / CHL 7 from a help script: Appear
	f.control.SpiritEject(2, true);
	const AdvisorSpirit& good = f.control.Dude(0);
	const AdvisorSpirit& evil = f.control.Dude(1);
	EXPECT_EQ(f.control.State(0), ControlState::Out);
	EXPECT_EQ(f.control.State(1), ControlState::Out);
	EXPECT_FLOAT_EQ(good.Hover().x, Hx(160)); // (W/4, H/2)
	EXPECT_FLOAT_EQ(good.Hover().y, 0.0f);
	EXPECT_FLOAT_EQ(evil.Hover().x, Hx(480)); // (3W/4, H/2)
	EXPECT_FLOAT_EQ(evil.Hover().y, 0.0f);
	EXPECT_EQ(good.Alpha(), 0.0f);
	EXPECT_EQ(good.AlphaTarget(), 1.0f);
	EXPECT_TRUE(good.PuffRunning());
	// fades in at 3/s
	f.Frames(1);
	EXPECT_NEAR(f.control.Dude(0).Alpha(), 0.3f, 1e-5f);
}

TEST(Spirits, PuffParticlesDrawRandomInTheExeOrder)
{
	// grey, vx, vy, vz, size, spin: the order of the random draws (at the puff's start and at the particles' creation)
	const std::array<glm::vec2, 6> order = {glm::vec2(116.0f, 250.0f), glm::vec2(-0.8f, 0.8f), glm::vec2(-1.9f, 1.5f),
	                                        glm::vec2(-1.0f, 1.0f),    glm::vec2(4.0f, 8.0f),  glm::vec2(-2.0f, 2.0f)};
	Fixture f;
	AdvisorSpirit& d = f.control.Dude(0);
	f.control.SpiritEject(1, true); // Appear: StartPuff before the particles exist draws nothing
	EXPECT_TRUE(d.PuffRunning());
	EXPECT_FALSE(d.PuffParticles().has_value());
	f.randomCalls.clear();
	f.Frames(1); // the first draw of the puff makes them, after the dude's update
	ASSERT_TRUE(d.PuffParticles().has_value());
	ASSERT_GE(f.randomCalls.size(), 6 * k_PuffParticles);
	const size_t first = f.randomCalls.size() - 6 * k_PuffParticles;
	for (size_t i = 0; i < 6 * k_PuffParticles; ++i)
	{
		EXPECT_EQ(f.randomCalls[first + i], order.at(i % 6)) << i;
	}
	const PuffParticle& p = d.PuffParticles()->at(0);
	EXPECT_EQ(p.grey, 0x747474u); // 116 truncated, three bytes
	// drawVelocityY is the vy before this frame's drift, which has already moved velocity.y (UpdateDraw)
	EXPECT_FLOAT_EQ(p.drawVelocityY, -1.9f);
	EXPECT_EQ(p.k, (1.0f - -1.9f) + 1.0f);
	EXPECT_GT(p.age, 0.0f); // stepped by this frame's draw
	// a second puff draws them again at once, in StartPuff itself
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
	f.control.SpiritHome(1, false); // CHL 8 from a challenge script: Home
	EXPECT_EQ(f.control.State(0), ControlState::GoingHome);
	f.Frames(9);
	EXPECT_EQ(f.control.State(0), ControlState::GoingHome);
	f.Frames(2);
	EXPECT_EQ(f.control.State(0), ControlState::Home);
	EXPECT_EQ(f.control.Dude(0).State(), dude_state::k_Hover);
}

TEST(Spirits, VanishWaitsForThePuff)
{
	Fixture f;
	f.control.SpiritEject(2, false);
	f.control.SpiritHome(2, true); // CHL 301: Vanish
	const AdvisorSpirit& d = f.control.Dude(1);
	EXPECT_EQ(f.control.State(1), ControlState::GoingHome);
	EXPECT_EQ(d.AlphaTarget(), 0.0f);
	EXPECT_TRUE(d.PuffRunning());
	f.Frames(5);
	EXPECT_NEAR(d.Alpha(), 0.0f, 1e-6f); // fades out at 2/s
	f.Frames(6);                         // 1.1 s: the timer is past 1 s but the puff still runs
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
	const AdvisorSpirit& good = f.control.Dude(0);
	const AdvisorSpirit& evil = f.control.Dude(1);
	EXPECT_EQ(f.control.State(0), ControlState::Clinging);
	EXPECT_EQ(good.ClingEdge(), Edge::Right);
	EXPECT_FLOAT_EQ(good.ClingTarget().x, 1.04f);
	EXPECT_EQ(evil.ClingEdge(), Edge::Left);
	EXPECT_FLOAT_EQ(evil.ClingTarget().x, -1.04f);
	// from home: snapped to the edge's off-screen anchor first, then arriving
	EXPECT_FLOAT_EQ(good.Hover().x, Hx(960));
	EXPECT_FLOAT_EQ(evil.Hover().x, Hx(-320));
	EXPECT_EQ(good.State(), dude_state::k_ClingArrive);
	EXPECT_FLOAT_EQ(good.HoverX().destination, 1.04f);

	// |x| > 1.2820513 |y| -> left / right, else bottom / top
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
	// as in Land 1's FollowUs challenge script
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
	AdvisorSpirit& d = f.control.Dude(0);
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
	// 0.8 e^(-+0.5 (0.4 - 0.5))
	EXPECT_NEAR(f.control.Dude(0).ModelScale(), 0.841, 5e-4);
	EXPECT_NEAR(f.control.Dude(1).ModelScale(), 0.761, 5e-4);
	// the original's values with the FPU at 24 bits (the exp rounded per step): 0x3F574CE2 / 0x3F42CFD2; the double
	// 0.8 e^-0.05 would be one ulp below for dude 1
	EXPECT_EQ(f.control.Dude(0).ModelScale(), 0.841016889f);
	EXPECT_EQ(f.control.Dude(1).ModelScale(), 0.760983586f);
	EXPECT_EQ(f.control.Focus(), 0);
}

TEST(Spirits, PointSide)
{
	Fixture f;
	f.control.SpiritEject(1, false);
	AdvisorSpirit& d = f.control.Dude(0);
	d.SetPosition({480, 240}); // hx 0.5
	// LocalRand(2) = 1 gives the random default side 1: the rule must override it
	f.randomMax = true;
	f.randCalls.clear();
	d.ScreenPoint({320, 240}); // target hx 0, left of the dude: side 2, the right arm, x + 0.2
	// the LocalRand(2) is drawn before the rules
	EXPECT_NE(std::find(f.randCalls.begin(), f.randCalls.end(), 2), f.randCalls.end());
	EXPECT_EQ(d.State(), dude_state::k_PointIntroR);
	EXPECT_FLOAT_EQ(d.HoverX().destination, 0.2f);
	d.SetState(dude_state::k_Hover, true);
	// LocalRand(2) = 0 gives the random default side 2: the rule must override it again
	f.randomMax = false;
	d.SetPosition({160, 240}); // hx -0.5: target on its right: side 1, x - 0.2
	d.ScreenPoint({320, 240});
	EXPECT_EQ(d.State(), dude_state::k_PointIntroL);
	EXPECT_FLOAT_EQ(d.HoverX().destination, -0.2f);
}

TEST(Spirits, PlayAnimAndPlayed)
{
	Fixture f;
	EXPECT_FALSE(f.control.SpiritPlayingAnim(1));
	f.control.SpiritPlayAnim(1, 0.5f, 0.5f, 24, 1.0f); // ejects first
	EXPECT_EQ(f.control.State(0), ControlState::Out);
	EXPECT_EQ(f.control.Dude(0).State(), dude_state::k_FlyToAnim);
	EXPECT_TRUE(f.control.SpiritPlayingAnim(1));      // SPIRIT_PLAYED pushes the negation
	f.control.SpiritPlayAnim(1, 0.5f, 0.5f, 0, 1.0f); // anim 0 stops it
	EXPECT_FALSE(f.control.SpiritPlayingAnim(1));
}

TEST(Spirits, AudioTags)
{
	std::string word;
	int errors = 0;
	auto tags = ParseAudioTags("[TE pleased TA knockscreen]", 1.5f, word, &errors);
	ASSERT_EQ(tags.size(), 2u);
	EXPECT_EQ(errors, 0);
	EXPECT_EQ(tags[0].who, 0);
	EXPECT_EQ(tags[0].action, 4);
	EXPECT_EQ(tags[0].index, 1); // Pleased
	EXPECT_EQ(tags[0].value, 100);
	EXPECT_EQ(tags[0].time, 1.5f);
	EXPECT_EQ(tags[1].action, 1);
	EXPECT_EQ(tags[1].index, 58); // KnockScreen
	EXPECT_EQ(tags[1].time, 0.0f); // only the label's first tag has its time
	tags = ParseAudioTags("[TA pray OA dismiss]", 0.0f, word, &errors);
	ASSERT_EQ(tags.size(), 2u);
	EXPECT_EQ(tags[0].index, 51);
	EXPECT_EQ(tags[1].who, 1);
	EXPECT_EQ(tags[1].index, 55);
	tags = ParseAudioTags("[TE sad40]", 0.0f, word, &errors);
	ASSERT_EQ(tags.size(), 1u);
	EXPECT_EQ(tags[0].index, 3);
	EXPECT_EQ(tags[0].value, 40);
	tags = ParseAudioTags("[GLS camera]", 0.0f, word, &errors);
	ASSERT_EQ(tags.size(), 1u);
	EXPECT_EQ(tags[0].who, 2);
	EXPECT_EQ(tags[0].action, 6);
	EXPECT_EQ(tags[0].index, 2);
	EXPECT_EQ(errors, 0);

	// FireTag: an emotion tag on the speaker
	Fixture f;
	AdvisorSpirit& d = f.control.Dude(1);
	d.FireTag(ParseAudioTags("[TE furious50]", 0.0f, word)[0], true, true);
	EXPECT_EQ(d.EmotionTarget(), 7u);
	EXPECT_FLOAT_EQ(d.EmotionPeak(), 0.5f);
	// "G" from the evil spirit goes to its partner
	d.partner = &f.control.Dude(0);
	d.FireTag(ParseAudioTags("[GA shrug]", 0.0f, word)[0], true, true);
	EXPECT_EQ(f.control.Dude(0).SlotMode(64), 1);
	EXPECT_EQ(d.SlotMode(64), 0);
}

TEST(Spirits, TheEyesAreScaledByTheFaceBeforeTheLids)
{
	// Each eye's three bones by its own scale, a bone of 0 or past the matrices skipped, the translations kept
	std::vector<glm::mat4> local(8, glm::mat4(1.0f));
	local[3][3] = glm::vec4(1.0f, 2.0f, 3.0f, 1.0f);
	ScaleEyes({1, 0, 2, 3, 9, 0, 4}, 2.0f, 3.0f, local);
	EXPECT_EQ(local[0], glm::mat4(1.0f));
	EXPECT_EQ(local[1][0], glm::vec4(2.0f, 0.0f, 0.0f, 0.0f));
	EXPECT_EQ(local[1][2], glm::vec4(0.0f, 0.0f, 2.0f, 0.0f));
	EXPECT_EQ(local[1][3], glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
	EXPECT_EQ(local[2][1], glm::vec4(0.0f, 2.0f, 0.0f, 0.0f));
	EXPECT_EQ(local[3][1], glm::vec4(0.0f, 3.0f, 0.0f, 0.0f));
	EXPECT_EQ(local[3][3], glm::vec4(1.0f, 2.0f, 3.0f, 1.0f));
	EXPECT_EQ(local[4], glm::mat4(1.0f)); // the head is not an eye

	// The face pushes the scale with its records 3 and 4
	Fixture f;
	f.control.SpiritEject(1, false);
	f.Frames(2);
	const auto& layers = f.control.Dude(0).Layers();
	const auto scale = std::ranges::find(layers, AnimLayer::Kind::ScaleEyes, &AnimLayer::kind);
	ASSERT_NE(scale, layers.end());
	EXPECT_EQ(scale->blend, f.control.Dude(0).Face()[3]);
	EXPECT_EQ(scale->blendB, f.control.Dude(0).Face()[4]);
}

TEST(Spirits, ThePupilsMoveWithTheFace)
{
	Fixture f;
	f.good.pupilScale = 0.5f;
	auto& neutral = f.good.faces[0];
	neutral[5] = 0.3f;
	neutral[6] = 0.1f;
	neutral[7] = 0.2f;
	neutral[8] = 1.9f;
	// The mesh's own texture until the face is first updated
	EXPECT_FALSE(f.control.Dude(0).Pupils().has_value());
	f.control.SpiritEject(1, false);
	f.Frames(1);
	const auto pupils = f.control.Dude(0).Pupils();
	ASSERT_TRUE(pupils.has_value());
	// across = rec[7 + e] + rec[5 + e], down = rec[5 + e] - rec[7 + e], each within 0 and 2, times the scale
	EXPECT_FLOAT_EQ((*pupils)[0].x, 0.25f);
	EXPECT_FLOAT_EQ((*pupils)[0].y, 0.05f);
	EXPECT_FLOAT_EQ((*pupils)[1].x, 1.0f);
	EXPECT_FLOAT_EQ((*pupils)[1].y, 0.0f);
}

TEST(Spirits, TheFaceStartsFromTheDefaultRecord)
{
	// Whatever the file's records, the face is the default one until it is first updated
	Fixture f;
	f.good.faces[0].fill(3.0f);
	const AdvisorSpirit fresh(k_GoodDude, f.good, f.control);
	EXPECT_EQ(fresh.Face(), help::k_DefaultFaceRecord);
	EXPECT_EQ(fresh.Face()[11], 1.0f); // the base pose at its own pace
}

TEST(Spirits, CloseAdvisorsSpreadInDepthByHowCloseTheyAre)
{
	Fixture f;
	f.control.SpiritEject(1, false);
	f.control.SpiritEject(2, false);
	// Both fly to the same spot, so they come close
	bool close = false;
	for (int frame = 0; frame < 40 && !close; ++frame)
	{
		f.Frames(1);
		const float d = f.control.Dude(0).PartnerDistance();
		close = d * d < 0.7f; // well inside the spacing's 0.9
	}
	ASSERT_TRUE(close);
	// The focused one comes forward by its closeness, at least to 0, and the other backs off by half of it
	const int focus = f.control.Focus();
	const float k = f.control.Dude(focus).PartnerDistance() * 1.1111110f;
	const float v = 1.0f - k * k;
	EXPECT_EQ(f.control.Dude(focus).DepthChannel().destination, v < 0.0f ? 0.0f : v);
	EXPECT_EQ(f.control.Dude(1 - focus).DepthChannel().destination, v * -0.5f);
}

TEST(Spirits, AudioTagsFromEveryBracket)
{
	std::string word;
	int errors = 0;
	// Text outside the brackets is skipped, and every bracket is read
	const auto tags = ParseAudioTags("well [TE sad] then [TA pray]", 0.0f, word, &errors);
	ASSERT_EQ(tags.size(), 2u);
	EXPECT_EQ(errors, 0);
	EXPECT_EQ(tags[0].action, 4);
	EXPECT_EQ(tags[1].index, 51);
	EXPECT_TRUE(ParseAudioTags("no tags here", 0.0f, word, &errors).empty());
	// An unclosed bracket is an error
	errors = 0;
	EXPECT_EQ(ParseAudioTags("[TE sad", 0.0f, word, &errors).size(), 1u);
	EXPECT_EQ(errors, 1);
	// ';' ends a group as an unclosed one, and the search goes on after it
	errors = 0;
	EXPECT_EQ(ParseAudioTags("[TE sad; x] [TA pray]", 0.0f, word, &errors).size(), 2u);
	EXPECT_EQ(errors, 1);
}

TEST(Spirits, ATagWithNoNameTakesTheLastWordRead)
{
	// The word buffer is the caller's and keeps its word from one call to the next
	std::string word;
	std::ignore = ParseAudioTags("[TA pray]", 0.0f, word);
	EXPECT_EQ(word, "pray");
	const auto carried = ParseAudioTags("[OA]", 1.0f, word);
	ASSERT_EQ(carried.size(), 1u);
	EXPECT_EQ(carried[0].who, 1);
	EXPECT_EQ(carried[0].action, 1);
	EXPECT_EQ(carried[0].index, 51); // Pray
	// A buffer of its own starts empty
	std::string fresh;
	const auto alone = ParseAudioTags("[OA]", 1.0f, fresh);
	ASSERT_EQ(alone.size(), 1u);
	EXPECT_NE(alone[0].index, 51);
	EXPECT_TRUE(fresh.empty());
}

TEST(Spirits, OneBadLabelDropsTheSentencesTags)
{
	std::string word;
	const std::array good {TagLabel {"[TE sad]", 0.5f}, TagLabel {"[TA pray]", 1.0f}};
	const auto tags = BuildSentenceTags(good, word);
	ASSERT_EQ(tags.size(), 2u);
	EXPECT_EQ(tags[0].time, 0.5f);
	EXPECT_EQ(tags[1].time, 1.0f);
	// Each label's first tag has the label's time, the ones after it 0, in a second bracket too
	const std::array two {TagLabel {"[TE sad TA pray]", 0.5f}, TagLabel {"[TE pleased] [TA dismiss]", 1.0f}};
	const auto times = BuildSentenceTags(two, word);
	ASSERT_EQ(times.size(), 4u);
	EXPECT_EQ(times[0].time, 0.5f);
	EXPECT_EQ(times[1].time, 0.0f);
	EXPECT_EQ(times[2].time, 1.0f);
	EXPECT_EQ(times[3].time, 0.0f);
	const std::array bad {TagLabel {"[TE sad]", 0.5f}, TagLabel {"[TA pray", 1.0f}};
	EXPECT_TRUE(BuildSentenceTags(bad, word).empty());
}

TEST(Spirits, AStoppedLineFiresItsTagsBeforeTheStopOnItsAdvisor)
{
	std::string word;
	Fixture f;
	AdvisorSpirit& d = f.control.Dude(0);
	d.partner = &f.control.Dude(1);
	// A tag for the other advisor fires on this one too
	f.control.SetSentenceTags(0, {ParseAudioTags("[OE sad]", 0.5f, word)[0], ParseAudioTags("[TE furious]", 2.0f, word)[0]});
	f.control.StopSentence(0, 1.0f);
	EXPECT_EQ(d.EmotionTarget(), 3u);
	EXPECT_EQ(f.control.Dude(1).EmotionTarget(), 0u);
	// The later tag is dropped with the line
	f.control.StopSentence(0, 10.0f);
	EXPECT_EQ(d.EmotionTarget(), 3u);
}

TEST(Spirits, AnimSoundsPlayAsTheirPointsArePassed)
{
	const std::array events {help::HelpDudeAnimEvent {1, 1, 0.1f}, help::HelpDudeAnimEvent {2, 1, 0.5f},
	                         help::HelpDudeAnimEvent {3, 1, 0.9f}};
	float last = -1.0f;
	// Not heard before: from where it is now
	EXPECT_TRUE(CrossedSounds(events, last, 0.0f).empty());
	auto crossed = CrossedSounds(events, last, 0.2f);
	ASSERT_EQ(crossed.size(), 1u);
	EXPECT_EQ(crossed[0].sample, 1u);
	// A jump of more than half the anim plays nothing, but is remembered
	EXPECT_TRUE(CrossedSounds(events, last, 0.95f).empty());
	EXPECT_FLOAT_EQ(last, 0.95f);
	// Round the end of the loop: both the end's and the start's points
	last = 0.85f;
	crossed = CrossedSounds(events, last, 1.15f);
	ASSERT_EQ(crossed.size(), 2u);
	EXPECT_EQ(crossed[0].sample, 1u);
	EXPECT_EQ(crossed[1].sample, 3u);
	EXPECT_FLOAT_EQ(last, 0.15f);
	// Both ends are included: held on a point, it plays every time
	last = 0.5f;
	EXPECT_EQ(CrossedSounds(events, last, 0.5f).size(), 1u);
	EXPECT_EQ(CrossedSounds(events, last, 0.5f).size(), 1u);
}

TEST(Spirits, AnAnimSoundGoesToTheInGameBankAsTheOriginalSendsIt)
{
	// Values the original gives for a knock on the screen heard from (10, 20.5, 33.25) with the advisor at (1, 2, 3)
	const glm::vec3 position {1.0f, 2.0f, 3.0f};
	const glm::vec3 camera {10.0f, 20.5f, 33.25f};
	const auto played = MakeAnimSound(58, help::HelpDudeAnimEvent {155, 1, 0.34f}, position, camera);
	EXPECT_EQ(played.anim, 58u);
	EXPECT_EQ(played.key, (std::array<int32_t, 5> {1, 1, 17, 1, 155}));
	EXPECT_EQ(played.action, 0);
	EXPECT_EQ(played.position, position);
	EXPECT_EQ(std::bit_cast<uint32_t>(played.distance), 0x421254F2u);
	// A clear flag stops the sound instead
	const auto stopped = MakeAnimSound(60, help::HelpDudeAnimEvent {154, 0, 0.48f}, position, camera);
	EXPECT_EQ(stopped.key, (std::array<int32_t, 5> {1, 1, 17, 1, 154}));
	EXPECT_EQ(stopped.action, 1);
}

TEST(Spirits, AnAvoidHopPlaysItsSoundsAsItsPhasePassesThem)
{
	Fixture f;
	f.good.sounds[anim::k_AvoidL] = {help::HelpDudeAnimEvent {155, 1, 0.2f}, help::HelpDudeAnimEvent {154, 0, 0.5f}};
	AdvisorSpirit& d = f.control.Dude(0);
	d.SetState(dude_state::k_Avoid, true);
	ASSERT_EQ(d.State(), dude_state::k_Avoid);
	// The hop's phase is 1.5 x its time: 0.15, 0.3, 0.45, 0.6... The first frame only remembers where it is
	f.Frames(1);
	EXPECT_TRUE(f.plays.empty());
	f.Frames(1);
	ASSERT_EQ(f.plays.size(), 1u);
	EXPECT_EQ(f.plays[0].first, 0);
	EXPECT_EQ(f.plays[0].second.anim, anim::k_AvoidL);
	EXPECT_EQ(f.plays[0].second.key[4], 155);
	EXPECT_EQ(f.plays[0].second.action, 0);
	ASSERT_EQ(d.Sounds().size(), 1u);
	EXPECT_EQ(d.Sounds()[0].key[4], 155);
	// Heard from the camera the queries give, (0, 0, 0) here
	const glm::vec3 p = f.plays[0].second.position;
	EXPECT_FLOAT_EQ(f.plays[0].second.distance, std::sqrt((p.z * p.z + p.y * p.y) + p.x * p.x));
	f.Frames(1);
	EXPECT_EQ(f.plays.size(), 1u);
	EXPECT_TRUE(d.Sounds().empty());
	f.Frames(1);
	ASSERT_EQ(f.plays.size(), 2u);
	EXPECT_EQ(f.plays[1].second.key[4], 154);
	EXPECT_EQ(f.plays[1].second.action, 1);
	// No other point is passed before the hop ends
	f.Frames(4);
	EXPECT_EQ(f.plays.size(), 2u);
}

TEST(Spirits, WithoutTheInGameBankNoAnimSoundIsLookedAt)
{
	Fixture f;
	f.soundBank = false;
	f.good.sounds[anim::k_AvoidL] = {help::HelpDudeAnimEvent {155, 1, 0.2f}, help::HelpDudeAnimEvent {154, 1, 0.5f}};
	AdvisorSpirit& d = f.control.Dude(0);
	d.SetState(dude_state::k_Avoid, true);
	f.Frames(4);
	EXPECT_TRUE(f.plays.empty());
	EXPECT_TRUE(d.Sounds().empty());
	// Once the bank is there the phase starts again from where the hop is, so the point already passed stays quiet
	f.soundBank = true;
	f.Frames(1);
	EXPECT_TRUE(f.plays.empty());
}

TEST(Spirits, EachAdvisorsLineIsLookedAtOnceAFrame)
{
	Fixture f;
	f.Frames(1);
	EXPECT_EQ(f.sentenceUpdates, (std::vector<int> {0, 1}));
	f.control.SpiritEject(1, false);
	f.sentenceUpdates.clear();
	f.Frames(2);
	EXPECT_EQ(f.sentenceUpdates, (std::vector<int> {0, 1, 0, 1}));
}

TEST(Spirits, AnAdvisorOutRunsItsLipSyncAfterItsLineWithTheFramesSeconds)
{
	Fixture f;
	f.control.SpiritEject(1, false);
	f.Frames(1, 0.05f);
	// The good one is out: its line, then its lip sync with the frame's seconds. The evil one at home hovering only has
	// its line looked at
	EXPECT_EQ(f.calls, (std::vector<std::pair<char, int>> {{'s', 0}, {'l', 0}, {'s', 1}}));
	ASSERT_EQ(f.lipCalls.size(), 1u);
	EXPECT_EQ(f.lipCalls[0].first, 0);
	EXPECT_EQ(f.lipCalls[0].second, 0.05f);
}

TEST(Spirits, BothAdvisorsOutGetTheSameSecondsForTheirLipSync)
{
	Fixture f;
	f.control.SpiritEject(1, false);
	f.control.SpiritEject(2, false);
	f.Frames(2, 0.033f);
	// Once a frame each, the good one first, each after its own line
	EXPECT_EQ(f.calls, (std::vector<std::pair<char, int>> {
	                       {'s', 0}, {'l', 0}, {'s', 1}, {'l', 1}, {'s', 0}, {'l', 0}, {'s', 1}, {'l', 1}}));
	ASSERT_EQ(f.lipCalls.size(), 4u);
	for (const auto& [dude, dt] : f.lipCalls)
	{
		EXPECT_EQ(dt, 0.033f) << dude;
	}
}

TEST(Spirits, AnAdvisorAtHomeHoveringGetsNoLipSync)
{
	Fixture f;
	ASSERT_EQ(f.control.State(0), ControlState::Home);
	ASSERT_EQ(f.control.Dude(0).State(), dude_state::k_Hover);
	f.lip = LipSyncFrame {.time = 1.0f, .playing = true, .weights = {0.5f, 0.5f, 0.0f}};
	f.Frames(3);
	// Only their lines are looked at, once a frame each
	EXPECT_TRUE(f.lipCalls.empty());
	EXPECT_EQ(f.sentenceUpdates, (std::vector<int> {0, 1, 0, 1, 0, 1}));
}

TEST(Spirits, AnAdvisorAtHomeNotHoveringRunsItsWholeUpdateWithTheLipSync)
{
	Fixture f;
	// The evil one stays at home, avoiding rather than hovering
	f.control.Dude(1).SetState(dude_state::k_Avoid, true);
	ASSERT_EQ(f.control.State(1), ControlState::Home);
	ASSERT_EQ(f.control.Dude(1).State(), dude_state::k_Avoid);
	f.Frames(1, 0.1f);
	EXPECT_EQ(f.calls, (std::vector<std::pair<char, int>> {{'s', 0}, {'s', 1}, {'l', 1}}));
	ASSERT_EQ(f.lipCalls.size(), 1u);
	EXPECT_EQ(f.lipCalls[0].first, 1);
	EXPECT_EQ(f.lipCalls[0].second, 0.1f);
}

TEST(SpiritTrail, APointPastEveryFifthAndAResetKeepsItsClock)
{
	Fixture f;
	AdvisorSpirit& d = f.control.Dude(0);
	d.ResetTrail();
	const glm::vec3 start = d.GetTrail().points[0];
	EXPECT_TRUE(std::ranges::all_of(d.GetTrail().points, [&](const glm::vec3& p) { return p == start; }));
	// Whatever its clock starts at, a point is taken on each call while it is past 0.2
	for (int i = 0; i < 64 && d.GetTrail().accumulator > 0.2f; ++i)
	{
		d.UpdateTrail(0.0f);
	}
	ASSERT_LE(d.GetTrail().accumulator, 0.2f);
	const size_t head = d.GetTrail().head;
	const float clock = d.GetTrail().accumulator;
	d.UpdateTrail(0.0f);
	EXPECT_EQ(d.GetTrail().head, head);
	// One point a call at most, however far past
	d.UpdateTrail(0.5f);
	EXPECT_EQ(d.GetTrail().head, (head + 1) & (Trail::k_Points - 1));
	EXPECT_EQ(d.GetTrail().accumulator, (clock + 0.5f) - 0.2f);
	d.UpdateTrail(0.0f);
	EXPECT_EQ(d.GetTrail().head, (head + 2) & (Trail::k_Points - 1));
	// A reset puts every point at the hover now, and leaves where the next point goes and its clock
	const float before = d.GetTrail().accumulator;
	d.ResetTrail();
	EXPECT_EQ(d.GetTrail().head, (head + 2) & (Trail::k_Points - 1));
	EXPECT_EQ(d.GetTrail().accumulator, before);
}

TEST(SpiritTrail, StartsWithItsClockAtFiveAndSparksAsOldAsTheirPlace)
{
	Fixture f;
	const AdvisorSpirit fresh(k_EvilDude, f.evil, f.control);
	const Trail& trail = fresh.GetTrail();
	EXPECT_EQ(trail.accumulator, 5.0f);
	EXPECT_EQ(trail.head, 0u);
	for (size_t i = 0; i < Trail::k_Sparks; ++i)
	{
		EXPECT_EQ(trail.sparks.at(i).age, static_cast<float>(i)) << i;
		EXPECT_EQ(trail.sparks.at(i).position, glm::vec3(0.0f)) << i;
		EXPECT_EQ(trail.sparks.at(i).velocity, glm::vec3(0.0f)) << i;
	}
}

TEST(SpiritTrail, SparksStartAgainPastSixteenWithFiveRandomDraws)
{
	Fixture f;
	AdvisorSpirit fresh(k_GoodDude, f.good, f.control);
	f.randomCalls.clear();
	// Every spark is past 16: sixteen starts, five draws each in this order
	fresh.UpdateTrail(16.0f);
	ASSERT_EQ(f.randomCalls.size(), 5u * Trail::k_Sparks);
	EXPECT_EQ(f.randomCalls[0], glm::vec2(-0.5f, 0.0f));
	EXPECT_EQ(f.randomCalls[1], glm::vec2(-0.1f, 0.1f));
	EXPECT_EQ(f.randomCalls[2], glm::vec2(-0.1f, 0.1f));
	EXPECT_EQ(f.randomCalls[3], glm::vec2(-0.3f, 0.3f));
	EXPECT_EQ(f.randomCalls[4], glm::vec2(-0.1f, 0.1f));
	// The fixture's draws give each range's low end: about the hover, drifting by dt v 0.1, rising by dt 0.021
	const auto& spark = fresh.GetTrail().sparks[0];
	const glm::vec3 hover(fresh.Hover(), fresh.DepthChannel().value);
	EXPECT_FLOAT_EQ(spark.position.x, hover.x + -0.1f + 16.0f * -0.1f * 0.1f);
	EXPECT_FLOAT_EQ(spark.position.y, hover.y + -0.1f + 16.0f * -0.3f * 0.1f);
	EXPECT_FLOAT_EQ(spark.position.z, hover.z + -0.5f);
	EXPECT_FLOAT_EQ(spark.velocity.y, -0.3f + 16.0f * 0.021f);
	EXPECT_EQ(spark.age, 0.0f);
	// Staggered by their start: the oldest is the next past 16
	f.randomCalls.clear();
	fresh.UpdateTrail(0.5f);
	EXPECT_TRUE(f.randomCalls.empty());
	fresh.UpdateTrail(0.6f);
	EXPECT_EQ(f.randomCalls.size(), 5u);
	EXPECT_NEAR(fresh.GetTrail().sparks[15].age, 0.1f, 1e-5f);
}

TEST(SpiritTrail, FedEveryFrameForBothAdvisorsAtHomeToo)
{
	Fixture f;
	const size_t good = f.control.Dude(0).GetTrail().head;
	const size_t evil = f.control.Dude(1).GetTrail().head;
	// Both at home: 0.1 s frames move each trail on by 1 a frame, a point each
	f.Frames(3);
	EXPECT_EQ(f.control.Dude(0).GetTrail().head, (good + 3) & (Trail::k_Points - 1));
	EXPECT_EQ(f.control.Dude(1).GetTrail().head, (evil + 3) & (Trail::k_Points - 1));
}

TEST(SpiritTrail, StillPointsMakeANarrowFaintStrip)
{
	Fixture f;
	AdvisorSpirit& d = f.control.Dude(1);
	d.ResetTrail();
	const auto strip = d.TrailStrip();
	// Still points: no width, the faintest alpha, both sides on the point
	EXPECT_EQ(strip[0].argb, (20u << 24u) | 0xFFFFFFu);
	EXPECT_EQ(strip[0].hover, strip[1].hover);
	// The evil band's edge is at the bottom of the texture, the middle at a half
	EXPECT_EQ(strip[2].uv, glm::vec2(0.03125f, 0.0f));
	EXPECT_EQ(strip[3].uv, glm::vec2(0.03125f, 0.5f));
}

TEST(SpiritTrail, TextureAlongAndAcross)
{
	using openblack::help::spirits::TrailUv;
	EXPECT_EQ(TrailUv(0, false, true), glm::vec2(0.0f, 1.0f));  // good: the rainbow side
	EXPECT_EQ(TrailUv(0, false, false), glm::vec2(0.0f, 0.0f)); // evil: the fire side
	EXPECT_EQ(TrailUv(16, true, true), glm::vec2(0.5f, 0.5f));
	EXPECT_EQ(TrailUv(31, false, true), glm::vec2(0.96875f, 1.0f));
}

TEST(SpiritTrail, TrianglesFromTheNewestSegment)
{
	const auto indices = openblack::help::spirits::TrailIndices();
	// the first two triangles are of points 30 and 31 (60, 61, 63) and (63, 62, 60); the last of points 0 and 1
	const std::array<uint16_t, 6> first = {60, 61, 63, 63, 62, 60};
	const std::array<uint16_t, 6> last = {0, 1, 3, 3, 2, 0};
	for (size_t i = 0; i < 6; ++i)
	{
		EXPECT_EQ(indices.at(i), first.at(i));
		EXPECT_EQ(indices.at(indices.size() - 6 + i), last.at(i));
	}
}
