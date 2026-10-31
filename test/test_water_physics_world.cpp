/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Things dropped into the sea, as the testbed scene physics.sinking drops them, played turn by turn in a small world of
// the game's own systems and data over a land of open sea beside a raised lake: only the sea counts as water, a body
// hitting it makes a ring twice its radius and six foam puffs, a floating thing bobs with small rings and drifts with
// nothing but its own speed, a heavy one sinks and is gone deep under, and a dead thing grows denser every turn. Every
// expected value is the original's (docs/wiki/physics/physics.md, docs/wiki/project/progress/physics/water_physics.md).

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <span>
#include <variant>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/LandIslandInterface.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Components/Life.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Systems/Implementations/WaterRingSystem.h"
#include "Graphics/ArgbColour.h"
#include "Physics/Body.h"
#include "support/LandFakes.h"
#include "support/RestoreService.h"
#include "support/WorldHarness.h"

using namespace openblack;
using ecs::components::Life;

namespace
{
/// West of x = 1500 open sea with its floor 40 m down; east of it a raised lake, water in cells of altitude 6 unless
/// given another
class SeaBesideALake final: public LandIslandInterface
{
public:
	static constexpr float k_Shore = 1500.0f;
	static constexpr float k_SeaFloor = -40.0f;
	static constexpr uint8_t k_LakeAltitude = 6;

	explicit SeaBesideALake(uint8_t lakeAltitude = k_LakeAltitude)
	    : _lake(test::MakeWaterCell(lakeAltitude, true, false))
	{
	}

	[[nodiscard]] float LakeHeight() const { return static_cast<float>(_lake.altitude) * k_HeightUnit; }
	[[nodiscard]] float GetHeightAt(glm::vec2 xz) const final { return xz.x < k_Shore ? k_SeaFloor : LakeHeight(); }
	[[nodiscard]] float GetUnflattenedHeightAt(glm::vec2 xz) const final { return GetHeightAt(xz); }
	[[nodiscard]] glm::vec3 GetNormalAt(glm::vec2) const final { return {0.0f, 1.0f, 0.0f}; }
	[[nodiscard]] const lnd::LNDCell& GetCell(const glm::u16vec2& cell) const final
	{
		return static_cast<float>(cell.x) * 10.0f < k_Shore ? _sea : _lake;
	}
	void DumpTextures() const final {}
	void DumpMaps() const final {}
	[[nodiscard]] std::vector<LandBlock>& GetBlocks() final { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<LandBlock>& GetBlocks() const final { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<lnd::LNDCountry>& GetCountries() const final { return _countries; }
	[[nodiscard]] const graphics::Texture2D& GetAlbedoArray() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetBump() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetSmallBump() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetHeightMap() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetCellMap() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] U16Extent2 GetIndexExtent() const final { return {}; }
	[[nodiscard]] Extent2 GetExtent() const final { return {}; }
	uint8_t GetNoise(glm::u8vec2) final { return 0; }

private:
	lnd::LNDCell _sea {test::MakeWaterCell(0, true, false)};
	lnd::LNDCell _lake;
	std::vector<lnd::LNDCountry> _countries;
};

constexpr std::string_view k_Scene = "physics.sinking";
const glm::vec2 k_Sea {1005.0f, 1205.0f};
const glm::vec2 k_Lake {1605.0f, 1205.0f};

class WaterPhysicsWorld: public test::world::WorldHarness
{
protected:
	void SetUp() override
	{
		WorldHarness::SetUp();
		if (IsSkipped() || HasFatalFailure())
		{
			return;
		}
		Locator::terrainSystem::emplace<SeaBesideALake>();
		_rings =
		    &static_cast<ecs::systems::WaterRingSystem&>(Locator::waterRingSystem::emplace<ecs::systems::WaterRingSystem>());
		if (GameData())
		{
			return;
		}
		// the scene's rock and the toy ball: a rock, and a light thing
		auto& rock = Info().mobileStatic.at(static_cast<size_t>(SceneRock()));
		rock.mobileType = static_cast<MobileStaticInfo>(2);
		rock.meshId = MeshId::ObjectRockChalk;
		rock.weight = 300.0f;
		auto& ball = Info().mobileStatic.at(static_cast<size_t>(MobileStaticInfo::ToyBall));
		ball.meshId = MeshId::ObjectToyBall;
		ball.weight = 100.0f;
	}

	/// The scene's first thing, a rock
	[[nodiscard]] static MobileStaticInfo SceneRock() { return std::get<MobileStaticInfo>(Scene(k_Scene).objects.at(0).type); }

