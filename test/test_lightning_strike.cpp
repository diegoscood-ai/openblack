/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The weather's fork strikes (Particles/Rules/LightningStrike.h): the queue, the sound size and UR_LightningStrike on a
// synthetic file stepped on the synced stream, with the game random hook choosing every draw. The file's strike atoms
// come from a point creator, which draws only the atom's random byte, and their group has no rules, so every float
// draw seen is the strike rule's

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <limits>
#include <memory>
#include <string_view>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Audio/Services/SpellSounds.h"
#include "Common/GameRandom.h"
#include "Common/GameRandomTesting.h"
#include "FileSystem/DefaultFileSystem.h"
#include "Locator.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/Rules/LightningStrike.h"
#include "Particles/SoundAction.h"
#include "Resources/Loaders.h"
#include "Resources/Resources.h"
#include "Resources/ResourcesInterface.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{

/// Group 0 holds the strike rule only (as in SF_LightningStrike); its atoms carry an empty group 1
constexpr std::string_view k_Strike = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 0
PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
ENDPROPERTIES
BEGINCLASS ParticlePointCreator ParticlePointCreator0
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS UR_LightningStrike UR_LightningStrike0
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 0
PROPERTY NextGroups ARRAY SIZE 1 1
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator0
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY SoundLightning SOUND_ACTION SOUND_SPELL_LIGHTNING LOOPING 0 ONLYONE 0 SOFTRELEASE 1 USESURFACE 0
ENDPROPERTIES
ENDCLASS
)";

/// The same rule without a creator
constexpr std::string_view k_StrikeNoCreator = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 0
PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
ENDPROPERTIES
BEGINCLASS UR_LightningStrike UR_LightningStrike0
BEGINPROPERTIES
PROPERTY Group INTEGER 0
PROPERTY NextGroups ARRAY SIZE 1 1
ENDPROPERTIES
ENDCLASS
)";

/// A tiny SoundAction.h with the strike's sound, and the value it gives it
constexpr std::string_view k_SyntheticSoundActions = "enum LHSoundAction\n{\n\tNO_SOUND_ACTION = 0,\n"
                                                     "\tSOUND_SPELL_LIGHTNING = 57,\n};\n";
constexpr int32_t k_SyntheticLightningSound = 57;

/// Puts the synthetic SoundAction.h into a fresh resource cache, under the path the sound actions are read from, so
/// no file is opened. The previous file system and resources come back when it goes
class SyntheticSoundActions
{
public:
	SyntheticSoundActions()
	{
		Locator::filesystem::emplace<filesystem::DefaultFileSystem>();
		Locator::resources::emplace<resources::Resources>();
		const auto path = Locator::filesystem::value().GetPath<filesystem::Path::Data>() / "SoundAction.h";
		Locator::resources::value().GetBlobs().Load(
		    resources::BlobId(path), resources::BlobLoader::FromBufferTag {},
		    std::vector<uint8_t>(k_SyntheticSoundActions.begin(), k_SyntheticSoundActions.end()));
	}

private:
	// built before the constructor's body, so they keep the services that were there before
	const test::RestoreService<Locator::filesystem> _restoreFilesystem;
	const test::RestoreService<Locator::resources> _restoreResources;
};

/// The largest value a float draw's 0..0xFFFF roll can give: 0.99998 of the range
constexpr uint32_t k_MaxDraw = game_random::k_FloatRandRange - 1;

std::shared_ptr<const psys::File> Parse(std::string_view text, const char* name)
{
	auto file = psys::File::Parse(text, name);
	return file.has_value() ? std::make_shared<const psys::File>(*file) : nullptr;
}

/// Every synced draw's range, in order; a float draw (0xFFFF) returns the next of `floats` (0 once they run out),
/// the others 0
struct DrawLog
{
	std::vector<uint32_t> ranges;
	std::vector<uint32_t> floats;
	size_t nextFloat {0};
};

void HookDraws(DrawLog& log)
{
	game_random::testing::SetGameRand(
	    [&log](uint32_t n) -> uint32_t {
		    log.ranges.push_back(n);
		    if (n != game_random::k_FloatRandRange)
		    {
			    return 0;
		    }
		    return log.nextFloat < log.floats.size() ? log.floats[log.nextFloat++] : 0;
	    },
	    nullptr);
}

size_t FloatDraws(const DrawLog& log)
{
	size_t count = 0;
	for (const auto n : log.ranges)
	{
		count += n == game_random::k_FloatRandRange ? 1 : 0;
	}
	return count;
}

