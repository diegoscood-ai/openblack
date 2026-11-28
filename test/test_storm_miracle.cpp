/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The storm miracle pinned to the original: the swirl at the caster's hand is stepped each turn only while the miracle
// has a caster, with a strength of 1, and goes once the caster is gone or the swirl has ended. Wiki:
// docs/wiki/magic/storm-and-tornado.md#storm-electric-storm-and-tornado.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <memory>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "ECS/Components/Spell.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Spells/SpellStormAndTornado.h"
#include "Particles/SpellLink.h"
#include "support/MagicFakes.h"
#include "support/ParticleFakes.h"
#include "support/RestoreService.h"
#include "support/WorldSystems.h"

using namespace openblack;
using openblack::ecs::components::Spell;
using openblack::ecs::components::SpellCreator;
namespace reactions = openblack::ecs::effects::reactions;

namespace
{
/// The running particle effects as the storm sees them: each step asked of them is noted and answers `running`, and
/// each effect deleted is noted
class SteppedEffects final: public test::InertParticleSystem
{
public:
	struct Step
	{
		EffectId id;
		particles::ProcessInfo info;
		float seconds;
	};
	bool ProcessForSpell(EffectId id, const particles::ProcessInfo& info, float seconds) override
	{
		steps.push_back({id, info, seconds});
		return running;
	}
	void Delete(EffectId id) override { deleted.push_back(id); }

	/// The steps asked of that effect
	[[nodiscard]] std::vector<Step> StepsOf(EffectId id) const
	{
		std::vector<Step> found;
		for (const auto& step : steps)
		{
			if (step.id == id)
			{
				found.push_back(step);
			}
		}
		return found;
	}

	bool running {true};
	std::vector<Step> steps;
	std::vector<EffectId> deleted;
};

/// The swirl's effect in the tests
constexpr ecs::systems::ParticleSystemInterface::EffectId k_Swirl = 33;

/// The miracles' classes over a fresh registry and world lists, the inert miracles' service (whose spell store the
/// classes register in), a zeroed table and particles that note their steps; each put back when the test ends
class StormMiracleSwirl: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		Locator::infoConstants::reset(std::make_unique<InfoConstants>().release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		test::EmplaceWorldSystems();
		reactions::Clear();
		Locator::magicSystem::emplace<test::InertMagicSystem>();
		_particles = &static_cast<SteppedEffects&>(Locator::particleSystem::emplace<SteppedEffects>());
	}
	void TearDown() override
	{
		magic::spell_storm::Clear();
		reactions::Clear();
		game_clock::SetTurn(0);
		Locator::entitiesRegistry::reset();
		test::ResetWorldSystems();
	}

	[[nodiscard]] static ecs::Registry& Registry() { return Locator::entitiesRegistry::value(); }
	[[nodiscard]] SteppedEffects& Particles() const { return *_particles; }

	/// A storm miracle with that caster and the swirl as its cast effect. It is already closed down and has no main
	/// effect, so its own turn leaves the chants and the clouds alone: only the swirl's part runs
	[[nodiscard]] static entt::entity MakeStorm(SpellCreator creator)
	{
		const auto spell = Registry().Create();
		auto& component = Registry().Assign<Spell>(spell);
		component.magicType = MagicType::StormWindRain;
		component.spellClass = magic::ClassOf(MagicType::StormWindRain);
		component.creator = creator;
		component.player = creator.player;
		component.hasPlayer = creator.kind != SpellCreator::Kind::None;
		component.position = glm::vec3(100.0f, 0.0f, 100.0f);
		component.castPos = component.position;
		component.magnitude = 40.0f;
		component.closedDown = true;
		Registry().Assign<magic::SpellStormData>(spell, magic::SpellStormData {.castEffect = k_Swirl, .waterReaction = 0});
		return spell;
	}
	/// The storm's turn, through its class
	static void Process(entt::entity spell)
	{
		static_cast<void>(magic::OpsOf(ecs::components::SpellClass::StormAndTornado).process(spell));
	}

private:
	// put back in reverse order; the registry and the world's lists go in TearDown, in the game's order
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::magicSystem> _restoreMagic;
	test::RestoreService<Locator::particleSystem> _restoreParticles;
	SteppedEffects* _particles {nullptr};
};

