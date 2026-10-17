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

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <numbers>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/GUtilsDistance.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureLook.h"

using namespace openblack;
using namespace openblack::creature_look;
using creature_desires::Desire;
using creature_desires::Desires;
namespace animations = creature_layers::animations;

namespace
{
constexpr float k_Tolerance = 1e-4f;
constexpr float k_Turn = 0.1f;
constexpr float k_TurnsPerSecond = 10.0f;

/// Always draws the same number, less than the range
creature_mind::Random Always(uint32_t value)
{
	return [value](uint32_t range) { return range == 0 ? 0u : std::min(value, range - 1); };
}

/// A body that plays each action it is told to for some turns
struct FakeBody
{
	int turnsLeft {0};
	bool looping {false};
	std::vector<size_t> played;
	int sits {0};

	[[nodiscard]] creature_mind::Senses Senses() const
	{
		return {.seconds = k_Turn,
		        .bodyBusy = turnsLeft > 0 || looping,
		        .bodyLooping = looping,
		        .strongest = std::nullopt,
		        .feedbackSeconds = std::nullopt,
		        .feedbackWasStroke = false};
	}

	void Obey(const creature_mind::Commands& commands, int actionTurns)
	{
		if (turnsLeft > 0)
		{
			--turnsLeft;
		}
		if (commands.endSit)
		{
			looping = false;
		}
		if (commands.playOnce.has_value() && turnsLeft == 0 && !looping)
		{
			played.push_back(*commands.playOnce);
			turnsLeft = actionTurns;
		}
		if (commands.startSequence && turnsLeft == 0)
		{
			looping = true;
			++sits;
		}
	}
};

Desires OneDesire(Desire desire, float value, float threshold, float increaseSeconds, float max = 1.0f)
{
	Desires desires;
	for (auto& state : desires.desires)
	{
		state.activated = false;
	}
	auto& state = desires[desire];
	state.activated = true;
	state.max = max;
	state.decay = 0.9f;
	state.increaseSeconds = increaseSeconds;
	state.sources.push_back({.type = 1, .value = value, .threshold = threshold, .multiplier = 1.0f});
	return desires;
}
} // namespace

TEST(CreatureDesires, TheSigmoidSteps)
{
	const auto& table = gutils::k_Sigmoid;
	EXPECT_EQ(creature_desires::Sigmoid(0.5f, 0.0f), 0.0f);
	EXPECT_EQ(creature_desires::Sigmoid(0.5f, -0.5f), 0.0f);
	EXPECT_EQ(creature_desires::Sigmoid(1.0f, 0.9f), 0.0f);
	// Exactly at the threshold is the middle step, 0.5
	EXPECT_EQ(creature_desires::Sigmoid(0.5f, 0.5f), table[20]);
	EXPECT_EQ(table[20], 0.5f);
	EXPECT_EQ(creature_desires::Sigmoid(0.0f, 1.0f), table[40]);
	EXPECT_EQ(creature_desires::Sigmoid(0.9f, 0.1f), table[4]);
	EXPECT_LT(creature_desires::Sigmoid(0.9f, 0.1f), 1e-6f);
	// Without the cut off a value of 0 reads its step; a threshold of 1 still gives nothing
	EXPECT_EQ(creature_desires::SigmoidStep(0.5f, 0.0f), table[10]);
	EXPECT_EQ(creature_desires::SigmoidStep(1.0f, 0.9f), 0.0f);
	// Both are the shared game maths' sigmoids
	for (const float threshold : {-0.9f, -0.2f, 0.0f, 0.3f, 0.5f, 0.99f})
	{
		for (const float value : {-1.5f, -0.4f, 0.0f, 0.01f, 0.25f, 0.6f, 1.0f, 2.0f})
		{
			EXPECT_EQ(creature_desires::Sigmoid(threshold, value), gutils::CreatureSigmoidThreshold(threshold, value));
			EXPECT_EQ(creature_desires::SigmoidStep(threshold, value), gutils::SigmoidThreshold(threshold, value));
		}
	}
}

/// The table written as rounded decimals, as an earlier port of the creature had it, is not the game's: 14 of its 41
/// steps are a float or two off the bits stored in the game. The sigmoid uses the game's
TEST(CreatureDesires, ADecimalTableIsNotTheGames)
{
	constexpr std::array<float, 41> k_Decimal {
	    0.0f,         3.6e-9f,      1e-8f,       2.8e-8f,     7.78e-8f,     2.163e-7f,    6.018e-7f,
	    1.674e-6f,    4.6568e-6f,   1.29542e-5f, 3.60351e-5f, 1.002359e-4f, 2.787865e-4f, 7.751431e-4f,
	    0.002153319f, 0.005967208f, 0.01642494f, 0.04439174f, 0.1144373f,   0.2644244f,   0.5f,
	    0.7355757f,   0.8855627f,   0.9556082f,  0.9835750f,  0.9940328f,   0.9978467f,   0.9992248f,
	    0.9997212f,   0.9998997f,   0.9999639f,  0.9999871f,  0.9999954f,   0.9999983f,   0.9999994f,
	    0.9999998f,   0.9999999f,   1.0f,        1.0f,        1.0f,         1.0f,
	};
	constexpr std::array<size_t, 14> k_Differ {16, 18, 19, 23, 24, 26, 27, 29, 30, 31, 32, 33, 35, 36};
	const auto& table = gutils::k_Sigmoid;
	for (size_t i = 0; i < table.size(); ++i)
	{
		const auto ulps = std::abs(static_cast<int64_t>(std::bit_cast<uint32_t>(table.at(i))) -
		                           static_cast<int64_t>(std::bit_cast<uint32_t>(k_Decimal.at(i))));
		const bool listed = std::ranges::find(k_Differ, i) != k_Differ.end();
		EXPECT_EQ(ulps != 0, listed) << "step " << i;
		EXPECT_LE(ulps, 2) << "step " << i;
	}
}

TEST(CreatureDesires, ADesireGrowsByItsDriveOverItsIncreaseTime)
{
	// A source fully past its threshold drives the desire up by 1 in two seconds
	auto desires = OneDesire(Desire::Hunger, 1.0f, 0.0f, 2.0f);
	for (int i = 0; i < 10; ++i)
	{
		creature_desires::UpdateDesires(desires, k_TurnsPerSecond);
	}
	EXPECT_NEAR(desires[Desire::Hunger].value, 0.5f, k_Tolerance);
	EXPECT_NEAR(desires.sum, 0.5f, k_Tolerance);
	for (int i = 0; i < 20; ++i)
	{
		creature_desires::UpdateDesires(desires, k_TurnsPerSecond);
	}
	EXPECT_NEAR(desires[Desire::Hunger].value, 1.0f, k_Tolerance);
}

TEST(CreatureDesires, ADesireWithoutDriveFades)
{
	auto desires = OneDesire(Desire::Play, 0.0f, 0.5f, 5.0f);
	desires[Desire::Play].value = 1.0f;
	creature_desires::UpdateDesires(desires, k_TurnsPerSecond);
	EXPECT_NEAR(desires[Desire::Play].value, 0.9f, k_Tolerance);
}

TEST(CreatureDesires, NoIncreaseTimeAndAValueThatIsNoNumberGoAsTheGamesDo)
{
	// A drive over no time takes the desire to its cap
	auto driven = OneDesire(Desire::Hunger, 1.0f, 0.0f, 0.0f);
	driven[Desire::Hunger].value = 0.3f;
	creature_desires::UpdateDesires(driven, k_TurnsPerSecond);
	EXPECT_EQ(driven[Desire::Hunger].value, 1.0f);
	// No drive over no time is no number, so the desire decays
	auto idle = OneDesire(Desire::Hunger, 0.0f, 0.5f, 0.0f);
	idle[Desire::Hunger].value = 0.5f;
	creature_desires::UpdateDesires(idle, k_TurnsPerSecond);
	EXPECT_EQ(idle[Desire::Hunger].value, 0.5f * 0.9f);
	// A value that is no number takes the floor
	auto unknown = OneDesire(Desire::Hunger, 1.0f, 0.0f, 2.0f);
	unknown[Desire::Hunger].value = std::numeric_limits<float>::quiet_NaN();
	creature_desires::UpdateDesires(unknown, k_TurnsPerSecond);
	EXPECT_EQ(unknown[Desire::Hunger].value, 0.0f);
}

