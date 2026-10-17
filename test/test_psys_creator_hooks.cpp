/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The particle creators' hooks (Particles/PSys.h): AfterAtomStep once per atom per effect step, after the atom's own
// step, and AtomRemoved on each path an effect deletes an atom (a remove rule, the willow wisp's DieAge, the close-down
// removal, a move to a group with no collection, the effect's destruction), with the atoms of its sub-collections. And
// the atom's drawn frame and its clip time. A fake creator records the calls on small synthetic effects.

#include <cstdint>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include "3D/FrameAnim.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysRegistry.h"

using namespace openblack;

namespace
{
/// What the fake creator was called for, in call order
struct HookLog
{
	std::vector<const psys::Atom*> made;
	std::vector<const psys::Atom*> stepped;
	std::vector<const psys::Atom*> removed;
	/// For each AfterAtomStep: the atom's step was done (drawn, and the drawn frame is the stepped one)
	std::vector<bool> stepDone;
};

HookLog& Log()
{
	static HookLog s_log;
	return s_log;
}

/// Records each atom it makes, each AfterAtomStep and each AtomRemoved. Its frames are read here, since the common
/// creator properties read them only for sprites
struct HookCreator final: psys::Creator
{
	void InitAtom(psys::Effect& /*effect*/, psys::Atom& atom) const override
	{
		atom.frameRate = frameRate;
		atom.playAnim = playAnim;
		Log().made.push_back(&atom);
	}
	void AfterAtomStep(psys::Effect& /*effect*/, psys::Atom& atom) const override
	{
		Log().stepped.push_back(&atom);
		Log().stepDone.push_back(atom.drawn && atom.current.frame == atom.frame);
	}
	void AtomRemoved(psys::Effect& /*effect*/, psys::Atom& atom) const override { Log().removed.push_back(&atom); }
};

constexpr const char* k_HookClass = "ParticleHookTestCreator";

std::unique_ptr<psys::Creator> MakeHookCreator(const psys::Object& object)
{
	auto creator = std::make_unique<HookCreator>();
	psys::ReadCreatorProperties(object, *creator);
	creator->numFrames = std::max(1, object.Int("NumFrames", 1));
	creator->frameRate = object.Float("FrameRate", 1.0f);
	creator->playAnim = object.Bool("PlayAnim", false);
	return creator;
}

std::shared_ptr<const psys::File> Parse(const std::string& text)
{
	auto file = psys::File::Parse(text, "SF_CreatorHooksTest");
	return file.has_value() ? std::make_shared<const psys::File>(std::move(*file)) : nullptr;
}

const std::string k_Header = "BEGINPROPERTIES\n"
                             "PROPERTY DeleteOnCloseDown BOOL 0\n"
                             "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
                             "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
                             "ENDPROPERTIES\n";

const std::string k_Hook = std::string("BEGINCLASS ") + k_HookClass +
                           " H0\nBEGINPROPERTIES\nPROPERTY NumFrames INTEGER 8\nPROPERTY FrameRate FLOAT 15\n"
                           "PROPERTY PlayAnim BOOL 1\nPROPERTY LoopAnim BOOL 1\nENDPROPERTIES\nENDCLASS\n";

/// CreateRuleAnAtom of H0 in that group: one atom, once, with sub-collections of nextGroups ("" for none)
std::string AnAtom(const std::string& name, int group, const std::string& nextGroups = "")
{
	const int count = nextGroups.empty() ? 0 : static_cast<int>(std::count(nextGroups.begin(), nextGroups.end(), ' ') + 1);
	return "BEGINCLASS CreateRuleAnAtom " + name + "\nBEGINPROPERTIES\nPROPERTY Group INTEGER " + std::to_string(group) +
	       "\nPROPERTY NextGroups ARRAY SIZE " + std::to_string(count) + (nextGroups.empty() ? "" : " " + nextGroups) +
	       "\nPROPERTY PCreator PERSIS_PNTR H0\nENDPROPERTIES\nENDCLASS\n";
}

/// Every atom of the effect (each kind of the fake creator is Other)
std::vector<const psys::Atom*> Atoms(const psys::Effect& effect, float t = 1.0f)
{
	std::vector<psys::Effect::DrawAtom> drawn;
	effect.Collect(t, drawn, psys::Creator::Kind::Other);
	std::vector<const psys::Atom*> atoms;
	for (const auto& atom : drawn)
	{
		atoms.push_back(atom.atom);
	}
	return atoms;
}

/// A second creator for the same class name, to tell which registration won
std::unique_ptr<psys::Creator> MakeOtherHookCreator(const psys::Object& object)
{
	return MakeHookCreator(object);
}

class CreatorHooks: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// the factories live in the particle system's state, which each test starts afresh
		if (psys::FindCreatorFactory(k_HookClass) == nullptr)
		{
			psys::RegisterCreator(k_HookClass, MakeHookCreator);
		}
		Log() = {};
	}
};
} // namespace