	/// A thing of the row let go this high above the sea's surface (or the lake's), as the scene drops its things
	[[nodiscard]] ecs::PhysicsEntry& Dropped(MobileStaticInfo row, glm::vec2 at, float height) const
	{
		LoadMesh(Info().mobileStatic.at(static_cast<size_t>(row)).meshId, glm::vec3(-1.0f, 0.0f, -1.0f),
		         glm::vec3(1.0f, 2.0f, 1.0f));
		const auto& land = static_cast<const SeaBesideALake&>(Locator::terrainSystem::value());
		const float surface = at.x < SeaBesideALake::k_Shore ? 0.0f : land.LakeHeight();
		const auto thing = ecs::archetypes::MobileStaticArchetype::CreateFromInfo(glm::vec3(at.x, surface + height, at.y), row,
		                                                                          0.0f, 0.0f, 1.0f);
		auto* po = Dynamics().AddObject(thing, Scene(k_Scene).throws.at(0).velocity, glm::vec3(0.0f));
		EXPECT_NE(po, nullptr);
		return *po;
	}

	[[nodiscard]] std::span<const water_rings::Ring> Rings() const { return _rings->GetRings(); }

private:
	test::RestoreService<Locator::waterRingSystem> _restoreRings;
	ecs::systems::WaterRingSystem* _rings {nullptr};
};
} // namespace

// Only the sea counts as water: a thing dropped into the raised lake lands on its floor and is never in the water
TEST_P(WaterPhysicsWorld, OnlyTheSeaCountsAsWater)
{
	const auto thrown = Scene(k_Scene).throws.at(0);
	const auto inSea = Dropped(SceneRock(), k_Sea, thrown.height).entity;
	const auto inLake = Dropped(SceneRock(), k_Lake, thrown.height).entity;
	bool seaWet = false;
	for (int turn = 0; turn < 60; ++turn)
	{
		Turn();
		if (const auto* po = Dynamics().Find(inSea))
		{
			seaWet = seaWet || po->body.inWater;
		}
		if (const auto* po = Dynamics().Find(inLake))
		{
			EXPECT_FALSE(po->body.inWater) << "turn " << turn;
		}
	}
	EXPECT_TRUE(seaWet);
}

// Hitting the sea a body makes a ring that grows to twice its radius, the faster the smaller it is, and six puffs
TEST_P(WaterPhysicsWorld, HittingTheSeaMakesARingTwiceTheRadiusAndSixFoamPuffs)
{
	auto& po = Dropped(SceneRock(), k_Sea, 1.0f);
	const float radius = po.body.Radius();
	ASSERT_TRUE(Rings().empty());
	for (int turn = 0; turn < 30 && Rings().empty(); ++turn)
	{
		Turn();
	}
	ASSERT_FALSE(Rings().empty());
	const auto& ring = Rings().front();
	EXPECT_EQ(ring.cell, 0x3F);
	EXPECT_EQ(ring.growth, 2.0f * radius);
	EXPECT_EQ(ring.rate, 1.0f / radius);
	EXPECT_EQ(ring.position.y, 0.1f);
	// the puffs are drawn with the game's own puff sprite: without the game's files there is none, and no puff is made
	if (GameData())
	{
		const auto& puffs = Dynamics().GetState().dustPuffs;
		EXPECT_EQ(puffs.size(), 6u);
		// the foam's colour, premultiplied by its alpha
		const auto foam = argb_colour::ToVec4(0x28C8F0F4u);
		for (const auto& puff : puffs)
		{
			EXPECT_EQ(puff.draw.tint, glm::vec4(glm::vec3(foam) * foam.a, foam.a));
		}
	}
}

// A floating thing bobs, making a small ring as it turns about near the surface, and drifts only with its own speed
TEST_P(WaterPhysicsWorld, AFloatingThingBobsWithSmallRingsAndDriftsWithNothingButItsOwnSpeed)
{
	auto& po = Dropped(MobileStaticInfo::ToyBall, k_Sea, 1.0f);
	ASSERT_LT(po.body.density, 1.0f);
	const auto ball = po.entity;
	const auto start = po.body.Centre();
	size_t bobs = 0;
	for (int turn = 0; turn < 100; ++turn)
	{
		Turn();
		bobs = static_cast<size_t>(std::ranges::count_if(Rings(), [](const auto& ring) { return ring.cell == 0x30; }));
	}
	EXPECT_GT(bobs, 0u);
	const auto* now = Dynamics().Find(ball);
	ASSERT_NE(now, nullptr);
	EXPECT_EQ(now->body.Centre().x, start.x);
	EXPECT_EQ(now->body.Centre().z, start.z);
}

// A heavy thing sinks and is deleted deep under the sea
TEST_P(WaterPhysicsWorld, AHeavyThingSinksAndIsGoneDeepUnderTheSea)
{
	auto& po = Dropped(SceneRock(), k_Sea, Scene(k_Scene).throws.at(0).height);
	ASSERT_GT(po.body.density, 1.0f);
	const auto rock = po.entity;
	bool gone = false;
	for (int turn = 0; turn < 600 && !gone; ++turn)
	{
		Turn();
		gone = Dynamics().Find(rock) == nullptr;
	}
	EXPECT_TRUE(gone);
}

// A floating thing soaks up water only while it is in it, at most twenty fifteen-thousandths of density a turn (ten
// turns a second), so it floats at least as long as its density takes to pass water's, and then goes under and is gone:
// how long a thing floats follows from its density alone
TEST_P(WaterPhysicsWorld, AFloatingThingFloatsUntilItHasSoakedPastWatersDensityAndThenSinks)
{
	auto& po = Dropped(MobileStaticInfo::ToyBall, k_Sea, 1.0f);
	const float start = po.body.density;
	ASSERT_LT(start, 1.0f);
	const auto ball = po.entity;
	constexpr float k_MostPerTurn = physics::k_SoakPerStep * static_cast<float>(physics::k_StepsPerTurn);
	const auto least = static_cast<int>(std::floor((1.0f - start) / k_MostPerTurn));
	float density = start;
	int floated = -1;
	int turn = 0;
	for (; turn < 4 * least + 600; ++turn)
	{
		Turn();
		const auto* now = Dynamics().Find(ball);
		if (now == nullptr)
		{
			break;
		}
		// never drier, and never more than a turn's worth of soaking at once
		EXPECT_GE(now->body.density, density) << "turn " << turn;
		EXPECT_LE(now->body.density - density, k_MostPerTurn * 1.001f) << "turn " << turn;
		density = now->body.density;
		if (floated < 0 && density > 1.0f)
		{
			floated = turn;
		}
	}
	ASSERT_GE(floated, 0) << "it never soaked past water's density";
	EXPECT_GE(floated, least);
	// sunk past water's density, it is gone
	EXPECT_EQ(Dynamics().Find(ball), nullptr);
}

// A dead thing in the physics, its life under a hundredth, grows a hundredth denser every turn
TEST_P(WaterPhysicsWorld, ADeadThingGrowsAHundredthDenserEveryTurn)
{
	// high above the sea, out of the water, so that nothing else changes its density
	auto& po = Dropped(MobileStaticInfo::ToyBall, k_Sea, 200.0f);
	const auto ball = po.entity;
	Registry().Assign<Life>(ball, Life {.value = 0.005f});
	float density = po.body.density;
	for (int turn = 0; turn < 5; ++turn)
	{
		Turn();
		const auto* now = Dynamics().Find(ball);
		ASSERT_NE(now, nullptr);
		ASSERT_FALSE(now->body.inWater);
		density += 0.01f;
		EXPECT_EQ(now->body.density, density) << "turn " << turn;
	}
}

// A river or lake of low land splashes as the sea does, by its cell's altitude: below 4 a thing landing in it makes a
// ring, and below 3 it lands in water, with the foam's puffs; dry land makes no ring
TEST_P(WaterPhysicsWorld, ALowRiverOrLakeRingsBelowFourAndIsWaterBelowThree)
{
	const auto landsIn = [this](uint8_t altitude) {
		Locator::terrainSystem::emplace<SeaBesideALake>(altitude);
		Locator::waterRingSystem::value().Reset();
		Dynamics().GetState().dustPuffs.clear();
		const auto rock = Dropped(SceneRock(), k_Lake + glm::vec2(0.0f, 20.0f * altitude), 1.0f).entity;
		for (int turn = 0; turn < 30 && Dynamics().GetState().dustPuffs.empty() && Rings().empty(); ++turn)
		{
			Turn();
		}
		static_cast<void>(rock);
	};
	const auto foam = argb_colour::ToVec4(0x28C8F0F4u);
	const auto foamTint = glm::vec4(glm::vec3(foam) * foam.a, foam.a);
	for (const uint8_t altitude : {uint8_t {2}, uint8_t {3}, uint8_t {6}})
	{
		landsIn(altitude);
		EXPECT_EQ(Rings().empty(), altitude >= 4) << static_cast<int>(altitude);
		// the puffs on the game's data, which has their sprite: the foam's below 3, the dust's above
		if (GameData())
		{
			const auto& puffs = Dynamics().GetState().dustPuffs;
			ASSERT_EQ(puffs.size(), 6u) << static_cast<int>(altitude);
			EXPECT_EQ(puffs.front().draw.tint == foamTint, altitude < 3) << static_cast<int>(altitude);
		}
	}
}

INSTANTIATE_TEST_SUITE_P(Data, WaterPhysicsWorld,
                         ::testing::Values(test::world::WorldData::Synthetic, test::world::WorldData::Game),
                         test::world::DataName);