TEST(CreatureDesires, SuppressedForLongerItsPlanIsWorthNothing)
{
	auto desires = OneDesire(Desire::Anger, 1.0f, 0.0f, 1.0f);
	creature_desires::Suppress(desires, Desire::Anger, 2.0f, k_TurnsPerSecond);
	EXPECT_TRUE(desires[Desire::Anger].planVoided);
	desires[Desire::Anger].planVoided = false;
	// Not for a shorter suppression, nor one as long
	creature_desires::Suppress(desires, Desire::Anger, 1.0f, k_TurnsPerSecond);
	creature_desires::Suppress(desires, Desire::Anger, 2.0f, k_TurnsPerSecond);
	EXPECT_FALSE(desires[Desire::Anger].planVoided);
	creature_desires::Suppress(desires, Desire::Anger, 2.5f, k_TurnsPerSecond);
	EXPECT_TRUE(desires[Desire::Anger].planVoided);
}

TEST(CreatureDesires, ASuppressedDesireOnlyFades)
{
	auto desires = OneDesire(Desire::Anger, 1.0f, 0.0f, 1.0f);
	desires[Desire::Anger].value = 0.5f;
	creature_desires::Suppress(desires, Desire::Anger, 1.0f, k_TurnsPerSecond);
	for (int i = 0; i < 9; ++i)
	{
		creature_desires::UpdateDesires(desires, k_TurnsPerSecond);
	}
	const auto faded = desires[Desire::Anger].value;
	EXPECT_LT(faded, 0.5f);
	creature_desires::UpdateDesires(desires, k_TurnsPerSecond);
	EXPECT_NEAR(desires[Desire::Anger].value, faded + 0.1f, k_Tolerance);
	// A shorter suppression doesn't cut a longer one short
	creature_desires::Suppress(desires, Desire::Anger, 2.0f, k_TurnsPerSecond);
	creature_desires::Suppress(desires, Desire::Anger, 0.5f, k_TurnsPerSecond);
	EXPECT_EQ(desires[Desire::Anger].suppressedTurns, 20u);
}

TEST(CreatureDesires, SourcesReadTheirStateThenFade)
{
	auto desires = OneDesire(Desire::Tiredness, 0.4f, 0.5f, 2.0f);
	desires[Desire::Tiredness].sources.push_back({.type = 2, .value = 1.0f, .threshold = 0.5f, .multiplier = 0.5f});
	creature_desires::UpdateSources(desires, [](uint32_t type, const Desires&) -> std::optional<float> {
		return type == 1 ? std::optional(0.8f) : std::nullopt;
	});
	EXPECT_NEAR(desires[Desire::Tiredness].sources[0].value, 0.8f, k_Tolerance);
	EXPECT_NEAR(desires[Desire::Tiredness].sources[1].value, 0.5f, k_Tolerance);
	EXPECT_NEAR(*creature_desires::SourceValue(desires[Desire::Tiredness], 2), 0.5f, k_Tolerance);
	EXPECT_FALSE(creature_desires::SourceValue(desires[Desire::Tiredness], 3).has_value());
}

TEST(CreatureDesires, CreatedFromTheSetup)
{
	std::array<creature_desires::DesireSetup, creature_desires::k_DesireCount> setup {};
	setup.at(static_cast<size_t>(Desire::Hunger)) = {
	    .max = 2.0f,
	    .decayMin = 0.9f,
	    .decayMax = 1.0f,
	    .increaseSeconds = 20.0f,
	    .sources = {{.type = 14, .value = 0.0f, .threshold = 0.4f, .multiplier = 1.0f},
	                {.type = creature_desires::k_NoSource, .value = 0.0f, .threshold = 0.0f, .multiplier = 1.0f}},
	};
	const auto desires = creature_desires::Create(setup, [](float low, float high) { return (low + high) / 2.0f; });
	const auto& hunger = desires[Desire::Hunger];
	EXPECT_FLOAT_EQ(hunger.max, 2.0f);
	EXPECT_FLOAT_EQ(hunger.decay, 0.95f);
	EXPECT_FLOAT_EQ(hunger.increaseSeconds, 20.0f);
	ASSERT_EQ(hunger.sources.size(), 1u);
	EXPECT_EQ(hunger.sources[0].type, 14u);
}

TEST(CreatureDesires, EachDesireDrawsItsDecayOnceInDesireOrder)
{
	// The game draws once per desire, in desire order, whether or not the desire has sources; every desire starts at 0
	std::array<creature_desires::DesireSetup, creature_desires::k_DesireCount> setup {};
	for (size_t d = 0; d < setup.size(); ++d)
	{
		setup.at(d).decayMin = 0.5f + static_cast<float>(d) * 0.01f;
		setup.at(d).decayMax = setup.at(d).decayMin + 0.001f;
	}
	std::vector<std::pair<float, float>> draws;
	const auto desires = creature_desires::Create(setup, [&draws](float low, float high) {
		draws.emplace_back(low, high);
		return low;
	});
	ASSERT_EQ(draws.size(), creature_desires::k_DesireCount);
	for (size_t d = 0; d < draws.size(); ++d)
	{
		EXPECT_EQ(draws[d].first, setup.at(d).decayMin);
		EXPECT_EQ(draws[d].second, setup.at(d).decayMax);
		EXPECT_EQ(desires.desires.at(d).decay, setup.at(d).decayMin);
		EXPECT_EQ(desires.desires.at(d).value, 0.0f);
	}
}

TEST(CreatureDesires, GrowingUpBringsAndTakesDesires)
{
	Desires desires;
	desires[Desire::Fear].suppressedTurns = 50;
	const std::vector<creature_desires::PhaseDesires> phases {
	    {.add = {Desire::Fear, Desire::Tiredness}, .remove = {}},
	    {.add = {Desire::Hunger}, .remove = {Desire::Tiredness}},
	};
	creature_desires::ActivateForPhase(desires, phases, 0);
	EXPECT_TRUE(desires[Desire::Fear].activated);
	EXPECT_TRUE(desires[Desire::Tiredness].activated);
	EXPECT_FALSE(desires[Desire::Hunger].activated);
	EXPECT_EQ(desires[Desire::Fear].suppressedTurns, 0u);
	creature_desires::ActivateForPhase(desires, phases, 1);
	EXPECT_TRUE(desires[Desire::Hunger].activated);
	EXPECT_FALSE(desires[Desire::Tiredness].activated);
}

TEST(CreatureDesires, EachShowableDesireHasItsEmote)
{
	EXPECT_EQ(creature_desires::EmoteFor(Desire::Hunger), animations::k_Hungry);
	EXPECT_EQ(creature_desires::EmoteFor(Desire::AttractAttention), animations::k_FriendlyWave);
	EXPECT_EQ(creature_desires::EmoteFor(Desire::Tiredness), animations::k_Tired);
	EXPECT_EQ(creature_desires::EmoteFor(Desire::Impress), animations::k_Summon);
	EXPECT_EQ(creature_desires::EmoteFor(Desire::GetColder), animations::k_Hot);
	EXPECT_FALSE(creature_desires::EmoteFor(Desire::Wanderlust).has_value());
}

TEST(CreatureDesires, TheStrongestShowableDesire)
{
	Desires desires;
	desires[Desire::Wanderlust].value = 0.9f;
	desires[Desire::Hunger].value = 0.5f;
	desires[Desire::Play].value = 0.6f;
	desires[Desire::Play].activated = false;
	desires[Desire::Sadness].value = 0.3f;
	EXPECT_EQ(creature_desires::StrongestShowable(desires, 0.2f), Desire::Hunger);
	EXPECT_FALSE(creature_desires::StrongestShowable(desires, 0.55f).has_value());
}