/// The strike atoms in collection order
std::vector<const psys::Atom*> Strikes(const psys::Effect& effect)
{
	std::vector<psys::Effect::DrawAtom> drawn;
	effect.Collect(1.0f, drawn, psys::Creator::Kind::Point);
	std::vector<const psys::Atom*> atoms;
	for (const auto& atom : drawn)
	{
		atoms.push_back(atom.atom);
	}
	return atoms;
}

/// The queue and the strikes' count start empty and end empty in every test
class LightningStrike: public ::testing::Test
{
protected:
	void SetUp() override { psys::lightning_strike::Clear(); }
	void TearDown() override { psys::lightning_strike::Clear(); }
};

} // namespace

TEST_F(LightningStrike, soundSize)
{
	using psys::lightning_strike::StrikeSoundSize;
	EXPECT_EQ(StrikeSoundSize(0.0f), 3);
	EXPECT_EQ(StrikeSoundSize(0.2999f), 3);
	EXPECT_EQ(StrikeSoundSize(0.3f), 2);
	EXPECT_EQ(StrikeSoundSize(0.7499f), 2);
	EXPECT_EQ(StrikeSoundSize(0.75f), 1);
	EXPECT_EQ(StrikeSoundSize(0.99f), 1);
	// a NaN fails both comparisons the way "below" does
	EXPECT_EQ(StrikeSoundSize(std::numeric_limits<float>::quiet_NaN()), 3);
}

TEST_F(LightningStrike, strikeSound)
{
	// the rule's action keeps its own bits and gains "delayed" and "on the ground" (0x22); the draw picks the size
	psys::SoundAction base;
	base.action = 7;
	base.flags = psys::SoundAction::k_SoftRelease;
	const auto small = psys::lightning_strike::StrikeSound(base, 0.1f);
	EXPECT_EQ(small.action, 7);
	EXPECT_EQ(small.size, 3);
	EXPECT_EQ(small.flags, psys::SoundAction::k_SoftRelease | 0x22);
	EXPECT_EQ(psys::lightning_strike::StrikeSound(base, 0.5f).size, 2);
	EXPECT_EQ(psys::lightning_strike::StrikeSound(base, 0.8f).size, 1);
	EXPECT_EQ(psys::SoundAction::k_Delayed | psys::SoundAction::k_SnapToGround, 0x22);
}

// The rule starts the strike's sound with the size from its second draw and the flags 0x22 added. The draws are max
// (the life) then 0 (the size): the second gives small (3), the first would give large (1), and the file's own
// action has the default medium (2) and no 0x22
TEST_F(LightningStrike, ruleStartsTheSizedSound)
{
	const SyntheticSoundActions soundActions;
	ASSERT_EQ(psys::SoundActionByName("SOUND_SPELL_LIGHTNING"), k_SyntheticLightningSound);
	const game_random::testing::ScopedState random;
	DrawLog log;
	log.floats = {k_MaxDraw, 0};
	HookDraws(log);
	const auto file = Parse(k_Strike, "SF_LightningStrikeTest");
	ASSERT_NE(file, nullptr);
	psys::lightning_strike::Queue(glm::vec3(10.0f, 5.0f, 10.0f), 40.0f);
	psys::Effect effect(file, glm::vec3(0.0f), 1.0f, game_random::psys::NetGameType::Synced);
	effect.Step(0.1f);
	const auto atoms = Strikes(effect);
	ASSERT_EQ(atoms.size(), 1u);
	ASSERT_EQ(atoms[0]->sounds.size(), 1u);
	const auto& action = atoms[0]->sounds.front()->action;
	EXPECT_EQ(action.action, k_SyntheticLightningSound);
	EXPECT_EQ(action.size, 3);
	EXPECT_EQ(action.flags,
	          psys::SoundAction::k_SoftRelease | psys::SoundAction::k_Delayed | psys::SoundAction::k_SnapToGround);
	audio::spell_sounds::Clear();
}

TEST_F(LightningStrike, queueAndClear)
{
	EXPECT_EQ(psys::lightning_strike::QueuedCount(), 0u);
	psys::lightning_strike::Queue(glm::vec3(1.0f, 2.0f, 3.0f), 40.0f);
	psys::lightning_strike::Queue(glm::vec3(4.0f, 5.0f, 6.0f), 40.0f);
	EXPECT_EQ(psys::lightning_strike::QueuedCount(), 2u);
	psys::lightning_strike::Clear();
	EXPECT_EQ(psys::lightning_strike::QueuedCount(), 0u);
}

