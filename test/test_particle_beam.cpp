/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The simple beam: its pure maths (Particles/BeamMaths) on fakes, and UR_SimpleBeam (Particles/ParticleBeamRules.cpp)
// run on small effects with a fake land and a registry of its own.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Common/GameRandomTesting.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Particles/BeamMaths.h"
#include "Particles/Noise.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysRegistry.h"
#include "support/LandFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::particles;

namespace
{
constexpr float k_Epsilon = 1e-4f;
constexpr float k_Step = 0.1f;

const auto k_FlatLand = [](glm::vec2 /*xz*/) { return 0.0f; };

/// SF_SimpleBeam as the game has it, with the minimum height given
std::string BeamFile(const std::string& minHeight = "-4.31602e+008")
{
	std::string text;
	text += "BEGINPROPERTIES\n";
	text += "PROPERTY DeleteOnCloseDown BOOL 0\n";
	text += "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n";
	text += "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n";
	text += "PROPERTY MaxSpellAge FLOAT -1\n";
	text += "ENDPROPERTIES\n";
	text += "BEGINCLASS RemoveRuleAfterCloseDown RemoveRuleAfterCloseDown0\n";
	text += "BEGINPROPERTIES\n";
	text += "PROPERTY Condition PERSIS_PNTR NULL_STRING\n";
	text += "PROPERTY Delay FLOAT 1.5\n";
	text += "PROPERTY Group INTEGER 0\n";
	text += "PROPERTY RemoveOnCloseDown BOOL 0\n";
	text += "ENDPROPERTIES\n";
	text += "ENDCLASS\n";
	text += "BEGINCLASS AR_FadeCollectionAlpha AR_FadeCollectionAlpha1\n";
	text += "BEGINPROPERTIES\n";
	text += "PROPERTY Condition PERSIS_PNTR NULL_STRING\n";
	text += "PROPERTY Group INTEGER 1\n";
	text += "PROPERTY RemoveOnCloseDown BOOL 0\n";
	text += "PROPERTY SetAlphaAfterStopTime BOOL 1\n";
	text += "PROPERTY StartAlpha INTEGER 255\n";
	text += "PROPERTY StartTime FLOAT 0\n";
	text += "PROPERTY StopAlpha INTEGER 0\n";
	text += "PROPERTY StopTime FLOAT 1.0\n";
	text += "PROPERTY TimesAreAfterCloseDown BOOL 1\n";
	text += "ENDPROPERTIES\n";
	text += "ENDCLASS\n";
	text += "BEGINCLASS UR_SimpleBeam UR_SimpleBeam0\n";
	text += "BEGINPROPERTIES\n";
	text += "PROPERTY Condition PERSIS_PNTR NULL_STRING\n";
	text += "PROPERTY BeamGroup INTEGER 1\n";
	text += "PROPERTY ForkScaleMax FLOAT 0.8\n";
	text += "PROPERTY ForkScaleMin FLOAT 0.1\n";
	text += "PROPERTY Group INTEGER 0\n";
	text += "PROPERTY MaxJointsPerFork INTEGER 20\n";
	text += "PROPERTY MinHeight FLOAT " + minHeight + "\n";
	text += "PROPERTY NextGroups ARRAY SIZE 0\n";
	text += "PROPERTY NumBeams INTEGER 3\n";
	text += "PROPERTY NumSplinePoints INTEGER 6\n";
	text += "PROPERTY PCreator PERSIS_PNTR ParticleChainCreator0\n";
	text += "PROPERTY RandomFrac FLOAT 3\n";
	text += "PROPERTY RemoveOnCloseDown BOOL 0\n";
	text += "PROPERTY SpeedV FLOAT 1\n";
	text += "PROPERTY WiggleFreq FLOAT 3\n";
	text += "PROPERTY WiggleSpeed FLOAT -4\n";
	text += "ENDPROPERTIES\n";
	text += "ENDCLASS\n";
	text += "BEGINCLASS ParticleChainCreator ParticleChainCreator0\n";
	text += "BEGINPROPERTIES\n";
	text += "PROPERTY ColorA INTEGER 33\n";
	text += "PROPERTY ColorB INTEGER 255\n";
	text += "PROPERTY ColorG INTEGER 255\n";
	text += "PROPERTY ColorR INTEGER 255\n";
	text += "PROPERTY FileOffset INTEGER 0\n";
	text += "PROPERTY FrameHeight INTEGER 256\n";
	text += "PROPERTY FrameOfHead INTEGER 0\n";
	text += "PROPERTY FrameOfTail INTEGER 0\n";
	text += "PROPERTY FrameWidth INTEGER 64\n";
	text += "PROPERTY InitialScale FLOAT 1\n";
	text += "PROPERTY LoopAnim BOOL 1\n";
	text += "PROPERTY MaterialSetDoubleSided BOOL 0\n";
	text += "PROPERTY MaterialUpdateZBuffer BOOL 0\n";
	text += "PROPERTY NumTexturesForWholeChain INTEGER 4\n";
	text += "PROPERTY SpecColorB INTEGER 0\n";
	text += "PROPERTY SpecColorG INTEGER 0\n";
	text += "PROPERTY SpecColorR INTEGER 0\n";
	text += "PROPERTY TextureFileName STRING .\\Data\\Textures\\S_Beam.raw\n";
	text += "PROPERTY UseAdditiveAlpha BOOL 1\n";
	text += "PROPERTY UseDynamicLighting BOOL 0\n";
	text += "ENDPROPERTIES\n";
	text += "ENDCLASS\n";
	text += "BEGINCLASS ParticlePointCreator ParticlePointCreator0\n";
	text += "BEGINPROPERTIES\n";
	text += "PROPERTY ColorA INTEGER 255\n";
	text += "PROPERTY ColorB INTEGER 255\n";
	text += "PROPERTY ColorG INTEGER 255\n";
	text += "PROPERTY ColorR INTEGER 255\n";
	text += "PROPERTY InitialScale FLOAT 1\n";
	text += "PROPERTY LoopAnim BOOL 1\n";
	text += "PROPERTY SpecColorB INTEGER 0\n";
	text += "PROPERTY SpecColorG INTEGER 0\n";
	text += "PROPERTY SpecColorR INTEGER 0\n";
	text += "ENDPROPERTIES\n";
	text += "ENDCLASS\n";
	return text;
}

/// The file's wiggle
constexpr maths::BeamWiggle k_FileWiggle {.frequency = 3.0f, .speed = -4.0f, .amount = 3.0f, .minHeight = -4.31602e+08f};

/// A small effect of the beam file on a fake land and a registry of its own; the previous services come back after
class SimpleBeamTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::terrainSystem::emplace<test::WaterCellIsland>(static_cast<uint16_t>(1));
	}

	std::unique_ptr<psys::Effect> Make(const std::string& text, glm::vec3 origin, float magnitude = 1.0f,
	                                   game_random::psys::NetGameType type = game_random::psys::NetGameType::Local)
	{
		auto file = psys::File::Parse(text, "test");
		EXPECT_TRUE(file.has_value());
		return std::make_unique<psys::Effect>(std::make_shared<const psys::File>(std::move(*file)), origin, magnitude, type);
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// The land the fake gives everywhere
	static constexpr float k_Land = test::WaterCellIsland::k_HeightMarker;

private:
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::terrainSystem> _restoreTerrain;
};
} // namespace