TEST(CreatureIdleMind, WaitingForAThingToLandEndsWhenItIsOnTheMapAndGivesUpWhenItIsGone)
{
	using namespace creature_mind;
	const std::vector<Step> agenda {{.kind = Step::Kind::WaitInMap, .object = 7}, {.kind = Step::Kind::Wait, .seconds = 1.0f}};
	IdleMind mind;
	Plan(mind, Activity::Planned, agenda);
	Senses senses {.seconds = 0.1f, .objectInMap = false};
	Think(mind, senses, Always(0));
	Think(mind, senses, Always(0));
	EXPECT_EQ(mind.step, 0u);
	senses.objectInMap = true;
	Think(mind, senses, Always(0));
	EXPECT_EQ(mind.step, 1u);
	EXPECT_FALSE(mind.gaveUp);
	// gone: the rest of the agenda is given up
	Plan(mind, Activity::Planned, agenda);
	senses.objectInMap.reset();
	Think(mind, senses, Always(0));
	Think(mind, senses, Always(0));
	EXPECT_EQ(mind.step, agenda.size());
	EXPECT_TRUE(mind.gaveUp);
}

TEST(CreatureIdleMind, LettingGoOfWhatThePlanActsOnIsDoneAsItStarts)
{
	using namespace creature_mind;
	const std::vector<Step> agenda {{.kind = Step::Kind::ClearObjectToActOn}, {.kind = Step::Kind::Wait, .seconds = 1.0f}};
	IdleMind mind;
	Plan(mind, Activity::Planned, agenda);
	int draws = 0;
	const Random counting = [&draws](uint32_t) {
		++draws;
		return 0u;
	};
	// it waits for the body like any step but a wait, then tells the mind and is done, with no face and no draw
	Senses senses {.seconds = 0.1f, .bodyBusy = true};
	auto commands = Think(mind, senses, counting);
	EXPECT_FALSE(commands.clearObjectToActOn);
	EXPECT_EQ(mind.step, 0u);
	senses.bodyBusy = false;
	commands = Think(mind, senses, counting);
	EXPECT_TRUE(commands.clearObjectToActOn);
	EXPECT_FALSE(commands.face.has_value());
	EXPECT_EQ(draws, 0);
	EXPECT_EQ(mind.step, 1u);
	EXPECT_FALSE(mind.stepStarted);
	EXPECT_FALSE(mind.gaveUp);
	// the next step starts on the next turn, and nothing more is let go of
	commands = Think(mind, senses, counting);
	EXPECT_FALSE(commands.clearObjectToActOn);
	EXPECT_TRUE(mind.stepStarted);
}

TEST(CreatureIdleMind, BeingIdleIsWaitingThenYawningTwice)
{
	creature_mind::IdleMind mind;
	FakeBody body;
	// Draws of 1: waits of 1.001 seconds, faces from the grimace, mirrored actions, and no sitting
	const auto random = Always(1);
	std::vector<int> actionTurns;
	std::vector<creature_mind::Eyes> eyes;
	for (int turn = 0; turn < 200 && body.played.size() < 2; ++turn)
	{
		const auto commands = creature_mind::Think(mind, body.Senses(), random);
		if (turn == 0)
		{
			EXPECT_EQ(mind.activity, creature_mind::Activity::BeIdle);
			EXPECT_EQ(mind.agenda.size(), 4u);
			ASSERT_TRUE(commands.face.has_value());
			EXPECT_EQ(commands.face->face, creature_face::Face::Grimace);
			EXPECT_TRUE(commands.lookAbout);
		}
		if (commands.playOnce.has_value())
		{
			actionTurns.push_back(turn);
			EXPECT_EQ(*commands.playOnce, animations::k_Tired);
			EXPECT_TRUE(commands.mirrored);
		}
		if (commands.eyes != creature_mind::Eyes::Unchanged)
		{
			eyes.push_back(commands.eyes);
		}
		body.Obey(commands, 30);
	}
	ASSERT_EQ(actionTurns.size(), 2u);
	// The wait starts on the first turn and lasts 11 turns more, then the yawn starts
	EXPECT_EQ(actionTurns[0], 12);
	// The yawn keeps the body busy 30 turns; the mind sees it free the turn after, starts the next wait on the one after
	// that, and yawns again 12 turns on
	EXPECT_EQ(actionTurns[1], 12 + 31 + 1 + 12);
	ASSERT_GE(eyes.size(), 3u);
	EXPECT_EQ(eyes[0], creature_mind::Eyes::Sleepy);
	EXPECT_EQ(eyes[1], creature_mind::Eyes::Normal);
	EXPECT_EQ(eyes[2], creature_mind::Eyes::Sleepy);
}

TEST(CreatureIdleMind, IdleStepsPullAFaceAsTheyStartAndEveryFewSeconds)
{
	creature_mind::IdleMind mind;
	FakeBody body;
	std::vector<int> pulled;
	for (int turn = 0; turn < 60; ++turn)
	{
		const auto commands = creature_mind::Think(mind, body.Senses(), Always(1));
		if (commands.face.has_value())
		{
			pulled.push_back(turn);
			EXPECT_FLOAT_EQ(commands.face->milliseconds, 3000.0f);
			EXPECT_EQ(commands.face->cue, creature_face::Cue::Idle);
		}
		// The mind lets the face go by itself: the body does, once its time is up
		EXPECT_FALSE(commands.relaxFace);
		body.Obey(commands, 100);
	}
	// Pulled as the wait starts and as the yawn starts, then again 42 turns into the long yawn, and not every turn
	ASSERT_EQ(pulled.size(), 3u);
	EXPECT_EQ(pulled[0], 0);
	EXPECT_EQ(pulled[1], 12);
	EXPECT_EQ(pulled[2], 12 + 42);
}

TEST(CreatureIdleMind, AgendasPullTheFacesOfWhatTheyDo)
{
	using creature_face::Cue;
	const auto sleep = creature_mind::Sleep(Always(0));
	ASSERT_EQ(sleep.size(), 3u);
	EXPECT_EQ(sleep[0].face, Cue::Grimace);
	// Asleep with its eyes closed it pulls no face
	EXPECT_EQ(sleep[1].face, Cue::None);
	EXPECT_EQ(sleep[2].face, Cue::Grimace);
	EXPECT_EQ(creature_mind::Hurl(1, {}, Always(1)).back().face, Cue::Anger);
	EXPECT_EQ(creature_mind::DestroyThing(1).back().face, Cue::Anger);
	EXPECT_EQ(creature_mind::RunFrom({}).back().face, Cue::Fear);
	EXPECT_EQ(creature_mind::ExamineByPickingUp(1, Always(0)).back().face, Cue::Curiosity);
	EXPECT_EQ(creature_mind::ThrowAbout(1, Always(0)).back().face, Cue::Playfulness);
	EXPECT_EQ(creature_mind::Poo(Always(1)).back().face, Cue::Smile);
	EXPECT_EQ(creature_mind::Puke().back().face, Cue::Grimace);
	EXPECT_EQ(creature_mind::Emote(animations::k_Happy).back().face, Cue::AttitudeToPlayer);
	EXPECT_EQ(creature_mind::HangAround(Always(0)).front().face, Cue::Idle);
	EXPECT_EQ(creature_mind::LookAt({}, 1.0f).back().face, Cue::Amazed);
	EXPECT_EQ(creature_mind::LookAtCamera().back().face, Cue::Curiosity);
	EXPECT_EQ(creature_mind::PutDownHeld().back().face, Cue::None);
	EXPECT_EQ(creature_mind::Faint().back().face, Cue::None);
}

