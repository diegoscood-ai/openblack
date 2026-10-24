/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The teleport miracle pinned to the original: the stone's pool hums with a looping sound while the stone stands, one
// per stone, and lets it fade out softly when the stone goes; a stone that goes takes its reactions, its pool and its
// travellers with it; and each player's turn drops the travellers who no longer follow the stone's reaction. Wiki:
// docs/wiki/magic/teleport.md#teleport.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Audio/AudioManagerNoOp.h"
#include "Audio/Engine/AnimEffects.h"
#include "Audio/Engine/SamplePlay.h"
#include "Audio/Game/Banks.h"
#include "Audio/Services/SpellSounds.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/VillagerTeleportState.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "Enums.h"
#include "FileSystem/DefaultFileSystem.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Magic/Objects/MagicTeleport.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/SoundAction.h"
#include "Resources/Loaders.h"
#include "Resources/Resources.h"
#include "Resources/ResourcesInterface.h"
#include "support/ParticleFakes.h"
#include "support/RestoreService.h"
#include "support/WorldSystems.h"

using namespace openblack;
using openblack::ecs::components::TeleportStone;
using openblack::ecs::components::VillagerTeleportState;
namespace reactions = openblack::ecs::effects::reactions;

namespace
{
/// The sound part of SF_TeleportVortex, as the game's file has it: the root atom, made at once, plays the pool's sound,
/// looping and released softly, and makes the group 2 atom the pool hangs from, which plays nothing
constexpr std::string_view k_VortexSounds = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 1
PROPERTY Hierarchies ARRAY SIZE 25 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY MaxSpellAge FLOAT -1
ENDPROPERTIES
BEGINCLASS CreateRuleAnAtom CreateRuleAnAtom0
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 0
PROPERTY InitScaleFP PERSIS_PNTR NULL_STRING
PROPERTY NextGroups ARRAY SIZE 1 2
PROPERTY OffsetX FLOAT 0
PROPERTY OffsetY FLOAT 0
PROPERTY OffsetZ FLOAT 0
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator0
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY SoundOfCreate SOUND_ACTION SOUND_SPELL_TELEPORT_POOL LOOPING 1 ONLYONE 0 SOFTRELEASE 1 USESURFACE 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleAnAtom CreateRuleAnAtom2
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 2
PROPERTY InitScaleFP PERSIS_PNTR NULL_STRING
PROPERTY NextGroups ARRAY SIZE 0
PROPERTY OffsetX FLOAT 0
PROPERTY OffsetY FLOAT 0.1
PROPERTY OffsetZ FLOAT 0
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator0
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY SoundOfCreate SOUND_ACTION NO_SOUND LOOPING 0 ONLYONE 0 SOFTRELEASE 0 USESURFACE 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticlePointCreator ParticlePointCreator0
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 2
PROPERTY LoopAnim BOOL 1
ENDPROPERTIES
ENDCLASS
)";

/// A tiny SoundAction.h with the pool's sound, at the value the game's own file gives it
constexpr std::string_view k_SyntheticSoundActions = "enum LHSoundAction\n{\n\tNO_SOUND_ACTION = 0,\n"
                                                     "\tSOUND_SPELL_TELEPORT_POOL = 76,\n};\n";
constexpr int32_t k_PoolSound = 76;

std::shared_ptr<const particles::File> Parse(std::string_view text, const char* name)
{
	auto file = particles::File::Parse(text, name);
	return file.has_value() ? std::make_shared<const particles::File>(*file) : nullptr;
}

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

/// An audio service that hears every animation effect asked of it, and has a channel still playing for every owner,
/// so a particle sound whose atom has gone is released rather than forgotten
class HeardAnimEffects final: public audio::AudioManagerNoOp
{
public:
	struct Request
	{
		audio::AnimKey key;
		audio::AnimAction action;
	};
	[[nodiscard]] audio::Channel PlayingChannel(audio::Owner /*owner*/, audio::BankId /*bank*/) override { return 7; }
	audio::Channel PlayAnimationEffect(audio::Owner /*owner*/, float /*distance*/, const audio::AnimKey& key,
	                                   audio::AnimAction action, audio::BankId /*bank*/, bool /*track*/, float /*minDistance*/,
	                                   float /*maxDistance*/) override
	{
		requests.push_back({key, action});
		return audio::k_NoChannel;
	}
	/// The actions asked for the sounds of that action (the key's last attribute), in order
	[[nodiscard]] std::vector<audio::AnimAction> ActionsOf(int32_t sound) const
	{
		std::vector<audio::AnimAction> actions;
		for (const auto& request : requests)
		{
			if (request.key[4] == sound)
			{
				actions.push_back(request.action);
			}
		}
		return actions;
	}
	std::vector<Request> requests;
};

