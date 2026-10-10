/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// ER_GlintsOnTarget (Particles/ParticleGlintRules.cpp) run on small effects with fake targets, and the glints' pulse
// and size (Particles/GlintMaths). The game's list registers the rule (psys::RegisterAll).

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Common/GameRandomTesting.h"
#include "ECS/Systems/GlintTargetsInterface.h"
#include "Locator.h"
#include "Particles/GlintMaths.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysRegistry.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::particles;

namespace
{
const auto k_Seed = static_cast<entt::entity>(5);
const auto k_Other = static_cast<entt::entity>(6);

/// Objects with the points of their model, their scale, and whether they are gone. Points from `holeFrom` on are
/// counted but have no place
class FakeTargets final: public ecs::systems::GlintTargetsInterface
{
public:
	struct Target
	{
		std::vector<glm::vec3> points;
		float scale {1.0f};
		bool gone {false};
		uint32_t holeFrom {std::numeric_limits<uint32_t>::max()};
	};

	[[nodiscard]] std::optional<glm::vec3> ObjectPosition(entt::entity object) const override
	{
		const auto* target = Find(object);
		return target != nullptr && !target->gone ? std::optional(glm::vec3(0.0f)) : std::nullopt;
	}
	[[nodiscard]] uint32_t TargetPointCount(entt::entity object) const override
	{
		++countCalls;
		const auto* target = Find(object);
		return target != nullptr ? static_cast<uint32_t>(target->points.size()) : 0;
	}
	[[nodiscard]] std::optional<glm::vec3> TargetPoint(entt::entity object, uint32_t index) const override
	{
		++pointCalls;
		const auto* target = Find(object);
		if (target == nullptr || index >= target->points.size() || index >= target->holeFrom)
		{
			return std::nullopt;
		}
		return target->points[index];
	}
	[[nodiscard]] float TargetScale(entt::entity object) const override
	{
		const auto* target = Find(object);
		return target != nullptr ? target->scale : 1.0f;
	}

