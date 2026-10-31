/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Which hand states draw the object the hand holds, and what stops them; when the hand's draw preparation stops
// after its state change; and the hand's distance zoomer, which a stopped preparation does not move

#include <cstdint>

#include <array>
#include <bit>
#include <optional>
#include <span>

#include <gtest/gtest.h>

#include "Common/Zoomer.h"
#include "ECS/Systems/Implementations/HandSystemDetail.h"

using namespace openblack::ecs::systems::hand_detail;

TEST(HandHeldDraw, OnlyTheTugHoldingMultiplePickUpAndGrainStatesDrawTheHeldObject)
{
	for (int32_t state = 0; state <= 25; ++state)
	{
		const bool draws = state == 3 || state == 4 || state == 6 || state == 8;
		EXPECT_EQ(DrawsHeldObject(state, true, false), draws) << "state " << state;
	}
}

TEST(HandHeldDraw, ABoxThatHasTheMouseHidesTheHeldObjectInEveryState)
{
	for (int32_t state = 0; state <= 25; ++state)
	{
		EXPECT_FALSE(DrawsHeldObject(state, true, true)) << "state " << state;
	}
}

TEST(HandHeldDraw, AnInactiveInterfaceHidesTheHeldObjectWithOrWithoutABox)
{
	for (const int32_t state : {3, 4, 6, 8})
	{
		EXPECT_FALSE(DrawsHeldObject(state, false, false)) << "state " << state;
		EXPECT_FALSE(DrawsHeldObject(state, false, true)) << "state " << state;
	}
}

TEST(HandDrawPreparation, RunsInEveryVisibleStateWithTheInterfaceActiveAndNoBox)
{
	for (int32_t state = 1; state <= 10; ++state)
	{
		EXPECT_FALSE(DrawPreparationStops(state, true, false)) << "state " << state;
	}
}

TEST(HandDrawPreparation, StopsInTheInvisibleStateWhateverTheInterface)
{
	EXPECT_TRUE(DrawPreparationStops(0, true, false));
	EXPECT_TRUE(DrawPreparationStops(0, true, true));
	EXPECT_TRUE(DrawPreparationStops(0, false, false));
}

TEST(HandDrawPreparation, ABoxThatHasTheMouseStopsItInEveryState)
{
	for (int32_t state = 0; state <= 10; ++state)
	{
		EXPECT_TRUE(DrawPreparationStops(state, true, true)) << "state " << state;
	}
}

TEST(HandDrawPreparation, AnInactiveInterfaceStopsItWithOrWithoutABox)
{
	for (int32_t state = 0; state <= 10; ++state)
	{
		EXPECT_TRUE(DrawPreparationStops(state, false, false)) << "state " << state;
		EXPECT_TRUE(DrawPreparationStops(state, false, true)) << "state " << state;
	}
}

namespace
{
constexpr float k_Frame = 1.0f / 30.0f;

void ExpectSameZoomer(const openblack::Zoomer& a, const openblack::Zoomer& b)
{
	EXPECT_EQ(a.value, b.value);
	EXPECT_EQ(a.destination, b.destination);
	EXPECT_EQ(a.speed, b.speed);
	EXPECT_EQ(a.time, b.time);
	EXPECT_EQ(a.duration, b.duration);
	EXPECT_EQ(a.startValue, b.startValue);
	EXPECT_EQ(a.startSpeed, b.startSpeed);
	EXPECT_EQ(a.c2, b.c2);
	EXPECT_EQ(a.c3, b.c3);
	EXPECT_EQ(a.c4, b.c4);
}
} // namespace

TEST(HandDistanceZoomer, TheFirstStartIsAtZeroAndHeldAtOne)
{
	// the zoomer's very first start is at 0, and a value below 1 is put at 1
	const auto distance = AdvanceHandDistance({}, 40.0f, 0.0f, false);
	EXPECT_TRUE(distance.started);
	EXPECT_EQ(distance.zoomer.value, 1.0f);
}