/// The sounds of that action on the drawn point atoms of an effect
std::vector<const audio::ParticleSound*> SoundsOf(const particles::Effect& effect, int32_t action)
{
	std::vector<particles::Effect::DrawAtom> drawn;
	effect.Collect(1.0f, drawn, particles::Creator::Kind::Point);
	std::vector<const audio::ParticleSound*> found;
	for (const auto& atom : drawn)
	{
		if (atom.atom == nullptr)
		{
			continue;
		}
		for (const auto& sound : atom.atom->sounds)
		{
			if (sound != nullptr && sound->action.action == action)
			{
				found.push_back(sound.get());
			}
		}
	}
	return found;
}

/// The particle tests set the game turn and leave particle sounds: both go after each one, also when an ASSERT stops it
class TeleportMiracleSounds: public ::testing::Test
{
protected:
	void TearDown() override
	{
		audio::spell_sounds::Clear();
		game_clock::SetTurn(0);
	}
};
} // namespace

// Each stone's pool plays its humming sound, looping and released softly, from the moment it is made: one per stone,
// and never stopped or let go while the stone stands (docs/wiki/audio/magic-and-weather-sounds.md, the lifetime of the
// particle sound; the sounds of SF_TeleportVortex)
TEST_F(TeleportMiracleSounds, AHummingLoopPlaysAtEachStoneWhileItStands)
{
	const SyntheticSoundActions soundActions;
	ASSERT_EQ(particles::SoundActionByName("SOUND_SPELL_TELEPORT_POOL"), k_PoolSound);
	const test::RestoreService<Locator::audio> restoreAudio;
	auto& heard = static_cast<HeardAnimEffects&>(Locator::audio::emplace<HeardAnimEffects>());
	const auto file = Parse(k_VortexSounds, "SF_TeleportVortexSoundTest");
	ASSERT_NE(file, nullptr);
	particles::Effect first(file, glm::vec3(100.0f, 0.0f, 100.0f), 1.0f);
	particles::Effect second(file, glm::vec3(300.0f, 0.0f, 100.0f), 1.0f);
	for (int i = 0; i < 10; ++i)
	{
		first.Step(0.1f);
		second.Step(0.1f);
		audio::spell_sounds::ProcessTurn(0.1f);
	}
	for (const auto* effect : {&first, &second})
	{
		const auto sounds = SoundsOf(*effect, k_PoolSound);
		ASSERT_EQ(sounds.size(), 1u);
		EXPECT_EQ(sounds[0]->action.flags, particles::SoundAction::k_Looping | particles::SoundAction::k_SoftRelease);
	}
	const auto pool = heard.ActionsOf(k_PoolSound);
	ASSERT_FALSE(pool.empty());
	for (const auto action : pool)
	{
		EXPECT_EQ(action, audio::AnimAction::Play);
	}
}