	std::map<entt::entity, Target> objects;
	mutable uint32_t countCalls {0};
	mutable uint32_t pointCalls {0};

private:
	[[nodiscard]] const Target* Find(entt::entity object) const
	{
		const auto it = objects.find(object);
		return it != objects.end() ? &it->second : nullptr;
	}
};

/// A glints file like the game's (SF_CreatureSpellFreezeOnHolder): a point creator for the parents, a sprite creator
/// for the glints, and the rule with these properties
std::string GlintFile(const std::string& parentCreator = "ParticlePointCreator0",
                      const std::string& glintCreator = "ParticleSpriteCreator0", const std::string& maxAtoms = "3",
                      const std::string& ageMaxSize = "0.15", const std::string& ageZeroSize = "0.9")
{
	std::string text;
	text += "BEGINPROPERTIES\n";
	text += "PROPERTY DeleteOnCloseDown BOOL 1\n";
	text += "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n";
	text += "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n";
	text += "PROPERTY MaxSpellAge FLOAT -1\n";
	text += "ENDPROPERTIES\n";
	text += "BEGINCLASS ParticlePointCreator ParticlePointCreator0\n";
	text += "BEGINPROPERTIES\n";
	text += "PROPERTY ColorA INTEGER 255\n";
	text += "PROPERTY ColorB INTEGER 255\n";
	text += "PROPERTY ColorG INTEGER 255\n";
	text += "PROPERTY ColorR INTEGER 255\n";
	text += "PROPERTY InitialScale FLOAT 0.8\n";
	text += "PROPERTY LoopAnim BOOL 1\n";
	text += "ENDPROPERTIES\n";
	text += "ENDCLASS\n";
	text += "BEGINCLASS ParticleSpriteCreator ParticleSpriteCreator0\n";
	text += "BEGINPROPERTIES\n";
	text += "PROPERTY ColorA INTEGER 255\n";
	text += "PROPERTY ColorB INTEGER 255\n";
	text += "PROPERTY ColorG INTEGER 220\n";
	text += "PROPERTY ColorR INTEGER 180\n";
	text += "PROPERTY FrameRate FLOAT 0\n";
	text += "PROPERTY InitialScale FLOAT 1.5\n";
	text += "PROPERTY NumFrames INTEGER 16\n";
	text += "PROPERTY NumSpritesPerRow INTEGER 8\n";
	text += "PROPERTY PlayAnim BOOL 0\n";
	text += "PROPERTY RandomiseFrameDirection BOOL 1\n";
	text += "PROPERTY RandomiseInitFrame BOOL 1\n";
	text += "PROPERTY RandomiseScale BOOL 0\n";
	text += "PROPERTY FileOffset INTEGER 48\n";
	text += "PROPERTY TextureFileName STRING .\\Data\\Textures\\S_SpriteSheet3.raw\n";
	text += "PROPERTY UseAdditiveAlpha BOOL 1\n";
	text += "ENDPROPERTIES\n";
	text += "ENDCLASS\n";
	text += "BEGINCLASS ER_GlintsOnTarget ER_GlintsOnTarget0\n";
	text += "BEGINPROPERTIES\n";
	text += "PROPERTY AtomAgeMaxSize FLOAT " + ageMaxSize + "\n";
	text += "PROPERTY AtomAgeZeroSize FLOAT " + ageZeroSize + "\n";
	text += "PROPERTY Condition PERSIS_PNTR NULL_STRING\n";
	text += "PROPERTY GlintCreator PERSIS_PNTR " + glintCreator + "\n";
	text += "PROPERTY GlintGroup INTEGER 1\n";
	text += "PROPERTY Group INTEGER 0\n";
	text += "PROPERTY MaxAlpha INTEGER 80\n";
	text += "PROPERTY MaxAtoms INTEGER " + maxAtoms + "\n";
	text += "PROPERTY NextGroups ARRAY SIZE 0\n";
	text += "PROPERTY PCreator PERSIS_PNTR " + parentCreator + "\n";
	text += "PROPERTY PulseMagnitude FLOAT 1\n";
	text += "PROPERTY PulseSpeed FLOAT 1\n";
	text += "PROPERTY RemoveOnCloseDown BOOL 0\n";
	text += "ENDPROPERTIES\n";
	text += "ENDCLASS\n";
	return text;
}

/// Fake targets in the locator; the previous targets come back after
class ParticleGlintsTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		targets = &static_cast<FakeTargets&>(Locator::glintTargets::emplace<FakeTargets>());
		// A seed with four points of its model, drawn at a scale of 2
		targets->objects[k_Seed] = FakeTargets::Target {
		    .points = {{10.0f, 5.0f, 10.0f}, {11.0f, 5.0f, 10.0f}, {10.0f, 6.0f, 10.0f}, {10.0f, 5.0f, 11.0f}},
		    .scale = 2.0f,
		};
	}

	static std::unique_ptr<psys::Effect> Make(const std::string& text,
	                                          game_random::psys::NetGameType type = game_random::psys::NetGameType::Local)
	{
		auto file = psys::File::Parse(text, "test");
		EXPECT_TRUE(file.has_value());
		// away from every point, so that a glint left where it was made is on none
		return std::make_unique<psys::Effect>(std::make_shared<const psys::File>(std::move(*file)),
		                                      glm::vec3(0.0f, 50.0f, 0.0f), 1.0f, type);
	}

	static std::vector<psys::Effect::DrawAtom> Drawn(const psys::Effect& effect, psys::Creator::Kind kind)
	{
		std::vector<psys::Effect::DrawAtom> atoms;
		effect.Collect(1.0f, atoms, kind);
		return atoms;
	}

	FakeTargets* targets {nullptr};

private:
	test::RestoreService<Locator::glintTargets> _restoreTargets;
};

/// Every range asked of the synced stream, in order; Rand(n) answers `answers[n]`, 0 for the others
struct DrawLog
{
	std::vector<uint32_t> asked;
	std::map<uint32_t, uint32_t> answers;
};

void Record(DrawLog& log)
{
	game_random::testing::SetGameRand(
	    [&log](uint32_t n) -> uint32_t {
		    log.asked.push_back(n);
		    const auto it = log.answers.find(n);
		    return it != log.answers.end() ? it->second : 0;
	    },
	    nullptr);
}
} // namespace

TEST(ParticleGlintRules, TheGameRegistersTheRule)
{
	EXPECT_NE(psys::FindModifierFactory("ER_GlintsOnTarget"), nullptr);
}

