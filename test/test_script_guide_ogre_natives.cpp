/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The natives the guide creature and the Ogre are set up with, as a script calls them (a fake mind takes the desires
// they change), and their rules: a desire set from a script, a creature's speed as a script sets it, and how a creature
// made from another takes its body and its owner

#include <cstdint>

#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include <LHVM.h>
#include <MindFile.h>
#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "Creature/CreatureDesires.h"
#include "ECS/Components/Creature.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/Implementations/ReactionSystem.h"
#include "Enums.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptPropertyRules.h"
#include "support/RestoreService.h"

using namespace openblack;
using creature_desires::Desire;

namespace
{
/// One creature's desires, which the natives change through the mind; every other call does nothing
class GuideMinds final: public ecs::systems::CreatureMindSystemInterface
{
public:
	void ProcessTurn() override {}
	void PlanTurn() override {}
	void LearnTurn() override {}
	void LoadMind(entt::entity, std::shared_ptr<const creaturemind::MindFileData>) override {}
	[[nodiscard]] std::optional<creaturemind::MindFileData> SaveMind(entt::entity) const override { return std::nullopt; }
	void ClearLearning(entt::entity) override {}
	void FocusByScript(entt::entity, const glm::vec3&) override {}
	void ReleasedByScript(entt::entity) override {}
	void AbandonAction(entt::entity) override {}
	void GiveUpPlan(entt::entity) override {}
	void SetAgendaPriority(entt::entity, float) override {}
	void SeeSkill(const glm::vec3&, size_t) override {}
	void SeeMiracle(const glm::vec3&, size_t) override {}
	void PlayerDid(PlayerNames, size_t, const glm::vec3&, std::optional<entt::entity>) override {}
	void EmpathiseWithPlayer(PlayerNames, CreatureDesires, float, const glm::vec3&) override {}
	void EmpathiseWithTownDesire(PlayerNames, TownDesireInfo, float, const glm::vec3&) override {}
	bool SetKnowsAction(entt::entity, uint32_t, uint32_t, bool) override { return false; }
	[[nodiscard]] const creature_mind_tables::Tables* GetTables() override { return nullptr; }
	bool PlayAction(entt::entity, size_t, std::optional<bool>) override { return false; }
	bool PlayGesture(entt::entity, size_t) override { return false; }
	void PullFace(entt::entity, size_t) override {}
	std::optional<creature_face::Request> ShowFeeling(entt::entity, creature_face::Cue) override { return std::nullopt; }
	bool SitDown(entt::entity) override { return false; }
	void StandUp(entt::entity) override {}
	void ReceiveFeedback(entt::entity, float) override {}
	bool ForceAction(entt::entity, size_t, bool, std::optional<creature_face::Request>, double) override { return false; }
	bool Sleep(entt::entity) override { return false; }
	bool Eat(entt::entity, std::optional<entt::entity>) override { return false; }
	bool Drink(entt::entity) override { return false; }
	bool Poo(entt::entity) override { return false; }
	bool Puke(entt::entity) override { return false; }
	void EndMeal(entt::entity, uint32_t, float, creature_eating::MealEnd) override {}
	bool Faint(entt::entity) override { return false; }
	void Wake(entt::entity) override {}
	void FoughtFight(entt::entity, bool) override {}
	void ChangeDesires(entt::entity creature, const std::function<void(creature_desires::Desires&)>& change) override
	{
		changed = creature;
		change(desires);
	}

	creature_desires::Desires desires;
	entt::entity changed {entt::null};
};

class ScriptGuideOgreNatives: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
		// entity 0 is "no thing" for a script id
		registry.Create();
		_creature = registry.Create();
		registry.Assign<ecs::components::Creature>(_creature);
		_rock = registry.Create();
		Locator::vm::emplace<lhvm::LHVM>();
		_minds = &static_cast<GuideMinds&>(Locator::creatureMindSystem::emplace<GuideMinds>());
	}

	/// The native of that name taking that many values, with the values pushed in the order a script pushes them
	void Call(std::string_view name, int stackIn, const std::function<void(lhvm::LHVM&)>& push)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native = std::ranges::find_if(table, [name, stackIn](const lhvm::NativeFunction& f) {
			return std::string_view(f.name) == name && f.stackIn == stackIn;
		});
		ASSERT_NE(native, table.end()) << name;
		push(Locator::vm::value());
		native->impl();
	}
	void SetDesire(std::string_view name, entt::entity creature, Desire desire, float value)
	{
		Call(name, 3, [creature, desire, value](lhvm::LHVM& vm) {
			vm.Pusho(entt::to_integral(creature));
			vm.Pushi(static_cast<int32_t>(desire));
			vm.Pushf(value);
		});
	}
	void Activate(entt::entity creature, std::optional<int32_t> desire, bool on)
	{
		Call("CREATURE_SET_DESIRE_ACTIVATED", desire.has_value() ? 3 : 2, [creature, desire, on](lhvm::LHVM& vm) {
			vm.Pusho(entt::to_integral(creature));
			if (desire.has_value())
			{
				vm.Pushi(*desire);
			}
			vm.Pushi(on ? 1 : 0);
		});
	}

	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::vm> _restoreVm;
	test::RestoreService<Locator::creatureMindSystem> _restoreMinds;
	chlapi::CHLApi _api;
	GuideMinds* _minds {nullptr};
	entt::entity _creature {entt::null};
	entt::entity _rock {entt::null};
};
} // namespace