TEST(BeamMaths, TheBulgeIsNoneAtTheEndsAndAllAtTheMiddle)
{
	EXPECT_FLOAT_EQ(maths::BeamBulge(0.0f), 0.0f);
	EXPECT_FLOAT_EQ(maths::BeamBulge(1.0f), 0.0f);
	EXPECT_FLOAT_EQ(maths::BeamBulge(0.5f), 1.0f);
	EXPECT_FLOAT_EQ(maths::BeamBulge(0.25f), 0.75f);
}

TEST(BeamMaths, KeyPointsRunStraightWithoutWiggle)
{
	const maths::BeamWiggle still {.frequency = 3.0f, .speed = -4.0f, .amount = 0.0f, .minHeight = -1000.0f};
	const auto keys = maths::BeamKeyPoints(
	    {0.0f, 10.0f, 0.0f}, {10.0f, 0.0f, 20.0f}, 6, still, 1.0f, 0, [](float) { return 0.5f; }, k_FlatLand);
	ASSERT_EQ(keys.size(), 6u);
	for (size_t i = 0; i < keys.size(); ++i)
	{
		const float t = static_cast<float>(i) / 5.0f;
		EXPECT_NEAR(keys[i].x, 10.0f * t, k_Epsilon);
		EXPECT_NEAR(keys[i].y, 10.0f - (10.0f * t), k_Epsilon);
		EXPECT_NEAR(keys[i].z, 20.0f * t, k_Epsilon);
	}
}

