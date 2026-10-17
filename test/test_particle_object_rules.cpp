/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The object rules (Particles/ParticleObjectRules.cpp): the pure surface point and atom size on fakes, then
// CreateRule_GameObjectRef and ER_EmitFromParentAtom run on small effects with a fake land and a registry of their own.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Common/GameRandomTesting.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysRegistry.h"
#include "Particles/ParticleObjectRules.h"
#include "support/LandFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
constexpr float k_Step = 0.1f;
constexpr uint32_t k_FloatDraw = 0xFFFFu; ///< a FloatRand on the synced stream draws GameRand(0xFFFF)

const std::string k_Header = "BEGINPROPERTIES\n"
                             "PROPERTY DeleteOnCloseDown BOOL 0\n"
                             "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
                             "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
                             "PROPERTY MaxSpellAge FLOAT -1\n"
                             "ENDPROPERTIES\n";

/// CreateRule_GameObjectRef in group 0 with these properties and the next groups
std::string ObjectRef(const std::string& properties, const std::string& nextGroups = "ARRAY SIZE 0")
{
	return "BEGINCLASS CreateRule_GameObjectRef CreateRule_GameObjectRef0\nBEGINPROPERTIES\n"
	       "PROPERTY Condition PERSIS_PNTR NULL_STRING\nPROPERTY Group INTEGER 0\nPROPERTY NextGroups " +
	       nextGroups + "\nPROPERTY PCreator PERSIS_PNTR NULL_STRING\nPROPERTY RemoveOnCloseDown BOOL 0\n" + properties +
	       "ENDPROPERTIES\nENDCLASS\n";
}

/// SF_SparklesFromObject as the game has it: the object's atom, its emitter in group 1 gated by the base condition
/// TrueOnCloseDown inverted (it emits until the effect closes down), and the Grain sprite with no random flags.
/// `aboveLand` and `scaling` replace the file's EmitOnlyAboveLandscape 1 and DoScaling 0
std::string SparklesFile(bool aboveLand = true, bool scaling = false, int maxAtoms = 15, const std::string& object = "")
{
	return k_Header + ObjectRef(object, "ARRAY SIZE 1 1") +
	       "BEGINCLASS ParticleSpriteCreator ParticleSpriteCreator_Grain\nBEGINPROPERTIES\n"
	       "PROPERTY CentreAtBase BOOL 0\nPROPERTY ColorA INTEGER 255\nPROPERTY ColorB INTEGER 255\n"
	       "PROPERTY ColorG INTEGER 255\nPROPERTY ColorR INTEGER 255\nPROPERTY FrameRate FLOAT 20\n"
	       "PROPERTY InitFrame INTEGER 0\nPROPERTY InitialScale FLOAT 1\nPROPERTY LoopAnim BOOL 1\n"
	       "PROPERTY NumFrames INTEGER 32\nPROPERTY PlayAnim BOOL 1\nPROPERTY RandomiseFrameDirection BOOL 0\n"
	       "PROPERTY RandomiseInitFrame BOOL 0\nPROPERTY RandomiseScale BOOL 0\n"
	       "PROPERTY TextureFileName STRING .\\Data\\Textures\\S_SpriteSheet1.raw\nPROPERTY UseAdditiveAlpha BOOL 1\n"
	       "ENDPROPERTIES\nENDCLASS\n"
	       "BEGINCLASS ER_EmitFromParentAtom ER_EmitFromParentAtom0\nBEGINPROPERTIES\n"
	       "PROPERTY AtomAgeMaxSize FLOAT 0.25\nPROPERTY AtomAgeZeroSize FLOAT 1.7\n"
	       "PROPERTY Condition PERSIS_PNTR EventConditionTrueOnCloseDown0\nPROPERTY DeleteAtoms BOOL 1\n"
	       "PROPERTY DoScaling BOOL " +
	       std::string(scaling ? "1" : "0") +
	       "\nPROPERTY Group INTEGER 1\nPROPERTY MaxAlpha INTEGER 80\nPROPERTY MaxAtoms INTEGER " + std::to_string(maxAtoms) +
	       "\nPROPERTY NextGroups ARRAY SIZE 0\nPROPERTY PCreator PERSIS_PNTR ParticleSpriteCreator_Grain\n"
	       "PROPERTY PulseMagnitude FLOAT 1\nPROPERTY PulseSpeed FLOAT 1\nPROPERTY RemoveOnCloseDown BOOL 0\n"
	       "PROPERTY EmitOnlyAboveLandscape BOOL " +
	       std::string(aboveLand ? "1" : "0") +
	       "\nENDPROPERTIES\nENDCLASS\n"
	       "BEGINCLASS EventConditionTrueOnCloseDown EventConditionTrueOnCloseDown0\nBEGINPROPERTIES\n"
	       "PROPERTY InvertResponse BOOL 1\nENDPROPERTIES\nENDCLASS\n";
}

