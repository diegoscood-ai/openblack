/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A recognised gesture's effect (Particles/Rules/Gesture.cpp) starts the gesture's light sheet:
// one sheet per record, on the gesture's shape lifted towards the camera, in the colour of the record's player, as high
// as the shape's size times LightSheetHeightScale, fed the strength of its atom's age, and gone with its atom. A
// synthetic effect (the game's gesture effect's values) runs on a synthetic record; the particle service is a fake put in
// the locator, which keeps the sheets it is given.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Audio/AudioManagerNoOp.h"
#include "Camera/Camera.h"
#include "Common/GameRandomTesting.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "Enums.h"
#include "Locator.h"
#include "Magic/Gestures/GestureBuffer.h"
#include "Magic/Gestures/GestureMatch.h"
#include "Magic/Gestures/GestureShapes.h"
#include "Particles/GestureTrail.h"
#include "Particles/LightSheet.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleManagerState.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/Rules/SurfRevol.h"
#include "Particles/Utility.h"
#include "support/ParticleFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using openblack::magic::gestures::Path;
using openblack::magic::gestures::RecognisedGesture;
using openblack::particles::LightSheet;
namespace gesture_trail = openblack::particles::gesture_trail;

namespace
{
/// The game's gesture effect's rule values (DieAge 7, LightSheetDieAge 4, LightSheetHeightScale 9), with a few sprites
constexpr std::string_view k_EffectText =
    "BEGINPROPERTIES\n"
    "PROPERTY DeleteOnCloseDown BOOL 1\n"
    "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
    "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
    "PROPERTY MaxSpellAge FLOAT -1\n"
    "ENDPROPERTIES\n"
    "BEGINCLASS ParticlePointCreator Point\nBEGINPROPERTIES\nPROPERTY InitialScale FLOAT 1\nENDPROPERTIES\nENDCLASS\n"
    "BEGINCLASS ParticleSpriteCreator Blob\nBEGINPROPERTIES\nPROPERTY InitialScale FLOAT 1\nENDPROPERTIES\nENDCLASS\n"
    "BEGINCLASS UR_GesturingRecognised Recognised\nBEGINPROPERTIES\n"
    "PROPERTY CollectionAlphaInit INTEGER 50\nPROPERTY CollectionAlphaPulse INTEGER 255\n"
    "PROPERTY DieAge FLOAT 7\nPROPERTY DispersalTime FLOAT 3\nPROPERTY DoTransition BOOL 1\n"
    "PROPERTY GoToIdeal BOOL 1\nPROPERTY Group INTEGER 0\n"
    "PROPERTY InterpGain FLOAT 0.9\nPROPERTY LightSheetDieAge FLOAT 4\nPROPERTY LightSheetHeightScale FLOAT 9\n"
    "PROPERTY MaxAlpha FLOAT 176.087\nPROPERTY NextGroups ARRAY SIZE 1 1\nPROPERTY NumAtoms INTEGER 4\n"
    "PROPERTY PCreator PERSIS_PNTR Point\nPROPERTY ShrinkTimeAfterDispersal FLOAT 2\n"
    "PROPERTY SparkleGroup INTEGER 3\nPROPERTY SpriteCreator PERSIS_PNTR Blob\n"
    "PROPERTY TimeToIdeal FLOAT 0.6\nPROPERTY WiggleFreq FLOAT 10\nPROPERTY WiggleMag FLOAT 0.1\n"
    "PROPERTY WiggleMagY FLOAT 0.01\nPROPERTY WigglePhaseSpeed FLOAT 1.5\nPROPERTY WiggleSpeed FLOAT 2\n"
    "ENDPROPERTIES\nENDCLASS\n";

constexpr float k_DieAge = 7.0f;
constexpr float k_SheetDieAge = 4.0f;
constexpr float k_HeightScale = 9.0f;
constexpr float k_Step = 0.1f;
/// The shape: a straight line 40 long on the land, so that its size (a hundredth of its length) is 0.4
constexpr float k_Length = 40.0f;
const glm::vec3 k_Start {100.0f, 0.0f, 200.0f};
const glm::vec3 k_CameraAt {120.0f, 100.0f, 200.0f};

/// A particle service that keeps the sheets it is given, and the engine's state for the rule to take its records from
class SheetKeepingParticles final: public test::InertParticleSystem
{
public:
	std::vector<std::weak_ptr<LightSheet>> added;