TEST(BeamMaths, KeyPointsBetweenTheEndsArePushedByTheNoise)
{
	const maths::BeamWiggle wiggle {.frequency = 3.0f, .speed = -4.0f, .amount = 3.0f, .minHeight = -1000.0f};
	std::vector<float> asked;
	const auto noise = [&asked](float x) {
		asked.push_back(x);
		return 0.5f;
	};
	const glm::vec3 start(0.0f, 50.0f, 0.0f);
	const glm::vec3 end(0.0f, 50.0f, 50.0f);
	const float age = 0.3f;
	const int beam = 2;
	const auto keys = maths::BeamKeyPoints(start, end, 6, wiggle, age, beam, noise, k_FlatLand);
	ASSERT_EQ(keys.size(), 6u);
	// The ends are left where they are
	EXPECT_EQ(keys.front(), start);
	EXPECT_NEAR(glm::distance(keys.back(), end), 0.0f, k_Epsilon);
	// Each point between asks the noise across, along and up, drifting at its own share of the speed
	ASSERT_EQ(asked.size(), 12u);
	for (int i = 1; i < 5; ++i)
	{
		const float t = static_cast<float>(i) / 5.0f;
		const float bulge = 1.0f - ((2.0f * t - 1.0f) * (2.0f * t - 1.0f));
		const auto k = static_cast<size_t>(i - 1) * 3;
		EXPECT_NEAR(asked[k], (age * -4.0f) + (t * 3.0f) + 2.0f, k_Epsilon);
		EXPECT_NEAR(asked[k + 1], (age * -4.0f * 0.7f) + (t * 3.0f) + 2.0f, k_Epsilon);
		EXPECT_NEAR(asked[k + 2], (age * -4.0f * 1.3f) + (t * 3.0f) + 2.0f, k_Epsilon);
		const auto& key = keys[static_cast<size_t>(i)];
		EXPECT_NEAR(key.x, 0.5f * 3.0f * bulge, k_Epsilon);
		EXPECT_NEAR(key.z, (50.0f * t) + (0.5f * 3.0f * bulge), k_Epsilon);
		// Upwards only, half as far
		EXPECT_NEAR(key.y, 50.0f + ((0.5f + 1.0f) * 3.0f * bulge * 0.5f), k_Epsilon);
	}
}

TEST(BeamMaths, KeyPointsKeepAboveTheLand)
{
	const maths::BeamWiggle wiggle {.frequency = 3.0f, .speed = 1.0f, .amount = 1.0f, .minHeight = 2.0f};
	const auto hill = [](glm::vec2 /*xz*/) { return 5.0f; };
	// Pushed as far down as the noise goes, the points between would be under the hill
	const auto keys = maths::BeamKeyPoints(
	    {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 30.0f}, 4, wiggle, 0.0f, 0, [](float) { return -1.0f; }, hill);
	ASSERT_EQ(keys.size(), 4u);
	EXPECT_FLOAT_EQ(keys[1].y, 7.0f);
	EXPECT_FLOAT_EQ(keys[2].y, 7.0f);
	// But not the ends
	EXPECT_FLOAT_EQ(keys.front().y, 1.0f);
	EXPECT_FLOAT_EQ(keys.back().y, 1.0f);
}

TEST(BeamMaths, JointsFollowACurveThroughTheKeyPointsThickestAtTheMiddle)
{
	const std::vector<glm::vec3> keys {
	    {0.0f, 0.0f, 0.0f},  {1.0f, 2.0f, 10.0f}, {-1.0f, 3.0f, 20.0f},
	    {2.0f, 1.0f, 30.0f}, {0.0f, 0.0f, 40.0f}, {0.0f, 0.0f, 50.0f},
	};
	// Eleven joints put every other one on a key point
	const auto joints = maths::BeamJoints(keys, 11, 0.1f, 0.8f);
	ASSERT_EQ(joints.size(), 11u);
	for (size_t i = 0; i < keys.size(); ++i)
	{
		EXPECT_NEAR(glm::distance(joints[2 * i].position, keys[i]), 0.0f, 1e-3f) << i;
	}
	EXPECT_NEAR(joints.front().scale, 0.1f, k_Epsilon);
	EXPECT_NEAR(joints.back().scale, 0.1f, k_Epsilon);
	EXPECT_NEAR(joints[5].scale, 0.8f, k_Epsilon);
	// A tenth of the way: 1 - 0.8^2 of the way from the smallest to the largest
	EXPECT_NEAR(joints[1].scale, 0.1f + (0.36f * 0.7f), k_Epsilon);
}