/// A model of fixed triangles: parts of items, each item a list of triangles
class FakeModel final: public object_surface::Model
{
public:
	using Triangle = std::array<glm::vec3, 3>;
	explicit FakeModel(std::vector<std::vector<std::vector<Triangle>>> parts)
	    : _parts(std::move(parts))
	{
	}
	[[nodiscard]] uint32_t PartCount() const override { return static_cast<uint32_t>(_parts.size()); }
	[[nodiscard]] uint32_t ItemCount(uint32_t part) const override { return static_cast<uint32_t>(_parts.at(part).size()); }
	[[nodiscard]] uint32_t TriangleCount(uint32_t part, uint32_t item) const override
	{
		return static_cast<uint32_t>(_parts.at(part).at(item).size());
	}
	[[nodiscard]] Triangle Corners(uint32_t part, uint32_t item, uint32_t triangle) const override
	{
		return _parts.at(part).at(item).at(triangle);
	}

private:
	std::vector<std::vector<std::vector<Triangle>>> _parts;
};

/// The draws of the synced stream, each answered by the next of `answers` (0 once they run out)
struct Draws
{
	std::vector<uint32_t> asked;
	std::vector<uint32_t> answers;
	void Hook()
	{
		game_random::testing::SetGameRand(
		    [this](uint32_t n) {
			    const auto answer = asked.size() < answers.size() ? answers[asked.size()] : 0u;
			    asked.push_back(n);
			    return answer;
		    },
		    [](float) { return 0.0f; });
	}
};

/// An effect of the text, its draws on the synced stream
std::unique_ptr<Effect> MakeEffect(const std::string& text)
{
	auto file = File::Parse(text, "test");
	EXPECT_TRUE(file.has_value());
	return std::make_unique<Effect>(std::make_shared<const File>(std::move(*file)), glm::vec3(0.0f), 1.0f,
	                                game_random::psys::NetGameType::Synced);
}

/// Small effects on a fake land (everywhere at its marker height) and a registry of their own; the previous services
/// come back after
class ObjectRulesTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::terrainSystem::emplace<test::WaterCellIsland>(static_cast<uint16_t>(1));
	}

	static std::unique_ptr<Effect> Make(const std::string& text) { return MakeEffect(text); }

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// An object (a feature): a place and a model (its mesh is not loaded here)
	static entt::entity Object(glm::vec3 position)
	{
		const auto entity = Reg().Create();
		Reg().Assign<ecs::components::Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<ecs::components::Mesh>(entity, entt::id_type {12345}, static_cast<int8_t>(-1), static_cast<int8_t>(-1));
		Reg().Assign<ecs::components::Feature>(entity);
		return entity;
	}

	static constexpr float k_Land = test::WaterCellIsland::k_HeightMarker;

private:
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::terrainSystem> _restoreTerrain;
};

} // namespace

TEST(ObjectSurface, ClassesAreRegistered)
{
	EXPECT_NE(FindModifierFactory("CreateRule_GameObjectRef"), nullptr);
	EXPECT_NE(FindModifierFactory("ER_EmitFromParentAtom"), nullptr);
}

TEST(ObjectSurface, TheTrianglePointSumsAsTheGameDoes)
{
	// ((c1 - c0) x a + c0) + (c2 - c0) x b, each a float: with c0 large, each small side added to it on its own is
	// rounded away, where the two sides added first would make a step of the float
	const std::array<glm::vec3, 3> corners = {glm::vec3(1.0e8f, 0.0f, 0.0f), glm::vec3(1.0e8f + 8.0f, 2.0f, 0.0f),
	                                          glm::vec3(1.0e8f + 8.0f, 0.0f, 4.0f)};
	const auto point = object_surface::TrianglePoint(corners, 0.5f, 0.5f);
	const float x = ((8.0f * 0.5f) + 1.0e8f) + (8.0f * 0.5f);
	EXPECT_EQ(std::bit_cast<uint32_t>(point.x), std::bit_cast<uint32_t>(x));
	EXPECT_FLOAT_EQ(point.y, 1.0f);
	EXPECT_FLOAT_EQ(point.z, 2.0f);
}

