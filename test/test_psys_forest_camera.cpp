/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The forest miracle's camera particle (Particles/Rules/Forest.h, ParticleAnimWithCameraCreator): the step and draw rules
// on their own, then the creator on small synthetic effects with a recording camera path system, and SF_Forest's own
// numbers from the game's data (skipped without it).

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <entt/resource/resource.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/CameraPath.h"
#include "3D/L3DAnim.h"
#include "Common/GameRandom.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "FileSystem/DefaultFileSystem.h"
#include "Locator.h"
#include "Particles/Creators/Mesh.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysRegistry.h"
#include "Particles/Rules/Forest.h"
#include "Particles/SpellLink.h"
#include "Resources/Loaders.h"
#include "Resources/Resources.h"
#include "Resources/ResourcesInterface.h"
#include "support/RestoreService.h"

using namespace openblack;
namespace fc = openblack::psys::forest_camera;

namespace
{
using PathOwner = ecs::systems::CameraPathSystemInterface::PathOwner;

/// Records what the camera particles ask of the camera paths, and grants every Begin
class RecordingCameraPaths final: public ecs::systems::CameraPathSystemInterface
{
public:
	struct Begun
	{
		PathOwner owner;
		bool hasPath;
		glm::mat4 placement;
		float pauseSeconds;
	};
	struct Follow
	{
		PathOwner owner;
		int32_t pathMilliseconds;
		bool playing;
	};
	std::vector<Begun> begun;
	std::vector<Follow> follows;
	std::vector<PathOwner> released;

	void Start(entt::id_type /*id*/) override {}
	void Stop() override {}
	void Play() override {}
	void Pause() override {}
	void Update(const std::chrono::microseconds& /*dt*/) override {}
	bool IsPathing() override { return false; }
	bool IsPaused() override { return false; }
	bool Begin(PathOwner owner, entt::resource<CameraPath> path, const glm::mat4& placement, float pauseSeconds) override
	{
		begun.push_back({owner, static_cast<bool>(path), placement, pauseSeconds});
		return true;
	}
	void FollowAt(PathOwner owner, int32_t pathMilliseconds, bool playing) override
	{
		follows.push_back({owner, pathMilliseconds, playing});
	}
	void Release(PathOwner owner) override { released.push_back(owner); }
	[[nodiscard]] bool HoldsCamera() const override { return false; }
	[[nodiscard]] std::optional<PlacedReadout> CurrentPlaced() const override { return std::nullopt; }
	void HandlePlayerControl(const PlayerControl& /*control*/) override {}
};

/// A spell that is another player's: its effects are not the local player's
class OtherPlayersSpell final: public psys::SpellSink
{
public:
	int SpellEvent(const psys::SpellEventInfo& /*event*/) override { return 0; }
	[[nodiscard]] int PowerUpLevel() const override { return 0; }
	[[nodiscard]] bool IsMyInterfaceCasting() const override { return false; }
};

const std::string k_Header = "BEGINPROPERTIES\n"
                             "PROPERTY DeleteOnCloseDown BOOL 0\n"
                             "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
                             "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
                             "ENDPROPERTIES\n";

/// The camera creator as SF_Forest writes it, with no clip (so its frame never moves by itself), a pause of 2.5 s, and
/// `className` for its class
std::string CameraCreator(const std::string& className, float initialScale = 0.5f)
{
	return "BEGINCLASS " + className +
	       " Camera0\nBEGINPROPERTIES\n"
	       "PROPERTY AnimFileName STRING NULL_STRING\n"
	       "PROPERTY CameraFileName STRING .\\Data\\SPELLS\\Anims\\Forest.cam\n"
	       "PROPERTY InitialScale FLOAT " +
	       std::to_string(initialScale) +
	       "\n"
	       "PROPERTY LoopAnim BOOL 0\n"
	       "PROPERTY MeshFileName STRING NULL_STRING\n"
	       "PROPERTY PauseBeforePlay FLOAT 2.5\n"
	       "PROPERTY PlayAnim BOOL 0\n"
	       "PROPERTY RandomiseInitFrame BOOL 0\n"
	       "ENDPROPERTIES\nENDCLASS\n";
}

/// One atom of Camera0 in group 0, gone past dieAge
std::string OneAtom(float dieAge)
{
	return "BEGINCLASS CreateRuleAnAtom Create0\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
	       "PROPERTY NextGroups ARRAY SIZE 0\nPROPERTY PCreator PERSIS_PNTR Camera0\nENDPROPERTIES\nENDCLASS\n"
	       "BEGINCLASS RemoveRuleOldAgeOnly Remove0\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\nPROPERTY DieAge FLOAT " +
	       std::to_string(dieAge) + "\nENDPROPERTIES\nENDCLASS\n";
}

std::shared_ptr<const psys::File> Parse(const std::string& text)
{
	auto file = psys::File::Parse(text, "SF_ForestCameraTest");
	return file.has_value() ? std::make_shared<const psys::File>(std::move(*file)) : nullptr;
}

/// The effect's only atom (the camera creator's atoms are mesh atoms)
psys::Atom* OnlyAtom(const psys::Effect& effect)
{
	std::vector<psys::Effect::DrawAtom> drawn;
	effect.Collect(1.0f, drawn, psys::Creator::Kind::Mesh);
	return drawn.size() == 1 ? const_cast<psys::Atom*>(drawn.front().atom) : nullptr;
}

PathOwner OwnerOf(const psys::Atom* atom)
{
	return static_cast<PathOwner>(reinterpret_cast<uintptr_t>(atom));
}

/// The game's folder (OPENBLACK_GAME_PATH or OPENBLACK_TEST_GAME_PATH), none without it
std::optional<std::filesystem::path> GamePath()
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		game = std::getenv("OPENBLACK_TEST_GAME_PATH");
	}
	return game != nullptr ? std::optional<std::filesystem::path>(game) : std::nullopt;
}