TEST(CreatureIdleMind, LookingAtTheCameraTurnsOnlyWhenMoreThanARightAngleOffForFiveWholeSeconds)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::None, creature_mind::LookAtCamera());
	const glm::vec2 ahead = creature_locomotion::DirectionOf(0.0f) * 50.0f;
	creature_mind::Senses senses {
	    .seconds = 0.1f, .turnsPerSecond = 10, .camera = -ahead, .cameraDistance = 50.0f, .hasPlayer = true, .heading = 0.0f};
	// The first turn starts it; behind it, the camera is turned to every turn
	EXPECT_FALSE(creature_mind::Think(mind, senses, Always(0)).move.has_value());
	const auto behind = creature_mind::Think(mind, senses, Always(0));
	ASSERT_TRUE(behind.move.has_value());
	EXPECT_EQ(behind.move->kind, creature_mind::Movement::Kind::TurnToFace);
	// within a right angle, not
	senses.camera = ahead;
	EXPECT_FALSE(creature_mind::Think(mind, senses, Always(0)).move.has_value());
	// Done once more than five whole seconds are past
	for (int turn = 3; turn < 60; ++turn)
	{
		std::ignore = creature_mind::Think(mind, senses, Always(0));
		ASSERT_EQ(mind.step, 0u);
	}
	std::ignore = creature_mind::Think(mind, senses, Always(0));
	EXPECT_EQ(mind.step, 1u);
	// and at once with the camera farther than a hundred metres through the air, however near along the ground
	creature_mind::Plan(mind, creature_mind::Activity::None, creature_mind::LookAtCamera());
	senses.cameraDistance = 100.5f;
	std::ignore = creature_mind::Think(mind, senses, Always(0));
	std::ignore = creature_mind::Think(mind, senses, Always(0));
	EXPECT_EQ(mind.step, 1u);
	// but not at a hundred
	creature_mind::Plan(mind, creature_mind::Activity::None, creature_mind::LookAtCamera());
	senses.cameraDistance = 100.0f;
	std::ignore = creature_mind::Think(mind, senses, Always(0));
	std::ignore = creature_mind::Think(mind, senses, Always(0));
	EXPECT_EQ(mind.step, 0u);
	// The player's hand still for fifty turns (five seconds at ten a second) lets it go on, for fifty-one ends it
	senses.handStillTurns = 50;
	std::ignore = creature_mind::Think(mind, senses, Always(0));
	EXPECT_EQ(mind.step, 0u);
	senses.handStillTurns = 51;
	std::ignore = creature_mind::Think(mind, senses, Always(0));
	EXPECT_EQ(mind.step, 1u);
}

// A creature without a player (the glade's) is done looking at the camera on its first turn after the start, without
// turning; one with a player, right on the camera along the ground, is done too
TEST(CreatureIdleMind, LookingAtTheCameraIsOverAtOnceWithoutAPlayer)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::None, creature_mind::LookAtCamera());
	const glm::vec2 behind = creature_locomotion::DirectionOf(0.0f) * -50.0f;
	creature_mind::Senses senses {
	    .seconds = 0.1f, .turnsPerSecond = 10, .camera = behind, .cameraDistance = 50.0f, .heading = 0.0f};
	std::ignore = creature_mind::Think(mind, senses, Always(0));
	EXPECT_EQ(mind.step, 0u);
	EXPECT_FALSE(creature_mind::Think(mind, senses, Always(0)).move.has_value());
	EXPECT_EQ(mind.step, 1u);

	creature_mind::Plan(mind, creature_mind::Activity::None, creature_mind::LookAtCamera());
	senses.hasPlayer = true;
	senses.camera = glm::vec2(0.05f, 0.0f);
	std::ignore = creature_mind::Think(mind, senses, Always(0));
	EXPECT_FALSE(creature_mind::Think(mind, senses, Always(0)).move.has_value());
	EXPECT_EQ(mind.step, 1u);
}

// In a cut scene the creature watches the camera for 21 turns, its first turn one of them, turning whenever the camera
// is more than a right angle off; neither the camera's distance nor a still hand ends it
TEST(CreatureIdleMind, WatchingTheCameraInACutSceneLastsTwentyOneTurnsWhateverTheCameraDoes)
{
	const creature_mind::Step watch {.kind = creature_mind::Step::Kind::LookAtCamera, .turns = 20};
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::None, {watch});
	const glm::vec2 ahead = creature_locomotion::DirectionOf(0.0f) * 50.0f;
	creature_mind::Senses senses {.seconds = 0.1f,
	                              .turnsPerSecond = 10,
	                              .camera = -ahead,
	                              .cameraDistance = 150.0f,
	                              .hasPlayer = true,
	                              .handStillTurns = 1000,
	                              .heading = 0.0f};
	// Its first turn turns it already
	const auto first = creature_mind::Think(mind, senses, Always(0));
	ASSERT_TRUE(first.move.has_value());
	EXPECT_EQ(first.move->kind, creature_mind::Movement::Kind::TurnToFace);
	EXPECT_EQ(mind.lookTurns, 1u);
	for (int turn = 2; turn <= 20; ++turn)
	{
		std::ignore = creature_mind::Think(mind, senses, Always(0));
		EXPECT_EQ(mind.step, 0u) << turn;
	}
	// Ahead of it there is no turn
	senses.camera = ahead;
	EXPECT_FALSE(creature_mind::Think(mind, senses, Always(0)).move.has_value());
	EXPECT_EQ(mind.step, 1u);

	// Right on the camera along the ground it neither turns nor stops
	creature_mind::Plan(mind, creature_mind::Activity::None, {watch});
	senses.camera = glm::vec2(0.05f, 0.0f);
	EXPECT_FALSE(creature_mind::Think(mind, senses, Always(0)).move.has_value());
	EXPECT_FALSE(creature_mind::Think(mind, senses, Always(0)).move.has_value());
	EXPECT_EQ(mind.step, 0u);
}

// Without a player the cut scene's watching is over on its first turn, without turning
TEST(CreatureIdleMind, WatchingTheCameraInACutSceneIsOverAtOnceWithoutAPlayer)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::None, {{.kind = creature_mind::Step::Kind::LookAtCamera, .turns = 20}});
	const glm::vec2 behind = creature_locomotion::DirectionOf(0.0f) * -50.0f;
	const creature_mind::Senses senses {.seconds = 0.1f, .turnsPerSecond = 10, .camera = behind, .heading = 0.0f};
	EXPECT_FALSE(creature_mind::Think(mind, senses, Always(0)).move.has_value());
	EXPECT_EQ(mind.step, 1u);
}

// Talking plays on top of the body as it starts, and is done once nothing plays there
TEST(CreatureIdleMind, AnOverlayIsDoneOnceNothingPlaysOnTopOfTheBody)
{
	namespace animations = openblack::creature_layers::animations;
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::None,
	                    {{.kind = creature_mind::Step::Kind::Overlay, .animation = animations::k_Talk}});
	creature_mind::Senses senses {.seconds = 0.1f, .turnsPerSecond = 10};
	EXPECT_EQ(creature_mind::Think(mind, senses, Always(0)).playOverlay, animations::k_Talk);
	senses.overlayPlaying = true;
	EXPECT_FALSE(creature_mind::Think(mind, senses, Always(0)).playOverlay.has_value());
	EXPECT_EQ(mind.step, 0u);
	senses.overlayPlaying = false;
	std::ignore = creature_mind::Think(mind, senses, Always(0));
	EXPECT_EQ(mind.step, 1u);
}