TEST(ObjectSurface, TheMatrixSumsZThenYThenX)
{
	// every axis on x: z + y first, then x, then the place. Summed x first, 1 + 1e8 would lose the 1
	const object_surface::Matrix matrix {
	    .x = {1.0f, 0.0f, 0.0f}, .y = {1.0f, 0.0f, 0.0f}, .z = {1.0f, 0.0f, 0.0f}, .translation = {0.0f, 5.0f, 0.0f}};
	const auto point = object_surface::ToWorld(matrix, {1.0f, 1.0e8f, -1.0e8f});
	EXPECT_EQ(point.x, 1.0f);
	EXPECT_EQ(point.y, 5.0f);
	EXPECT_EQ(point.z, 0.0f);
	// a turned and scaled model: the axes are the matrix's rows
	const object_surface::Matrix turned {
	    .x = {0.0f, 0.0f, -2.0f}, .y = {0.0f, 2.0f, 0.0f}, .z = {2.0f, 0.0f, 0.0f}, .translation = {10.0f, 20.0f, 30.0f}};
	const auto moved = object_surface::ToWorld(turned, {1.0f, 2.0f, 3.0f});
	EXPECT_EQ(moved, glm::vec3(16.0f, 24.0f, 28.0f));
}

TEST(ObjectSurface, APointDrawsPartItemTriangleThenTwoFractionsFoldedIntoTheTriangle)
{
	const game_random::testing::ScopedState state;
	Draws draws;
	// part 1 of 2, item 1 of 2, triangle 2 of 3; the fractions from 0xC000 and 0x8000 of 0xFFFF
	draws.answers = {1, 1, 2, 0xC000u, 0x8000u};
	draws.Hook();
	const FakeModel::Triangle unit = {glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)};
	const FakeModel::Triangle shifted = {glm::vec3(10.0f, 0.0f, 0.0f), glm::vec3(14.0f, 0.0f, 0.0f),
	                                     glm::vec3(10.0f, 0.0f, 8.0f)};
	const FakeModel model({{{unit}}, {{unit}, {unit, unit, shifted}}});
	auto effect = MakeEffect(k_Header);
	const object_surface::Matrix matrix {.translation = {0.0f, 100.0f, 0.0f}};
	float a = 0.0f;
	float b = 0.0f;
	std::optional<glm::vec3> point;
	{
		const game_random::psys::StepScope scope(game_random::psys::NetGameType::Synced);
		point = object_surface::RandomPoint(*effect, model, matrix);
	}
	ASSERT_EQ(draws.asked, (std::vector<uint32_t> {2, 2, 3, k_FloatDraw, k_FloatDraw}));
	// the same two fractions drawn again, to fold them as the game does
	draws.asked.clear();
	draws.answers = {0xC000u, 0x8000u};
	{
		const game_random::psys::StepScope scope(game_random::psys::NetGameType::Synced);
		a = effect->Random(1.0f);
		b = effect->Random(1.0f);
	}
	ASSERT_GT(a + b, 1.0f);
	a = 1.0f - a;
	b = 1.0f - b;
	ASSERT_TRUE(point.has_value());
	const float x = ((4.0f * a) + 10.0f) + (0.0f * b);
	const float z = ((0.0f * a) + 0.0f) + (8.0f * b);
	EXPECT_EQ(std::bit_cast<uint32_t>(point->x), std::bit_cast<uint32_t>(x));
	EXPECT_EQ(point->y, 100.0f);
	EXPECT_EQ(std::bit_cast<uint32_t>(point->z), std::bit_cast<uint32_t>(z));
}

TEST(ObjectSurface, NoPointAndNoMoreDrawsOnceACountIsZero)
{
	const game_random::testing::ScopedState state;
	Draws draws;
	draws.Hook();
	auto effect = MakeEffect(k_Header);
	const game_random::psys::StepScope scope(game_random::psys::NetGameType::Synced);
	EXPECT_FALSE(object_surface::RandomPoint(*effect, FakeModel({}), {}).has_value());
	EXPECT_TRUE(draws.asked.empty());
	EXPECT_FALSE(object_surface::RandomPoint(*effect, FakeModel({{{}}}), {}).has_value());
	EXPECT_EQ(draws.asked, (std::vector<uint32_t> {1, 1}));
}