TEST_F(CreatorHooks, afterAtomStepOncePerAtomPerStep)
{
	// two atoms in the root collection, and a child under the first (group 1, one of its sub-collections)
	const auto file = Parse(k_Header + k_Hook + AnAtom("A", 0, "1") + AnAtom("B", 0) + AnAtom("C", 1));
	ASSERT_NE(file, nullptr);
	{
		psys::Effect effect(file, glm::vec3(0.0f), 1.0f);
		for (int step = 1; step <= 5; ++step)
		{
			Log().stepped.clear();
			effect.Step(0.1f);
			ASSERT_EQ(effect.AtomCount(), 3u);
			// each atom once, all three
			EXPECT_EQ(Log().stepped.size(), 3u) << "step " << step;
			const std::set<const psys::Atom*> stepped(Log().stepped.begin(), Log().stepped.end());
			const std::set<const psys::Atom*> made(Log().made.begin(), Log().made.end());
			EXPECT_EQ(stepped, made) << "step " << step;
			// an atom's hook comes after those of its sub-collections' atoms
			for (size_t i = 0; i < Log().stepped.size(); ++i)
			{
				for (const auto& sub : Log().stepped[i]->subCollections)
				{
					for (const auto& child : sub->atoms)
					{
						const auto at = std::find(Log().stepped.begin(), Log().stepped.end(), child.get());
						ASSERT_NE(at, Log().stepped.end());
						EXPECT_LT(static_cast<size_t>(at - Log().stepped.begin()), i) << "step " << step;
					}
				}
			}
		}
		// called after the atom's own step: drawn, with the frame of this step
		EXPECT_EQ(std::count(Log().stepDone.begin(), Log().stepDone.end(), false), 0);
		EXPECT_TRUE(Log().removed.empty());
	}
}

TEST_F(CreatorHooks, atomRemovedWhenARemoveRuleDeletesIt)
{
	// the atom dies past 0.25 s of age, and its child with it, the parent reported first
	const auto file = Parse(k_Header + k_Hook + AnAtom("A", 0, "1") + AnAtom("C", 1) +
	                        "BEGINCLASS RemoveRuleOldAgeOnly R\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
	                        "PROPERTY DieAge FLOAT 0.25\nENDPROPERTIES\nENDCLASS\n");
	ASSERT_NE(file, nullptr);
	{
		psys::Effect effect(file, glm::vec3(0.0f), 1.0f);
		for (int step = 0; step < 6 && (step == 0 || effect.AtomCount() != 0); ++step)
		{
			effect.Step(0.1f);
		}
		EXPECT_EQ(effect.AtomCount(), 0u);
		ASSERT_EQ(Log().made.size(), 2u);
		ASSERT_EQ(Log().removed.size(), 2u);
		EXPECT_EQ(Log().removed[0], Log().made[0]);
		EXPECT_EQ(Log().removed[1], Log().made[1]);
		// no step reaches a deleted atom
		const size_t stepped = Log().stepped.size();
		effect.Step(0.1f);
		EXPECT_EQ(Log().stepped.size(), stepped);
	}
	// nothing left for the destruction
	EXPECT_EQ(Log().removed.size(), 2u);
}