TEST(CreatureIdleMind, EachFacePulledMovesTheVarietyOn)
{
	creature_mind::IdleMind mind;
	const creature_mind::Senses senses {};
	const auto first = creature_mind::PullFace(mind, creature_face::Cue::Fear, senses, Always(3));
	ASSERT_TRUE(first.has_value());
	EXPECT_EQ(first->face, creature_face::Face::Scared);
	EXPECT_EQ(mind.faceVariety, 3u);
	const auto second = creature_mind::PullFace(mind, creature_face::Cue::Fear, senses, Always(3));
	ASSERT_TRUE(second.has_value());
	EXPECT_EQ(second->face, creature_face::Face::Ooh);
	EXPECT_EQ(mind.faceVariety, 6u);
	// A reason that pulls no face leaves it be
	EXPECT_FALSE(creature_mind::PullFace(mind, creature_face::Cue::None, senses, Always(3)).has_value());
	EXPECT_EQ(mind.faceVariety, 6u);
}

TEST(CreatureIdleMind, NothingStartsWhileTheBodyIsBusy)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Told,
	                    {{.kind = creature_mind::Step::Kind::Action, .seconds = 0.0f, .animation = 60, .sleepyEyes = false}});
	auto senses = FakeBody {}.Senses();
	senses.bodyBusy = true;
	for (int i = 0; i < 5; ++i)
	{
		EXPECT_FALSE(creature_mind::Think(mind, senses, Always(0)).playOnce.has_value());
	}
	senses.bodyBusy = false;
	EXPECT_EQ(creature_mind::Think(mind, senses, Always(0)).playOnce, 60u);
}

TEST(CreatureIdleMind, AScriptsFocusTurnsItOnlyWhenItFacesMoreThanAnEighthOfARightAngleAway)
{
	uint32_t draws = 0;
	const creature_mind::Random counting = [&draws](uint32_t) {
		++draws;
		return 0u;
	};
	auto senses = FakeBody {}.Senses();
	senses.position = {0.0f, 0.0f};
	senses.choosesOwnActivity = false;
	// facing the point to within 22.5 degrees each way, it does not turn, and is done on the next turn
	const glm::vec2 ahead {std::sin(0.3f) * 10.0f, std::cos(0.3f) * 10.0f};
	senses.heading = creature_locomotion::HeadingOf(ahead) - 0.3f;
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Told, creature_mind::TurnToFacePoint(ahead));
	EXPECT_FALSE(creature_mind::Think(mind, senses, counting).move.has_value());
	EXPECT_EQ(mind.step, 0u);
	static_cast<void>(creature_mind::Think(mind, senses, counting));
	EXPECT_EQ(mind.step, 1u);
	// further off, it turns to it, and is done once the turn is over
	senses.heading = creature_locomotion::HeadingOf(ahead) - 0.5f;
	creature_mind::Plan(mind, creature_mind::Activity::Told, creature_mind::TurnToFacePoint(ahead));
	const auto turn = creature_mind::Think(mind, senses, counting);
	ASSERT_TRUE(turn.move.has_value());
	EXPECT_EQ(turn.move->kind, creature_mind::Movement::Kind::TurnToFace);
	EXPECT_EQ(turn.move->point, ahead);
	senses.moving = true;
	static_cast<void>(creature_mind::Think(mind, senses, counting));
	EXPECT_EQ(mind.step, 0u);
	senses.moving = false;
	static_cast<void>(creature_mind::Think(mind, senses, counting));
	EXPECT_EQ(mind.step, 1u);
	// right on the point it has nothing to turn to
	creature_mind::Plan(mind, creature_mind::Activity::Told, creature_mind::TurnToFacePoint(senses.position));
	EXPECT_FALSE(creature_mind::Think(mind, senses, counting).move.has_value());
	// and it pulls no face, so nothing is drawn
	EXPECT_EQ(draws, 0u);
}

TEST(CreatureIdleMind, ASitCountsItsTimeFromTheTurnAfterItStartsToTheTurnItIsAskedToEnd)
{
	FakeBody body;
	auto senses = body.Senses();
	senses.choosesOwnActivity = false;
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Told, {creature_mind::SitDown(Always(0))});
	const auto& sit = mind.agenda.front();
	ASSERT_EQ(sit.sequence[1], animations::k_Sit);
	const auto turns = static_cast<int>(std::lround(sit.seconds / k_Turn));
	// not yet started, then the turn it starts: not counting
	EXPECT_FALSE(creature_mind::SitCounting(mind));
	body.Obey(creature_mind::Think(mind, senses, Always(0)), 3);
	// from the next turn on, while it sits down and sits, until the turn its end is asked for
	int counted = 0;
	for (int turn = 0; turn < turns + 5 && mind.step == 0; ++turn)
	{
		const bool counting = creature_mind::SitCounting(mind);
		counted += counting ? 1 : 0;
		const auto commands = creature_mind::Think(mind, body.Senses(), Always(0));
		if (commands.endSit)
		{
			EXPECT_TRUE(counting);
			EXPECT_FALSE(creature_mind::SitCounting(mind));
		}
		body.Obey(commands, 3);
	}
	EXPECT_EQ(counted, turns);
	// a static step of another loop never counts as a sit
	creature_mind::Plan(mind, creature_mind::Activity::Told,
	                    {{.kind = creature_mind::Step::Kind::Static, .seconds = 1.0f, .sequence = {28, 29, 30}}});
	body.Obey(creature_mind::Think(mind, body.Senses(), Always(0)), 3);
	EXPECT_FALSE(creature_mind::SitCounting(mind));
}

TEST(CreatureIdleMind, FacingDownTheSlopeIsDoneOnceTheBodyIsFree)
{
	auto senses = FakeBody {}.Senses();
	senses.choosesOwnActivity = false;
	creature_mind::IdleMind mind;
	creature_mind::Plan(
	    mind, creature_mind::Activity::Told,
	    {{.kind = creature_mind::Step::Kind::Move, .movement = {.kind = creature_mind::Movement::Kind::FaceDownSlope}}});
	const auto start = creature_mind::Think(mind, senses, Always(0));
	ASSERT_TRUE(start.move.has_value());
	EXPECT_EQ(start.move->kind, creature_mind::Movement::Kind::FaceDownSlope);
	// turning, it is not done
	senses.moving = true;
	senses.bodyBusy = true;
	static_cast<void>(creature_mind::Think(mind, senses, Always(0)));
	EXPECT_EQ(mind.step, 0u);
	// the turn over, an action still playing keeps it going
	senses.moving = false;
	static_cast<void>(creature_mind::Think(mind, senses, Always(0)));
	EXPECT_EQ(mind.step, 0u);
	// the body free, it is done
	senses.bodyBusy = false;
	static_cast<void>(creature_mind::Think(mind, senses, Always(0)));
	EXPECT_EQ(mind.step, 1u);
}

TEST(CreatureIdleMind, WithNoPlanItChoosesNothingOfItsOwn)
{
	// The game's creature stands as it is when its agenda runs out; only openblack's former idle policy chose
	creature_mind::IdleMind mind;
	auto senses = FakeBody {}.Senses();
	senses.choosesOwnActivity = false;
	uint32_t draws = 0;
	const creature_mind::Random counting = [&draws](uint32_t) {
		++draws;
		return 0u;
	};
	for (int i = 0; i < 5; ++i)
	{
		const auto commands = creature_mind::Think(mind, senses, counting);
		EXPECT_FALSE(commands.playOnce.has_value());
		EXPECT_FALSE(commands.move.has_value());
		EXPECT_FALSE(commands.face.has_value());
	}
	EXPECT_TRUE(mind.agenda.empty());
	EXPECT_EQ(draws, 0u);
	// With the former policy it does choose something
	senses.choosesOwnActivity = true;
	const auto chosen = creature_mind::Think(mind, senses, counting);
	static_cast<void>(chosen);
	EXPECT_FALSE(mind.agenda.empty());
}

