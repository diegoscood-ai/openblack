/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The point a script's CREATE hands a new thing, on a land whose height is a function of the point: x and z on the
// map's fixed point, and no height above the land for every type but a marker and a mobile static. A vortex is made
// standing on the land there, whatever height the script gives (a fake vortex system keeps the point it is given)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "CHLApi.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/ScriptObjectsSystem.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/LandFakes.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{
/// The land's height at a point
[[nodiscard]] float LandAt(glm::vec2 xz)
{
	return 20.0f + (xz.x * 0.01f) + (xz.y * 0.001f);
}

/// The vortex system as far as CREATE asks it: each vortex a new entity, with the point and the kind it was made with
class MadeVortices final: public ecs::systems::VortexSystemInterface
{
public:
	entt::entity Create(glm::vec3 position, VortexType type) override
	{
		const auto vortex = Locator::entitiesRegistry::value().Create();
		made.push_back({position, type});
		return vortex;
	}
	bool StartFadeOut(entt::entity /*vortex*/) override { return false; }
	void SetParameters(entt::entity /*vortex*/, entt::entity /*town*/, glm::vec3 /*position*/, float /*a*/, float /*b*/,
	                   entt::entity /*flock*/) override
	{
	}
	void ProcessTurn() override {}
	void UpdateGroundEffects(float /*seconds*/) override {}
	void UpdateOverLandEffects(float /*seconds*/) override {}

	struct Made
	{
		glm::vec3 position;
		VortexType type;
	};
	std::vector<Made> made;
};

class ScriptCreatePoints: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		// entity 0 is the scripts' "no thing"
		Locator::entitiesRegistry::value().Create();
		Locator::vm::emplace<lhvm::LHVM>();
		Locator::terrainSystem::emplace<test::HeightFieldIsland>(LandAt);
		Locator::scriptObjects::emplace<ecs::systems::ScriptObjectsSystem>();
		_info = std::make_unique<InfoConstants>();
		Locator::infoConstants::emplace(*_info);
		_vortices = &static_cast<MadeVortices&>(Locator::vortexSystem::emplace<MadeVortices>());
	}

	/// CREATE of a type as a script calls it: the type, the sub-type, then the point; or, with an angle in degrees and a
	/// scale first, CREATE_WITH_ANGLE_AND_SCALE. The thing it gives, the only value it leaves above the script's own
	uint32_t Create(int32_t type, int32_t subtype, glm::vec3 at, std::optional<std::pair<float, float>> angleAndScale = {})
	{
		const std::string_view name = angleAndScale.has_value() ? "CREATE_WITH_ANGLE_AND_SCALE" : "CREATE";
		const auto& table = _api.GetFunctionsTable();
		const auto native =
		    std::ranges::find_if(table, [name](const lhvm::NativeFunction& f) { return std::string_view(f.name) == name; });
		EXPECT_NE(native, table.end()) << name;
		if (native == table.end())
		{
			return 0;
		}
		auto& vm = Locator::vm::value();
		vm.Pushi(k_ScriptsOwn);
		if (angleAndScale.has_value())
		{
			vm.Pushf(angleAndScale->first);
			vm.Pushf(angleAndScale->second);
		}
		vm.Pushi(type);
		vm.Pushi(subtype);
		vm.Pushv(at.x);
		vm.Pushv(at.y);
		vm.Pushv(at.z);
		native->impl();
		const auto thing = vm.Pop().uintVal;
		EXPECT_EQ(vm.Pop().intVal, k_ScriptsOwn) << name << " left more than one value";
		return thing;
	}

	static constexpr int32_t k_ScriptsOwn = 7777;
	static constexpr int32_t k_VortexType = 23;
	static constexpr int32_t k_MarkerType = 1;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::vm> _restoreVm;
	test::RestoreService<Locator::terrainSystem> _restoreTerrain;
	test::RestoreService<Locator::scriptObjects> _restoreScriptObjects;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	test::RestoreService<Locator::vortexSystem> _restoreVortices;
	chlapi::CHLApi _api;
	std::unique_ptr<InfoConstants> _info;
	MadeVortices* _vortices {nullptr};
};
} // namespace

TEST_F(ScriptCreatePoints, AVortexStandsOnTheLandAtThePointsMapPosition)
{
	const glm::vec3 given(1000.07f, 300.0f, 1200.03f);
	const auto vortex = Create(k_VortexType, 1, given);
	EXPECT_NE(vortex, 0u);
	ASSERT_EQ(_vortices->made.size(), 1u);
	const auto& made = _vortices->made.front();
	EXPECT_EQ(made.type, VortexType::Out);
	const float x = map_coords::Quantise(given.x);
	const float z = map_coords::Quantise(given.z);
	EXPECT_EQ(made.position.x, x);
	EXPECT_EQ(made.position.z, z);
	EXPECT_EQ(made.position.y, LandAt({x, z}));
}

// Both creates go through the one switch with the one point (docs/wiki/scripts/map-script-reader.md#the-natives): with an
// angle and a scale a thing stands at the height the plain create gives it, a vortex on the land and a marker at the
// script's height
TEST_F(ScriptCreatePoints, WithAnAngleAndAScaleAThingStandsAtTheHeightThePlainCreateGivesIt)
{
	const glm::vec3 given(1000.07f, 300.0f, 1200.03f);
	ASSERT_NE(Create(k_VortexType, 1, given), 0u);
	ASSERT_NE(Create(k_VortexType, 1, given, std::pair {37.0f, 0.5f}), 0u);
	ASSERT_EQ(_vortices->made.size(), 2u);
	EXPECT_EQ(_vortices->made[1].position, _vortices->made[0].position);
	EXPECT_EQ(_vortices->made[1].position.y, LandAt({map_coords::Quantise(given.x), map_coords::Quantise(given.z)}));
	// a marker keeps the script's height with either
	const auto plain = Create(k_MarkerType, 0, given);
	const auto turned = Create(k_MarkerType, 0, given, std::pair {37.0f, 0.5f});
	ASSERT_NE(plain, 0u);
	ASSERT_NE(turned, 0u);
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& a = registry.Get<const ecs::components::Transform>(static_cast<entt::entity>(plain));
	const auto& b = registry.Get<const ecs::components::Transform>(static_cast<entt::entity>(turned));
	EXPECT_EQ(a.position, b.position);
	EXPECT_EQ(b.position.y, given.y);
}