TEST(HandDistanceZoomer, TheNormalStatesEntryPutsItAtRestAtTheViewDistance)
{
	const auto moving = AdvanceHandDistance(AdvanceHandDistance({}, 40.0f, k_Frame, false), 40.0f, k_Frame, false);
	const auto entered = RestartHandDistance(moving, 17.5f);
	EXPECT_TRUE(entered.started);
	openblack::Zoomer atRest;
	atRest.SetPosition(17.5f);
	ExpectSameZoomer(entered.zoomer, atRest);
	// and it moves on from there, not from the target
	const auto next = AdvanceHandDistance(entered, 40.0f, 0.0f, false);
	EXPECT_EQ(next.zoomer.value, 17.5f);
}

TEST(HandDistanceZoomer, BeforeItsFirstMoveTheEntryLeavesTheStartAtZero)
{
	const auto entered = RestartHandDistance({}, 10.0f);
	EXPECT_FALSE(entered.started);
	EXPECT_EQ(entered.zoomer.value, 10.0f);
	// the first move starts it at 0, which the floor puts at 1
	const auto first = AdvanceHandDistance(entered, 40.0f, 0.0f, false);
	EXPECT_TRUE(first.started);
	EXPECT_EQ(first.zoomer.value, 1.0f);
}

TEST(HandDistanceZoomer, ComingCloserTakesATenthOfASecondAndMovingAwayTwentyEightHundredths)
{
	HandDistance distance {.started = true};
	distance.zoomer.SetPosition(20.0f);
	EXPECT_EQ(AdvanceHandDistance(distance, 10.0f, k_Frame, false).zoomer.duration, 0.1f);
	EXPECT_EQ(AdvanceHandDistance(distance, 30.0f, k_Frame, false).zoomer.duration, 0.28f);
}

TEST(HandDistanceZoomer, WhileGivingOrWatchingAFightMovingAwayTakesTwoSecondsUnlessItComesCloserFast)
{
	HandDistance distance {.started = true};
	distance.zoomer.SetPosition(20.0f);
	// moving away slowly takes 2 s; coming closer still takes a tenth of a second
	EXPECT_EQ(AdvanceHandDistance(distance, 30.0f, k_Frame, false, true).zoomer.duration, 2.0f);
	EXPECT_EQ(AdvanceHandDistance(distance, 10.0f, k_Frame, false, true).zoomer.duration, 0.1f);
	// coming closer at a speed of 1 or more, the move away takes the usual 0.28 s
	auto closing = distance;
	closing.zoomer.speed = -1.0f;
	EXPECT_EQ(AdvanceHandDistance(closing, 30.0f, k_Frame, false, true).zoomer.duration, 0.28f);
	closing.zoomer.speed = -0.99f;
	EXPECT_EQ(AdvanceHandDistance(closing, 30.0f, k_Frame, false, true).zoomer.duration, 2.0f);
}

TEST(HandDistanceZoomer, AStoppedPreparationNeitherStartsNorMovesIt)
{
	const auto fresh = AdvanceHandDistance({}, 40.0f, k_Frame, true);
	EXPECT_FALSE(fresh.started);
	ExpectSameZoomer(fresh.zoomer, openblack::Zoomer {});

	const auto moving = AdvanceHandDistance(AdvanceHandDistance({}, 40.0f, k_Frame, false), 40.0f, k_Frame, false);
	const auto stopped = AdvanceHandDistance(moving, 12.0f, k_Frame, true);
	EXPECT_TRUE(stopped.started);
	ExpectSameZoomer(stopped.zoomer, moving.zoomer);
}

TEST(HandDistanceZoomer, ItGoesOnFromWhereItStoppedOnceThePreparationRunsAgain)
{
	// frames under a box, hidden or with the interface inactive leave no trace: the run with them is the run without
	auto withStops = AdvanceHandDistance({}, 40.0f, k_Frame, false);
	auto without = withStops;
	for (int frame = 0; frame < 5; ++frame)
	{
		withStops = AdvanceHandDistance(withStops, 8.0f, k_Frame, true);
	}
	withStops = AdvanceHandDistance(withStops, 25.0f, k_Frame, false);
	without = AdvanceHandDistance(without, 25.0f, k_Frame, false);
	ExpectSameZoomer(withStops.zoomer, without.zoomer);
	EXPECT_EQ(withStops.started, without.started);
}

