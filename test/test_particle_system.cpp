/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The particle system (ecs::systems::ParticleSystem) in front of the particle engine: the synced flag picks the shared
// or the local random numbers, a particle type starts its file, an effect runs until it is deleted, a target, a target
// point or a player for an effect that is not running goes nowhere, an effect stepped by frame is left alone by the
// turns, a draw offset moves only its own effect's drawing, a shield is found within its radius and margin, there are
// no particle sounds without the audio, a gesture's trail is taken in the order it came, a belief sprite reaches the
// towns' queue until it is full, the hand's trail is sized by the camera's distance from the hand, a spot visual's
// turns go to its container, the entries flagged TargetOwnerObject give their effect the owner as a target, the sheets
// of light are moved on every frame while something holds them, the debug window lists every running effect and the
// spell files, and a paused turn steps nothing. The system is made here and only put in the locator, where the engine
// finds its state; the spell files are synthetic, in a game folder of the test's own, and so are the sheets.

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "ECS/Registry.h"
#include "ECS/Systems/AudioStateInterface.h"
#include "ECS/Systems/Implementations/ParticleSystem.h"
#include "ECS/Systems/Implementations/TownStateSystem.h"
#include "ECS/Town/TownBelief.h"
#include "Enums.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Gestures/GestureShapes.h"
#include "Particles/GestureTrail.h"
#include "Particles/LightSheet.h"
#include "Particles/PSys.h"
#include "Particles/PSysManager.h"
#include "Particles/PSysManagerState.h"
#include "Particles/Rules/Shield.h"
#include "Particles/SpellLink.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"

using namespace openblack;
using openblack::ecs::systems::ParticleSystem;
using openblack::ecs::systems::ParticleSystemInterface;
using openblack::game_random::psys::NetGameType;

namespace
{
/// One sprite made by one create rule at the first step, in a group created at the start: an effect that keeps an atom
/// for its first steps
constexpr std::string_view k_EffectText =
    "BEGINPROPERTIES\n"
    "PROPERTY DeleteOnCloseDown BOOL 1\n"
    "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
    "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
    "PROPERTY MaxSpellAge FLOAT 100\n"
    "ENDPROPERTIES\n"
    "BEGINCLASS ParticleSpriteCreator Sprite\nBEGINPROPERTIES\nPROPERTY InitialScale FLOAT 1\n"
    "ENDPROPERTIES\nENDCLASS\n"
    "BEGINCLASS CreateRuleAnAtom Create\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
    "PROPERTY NextGroups ARRAY SIZE 0\nPROPERTY PCreator PERSIS_PNTR Sprite\n"
    "ENDPROPERTIES\nENDCLASS\n";

constexpr std::string_view k_Effect = "openblack_test_particle_system";
constexpr entt::entity k_NoObject = entt::null;

class ParticleSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// the loader and the spot visuals log on the game's logger
		if (!spdlog::get("game"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		static std::atomic<int> s_count {0};
		_root = std::filesystem::temp_directory_path() /
		        ("openblack_test_particle_system_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "_" +
		         std::to_string(s_count++));
		std::filesystem::create_directories(_root / "Data" / "Spells" / "ZSpellFiles");
		Locator::filesystem::value().SetGamePath(_root);
		Write(k_Effect);
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		_system = &Locator::particleSystem::emplace<ParticleSystem>();
	}
	void TearDown() override
	{
		_system->Reset();
		Locator::entitiesRegistry::reset();
		std::error_code ec;
		std::filesystem::remove_all(_root, ec);
	}

	/// A spell file of that name in the test's game folder
	void Write(std::string_view name, std::string_view text = k_EffectText) const
	{
		std::ofstream(_root / "Data" / "Spells" / "ZSpellFiles" / (std::string(name) + ".txt"), std::ios::binary)
		    .write(text.data(), static_cast<std::streamsize>(text.size()));
	}

	[[nodiscard]] ParticleSystemInterface& System() const { return *_system; }

	// first, so that it goes last
	const test::ScopedDefaultFileSystem _fileSystem;
	std::filesystem::path _root;
	ParticleSystemInterface* _system {nullptr};
};
} // namespace

TEST_F(ParticleSystemTest, SyncedPicksTheSharedRandomNumbers)
{
	const auto local = System().Start(k_Effect, glm::vec3(0.0f), 1.0f);
	const auto synced = System().Start(k_Effect, glm::vec3(0.0f), 1.0f, true);
	ASSERT_NE(System().Find(local), nullptr);
	ASSERT_NE(System().Find(synced), nullptr);
	EXPECT_EQ(System().Find(local)->NetType(), NetGameType::Local);
	EXPECT_EQ(System().Find(synced)->NetType(), NetGameType::Synced);
	EXPECT_EQ(System().Start("openblack_test_missing", glm::vec3(0.0f), 1.0f), ParticleSystemInterface::k_NoEffect);
}

TEST_F(ParticleSystemTest, SpellEffectsAreTheSpellsAndSyncedAsAsked)
{
	const auto local = System().StartForSpell(k_Effect, glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), 1.0f, nullptr, false);
	const auto synced = System().StartForSpell(k_Effect, glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), 1.0f, nullptr, true);
	ASSERT_NE(System().Find(local), nullptr);
	ASSERT_NE(System().Find(synced), nullptr);
	EXPECT_EQ(System().Find(local)->NetType(), NetGameType::Local);
	EXPECT_EQ(System().Find(synced)->NetType(), NetGameType::Synced);
	// stepped by their spell, so a turn leaves them alone
	auto& effects = System().GetState().effects;
	EXPECT_TRUE(effects.find(local)->second.ownedBySpell);
	EXPECT_TRUE(effects.find(synced)->second.ownedBySpell);
}

