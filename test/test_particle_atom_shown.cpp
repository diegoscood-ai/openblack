/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <memory>
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
constexpr float k_Step = 0.1f;

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

constexpr const char* k_CreatorClass = "ParticleAtomShownTestCreator";

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

/// A willow wisp making one atom in its first step, drawn on its first update or not
std::string WillowWisp(bool drawOnFirstUpdate)
{
	return "BEGINCLASS UR_WillowWisp W\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\nPROPERTY NextGroups ARRAY SIZE 0\n"
	       "PROPERTY PCreator PERSIS_PNTR C0\nPROPERTY MaxAtoms INTEGER 20\nPROPERTY DieAge FLOAT 1\n"
	       "PROPERTY DrawOnFirstUpdate BOOL " +
	       std::string(drawOnFirstUpdate ? "1" : "0") + "\nENDPROPERTIES\nENDCLASS\n";
}

std::unique_ptr<particles::Effect> MakeEffect(const std::string& text)
{
	auto file = particles::File::Parse(text, "SF_AtomShownTest");
	EXPECT_TRUE(file.has_value());
	if (!file.has_value())
	{
		return nullptr;
	}
	return std::make_unique<particles::Effect>(std::make_shared<const particles::File>(std::move(*file)), glm::vec3(0.0f), 1.0f,
	                                           game_random::particles::NetGameType::Synced);
}

class ParticleAtomShown: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (particles::FindCreatorFactory(k_CreatorClass) == nullptr)
		{
			particles::RegisterCreator(k_CreatorClass, MakeRecordingCreator);
		}
		Made().clear();
		// every draw gives the middle of its range (the effect's draws on the synced stream are GameRand's, its floats
		// too): the willow wisp's point in the ball is then about its centre. A draw of 0 would give the corner
		// (-1, -1, -1), outside the ball, every time, and its search would never end
		game_random::testing::SetGameRand([](uint32_t n) { return n / 2u; }, [](float range) { return range * 0.5f; });
	}
	void TearDown() override { Made().clear(); }

	const game_random::testing::ScopedState state;
};
} // namespace

// The game draws an atom once it has been stepped twice, or after one step when it is drawn on its first update
TEST(ParticleAtomShownRule, AnAtomIsShownAfterTwoStepsOrAfterOneWhenDrawnOnItsFirstUpdate)
{
	particles::Atom atom;
	EXPECT_FALSE(atom.Shown());
	atom.steps = 1;
	EXPECT_FALSE(atom.Shown());
	atom.drawOnFirstUpdate = true;
	EXPECT_TRUE(atom.Shown());
	atom.drawOnFirstUpdate = false;
	atom.steps = 2;
	EXPECT_TRUE(atom.Shown());
}

// An atom made by a rule is not shown after the step it is made in, and is after the next one; the steps it counts
// stop at two
TEST_F(ParticleAtomShown, AnAtomMadeInAStepIsShownFromTheNextStep)
{
	auto effect = MakeEffect(k_Header + k_Creator +
	                         "BEGINCLASS CreateRuleAnAtom A\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
	                         "PROPERTY NextGroups ARRAY SIZE 0\nPROPERTY PCreator PERSIS_PNTR C0\nENDPROPERTIES\nENDCLASS\n");
	ASSERT_NE(effect, nullptr);
	effect->Step(k_Step);
	ASSERT_EQ(Made().size(), 1u);
	EXPECT_EQ(Made()[0]->steps, 1u);
	EXPECT_FALSE(Made()[0]->Shown());
	effect->Step(k_Step);
	EXPECT_TRUE(Made()[0]->Shown());
	effect->Step(k_Step);
	EXPECT_EQ(Made()[0]->steps, 2u);
	EXPECT_TRUE(Made()[0]->Shown());
}

// The willow wisp's DrawOnFirstUpdate shows the atoms it makes after their first step already
TEST_F(ParticleAtomShown, TheWillowWispsDrawOnFirstUpdateShowsItsAtomsAfterOneStep)
{
	auto drawn = MakeEffect(k_Header + k_Creator + WillowWisp(true));
	ASSERT_NE(drawn, nullptr);
	drawn->Step(k_Step);
	ASSERT_EQ(Made().size(), 1u);
	EXPECT_TRUE(Made()[0]->drawOnFirstUpdate);
	EXPECT_TRUE(Made()[0]->Shown());

	Made().clear();
	auto plain = MakeEffect(k_Header + k_Creator + WillowWisp(false));
	ASSERT_NE(plain, nullptr);
	plain->Step(k_Step);
	ASSERT_EQ(Made().size(), 1u);
	EXPECT_FALSE(Made()[0]->Shown());
}