	void AddLightSheet(const std::shared_ptr<LightSheet>& sheet) override { added.push_back(sheet); }
};

/// The shape's five points, from k_Start along x
Path Ideal()
{
	Path ideal;
	for (int i = 0; i <= 4; ++i)
	{
		ideal.Add(k_Start + glm::vec3(k_Length * static_cast<float>(i) / 4.0f, 0.0f, 0.0f));
	}
	return ideal;
}

/// The shape as the rule lifts it: every point towards the camera until it is the shape's size higher, at most half-way
Path LiftedIdeal()
{
	Path ideal = Ideal();
	const float scale = ideal.Length() * 0.01f;
	for (size_t k = 0; k < ideal.Size(); ++k)
	{
		glm::vec3 d = k_CameraAt - ideal[k];
		const float length = glm::length(d);
		d /= length;
		ideal[k] += d * std::min(scale / d.y, length * 0.5f);
	}
	ideal.Measure();
	return ideal;
}

RecognisedGesture Record(PlayerNames player)
{
	RecognisedGesture record;
	for (int i = 0; i <= 4; ++i)
	{
		record.stroke.Add(k_Start + glm::vec3(k_Length * static_cast<float>(i) / 4.0f, 0.0f, 5.0f));
	}
	record.ideal = Ideal();
	record.player = player;
	return record;
}

class GestureEffects: public ::testing::Test
{
	// first, so that they go last: the services from before come back after the test's, the random state among them
	// (the rule's shuffle draws from the particles' random stream)
	const game_random::testing::ScopedState _random;
	const test::RestoreService<Locator::camera> _camera;
	const test::RestoreService<Locator::audio> _audio;
	const test::RestoreService<Locator::particleSystem> _particleSystem;

protected:
	void SetUp() override
	{
		Locator::camera::emplace(glm::vec3(0.0f)).SetOrigin(k_CameraAt);
		Locator::audio::emplace<audio::AudioManagerNoOp>();
		_particles = &static_cast<SheetKeepingParticles&>(Locator::particleSystem::emplace<SheetKeepingParticles>());
		auto file = particles::File::Parse(k_EffectText, "SF_GestureTest");
		ASSERT_TRUE(file.has_value());
		_effect = std::make_unique<particles::Effect>(std::make_shared<const particles::File>(std::move(*file)),
		                                              glm::vec3(0.0f), 1.0f);
	}
	void TearDown() override { _effect.reset(); }

	/// A record of the player's arrives and the effect steps once: the step that takes it
	void Recognise(PlayerNames player)
	{
		_particles->GetState().pendingGestures.push_back(Record(player));
		_effect->Step(k_Step);
	}

	[[nodiscard]] std::shared_ptr<LightSheet> OnlySheet() const
	{
		EXPECT_EQ(_particles->added.size(), 1u);
		return _particles->added.empty() ? nullptr : _particles->added.front().lock();
	}