TEST(BeamMaths, JointsAreTheGamesFloatForFloat)
{
	// Bit patterns from a step-by-step float model of the game's three-axis spline (its build, then its read in the
	// three-axis order) and of the joint scale, each operation rounded to a float as the game's FPU does
	const std::vector<glm::vec3> keys {
	    {0.0f, 0.0f, 0.0f},  {1.0f, 2.0f, 10.0f}, {-1.0f, 3.0f, 20.0f},
	    {2.0f, 1.0f, 30.0f}, {0.0f, 0.0f, 40.0f}, {0.0f, 0.0f, 50.0f},
	};
	const auto joints = maths::BeamJoints(keys, 20, 0.1f, 0.8f);
	ASSERT_EQ(joints.size(), 20u);
	struct Pinned
	{
		size_t joint;
		std::array<uint32_t, 3> position;
		uint32_t scale;
	};
	const std::array<Pinned, 4> pinned {{
	    {1, {0x3E602BEDu, 0x3E5DD50Au, 0x3F88C520u}, 0x3E755CE8u},
	    {7, {0xBF77BC54u, 0x40454F1Fu, 0x41944827u}, 0x3F4063D9u},
	    {13, {0x3FC738B8u, 0x3EBB9C33u, 0x420776EFu}, 0x3F3479F9u},
	    {18, {0xBDB7023Au, 0xBC5802A6u, 0x4243B9D8u}, 0x3E755CE2u},
	}};
	for (const auto& [joint, position, scale] : pinned)
	{
		for (glm::length_t axis = 0; axis < 3; ++axis)
		{
			EXPECT_EQ(std::bit_cast<uint32_t>(joints[joint].position[axis]), position[static_cast<size_t>(axis)])
			    << joint << " " << axis;
		}
		EXPECT_EQ(std::bit_cast<uint32_t>(joints[joint].scale), scale) << joint;
	}
}

TEST(BeamMaths, TheCurveLeavesAndArrivesFlat)
{
	// Evenly along a straight line the curve still bends at the ends, where it leaves and arrives flat
	const std::vector<glm::vec3> keys {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 2.0f}, {0.0f, 0.0f, 3.0f}};
	const auto joints = maths::BeamJoints(keys, 31, 1.0f, 1.0f);
	ASSERT_EQ(joints.size(), 31u);
	const float first = joints[1].position.z - joints[0].position.z;
	const float middle = joints[16].position.z - joints[15].position.z;
	EXPECT_LT(first, middle * 0.5f);
	EXPECT_NEAR(joints[15].position.z, 1.5f, k_Epsilon);
}

TEST(BeamMaths, ALoneJointIsNotANumberAsInTheGame)
{
	// One joint: its place on the curve is 0 x 1 / 0
	const std::vector<glm::vec3> keys {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 2.0f}};
	const auto joints = maths::BeamJoints(keys, 1, 0.1f, 0.8f);
	ASSERT_EQ(joints.size(), 1u);
	EXPECT_TRUE(std::isnan(joints[0].position.z));
	EXPECT_TRUE(std::isnan(joints[0].scale));
}

TEST(BeamMaths, FewerThanTwoKeyPointsMakeNoCurve)
{
	const maths::BeamWiggle wiggle {};
	EXPECT_TRUE(maths::BeamKeyPoints(
	                {}, {1.0f, 0.0f, 0.0f}, 1, wiggle, 0.0f, 0, [](float) { return 0.0f; }, k_FlatLand)
	                .empty());
	EXPECT_TRUE(maths::BeamJoints(std::vector<glm::vec3> {{1.0f, 2.0f, 3.0f}}, 4, 0.1f, 0.8f).empty());
}

