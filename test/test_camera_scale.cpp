/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// UR_ScaleByCameraDist (Particles/Rules/CameraScale): its maths, and the rule on a small synthetic effect with a fake
// camera. The expected bits are the original's, run with the same inputs: the rule, a camera and one atom.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <bit>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/Camera.h"
#include "Common/GameRandomTesting.h"
#include "Locator.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysRegistry.h"
#include "Particles/Rules/CameraScale.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
/// SF_FailedCast's root: the rule first, then the one point atom it scales
constexpr std::string_view k_EffectText = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 1
PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY MaxSpellAge FLOAT -1
ENDPROPERTIES
BEGINCLASS ParticlePointCreator ParticlePointCreator0
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS UR_ScaleByCameraDist UR_ScaleByCameraDist0
BEGINPROPERTIES
PROPERTY CameraDistMax FLOAT 100
PROPERTY CameraDistMin FLOAT 10
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 0
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY ScaleAtCameraDistMax FLOAT 1
PROPERTY ScaleAtCameraDistMin FLOAT 0.1
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleAnAtom CreateRuleAnAtom0
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 0
PROPERTY NextGroups ARRAY SIZE 0
PROPERTY OffsetX FLOAT 0
PROPERTY OffsetY FLOAT 0
PROPERTY OffsetZ FLOAT 0
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator0
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY SoundOfCreate SOUND_ACTION NO_SOUND LOOPING 0 ONLYONE 0 SOFTRELEASE 0 USESURFACE 0
ENDPROPERTIES
ENDCLASS
)";

/// The effect at a point, with the camera at the world's origin
class CameraScaleRule: public ::testing::Test
{
	const game_random::testing::ScopedState _random;
	const test::RestoreService<Locator::camera> _camera;

protected:
	void SetUp() override { Locator::camera::emplace(glm::vec3(0.0f)).SetOrigin(glm::vec3(0.0f)); }

	static std::unique_ptr<Effect> Make(const glm::vec3& at, int player)
	{
		auto file = File::Parse(std::string(k_EffectText), "SF_CameraScaleTest");
		EXPECT_TRUE(file.has_value());
		auto effect = std::make_unique<Effect>(std::make_shared<const File>(std::move(*file)), at, 1.0f);
		effect->SetPlayer(player);
		return effect;
	}

	static float RootScale(const Effect& effect)
	{
		std::vector<Effect::DrawAtom> atoms;
		effect.Collect(1.0f, atoms, Creator::Kind::Point);
		EXPECT_EQ(atoms.size(), 1u);
		return atoms.empty() ? 0.0f : atoms.front().atom->ruleScale;
	}
};

uint32_t ScaleBits(const camera_scale::Range& range, const glm::vec3& camera, const glm::vec3& atom)
{
	return std::bit_cast<uint32_t>(camera_scale::ScaleAtDistance(range, camera_scale::CameraDistance(camera, atom)));
}
} // namespace

TEST(CameraScale, DefaultRangeMatchesTheOriginal)
{
	const camera_scale::Range range {};
	const glm::vec3 camera(0.0f);
	struct Case
	{
		glm::vec3 atom;
		uint32_t scale;
	};
	const Case cases[] = {
	    {{0.0f, 0.0f, 0.0f}, 0x3DCCCCCDu},     // at the camera: the near scale
	    {{5.0f, 0.0f, 0.0f}, 0x3DCCCCCDu},     // nearer than the near distance
	    {{10.0f, 0.0f, 0.0f}, 0x3DCCCCCDu},    // at it: the line's start
	    {{0.0f, 0.0f, 10.0f}, 0x3DCCCCCDu},    //
	    {{3.0f, 4.0f, 12.0f}, 0x3E051EB8u},    // 13 m
	    {{17.3f, -2.25f, 31.7f}, 0x3EB94261u}, //
	    {{40.0f, 30.0f, 0.0f}, 0x3F000000u},   // 50 m: 0.5
	    {{55.5f, 0.1f, 0.2f}, 0x3F0E14C7u},    //
	    {{99.9f, 0.0f, 0.0f}, 0x3F7FBE77u},    //
	    {{100.0f, 0.0f, 0.0f}, 0x3F800000u},   // at the far distance
	    {{0.0f, 100.0f, 0.0f}, 0x3F800000u},   //
	    {{120.0f, 0.0f, 0.0f}, 0x3F800000u},   // past it: the far scale
	    {{0.001f, 0.002f, 0.0f}, 0x3DCCCCCDu}, //
	};
	for (const auto& c : cases)
	{
		EXPECT_EQ(ScaleBits(range, camera, c.atom), c.scale) << c.atom.x << ", " << c.atom.y << ", " << c.atom.z;
	}
	EXPECT_EQ(ScaleBits(range, {1000.25f, 37.5f, 2000.75f}, {1012.5f, 80.125f, 2033.0f}), 0x3F0C617Eu);
}

