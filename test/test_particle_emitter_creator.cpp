/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <array>
#include <memory>
#include <string>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "Common/GameRandomTesting.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleSpellFile.h"

using namespace openblack;

namespace
{
const std::string k_Header = "BEGINPROPERTIES\n"
                             "PROPERTY DeleteOnCloseDown BOOL 0\n"
                             "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
                             "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
                             "ENDPROPERTIES\n";

const std::string k_Point = "BEGINCLASS ParticlePointCreator P\n"
                            "BEGINPROPERTIES\n"
                            "ENDPROPERTIES\n"
                            "ENDCLASS\n";

/// An emitter of the class, of the creator named (none for an empty name: the files' NULL_STRING), ten atoms a second
std::string Emitter(const std::string& className, const std::string& creator)
{
	return "BEGINCLASS " + className + " E\n" + "BEGINPROPERTIES\n" + "PROPERTY Group INTEGER 0\n" +
	       "PROPERTY NextGroups ARRAY SIZE 0\n" + "PROPERTY PCreator PERSIS_PNTR " +
	       (creator.empty() ? std::string("NULL_STRING") : creator) + "\n" + "PROPERTY EmissionFreq FLOAT 10\n" +
	       "ENDPROPERTIES\n" + "ENDCLASS\n";
}

std::unique_ptr<particles::Effect> MakeEffect(const std::string& text)
{
	auto file = particles::File::Parse(text, "SF_EmitterCreatorTest");
	EXPECT_TRUE(file.has_value());
	if (!file.has_value())
	{
		return nullptr;
	}
	return std::make_unique<particles::Effect>(std::make_shared<const particles::File>(std::move(*file)), glm::vec3(0.0f), 1.0f,
	                                           game_random::particles::NetGameType::Synced);
}
} // namespace

// Every emitter shape comes off its collection with no atom made when it has no creator, as the original's do; with
// one it emits
TEST(ParticleEmitterCreator, AnEmitterWithNoCreatorComesOffItsCollectionWithoutEmitting)
{
	const game_random::testing::ScopedState state;
	// every draw half way: an emitted point is drawn in the ball until one falls inside it, which all-zero draws never give
	game_random::testing::SetGameRand([](uint32_t n) { return n / 2; }, [](float range) { return range * 0.5f; });
	constexpr std::array<const char*, 4> k_Shapes {"EmitterRuleSimple", "DiskEmitter", "SpreadingDiskEmitter",
	                                               "EmitterRuleConical"};
	for (const auto* shape : k_Shapes)
	{
		auto without = MakeEffect(k_Header + Emitter(shape, ""));
		ASSERT_NE(without, nullptr) << shape;
		without->Step(0.1f);
		const auto& root = *without->GetRoots().front();
		EXPECT_TRUE(root.atoms.empty()) << shape;
		ASSERT_EQ(root.modifiers.size(), 1u) << shape;
		EXPECT_FALSE(root.modifiers.front().attached) << shape;

		auto with = MakeEffect(k_Header + k_Point + Emitter(shape, "P"));
		ASSERT_NE(with, nullptr) << shape;
		with->Step(0.1f);
		EXPECT_FALSE(with->GetRoots().front()->atoms.empty()) << shape;
		EXPECT_TRUE(with->GetRoots().front()->modifiers.front().attached) << shape;
	}
}