TEST(HandDistanceZoomer, OnlyTheNormalHoldingAndGrainStatesMoveIt)
{
	for (int32_t state = 0; state <= 10; ++state)
	{
		const bool moves = state == 1 || state == 4 || state == 8;
		EXPECT_EQ(StateMovesHandDistance(state, true, false), moves) << "state " << state;
	}
}

TEST(HandDistanceZoomer, AnInteractionThatHoldsTheHandStopsItWhileHoldingButNotInTheNormalState)
{
	EXPECT_FALSE(StateMovesHandDistance(4, true, true));
	EXPECT_FALSE(StateMovesHandDistance(8, true, true));
	EXPECT_TRUE(StateMovesHandDistance(1, false, true));
}

TEST(HandDistanceZoomer, TheGrainStateMovesItOnlyWhileTheHandHoldsItsObject)
{
	EXPECT_TRUE(StateMovesHandDistance(8, true, false));
	EXPECT_FALSE(StateMovesHandDistance(8, false, false));
	EXPECT_TRUE(StateMovesHandDistance(4, false, false));
}

TEST(HandDistanceZoomer, ItsStepIsTheStateUpdatesWholeMillisecondsTimesAThousandth)
{
	// one float product, as the original rounds it: 20 ms give 0.020000001, where 20000 us / 1e6 give 0.02
	EXPECT_EQ(std::bit_cast<uint32_t>(StateUpdateSeconds(33)), 0x3D072B02u);
	EXPECT_EQ(std::bit_cast<uint32_t>(StateUpdateSeconds(16)), 0x3C83126Fu);
	EXPECT_EQ(std::bit_cast<uint32_t>(StateUpdateSeconds(20)), 0x3CA3D70Bu);
	EXPECT_EQ(std::bit_cast<uint32_t>(StateUpdateSeconds(30)), 0x3CF5C290u);
	EXPECT_EQ(std::bit_cast<uint32_t>(StateUpdateSeconds(5)), 0x3BA3D70Bu);
	EXPECT_EQ(StateUpdateSeconds(0), 0.0f);
}

namespace
{
struct DistanceFrame
{
	float target;
	uint32_t secondsBits;
	uint32_t valueBits;
	uint32_t speedBits;
};

// The original's own steps of the hand's distance zoomer, run in the exe from a zoomer put at the start: each frame's
// target, time step, and the value and speed after it
constexpr std::array k_FromTwenty {
    DistanceFrame {.target = 10.0f, .secondsBits = 0x3D072B02u, .valueBits = 0x417FC353u, .speedBits = 0xC331C3A7u},
    DistanceFrame {.target = 12.0f, .secondsBits = 0x3D072B02u, .valueBits = 0x4149EF0Fu, .speedBits = 0xC2353A60u},
    // a zoomer's own step would sum this speed to 0xC090E9A8
    DistanceFrame {.target = 12.0f, .secondsBits = 0x3D072B02u, .valueBits = 0x413EC02Du, .speedBits = 0xC090E9A0u},
    DistanceFrame {.target = 30.0f, .secondsBits = 0x3C8B4396u, .valueBits = 0x41439F0Bu, .speedBits = 0x4219E7B2u},
    DistanceFrame {.target = 30.0f, .secondsBits = 0x3D4CCCCDu, .valueBits = 0x417F00FEu, .speedBits = 0x42C66592u},
};
constexpr std::array k_FromFive {
    DistanceFrame {.target = 47.3f, .secondsBits = 0x3C83126Fu, .valueBits = 0x40B88AD1u, .speedBits = 0x42B82E93u},
    DistanceFrame {.target = 47.3f, .secondsBits = 0x3C83126Fu, .valueBits = 0x40F82906u, .speedBits = 0x43199368u},
    DistanceFrame {.target = 46.9f, .secondsBits = 0x3C83126Fu, .valueBits = 0x412863B1u, .speedBits = 0x433E8AC9u},
    DistanceFrame {.target = 3.5f, .secondsBits = 0x3C83126Fu, .valueBits = 0x41377FE7u, .speedBits = 0xC23B0BB4u},
};

void ExpectTheOriginalsSteps(float start, std::span<const DistanceFrame> frames)
{
	HandDistance distance {.started = true};
	distance.zoomer.SetPosition(start);
	for (size_t i = 0; i < frames.size(); ++i)
	{
		const auto& frame = frames[i];
		distance = AdvanceHandDistance(distance, frame.target, std::bit_cast<float>(frame.secondsBits), false);
		EXPECT_EQ(std::bit_cast<uint32_t>(distance.zoomer.value), frame.valueBits) << "frame " << i;
		EXPECT_EQ(std::bit_cast<uint32_t>(distance.zoomer.speed), frame.speedBits) << "frame " << i;
	}
}
} // namespace

