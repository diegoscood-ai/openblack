/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/
// CREATURE_CREATE_RELATIVE_TO_CREATURE, as a script calls it: the creature it is made from, the scale, the place and
// the species; a new creature of the species as big and as strong as that one times the scale, on the ground at the
// place, taking up no reactions and learning nothing from the feedback, added to the script's things as made by it.
// docs/wiki/creature/players-creature.md#a-creature-made-from-another

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureReaction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptPropertyRules.h"
#include "creature/CreatureSystemWorld.h"
#include "support/LandFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// The ground's height everywhere
constexpr float k_Ground = 7.0f;
/// The scripts' number of the Giant Ape
constexpr int32_t k_ScriptApe = 0;

/// The scripts' table of objects, as far as the native reaches it: what each Register was given
class RegisteredThings final: public ecs::systems::ScriptObjectsSystemInterface
{
public:
	bool Register(entt::entity object, bool createdByScript) override
	{
		registered.emplace_back(object, createdByScript);
		return true;
	}
	void EnterNative(uint32_t /*native*/) override {}
	entt::entity Fetch(entt::entity object) override { return object; }
	void AddReference(entt::entity /*object*/) override {}
	void RemoveReference(entt::entity /*object*/) override {}
	void ReleaseFromScript(entt::entity /*object*/) override {}
	void Replace(entt::entity /*from*/, entt::entity /*to*/) override {}
	void Reset() override {}
	void ReleaseUnreferenced() override {}
	bool Disband(entt::entity /*container*/) override { return false; }
	[[nodiscard]] std::vector<ecs::systems::ScriptObjectPlace> Places() const override { return {}; }
	[[nodiscard]] uint32_t TimesFull() const override { return 0; }
	void ClearForNewLand() override {}
	void DeleteAllScriptThings() override {}
	std::vector<std::pair<entt::entity, bool>> registered;
};

class CreateRelativeToCreatureNative: public ::testing::Test
{
protected:
	CreateRelativeToCreatureNative()
	    : _vm(&Locator::vm::emplace<lhvm::LHVM>())
	{
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		Locator::chlapi::emplace<chlapi::CHLApi>();
		Locator::terrainSystem::emplace<test::HeightFieldIsland>([](glm::vec2 /*xz*/) { return k_Ground; });
		_things = &static_cast<RegisteredThings&>(Locator::scriptObjects::emplace<RegisteredThings>());
		// a script's object 0 is no thing, so the first entity is not one the tests name
		std::ignore = test::creature_world::World::Registry().Create();
	}

	/// The native's answer: the creature it is made from pushed first, then the scale, the place and the species; a value
	/// under them must stay
	uint32_t Call(uint32_t from, float scale, glm::vec3 place, int32_t species)
	{
		const auto& table = Locator::chlapi::value().GetFunctionsTable();
		const auto native = std::ranges::find_if(table, [](const lhvm::NativeFunction& f) {
			return std::string_view(f.name) == "CREATURE_CREATE_RELATIVE_TO_CREATURE";
		});
		EXPECT_NE(native, table.end());
		_vm->Pushi(77);
		_vm->Pusho(from);
		_vm->Pushf(scale);
		_vm->Pushf(place.x);
		_vm->Pushf(place.y);
		_vm->Pushf(place.z);
		_vm->Pushi(species);
		native->impl();
		const auto made = _vm->Pop().uintVal;
		EXPECT_EQ(_vm->Pop().intVal, 77);
		return made;
	}

	static uint32_t Id(entt::entity entity) { return static_cast<uint32_t>(entt::to_integral(entity)); }

	test::creature_world::World _world;
	const test::RestoreService<Locator::vm> _restoreVm;
	const test::RestoreService<Locator::chlapi> _restoreApi;
	const test::RestoreService<Locator::terrainSystem> _restoreTerrain;
	const test::RestoreService<Locator::scriptObjects> _restoreThings;
	lhvm::LHVM* _vm;
	RegisteredThings* _things {nullptr};
};
} // namespace

TEST_F(CreateRelativeToCreatureNative, ItMakesACreatureOfTheSpeciesScaledFromTheOtherOnTheGround)
{
	auto& registry = test::creature_world::World::Registry();
	const auto from = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	auto& source = registry.Get<Creature>(from);
	source.size = 1.5f;
	source.strength = 0.6f;
	const auto made = Call(Id(from), 0.5f, glm::vec3(120.0f, 3.0f, 140.0f), k_ScriptApe);
	ASSERT_NE(made, 0u);
	const auto created = static_cast<entt::entity>(made);
	ASSERT_TRUE(registry.Valid(created));
	ASSERT_TRUE(registry.AllOf<Creature>(created));
	const auto& body = std::as_const(registry).Get<const Creature>(created);
	EXPECT_EQ(body.species, CreatureType::GiantApe);
	const auto copied = script::property_rules::CopyBody(1.5f, 0.6f, 0.5f);
	EXPECT_FLOAT_EQ(body.size, copied.size);
	EXPECT_FLOAT_EQ(body.strength, copied.strength);
	// on the ground at the place, whatever height the script gave
	const auto& at = std::as_const(registry).Get<const Transform>(created).position;
	EXPECT_EQ(at, glm::vec3(120.0f, k_Ground, 140.0f));
	// no reactions, no learning from the feedback, the other's weak to strong target kept until its first turn
	EXPECT_TRUE(registry.AllOf<CreatureReactionsOff>(created));
	EXPECT_TRUE(registry.AllOf<CreatureLearningOff>(created));
	EXPECT_TRUE(registry.AllOf<CreatureMadeWeakStrong>(created));
	// the script's own thing
	ASSERT_EQ(_things->registered.size(), 1u);
	EXPECT_EQ(_things->registered.front(), std::pair(created, true));
}

TEST_F(CreateRelativeToCreatureNative, NoneIsMadeFromAThingThatIsNoCreatureOrForASpeciesThereIsNot)
{
	auto& registry = test::creature_world::World::Registry();
	const auto from = test::creature_world::World::MakeCreature();
	const auto thing = registry.Create();
	registry.Assign<Transform>(thing, glm::vec3(10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_EQ(Call(Id(thing), 1.0f, glm::vec3(120.0f, 0.0f, 140.0f), k_ScriptApe), 0u);
	EXPECT_EQ(Call(0, 1.0f, glm::vec3(120.0f, 0.0f, 140.0f), k_ScriptApe), 0u);
	EXPECT_EQ(Call(Id(from), 1.0f, glm::vec3(120.0f, 0.0f, 140.0f), -1), 0u);
	EXPECT_TRUE(_things->registered.empty());
}