// When the stone goes (its pool's effect and atoms with it) the next sound turn lets the hum go softly, to fade out,
// rather than stopping it at once; once only (docs/wiki/audio/magic-and-weather-sounds.md, the lifetime of the particle
// sound; the sounds of SF_TeleportVortex)
TEST_F(TeleportMiracleSounds, TheHumFadesOutSoftlyWhenTheStoneGoes)
{
	const SyntheticSoundActions soundActions;
	ASSERT_EQ(particles::SoundActionByName("SOUND_SPELL_TELEPORT_POOL"), k_PoolSound);
	const test::RestoreService<Locator::audio> restoreAudio;
	auto& heard = static_cast<HeardAnimEffects&>(Locator::audio::emplace<HeardAnimEffects>());
	const auto file = Parse(k_VortexSounds, "SF_TeleportVortexSoundTest");
	ASSERT_NE(file, nullptr);
	{
		particles::Effect effect(file, glm::vec3(100.0f, 0.0f, 100.0f), 1.0f);
		effect.Step(0.1f);
		effect.Step(0.1f);
		ASSERT_EQ(SoundsOf(effect, k_PoolSound).size(), 1u);
	}
	heard.requests.clear();
	audio::spell_sounds::ProcessTurn(0.1f);
	const auto pool = heard.ActionsOf(k_PoolSound);
	ASSERT_EQ(pool.size(), 1u);
	EXPECT_EQ(pool.back(), audio::AnimAction::Release);
	const auto asked = heard.requests.size();
	audio::spell_sounds::ProcessTurn(0.1f);
	EXPECT_EQ(heard.requests.size(), asked);
}

namespace
{
/// The running particle effects as the stones see them: nothing runs, and each effect deleted is noted
class DeletedEffects final: public test::InertParticleSystem
{
public:
	void Delete(EffectId id) override { deleted.push_back(id); }
	std::vector<EffectId> deleted;
};

/// A registry with the world's lists and the reactions, zeroed tables and particles that note what they delete; each
/// put back when the test ends
class TeleportMiracleStones: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		test::EmplaceWorldSystems();
		reactions::Clear();
		Locator::infoConstants::reset(std::make_unique<InfoConstants>().release());
		_particles = &static_cast<DeletedEffects&>(Locator::particleSystem::emplace<DeletedEffects>());
	}
	void TearDown() override
	{
		reactions::Clear();
		Locator::entitiesRegistry::reset();
		test::ResetWorldSystems();
	}

	[[nodiscard]] static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
	[[nodiscard]] DeletedEffects& Particles() const { return *_particles; }

	/// A stone of that player with that serial; it has no place on the map, so its reaction reaches nobody
	[[nodiscard]] static entt::entity MakeStone(PlayerNames player, uint32_t serial)
	{
		const auto stone = Reg().Create();
		auto& component = Reg().Assign<TeleportStone>(stone);
		component.player = player;
		component.serial = serial;
		return stone;
	}
	/// A walker following that reaction (0: none, as one that never took it or has stopped reacting)
	[[nodiscard]] static entt::entity MakeWalker(uint32_t reaction)
	{
		const auto walker = Reg().Create();
		if (reaction != 0)
		{
			Reg().AssignState<VillagerTeleportState>(walker, VillagerTeleportState {.stone = entt::null, .reaction = reaction});
		}
		return walker;
	}
	[[nodiscard]] static std::vector<entt::entity> TravellersOf(entt::entity stone)
	{
		std::vector<entt::entity> travellers;
		for (const auto& traveller : Reg().Get<const TeleportStone>(stone).travellers)
		{
			travellers.push_back(traveller.living);
		}
		return travellers;
	}

private:
	// put back in reverse order; the registry and the world's lists go in TearDown, in the game's order
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	test::RestoreService<Locator::particleSystem> _restoreParticles;
	DeletedEffects* _particles {nullptr};
};
} // namespace