TEST(CreatureIdleMind, SometimesItSitsForAWhile)
{
	creature_mind::IdleMind mind;
	FakeBody body;
	// Draws of 0 choose to sit, for ten seconds
	std::optional<int> stood;
	for (int turn = 0; turn < 300 && !stood; ++turn)
	{
		const auto commands = creature_mind::Think(mind, body.Senses(), Always(0));
		if (turn == 0)
		{
			EXPECT_EQ(mind.activity, creature_mind::Activity::Sit);
			EXPECT_TRUE(commands.startSequence);
		}
		if (commands.endSit)
		{
			stood = turn;
		}
		body.Obey(commands, 1);
	}
	EXPECT_EQ(body.sits, 1);
	ASSERT_TRUE(stood.has_value());
	EXPECT_EQ(*stood, 100);
}

TEST(CreatureIdleMind, ItShowsItsStrongestDesireOnceAMinute)
{
	creature_mind::IdleMind mind;
	FakeBody body;
	std::vector<int> shown;
	for (int turn = 0; turn < 1300; ++turn)
	{
		auto senses = body.Senses();
		senses.strongest = Desire::Hunger;
		const auto commands = creature_mind::Think(mind, senses, Always(1));
		if (commands.playOnce == animations::k_Hungry)
		{
			shown.push_back(turn);
		}
		body.Obey(commands, 20);
	}
	ASSERT_GE(shown.size(), 2u);
	EXPECT_EQ(shown[0], 0);
	// Not again until a minute has passed, at the next choice after it
	EXPECT_GE(shown[1], 600);
	EXPECT_LT(shown[1], 700);
	EXPECT_EQ(mind.shown, Desire::Hunger);
}

TEST(CreatureIdleMind, AStrokeOrASlapIsShownFirst)
{
	creature_mind::IdleMind mind;
	auto senses = FakeBody {}.Senses();
	senses.strongest = Desire::Hunger;
	senses.feedbackSeconds = 4.0f;
	senses.feedbackWasStroke = true;
	EXPECT_EQ(creature_mind::Think(mind, senses, Always(1)).playOnce, animations::k_Happy);

	creature_mind::IdleMind slapped;
	senses.feedbackWasStroke = false;
	EXPECT_EQ(creature_mind::Think(slapped, senses, Always(1)).playOnce, animations::k_Sad);

	// Long after, the desire is shown instead
	creature_mind::IdleMind later;
	senses.feedbackSeconds = 20.0f;
	EXPECT_EQ(creature_mind::Think(later, senses, Always(1)).playOnce, animations::k_Hungry);
}

TEST(CreatureIdleMind, AnAbandonedSitEnds)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::BeIdle,
	                    {{.kind = creature_mind::Step::Kind::Wait, .seconds = 5.0f, .animation = 0, .sleepyEyes = false}});
	auto senses = FakeBody {}.Senses();
	senses.bodyBusy = true;
	senses.bodyLooping = true;
	EXPECT_TRUE(creature_mind::Think(mind, senses, Always(1)).endSit);
}

TEST(CreatureIdleMind, HangingAroundWalksSomewhereNearbyThenSits)
{
	creature_mind::IdleMind mind;
	// Draws of 5: the last lot, which is hanging around, 5 degrees round and 25 units away
	const auto random = Always(5);
	auto senses = FakeBody {}.Senses();
	senses.position = {100.0f, 200.0f};
	const auto first = creature_mind::Think(mind, senses, random);
	EXPECT_EQ(mind.activity, creature_mind::Activity::HangAround);
	ASSERT_TRUE(first.move.has_value());
	EXPECT_EQ(first.move->kind, creature_mind::Movement::Kind::ToPoint);
	const auto angle = 5.0f * std::numbers::pi_v<float> / 180.0f;
	EXPECT_NEAR(first.move->point.x, 100.0f + (25.0f * std::cos(angle)), k_Tolerance);
	EXPECT_NEAR(first.move->point.y, 200.0f + (25.0f * std::sin(angle)), k_Tolerance);

	// On its way, it keeps going
	senses.moving = true;
	senses.bodyBusy = true;
	for (int i = 0; i < 10; ++i)
	{
		const auto commands = creature_mind::Think(mind, senses, random);
		EXPECT_FALSE(commands.move.has_value());
		EXPECT_FALSE(commands.startSequence);
		EXPECT_EQ(mind.step, 0u);
	}
	// Arrived, it sits down
	senses.moving = false;
	senses.bodyBusy = false;
	EXPECT_FALSE(creature_mind::Think(mind, senses, random).startSequence);
	EXPECT_EQ(mind.step, 1u);
	EXPECT_TRUE(creature_mind::Think(mind, senses, random).startSequence);
}

TEST(CreatureIdleMind, AFollowStepStopsWhenItsTimeIsUp)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Told,
	                    {{.kind = creature_mind::Step::Kind::Move,
	                      .seconds = 1.0f,
	                      .animation = 0,
	                      .sleepyEyes = false,
	                      .movement = {.kind = creature_mind::Movement::Kind::Follow,
	                                   .point = {0.0f, 0.0f},
	                                   .object = 7u,
	                                   .run = false,
	                                   .minDistance = 0.0f,
	                                   .maxDistance = 10.0f}}});
	auto senses = FakeBody {}.Senses();
	const auto first = creature_mind::Think(mind, senses, Always(0));
	ASSERT_TRUE(first.move.has_value());
	EXPECT_EQ(first.move->object, 7u);
	senses.moving = true;
	senses.bodyBusy = true;
	int stoppedAt = -1;
	for (int turn = 1; turn < 30 && stoppedAt < 0; ++turn)
	{
		if (creature_mind::Think(mind, senses, Always(0)).stopMoving)
		{
			stoppedAt = turn;
		}
	}
	EXPECT_GE(stoppedAt, 10);
	EXPECT_LE(stoppedAt, 11);
}

TEST(CreatureNeedsMind, TheStrongestNeedWithTheMeansAtHandIsSeenTo)
{
	creature_mind::Wants wants {.hunger = 0.9f, .tiredness = 0.5f, .poo = 0.4f, .water = 0.6f};
	// Hungry with nothing to eat and thirsty with no water, it sleeps
	auto plan = creature_mind::ChooseNeed(wants, Always(1));
	ASSERT_TRUE(plan.has_value());
	EXPECT_EQ(plan->activity, creature_mind::Activity::Sleep);
	wants.waterSpot = {.shore = glm::vec2(10.0f, 20.0f), .water = glm::vec2(10.0f, 30.0f)};
	plan = creature_mind::ChooseNeed(wants, Always(1));
	ASSERT_TRUE(plan.has_value());
	EXPECT_EQ(plan->activity, creature_mind::Activity::Drink);
	wants.food = 42u;
	plan = creature_mind::ChooseNeed(wants, Always(1));
	ASSERT_TRUE(plan.has_value());
	EXPECT_EQ(plan->activity, creature_mind::Activity::Eat);
	// Nothing pressing, nothing to see to
	EXPECT_FALSE(creature_mind::ChooseNeed({.hunger = 0.2f, .tiredness = 0.1f, .poo = 0.29f, .water = 0.0f}, Always(1)));
}

TEST(CreatureNeedsMind, ANeedComesBeforeIdling)
{
	creature_mind::IdleMind mind;
	auto senses = FakeBody {}.Senses();
	senses.wants = {.hunger = 0.0f, .tiredness = 0.0f, .poo = 0.8f, .water = 0.0f};
	creature_mind::ChooseNext(mind, senses, Always(1));
	EXPECT_EQ(mind.activity, creature_mind::Activity::Poo);
}

