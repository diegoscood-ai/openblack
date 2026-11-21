/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The order in which particle rules make their random draws: each draw of the synced stream is answered from a list,
// so a rule that takes its draws in another order, or makes another number of them, gives other values.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "Common/GameRandomTesting.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleRegistry.h"
#include "Particles/ParticleSpellFile.h"

using namespace openblack;

namespace
{
constexpr uint32_t k_FloatDraw = 0xFFFFu;   ///< a FloatRand on the synced stream draws GameRand(0xFFFF)
constexpr uint32_t k_AtomByteDraw = 0x100u; ///< every new atom first draws its own random byte
constexpr float k_Step = 0.1f;

/// The draws of the synced stream, each answered by the next of `answers` (0 once they run out)
struct Draws
{
	std::vector<uint32_t> asked;
	std::vector<uint32_t> answers;
	void Hook()
	{
		game_random::testing::SetGameRand(
		    [this](uint32_t n) {
			    const auto answer = asked.size() < answers.size() ? answers[asked.size()] : 0u;
			    asked.push_back(n);
			    return answer;
		    },
		    [](float) { return 0.0f; });
	}
};

/// A particle's float of the answer u up to x, as the effects compute it: (u x scale) x x
float FloatOf(uint32_t u, float x)
{
	float r = static_cast<float>(u);
	r = r * game_random::FloatRandScale();
	r = r * x;
	return r;
}

/// Every atom the test creator makes, in order
std::vector<particles::Atom*>& Made()
{
	static std::vector<particles::Atom*> s_made;
	return s_made;
}

/// A creator that only records the atoms it makes
struct RecordingCreator final: particles::Creator
{
	void InitAtom(particles::Effect& /*effect*/, particles::Atom& atom) const override { Made().push_back(&atom); }
};

constexpr const char* k_CreatorClass = "ParticleDrawOrderTestCreator";

std::unique_ptr<particles::Creator> MakeRecordingCreator(const particles::Object& object)
{
	auto creator = std::make_unique<RecordingCreator>();
	particles::ReadCreatorProperties(object, *creator);
	return creator;
}

const std::string k_Header = "BEGINPROPERTIES\n"
                             "PROPERTY DeleteOnCloseDown BOOL 0\n"
                             "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
                             "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
                             "ENDPROPERTIES\n";

const std::string k_Creator = std::string("BEGINCLASS ") + k_CreatorClass + " C0\nBEGINPROPERTIES\nENDPROPERTIES\nENDCLASS\n";

/// An effect of the text at the origin, its draws on the synced stream
std::unique_ptr<particles::Effect> MakeEffect(const std::string& text, glm::vec3 origin = glm::vec3(0.0f))
{
	auto file = particles::File::Parse(text, "SF_DrawOrderTest");
	EXPECT_TRUE(file.has_value());
	if (!file.has_value())
	{
		return nullptr;
	}
	return std::make_unique<particles::Effect>(std::make_shared<const particles::File>(std::move(*file)), origin, 1.0f,
	                                           game_random::particles::NetGameType::Synced);
}

class ParticleDrawOrder: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (particles::FindCreatorFactory(k_CreatorClass) == nullptr)
		{
			particles::RegisterCreator(k_CreatorClass, MakeRecordingCreator);
		}
		Made().clear();
		draws.Hook();
	}
	void TearDown() override { Made().clear(); }

	const game_random::testing::ScopedState state;
	Draws draws;
};
} // namespace

TEST_F(ParticleDrawOrder, theWillowWispDrawsThePointInTheBallBeforeTheSpeed)
{
	// One atom in the first step (two are due at 20 a second for 0.1 s, and the rule makes one fewer than are due),
	// with no radius, no speed of its own and no parent: its velocity is the random one alone
	auto effect = MakeEffect(k_Header + k_Creator +
	                         "BEGINCLASS UR_WillowWisp W\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
	                         "PROPERTY NextGroups ARRAY SIZE 0\nPROPERTY PCreator PERSIS_PNTR C0\n"
	                         "PROPERTY MaxAtoms INTEGER 20\nPROPERTY DieAge FLOAT 1\nPROPERTY RandomSpeed FLOAT 3\n"
	                         "ENDPROPERTIES\nENDCLASS\n");
	ASSERT_NE(effect, nullptr);
	draws.answers = {0u, 0x4000u, 0x8000u, 0xC000u, 0x2000u};
	effect->Step(k_Step);
	// the atom's byte, the three of the point (inside the ball at once), then the speed
	ASSERT_EQ(draws.asked, (std::vector<uint32_t> {k_AtomByteDraw, k_FloatDraw, k_FloatDraw, k_FloatDraw, k_FloatDraw}));
	ASSERT_EQ(Made().size(), 1u);
	glm::vec3 ball(FloatOf(0x4000u, 2.0f) - 1.0f, FloatOf(0x8000u, 2.0f) - 1.0f, FloatOf(0xC000u, 2.0f) - 1.0f);
	ball *= FloatOf(0x2000u, 3.0f);
	EXPECT_EQ(Made()[0]->velocity, ball);
}