	/// The sheet is the one a sheet started on the lifted shape's sheet points, in that colour, as high as the shape's
	/// size times LightSheetHeightScale, gives: fed the same strength and moved on by the same time, they build the same
	static void ExpectStartedAs(LightSheet& sheet, uint32_t rgb)
	{
		const float height = (k_Length * 0.01f) * k_HeightScale;
		LightSheet twin;
		twin.Start(gesture_trail::SheetPoints(LiftedIdeal()), rgb, height, gesture_trail::k_SheetShiftSeconds);
		for (auto* each : {&sheet, &twin})
		{
			each->SetStrength(1.0f);
			each->Update(0.04f);
		}
		EXPECT_EQ(sheet.Strengths(), twin.Strengths());
		EXPECT_EQ(sheet.Heights(), twin.Heights());
		// the first point's height: the wave at its first point times the shape's size times 9
		const double wave = std::cos(0.0 - (static_cast<double>(0.04f) * 3.0));
		ASSERT_FALSE(sheet.Heights().empty());
		EXPECT_FLOAT_EQ(sheet.Heights().front(),
		                static_cast<float>(((wave * static_cast<double>(0.3f)) + static_cast<double>(0.8f)) * height));

		std::vector<LightSheet::Vertex> vertices;
		std::vector<LightSheet::Vertex> twinVertices;
		std::vector<uint32_t> triangles;
		std::vector<uint32_t> twinTriangles;
		sheet.Build(vertices, triangles);
		twin.Build(twinVertices, twinTriangles);
		ASSERT_EQ(vertices.size(), static_cast<size_t>(gesture_trail::k_SheetPoints) * 3u);
		ASSERT_EQ(vertices.size(), twinVertices.size());
		EXPECT_EQ(triangles, twinTriangles);
		for (size_t i = 0; i < vertices.size(); ++i)
		{
			EXPECT_NEAR(vertices[i].position.x, twinVertices[i].position.x, 1e-4f) << i;
			EXPECT_NEAR(vertices[i].position.y, twinVertices[i].position.y, 1e-4f) << i;
			EXPECT_NEAR(vertices[i].position.z, twinVertices[i].position.z, 1e-4f) << i;
			EXPECT_EQ(vertices[i].argb, twinVertices[i].argb) << i;
			EXPECT_EQ(vertices[i].specularArgb, twinVertices[i].specularArgb) << i;
		}
		// its ends stand on the lifted shape's ends, above the land
		const auto lifted = LiftedIdeal();
		EXPECT_NEAR(glm::distance(vertices.front().position, lifted[0]), 0.0f, 1e-4f);
		EXPECT_NEAR(glm::distance(vertices[vertices.size() - 3].position, lifted[lifted.Size() - 1]), 0.0f, 1e-4f);
		EXPECT_GT(vertices.front().position.y, 0.0f);
	}

	SheetKeepingParticles* _particles {nullptr};
	std::unique_ptr<particles::Effect> _effect;
};
} // namespace

TEST_F(GestureEffects, ARecognitionStartsOneSheetOnTheLiftedShape)
{
	_effect->Step(k_Step);
	EXPECT_TRUE(_particles->added.empty());
	Recognise(PlayerNames::PLAYER_ONE);
	EXPECT_TRUE(_particles->GetState().pendingGestures.empty());
	const auto sheet = OnlySheet();
	ASSERT_NE(sheet, nullptr);
	EXPECT_TRUE(sheet->Started());
	// the next steps start no other
	_effect->Step(k_Step);
	_effect->Step(k_Step);
	EXPECT_EQ(_particles->added.size(), 1u);
	// the first player's colour, the one the sparkles always had
	EXPECT_EQ(particles::surf_revol::PlayerColour(static_cast<int>(PlayerNames::PLAYER_ONE)) & 0xFFFFFFu, 0xFF4646u);
	ExpectStartedAs(*sheet, 0xFF4646u);
}

TEST_F(GestureEffects, TheSheetIsInTheRecordsPlayersColour)
{
	Recognise(PlayerNames::PLAYER_TWO);
	const auto sheet = OnlySheet();
	ASSERT_NE(sheet, nullptr);
	EXPECT_EQ(particles::surf_revol::PlayerColour(static_cast<int>(PlayerNames::PLAYER_TWO)) & 0xFFFFFFu, 0x47FF54u);
	ExpectStartedAs(*sheet, 0x47FF54u);
}

TEST_F(GestureEffects, TheSheetIsFedTheStrengthOfItsAtomsAge)
{
	Recognise(PlayerNames::PLAYER_ONE);
	const auto sheet = OnlySheet();
	ASSERT_NE(sheet, nullptr);
	// each move on by more than the shift period takes in the strength fed last: nothing at the start
	sheet->Update(0.04f);
	EXPECT_EQ(sheet->Strengths().front(), 0.0f);
	for (int step = 1; step <= 45; ++step)
	{
		_effect->Step(k_Step);
		sheet->Update(0.04f);
		const float age = static_cast<float>(step) * k_Step;
		EXPECT_NEAR(sheet->Strengths().front(), gesture_trail::SheetStrength(age, k_SheetDieAge), 1e-3f) << step;
	}
	// full at half of LightSheetDieAge (step 20), nothing again from LightSheetDieAge on
	EXPECT_EQ(sheet->Strengths().front(), 0.0f);
}