class ForestCamera: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// the anim creator and the effects warn on the game's logger (no mesh, no animation, a class not ported)
		if (!spdlog::get("game"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		_paths = &static_cast<RecordingCameraPaths&>(Locator::cameraPathSystem::emplace<RecordingCameraPaths>());
	}
	[[nodiscard]] RecordingCameraPaths& Paths() const { return *_paths; }

private:
	// built before SetUp: the camera path system before the test comes back after it
	const test::RestoreService<Locator::cameraPathSystem> _restorePaths;
	RecordingCameraPaths* _paths {nullptr};
};
} // namespace

// ---- the rules on their own ----

TEST(ForestCameraRules, TheFirstStepThatIsMineAsksForTheCameraOnce)
{
	fc::Flags flags;
	const auto first = fc::Step(flags, true, 0.1f, 4.0f);
	EXPECT_TRUE(first.begin);
	EXPECT_FALSE(first.release);
	EXPECT_FALSE(first.playAnim);
	EXPECT_TRUE(flags.asked);
	EXPECT_TRUE(flags.wanted);
	EXPECT_FALSE(flags.playing);
	for (int i = 0; i < 5; ++i)
	{
		EXPECT_FALSE(fc::Step(flags, true, 0.2f, 4.0f).begin);
	}
}

TEST(ForestCameraRules, ItPlaysFromALaterStepOnceOlderThanThePauseNotAtIt)
{
	fc::Flags flags;
	(void)fc::Step(flags, true, 0.1f, 4.0f);
	// at the pause: not older than it
	const auto atPause = fc::Step(flags, true, 4.0f, 4.0f);
	EXPECT_FALSE(atPause.playAnim);
	EXPECT_FALSE(flags.playing);
	const auto after = fc::Step(flags, true, 4.1f, 4.0f);
	EXPECT_TRUE(after.playAnim);
	EXPECT_TRUE(flags.playing);
}

TEST(ForestCameraRules, AFirstStepPastThePausePlaysTheAnimationButNotYetThePath)
{
	fc::Flags flags;
	const auto first = fc::Step(flags, true, 5.0f, 4.0f);
	EXPECT_TRUE(first.begin);
	EXPECT_TRUE(first.playAnim);
	EXPECT_FALSE(flags.playing);
	(void)fc::Step(flags, true, 5.1f, 4.0f);
	EXPECT_TRUE(flags.playing);
}

TEST(ForestCameraRules, AStepThatIsNotMineNeverAsksAndTheWishIsGoneForGood)
{
	fc::Flags flags;
	const auto notMine = fc::Step(flags, false, 0.1f, 4.0f);
	EXPECT_FALSE(notMine.begin);
	EXPECT_FALSE(notMine.release);
	EXPECT_FALSE(flags.wanted);
	const auto mineAgain = fc::Step(flags, true, 5.0f, 4.0f);
	EXPECT_FALSE(mineAgain.begin);
	// the animation plays all the same
	EXPECT_TRUE(mineAgain.playAnim);
	EXPECT_FALSE(flags.playing);
}