TEST_F(SimpleBeamTest, IsRegisteredAndWaitsForTargetsUntilClosed)
{
	ASSERT_NE(psys::FindModifierFactory("UR_SimpleBeam"), nullptr);
	auto effect = Make(BeamFile(), {0.0f, 20.0f, 0.0f});
	effect->Step(k_Step);
	// Flag 2: it makes nothing, but the effect waits for targets
	EXPECT_EQ(effect->AtomCount(), 0u);
	EXPECT_FALSE(effect->Finished());
	// Once closing down, a target makes nothing and the effect ends
	effect->CloseDown();
	effect->AddTargetPoint({0.0f, 0.0f, 10.0f});
	effect->Step(k_Step);
	EXPECT_EQ(effect->AtomCount(), 0u);
	EXPECT_EQ(effect->TargetPointCount(), 1u);
	EXPECT_TRUE(effect->Finished());
}

TEST_F(SimpleBeamTest, EachTargetPointGetsThreeBeamsFromTheOrigin)
{
	const glm::vec3 origin(100.0f, 30.0f, 100.0f);
	const glm::vec3 target(110.0f, 5.0f, 120.0f);
	auto effect = Make(BeamFile(), origin);
	effect->AddTargetPoint(target);
	effect->Step(k_Step);
	effect->Step(k_Step);
	EXPECT_EQ(effect->TargetPointCount(), 0u);
	// The beam's own atom and the joints of its three beams
	EXPECT_EQ(effect->AtomCount(), 1u + (3u * 20u));
	std::vector<psys::Effect::DrawChain> chains;
	effect->CollectChains(1.0f, chains);
	ASSERT_EQ(chains.size(), 3u);
	for (size_t s = 0; s < chains.size(); ++s)
	{
		const auto& chain = chains[s];
		ASSERT_EQ(chain.joints.size(), 20u);
		ASSERT_NE(chain.collection, nullptr);
		// A chain's texture slides at SpeedV
		EXPECT_FLOAT_EQ(chain.collection->chainScrollRate, 1.0f);
		// The newest beam is number 0; the step laid it at the age before its end
		const int number = static_cast<int>(chains.size() - 1 - s);
		const float age = effect->CollectionAge(*chain.collection) - k_Step;
		const auto keys = maths::BeamKeyPoints(origin, target, 6, k_FileWiggle, age, number, psys::noise::SignedValueNoise,
		                                       [](glm::vec2) { return k_Land; });
		const auto laid = maths::BeamJoints(keys, 20, 0.1f, 0.8f);
		ASSERT_EQ(laid.size(), 20u);
		// The joints from the newest at the origin to the first made at the target
		for (size_t j = 0; j < laid.size(); ++j)
		{
			const auto* joint = chain.joints[chain.joints.size() - 1 - j].atom;
			ASSERT_NE(joint, nullptr);
			EXPECT_FLOAT_EQ(joint->position.x, laid[j].position.x) << s << " " << j;
			EXPECT_FLOAT_EQ(joint->position.y, laid[j].position.y) << s << " " << j;
			EXPECT_FLOAT_EQ(joint->position.z, laid[j].position.z) << s << " " << j;
			EXPECT_FLOAT_EQ(joint->ruleScale, laid[j].scale) << s << " " << j;
		}
		EXPECT_NEAR(glm::distance(chain.joints.front().atom->position, target), 0.0f, 1e-3f);
		EXPECT_NEAR(glm::distance(chain.joints.back().atom->position, origin), 0.0f, 1e-3f);
	}
	// No two beams wiggle alike
	const auto middle = [&chains](size_t chain) { return chains[chain].joints[10].atom->position; };
	EXPECT_GT(glm::distance(middle(0), middle(1)), k_Epsilon);
	EXPECT_GT(glm::distance(middle(1), middle(2)), k_Epsilon);
}

TEST_F(SimpleBeamTest, BeamsKeepAboveTheLandBetweenTheirEnds)
{
	auto effect = Make(BeamFile("2"), {0.0f, 20.0f, 0.0f});
	effect->AddTargetPoint({0.0f, 20.0f, 30.0f});
	effect->Step(k_Step);
	std::vector<psys::Effect::DrawChain> chains;
	effect->CollectChains(1.0f, chains);
	ASSERT_EQ(chains.size(), 3u);
	// The ends stay under the land, but the curve rises over it through the key points between
	const auto& joints = chains[0].joints;
	EXPECT_NEAR(joints.front().atom->position.y, 20.0f, k_Epsilon);
	float highest = 0.0f;
	for (const auto& joint : joints)
	{
		highest = std::max(highest, joint.atom->position.y);
	}
	EXPECT_GT(highest, k_Land + 2.0f);
}