// The regression for the strike that used to appear at the effect's origin: with nothing queued the rule makes no atom
// and draws nothing, and stays attached, so the always-on effect does not finish
TEST_F(LightningStrike, emptyQueueMakesNothing)
{
	const game_random::testing::ScopedState random;
	DrawLog log;
	HookDraws(log);
	const auto file = Parse(k_Strike, "SF_LightningStrikeTest");
	ASSERT_NE(file, nullptr);
	psys::Effect effect(file, glm::vec3(0.0f), 1.0f, game_random::psys::NetGameType::Synced);
	for (int i = 0; i < 10; ++i)
	{
		effect.Step(0.1f);
	}
	EXPECT_EQ(effect.AtomCount(), 0u);
	EXPECT_TRUE(log.ranges.empty());
	EXPECT_FALSE(effect.Finished());
}

TEST_F(LightningStrike, noCreatorDetaches)
{
	// without a creator the rule detaches and leaves the queue as it is
	const game_random::testing::ScopedState random;
	DrawLog log;
	HookDraws(log);
	const auto file = Parse(k_StrikeNoCreator, "SF_LightningStrikeNoCreatorTest");
	ASSERT_NE(file, nullptr);
	psys::lightning_strike::Queue(glm::vec3(1.0f, 2.0f, 3.0f), 40.0f);
	psys::Effect effect(file, glm::vec3(0.0f), 1.0f, game_random::psys::NetGameType::Synced);
	effect.Step(0.1f);
	EXPECT_EQ(psys::lightning_strike::QueuedCount(), 1u);
	EXPECT_EQ(effect.AtomCount(), 0u);
	EXPECT_TRUE(log.ranges.empty());
	EXPECT_TRUE(effect.Finished());
}

// Two queued points: the last one first, each with the atom's random byte, then its life, then its sound size. The
// lives tell the order apart: the draws are 0 (life 0.4), max, max (life about 0.8), 0. Had the size come before the
// life, the lives would be swapped
TEST_F(LightningStrike, lastQueuedFirstLifeThenSize)
{
	const game_random::testing::ScopedState random;
	DrawLog log;
	log.floats = {0, k_MaxDraw, k_MaxDraw, 0};
	HookDraws(log);
	const auto file = Parse(k_Strike, "SF_LightningStrikeTest");
	ASSERT_NE(file, nullptr);
	const glm::vec3 first(10.0f, 5.0f, 10.0f);
	const glm::vec3 second(20.0f, 6.0f, 30.0f);
	psys::lightning_strike::Queue(first, 40.0f);
	psys::lightning_strike::Queue(second, 40.0f);
	psys::Effect effect(file, glm::vec3(0.0f), 1.0f, game_random::psys::NetGameType::Synced);
	// steps of 0.2 s: the ages are 0, 0.2, 0.4, 0.6 and 0.8 exactly
	effect.Step(0.2f);
	EXPECT_EQ(psys::lightning_strike::QueuedCount(), 0u);
	const std::vector<uint32_t> expected {0x100, game_random::k_FloatRandRange, game_random::k_FloatRandRange,
	                                      0x100, game_random::k_FloatRandRange, game_random::k_FloatRandRange};
	EXPECT_EQ(log.ranges, expected);
	auto atoms = Strikes(effect);
	ASSERT_EQ(atoms.size(), 2u);
	// at the queued points, the second one made first
	EXPECT_FLOAT_EQ(atoms[0]->position.x, second.x);
	EXPECT_FLOAT_EQ(atoms[0]->position.y, second.y);
	EXPECT_FLOAT_EQ(atoms[0]->position.z, second.z);
	EXPECT_FLOAT_EQ(atoms[1]->position.x, first.x);
	EXPECT_FLOAT_EQ(atoms[1]->position.y, first.y);
	EXPECT_FLOAT_EQ(atoms[1]->position.z, first.z);
	// the second point's life is 0.4: kept at age 0.4, gone at 0.6; the first one's (about 0.8) is still alive
	effect.Step(0.2f);
	effect.Step(0.2f);
	EXPECT_EQ(Strikes(effect).size(), 2u);
	effect.Step(0.2f);
	atoms = Strikes(effect);
	ASSERT_EQ(atoms.size(), 1u);
	EXPECT_FLOAT_EQ(atoms[0]->position.x, first.x);
	// at age 0.8 it is past its life too
	effect.Step(0.2f);
	EXPECT_TRUE(Strikes(effect).empty());
	EXPECT_EQ(FloatDraws(log), 4u);
	EXPECT_FALSE(effect.Finished());
}

