/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a villager carries is only drawn: a mesh fixed to the grip bone at the end of its arm (bone 15), for each of the
// fourteen objects and none for the first two entries, never while the hand holds the villager or while its state is
// not drawn, and back as soon as the villager is drawn with it again. The prop is no thing of the world: only a drawn
// mesh, gone with its villager.
// docs/wiki/project/progress/villager/tools_and_carried_items.md, docs/wiki/beings/animation.md#objects-in-the-hand

#include <cstddef>
#include <cstdint>

#include <memory>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "3D/AllMeshes.h"
#include "ECS/CarriedProps.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
constexpr size_t k_Bones = 22;
constexpr size_t k_GripBone = 15;
/// A state whose row says its clip is not drawn (the inside of a home)
constexpr auto k_NotDrawn = VillagerStates::SleepingAtHome;

class CarriedPropsTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto info = std::make_unique<InfoConstants>();
		for (auto& row : info->villagerStateTable)
		{
			row.animation = 385;
		}
		info->villagerStateTable.at(static_cast<size_t>(k_NotDrawn)).animation = static_cast<uint32_t>(-4);
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// A villager standing at (10, 0, 20), its pose with every bone at rest but the grip bone, carrying `carried`
	static entt::entity MakeVillager(int32_t carried, glm::vec3 grip = {1.0f, 2.0f, 3.0f})
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		registry.Assign<Villager>(e);
		registry.Assign<Transform>(e, glm::vec3(10.0f, 0.0f, 20.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<LivingAction>(e, VillagerStates::MoveToPos, static_cast<uint16_t>(0));
		auto& animation = registry.Assign<SkeletalAnimation>(e);
		animation.carriedObject = carried;
		animation.pose.assign(k_Bones, glm::mat4(1.0f));
		animation.pose.at(k_GripBone) = glm::translate(glm::mat4(1.0f), grip);
		return e;
	}

	static void SetTop(entt::entity villager, VillagerStates state)
	{
		Reg().Get<LivingAction>(villager).states.at(static_cast<size_t>(LivingAction::Index::Top)) =
		    static_cast<uint8_t>(state);
	}

	static std::vector<entt::entity> Props()
	{
		std::vector<entt::entity> props;
		Reg().Each<const ecs::components::CarriedProp>([&props](entt::entity e, const auto&) { props.push_back(e); });
		return props;
	}

	const test::RestoreService<Locator::infoConstants> _restoreInfo;
	const test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
};
} // namespace

// The prop is fixed to bone 15 of the pose, the grip at the end of the -X arm (the bone's rows -X, -Z, -Y, no offset),
// and follows the bone as the clip moves it
TEST_F(CarriedPropsTest, AToolIsDrawnAtTheGripBoneOfTheArmAndTurnsWithIt)
{
	const auto villager = MakeVillager(2); // an axe
	ecs::UpdateCarriedProps();
	const auto props = Props();
	ASSERT_EQ(props.size(), 1u);
	const auto& prop = Reg().Get<const ecs::components::CarriedProp>(props.front());
	EXPECT_EQ(prop.owner, villager);
	EXPECT_EQ(prop.type, 2);
	EXPECT_EQ(Reg().Get<const Mesh>(props.front()).id, resources::HashIdentifier(static_cast<MeshId>(342)));
	const auto& transform = Reg().Get<const Transform>(props.front());
	EXPECT_EQ(transform.position, glm::vec3(11.0f, 2.0f, 23.0f));
	EXPECT_EQ(transform.rotation[0], glm::vec3(-1.0f, 0.0f, 0.0f));
	EXPECT_EQ(transform.rotation[1], glm::vec3(0.0f, 0.0f, -1.0f));
	EXPECT_EQ(transform.rotation[2], glm::vec3(0.0f, -1.0f, 0.0f));

	// the clip moves the bone: the prop moves with it, the same prop
	Reg().Get<SkeletalAnimation>(villager).pose.at(k_GripBone) = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	ecs::UpdateCarriedProps();
	ASSERT_EQ(Props(), props);
	EXPECT_EQ(Reg().Get<const Transform>(props.front()).position, glm::vec3(10.0f, 1.0f, 20.0f));
}

// Each of the fourteen objects (2 axe ... 15 the third tree's log) is drawn; nothing for none (0) and the first entry (1)
TEST_F(CarriedPropsTest, FourteenObjectsAreDrawnAndNothingForNone)
{
	for (int32_t carried = 0; carried < 16; ++carried)
	{
		const auto villager = MakeVillager(carried);
		ecs::UpdateCarriedProps();
		EXPECT_EQ(Props().size(), carried > 1 ? 1u : 0u) << carried;
		Reg().Destroy(villager);
		ecs::UpdateCarriedProps();
		EXPECT_TRUE(Props().empty()) << carried;
	}
}

// Not while the hand holds the villager, nor in a state whose clip is not drawn; the tool comes back as soon as the
// villager is drawn with it again, and goes when the villager stops carrying it: it is never dropped or left behind
TEST_F(CarriedPropsTest, NotWhileHeldOrHiddenAndBackAsSoonAsItIsDrawn)
{
	const auto villager = MakeVillager(10); // a scythe
	ecs::UpdateCarriedProps();
	ASSERT_EQ(Props().size(), 1u);

	SetTop(villager, VillagerStates::InHand);
	ecs::UpdateCarriedProps();
	EXPECT_TRUE(Props().empty());

	SetTop(villager, k_NotDrawn);
	ecs::UpdateCarriedProps();
	EXPECT_TRUE(Props().empty());

	SetTop(villager, VillagerStates::MoveToPos);
	ecs::UpdateCarriedProps();
	EXPECT_EQ(Props().size(), 1u);

	// it stops carrying: the prop goes, with nothing left in the world
	Reg().Get<SkeletalAnimation>(villager).carriedObject = 1;
	ecs::UpdateCarriedProps();
	EXPECT_TRUE(Props().empty());
	size_t things = 0;
	Reg().Each<const Transform>([&things](entt::entity, const Transform&) { ++things; });
	EXPECT_EQ(things, 1u); // the villager alone
}

// The prop is only a drawn mesh: no villager, no life, no physics; it goes with its villager
TEST_F(CarriedPropsTest, AToolIsOnlyADrawnMeshThatGoesWithItsVillager)
{
	const auto villager = MakeVillager(4); // a crook
	ecs::UpdateCarriedProps();
	const auto props = Props();
	ASSERT_EQ(props.size(), 1u);
	EXPECT_TRUE((Reg().AllOf<ecs::components::CarriedProp, Transform, Mesh>(props.front())));
	EXPECT_FALSE(Reg().AnyOf<Villager>(props.front()));
	Reg().Destroy(villager);
	ecs::UpdateCarriedProps();
	EXPECT_TRUE(Props().empty());
}