TEST(ObjectSurface, TheEmittedAtomGrowsPulsesAndShrinks)
{
	// a ramp up to 1 at 0.25 s and down to 0 at 2 s, with no pulse (magnitude 0)
	EXPECT_FLOAT_EQ(object_surface::EmittedAtomScale(0.0f, 1.0f, 0.0f, 0.25f, 2.0f), 0.0f);
	EXPECT_FLOAT_EQ(object_surface::EmittedAtomScale(0.125f, 1.0f, 0.0f, 0.25f, 2.0f), 0.5f);
	EXPECT_FLOAT_EQ(object_surface::EmittedAtomScale(0.25f, 1.0f, 0.0f, 0.25f, 2.0f), 1.0f);
	EXPECT_FLOAT_EQ(object_surface::EmittedAtomScale(1.125f, 1.0f, 0.0f, 0.25f, 2.0f), 0.5f);
	EXPECT_FLOAT_EQ(object_surface::EmittedAtomScale(3.0f, 1.0f, 0.0f, 0.25f, 2.0f), 0.0f);
	// the pulse: cos(age x speed x 2 pi) x magnitude / 2 + 1, 1.5 at age 0 (times the ramp's 0 there), 0.5 at half a
	// cycle, never below 0
	EXPECT_FLOAT_EQ(object_surface::EmittedAtomScale(0.5f, 1.0f, 1.0f, 0.25f, 2.0f), 0.5f * (1.0f - (0.25f / 1.75f)));
	EXPECT_FLOAT_EQ(object_surface::EmittedAtomScale(0.5f, 1.0f, 4.0f, 0.25f, 2.0f), 0.0f);
	// not a number gives 0, where a clamp would keep it
	const float nan = std::numeric_limits<float>::quiet_NaN();
	EXPECT_EQ(object_surface::EmittedAtomScale(nan, 1.0f, 0.0f, 0.25f, 2.0f), 0.0f);
	EXPECT_EQ(object_surface::EmittedAtomScale(0.5f, 1.0f, 0.0f, 0.5f, 0.5f), 0.0f);
}

TEST_F(ObjectRulesTest, TheObjectRuleKeepsTheEffectUntilItClosesWithoutMakingAtoms)
{
	auto effect = Make(k_Header + ObjectRef(""));
	for (int i = 0; i < 5; ++i)
	{
		effect->Step(k_Step);
	}
	EXPECT_EQ(effect->AtomCount(), 0u);
	EXPECT_FALSE(effect->Finished());
	effect->CloseDown();
	effect->Step(k_Step);
	EXPECT_TRUE(effect->Finished());
}