TEST(ForestCameraRules, NoLongerMineAfterAskingTheStopFindsTheWishClearedAndLetsNothingGo)
{
	fc::Flags flags;
	(void)fc::Step(flags, true, 0.1f, 4.0f);
	(void)fc::Step(flags, true, 4.5f, 4.0f);
	ASSERT_TRUE(flags.playing);
	const auto notMine = fc::Step(flags, false, 4.6f, 4.0f);
	EXPECT_FALSE(notMine.release);
	EXPECT_FALSE(flags.wanted);
	// it still plays, and neither the last frame nor the removal lets anything go
	EXPECT_TRUE(flags.playing);
	EXPECT_FALSE(fc::Draw(flags, 6633, 999).release);
	EXPECT_FALSE(fc::Removed(flags));
}

TEST(ForestCameraRules, TheDrawnFrameGivesThePathItsTimeInWholeFrames)
{
	fc::Flags flags;
	EXPECT_EQ(fc::Draw(flags, 6633, 0).pathMilliseconds, 0);
	EXPECT_EQ(fc::Draw(flags, 6633, 1).pathMilliseconds, 6);
	EXPECT_EQ(fc::Draw(flags, 6633, 500).pathMilliseconds, 3316);
	EXPECT_EQ(fc::Draw(flags, 6633, 999).pathMilliseconds, 6626);
}

TEST(ForestCameraRules, TheLastFrameLetsGoOnceAndOnlyOnceTheCameraWasAskedFor)
{
	fc::Flags flags;
	// not asked yet: nothing to let go of
	EXPECT_FALSE(fc::Draw(flags, 6633, 999).release);
	EXPECT_TRUE(flags.wanted);
	(void)fc::Step(flags, true, 0.1f, 4.0f);
	EXPECT_FALSE(fc::Draw(flags, 6633, 998).release);
	EXPECT_TRUE(fc::Draw(flags, 6633, 999).release);
	EXPECT_FALSE(flags.wanted);
	EXPECT_FALSE(fc::Draw(flags, 6633, 999).release);
	// gone for good: no later step asks again, and the removal lets nothing go
	EXPECT_FALSE(fc::Step(flags, true, 20.0f, 4.0f).begin);
	EXPECT_FALSE(fc::Removed(flags));
}

TEST(ForestCameraRules, TheRemovalLetsGoOnlyOfACameraAskedForAndStillWanted)
{
	fc::Flags never;
	EXPECT_FALSE(fc::Removed(never));
	fc::Flags asked;
	(void)fc::Step(asked, true, 0.1f, 4.0f);
	EXPECT_TRUE(fc::Removed(asked));
	EXPECT_FALSE(fc::Removed(asked));
}

// ---- the creator on synthetic effects ----

TEST_F(ForestCamera, TheCreatorIsTheAnimCreatorsWithNoMesh)
{
	ASSERT_NE(psys::FindCreatorFactory("ParticleAnimWithCameraCreator"), nullptr);
	const auto file = Parse(k_Header + CameraCreator("ParticleAnimWithCameraCreator") + OneAtom(30.0f));
	ASSERT_NE(file, nullptr);
	const psys::Effect effect(file, glm::vec3(0.0f), 1.0f);
	const auto* creator = dynamic_cast<const psys::MeshCreator*>(effect.FindCreator("Camera0"));
	ASSERT_NE(creator, nullptr);
	EXPECT_EQ(creator->className, "ParticleAnimWithCameraCreator");
	EXPECT_EQ(creator->kind, psys::Creator::Kind::Mesh);
	EXPECT_TRUE(creator->animated);
	EXPECT_EQ(creator->meshId, 0u);
	EXPECT_EQ(creator->FramesPerAtom(), psys::k_AnimFrames);
	EXPECT_FALSE(creator->animPlay);
	EXPECT_FALSE(creator->loopAnim);
}