TEST_F(CreatorHooks, atomRemovedWhenTheCloseDownClearsTheEffect)
{
	// RemoveRuleAfterCloseDown, no delay: every atom goes once the effect is closing
	const auto file = Parse(k_Header + k_Hook + AnAtom("A", 0) + AnAtom("B", 0) +
	                        "BEGINCLASS RemoveRuleAfterCloseDown R\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
	                        "PROPERTY Delay FLOAT 0\nENDPROPERTIES\nENDCLASS\n");
	ASSERT_NE(file, nullptr);
	psys::Effect effect(file, glm::vec3(0.0f), 1.0f);
	effect.Step(0.1f);
	effect.Step(0.1f);
	ASSERT_EQ(effect.AtomCount(), 2u);
	EXPECT_TRUE(Log().removed.empty());
	effect.CloseDown();
	for (int step = 0; step < 5 && effect.AtomCount() != 0; ++step)
	{
		effect.Step(0.1f);
	}
	EXPECT_EQ(effect.AtomCount(), 0u);
	const std::set<const psys::Atom*> removed(Log().removed.begin(), Log().removed.end());
	const std::set<const psys::Atom*> made(Log().made.begin(), Log().made.end());
	EXPECT_EQ(Log().removed.size(), 2u);
	EXPECT_EQ(removed, made);
}

TEST_F(CreatorHooks, atomRemovedAtTheWillowWispsDieAge)
{
	// DeleteAtomsAtDieAge: every atom it made and deleted is reported, and the rest when the effect goes
	const auto file = Parse(k_Header + k_Hook +
	                        "BEGINCLASS UR_WillowWisp W\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
	                        "PROPERTY NextGroups ARRAY SIZE 0\nPROPERTY PCreator PERSIS_PNTR H0\n"
	                        "PROPERTY MaxAtoms INTEGER 10\nPROPERTY DieAge FLOAT 0.3\n"
	                        "PROPERTY DeleteAtomsAtDieAge BOOL 1\nENDPROPERTIES\nENDCLASS\n");
	ASSERT_NE(file, nullptr);
	{
		psys::Effect effect(file, glm::vec3(0.0f), 1.0f);
		for (int step = 0; step < 10; ++step)
		{
			effect.Step(0.1f);
			// made = alive + removed, after every step
			EXPECT_EQ(Log().made.size(), effect.AtomCount() + Log().removed.size()) << "step " << step;
		}
		EXPECT_FALSE(Log().removed.empty());
	}
	EXPECT_EQ(Log().removed.size(), Log().made.size());
}

TEST_F(CreatorHooks, atomRemovedWhenMovedToAGroupWithNoCollection)
{
	const auto file = Parse(k_Header + k_Hook + AnAtom("A", 0));
	ASSERT_NE(file, nullptr);
	psys::Effect effect(file, glm::vec3(0.0f), 1.0f);
	effect.Step(0.1f);
	const auto atoms = Atoms(effect);
	ASSERT_EQ(atoms.size(), 1u);
	// no root collection of group 7: the atom goes
	effect.MoveToBaseGroup(*atoms.front()->collection, *atoms.front(), 7);
	EXPECT_EQ(Atoms(effect).size(), 0u);
	ASSERT_EQ(Log().removed.size(), 1u);
	EXPECT_EQ(Log().removed.front(), atoms.front());
}

TEST_F(CreatorHooks, atomRemovedForEveryAtomWhenTheEffectIsDestroyed)
{
	// the atoms of the sub-collections too, each right after its parent
	const auto file = Parse(k_Header + k_Hook + AnAtom("A", 0, "1") + AnAtom("B", 0) + AnAtom("C", 1));
	ASSERT_NE(file, nullptr);
	{
		psys::Effect effect(file, glm::vec3(0.0f), 1.0f);
		effect.Step(0.1f);
		effect.Step(0.1f);
		ASSERT_EQ(effect.AtomCount(), 3u);
		EXPECT_TRUE(Log().removed.empty());
	}
	ASSERT_EQ(Log().made.size(), 3u);
	ASSERT_EQ(Log().removed.size(), 3u);
	// made: A and B by the root's rules, then C when A's sub-collection is updated; removed in walk order: A, its
	// child C, then B
	EXPECT_EQ(Log().removed[0], Log().made[0]);
	EXPECT_EQ(Log().removed[1], Log().made[2]);
	EXPECT_EQ(Log().removed[2], Log().made[1]);
}