TEST_F(ObjectRulesTest, OneObjectTargetAStepAndOnlyObjects)
{
	const game_random::testing::ScopedState state;
	Draws draws;
	draws.Hook();
	auto effect = Make(k_Header + ObjectRef("PROPERTY Alpha INTEGER 100\n"));
	const auto first = Object({100.0f, k_Land + 2.0f, 200.0f});
	const auto second = Object({300.0f, k_Land, 400.0f});
	// a thing with only a place is not an object: it is taken and dropped
	const auto notObject = Reg().Create();
	Reg().Assign<ecs::components::Transform>(notObject, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	effect->AddTarget(first);
	effect->AddTarget(notObject);
	effect->AddTarget(second);
	effect->Step(k_Step);
	EXPECT_EQ(effect->AtomCount(), 1u);
	EXPECT_EQ(effect->GetTargets().size(), 2u);
	effect->Step(k_Step);
	EXPECT_EQ(effect->AtomCount(), 1u);
	EXPECT_EQ(effect->GetTargets().size(), 1u);
	effect->Step(k_Step);
	EXPECT_EQ(effect->AtomCount(), 2u);
	EXPECT_TRUE(effect->GetTargets().empty());
	// one Rand(0x100) for each atom, nothing else
	EXPECT_EQ(draws.asked, (std::vector<uint32_t> {0x100u, 0x100u}));
	// the atoms are not drawn (no creator)
	std::vector<Effect::DrawAtom> drawn;
	effect->Collect(1.0f, drawn);
	EXPECT_TRUE(drawn.empty());
}

TEST_F(ObjectRulesTest, OnlyGameObjectsAreKeptWhateverTheirModel)
{
	const game_random::testing::ScopedState state;
	Draws draws;
	draws.Hook();
	auto effect = Make(k_Header + ObjectRef(""));
	// the hand has a place and a model, but is no game object
	const auto hand = Reg().Create();
	Reg().Assign<ecs::components::Transform>(hand, glm::vec3(10.0f, k_Land, 10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<ecs::components::Mesh>(hand, entt::id_type {12345}, static_cast<int8_t>(-1), static_cast<int8_t>(-1));
	Reg().Assign<ecs::components::Hand>(hand);
	// a fish farm has no model, and is one
	const auto farm = Reg().Create();
	Reg().Assign<ecs::components::Transform>(farm, glm::vec3(100.0f, k_Land, 200.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<ecs::components::FishFarm>(farm);
	EXPECT_FALSE(object_surface::IsGameObject(hand));
	EXPECT_TRUE(object_surface::IsGameObject(farm));
	EXPECT_FALSE(object_surface::IsGameObject(entt::null));
	effect->AddTarget(hand);
	effect->AddTarget(farm);
	// the last target first: the farm's atom with its Rand(0x100), then the hand dropped with no draw
	effect->Step(k_Step);
	EXPECT_EQ(effect->AtomCount(), 1u);
	effect->Step(k_Step);
	EXPECT_EQ(effect->AtomCount(), 1u);
	EXPECT_TRUE(effect->GetTargets().empty());
	EXPECT_EQ(draws.asked, (std::vector<uint32_t> {0x100u}));
}

TEST(ObjectSurface, AnObjectTurnedAboutYRoundsItsCosineOnceWithTheScale)
{
	const glm::vec3 place {1.0f, 2.0f, 3.0f};
	// no angle: the scale down the diagonal, the identity for a scale of 1; an angle or a scale that is not a number
	// counts as none or as 1
	const float nan = std::numeric_limits<float>::quiet_NaN();
	for (const float angle : {0.0f, nan})
	{
		const auto scaled = object_surface::ObjectWorldMatrix(place, angle, 2.0f);
		EXPECT_EQ(scaled.x, glm::vec3(2.0f, 0.0f, 0.0f));
		EXPECT_EQ(scaled.y, glm::vec3(0.0f, 2.0f, 0.0f));
		EXPECT_EQ(scaled.z, glm::vec3(0.0f, 0.0f, 2.0f));
		EXPECT_EQ(scaled.translation, place);
		const auto unit = object_surface::ObjectWorldMatrix(place, angle, nan);
		EXPECT_EQ(unit.x, glm::vec3(1.0f, 0.0f, 0.0f));
		EXPECT_EQ(unit.z, glm::vec3(0.0f, 0.0f, 1.0f));
	}
	// a scale of 1: the angle's cosine and sine as floats, the object rotation about Y
	const auto turned = object_surface::ObjectWorldMatrix(place, 0.5f, 1.0f);
	const auto rows = affine::AngleY(0.5f);
	EXPECT_EQ(turned.x, rows[0]);
	EXPECT_EQ(turned.y, rows[1]);
	EXPECT_EQ(turned.z, rows[2]);
	// another scale: the cosine is multiplied before it is rounded. At this angle that is one unit of the float away
	// from the rounded cosine times the scale
	const float angle = 0.0013f;
	const auto big = object_surface::ObjectWorldMatrix(place, angle, 3.0f);
	const auto once = static_cast<float>(std::cos(static_cast<double>(angle)) * 3.0);
	const auto twice = static_cast<float>(std::cos(static_cast<double>(angle))) * 3.0f;
	ASSERT_NE(std::bit_cast<uint32_t>(once), std::bit_cast<uint32_t>(twice));
	EXPECT_EQ(std::bit_cast<uint32_t>(big.x.x), std::bit_cast<uint32_t>(once));
	EXPECT_EQ(std::bit_cast<uint32_t>(big.z.z), std::bit_cast<uint32_t>(once));
	const auto sine = static_cast<float>(std::sin(static_cast<double>(angle)) * 3.0);
	EXPECT_EQ(big.x.z, sine);
	EXPECT_EQ(big.z.x, -sine);
	EXPECT_EQ(big.y, glm::vec3(0.0f, 3.0f, 0.0f));
	// the sums with 0 make a zero positive
	EXPECT_FALSE(std::signbit(big.x.y));
	EXPECT_FALSE(std::signbit(big.z.y));
}

TEST(ObjectSurface, AMobileObjectScalesEveryCellOfItsTurnUnlessTheScaleIs1)
{
	const glm::vec3 place {1.0f, 2.0f, 3.0f};
	const auto rotation = affine::RotationYXZ(0.5f, 0.25f, -0.75f);
	const auto unit = object_surface::MobileWorldMatrix(place, rotation, 1.0f);
	EXPECT_EQ(unit.x, rotation[0]);
	EXPECT_EQ(unit.y, rotation[1]);
	EXPECT_EQ(unit.z, rotation[2]);
	EXPECT_EQ(unit.translation, place);
	const auto nan = object_surface::MobileWorldMatrix(place, rotation, std::numeric_limits<float>::quiet_NaN());
	EXPECT_EQ(nan.y, rotation[1]);
	const auto scaled = object_surface::MobileWorldMatrix(place, rotation, 1.5f);
	EXPECT_EQ(scaled.x, rotation[0] * 1.5f);
	EXPECT_EQ(scaled.y, rotation[1] * 1.5f);
	EXPECT_EQ(scaled.z, rotation[2] * 1.5f);
}

TEST(ObjectSurface, TheLandIsAskedAtTheGamesMapCoordinates)
{
	// x 65536 then x 0.1, each product a float, truncated toward zero
	const glm::vec3 point {1234.5678f, 0.0f, -2.71828f};
	const auto cell = object_surface::LandTestCell(point);
	EXPECT_EQ(cell.x, static_cast<int32_t>((point.x * 65536.0f) * 0.1f));
	EXPECT_EQ(cell.y, static_cast<int32_t>((point.z * 65536.0f) * 0.1f));
	// 65536 x 0.1f is 6553.6f exactly, so it is the map coordinates' own conversion, value for value
	static_assert(65536.0f * 0.1f == map_coords::k_FixedPerMetre);
	EXPECT_EQ(cell.x, map_coords::ToFixed(point.x));
	EXPECT_EQ(cell.y, map_coords::ToFixed(point.z));
}

TEST_F(ObjectRulesTest, AnObjectsWorldMatrixIsTheOneOfItsClass)
{
	const glm::vec3 position {100.3f, k_Land + 2.0f, 200.7f};
	const auto place = map_coords::ToWorld(map_coords::FromWorld(position));
	// a pile: all three angles, every cell times its scale, at its map coordinates
	const auto pile = Reg().Create();
	const auto turn = affine::RotationYXZ(0.5f, 0.25f, -0.75f);
	Reg().Assign<ecs::components::Transform>(pile, position, turn, glm::vec3(2.0f));
	Reg().Assign<ecs::components::Pot>(pile);
	const auto pileMatrix = object_surface::WorldMatrixOf(pile);
	EXPECT_EQ(pileMatrix.x, turn[0] * 2.0f);
	EXPECT_EQ(pileMatrix.y, turn[1] * 2.0f);
	EXPECT_EQ(pileMatrix.z, turn[2] * 2.0f);
	EXPECT_EQ(pileMatrix.translation, place);
	// a feature: Y only; with a scale of 1 the cells are the angle's cosine and sine as the Transform holds them
	const auto feature = Object(position);
	Reg().Get<ecs::components::Transform>(feature).rotation = affine::AngleY(0.5f);
	const auto unit = object_surface::WorldMatrixOf(feature);
	EXPECT_EQ(unit.x, affine::AngleY(0.5f)[0]);
	EXPECT_EQ(unit.z, affine::AngleY(0.5f)[2]);
	EXPECT_EQ(unit.translation, place);
	// another scale: the Y turn of the angle read back from the Transform, with the cosine rounded with the scale
	Reg().Get<ecs::components::Transform>(feature).scale = glm::vec3(3.0f);
	float y = 0.0f;
	float x = 0.0f;
	float z = 0.0f;
	affine::DecomposeYXZ(affine::AngleY(0.5f), y, x, z);
	const auto scaled = object_surface::WorldMatrixOf(feature);
	const auto expected = object_surface::ObjectWorldMatrix(place, y, 3.0f);
	EXPECT_EQ(scaled.x, expected.x);
	EXPECT_EQ(scaled.y, glm::vec3(0.0f, 3.0f, 0.0f));
	EXPECT_EQ(scaled.z, expected.z);
	EXPECT_NEAR(scaled.x.x, 3.0f * std::cos(0.5f), 1e-6f);
}

TEST_F(ObjectRulesTest, TheAtomStandsOnTheObjectRaisedByTheOffsetUntilTheObjectGoes)
{
	// the object has no model here, so each sparkle is let out at the object's atom: one a step shows where it was
	auto effect = Make(SparklesFile(false, false, 15, "PROPERTY OffsetY FLOAT 1.0\n"));
	const auto object = Object({100.0f, k_Land + 2.0f, 200.0f});
	effect->AddTarget(object);
	const auto placeOf = [](glm::vec3 position) {
		// the land at the map coordinates, plus the object's height above it, plus the offset
		auto place = map_coords::ToWorld(map_coords::FromWorld(position));
		place.y = place.y + 1.0f;
		return place;
	};
	std::vector<glm::vec3> expected;
	effect->Step(k_Step);
	expected.push_back(placeOf({100.0f, k_Land + 2.0f, 200.0f}));
	// it follows the object
	Reg().Get<ecs::components::Transform>(object).position = {150.0f, k_Land + 5.0f, 250.0f};
	effect->Step(k_Step);
	expected.push_back(placeOf({150.0f, k_Land + 5.0f, 250.0f}));
	// unavailable: still read the step it is found so, then let go
	Reg().Assign<ecs::components::Unavailable>(object);
	Reg().Get<ecs::components::Transform>(object).position = {160.0f, k_Land + 5.0f, 260.0f};
	effect->Step(k_Step);
	expected.push_back(placeOf({160.0f, k_Land + 5.0f, 260.0f}));
	Reg().Get<ecs::components::Transform>(object).position = {500.0f, k_Land, 500.0f};
	effect->Step(k_Step);
	expected.push_back(expected.back());
	std::vector<Effect::DrawAtom> drawn;
	effect->Collect(1.0f, drawn);
	ASSERT_EQ(drawn.size(), expected.size());
	for (size_t i = 0; i < drawn.size(); ++i)
	{
		EXPECT_EQ(drawn[i].position, expected[i]) << i;
	}
}

TEST_F(ObjectRulesTest, NoSparklesFromAnAnimatedModel)
{
	// (not ported) an animated model gives one of its bones
	auto effect = Make(SparklesFile(false));
	const auto object = Object({100.0f, k_Land + 2.0f, 200.0f});
	Reg().Assign<ecs::components::SkeletalAnimation>(object);
	effect->AddTarget(object);
	for (int i = 0; i < 3; ++i)
	{
		effect->Step(k_Step);
	}
	EXPECT_EQ(effect->AtomCount(), 1u);
}

TEST_F(ObjectRulesTest, SparklesComeAtTheRateUpToTheirMostAndOnlyAboveTheLand)
{
	const game_random::testing::ScopedState state;
	Draws draws;
	draws.Hook();
	auto effect = Make(SparklesFile());
	// above the land: no model here, so each sparkle is let out at the atom's place
	effect->AddTarget(Object({100.0f, k_Land + 2.0f, 200.0f}));
	// 15 over 1.7 s: owed 0.88235 a step of 0.1 s; one let out whenever the count let out is below what is owed
	std::vector<size_t> counts;
	for (int i = 0; i < 4; ++i)
	{
		effect->Step(k_Step);
		counts.push_back(effect->AtomCount());
	}
	// the object's atom and 1, 2, 3, 4 sparkles (0.88, 1.76, 2.65, 3.53 owed)
	EXPECT_EQ(counts, (std::vector<size_t> {2, 3, 4, 5}));
	// each atom one Rand(0x100): the object's, then a sparkle a step
	EXPECT_EQ(draws.asked, (std::vector<uint32_t>(5, 0x100u)));
	std::vector<Effect::DrawAtom> drawn;
	effect->Collect(1.0f, drawn);
	ASSERT_EQ(drawn.size(), 4u);
	const auto place = map_coords::ToWorld(map_coords::FromWorld(glm::vec3(100.0f, k_Land + 2.0f, 200.0f)));
	for (const auto& sparkle : drawn)
	{
		EXPECT_EQ(sparkle.position, place);
	}

	// below the land: owed and counted as let out, but none made
	auto below = Make(SparklesFile());
	below->AddTarget(Object({100.0f, k_Land - 2.0f, 200.0f}));
	for (int i = 0; i < 4; ++i)
	{
		below->Step(k_Step);
	}
	EXPECT_EQ(below->AtomCount(), 1u);
	// without the land test they are made
	auto anywhere = Make(SparklesFile(false));
	anywhere->AddTarget(Object({100.0f, k_Land - 2.0f, 200.0f}));
	for (int i = 0; i < 4; ++i)
	{
		anywhere->Step(k_Step);
	}
	EXPECT_EQ(anywhere->AtomCount(), 5u);
}

TEST_F(ObjectRulesTest, APointNotGivenIsThePreviousSparklesOwnLocalPosition)
{
	// the sparkles' group under the object's atom (Hierarchies of group 0), and a parent whose model is not loaded, so
	// it gives no point: the step's first sparkle starts at the parent's place, each next one at the local position the
	// one before was turned into
	std::string text = SparklesFile(false);
	const std::string flat = "PROPERTY Hierarchies ARRAY SIZE 25 0 ";
	text.replace(text.find(flat), flat.size(), "PROPERTY Hierarchies ARRAY SIZE 25 1 ");
	auto effect = Make(text);
	effect->AddTarget(Object({100.0f, k_Land + 2.0f, 200.0f}));
	// 15 over 1.7 s owes 2.6 in 0.3 s: three sparkles in one step
	effect->Step(0.3f);
	ASSERT_EQ(effect->AtomCount(), 4u);
	const auto place = map_coords::ToWorld(map_coords::FromWorld(glm::vec3(100.0f, k_Land + 2.0f, 200.0f)));
	// their local positions 0, -place and -2 place, drawn on the parent at place, 0 and -place
	std::vector<Effect::DrawAtom> drawn;
	effect->Collect(1.0f, drawn);
	ASSERT_EQ(drawn.size(), 3u);
	for (const auto& expected : {place, glm::vec3(0.0f), -place})
	{
		const auto found = std::ranges::count_if(drawn, [&](const auto& atom) { return atom.position == expected; });
		EXPECT_EQ(found, 1) << expected.x << " " << expected.y << " " << expected.z;
	}
}

TEST_F(ObjectRulesTest, NoMoreSparklesThanTheMostAtOnceAndTheyGoWhenOld)

{
	// three at most: a long step owes more than three (3 / 1.7 a second for 2.1 s), but the count stops it; what it
	// holds back stays owed
	auto effect = Make(SparklesFile(false, false, 3));
	effect->AddTarget(Object({100.0f, k_Land + 2.0f, 200.0f}));
	effect->Step(k_Step);
	effect->Step(2.0f);
	EXPECT_EQ(effect->AtomCount(), 1u + 3u);
	// AtomAgeZeroSize 1.7: gone once older
	for (int i = 0; i < 25; ++i)
	{
		effect->Step(k_Step);
	}
	EXPECT_LE(effect->AtomCount(), 1u + 3u);
}

TEST_F(ObjectRulesTest, SparklesStopOnceTheEffectClosesDown)
{
	auto effect = Make(SparklesFile(false));
	effect->AddTarget(Object({100.0f, k_Land + 2.0f, 200.0f}));
	effect->Step(k_Step);
	effect->Step(k_Step);
	const auto before = effect->AtomCount();
	EXPECT_EQ(before, 3u);
	effect->CloseDown();
	effect->Step(k_Step);
	effect->Step(k_Step);
	EXPECT_EQ(effect->AtomCount(), before);
}

TEST_F(ObjectRulesTest, SparklesComeFromTheAtomsPlaceOnceItsObjectHasGone)
{
	auto effect = Make(SparklesFile(false));
	const auto object = Object({100.0f, k_Land + 2.0f, 200.0f});
	effect->AddTarget(object);
	effect->Step(k_Step);
	// the object goes; the atom stays where it was, and the sparkles come from there
	Reg().Destroy(object);
	for (int i = 0; i < 3; ++i)
	{
		effect->Step(k_Step);
	}
	std::vector<Effect::DrawAtom> drawn;
	effect->Collect(1.0f, drawn);
	ASSERT_EQ(drawn.size(), 4u);
	const auto place = map_coords::ToWorld(map_coords::FromWorld(glm::vec3(100.0f, k_Land + 2.0f, 200.0f)));
	for (const auto& sparkle : drawn)
	{
		EXPECT_EQ(sparkle.position, place);
	}
}

TEST_F(ObjectRulesTest, AnEmitterWithoutAParentLetsGo)
{
	// in a root group: no parent atom, so the emitter is detached and the effect, with nothing else, ends
	auto effect =
	    Make(k_Header + "BEGINCLASS ParticlePointCreator P0\nBEGINPROPERTIES\nPROPERTY InitialScale FLOAT 1\n"
	                    "ENDPROPERTIES\nENDCLASS\n"
	                    "BEGINCLASS ER_EmitFromParentAtom E0\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
	                    "PROPERTY NextGroups ARRAY SIZE 0\nPROPERTY PCreator PERSIS_PNTR P0\nENDPROPERTIES\nENDCLASS\n");
	effect->Step(k_Step);
	EXPECT_EQ(effect->AtomCount(), 0u);
	EXPECT_TRUE(effect->Finished());
}