TEST(HandDistanceZoomer, ItStepsAsTheOriginalDoesInPlace)
{
	ExpectTheOriginalsSteps(20.0f, k_FromTwenty);
	ExpectTheOriginalsSteps(5.0f, k_FromFive);
}

TEST(HandViewDistance, TheTugTotemCreaturePlayAnimAndTempleStatesTakeNoScaleFromThePlace)
{
	for (int32_t state = 0; state <= 10; ++state)
	{
		const bool takes = state != 3 && state != 5 && state != 7 && state != 9 && state != 10;
		EXPECT_EQ(StateTakesViewDistance(state), takes) << "state " << state;
	}
}

TEST(HandViewDistance, TheTotemsIsItsDistanceFromTheEyeLessItsRadius)
{
	EXPECT_EQ(TotemViewDistance({3.0f, 4.0f, 12.0f}, {0.0f, 0.0f, 0.0f}, 2.0f), 11.0f);
	// summed z, y, x: summed x, y, z it would be 0x43F42191
	const float distance = TotemViewDistance({202.54678344726562f, 33.87259292602539f, 85.37661743164062f},
	                                         {-188.45623779296875f, 295.52606201171875f, 215.96791076660156f}, 0.0f);
	EXPECT_EQ(std::bit_cast<uint32_t>(distance), 0x43F42190u);
}

namespace
{
openblack::Zoomer AtRest(float value)
{
	openblack::Zoomer zoomer;
	zoomer.SetPosition(value);
	return zoomer;
}

// The original's creature state run in the exe: the eye, the body's bottom bone, its height (15 x 1.3) and the frame's
// time step
const glm::vec3 k_CreatureEye {120.5f, 95.25f, -40.75f};
const glm::vec3 k_CreatureGroin {100.0f, 12.5f, 10.0f};
constexpr float k_CreatureHeight = 19.5f;
constexpr float k_CreatureFrame = 0.033f;
} // namespace

TEST(CreatureHandDistance, TheEntryIsTheEyesDistanceToTheCreatureLessMostOfItsSize)
{
	EXPECT_EQ(CreatureEntryDistance({3.0f, 4.0f, 12.0f}, {0.0f, 0.0f, 0.0f}, 2.0f), 13.0f - 2.0f * 0.825f);
	// summed z, y, x
	const float distance = CreatureEntryDistance({202.54678344726562f, 33.87259292602539f, 85.37661743164062f},
	                                             {-188.45623779296875f, 295.52606201171875f, 215.96791076660156f}, 0.0f);
	EXPECT_EQ(std::bit_cast<uint32_t>(distance), 0x43F42190u);
}