TEST(AtomDrawnFrame, theWholeFrameDrawnAndItsClipTime)
{
	// the forest camera's take: 1000 frames, not looped
	psys::Creator anim;
	anim.numFrames = 1000;
	anim.loopAnim = false;
	psys::Atom atom;
	atom.creator = &anim;
	atom.previous.frame = 998.0f;
	atom.current.frame = 1000.5f;
	// the lerp at t = 0, 0.3, 0.5 and 3 (clamped to 1 without a loop), then truncated and clamped to 999
	EXPECT_EQ(atom.DrawnFrame(0.0f), 998);
	EXPECT_EQ(atom.DrawnFrame(0.3f), 998);
	EXPECT_EQ(atom.DrawnFrame(0.5f), 999);
	EXPECT_EQ(atom.DrawnFrame(3.0f), 999);
	// the 6633 ms clip: 6633 x frame / 1000 in integers
	EXPECT_EQ(atom.DrawnClipMs(6633, 0.0f), 6619);
	EXPECT_EQ(atom.DrawnClipMs(6633, 1.0f), 6626);
	atom.previous.frame = 0.0f;
	atom.current.frame = 1.0f;
	EXPECT_EQ(atom.DrawnClipMs(6633, 0.0f), 0);
	EXPECT_EQ(atom.DrawnClipMs(6633, 1.0f), 6);

	// looped: the lerp goes up to t = 5 and wraps within the frames
	psys::Creator sprite;
	sprite.numFrames = 8;
	sprite.loopAnim = true;
	atom.creator = &sprite;
	atom.previous.frame = 7.0f;
	atom.current.frame = 9.0f;
	EXPECT_EQ(atom.DrawnFrame(0.0f), 7);
	EXPECT_EQ(atom.DrawnFrame(0.5f), 0);
	EXPECT_EQ(atom.DrawnFrame(2.0f), 3);

	// no creator: 0
	atom.creator = nullptr;
	EXPECT_EQ(atom.DrawnFrame(0.5f), 0);
}

TEST_F(CreatorHooks, theDrawnFrameIsTheOneTheDrawPicks)
{
	// H0: 8 frames, 15 a second, looped; the draw's frame at a few fractions of a step
	const auto file = Parse(k_Header + k_Hook + AnAtom("A", 0));
	ASSERT_NE(file, nullptr);
	psys::Effect effect(file, glm::vec3(0.0f), 1.0f);
	for (int step = 0; step < 4; ++step)
	{
		effect.Step(0.1f);
		for (const float t : {0.0f, 0.3f, 0.7f, 1.0f})
		{
			std::vector<psys::Effect::DrawAtom> drawn;
			effect.Collect(t, drawn, psys::Creator::Kind::Other);
			ASSERT_EQ(drawn.size(), 1u);
			EXPECT_EQ(drawn.front().atom->DrawnFrame(t), graphics::frame_anim::ParticleFrameIndex(drawn.front().frame, 8, true))
			    << "step " << step << ", t " << t;
		}
	}
}

TEST(PSysRegistry, aClassRegisteredTwiceWithNoGameLoggerKeepsTheLast)
{
	// as when the test runs on its own: no game logger to warn through
	const auto logger = spdlog::get("game");
	spdlog::drop("game");
	constexpr const char* k_Twice = "ParticleRegisteredTwiceTestCreator";
	psys::RegisterCreator(k_Twice, MakeHookCreator);
	psys::RegisterCreator(k_Twice, MakeOtherHookCreator);
	EXPECT_EQ(psys::FindCreatorFactory(k_Twice), &MakeOtherHookCreator);
	if (logger != nullptr)
	{
		spdlog::register_logger(logger);
	}
}