TEST_F(ParticleDrawOrder, theSphereSurfaceTracerDrawsThreeAnglesThetaFirst)
{
	// One atom on the sphere: its first step draws theta, phi and a third angle the rule does not use, and later steps
	// draw nothing
	auto effect = MakeEffect(k_Header + k_Creator +
	                         "BEGINCLASS CreateRuleAnAtom A\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
	                         "PROPERTY NextGroups ARRAY SIZE 0\nPROPERTY PCreator PERSIS_PNTR C0\nENDPROPERTIES\nENDCLASS\n"
	                         "BEGINCLASS UR_SphereSurfaceTracer T\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
	                         "PROPERTY ThetaSpeed FLOAT 0\nPROPERTY PhiSpeed FLOAT 0\nPROPERTY SphereRadius FLOAT 2\n"
	                         "ENDPROPERTIES\nENDCLASS\n");
	ASSERT_NE(effect, nullptr);
	draws.answers = {0u, 0x4000u, 0x8000u, 0xC000u};
	effect->Step(k_Step);
	effect->Step(k_Step);
	ASSERT_EQ(draws.asked, (std::vector<uint32_t> {k_AtomByteDraw, k_FloatDraw, k_FloatDraw, k_FloatDraw}));
	ASSERT_EQ(Made().size(), 1u);
	const auto& data = Made()[0]->data;
	ASSERT_EQ(data.size(), 1u);
	const float twoPi = 2.0f * std::numbers::pi_v<float>;
	EXPECT_EQ(data.begin()->second.x, FloatOf(0x4000u, twoPi)); // theta
	EXPECT_EQ(data.begin()->second.y, FloatOf(0x8000u, twoPi)); // phi
}

TEST_F(ParticleDrawOrder, theWillowWispAddingTheCastVelocityStartsFromTheParentAcrossTheGround)
{
	// With a random radius the atom is first put beside the origin; adding the cast velocity starts again from the
	// origin's x and z, so the radius's draws are made and their offset dropped
	const glm::vec3 origin(10.0f, 2.0f, 20.0f);
	auto effect = MakeEffect(k_Header + k_Creator +
	                             "BEGINCLASS UR_WillowWisp W\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
	                             "PROPERTY NextGroups ARRAY SIZE 0\nPROPERTY PCreator PERSIS_PNTR C0\n"
	                             "PROPERTY MaxAtoms INTEGER 20\nPROPERTY DieAge FLOAT 1\nPROPERTY RandomSpeed FLOAT 3\n"
	                             "PROPERTY RandomRadiusMin FLOAT 1\nPROPERTY RandomRadiusMax FLOAT 4\n"
	                             "PROPERTY AddCastVelToInitPos BOOL 1\nENDPROPERTIES\nENDCLASS\n",
	                         origin);
	ASSERT_NE(effect, nullptr);
	// the atom's byte, the angle and the radius, the point in the ball, the speed
	draws.answers = {0u, 0x4000u, 0x8000u, 0x4000u, 0x8000u, 0xC000u, 0x2000u};
	effect->Step(k_Step);
	ASSERT_EQ(draws.asked.size(), 7u);
	ASSERT_EQ(Made().size(), 1u);
	glm::vec3 velocity(FloatOf(0x4000u, 2.0f) - 1.0f, FloatOf(0x8000u, 2.0f) - 1.0f, FloatOf(0xC000u, 2.0f) - 1.0f);
	velocity *= FloatOf(0x2000u, 3.0f);
	EXPECT_EQ(Made()[0]->velocity, velocity);
	EXPECT_EQ(Made()[0]->position, origin + velocity * k_Step);
}