TEST_F(SimpleBeamTest, ObjectsComeBeforePointsAndAreFollowedWhileAvailable)
{
	auto& registry = Reg();
	const auto object = registry.Create();
	registry.Assign<ecs::components::Transform>(object, glm::vec3(0.0f, 0.0f, 40.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto effect = Make(BeamFile(), {0.0f, 20.0f, 0.0f}, 2.0f);
	effect->AddTargetPoint({50.0f, 0.0f, 0.0f});
	effect->AddTarget(object);
	effect->Step(k_Step);
	// Both are taken in the same step, the object first: the newest atom (the point's) is the last
	EXPECT_EQ(effect->AtomCount(), 2u * (1u + (3u * 20u)));
	EXPECT_EQ(effect->TargetPointCount(), 0u);
	EXPECT_TRUE(effect->GetTargets().empty());
	registry.Get<ecs::components::Transform>(object).position = glm::vec3(5.0f, 0.0f, 40.0f);
	effect->SetOrigin({1.0f, 20.0f, 0.0f});
	effect->Step(k_Step);
	std::vector<psys::Effect::DrawChain> chains;
	effect->CollectChains(1.0f, chains);
	ASSERT_EQ(chains.size(), 6u);
	// To half the object's height above its place, from where the origin is now; the magnitude scales the joints
	const glm::vec3 end(5.0f, ecs::object::GetHeight(object) * 0.5f, 40.0f);
	EXPECT_NEAR(glm::distance(chains[0].joints.front().atom->position, end), 0.0f, 1e-3f);
	EXPECT_NEAR(glm::distance(chains[0].joints.back().atom->position, glm::vec3(1.0f, 20.0f, 0.0f)), 0.0f, 1e-3f);
	EXPECT_NEAR(chains[0].joints.front().atom->ruleScale, 0.2f, k_Epsilon);
	EXPECT_NEAR(glm::distance(chains[3].joints.front().atom->position, glm::vec3(50.0f, 0.0f, 0.0f)), 0.0f, 1e-3f);
	// Once the object has gone the beams keep to where it last was
	registry.Destroy(object);
	effect->Step(k_Step);
	chains.clear();
	effect->CollectChains(1.0f, chains);
	ASSERT_EQ(chains.size(), 6u);
	EXPECT_NEAR(glm::distance(chains[0].joints.front().atom->position, end), 0.0f, 1e-3f);
}

TEST_F(SimpleBeamTest, AnObjectGoneBeforeItsFirstStepKeepsTheAllocatorsFill)
{
	auto& registry = Reg();
	const auto object = registry.Create();
	registry.Assign<ecs::components::Transform>(object, glm::vec3(0.0f, 0.0f, 40.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto effect = Make(BeamFile(), {0.0f, 20.0f, 0.0f});
	effect->AddTarget(object);
	registry.Destroy(object);
	effect->Step(k_Step);
	std::vector<psys::Effect::DrawChain> chains;
	effect->CollectChains(1.0f, chains);
	ASSERT_EQ(chains.size(), 3u);
	// The beams run to the point whose three coordinates are 0xCDCDCDCD
	const float fill = std::bit_cast<float>(0xCDCDCDCDu);
	const auto end = chains[0].joints.front().atom->position;
	EXPECT_NEAR(end.x / fill, 1.0f, 1e-6f);
	EXPECT_NEAR(end.y / fill, 1.0f, 1e-6f);
	EXPECT_NEAR(end.z / fill, 1.0f, 1e-6f);
}

TEST_F(SimpleBeamTest, EachTargetDrawsOneRandomByteForEveryAtomItMakes)
{
	const game_random::testing::ScopedState state;
	std::vector<uint32_t> draws;
	game_random::testing::SetGameRand(
	    [&draws](uint32_t n) {
		    draws.push_back(n);
		    return 0u;
	    },
	    [](float) { return 0.0f; });
	auto effect = Make(BeamFile(), {0.0f, 20.0f, 0.0f}, 1.0f, game_random::psys::NetGameType::Synced);
	effect->AddTargetPoint({0.0f, 0.0f, 10.0f});
	effect->Step(k_Step);
	effect->Step(k_Step);
	// The carrier and its 3 x 20 joints, each Rand(0x100) on the effect's stream; laying the beams draws nothing
	ASSERT_EQ(draws.size(), 61u);
	EXPECT_TRUE(std::ranges::all_of(draws, [](uint32_t n) { return n == 0x100u; }));
}