TEST_F(ForestCamera, ItAsksForTheCameraAtItsFirstStepAtTheParticleThenFollowsAndPlaysAfterThePause)
{
	const auto file = Parse(k_Header + CameraCreator("ParticleAnimWithCameraCreator") + OneAtom(30.0f));
	ASSERT_NE(file, nullptr);
	const glm::vec3 origin(100.0f, 20.0f, 300.0f);
	psys::Effect effect(file, origin, 1.0f);
	effect.Step(0.1f);
	auto* atom = OnlyAtom(effect);
	ASSERT_NE(atom, nullptr);
	ASSERT_EQ(Paths().begun.size(), 1u);
	const auto& begun = Paths().begun.front();
	EXPECT_EQ(begun.owner, OwnerOf(atom));
	EXPECT_FLOAT_EQ(begun.pauseSeconds, 2.5f);
	// no camera path cache in the tests: the system gets no path, which it refuses
	EXPECT_FALSE(begun.hasPath);
	// placed at the particle: its scale on the axes, its position
	EXPECT_FLOAT_EQ(begun.placement[0][0], 0.5f);
	EXPECT_FLOAT_EQ(begun.placement[1][1], 0.5f);
	EXPECT_FLOAT_EQ(begun.placement[2][2], 0.5f);
	EXPECT_FLOAT_EQ(begun.placement[3][0], origin.x);
	EXPECT_FLOAT_EQ(begun.placement[3][1], origin.y);
	EXPECT_FLOAT_EQ(begun.placement[3][2], origin.z);
	EXPECT_FLOAT_EQ(begun.placement[3][3], 1.0f);
	// every frame its drawn frame gives the path its time. It plays, and so does its animation, from the same step: the
	// first whose age during the step (0.1 s less than after it) is past the pause
	std::optional<float> playingFrom;
	std::optional<float> playAnimFrom;
	for (int step = 2; step <= 40; ++step)
	{
		effect.Step(0.1f);
		const float age = effect.AtomAge(*atom);
		Paths().follows.clear();
		fc::UpdateFrame(0.5f);
		ASSERT_EQ(Paths().follows.size(), 1u) << "step " << step;
		const auto& follow = Paths().follows.front();
		EXPECT_EQ(follow.owner, OwnerOf(atom));
		EXPECT_EQ(follow.pathMilliseconds, 0); // no clip
		// once on, both stay on
		EXPECT_TRUE(!playingFrom.has_value() || follow.playing) << "age " << age;
		EXPECT_TRUE(!playAnimFrom.has_value() || atom->playAnim) << "age " << age;
		if (follow.playing && !playingFrom.has_value())
		{
			playingFrom = age;
		}
		if (atom->playAnim && !playAnimFrom.has_value())
		{
			playAnimFrom = age;
		}
	}
	ASSERT_TRUE(playingFrom.has_value());
	ASSERT_TRUE(playAnimFrom.has_value());
	EXPECT_FLOAT_EQ(*playingFrom, *playAnimFrom);
	EXPECT_GT(*playingFrom, 2.55f);
	EXPECT_LT(*playingFrom, 2.75f);
	EXPECT_EQ(Paths().begun.size(), 1u);
	EXPECT_TRUE(Paths().released.empty());
}

TEST_F(ForestCamera, TheLastDrawnFrameLetsTheCameraGoOnceAndTheRemovalDoesNotAgain)
{
	const auto file = Parse(k_Header + CameraCreator("ParticleAnimWithCameraCreator") + OneAtom(30.0f));
	ASSERT_NE(file, nullptr);
	PathOwner owner = 0;
	{
		psys::Effect effect(file, glm::vec3(0.0f), 1.0f);
		effect.Step(0.1f);
		auto* atom = OnlyAtom(effect);
		ASSERT_NE(atom, nullptr);
		owner = OwnerOf(atom);
		// the frame drawn between the last two steps: 998 then 999
		atom->previous.frame = 998.0f;
		atom->current.frame = 998.0f;
		fc::UpdateFrame(0.5f);
		EXPECT_TRUE(Paths().released.empty());
		atom->current.frame = 1000.0f;
		fc::UpdateFrame(0.0f);
		EXPECT_TRUE(Paths().released.empty()); // 998 drawn at the start of the step
		fc::UpdateFrame(0.5f);
		ASSERT_EQ(Paths().released.size(), 1u); // 999
		EXPECT_EQ(Paths().released.front(), owner);
		ASSERT_FALSE(Paths().follows.empty());
		EXPECT_EQ(Paths().follows.back().pathMilliseconds, 0); // no clip
		const auto watches = fc::Watches();
		ASSERT_EQ(watches.size(), 1u);
		EXPECT_EQ(watches.front().owner, owner);
		EXPECT_EQ(watches.front().drawnFrame, 999);
		EXPECT_FALSE(watches.front().flags.wanted);
		fc::UpdateFrame(0.5f);
		EXPECT_EQ(Paths().released.size(), 1u);
	}
	// the effect's destruction removes the particle without letting go again
	EXPECT_EQ(Paths().released.size(), 1u);
	EXPECT_TRUE(fc::Watches().empty());
}