TEST_F(GestureEffects, TheSheetGoesWithItsAtom)
{
	Recognise(PlayerNames::PLAYER_ONE);
	ASSERT_EQ(_particles->added.size(), 1u);
	// the atom is a step short of DieAge (6.9 s old): it and its sheet are still there
	const auto shortOfDieAge = static_cast<int>(std::lround(k_DieAge / k_Step)) - 1;
	for (int step = 1; step <= shortOfDieAge; ++step)
	{
		_effect->Step(k_Step);
	}
	EXPECT_FALSE(_particles->added.front().expired());
	EXPECT_GT(_effect->AtomCount(), 0u);
	// past DieAge (7 s) the atom goes, and the sheet with it: the particle service only kept a weak hold
	for (int step = 0; step < 3; ++step)
	{
		_effect->Step(k_Step);
	}
	EXPECT_TRUE(_particles->added.front().expired());
	EXPECT_EQ(_effect->AtomCount(), 0u);
	EXPECT_EQ(_particles->added.size(), 1u);
}

TEST_F(GestureEffects, GesturesInQuickSuccessionEachLeaveTheirOwnTrailTheNewestFirst)
{
	// two recognitions inside one turn: each step takes one record, the newest first
	_particles->GetState().pendingGestures.push_back(Record(PlayerNames::PLAYER_ONE));
	_particles->GetState().pendingGestures.push_back(Record(PlayerNames::PLAYER_TWO));
	_effect->Step(k_Step);
	ASSERT_EQ(_particles->GetState().pendingGestures.size(), 1u);
	ASSERT_EQ(_particles->added.size(), 1u);
	auto newest = _particles->added.front().lock();
	ASSERT_NE(newest, nullptr);
	ExpectStartedAs(*newest, 0x47FF54u);
	_effect->Step(k_Step);
	EXPECT_TRUE(_particles->GetState().pendingGestures.empty());
	ASSERT_EQ(_particles->added.size(), 2u);
	auto older = _particles->added.back().lock();
	ASSERT_NE(older, nullptr);
	ExpectStartedAs(*older, 0xFF4646u);
	EXPECT_FALSE(_particles->added.front().expired());
}

namespace
{
using openblack::magic::gestures::GestureSystem;
using openblack::magic::gestures::Result;
using openblack::magic::gestures::Shape;

/// A square shape over the whole of 0..1
const Shape& SquareShape(magic::gestures::Gesture /*gesture*/)
{
	static const Shape k_Square {
	    .points = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
	    .length = 4.0f,
	    .aspect = 1.0f,
	    .maxX = 1.0f,
	    .minX = 0.0f,
	    .maxZ = 1.0f,
	    .minZ = 0.0f,
	};
	return k_Square;
}

/// Three samples whose first two span the pixels 100..500 both ways; the last is a land point only when asked
GestureSystem ThreeSamples(bool lastOnLand = true)
{
	GestureSystem system;
	system.AddSample({10.0f, 0.0f, 10.0f}, {100, 100});
	system.AddSample({50.0f, 0.0f, 50.0f}, {500, 500});
	system.AddSample(lastOnLand ? glm::vec3(60.0f, 0.0f, 60.0f) : glm::vec3(0.0f), {600, 600});
	return system;
}

/// The match over the whole buffer: its last keypoint is the newest sample
Result WholeMatch(const GestureSystem& system)
{
	uint8_t keys = 0;
	for (int i = 0; i + 1 < system.Count(); ++i)
	{
		if ((system.At(i).flags & 0xBu) != 0)
		{
			++keys;
		}
	}
	return {.gesture = magic::gestures::k_Circle, .start = 0, .end = keys};
}

/// The land seen from a camera looking along +z: a pixel's land point, its depth stretched `depth` times
struct StretchedLand
{
	float depth {1.0f};
	std::vector<float> lengths;
	glm::vec3 operator()(glm::ivec2 pixel, float length)
	{
		lengths.push_back(length);
		return {static_cast<float>(pixel.x), 0.0f, static_cast<float>(pixel.y) * depth};
	}
};

/// How many times the shape was laid on the land, from the land lookups (one per point of the shape)
size_t Layings(const StretchedLand& land)
{
	return land.lengths.size() / SquareShape(0).points.size();
}
} // namespace