TEST(CreatureHandDistance, ItIsKeptNoFurtherThanTheBodysBottomLessAQuarterOfItsHeight)
{
	const auto fromFar =
	    StepCreatureDistance(AtRest(150.0f), k_CreatureEye, k_CreatureGroin, std::nullopt, k_CreatureHeight, k_CreatureFrame);
	EXPECT_EQ(std::bit_cast<uint32_t>(fromFar.viewDistance), 0x42BCAD75u);
	EXPECT_EQ(std::bit_cast<uint32_t>(fromFar.zoomer.value), 0x42BCAD75u);
	const auto fromNear =
	    StepCreatureDistance(AtRest(80.0f), k_CreatureEye, k_CreatureGroin, std::nullopt, k_CreatureHeight, k_CreatureFrame);
	EXPECT_EQ(fromNear.viewDistance, 80.0f);
	EXPECT_EQ(fromNear.zoomer.value, 80.0f);
}

TEST(CreatureHandDistance, OnTheBodyItEasesToTheTouchAsTheOriginalDoes)
{
	struct Frame
	{
		std::optional<glm::vec3> touch;
		uint32_t viewBits;
		uint32_t valueBits;
		uint32_t speedBits;
	};
	// two frames on the body, then two off it; a zoomer's own step would sum the second frame's speed to 0x4361B9AC
	const std::array frames {
	    Frame {.touch = glm::vec3(105.0f, 20.0f, 5.0f),
	           .viewBits = 0x42200000u,
	           .valueBits = 0x422C10D8u,
	           .speedBits = 0x4328807Fu},
	    Frame {.touch = glm::vec3(104.0f, 22.0f, 4.0f),
	           .viewBits = 0x422C10D8u,
	           .valueBits = 0x42468EA1u,
	           .speedBits = 0x4361B9ABu},
	    Frame {.touch = std::nullopt, .viewBits = 0x42468EA1u, .valueBits = 0x4265F433u, .speedBits = 0x43743C58u},
	    Frame {.touch = std::nullopt, .viewBits = 0x4265F433u, .valueBits = 0x4282DA1Eu, .speedBits = 0x43687D28u},
	};
	auto zoomer = AtRest(40.0f);
	for (size_t i = 0; i < frames.size(); ++i)
	{
		const auto step =
		    StepCreatureDistance(zoomer, k_CreatureEye, k_CreatureGroin, frames[i].touch, k_CreatureHeight, k_CreatureFrame);
		zoomer = step.zoomer;
		EXPECT_EQ(std::bit_cast<uint32_t>(step.viewDistance), frames[i].viewBits) << "frame " << i;
		EXPECT_EQ(std::bit_cast<uint32_t>(zoomer.value), frames[i].valueBits) << "frame " << i;
		EXPECT_EQ(std::bit_cast<uint32_t>(zoomer.speed), frames[i].speedBits) << "frame " << i;
	}
}

TEST(CreatureHandDistance, FromFarItIsPutAtTheTouchAtOnceThenEasesOn)
{
	const auto step = StepCreatureDistance(AtRest(150.0f), k_CreatureEye, k_CreatureGroin, glm::vec3(105.0f, 20.0f, 5.0f),
	                                       k_CreatureHeight, k_CreatureFrame);
	EXPECT_EQ(std::bit_cast<uint32_t>(step.viewDistance), 0x42AF8639u);
	EXPECT_EQ(std::bit_cast<uint32_t>(step.zoomer.value), 0x42AF98E9u);
	EXPECT_EQ(std::bit_cast<uint32_t>(step.zoomer.destination), 0x42B0B1BEu);
	EXPECT_EQ(std::bit_cast<uint32_t>(step.zoomer.speed), 0x40027CD2u);
}

namespace
{
// The original's temple state run in the exe: the eye, and each frame's point where the cursor meets the room
const glm::vec3 k_TempleEye {10.5f, 20.25f, -3.75f};
} // namespace