TEST_F(ForestCamera, TheParticleRemovedBeforeItsLastFrameLetsTheCameraGo)
{
	// the remove rule takes it at 1 s
	const auto file = Parse(k_Header + CameraCreator("ParticleAnimWithCameraCreator") + OneAtom(1.0f));
	ASSERT_NE(file, nullptr);
	psys::Effect effect(file, glm::vec3(0.0f), 1.0f);
	effect.Step(0.1f);
	const auto* atom = OnlyAtom(effect);
	ASSERT_NE(atom, nullptr);
	const auto owner = OwnerOf(atom);
	ASSERT_EQ(Paths().begun.size(), 1u);
	for (int step = 0; step < 15 && effect.AtomCount() != 0; ++step)
	{
		effect.Step(0.1f);
	}
	EXPECT_EQ(effect.AtomCount(), 0u);
	ASSERT_EQ(Paths().released.size(), 1u);
	EXPECT_EQ(Paths().released.front(), owner);
	EXPECT_TRUE(fc::Watches().empty());
}

TEST_F(ForestCamera, AnotherPlayersEffectNeverAsksForTheCamera)
{
	const auto file = Parse(k_Header + CameraCreator("ParticleAnimWithCameraCreator") + OneAtom(30.0f));
	ASSERT_NE(file, nullptr);
	OtherPlayersSpell spell;
	psys::Effect effect(file, glm::vec3(0.0f), 1.0f);
	effect.SetSink(&spell);
	for (int step = 0; step < 40; ++step)
	{
		effect.Step(0.1f);
		fc::UpdateFrame(0.5f);
	}
	EXPECT_TRUE(Paths().begun.empty());
	EXPECT_TRUE(Paths().released.empty());
	// its animation plays all the same
	const auto* atom = OnlyAtom(effect);
	ASSERT_NE(atom, nullptr);
	EXPECT_TRUE(atom->playAnim);
}

TEST_F(ForestCamera, ItDrawsNoRandomNumberMoreThanTheUnportedCreatorItReplaces)
{
	// the same effect with the class as it was before the port (a plain creator) and with the port: the same draws
	const auto seedsAfter = [](const std::string& className) {
		const auto file = Parse(k_Header + CameraCreator(className) + OneAtom(3.0f));
		EXPECT_NE(file, nullptr);
		game_random::Load({.synced = 12345u, .local = 67890u});
		{
			psys::Effect effect(file, glm::vec3(0.0f), 1.0f);
			for (int step = 0; step < 40; ++step)
			{
				effect.Step(0.1f);
				fc::UpdateFrame(0.5f);
			}
		}
		return game_random::Current();
	};
	const auto unported = seedsAfter("ParticleUnportedCameraTestCreator");
	const auto ported = seedsAfter("ParticleAnimWithCameraCreator");
	EXPECT_EQ(ported.synced, unported.synced);
	EXPECT_EQ(ported.local, unported.local);
}