TEST_F(ParticleGlintsTest, NoMoreThanTheMostOnThePointsOfTheModelAtAFixedAlpha)
{
	auto effect = Make(GlintFile());
	effect->AddTarget(k_Seed);
	const auto& points = targets->objects[k_Seed].points;
	for (int step = 0; step < 20; ++step)
	{
		effect->Step(0.1f);
		const auto glints = Drawn(*effect, psys::Creator::Kind::Sprite);
		EXPECT_LE(glints.size(), 3u);
		for (const auto& glint : glints)
		{
			ASSERT_NE(glint.atom, nullptr);
			EXPECT_EQ(glint.atom->colour[3], 80);
			EXPECT_NEAR(glint.alpha, 80.0f, 1e-3f);
			EXPECT_EQ(glint.colour[0], 180);
			// On one of the points, at the seed's scale times the sprite's
			const bool onAPoint =
			    std::ranges::any_of(points, [&](const glm::vec3& point) { return point == glint.atom->position; });
			EXPECT_TRUE(onAPoint);
			EXPECT_FLOAT_EQ(glint.atom->baseScale, 2.0f * 1.5f);
			// Grown and pulsing: at most full size times 1.5
			EXPECT_LE(glint.atom->ruleScale, 1.5f);
			EXPECT_GE(glint.atom->ruleScale, 0.0f);
			// Deleted once older than 0.9 (the age read in the step, before the step's time is added)
			EXPECT_LE(effect->AtomAge(*glint.atom), 0.9f + 0.1f + 1e-4f);
		}
	}
	EXPECT_FALSE(Drawn(*effect, psys::Creator::Kind::Sprite).empty());
	// One parent, never drawn as a sprite
	EXPECT_EQ(Drawn(*effect, psys::Creator::Kind::Point).size(), 1u);
}

TEST_F(ParticleGlintsTest, TheyGoWithTheirObjectUnstepped)
{
	auto effect = Make(GlintFile());
	effect->AddTarget(k_Seed);
	for (int step = 0; step < 5; ++step)
	{
		effect->Step(0.1f);
	}
	EXPECT_FALSE(Drawn(*effect, psys::Creator::Kind::Sprite).empty());
	targets->objects[k_Seed].gone = true;
	const auto counted = targets->countCalls;
	const auto placed = targets->pointCalls;
	effect->Step(0.1f);
	EXPECT_TRUE(Drawn(*effect, psys::Creator::Kind::Sprite).empty());
	EXPECT_EQ(effect->AtomCount(), 0u);
	// The parent went first: its glints were neither made nor moved
	EXPECT_EQ(targets->countCalls, counted);
	EXPECT_EQ(targets->pointCalls, placed);
}

TEST_F(ParticleGlintsTest, ParentsAreWalkedNewestFirst)
{
	// 4 glints over an age of 2: 2 a second, 0.5 due every quarter second
	targets->objects[k_Other] = FakeTargets::Target {
	    .points = {{0.0f, 1.0f, 0.0f},
	               {0.0f, 2.0f, 0.0f},
	               {0.0f, 3.0f, 0.0f},
	               {0.0f, 4.0f, 0.0f},
	               {0.0f, 5.0f, 0.0f},
	               {0.0f, 6.0f, 0.0f},
	               {0.0f, 7.0f, 0.0f}},
	    .scale = 0.5f,
	};
	DrawLog log;
	log.answers = {{4u, 3u}, {7u, 6u}};
	Record(log);
	auto effect = Make(GlintFile("ParticlePointCreator0", "ParticleSpriteCreator0", "4", "0.5", "2"),
	                   game_random::psys::NetGameType::Synced);
	// One object a step, each a parent; a step of no time makes no glint
	effect->AddTarget(k_Seed);
	effect->Step(0.0f);
	effect->AddTarget(k_Other);
	effect->Step(0.0f);
	EXPECT_EQ(log.asked, (std::vector<uint32_t> {0x100u, 0x100u}));
	EXPECT_TRUE(Drawn(*effect, psys::Creator::Kind::Sprite).empty());
	log.asked.clear();
	effect->Step(0.25f);
	// The newer parent's glint first: its atom, the sprite's first frame and direction, then its point; then the older's
	EXPECT_EQ(log.asked, (std::vector<uint32_t> {0x100u, 16u, 0x100u, 7u, 0x100u, 16u, 0x100u, 4u}));
	const auto glints = Drawn(*effect, psys::Creator::Kind::Sprite);
	ASSERT_EQ(glints.size(), 2u);
	const auto on = [&glints](const glm::vec3& point, float scale) {
		return std::ranges::any_of(
		    glints, [&](const auto& glint) { return glint.atom->position == point && glint.atom->baseScale == scale * 1.5f; });
	};
	EXPECT_TRUE(on(glm::vec3(0.0f, 7.0f, 0.0f), 0.5f));
	EXPECT_TRUE(on(glm::vec3(10.0f, 5.0f, 11.0f), 2.0f));
}

TEST_F(ParticleGlintsTest, WithoutAParentCreatorTheRuleDetachesAndTakesNoObject)
{
	auto effect = Make(GlintFile("NULL_STRING"));
	effect->AddTarget(k_Seed);
	effect->Step(0.1f);
	effect->Step(0.1f);
	EXPECT_EQ(effect->AtomCount(), 0u);
	EXPECT_EQ(effect->GetTargets().size(), 1u);
	EXPECT_TRUE(effect->Finished());
}