// The Ogre's desires: all off, then hunger and anger on, wanted a little and no more
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptGuideOgreNatives, TheOgresDesires)
{
	Activate(_creature, std::nullopt, false);
	EXPECT_EQ(_minds->changed, _creature);
	EXPECT_TRUE(std::ranges::none_of(_minds->desires.desires, [](const auto& state) { return state.activated; }));
	Activate(_creature, static_cast<int32_t>(Desire::Hunger), true);
	SetDesire("CREATURE_SET_DESIRE_VALUE", _creature, Desire::Hunger, 0.1f);
	SetDesire("CREATURE_SET_DESIRE_MAXIMUM", _creature, Desire::Hunger, 0.1f);
	Activate(_creature, static_cast<int32_t>(Desire::Anger), true);
	SetDesire("CREATURE_SET_DESIRE_VALUE", _creature, Desire::Anger, 0.05f);
	SetDesire("CREATURE_SET_DESIRE_MAXIMUM", _creature, Desire::Anger, 0.05f);
	const auto& desires = _minds->desires;
	EXPECT_TRUE(desires[Desire::Hunger].activated);
	EXPECT_TRUE(desires[Desire::Anger].activated);
	EXPECT_FALSE(desires[Desire::Play].activated);
	EXPECT_FLOAT_EQ(desires[Desire::Hunger].value, 0.1f);
	EXPECT_FLOAT_EQ(desires[Desire::Hunger].max, 0.1f);
	EXPECT_FLOAT_EQ(desires[Desire::Anger].value, 0.05f);
	EXPECT_FLOAT_EQ(desires[Desire::Anger].max, 0.05f);
	// Not a creature: its desires aren't asked for
	_minds->changed = entt::null;
	SetDesire("CREATURE_SET_DESIRE_VALUE", _rock, Desire::Hunger, 0.5f);
	EXPECT_TRUE(_minds->changed == entt::null);
}

// A desire past the last changes nothing, though the creature's desires are asked for
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptGuideOgreNatives, ADesirePastTheLastChangesNothing)
{
	const auto before = _minds->desires.desires;
	Call("CREATURE_SET_DESIRE_VALUE", 3, [this](lhvm::LHVM& vm) {
		vm.Pusho(entt::to_integral(_creature));
		vm.Pushi(static_cast<int32_t>(creature_desires::k_DesireCount));
		vm.Pushf(0.5f);
	});
	EXPECT_EQ(_minds->changed, _creature);
	EXPECT_TRUE(
	    std::ranges::equal(before, _minds->desires.desires, [](const auto& a, const auto& b) { return a.value == b.value; }));
}