// ---- SF_Forest's own numbers ----

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it. The synthetic twins are
// the ForestCamera tests above
TEST_F(ForestCamera, SFForestsCameraTakesTheCameraForItsAnimationsLengthAfterFourSeconds)
{
	const auto game = GamePath();
	if (!game.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const test::RestoreService<Locator::filesystem> restoreFilesystem;
	const test::RestoreService<Locator::resources> restoreResources;
	Locator::filesystem::emplace<filesystem::DefaultFileSystem>();
	Locator::filesystem::value().SetGamePath(*game);
	Locator::resources::emplace<resources::Resources>();

	const auto forest = psys::File::Load("SF_Forest");
	ASSERT_NE(forest, nullptr);
	const auto* cameraObject = forest->Find("ParticleAnimWithCameraCreator0");
	const auto* create = forest->Find("CreateRuleAnAtom_Camera");
	const auto* remove = forest->Find("RemoveRuleOldAgeOnly_Camera");
	ASSERT_NE(cameraObject, nullptr);
	ASSERT_NE(create, nullptr);
	ASSERT_NE(remove, nullptr);
	EXPECT_EQ(cameraObject->className, "ParticleAnimWithCameraCreator");
	EXPECT_FLOAT_EQ(cameraObject->Float("PauseBeforePlay", 0.0f), 4.0f);
	EXPECT_FLOAT_EQ(cameraObject->Float("SpeedUpFactor", 0.0f), 0.6f);
	// MeshFileName is written NULL_STRING, which the spell file parser reads as no text
	EXPECT_TRUE(cameraObject->Has("MeshFileName"));
	EXPECT_EQ(cameraObject->String("MeshFileName"), "");
	EXPECT_FALSE(cameraObject->Has("MeshEnum"));
	EXPECT_FLOAT_EQ(remove->Float("DieAge", 0.0f), 25.0f);

	// the path and the clip both last 6633 ms; the path has 198 points
	const auto anims = Locator::filesystem::value().GetPath<filesystem::Path::Data>() / "Spells" / "Anims";
	CameraPath path;
	ASSERT_TRUE(path.LoadFromFile(Locator::filesystem::value().FindPath(anims / "Forest.cam")));
	EXPECT_EQ(path.GetDuration(), std::chrono::milliseconds(6633));
	EXPECT_EQ(path.GetPoints().size(), 198u);
	L3DAnim clip;
	ASSERT_TRUE(clip.LoadFromFile(Locator::filesystem::value().FindPath(anims / "Tree_Goddess_test.anm")));
	EXPECT_EQ(clip.GetDurationMs(), 6633);

	// the camera group alone: its creator, its one atom and its removal at 25 s, at the forest's place
	psys::File cameraGroup;
	cameraGroup.name = "SF_ForestCamera";
	cameraGroup.header = forest->header;
	cameraGroup.objects = {*cameraObject, *create, *remove};
	const glm::vec3 origin(1000.0f, 30.0f, 2000.0f);
	psys::Effect effect(std::make_shared<const psys::File>(std::move(cameraGroup)), origin, 1.0f);
	effect.Step(0.1f);
	const auto* atom = OnlyAtom(effect);
	ASSERT_NE(atom, nullptr);
	const auto owner = OwnerOf(atom);
	ASSERT_EQ(Paths().begun.size(), 1u);
	EXPECT_TRUE(Paths().begun.front().hasPath);
	EXPECT_FLOAT_EQ(Paths().begun.front().pauseSeconds, 4.0f);
	EXPECT_FLOAT_EQ(Paths().begun.front().placement[0][0], 0.140486f);
	EXPECT_FLOAT_EQ(Paths().begun.front().placement[3][0], origin.x);
	// played from the step after it is older than 4 s, 90.45 frames a second: the last frame comes about 11 s later
	std::optional<float> playedAt;
	std::optional<float> releasedAt;
	int32_t lastTime = -1;
	for (int step = 0; step < 300 && effect.AtomCount() != 0; ++step)
	{
		effect.Step(0.1f);
		if (effect.AtomCount() == 0)
		{
			break;
		}
		const float age = effect.AtomAge(*atom);
		Paths().follows.clear();
		fc::UpdateFrame(0.5f);
		ASSERT_EQ(Paths().follows.size(), 1u);
		const auto& follow = Paths().follows.front();
		EXPECT_GE(follow.pathMilliseconds, lastTime) << "the path's time never goes back";
		lastTime = follow.pathMilliseconds;
		if (follow.playing && !playedAt.has_value())
		{
			playedAt = age;
		}
		if (!Paths().released.empty() && !releasedAt.has_value())
		{
			releasedAt = age;
			EXPECT_EQ(follow.pathMilliseconds, 6626);
		}
	}
	// the ages are read after each step, 0.1 s past the age the step saw
	ASSERT_TRUE(playedAt.has_value());
	EXPECT_GT(*playedAt, 4.05f);
	EXPECT_LT(*playedAt, 4.25f);
	ASSERT_TRUE(releasedAt.has_value());
	EXPECT_GT(*releasedAt, 15.0f);
	EXPECT_LT(*releasedAt, 15.4f);
	// once, at the last frame: the removal at 25 s lets nothing go again
	ASSERT_EQ(Paths().released.size(), 1u);
	EXPECT_EQ(Paths().released.front(), owner);
	EXPECT_EQ(effect.AtomCount(), 0u);
}