TEST_F(ParticleGlintsTest, WithoutAGlintCreatorOnlyTheParentIsMade)
{
	auto effect = Make(GlintFile("ParticlePointCreator0", "NULL_STRING"));
	effect->AddTarget(k_Seed);
	for (int step = 0; step < 5; ++step)
	{
		effect->Step(0.1f);
	}
	EXPECT_EQ(effect->AtomCount(), 1u);
	EXPECT_TRUE(effect->GetTargets().empty());
	EXPECT_EQ(targets->countCalls, 0u);
	EXPECT_EQ(targets->pointCalls, 0u);
}

TEST_F(ParticleGlintsTest, APointWithNoPlaceLeavesTheGlintWhereItIs)
{
	// Four points counted, the last one with no place
	targets->objects[k_Seed].holeFrom = 3;
	DrawLog log;
	log.answers = {{4u, 3u}};
	Record(log);
	auto effect = Make(GlintFile(), game_random::psys::NetGameType::Synced);
	effect->AddTarget(k_Seed);
	effect->Step(0.1f);
	auto glints = Drawn(*effect, psys::Creator::Kind::Sprite);
	ASSERT_EQ(glints.size(), 1u);
	const auto where = glints.front().atom->position;
	for (const auto& point : targets->objects[k_Seed].points)
	{
		EXPECT_NE(where, point);
	}
	// The model moves; the glint does not
	for (auto& point : targets->objects[k_Seed].points)
	{
		point += glm::vec3(100.0f, 0.0f, 0.0f);
	}
	effect->Step(0.1f);
	glints = Drawn(*effect, psys::Creator::Kind::Sprite);
	ASSERT_EQ(glints.size(), 1u);
	EXPECT_EQ(glints.front().atom->position, where);
}

TEST_F(ParticleGlintsTest, AStepOfNoTimeTakesTheObjectButMakesNoGlint)
{
	auto effect = Make(GlintFile());
	effect->AddTarget(k_Seed);
	effect->Step(0.0f);
	effect->Step(0.0f);
	EXPECT_TRUE(effect->GetTargets().empty());
	EXPECT_EQ(effect->AtomCount(), 1u);
	EXPECT_TRUE(Drawn(*effect, psys::Creator::Kind::Sprite).empty());
	// The points were counted, with nothing due
	EXPECT_EQ(targets->countCalls, 2u);
}

TEST(GlintMaths, TheRateIsTheMostOverTheAgeAtWhichTheyVanish)
{
	EXPECT_FLOAT_EQ(maths::GlintRate(3, 0.9f), 3.0f / 0.9f);
	EXPECT_FALSE(maths::GlintRate(0, 2.0f) > 0.0f);
	EXPECT_FALSE(maths::GlintRate(3, -1.0f) > 0.0f);
	EXPECT_FALSE(maths::GlintRate(3, std::nanf("")) > 0.0f);
}

TEST(GlintMaths, ThePulseIsNeverBelowZero)
{
	EXPECT_EQ(maths::GlintPulse(0.0f, 1.0f, 1.0f), 1.5f);
	EXPECT_NEAR(maths::GlintPulse(0.25f, 1.0f, 1.0f), 1.0f, 1e-6f);
	EXPECT_NEAR(maths::GlintPulse(0.5f, 1.0f, 1.0f), 0.5f, 1e-6f);
	// cos(pi) x 4 x 0.5 + 1 is -1: none
	EXPECT_EQ(maths::GlintPulse(0.5f, 1.0f, 4.0f), 0.0f);
	EXPECT_EQ(maths::GlintPulse(std::nanf(""), 1.0f, 1.0f), 0.0f);
}

TEST(GlintMaths, TheSizeGrowsThenShrinksBetweenZeroAndOne)
{
	EXPECT_EQ(maths::GlintSize(0.0f, 0.15f, 0.9f), 0.0f);
	EXPECT_FLOAT_EQ(maths::GlintSize(0.075f, 0.15f, 0.9f), 0.075f / 0.15f);
	EXPECT_EQ(maths::GlintSize(0.15f, 0.15f, 0.9f), 1.0f);
	EXPECT_FLOAT_EQ(maths::GlintSize(0.525f, 0.15f, 0.9f), 1.0f - ((0.525f - 0.15f) / (0.9f - 0.15f)));
	// Past the age of no size, and an age that is not a number: none
	EXPECT_EQ(maths::GlintSize(1.0f, 0.15f, 0.9f), 0.0f);
	EXPECT_EQ(maths::GlintSize(std::nanf(""), 0.15f, 0.9f), 0.0f);
	// More than full size is full size
	EXPECT_EQ(maths::GlintSize(2.0f, 1.0f, 0.5f), 1.0f);
}