TEST_F(LightningStrike, lifeRange)
{
	// 0.4 + rand(0.4): the largest draw stays below 0.8, so the strike is gone at age 0.8 and alive at 0.6
	const game_random::testing::ScopedState random;
	DrawLog log;
	log.floats = {k_MaxDraw, 0};
	HookDraws(log);
	const auto file = Parse(k_Strike, "SF_LightningStrikeTest");
	ASSERT_NE(file, nullptr);
	psys::lightning_strike::Queue(glm::vec3(0.0f), 40.0f);
	psys::Effect effect(file, glm::vec3(0.0f), 1.0f, game_random::psys::NetGameType::Synced);
	for (int i = 0; i < 4; ++i)
	{
		effect.Step(0.2f);
	}
	EXPECT_EQ(Strikes(effect).size(), 1u);
	effect.Step(0.2f);
	EXPECT_TRUE(Strikes(effect).empty());
}

// 50 live strikes: a 51st point is dropped with no draw. A point queued on the step the strikes die is dropped too,
// since the queue is emptied before the old strikes go; one queued on the next step makes a strike again
TEST_F(LightningStrike, fiftyAlive)
{
	const game_random::testing::ScopedState random;
	DrawLog log; // every float draw 0: every life 0.4
	HookDraws(log);
	const auto file = Parse(k_Strike, "SF_LightningStrikeTest");
	ASSERT_NE(file, nullptr);
	for (int i = 0; i <= 50; ++i)
	{
		psys::lightning_strike::Queue(glm::vec3(static_cast<float>(i), 0.0f, 0.0f), 40.0f);
	}
	psys::Effect effect(file, glm::vec3(0.0f), 1.0f, game_random::psys::NetGameType::Synced);
	effect.Step(0.2f); // age 0
	EXPECT_EQ(psys::lightning_strike::QueuedCount(), 0u);
	auto atoms = Strikes(effect);
	ASSERT_EQ(atoms.size(), 50u);
	EXPECT_EQ(FloatDraws(log), 100u);
	// newest first: points 50 down to 1; point 0 (the first queued) was dropped
	EXPECT_FLOAT_EQ(atoms.front()->position.x, 50.0f);
	EXPECT_FLOAT_EQ(atoms.back()->position.x, 1.0f);

	psys::lightning_strike::Queue(glm::vec3(100.0f, 0.0f, 0.0f), 40.0f);
	effect.Step(0.2f); // age 0.2
	EXPECT_EQ(Strikes(effect).size(), 50u);
	EXPECT_EQ(FloatDraws(log), 100u);
	effect.Step(0.2f); // age 0.4: still alive
	psys::lightning_strike::Queue(glm::vec3(101.0f, 0.0f, 0.0f), 40.0f);
	effect.Step(0.2f); // age 0.6: dropped, then all 50 go
	EXPECT_TRUE(Strikes(effect).empty());
	EXPECT_EQ(FloatDraws(log), 100u);

	psys::lightning_strike::Queue(glm::vec3(102.0f, 0.0f, 0.0f), 40.0f);
	effect.Step(0.2f);
	atoms = Strikes(effect);
	ASSERT_EQ(atoms.size(), 1u);
	EXPECT_FLOAT_EQ(atoms[0]->position.x, 102.0f);
	EXPECT_EQ(FloatDraws(log), 102u);
}

TEST_F(LightningStrike, strikesCountOutWithTheirEffect)
{
	// the live count follows the atoms: once an effect with 50 strikes is gone, a new one makes strikes again
	const game_random::testing::ScopedState random;
	DrawLog log;
	HookDraws(log);
	const auto file = Parse(k_Strike, "SF_LightningStrikeTest");
	ASSERT_NE(file, nullptr);
	{
		for (int i = 0; i < 50; ++i)
		{
			psys::lightning_strike::Queue(glm::vec3(0.0f), 40.0f);
		}
		psys::Effect full(file, glm::vec3(0.0f), 1.0f, game_random::psys::NetGameType::Synced);
		full.Step(0.1f);
		ASSERT_EQ(Strikes(full).size(), 50u);
		psys::Effect other(file, glm::vec3(0.0f), 1.0f, game_random::psys::NetGameType::Synced);
		psys::lightning_strike::Queue(glm::vec3(0.0f), 40.0f);
		other.Step(0.1f);
		EXPECT_TRUE(Strikes(other).empty()); // the cap is shared by every strike
	}
	psys::Effect fresh(file, glm::vec3(0.0f), 1.0f, game_random::psys::NetGameType::Synced);
	psys::lightning_strike::Queue(glm::vec3(0.0f), 40.0f);
	fresh.Step(0.1f);
	EXPECT_EQ(Strikes(fresh).size(), 1u);
}