TEST_F(ParticleSystemTest, TargetForAnEffectNotRunningGoesNowhere)
{
	const auto id = System().Start(k_Effect, glm::vec3(0.0f), 1.0f);
	ASSERT_NE(System().Find(id), nullptr);
	const auto target = entt::entity {7};
	System().AddTarget(id, target);
	System().AddTarget(id + 1, entt::entity {8});
	System().AddTarget(ParticleSystemInterface::k_NoEffect, entt::entity {9});
	ASSERT_EQ(System().Find(id)->GetTargets().size(), 1u);
	EXPECT_EQ(System().Find(id)->GetTargets().front(), target);
	System().Delete(id);
	System().AddTarget(id, target); // gone: nothing to add to
	EXPECT_EQ(System().Find(id), nullptr);
}

TEST_F(ParticleSystemTest, ATypeStartsItsFileAndATypeWithoutOneNothing)
{
	// the leaves' file
	Write("SF_Forest");
	const auto leaves = System().Start(ParticleType::Leaves, glm::vec3(1.0f, 2.0f, 3.0f), 1.0f, true);
	ASSERT_NE(leaves, ParticleSystemInterface::k_NoEffect);
	ASSERT_NE(System().Find(leaves), nullptr);
	EXPECT_EQ(System().Find(leaves)->GetFile().name, "SF_Forest");
	EXPECT_EQ(System().Find(leaves)->NetType(), NetGameType::Synced);
	// stepped by the turns, as an effect started by its file is
	auto& effects = System().GetState().effects;
	EXPECT_FALSE(effects.find(leaves)->second.ownedBySpell);
	// the tornado's type has no file; the food's file is not in the game folder
	EXPECT_EQ(System().Start(ParticleType::Tornado, glm::vec3(0.0f), 1.0f), ParticleSystemInterface::k_NoEffect);
	EXPECT_EQ(System().Start(ParticleType::Food, glm::vec3(0.0f), 1.0f), ParticleSystemInterface::k_NoEffect);
	EXPECT_EQ(std::distance(effects.begin(), effects.end()), 1);
}

TEST_F(ParticleSystemTest, AnEffectRunsUntilItIsDeleted)
{
	const auto id = System().Start(k_Effect, glm::vec3(0.0f), 1.0f);
	ASSERT_NE(id, ParticleSystemInterface::k_NoEffect);
	EXPECT_TRUE(System().IsRunning(id));
	EXPECT_FALSE(System().IsRunning(id + 1));
	EXPECT_FALSE(System().IsRunning(ParticleSystemInterface::k_NoEffect));
	System().Delete(id);
	EXPECT_FALSE(System().IsRunning(id));
}

TEST_F(ParticleSystemTest, ThePlayerReachesARunningEffectOnly)
{
	const auto id = System().Start(k_Effect, glm::vec3(0.0f), 1.0f);
	ASSERT_NE(System().Find(id), nullptr);
	// shown for no player until told
	EXPECT_EQ(System().Find(id)->GetPlayer(), -1);
	System().SetPlayer(id, 3);
	EXPECT_EQ(System().Find(id)->GetPlayer(), 3);
	// nothing to set for an effect not running
	System().SetPlayer(id + 1, 2);
	System().SetPlayer(ParticleSystemInterface::k_NoEffect, 2);
	EXPECT_EQ(System().Find(id)->GetPlayer(), 3);
	EXPECT_EQ(System().Find(id + 1), nullptr);
}