const SpellCreator k_Player {.kind = SpellCreator::Kind::Player, .player = PlayerNames::PLAYER_ONE, .entity = entt::null};
const SpellCreator k_NoCaster {.kind = SpellCreator::Kind::None, .player = PlayerNames::PLAYER_ONE, .entity = entt::null};
} // namespace

// While the miracle has a caster its swirl is stepped every turn, for a turn's 0.1 s, with a process info of zeros
// that the caster fills in (a player's miracle not cast from the hand leaves it so), at strength 1 and enabled; it
// stays while it runs (docs/wiki/magic/storm-and-tornado.md#storm-electric-storm-and-tornado, the storm's turn)
TEST_F(StormMiracleSwirl, TheSwirlRunsWhileTheMiracleHasACaster)
{
	ASSERT_EQ(magic::ClassOf(MagicType::StormWindRain), ecs::components::SpellClass::StormAndTornado);
	const auto spell = MakeStorm(k_Player);
	for (int turn = 0; turn < 3; ++turn)
	{
		Process(spell);
	}
	const auto steps = Particles().StepsOf(k_Swirl);
	ASSERT_EQ(steps.size(), 3u);
	for (const auto& step : steps)
	{
		EXPECT_FLOAT_EQ(step.seconds, 0.1f);
		EXPECT_FLOAT_EQ(step.info.power, 1.0f);
		EXPECT_TRUE(step.info.enabled);
		EXPECT_EQ(step.info.handPos, glm::vec3(0.0f));
		EXPECT_EQ(step.info.direction, glm::vec3(0.0f));
		EXPECT_FLOAT_EQ(step.info.curl, 0.0f);
	}
	EXPECT_TRUE(Particles().deleted.empty());
	EXPECT_EQ(magic::spell_storm::CastEffectOf(spell), k_Swirl);
}

// A miracle with no caster deletes its swirl at its next turn without stepping it, and from then on there is no swirl
// to step (docs/wiki/magic/storm-and-tornado.md#storm-electric-storm-and-tornado, the storm's turn)
TEST_F(StormMiracleSwirl, TheSwirlIsRemovedWhenTheMiracleHasNoCaster)
{
	const auto spell = MakeStorm(k_NoCaster);
	Process(spell);
	EXPECT_TRUE(Particles().StepsOf(k_Swirl).empty());
	EXPECT_EQ(Particles().deleted, std::vector<ecs::systems::ParticleSystemInterface::EffectId> {k_Swirl});
	EXPECT_EQ(magic::spell_storm::CastEffectOf(spell), 0u);
	Process(spell);
	EXPECT_TRUE(Particles().StepsOf(k_Swirl).empty());
	EXPECT_EQ(Particles().deleted.size(), 1u);
}

// A swirl that has ended (its step answers that it is finished) is deleted the same turn, caster or not
// (docs/wiki/magic/storm-and-tornado.md#storm-electric-storm-and-tornado, the storm's turn)
TEST_F(StormMiracleSwirl, AnEndedSwirlIsRemoved)
{
	const auto spell = MakeStorm(k_Player);
	Process(spell);
	EXPECT_EQ(magic::spell_storm::CastEffectOf(spell), k_Swirl);
	Particles().running = false;
	Process(spell);
	EXPECT_EQ(Particles().StepsOf(k_Swirl).size(), 2u);
	EXPECT_EQ(Particles().deleted, std::vector<ecs::systems::ParticleSystemInterface::EffectId> {k_Swirl});
	EXPECT_EQ(magic::spell_storm::CastEffectOf(spell), 0u);
	Process(spell);
	EXPECT_EQ(Particles().StepsOf(k_Swirl).size(), 2u);
}