TEST(GestureTrail, TheTrailNeedsTwoSamplesAGestureAndTwoLandPoints)
{
	StretchedLand land;
	auto call = [&land](glm::ivec2 pixel, float length) { return land(pixel, length); };
	const glm::vec3 forward(0.0f, 0.0f, 1.0f);

	// one sample
	GestureSystem one;
	one.AddSample({10.0f, 0.0f, 10.0f}, {100, 100});
	EXPECT_FALSE(particles::utility::RecognisedTrail(one, {.gesture = magic::gestures::k_Circle}, SquareShape, forward, call));
	// no gesture
	const auto three = ThreeSamples();
	EXPECT_FALSE(particles::utility::RecognisedTrail(three, {}, SquareShape, forward, call));
	// two samples, one of them off the land
	GestureSystem twoOneOnLand;
	twoOneOnLand.AddSample({10.0f, 0.0f, 10.0f}, {100, 100});
	twoOneOnLand.AddSample(glm::vec3(0.0f), {500, 500});
	EXPECT_FALSE(particles::utility::RecognisedTrail(twoOneOnLand, WholeMatch(twoOneOnLand), SquareShape, forward, call));
	EXPECT_TRUE(land.lengths.empty());
	// two land points of three: a trail of those two, the shape resampled to as many
	const auto twoOfThree = ThreeSamples(false);
	const auto record = particles::utility::RecognisedTrail(twoOfThree, WholeMatch(twoOfThree), SquareShape, forward, call);
	ASSERT_TRUE(record.has_value());
	EXPECT_EQ(record->stroke.Size(), 2u);
	EXPECT_EQ(record->ideal.Size(), 2u);
	EXPECT_TRUE(record->fromInterface);
}

TEST(GestureTrail, TheShapeIsSquashedUpTheScreenUntilNoDeeperThanTwiceItsWidth)
{
	const glm::vec3 forward(0.0f, 0.0f, 1.0f);
	const auto system = ThreeSamples();
	const auto match = WholeMatch(system);
	auto layings = [&](float depth) {
		StretchedLand land {.depth = depth};
		const auto record = particles::utility::RecognisedTrail(
		    system, match, SquareShape, forward, [&land](glm::ivec2 pixel, float length) { return land(pixel, length); });
		EXPECT_TRUE(record.has_value());
		return Layings(land);
	};
	// seen from above, as deep as wide: laid once
	EXPECT_EQ(layings(1.0f), 1u);
	// exactly twice as deep: no squash
	EXPECT_EQ(layings(2.0f), 1u);
	// ten times as deep: x 0.75 six times (10 x 0.75^5 is still above 2, 10 x 0.75^6 is 1.78)
	EXPECT_EQ(layings(10.0f), 7u);
	// from a camera nearly level with the land: at most 15 layings
	EXPECT_EQ(layings(1.0e4f), 15u);
}

TEST(GestureTrail, OffTheLandTheShapesPointsGoFourHundredAlongTheirLine)
{
	EXPECT_EQ(particles::utility::k_ShapeOffTheLand, 400.0f);
	const auto system = ThreeSamples();
	std::vector<float> lengths;
	const glm::vec3 farAway(1000.0f, 50.0f, 2000.0f);
	const auto record = particles::utility::RecognisedTrail(system, WholeMatch(system), SquareShape, {0.0f, 0.0f, 1.0f},
	                                                        [&](glm::ivec2 /*pixel*/, float length) {
		                                                        lengths.push_back(length);
		                                                        return farAway;
	                                                        });
	ASSERT_TRUE(record.has_value());
	ASSERT_FALSE(lengths.empty());
	EXPECT_TRUE(std::ranges::all_of(lengths, [](float length) { return length == 400.0f; }));
	// the points it gives are the shape's, as they are
	for (size_t k = 0; k < record->ideal.Size(); ++k)
	{
		EXPECT_EQ(record->ideal[k], farAway);
	}
}