TEST(CreatureNeedsMind, ItSleepsWithItsEyesClosedUntilRested)
{
	creature_mind::IdleMind mind;
	// Draws of 1: no yawn first
	creature_mind::Plan(mind, creature_mind::Activity::Sleep, creature_mind::Sleep(Always(1)));
	ASSERT_EQ(mind.agenda.size(), 2u);
	FakeBody body;
	auto commands = creature_mind::Think(mind, body.Senses(), Always(1));
	ASSERT_TRUE(commands.startSequence.has_value());
	EXPECT_EQ((*commands.startSequence)[1], animations::k_Sleep);
	EXPECT_EQ(commands.eyes, creature_mind::Eyes::Closed);
	EXPECT_TRUE(creature_mind::IsAsleep(mind));
	body.Obey(commands, 1);
	// However long it sleeps, it sleeps on until rested
	for (int turn = 0; turn < 1000; ++turn)
	{
		commands = creature_mind::Think(mind, body.Senses(), Always(1));
		EXPECT_FALSE(commands.endSit);
		EXPECT_FALSE(commands.lookAbout);
		body.Obey(commands, 1);
	}
	auto senses = body.Senses();
	senses.rested = true;
	commands = creature_mind::Think(mind, senses, Always(1));
	EXPECT_TRUE(commands.endSit);
	EXPECT_EQ(commands.effect, creature_mind::Effect::Slept);
	body.Obey(commands, 1);
	commands = creature_mind::Think(mind, body.Senses(), Always(1));
	EXPECT_EQ(commands.eyes, creature_mind::Eyes::Normal);
	EXPECT_FALSE(creature_mind::IsAsleep(mind));
	// Then a dazed look about with sleepy eyes
	commands = creature_mind::Think(mind, body.Senses(), Always(1));
	EXPECT_EQ(commands.playOnce, animations::k_Confused);
	EXPECT_EQ(commands.eyes, creature_mind::Eyes::Sleepy);
}

TEST(CreatureNeedsMind, APooTakesFourSecondsAndDropsAsItEnds)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Poo, creature_mind::Poo(Always(1)));
	ASSERT_EQ(mind.agenda.size(), 1u);
	FakeBody body;
	std::optional<int> dropped;
	for (int turn = 0; turn < 100 && !dropped; ++turn)
	{
		const auto commands = creature_mind::Think(mind, body.Senses(), Always(1));
		if (turn == 0)
		{
			ASSERT_TRUE(commands.startSequence.has_value());
			EXPECT_EQ(*commands.startSequence,
			          (std::array<size_t, 3> {animations::k_StartPoo, animations::k_Poo, animations::k_EndPoo}));
		}
		if (commands.effect == creature_mind::Effect::Poo)
		{
			EXPECT_TRUE(commands.endSit);
			dropped = turn;
		}
		body.Obey(commands, 1);
	}
	// Four seconds of tenths, give or take a turn's rounding
	ASSERT_TRUE(dropped.has_value());
	EXPECT_GE(*dropped, 40);
	EXPECT_LE(*dropped, 41);
	// A third of the time it shows it needs one first
	EXPECT_EQ(creature_mind::Poo(Always(0)).front().animation, animations::k_NeedAPoo);
}

TEST(CreatureNeedsMind, ItPicksFoodUpAndEatsIt)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Eat, creature_mind::Eat(7u));
	FakeBody body;
	auto senses = body.Senses();
	// Its hands go and pick the food up
	auto commands = creature_mind::Think(mind, senses, Always(1));
	ASSERT_TRUE(commands.object.has_value());
	EXPECT_EQ(commands.object->kind, creature_mind::ObjectOrder::Kind::PickUp);
	EXPECT_EQ(commands.object->object, 7u);
	senses.hands = creature_mind::HandsState::Busy;
	EXPECT_EQ(creature_mind::Think(mind, senses, Always(1)).effect, creature_mind::Effect::None);
	// Having it, it examines it, then eats it; eaten, the desire is satisfied
	senses.hands = creature_mind::HandsState::Done;
	static_cast<void>(creature_mind::Think(mind, senses, Always(1)));
	commands = creature_mind::Think(mind, senses, Always(1));
	ASSERT_TRUE(commands.object.has_value());
	EXPECT_EQ(commands.object->kind, creature_mind::ObjectOrder::Kind::Keep);
	static_cast<void>(creature_mind::Think(mind, senses, Always(1)));
	commands = creature_mind::Think(mind, senses, Always(1));
	ASSERT_TRUE(commands.object.has_value());
	EXPECT_EQ(commands.object->kind, creature_mind::ObjectOrder::Kind::Eat);
	commands = creature_mind::Think(mind, senses, Always(1));
	EXPECT_EQ(commands.effect, creature_mind::Effect::Eat);
}

TEST(CreatureNeedsMind, FoodItCantPickUpIsGivenUpUneaten)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Eat, creature_mind::Eat(7u));
	FakeBody body;
	auto senses = body.Senses();
	static_cast<void>(creature_mind::Think(mind, senses, Always(1)));
	// Its hands couldn't get it: the rest of the agenda is no use, nothing is eaten, and the agenda was given up rather
	// than carried out, so the hunger it served is still wanted
	senses.hands = creature_mind::HandsState::Failed;
	EXPECT_EQ(creature_mind::Think(mind, senses, Always(1)).effect, creature_mind::Effect::None);
	EXPECT_GE(mind.step, mind.agenda.size());
	EXPECT_TRUE(mind.gaveUp);
	// The next agenda starts afresh
	creature_mind::Plan(mind, creature_mind::Activity::Eat, creature_mind::Eat(8u));
	EXPECT_FALSE(mind.gaveUp);
}

TEST(CreatureNeedsMind, ItDrinksAtTheWatersEdge)
{
	const auto agenda = creature_mind::Drink({100.0f, 200.0f}, {100.0f, 230.0f});
	ASSERT_EQ(agenda.size(), 3u);
	EXPECT_EQ(agenda[0].movement.kind, creature_mind::Movement::Kind::ToPoint);
	EXPECT_EQ(agenda[0].movement.point, glm::vec2(100.0f, 200.0f));
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, creature_mind::k_DrinkReach);
	EXPECT_EQ(agenda[1].movement.kind, creature_mind::Movement::Kind::TurnToFace);
	EXPECT_EQ(agenda[1].movement.point, glm::vec2(100.0f, 230.0f));
	EXPECT_EQ(agenda[2].animation, animations::k_Drink);
	EXPECT_EQ(agenda[2].effect, creature_mind::Effect::Drink);
}

TEST(CreatureNeedsMind, FaintedItLiesStillThenComesRound)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Faint, creature_mind::Faint());
	FakeBody body;
	auto commands = creature_mind::Think(mind, body.Senses(), Always(1));
	ASSERT_TRUE(commands.startSequence.has_value());
	EXPECT_TRUE(commands.holdLoop);
	EXPECT_EQ((*commands.startSequence)[0], animations::k_Faint);
	EXPECT_EQ((*commands.startSequence)[2], animations::k_GetUp);
	EXPECT_TRUE(creature_mind::IsUnconscious(mind));
	body.Obey(commands, 1);
	std::optional<int> cameRound;
	for (int turn = 1; turn < 200 && !cameRound; ++turn)
	{
		commands = creature_mind::Think(mind, body.Senses(), Always(1));
		if (commands.effect == creature_mind::Effect::CameRound)
		{
			cameRound = turn;
		}
		body.Obey(commands, 1);
	}
	ASSERT_TRUE(cameRound.has_value());
	EXPECT_EQ(*cameRound, static_cast<int>(creature_mind::k_FaintSeconds * k_TurnsPerSecond));
}

namespace
{
/// One game turn a second, so that a turn watched is a second watched
constexpr float k_LookTurnsPerSecond = 1.0f;

Viewer AtOrigin(float size = 1.0f)
{
	return {.position = {0.0f, 0.0f, 0.0f}, .ahead = {0.0f, 0.0f, -1.0f}, .size = size};
}
} // namespace

TEST(CreatureLook, DovesAndTheCitadelCatchTheEyeMostAndFixedThingsLeast)
{
	EXPECT_GT(InterestOf(Interest::Dove), InterestOf(Interest::Creature));
	EXPECT_GT(InterestOf(Interest::Creature), InterestOf(Interest::Animal));
	EXPECT_GT(InterestOf(Interest::Animal), InterestOf(Interest::Villager));
	EXPECT_GT(InterestOf(Interest::Villager), InterestOf(Interest::Abode));
	EXPECT_GT(InterestOf(Interest::Abode), InterestOf(Interest::Tree));
	EXPECT_GT(InterestOf(Interest::Tree), InterestOf(Interest::Fixed));
	EXPECT_FLOAT_EQ(InterestOf(Interest::Dove), InterestOf(Interest::Citadel));
}