// When the miracle ends, its stone goes and takes with it the reactions it started, its pool's particle effect and its
// list of travellers, and it leaves its player's list of stones; the player's other stones stay as they were
// (docs/wiki/magic/teleport.md#teleport, the stone's deletion)
TEST_F(TeleportMiracleStones, AStoneThatGoesTakesItsReactionsItsPoolAndItsTravellers)
{
	const auto older = MakeStone(PlayerNames::PLAYER_ONE, 0);
	const auto stone = MakeStone(PlayerNames::PLAYER_ONE, 1);
	const auto reaction = reactions::CreateReaction(stone, Reaction::ReactToTeleport, PlayerNames::PLAYER_ONE, false);
	ASSERT_NE(reaction, 0u);
	const auto olderReaction = reactions::CreateReaction(older, Reaction::ReactToTeleport, PlayerNames::PLAYER_ONE, false);
	ASSERT_NE(olderReaction, 0u);
	{
		auto& component = Reg().Get<TeleportStone>(stone);
		component.reaction = reaction;
		component.pool = 17;
	}
	const auto walker = MakeWalker(reaction);
	magic::teleport::RegisterDestination(stone, walker, glm::vec3(400.0f, 0.0f, 300.0f));
	ASSERT_EQ(TravellersOf(stone), std::vector {walker});
	ASSERT_EQ(magic::teleport::StonesOf(PlayerNames::PLAYER_ONE), (std::vector {stone, older}));

	magic::teleport::ToBeDeleted(stone);
	EXPECT_FALSE(Reg().Valid(stone));
	EXPECT_FALSE(reactions::IsAvailable(reactions::Find(reaction)));
	EXPECT_EQ(reactions::GetReactionInitiatedBy(stone), 0u);
	EXPECT_EQ(Particles().deleted, std::vector<ecs::systems::ParticleSystemInterface::EffectId> {17});
	EXPECT_EQ(magic::teleport::StonesOf(PlayerNames::PLAYER_ONE), std::vector {older});
	// the other stone keeps its own reaction, and the walker itself stays
	EXPECT_TRUE(reactions::IsAvailable(reactions::Find(olderReaction)));
	EXPECT_TRUE(Reg().Valid(walker));
}

// In each player's turn every stone of the player drops from its travellers those that are gone or no longer
// available, and those whose reaction is no longer the stone's: they gave it up for another, or left it when they
// jumped or stopped reacting. The others stay, in their order (docs/wiki/magic/teleport.md#teleport, the player's turn
// over the stones)
TEST_F(TeleportMiracleStones, WalkersStopBeingTravellersOnceTheyLeaveGiveUpOrJump)
{
	const auto stone = MakeStone(PlayerNames::PLAYER_TWO, 0);
	constexpr uint32_t k_Reaction = 5;
	constexpr uint32_t k_OtherReaction = 9;
	Reg().Get<TeleportStone>(stone).reaction = k_Reaction;
	const auto first = MakeWalker(k_Reaction);
	const auto gaveUp = MakeWalker(k_OtherReaction);
	const auto jumped = MakeWalker(0);
	const auto unavailable = MakeWalker(k_Reaction);
	Reg().Assign<ecs::components::Unavailable>(unavailable);
	const auto gone = MakeWalker(k_Reaction);
	const auto second = MakeWalker(k_Reaction);
	// newest first: second, gone, unavailable, jumped, gaveUp, first
	for (const auto walker : {first, gaveUp, jumped, unavailable, gone, second})
	{
		magic::teleport::RegisterDestination(stone, walker, glm::vec3(400.0f, 0.0f, 300.0f));
	}
	Reg().Destroy(gone);
	ASSERT_EQ(TravellersOf(stone).size(), 6u);

	magic::teleport::ProcessPlayers();
	EXPECT_EQ(TravellersOf(stone), (std::vector {second, first}));
	// nothing changes while they keep following it
	magic::teleport::ProcessPlayers();
	EXPECT_EQ(TravellersOf(stone), (std::vector {second, first}));
	// one that stops reacting is dropped at the next turn
	Reg().RemoveState<VillagerTeleportState>(first);
	magic::teleport::ProcessPlayers();
	EXPECT_EQ(TravellersOf(stone), std::vector {second});
}

/// With OPENBLACK_GAME_PATH set to the install: the teleport's effect row of info.dat has no burn, crush, hit, heal,
/// throw or belief, and the plain alignment modifier of 1, so the point event each jump sends the miracle at its stone
/// changes no life and so moves no alignment (an effect moves it by the life it changed)
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(TeleportMiracleRows, realInfoDatAJumpsEventHasNoEffect)
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
	const auto& row = magic::GetMagicEffectInfo(*info, MagicType::Teleport);
	EXPECT_EQ(row.effectBurn, 0.0f);
	EXPECT_EQ(row.effectCrush, 0.0f);
	EXPECT_EQ(row.effectHit, 0.0f);
	EXPECT_EQ(row.effectHeal, 0.0f);
	EXPECT_EQ(row.effectFlyAway, 0.0f);
	EXPECT_EQ(row.effectAlignmentModification, 1.0f);
	EXPECT_EQ(row.effectBeliefModification, 0.0f);
}