// The guide sheep's reaction: the thing's own, of no player (not the neutral one), and taken away again by its kind
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptGuideOgreNatives, AScriptsReactionIsOfNoPlayerAndGoesByItsKind)
{
	const test::RestoreService<Locator::reactionSystem> restoreReactions;
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	const auto make = [this](Reaction type) {
		Call("CREATE_REACTION", 2, [this, type](lhvm::LHVM& vm) {
			vm.Pusho(entt::to_integral(_rock));
			vm.Pushi(static_cast<int32_t>(type));
		});
	};
	make(Reaction::FleeFromObject);
	make(Reaction::ReactToFire);
	const auto& made = ecs::effects::reactions::All();
	ASSERT_EQ(made.size(), 2u);
	EXPECT_EQ(made.front().initiator, _rock);
	EXPECT_EQ(made.front().type, Reaction::FleeFromObject);
	EXPECT_FALSE(made.front().player.has_value());
	EXPECT_EQ(made.front().turnCreated, 0u);
	Call("REMOVE_REACTION_OF_TYPE", 2, [this](lhvm::LHVM& vm) {
		vm.Pusho(entt::to_integral(_rock));
		vm.Pushi(static_cast<int32_t>(Reaction::FleeFromObject));
	});
	ASSERT_EQ(ecs::effects::reactions::All().size(), 1u);
	EXPECT_EQ(ecs::effects::reactions::All().front().type, Reaction::ReactToFire);
}

// Exactly all of a desire also fills what drives it, but anger from being hurt and restoring health from its life;
// only the desire's own sources are filled
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureDesiresFromScript, AllOfADesireFillsItsOwnSources)
{
	namespace sources = creature_desires::sources;
	creature_desires::Desires desires;
	desires[Desire::Anger].sources = {{.type = sources::k_AngerFromDamage, .value = 0.2f},
	                                  {.type = sources::k_InnateAggression, .value = 0.2f}};
	desires[Desire::RestoreHealth].sources = {{.type = sources::k_RestoreHealthFromLife, .value = 0.3f}};
	// Another desire with a source of the same kind keeps its own
	desires[Desire::Play].sources = {{.type = sources::k_InnateAggression, .value = 0.1f}};

	creature_desires::SetValueFromScript(desires, Desire::Anger, 1.5f);
	EXPECT_FLOAT_EQ(desires[Desire::Anger].value, 1.0f);
	EXPECT_FLOAT_EQ(desires[Desire::Anger].sources[1].value, 0.2f);

	creature_desires::SetValueFromScript(desires, Desire::Anger, -0.5f);
	EXPECT_FLOAT_EQ(desires[Desire::Anger].value, 0.0f);

	creature_desires::SetValueFromScript(desires, Desire::Anger, 1.0f);
	EXPECT_FLOAT_EQ(desires[Desire::Anger].value, 1.0f);
	EXPECT_FLOAT_EQ(desires[Desire::Anger].sources[0].value, 0.2f);
	EXPECT_FLOAT_EQ(desires[Desire::Anger].sources[1].value, 1.0f);
	EXPECT_FLOAT_EQ(desires[Desire::Play].sources[0].value, 0.1f);

	creature_desires::SetValueFromScript(desires, Desire::RestoreHealth, 1.0f);
	EXPECT_FLOAT_EQ(desires[Desire::RestoreHealth].sources[0].value, 0.3f);
}

// The Ogre is made from the player's creature at its scale, and belongs to the first player with no creature
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureScriptProperties, ACopyTakesItsSizeAndStrengthAndAnOwner)
{
	using namespace script::property_rules;
	const auto body = CopyBody(1.5f, 0.6f, 1.0f);
	EXPECT_FLOAT_EQ(body.size, 1.5f);
	EXPECT_FLOAT_EQ(body.strength, 0.6f);
	const auto bigger = CopyBody(1.5f, 0.6f, 2.0f);
	EXPECT_FLOAT_EQ(bigger.size, 3.0f);
	EXPECT_FLOAT_EQ(bigger.strength, 1.2f);

	std::array<bool, 7> have {true, false, false, false, false, false, false};
	EXPECT_EQ(OwnerOfCopy(have), PlayerNames::PLAYER_TWO);
	have = {false, true, true, true, true, true, true};
	EXPECT_EQ(OwnerOfCopy(have), PlayerNames::PLAYER_ONE);
	have.fill(true);
	EXPECT_EQ(OwnerOfCopy(have), std::nullopt);
}