TEST(CreatureLook, BiggerCreaturesSeeFurther)
{
	// An odd number of cells of ten metres each: the root of 160 is 12.6, six cells either side of its own
	EXPECT_FLOAT_EQ(LookRange(0.0f), 130.0f);
	EXPECT_GT(LookRange(2.0f), LookRange(0.0f));
	EXPECT_GE(LookRange(4.0f), LookRange(2.0f));
	// A size below zero leaves a range, rather than the root of a negative number
	EXPECT_GT(LookRange(-10.0f), 0.0f);
}

TEST(CreatureLook, ThingsWithinHalfTheRangeAreAsInterestingAsTheyGet)
{
	EXPECT_FLOAT_EQ(DistanceFactor(0.0f, 100.0f), 1.0f);
	EXPECT_FLOAT_EQ(DistanceFactor(49.0f, 100.0f), 1.0f);
	EXPECT_FLOAT_EQ(DistanceFactor(50.0f, 100.0f), 0.5f);
	EXPECT_FLOAT_EQ(DistanceFactor(100.0f, 100.0f), 0.0f);
	EXPECT_FLOAT_EQ(DistanceFactor(10.0f, 0.0f), 0.0f);
}

TEST(CreatureLook, CreaturesSeeAheadOfThemAndWhateverIsOnTopOfThem)
{
	const auto viewer = AtOrigin();
	EXPECT_TRUE(CanSee(viewer, {0.0f, 0.0f, -50.0f}));
	// Anything within a cell is seen, even behind
	EXPECT_TRUE(CanSee(viewer, {0.0f, 0.0f, 5.0f}));
	// Behind, and beyond the range
	EXPECT_FALSE(CanSee(viewer, {0.0f, 0.0f, 50.0f}));
	EXPECT_FALSE(CanSee(viewer, {0.0f, 0.0f, -10000.0f}));
	// A creature facing nowhere sees nothing it isn't standing on
	const Viewer lost {.position = {0.0f, 0.0f, 0.0f}, .ahead = {0.0f, 0.0f, 0.0f}, .size = 1.0f};
	EXPECT_FALSE(CanSee(lost, {0.0f, 0.0f, -50.0f}));
	EXPECT_TRUE(CanSee(lost, {0.0f, 0.0f, -5.0f}));
}

TEST(CreatureLook, TheNearerAndTheMoreInterestingWins)
{
	const auto viewer = AtOrigin();
	const std::array<Candidate, 2> candidates {
	    Candidate {.id = 1, .kind = Interest::Tree, .point = {0.0f, 0.0f, -20.0f}},
	    Candidate {.id = 2, .kind = Interest::Villager, .point = {0.0f, 0.0f, -20.0f}},
	};
	const auto target = LookAbout({}, candidates, viewer, k_LookTurnsPerSecond);
	EXPECT_EQ(target.id, 2u);
	EXPECT_EQ(target.kind, Interest::Villager);
	EXPECT_EQ(target.watchedTurns, 0u);

	// The same thing further off catches the eye less
	const std::array<Candidate, 1> near {Candidate {.id = 3, .kind = Interest::Tree, .point = {0.0f, 0.0f, -20.0f}}};
	const std::array<Candidate, 1> far {Candidate {.id = 3, .kind = Interest::Tree, .point = {0.0f, 0.0f, -120.0f}}};
	EXPECT_GT(Priority(viewer, near[0].kind, near[0].point), Priority(viewer, far[0].kind, far[0].point));
}

TEST(CreatureLook, WhatIsWatchedIsWatchedALittleLongerEachTurn)
{
	const auto viewer = AtOrigin();
	const std::array<Candidate, 1> candidates {Candidate {.id = 7, .kind = Interest::Tree, .point = {0.0f, 0.0f, -20.0f}}};
	auto target = LookAbout({}, candidates, viewer, k_LookTurnsPerSecond);
	ASSERT_EQ(target.id, 7u);
	for (uint32_t turn = 1; turn <= 5; ++turn)
	{
		target = LookAbout(target, candidates, viewer, k_LookTurnsPerSecond);
		EXPECT_EQ(target.watchedTurns, turn);
		EXPECT_EQ(target.id, 7u);
	}
	// It moves: the creature follows it as long as it stays in sight
	const std::array<Candidate, 1> moved {Candidate {.id = 7, .kind = Interest::Tree, .point = {10.0f, 0.0f, -30.0f}}};
	target = LookAbout(target, moved, viewer, k_LookTurnsPerSecond);
	EXPECT_EQ(target.id, 7u);
	EXPECT_NEAR(target.point.x, 10.0f, k_Tolerance);
}

TEST(CreatureLook, WhatGoesOutOfSightIsDropped)
{
	const auto viewer = AtOrigin();
	const std::array<Candidate, 1> candidates {Candidate {.id = 7, .kind = Interest::Tree, .point = {0.0f, 0.0f, -20.0f}}};
	auto target = LookAbout({}, candidates, viewer, k_LookTurnsPerSecond);
	ASSERT_EQ(target.id, 7u);
	// Gone from the candidates: it is no longer there
	target = LookAbout(target, std::span<const Candidate> {}, viewer, k_LookTurnsPerSecond);
	EXPECT_FALSE(target.id.has_value());
	// Still there, but behind the creature now
	target = LookAbout({}, candidates, viewer, k_LookTurnsPerSecond);
	ASSERT_EQ(target.id, 7u);
	const std::array<Candidate, 1> behind {Candidate {.id = 7, .kind = Interest::Tree, .point = {0.0f, 0.0f, 60.0f}}};
	target = LookAbout(target, behind, viewer, k_LookTurnsPerSecond);
	EXPECT_FALSE(target.id.has_value());
}

TEST(CreatureLook, BoredomLetsSomethingLessInterestingTakeOver)
{
	const auto viewer = AtOrigin();
	const std::array<Candidate, 1> villager {Candidate {.id = 1, .kind = Interest::Villager, .point = {0.0f, 0.0f, -20.0f}}};
	auto target = LookAbout({}, villager, viewer, k_LookTurnsPerSecond);
	ASSERT_EQ(target.id, 1u);
	// A tree does not catch its eye while the villager is fresh
	const std::array<Candidate, 2> both {
	    villager[0],
	    Candidate {.id = 2, .kind = Interest::Tree, .point = {0.0f, 0.0f, -20.0f}},
	};
	target = LookAbout(target, both, viewer, k_LookTurnsPerSecond);
	EXPECT_EQ(target.id, 1u);
	// Twenty seconds of watching, and nothing else catches its eye at all
	target.watchedTurns = static_cast<uint32_t>(k_BoredSeconds);
	target = LookAbout(target, both, viewer, k_LookTurnsPerSecond);
	EXPECT_EQ(target.id, 1u);
}

TEST(CreatureLook, WithNothingToLookAtItLooksAhead)
{
	const auto point = PointAhead(AtOrigin(2.0f));
	EXPECT_NEAR(point.x, 0.0f, k_Tolerance);
	EXPECT_NEAR(point.y, k_HeadHeight * 2.0f, k_Tolerance);
	EXPECT_NEAR(point.z, -50.0f, k_Tolerance);
	// Facing nowhere, it looks along -z
	const Viewer lost {.position = {1.0f, 0.0f, 2.0f}, .ahead = {0.0f, 1.0f, 0.0f}, .size = 1.0f};
	const auto fallback = PointAhead(lost);
	EXPECT_NEAR(fallback.x, 1.0f, k_Tolerance);
	EXPECT_NEAR(fallback.z, 2.0f - 50.0f, k_Tolerance);
}