TEST(CitadelHandDistance, TheAimIsTheRoomsDistanceLessFiveAndAtLeastOne)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(CitadelRoomDistance(k_TempleEye, {18.0f, 12.0f, 9.0f})), 0x413EFF88u);
	EXPECT_EQ(std::bit_cast<uint32_t>(CitadelRoomDistance(k_TempleEye, {60.0f, 5.0f, 70.0f})), 0x42AA3E2Eu);
	EXPECT_EQ(CitadelRoomDistance(k_TempleEye, {11.0f, 20.0f, -2.0f}), 1.0f);
}

TEST(CitadelHandDistance, ItEasesAsTheOriginalDoesWithinFourAndThreeHundred)
{
	struct Frame
	{
		glm::vec3 along;
		uint32_t destinationBits;
		float duration;
		uint32_t valueBits;
		uint32_t speedBits;
	};
	// nearer in 0.2 s, a destination it already has in 0.4 s, further in 0.4 s, and at least 4
	const std::array frames {
	    Frame {glm::vec3(17.9f, 12.1f, 8.9f), 0x41862741u, 0.2f, 0x41E24746u, 0xC2B6A6F8u},
	    Frame {glm::vec3(17.9f, 12.1f, 8.9f), 0x41862741u, 0.4f, 0x41CC4BB2u, 0xC29700B2u},
	    Frame {glm::vec3(59.0f, 5.5f, 69.0f), 0x42B1570Fu, 0.4f, 0x41CF53E5u, 0x42B1E78Au},
	    Frame {glm::vec3(11.0f, 20.0f, -2.5f), 0x40800000u, 0.2f, 0x41C6453Eu, 0xC302304Eu},
	};
	openblack::Zoomer zoomer;
	zoomer.SetPosition(30.0f);
	for (size_t i = 0; i < frames.size(); ++i)
	{
		const auto step = StepCitadelDistance(zoomer, k_TempleEye, frames[i].along, 0.033f);
		zoomer = step.zoomer;
		EXPECT_EQ(std::bit_cast<uint32_t>(zoomer.destination), frames[i].destinationBits) << "frame " << i;
		EXPECT_EQ(zoomer.duration, frames[i].duration) << "frame " << i;
		EXPECT_EQ(std::bit_cast<uint32_t>(zoomer.value), frames[i].valueBits) << "frame " << i;
		EXPECT_EQ(std::bit_cast<uint32_t>(zoomer.speed), frames[i].speedBits) << "frame " << i;
		EXPECT_EQ(step.distance, zoomer.value) << "frame " << i;
	}
}

TEST(CitadelHandDistance, ItSumsItsValueAsTheOriginalDoes)
{
	// a frame where a zoomer's own step and the in-place orders of the other states give other bits
	openblack::Zoomer zoomer;
	zoomer.value = std::bit_cast<float>(0x4276B5F8u);
	zoomer.destination = std::bit_cast<float>(0x41907AADu);
	zoomer.speed = std::bit_cast<float>(0xC3FCD556u);
	zoomer.time = std::bit_cast<float>(0x3D4CCCCDu);
	zoomer.duration = std::bit_cast<float>(0x3E4CCCCDu);
	zoomer.startValue = std::bit_cast<float>(0x429BFB4Cu);
	zoomer.startSpeed = std::bit_cast<float>(0xC1EE3460u);
	zoomer.c2 = std::bit_cast<float>(0xC6857BFCu);
	zoomer.c3 = std::bit_cast<float>(0x48A90945u);
	zoomer.c4 = std::bit_cast<float>(0xCA1F2729u);
	const glm::vec3 along {80.09867095947266f, -35.93870162963867f, -29.330320358276367f};
	const auto step = StepCitadelDistance(zoomer, k_TempleEye, along, std::bit_cast<float>(0x3D072B02u));
	EXPECT_EQ(std::bit_cast<uint32_t>(step.zoomer.destination), 0x42BA11E2u);
	EXPECT_EQ(std::bit_cast<uint32_t>(step.zoomer.value), 0x4247BBD3u);
	EXPECT_EQ(std::bit_cast<uint32_t>(step.zoomer.speed), 0xC35BDE98u);
}