TEST_F(ParticleSystemTest, ATargetPointReachesARunningEffectOnly)
{
	const auto id = System().Start(k_Effect, glm::vec3(0.0f), 1.0f);
	ASSERT_NE(System().Find(id), nullptr);
	System().AddTargetPosition(id, glm::vec3(1.0f, 2.0f, 3.0f));
	// nothing to add to for an effect not running
	System().AddTargetPosition(id + 1, glm::vec3(4.0f));
	System().AddTargetPosition(ParticleSystemInterface::k_NoEffect, glm::vec3(5.0f));
	auto* effect = System().Find(id);
	ASSERT_EQ(effect->TargetPointCount(), 1u);
	// a point, not an object
	EXPECT_TRUE(effect->GetTargets().empty());
	glm::vec3 point(0.0f);
	ASSERT_TRUE(effect->TakeTargetPoint(point));
	EXPECT_EQ(point, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_EQ(System().Find(id + 1), nullptr);
}

TEST_F(ParticleSystemTest, AnEffectSteppedByFrameIsLeftByTheTurns)
{
	EXPECT_FALSE(System().ProcessByFrame(ParticleSystemInterface::k_NoEffect, 0.25f));
	const auto byFrame = System().Start(k_Effect, glm::vec3(0.0f), 1.0f);
	const auto byTurn = System().Start(k_Effect, glm::vec3(0.0f), 1.0f);
	ASSERT_NE(System().Find(byFrame), nullptr);
	ASSERT_NE(System().Find(byTurn), nullptr);
	EXPECT_FALSE(System().ProcessByFrame(byTurn + 1, 0.25f));

	// stepped by the seconds given, and drawn where that step left it
	EXPECT_TRUE(System().ProcessByFrame(byFrame, 0.25f));
	ASSERT_NE(System().Find(byFrame), nullptr);
	EXPECT_FLOAT_EQ(System().Find(byFrame)->GetAge(), 0.25f);
	const auto& running = System().GetState().effects.find(byFrame)->second;
	EXPECT_TRUE(running.ownedBySpell);
	EXPECT_TRUE(running.perFrame);

	// a turn steps the other effect only
	System().ProcessTurn();
	ASSERT_NE(System().Find(byFrame), nullptr);
	ASSERT_NE(System().Find(byTurn), nullptr);
	EXPECT_FLOAT_EQ(System().Find(byFrame)->GetAge(), 0.25f);
	EXPECT_FLOAT_EQ(System().Find(byTurn)->GetAge(), game_clock::k_TurnSeconds);
	EXPECT_TRUE(System().ProcessByFrame(byFrame, 0.05f));
	EXPECT_FLOAT_EQ(System().Find(byFrame)->GetAge(), 0.3f);
}

TEST_F(ParticleSystemTest, AShieldIsFoundWithinItsRadiusAndMargin)
{
	EXPECT_EQ(System().FindShield(glm::vec3(0.0f), 100.0f), nullptr);
	const auto id = System().Start(k_Effect, glm::vec3(0.0f), 1.0f);
	const auto* effect = System().Find(id);
	ASSERT_NE(effect, nullptr);
	const auto sphere = psys::shields::AddDefensiveSphere(*effect, glm::vec3(10.0f, 0.0f, 0.0f), 2.0f);

	// inside its radius
	const auto* inside = System().FindShield(glm::vec3(11.0f, 0.0f, 0.0f), 0.0f);
	ASSERT_NE(inside, nullptr);
	EXPECT_EQ(inside->id, sphere);
	EXPECT_EQ(inside->owner, effect);
	// outside it, unless the margin reaches the point; the edge itself is outside
	const glm::vec3 outside(13.0f, 0.0f, 0.0f);
	EXPECT_EQ(System().FindShield(outside, 0.0f), nullptr);
	EXPECT_EQ(System().FindShield(outside, 0.5f), nullptr);
	EXPECT_EQ(System().FindShield(outside, 1.0f), nullptr);
	ASSERT_NE(System().FindShield(outside, 1.5f), nullptr);
	EXPECT_EQ(System().FindShield(outside, 1.5f)->id, sphere);

	// the newest first, as the engine's own lookup has it
	const auto newer = psys::shields::AddDefensiveSphere(*effect, glm::vec3(11.0f, 0.0f, 0.0f), 2.0f);
	const glm::vec3 both(11.0f, 0.0f, 0.0f);
	ASSERT_NE(System().FindShield(both, 0.0f), nullptr);
	EXPECT_EQ(System().FindShield(both, 0.0f)->id, newer);
	EXPECT_EQ(System().FindShield(both, 0.0f), psys::shields::FindShieldContainingPoint(both, 0.0f));

	// the spheres go with their effect
	System().Delete(id);
	EXPECT_EQ(System().FindShield(both, 0.0f), nullptr);
}

TEST_F(ParticleSystemTest, TheParticleSoundsAreNoneWithoutTheAudio)
{
	// no audio service, put back before the system is reset
	const test::RestoreService<Locator::audioState> restoreAudio;
	Locator::audioState::reset();
	EXPECT_EQ(System().GetSoundCount(), 0u);
}

TEST_F(ParticleSystemTest, NoParticleSoundIsCountedWhileNothingPlays)
{
	// the test listener's audio service, with nothing playing, and no effect here makes a sound
	EXPECT_EQ(System().GetSoundCount(), 0u);
	const auto id = System().Start(k_Effect, glm::vec3(0.0f), 1.0f);
	ASSERT_NE(System().Find(id), nullptr);
	System().ProcessTurn();
	EXPECT_EQ(System().GetSoundCount(), 0u);
}

TEST_F(ParticleSystemTest, AGestureTrailIsTakenInTheOrderItCame)
{
	const auto& pending = System().GetState().pendingGestures;
	ASSERT_TRUE(pending.empty());
	// no trail, nothing taken
	System().AddGestureTrail(nullptr);
	EXPECT_TRUE(pending.empty());

	auto first = std::make_shared<particles::GestureTrail>();
	first->stroke.Add(glm::vec3(1.0f, 0.0f, 2.0f));
	first->stroke.Add(glm::vec3(3.0f, 0.0f, 4.0f));
	first->ideal.Add(glm::vec3(1.0f, 0.0f, 2.5f));
	first->ideal.Add(glm::vec3(3.0f, 0.0f, 4.5f));
	first->handPosition = glm::vec3(5.0f, 6.0f, 7.0f);
	first->player = PlayerNames::PLAYER_TWO;
	System().AddGestureTrail(first);
	auto second = std::make_shared<particles::GestureTrail>();
	second->fromInterface = false;
	System().AddGestureTrail(second);

	// the test still holds them, so they are copied and stay whole
	ASSERT_EQ(first->stroke.Size(), 2u);
	EXPECT_EQ(first->stroke[1], glm::vec3(3.0f, 0.0f, 4.0f));
	ASSERT_EQ(first->ideal.Size(), 2u);
	EXPECT_EQ(first->player, PlayerNames::PLAYER_TWO);
	ASSERT_EQ(pending.size(), 2u);
	EXPECT_EQ(pending[0].player, PlayerNames::PLAYER_TWO);
	EXPECT_TRUE(pending[0].fromInterface);
	EXPECT_EQ(pending[0].handPosition, glm::vec3(5.0f, 6.0f, 7.0f));
	ASSERT_EQ(pending[0].stroke.Size(), 2u);
	EXPECT_EQ(pending[0].stroke[1], glm::vec3(3.0f, 0.0f, 4.0f));
	ASSERT_EQ(pending[0].ideal.Size(), 2u);
	EXPECT_EQ(pending[0].ideal[0], glm::vec3(1.0f, 0.0f, 2.5f));
	EXPECT_EQ(pending[1].player, PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(pending[1].fromInterface);
	EXPECT_EQ(pending[1].stroke.Size(), 0u);
}

TEST_F(ParticleSystemTest, ABeliefSpriteReachesTheTownsQueueUntilItIsFull)
{
	// the towns' shared lists, empty, made here
	auto& towns = Locator::townStateSystem::emplace<ecs::systems::TownStateSystem>();
	const auto& queued = towns.BeliefSprites();

	System().AddBeliefSprite({.position = glm::vec3(1.0f, 2.0f, 3.0f), .amount = 250, .colour = 0x00FF00});
	ASSERT_EQ(queued.size(), 1u);
	EXPECT_EQ(queued[0].position, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_EQ(queued[0].amount, 250);
	EXPECT_EQ(queued[0].colour, 0x00FF00u);

	// up to the limit, in the order they came
	for (size_t i = queued.size(); i < ecs::town_belief::k_MaxBeliefSprites; ++i)
	{
		System().AddBeliefSprite({.position = glm::vec3(0.0f), .amount = static_cast<int32_t>(i), .colour = 0});
	}
	ASSERT_EQ(queued.size(), ecs::town_belief::k_MaxBeliefSprites);
	EXPECT_EQ(queued.back().amount, static_cast<int32_t>(ecs::town_belief::k_MaxBeliefSprites - 1));
	// one more is dropped
	System().AddBeliefSprite({.position = glm::vec3(0.0f), .amount = -1, .colour = 0});
	EXPECT_EQ(queued.size(), ecs::town_belief::k_MaxBeliefSprites);
	EXPECT_EQ(queued.back().amount, static_cast<int32_t>(ecs::town_belief::k_MaxBeliefSprites - 1));
}

namespace
{
/// The positions of an effect's sprites in Collect, in its order
std::vector<glm::vec3> Drawn(const std::vector<psys::manager::Drawable>& drawables, uint32_t effect)
{
	std::vector<glm::vec3> positions;
	for (const auto& drawable : drawables)
	{
		if (drawable.effect != effect)
		{
			continue;
		}
		for (const auto& atom : drawable.atoms)
		{
			positions.push_back(atom.position);
		}
	}
	return positions;
}

/// The keys of an effect's sprites in CollectSorted, in its order
std::vector<glm::vec3> SortedKeys(const psys::manager::SortedFrame& frame, uint32_t effect)
{
	std::vector<glm::vec3> keys;
	for (const auto& sprite : frame.sprites)
	{
		if (sprite.effect == effect)
		{
			keys.push_back(sprite.key);
		}
	}
	return keys;
}

/// The positions of an effect's items in CollectQueued, in its order
std::vector<glm::vec3> Queued(const std::vector<psys::manager::OrderedEffect>& effects, uint32_t effect)
{
	std::vector<glm::vec3> positions;
	for (const auto& ordered : effects)
	{
		if (ordered.effect != effect)
		{
			continue;
		}
		for (const auto& item : ordered.items)
		{
			positions.push_back(item.atom.position);
		}
	}
	return positions;
}
} // namespace

TEST_F(ParticleSystemTest, ADrawOffsetMovesOnlyItsEffectsDrawingByEachAtomsWeight)
{
	const auto moved = System().Start(k_Effect, glm::vec3(1.0f, 0.0f, 0.0f), 1.0f);
	const auto still = System().Start(k_Effect, glm::vec3(2.0f, 0.0f, 0.0f), 1.0f);
	const auto queued = System().Start(k_Effect, glm::vec3(3.0f, 0.0f, 0.0f), 1.0f);
	System().SetDrawPath(queued, psys::DrawPath::Queued);
	// stepped twice, so that their sprites are drawn
	for (const auto id : {moved, still, queued})
	{
		ASSERT_NE(System().Find(id), nullptr);
		System().Find(id)->Step(0.1f);
		System().Find(id)->Step(0.1f);
	}
	// no camera and no hand: the effects' own atoms only
	const psys::manager::FrameInputs inputs {.turnFraction = 1.0f};
	const auto movedBefore = Drawn(psys::manager::Collect(psys::Creator::Kind::Sprite, inputs), moved);
	const auto stillBefore = Drawn(psys::manager::Collect(psys::Creator::Kind::Sprite, inputs), still);
	const auto keysBefore = SortedKeys(psys::manager::CollectSorted(inputs), moved);
	const auto queuedBefore = Queued(psys::manager::CollectQueued(inputs), queued);
	ASSERT_FALSE(movedBefore.empty());
	ASSERT_EQ(keysBefore.size(), movedBefore.size());
	ASSERT_FALSE(queuedBefore.empty());

	// a zero offset leaves the drawing exactly as it was
	System().SetDrawOffset(moved, glm::vec3(0.0f));
	EXPECT_EQ(Drawn(psys::manager::Collect(psys::Creator::Kind::Sprite, inputs), moved), movedBefore);
	EXPECT_EQ(SortedKeys(psys::manager::CollectSorted(inputs), moved), keysBefore);

	// every atom's draw weight is 1 until a rule sets it; the moved effect's atoms take a quarter here, set as a rule
	// would set it, and the queued effect's keep the weight of 1
	constexpr float k_Weight = 0.25f;
	for (const auto& drawable : psys::manager::Collect(psys::Creator::Kind::Sprite, inputs))
	{
		for (const auto& drawn : drawable.atoms)
		{
			ASSERT_NE(drawn.atom, nullptr);
			EXPECT_FLOAT_EQ(drawn.atom->drawWeight, 1.0f);
			if (drawable.effect == moved)
			{
				const_cast<psys::Atom*>(drawn.atom)->drawWeight = k_Weight;
			}
		}
	}

	// each sprite takes the offset times its weight, and is sorted where it is drawn
	const glm::vec3 offset(0.5f, 4.0f, -2.0f);
	System().SetDrawOffset(moved, offset);
	System().SetDrawOffset(queued, offset);
	const auto movedAfter = Drawn(psys::manager::Collect(psys::Creator::Kind::Sprite, inputs), moved);
	const auto keysAfter = SortedKeys(psys::manager::CollectSorted(inputs), moved);
	const auto queuedAfter = Queued(psys::manager::CollectQueued(inputs), queued);
	ASSERT_EQ(movedAfter.size(), movedBefore.size());
	ASSERT_EQ(keysAfter.size(), keysBefore.size());
	ASSERT_EQ(queuedAfter.size(), queuedBefore.size());
	for (size_t i = 0; i < movedBefore.size(); ++i)
	{
		EXPECT_EQ(movedAfter[i], movedBefore[i] + offset * k_Weight) << i;
		for (int axis = 0; axis < 3; ++axis)
		{
			EXPECT_NEAR(keysAfter[i][axis], keysBefore[i][axis] + offset[axis] * k_Weight, 1e-4f) << i << " " << axis;
		}
	}
	for (size_t i = 0; i < queuedBefore.size(); ++i)
	{
		EXPECT_EQ(queuedAfter[i], queuedBefore[i] + offset) << i;
	}
	// the other effect is not moved, and an effect not running gets no offset
	EXPECT_EQ(Drawn(psys::manager::Collect(psys::Creator::Kind::Sprite, inputs), still), stillBefore);
	System().SetDrawOffset(queued + 1, offset);
	auto& effects = System().GetState().effects;
	EXPECT_EQ(std::distance(effects.begin(), effects.end()), 3);
}

TEST_F(ParticleSystemTest, TrailSizedByTheCameraDistance)
{
	// the trail is the gesture chain's file
	Write("SF_GestureChain");
	const glm::vec3 hand(10.0f, 5.0f, 20.0f);
	// nothing while the game is paused
	System().UpdateFrame(0.0f, {.position = hand, .size = 2.0f, .cameraPosition = hand});
	EXPECT_EQ(System().GetState().utility.trail, ParticleSystemInterface::k_NoEffect);
	// the camera on the hand (or no camera, the hand's own position): distance 0, the trail at a fifth of the hand's size
	System().UpdateFrame(0.1f, {.position = hand, .size = 2.0f, .cameraPosition = hand});
	const auto trail = System().GetState().utility.trail;
	ASSERT_NE(System().Find(trail), nullptr);
	EXPECT_FLOAT_EQ(System().Find(trail)->GetMagnitude(), 0.4f);
	// 25 away, halfway to the trail's full size at 50
	System().UpdateFrame(0.1f, {.position = hand, .size = 2.0f, .cameraPosition = hand + glm::vec3(0.0f, 0.0f, 25.0f)});
	EXPECT_EQ(System().GetState().utility.trail, trail);
	ASSERT_NE(System().Find(trail), nullptr);
	EXPECT_FLOAT_EQ(System().Find(trail)->GetMagnitude(), 1.2f);
}

TEST_F(ParticleSystemTest, SpotVisualTurnsGoToItsContainer)
{
	// the bonfire's spot visual, whose own life is 30 turns
	Write("SF_Bonfire");
	const auto turnsOf = [this](std::optional<int> turns) {
		const auto object =
		    System().StartSpotVisual(SpotVisualType::Bonfire, glm::vec3(100.0f, 0.0f, 200.0f), turns, entt::null);
		EXPECT_NE(object, k_NoObject);
		const auto& container = System().GetState().containers.front();
		EXPECT_EQ(container.object, object);
		EXPECT_NE(System().Find(container.effect), nullptr);
		if (const auto* effect = System().Find(container.effect); effect != nullptr)
		{
			EXPECT_EQ(effect->NetType(), NetGameType::Synced);
		}
		return container.turns;
	};
	EXPECT_EQ(turnsOf(std::nullopt), 30);
	EXPECT_EQ(turnsOf(-5), -1);
	EXPECT_EQ(turnsOf(-1), -1);
	EXPECT_EQ(turnsOf(0), 0);
	EXPECT_EQ(turnsOf(7), 7);
	// a spot visual without a spell file has no container
	EXPECT_EQ(System().StartSpotVisual(SpotVisualType::SpellSucceedCast, glm::vec3(0.0f), std::nullopt, entt::null),
	          k_NoObject);
}

namespace
{
/// A synthetic sheet of four points in a row, its strength fed in
std::shared_ptr<openblack::particles::LightSheet> MakeSheet()
{
	auto sheet = std::make_shared<openblack::particles::LightSheet>();
	sheet->Start({glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(2.0f, 0.0f, 0.0f), glm::vec3(3.0f, 0.0f, 0.0f)},
	             0x123456, 9.0f, 0.03f);
	sheet->SetStrength(0.5f);
	return sheet;
}

const ParticleSystemInterface::HandFrame k_Hand {
    .position = glm::vec3(10.0f, 5.0f, 20.0f), .size = 1.0f, .cameraPosition = glm::vec3(10.0f, 5.0f, 20.0f)};
} // namespace

TEST_F(ParticleSystemTest, AnAddedSheetIsListedAndMovedOnByEachFrame)
{
	Write("SF_GestureChain");
	const auto sheet = MakeSheet();
	System().AddLightSheet(sheet);
	const auto listed = System().LightSheets();
	ASSERT_EQ(listed.size(), 1u);
	EXPECT_EQ(listed.front(), sheet);

	// the frame moves it on exactly as its own Update by the frame's game seconds
	const auto twin = MakeSheet();
	twin->Update(0.1f);
	System().UpdateFrame(0.1f, k_Hand);
	EXPECT_EQ(sheet->Strengths(), twin->Strengths());
	EXPECT_EQ(sheet->Heights(), twin->Heights());
	EXPECT_NE(sheet->Strengths().front(), 0.0f);
	twin->Update(0.05f);
	System().UpdateFrame(0.05f, k_Hand);
	EXPECT_EQ(sheet->Strengths(), twin->Strengths());
	EXPECT_EQ(sheet->Heights(), twin->Heights());
}

TEST_F(ParticleSystemTest, ASheetDropsOutOnceItsLastOwnerGoes)
{
	auto first = MakeSheet();
	const auto second = MakeSheet();
	System().AddLightSheet(first);
	System().AddLightSheet(second);
	ASSERT_EQ(System().LightSheets().size(), 2u);
	first.reset();
	const auto listed = System().LightSheets();
	ASSERT_EQ(listed.size(), 1u);
	EXPECT_EQ(listed.front(), second);
	// the gone one is forgotten, not only skipped
	EXPECT_EQ(System().GetState().lightSheets.size(), 1u);
}

TEST_F(ParticleSystemTest, ANewLandForgetsTheSheets)
{
	Write("SF_GestureChain");
	const auto sheet = MakeSheet();
	System().AddLightSheet(sheet);
	System().Reset();
	EXPECT_TRUE(System().LightSheets().empty());
	EXPECT_TRUE(System().GetState().lightSheets.empty());
	// still held by its owner, but no longer moved on
	const auto heights = sheet->Heights();
	const auto strengths = sheet->Strengths();
	System().UpdateFrame(0.1f, k_Hand);
	EXPECT_EQ(sheet->Heights(), heights);
	EXPECT_EQ(sheet->Strengths(), strengths);
}

TEST_F(ParticleSystemTest, APausedFrameChangesNoSheet)
{
	Write("SF_GestureChain");
	const auto sheet = MakeSheet();
	System().AddLightSheet(sheet);
	System().UpdateFrame(0.1f, k_Hand);
	const auto heights = sheet->Heights();
	const auto strengths = sheet->Strengths();
	std::vector<openblack::particles::LightSheet::Vertex> vertices;
	std::vector<uint32_t> triangles;
	sheet->Build(vertices, triangles);
	const auto before = vertices;
	// paused: the frame's game time is 0
	System().UpdateFrame(0.0f, k_Hand);
	EXPECT_EQ(sheet->Heights(), heights);
	EXPECT_EQ(sheet->Strengths(), strengths);
	sheet->Build(vertices, triangles);
	ASSERT_EQ(vertices.size(), before.size());
	for (size_t i = 0; i < vertices.size(); ++i)
	{
		EXPECT_EQ(vertices[i].position, before[i].position) << i;
		EXPECT_EQ(vertices[i].uv, before[i].uv) << i;
		EXPECT_EQ(vertices[i].argb, before[i].argb) << i;
	}
}

namespace
{
/// The debug window's line for an effect; nullopt when it is not listed
std::optional<ParticleSystemInterface::EffectInfo> InfoOf(const ParticleSystemInterface& system, uint32_t id)
{
	for (const auto& info : system.GetEffects())
	{
		if (info.id == id)
		{
			return info;
		}
	}
	return std::nullopt;
}

/// A spell file with two classes the game doesn't run, one of them twice
constexpr std::string_view k_UnportedText =
    "BEGINPROPERTIES\n"
    "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
    "ENDPROPERTIES\n"
    "BEGINCLASS OpenblackTestRuleA FirstA\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\nENDPROPERTIES\nENDCLASS\n"
    "BEGINCLASS OpenblackTestRuleB OnlyB\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\nENDPROPERTIES\nENDCLASS\n"
    "BEGINCLASS OpenblackTestRuleA SecondA\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\nENDPROPERTIES\nENDCLASS\n";
} // namespace

TEST_F(ParticleSystemTest, TheDebugWindowListsEveryRunningEffectNewestFirst)
{
	EXPECT_TRUE(System().GetEffects().empty());
	const auto byTurn = System().Start(k_Effect, glm::vec3(1.0f, 2.0f, 3.0f), 1.0f);
	const auto bySpell =
	    System().StartForSpell(k_Effect, glm::vec3(4.0f, 5.0f, 6.0f), glm::vec3(1.0f, 0.0f, 0.0f), 1.0f, nullptr, false);
	ASSERT_NE(System().Find(byTurn), nullptr);
	ASSERT_NE(System().Find(bySpell), nullptr);
	System().AddTarget(byTurn, entt::entity {7});
	System().AddTargetPosition(byTurn, glm::vec3(8.0f));
	System().SetDrawPath(bySpell, psys::DrawPath::Queued);

	const auto effects = System().GetEffects();
	ASSERT_EQ(effects.size(), 2u);
	EXPECT_EQ(effects[0].id, bySpell);
	EXPECT_EQ(effects[1].id, byTurn);
	const auto& turn = effects[1];
	EXPECT_EQ(turn.file, k_Effect);
	EXPECT_EQ(turn.origin, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_FLOAT_EQ(turn.age, 0.0f);
	EXPECT_EQ(turn.atoms, System().Find(byTurn)->AtomCount());
	// the group created at the start
	EXPECT_EQ(turn.collections, 1u);
	EXPECT_FALSE(turn.closing);
	EXPECT_FALSE(turn.ownedBySpell);
	EXPECT_EQ(turn.path, psys::DrawPath::Sorted);
	// an object and a point
	EXPECT_EQ(turn.targets, 2u);
	EXPECT_FALSE(turn.secondsLeft.has_value());
	EXPECT_TRUE(turn.unportedClasses.empty());
	const auto& spell = effects[0];
	EXPECT_EQ(spell.origin, glm::vec3(4.0f, 5.0f, 6.0f));
	EXPECT_TRUE(spell.ownedBySpell);
	EXPECT_EQ(spell.path, psys::DrawPath::Queued);
	EXPECT_EQ(spell.targets, 0u);

	// a step and a close show
	System().ProcessTurn();
	System().CloseDown(byTurn);
	ASSERT_NE(System().Find(byTurn), nullptr);
	const auto stepped = InfoOf(System(), byTurn);
	ASSERT_TRUE(stepped.has_value());
	EXPECT_FLOAT_EQ(stepped->age, game_clock::k_TurnSeconds);
	EXPECT_EQ(stepped->atoms, System().Find(byTurn)->AtomCount());
	EXPECT_TRUE(stepped->closing);
	System().Delete(byTurn);
	EXPECT_FALSE(InfoOf(System(), byTurn).has_value());
	EXPECT_EQ(System().GetEffects().size(), 1u);
}

TEST_F(ParticleSystemTest, TheDebugWindowNamesTheClassesNotRunYetOnce)
{
	Write("openblack_test_unported", k_UnportedText);
	const auto id = System().Start("openblack_test_unported", glm::vec3(0.0f), 1.0f);
	ASSERT_NE(System().Find(id), nullptr);
	const auto info = InfoOf(System(), id);
	ASSERT_TRUE(info.has_value());
	EXPECT_EQ(info->unportedClasses, (std::vector<std::string> {"OpenblackTestRuleA", "OpenblackTestRuleB"}));
}

TEST_F(ParticleSystemTest, ASpotVisualsSecondsLeftAreItsTurns)
{
	Write("SF_Bonfire");
	const auto effectOf = [this](std::optional<int> turns) {
		const auto object =
		    System().StartSpotVisual(SpotVisualType::Bonfire, glm::vec3(100.0f, 0.0f, 200.0f), turns, entt::null);
		EXPECT_NE(object, k_NoObject);
		return System().GetState().containers.front().effect;
	};
	const auto forTurns = effectOf(7);
	const auto forEver = effectOf(-1);
	const auto timed = InfoOf(System(), forTurns);
	ASSERT_TRUE(timed.has_value());
	ASSERT_TRUE(timed->secondsLeft.has_value());
	EXPECT_FLOAT_EQ(*timed->secondsLeft, 7.0f * game_clock::k_TurnSeconds);
	const auto lasting = InfoOf(System(), forEver);
	ASSERT_TRUE(lasting.has_value());
	EXPECT_FALSE(lasting->secondsLeft.has_value());

	// a turn later, one turn less
	System().ProcessTurn();
	const auto later = InfoOf(System(), forTurns);
	ASSERT_TRUE(later.has_value());
	ASSERT_TRUE(later->secondsLeft.has_value());
	EXPECT_FLOAT_EQ(*later->secondsLeft, 6.0f * game_clock::k_TurnSeconds);
}

TEST_F(ParticleSystemTest, APausedTurnStepsNothing)
{
	Write("SF_Bonfire");
	EXPECT_FALSE(System().IsPaused());
	const auto id = System().Start(k_Effect, glm::vec3(0.0f), 1.0f);
	ASSERT_NE(System().StartSpotVisual(SpotVisualType::Bonfire, glm::vec3(100.0f, 0.0f, 200.0f), 7, entt::null), k_NoObject);
	const auto& container = System().GetState().containers.front();
	const auto visual = container.effect;
	ASSERT_NE(System().Find(id), nullptr);
	ASSERT_NE(System().Find(visual), nullptr);

	System().SetPaused(true);
	EXPECT_TRUE(System().IsPaused());
	System().ProcessTurn();
	System().ProcessTurn();
	EXPECT_FLOAT_EQ(System().Find(id)->GetAge(), 0.0f);
	EXPECT_FLOAT_EQ(System().Find(visual)->GetAge(), 0.0f);
	EXPECT_EQ(container.turns, 7);

	// unpaused, the turns step again from where they were
	System().SetPaused(false);
	EXPECT_FALSE(System().IsPaused());
	System().ProcessTurn();
	ASSERT_NE(System().Find(id), nullptr);
	ASSERT_NE(System().Find(visual), nullptr);
	EXPECT_FLOAT_EQ(System().Find(id)->GetAge(), game_clock::k_TurnSeconds);
	EXPECT_FLOAT_EQ(System().Find(visual)->GetAge(), game_clock::k_TurnSeconds);
	EXPECT_EQ(container.turns, 6);
}

TEST_F(ParticleSystemTest, APausedFrameOrSpellStepStepsNothing)
{
	const auto byFrame = System().Start(k_Effect, glm::vec3(0.0f), 1.0f);
	const auto spell = System().StartForSpell(k_Effect, glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), 1.0f, nullptr, false);
	ASSERT_NE(System().Find(byFrame), nullptr);
	ASSERT_NE(System().Find(spell), nullptr);

	// still running and taken by its owner, but not stepped
	System().SetPaused(true);
	EXPECT_TRUE(System().ProcessByFrame(byFrame, 0.25f));
	ASSERT_NE(System().Find(byFrame), nullptr);
	EXPECT_FLOAT_EQ(System().Find(byFrame)->GetAge(), 0.0f);
	const auto& running = System().GetState().effects.find(byFrame)->second;
	EXPECT_TRUE(running.ownedBySpell);
	EXPECT_TRUE(running.perFrame);
	EXPECT_TRUE(System().ProcessForSpell(spell, psys::ProcessInfo {}, 0.25f));
	ASSERT_NE(System().Find(spell), nullptr);
	EXPECT_FLOAT_EQ(System().Find(spell)->GetAge(), 0.0f);
	// an effect not running is still not running
	EXPECT_FALSE(System().ProcessByFrame(spell + 1, 0.25f));
	EXPECT_FALSE(System().ProcessForSpell(spell + 1, psys::ProcessInfo {}, 0.25f));

	// unpaused, they step again
	System().SetPaused(false);
	EXPECT_TRUE(System().ProcessByFrame(byFrame, 0.25f));
	ASSERT_NE(System().Find(byFrame), nullptr);
	EXPECT_FLOAT_EQ(System().Find(byFrame)->GetAge(), 0.25f);
	EXPECT_TRUE(System().ProcessForSpell(spell, psys::ProcessInfo {}, 0.25f));
	ASSERT_NE(System().Find(spell), nullptr);
	EXPECT_FLOAT_EQ(System().Find(spell)->GetAge(), 0.25f);
}

TEST_F(ParticleSystemTest, TheFileNamesAreTheSpellFilesEachOnce)
{
	const auto folder = _root / "Data" / "Spells" / "ZSpellFiles";
	Write("a");
	for (const auto* name : {"a_txt.zzz", "b_txt.zzz", "notes.dat"})
	{
		std::ofstream(folder / name, std::ios::binary).put('x');
	}
	EXPECT_EQ(System().GetFileNames(), (std::vector<std::string> {"a", "b", std::string(k_Effect)}));

	// none without the spell files' folder: a game folder that has none
	std::filesystem::create_directories(_root / "empty");
	Locator::filesystem::value().SetGamePath(_root / "empty");
	EXPECT_TRUE(System().GetFileNames().empty());
	Locator::filesystem::value().SetGamePath(_root);

	// none without a file system
	const auto fileSystem = Locator::filesystem::handle();
	Locator::filesystem::reset();
	EXPECT_TRUE(System().GetFileNames().empty());
	Locator::filesystem::reset(fileSystem);
	EXPECT_EQ(System().GetFileNames().size(), 3u);
}

TEST_F(ParticleSystemTest, FlaggedSpotVisualsTargetTheirOwner)
{
	// the five entries info.dat flags, and three it does not, each with a spell file of its own
	struct Entry
	{
		SpotVisualType type;
		std::string_view file;
		bool targetsOwner;
	};
	constexpr std::array<Entry, 8> k_Entries = {{
	    {SpotVisualType::HealFx, "SF_HealChakra", true},
	    {SpotVisualType::HighlightOnObject, "SF_HighlightOnObject", true},
	    {SpotVisualType::ButterfliesOnObject, "SF_ButterfliesOnObject", true},
	    {SpotVisualType::FliesOnObject, "SF_FliesOnObject", true},
	    {SpotVisualType::PilefoodSpeedup, "SF_SparklesFromObject", true},
	    {SpotVisualType::Bonfire, "SF_Bonfire", false},
	    {SpotVisualType::MagicBeam, "SF_SimpleBeam", false},
	    {SpotVisualType::Butterflies, "SF_Butterflies", false},
	}};
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto& entry : k_Entries)
	{
		Write(entry.file);
		const auto owner = registry.Create();
		for (const auto given : {owner, entt::entity {entt::null}})
		{
			const auto object = System().StartSpotVisual(entry.type, glm::vec3(100.0f, 0.0f, 200.0f), std::nullopt, given);
			ASSERT_NE(object, k_NoObject) << entry.file;
			const auto* effect = System().Find(System().GetState().containers.front().effect);
			ASSERT_NE(effect, nullptr) << entry.file;
			if (entry.targetsOwner && given != entt::null)
			{
				ASSERT_EQ(effect->GetTargets().size(), 1u) << entry.file;
				EXPECT_EQ(effect->GetTargets().front(), owner) << entry.file;
			}
			else
			{
				EXPECT_TRUE(effect->GetTargets().empty()) << entry.file;
			}
		}
	}
}

TEST_F(ParticleSystemTest, InfoBlockDecidesWhichSpotVisualsTargetTheirOwner)
{
	// with the info block, its flag is read, not the list kept for when there is none
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	auto info = std::make_unique<InfoConstants>();
	info->spotVisual.at(static_cast<size_t>(SpotVisualType::Bonfire)).targetOwnerObject = 1;
	info->spotVisual.at(static_cast<size_t>(SpotVisualType::PilefoodSpeedup)).targetOwnerObject = 0;
	Locator::infoConstants::reset(info.release());
	Write("SF_Bonfire");
	Write("SF_SparklesFromObject");
	const auto owner = Locator::entitiesRegistry::value().Create();
	const auto targetsOf = [&](SpotVisualType type) {
		EXPECT_NE(System().StartSpotVisual(type, glm::vec3(0.0f), std::nullopt, owner), k_NoObject);
		return System().Find(System().GetState().containers.front().effect)->GetTargets().size();
	};
	EXPECT_EQ(targetsOf(SpotVisualType::Bonfire), 1u);
	EXPECT_EQ(targetsOf(SpotVisualType::PilefoodSpeedup), 0u);
}

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(SpotVisualInfo, realInfoDatFlagsTheOwnerTargets)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	std::ifstream file(std::filesystem::path(game) / "Scripts" / "info.dat", std::ios::binary);
	ASSERT_TRUE(file.is_open());
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	ASSERT_EQ(data.size(), 0x2C + sizeof(InfoConstants));
	auto info = std::make_unique<InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(InfoConstants));
	// the same five entries as the list the manager keeps for when there is no info block
	std::vector<size_t> flagged;
	for (size_t i = 0; i < info->spotVisual.size(); ++i)
	{
		if (info->spotVisual.at(i).targetOwnerObject != 0)
		{
			flagged.push_back(i);
		}
	}
	EXPECT_EQ(flagged, (std::vector<size_t> {33, 34, 38, 40, 46}));
}