TEST(CameraScale, OtherRangesMatchTheOriginal)
{
	// the far distance below the near one: the near test comes first
	const camera_scale::Range inverted {.distMin = 100.0f, .distMax = 10.0f, .scaleAtMin = 1.0f, .scaleAtMax = 0.1f};
	EXPECT_EQ(ScaleBits(inverted, glm::vec3(0.0f), {30.0f, 0.0f, 0.0f}), 0x3F800000u);
	EXPECT_EQ(ScaleBits(inverted, glm::vec3(0.0f), {5.0f, 0.0f, 0.0f}), 0x3F800000u);
	EXPECT_EQ(ScaleBits(inverted, glm::vec3(0.0f), {150.0f, 0.0f, 0.0f}), 0x3DCCCCCDu);
	// a scale that shrinks with the distance
	const camera_scale::Range shrinking {.distMin = 2.5f, .distMax = 7.75f, .scaleAtMin = 3.0f, .scaleAtMax = 0.25f};
	EXPECT_EQ(ScaleBits(shrinking, {1.0f, 2.0f, 3.0f}, {4.5f, 6.25f, 2.0f}), 0x3FB06FD4u);
}

TEST(CameraScale, RunsOnlyForAPlayersEffectWithTwoDistances)
{
	EXPECT_TRUE(camera_scale::Runs({}, true));
	EXPECT_FALSE(camera_scale::Runs({}, false));
	EXPECT_FALSE(camera_scale::Runs({.distMin = 50.0f, .distMax = 50.0f}, true));
	EXPECT_TRUE(camera_scale::Runs({.distMin = 100.0f, .distMax = 10.0f}, true));
	const float nan = std::numeric_limits<float>::quiet_NaN();
	EXPECT_FALSE(camera_scale::Runs({.distMin = nan, .distMax = 10.0f}, true));
}

TEST(CameraScale, NotANumberDistanceTakesTheNearScale)
{
	const float nan = std::numeric_limits<float>::quiet_NaN();
	EXPECT_EQ(camera_scale::ScaleAtDistance({}, nan), 0.1f);
}

TEST(CameraScale, TheGameRegistersTheRule)
{
	EXPECT_NE(FindModifierFactory("UR_ScaleByCameraDist"), nullptr);
}

TEST_F(CameraScaleRule, APlayersEffectScalesItsAtomsByTheirDistance)
{
	auto effect = Make({17.3f, -2.25f, 31.7f}, 0);
	// the first step makes the atom after the rule has run: the rule scale it is born with
	effect->Step(0.1f);
	EXPECT_EQ(RootScale(*effect), 1.0f);
	effect->Step(0.1f);
	EXPECT_EQ(std::bit_cast<uint32_t>(RootScale(*effect)), 0x3EB94261u);
	// it follows the camera
	Locator::camera::value().SetOrigin({0.0f, 0.0f, 200.0f});
	effect->Step(0.1f);
	EXPECT_EQ(RootScale(*effect), 1.0f);
}

TEST_F(CameraScaleRule, AnEffectNoPlayerOwnsKeepsItsScale)
{
	auto effect = Make({3.0f, 4.0f, 0.0f}, -1);
	for (int step = 0; step < 3; ++step)
	{
		effect->Step(0.1f);
	}
	EXPECT_EQ(RootScale(*effect), 1.0f);
	// detached on its first step: a player set later changes nothing
	effect->SetPlayer(0);
	effect->Step(0.1f);
	EXPECT_EQ(RootScale(*effect), 1.0f);
}
