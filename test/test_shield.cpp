/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The shields: the shield spell's radius clamp and upkeep, the physical shield's curves, the DefensiveSphere
// helpers and registry, the fireball's DoAnyShieldDeflections, and the PSys hierarchy frame the magic
// shield's dome relies on (the flagged ancestors' matrices, with their scale).

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <vector>

#include <PackFile.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/Implementations/VillagerShield.h"
#include "EngineConfig.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Magic/Objects/MapShield.h"
#include "Magic/Spells/SpellShield.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/Rules/Shield.h"
#include "support/BgfxShutdown.h"
#include "support/RestoreService.h"
#include "support/WorldSystems.h"

using namespace openblack;

namespace
{
GMagicShieldInfo ShieldInfo()
{
	GMagicShieldInfo info {};
	info.minRadius = 5.0f;
	info.maxRadius = 1000.0f;
	info.radiusForNormalCost = 30.0f;
	return info;
}

/// UR_AddDefensiveSphere on the root, its radius the magnitude x 1.11062 (as in SF_DefenseSphere)
constexpr std::string_view k_Sphere = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 0
PROPERTY Hierarchies ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY MaxSpellAge FLOAT -1
ENDPROPERTIES
BEGINCLASS MagnitudeFloatProvider MagnitudeFloatProvider0
BEGINPROPERTIES
PROPERTY Maximum FLOAT 1000
PROPERTY Minimum FLOAT 0
PROPERTY ScaleBy FLOAT 1.11062
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticlePointCreator ParticlePointCreator0
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleAnAtom CreateRuleAnAtom_Root
BEGINPROPERTIES
PROPERTY Group INTEGER 0
PROPERTY NextGroups ARRAY SIZE 0
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator0
ENDPROPERTIES
ENDCLASS
BEGINCLASS UR_AddDefensiveSphere UR_AddDefensiveSphere0
BEGINPROPERTIES
PROPERTY Group INTEGER 0
PROPERTY IsMagical BOOL 1
PROPERTY SphereRadius PERSIS_PNTR MagnitudeFloatProvider0
ENDPROPERTIES
ENDCLASS
)";

/// Three levels under a flagged root of scale 2: group 1 at (1, 0, 0) in the root's frame, group 2 made under it at
/// its parent's point + (0, 1, 0), in the same (root) frame
constexpr std::string_view k_Frames = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 0
PROPERTY Hierarchies ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY MaxSpellAge FLOAT -1
ENDPROPERTIES
BEGINCLASS ParticlePointCreator Root
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 2
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticlePointCreator Child
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleAnAtom CreateRuleAnAtom0
BEGINPROPERTIES
PROPERTY Group INTEGER 0
PROPERTY NextGroups ARRAY SIZE 1 1
PROPERTY PCreator PERSIS_PNTR Root
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleAnAtom CreateRuleAnAtom1
BEGINPROPERTIES
PROPERTY Group INTEGER 1
PROPERTY NextGroups ARRAY SIZE 1 2
PROPERTY OffsetX FLOAT 1
PROPERTY PCreator PERSIS_PNTR Child
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleAnAtom CreateRuleAnAtom2
BEGINPROPERTIES
PROPERTY Group INTEGER 2
PROPERTY NextGroups ARRAY SIZE 0
PROPERTY OffsetY FLOAT 1
PROPERTY PCreator PERSIS_PNTR Child
ENDPROPERTIES
ENDCLASS
)";

std::shared_ptr<const particles::File> Parse(std::string_view text, const char* name)
{
	auto file = particles::File::Parse(text, name);
	return file.has_value() ? std::make_shared<const particles::File>(*file) : nullptr;
}

/// One primitive of a mesh built here: a single triangle in one material
struct SyntheticPrimitive
{
	l3d::L3DMaterial::Type type {l3d::L3DMaterial::Type::Textured};
	uint32_t skin {0};
	uint8_t alphaRef {0};
	uint8_t flags {0}; ///< the material's flags: bit 0 two-sided, bit 2 wrap
};

/// One sub-mesh of a mesh built here
struct SyntheticSubMesh
{
	std::vector<SyntheticPrimitive> primitives;
	bool physics {false};
};

/// The bytes of an L3D file laid out as the game's are: the header, the sub-mesh table and headers, each sub-mesh's
/// primitive table, each primitive's header with its three vertices and one triangle, then the skins (blank texels)
std::vector<uint8_t> SyntheticL3d(const std::vector<SyntheticSubMesh>& subMeshes, const std::vector<uint32_t>& skins = {})
{
	constexpr uint32_t k_None = std::numeric_limits<uint32_t>::max();
	std::vector<uint8_t> file;
	const auto reserve = [&file](size_t bytes) {
		const auto at = static_cast<uint32_t>(file.size());
		file.resize(file.size() + bytes);
		return at;
	};
	const auto write = [&file](uint32_t at, const auto& value) { std::memcpy(file.data() + at, &value, sizeof(value)); };

	l3d::L3DHeader header {};
	std::memcpy(header.magic.data(), "L3D0", header.magic.size());
	header.submeshCount = static_cast<uint32_t>(subMeshes.size());
	header.skinCount = static_cast<uint32_t>(skins.size());
	header.skinOffsetsOffset = k_None;
	header.extraDataOffset = k_None;
	reserve(sizeof(header));
	header.submeshOffsetsOffset = reserve(subMeshes.size() * sizeof(uint32_t));
	std::vector<uint32_t> subMeshAt;
	for (size_t i = 0; i < subMeshes.size(); ++i)
	{
		subMeshAt.push_back(reserve(sizeof(l3d::L3DSubmeshHeader)));
		write(header.submeshOffsetsOffset + static_cast<uint32_t>(i * sizeof(uint32_t)), subMeshAt.back());
	}
	constexpr std::array<l3d::L3DVertex, 3> k_Vertices {{
	    {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
	    {{1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
	    {{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
	}};
	constexpr std::array<uint16_t, 4> k_Triangle {0, 1, 2, 0}; // padded to four bytes
	for (size_t i = 0; i < subMeshes.size(); ++i)
	{
		const auto& source = subMeshes[i];
		l3d::L3DSubmeshHeader subMesh {};
		subMesh.flags.isPhysics = source.physics ? 1u : 0u;
		subMesh.numPrimitives = static_cast<uint32_t>(source.primitives.size());
		subMesh.primitivesOffset = reserve(source.primitives.size() * sizeof(uint32_t));
		subMesh.bonesOffset = k_None;
		write(subMeshAt[i], subMesh);
		for (size_t j = 0; j < source.primitives.size(); ++j)
		{
			const auto& material = source.primitives[j];
			const auto at = reserve(sizeof(l3d::L3DPrimitiveHeader));
			write(subMesh.primitivesOffset + static_cast<uint32_t>(j * sizeof(uint32_t)), at);
			l3d::L3DPrimitiveHeader primitive {};
			primitive.material.type = material.type;
			primitive.material.alphaCutoutThreshold = material.alphaRef;
			primitive.material.cullMode = material.flags;
			primitive.material.skinID = material.skin;
			primitive.numVertices = static_cast<uint32_t>(k_Vertices.size());
			primitive.verticesOffset = reserve(sizeof(k_Vertices));
			write(primitive.verticesOffset, k_Vertices);
			primitive.numTriangles = 1;
			primitive.trianglesOffset = reserve(sizeof(k_Triangle));
			write(primitive.trianglesOffset, k_Triangle);
			primitive.groupsOffset = k_None;
			primitive.vertexBlendsOffset = k_None;
			write(at, primitive);
		}
	}
	if (!skins.empty())
	{
		header.skinOffsetsOffset = reserve(skins.size() * sizeof(uint32_t));
		const auto texture = std::make_unique<l3d::L3DTexture>(); // blank texels
		for (size_t i = 0; i < skins.size(); ++i)
		{
			texture->id = skins[i];
			const auto at = reserve(sizeof(l3d::L3DTexture));
			write(header.skinOffsetsOffset + static_cast<uint32_t>(i * sizeof(uint32_t)), at);
			write(at, *texture);
		}
	}
	header.size = static_cast<uint32_t>(file.size());
	write(0, header);
	return file;
}
} // namespace

TEST(Shield, radiusClamp)
{
	// max first, then min
	const auto info = ShieldInfo();
	EXPECT_FLOAT_EQ(magic::ClampShieldRadius(info, 3.0f), 5.0f);
	EXPECT_FLOAT_EQ(magic::ClampShieldRadius(info, 5.0f), 5.0f);
	EXPECT_FLOAT_EQ(magic::ClampShieldRadius(info, 40.0f), 40.0f);
	EXPECT_FLOAT_EQ(magic::ClampShieldRadius(info, 1000.0f), 1000.0f);
	EXPECT_FLOAT_EQ(magic::ClampShieldRadius(info, 2500.0f), 1000.0f);
}

TEST(Shield, upkeepGrowsWithTheSquareOfTheRadius)
{
	// costPerGameTurn x (r / radiusForNormalCost)^2
	EXPECT_FLOAT_EQ(magic::ShieldCostToMaintain(20.0f, 30.0f, 30.0f), 20.0f);
	EXPECT_FLOAT_EQ(magic::ShieldCostToMaintain(20.0f, 60.0f, 30.0f), 80.0f);
	EXPECT_NEAR(magic::ShieldCostToMaintain(22.0f, 40.0f, 30.0f), 22.0f * 16.0f / 9.0f, 1e-4f);
	EXPECT_FLOAT_EQ(magic::ShieldCostToMaintain(20.0f, 5.0f, 30.0f), 20.0f / 36.0f);
}

TEST(Shield, physicalShieldCurves)
{
	// the physical shield's grow and spin-down curves
	using magic::map_shield::CurvesAt;
	EXPECT_FLOAT_EQ(CurvesAt(0.0f).grow, 1.0f); // primed at full size, then shrunk while hidden
	EXPECT_FLOAT_EQ(CurvesAt(0.25f).grow, 0.5f);
	EXPECT_FALSE(CurvesAt(0.25f).spinning);
	EXPECT_FLOAT_EQ(CurvesAt(0.5f).grow, 0.0f);
	EXPECT_TRUE(CurvesAt(0.5f).spinning);
	EXPECT_FLOAT_EQ(CurvesAt(0.5f).spinDown, 0.0f);
	// x = 0.75 / 1.5 = 0.5: 0.5 + 0.25 - 0.125; y = 0.75 / 6 = 0.125
	EXPECT_NEAR(CurvesAt(1.25f).grow, 0.625f, 1e-6f);
	EXPECT_NEAR(CurvesAt(1.25f).spinDown, 0.125f + 0.015625f - 0.001953125f, 1e-6f);
	EXPECT_FLOAT_EQ(CurvesAt(2.0f).grow, 1.0f); // grown after 0.5 + 1.5 s
	EXPECT_LT(CurvesAt(6.0f).spinDown, 1.0f);
	EXPECT_FLOAT_EQ(CurvesAt(6.5f).spinDown, 1.0f); // spun down after 0.5 + 6 s
}

TEST(Shield, sphereHelpers)
{
	particles::shields::DefensiveSphere sphere;
	sphere.centre = glm::vec3(0.0f);
	sphere.radius = 10.0f;
	// strictly inside (r + margin)
	EXPECT_TRUE(particles::shields::IsPointInShield(sphere, glm::vec3(9.9f, 0.0f, 0.0f), 0.0f));
	EXPECT_FALSE(particles::shields::IsPointInShield(sphere, glm::vec3(10.0f, 0.0f, 0.0f), 0.0f));
	EXPECT_TRUE(particles::shields::IsPointInShield(sphere, glm::vec3(10.0f, 0.0f, 0.0f), 0.5f));
	EXPECT_TRUE(
	    particles::shields::HasCrossedIntoShield(sphere, glm::vec3(-20.0f, 0.0f, 0.0f), glm::vec3(-5.0f, 0.0f, 0.0f), 0.0f));
	EXPECT_FALSE(
	    particles::shields::HasCrossedIntoShield(sphere, glm::vec3(-5.0f, 0.0f, 0.0f), glm::vec3(-4.0f, 0.0f, 0.0f), 0.0f));
	// the first root along the move
	glm::vec3 hit;
	EXPECT_TRUE(
	    particles::shields::FindIntersect(sphere, glm::vec3(-20.0f, 0.0f, 0.0f), glm::vec3(-5.0f, 0.0f, 0.0f), 1.0f, hit));
	EXPECT_NEAR(hit.x, -11.0f, 1e-4f);
	EXPECT_FALSE(
	    particles::shields::FindIntersect(sphere, glm::vec3(-20.0f, 20.0f, 0.0f), glm::vec3(20.0f, 20.0f, 0.0f), 0.0f, hit));
	// v -= 2 (v.n) n
	glm::vec3 v(3.0f, -1.0f, 0.0f);
	particles::shields::DeflectOffShield(sphere, glm::vec3(-10.0f, 0.0f, 0.0f), v);
	EXPECT_NEAR(v.x, -3.0f, 1e-5f);
	EXPECT_NEAR(v.y, -1.0f, 1e-5f);
}

TEST(Shield, defensiveSphereRegistryAndDeflection)
{
	const auto file = Parse(k_Sphere, "SF_DefenseSphereTest");
	ASSERT_NE(file, nullptr);
	{
		particles::Effect effect(file, glm::vec3(100.0f, 20.0f, 200.0f), 40.0f);
		effect.Step(0.1f);
		// UR_AddDefensiveSphere: at the parent position (the effect's origin for the root collection)
		ASSERT_EQ(particles::shields::All().size(), 1u);
		const auto sphere = particles::shields::All().front();
		EXPECT_EQ(sphere.owner, &effect);
		EXPECT_NEAR(sphere.radius, 40.0f * 1.11062f, 1e-3f);
		EXPECT_EQ(sphere.centre, glm::vec3(100.0f, 20.0f, 200.0f));
		// the radius follows the provider, the centre stays
		effect.SetMagnitude(20.0f);
		effect.SetOrigin(glm::vec3(0.0f));
		effect.Step(0.1f);
		EXPECT_NEAR(particles::shields::All().front().radius, 20.0f * 1.11062f, 1e-3f);
		EXPECT_EQ(particles::shields::All().front().centre, glm::vec3(100.0f, 20.0f, 200.0f));
		// an atom (scale 1: margin 1.25) that moved in from the outside is put on the
		// sphere and reflected (no spell to let it through: the event answers 0), with a spark target for the shield
		particles::Atom atom;
		atom.position = glm::vec3(100.0f, 20.0f, 200.0f + 10.0f);
		atom.velocity = glm::vec3(0.0f, 0.0f, -30.0f);
		const glm::vec3 old(100.0f, 20.0f, 250.0f);
		const size_t sparks = effect.TargetPointCount();
		EXPECT_TRUE(particles::shields::DoAnyShieldDeflections(effect, atom, old));
		EXPECT_NEAR(atom.position.z, 200.0f + 20.0f * 1.11062f + 1.25f, 1e-3f);
		EXPECT_NEAR(atom.velocity.z, 30.0f, 1e-4f);
		EXPECT_EQ(effect.TargetPointCount(), sparks + 1);
		// already inside: nothing
		EXPECT_FALSE(particles::shields::DoAnyShieldDeflections(effect, atom, atom.position));
	}
	// the sphere goes with its effect
	EXPECT_TRUE(particles::shields::All().empty());
}

TEST(Shield, hierarchyFrameIsTheFlaggedAncestorsWithTheirScale)
{
	// a collection is hierarchical under any flagged ancestor, a new atom starts at the parent's point unless the parent
	// is flagged, and the frame is the flagged atoms' rotation x scale
	const auto file = Parse(k_Frames, "SF_FramesTest");
	ASSERT_NE(file, nullptr);
	particles::Effect effect(file, glm::vec3(10.0f, 0.0f, 0.0f), 1.0f);
	effect.Step(0.1f);
	effect.Step(0.1f);
	std::vector<particles::Effect::DrawAtom> atoms;
	effect.Collect(1.0f, atoms, particles::Creator::Kind::Point);
	ASSERT_EQ(atoms.size(), 3u);
	// the root at the origin, scale 2; group 1 at 2 x (1, 0, 0) from it; group 2 at 2 x ((1, 0, 0) + (0, 1, 0))
	std::vector<glm::vec3> positions;
	for (const auto& atom : atoms)
	{
		positions.push_back(atom.position);
	}
	const auto has = [&positions](const glm::vec3& p) {
		for (const auto& q : positions)
		{
			if (glm::length(q - p) < 1e-4f)
			{
				return true;
			}
		}
		return false;
	};
	EXPECT_TRUE(has(glm::vec3(10.0f, 0.0f, 0.0f)));
	EXPECT_TRUE(has(glm::vec3(12.0f, 0.0f, 0.0f)));
	EXPECT_TRUE(has(glm::vec3(12.0f, 2.0f, 0.0f)));
}

/// With OPENBLACK_GAME_PATH set to the install: the real shield rows
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(Shield, realInfoDat)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	std::ifstream in(std::filesystem::path(game) / "Scripts" / "info.dat", std::ios::binary);
	ASSERT_TRUE(in.is_open());
	const std::vector<char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	ASSERT_EQ(data.size(), 0x2C + sizeof(InfoConstants));
	auto info = std::make_unique<InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(InfoConstants));
	const auto* shield = magic::GetMagicInfoAs<GMagicShieldInfo>(*info, MagicType::Shield);
	const auto* physical = magic::GetMagicInfoAs<GMagicShieldInfo>(*info, MagicType::PhysicalShield);
	ASSERT_NE(shield, nullptr);
	ASSERT_NE(physical, nullptr);
	EXPECT_EQ(shield->particleType, ParticleType::Shield);
	EXPECT_FLOAT_EQ(shield->minRadius, 5.0f);
	EXPECT_FLOAT_EQ(shield->maxRadius, 1000.0f);
	EXPECT_FLOAT_EQ(shield->radiusForNormalCost, 30.0f);
	EXPECT_FLOAT_EQ(physical->chantCostPerImpactMomentum, 25.0f);
	EXPECT_FLOAT_EQ(physical->shieldHeight, 0.0f);
	EXPECT_FLOAT_EQ(physical->raiseWithScale, -2.0f);
	EXPECT_FLOAT_EQ(physical->bobMagnitude, 3.0f);
	EXPECT_FLOAT_EQ(magic::GetMagicEffectInfo(*info, MagicType::Shield).costPerGameTurn, 20.0f);
	EXPECT_FLOAT_EQ(magic::GetMagicEffectInfo(*info, MagicType::PhysicalShield).costPerGameTurn, 22.0f);
	EXPECT_FLOAT_EQ(magic::GetTimerWhenPlayerCasting(*info, MagicType::Shield), -1.0f);
	// the map shield weight (x scale^3: the physical shield's body mass)
	EXPECT_FLOAT_EQ(info->mapShield[1].weight, 50000.0f);
	// 40 m: 20 x (40 / 30)^2 = 35.6 chants a turn
	EXPECT_NEAR(magic::ShieldCostToMaintain(20.0f, 40.0f, shield->radiusForNormalCost), 35.556f, 1e-3f);
}

/// realInfoDat on an info table built here: the shield and physical shield rows are found by their magic type, their
/// effect rows give the upkeep and the timer, and the upkeep grows with the square of the radius over the row's
/// radiusForNormalCost. The map shield's weight is the game data's own, so only the original test checks it
TEST(Shield, realInfoDatSynthetic)
{
	auto info = std::make_unique<InfoConstants>(); // every row zeroed
	auto& shieldRow = info->magicShield[0];
	shieldRow.particleType = ParticleType::Shield;
	shieldRow.minRadius = 2.0f;
	shieldRow.maxRadius = 500.0f;
	shieldRow.radiusForNormalCost = 25.0f;
	auto& physicalRow = info->magicShield[1];
	physicalRow.chantCostPerImpactMomentum = 10.0f;
	physicalRow.shieldHeight = 1.0f;
	physicalRow.raiseWithScale = -1.0f;
	physicalRow.bobMagnitude = 2.0f;
	info->magicEffect.at(static_cast<size_t>(MagicType::Shield)).costPerGameTurn = 12.0f;
	info->magicEffect.at(static_cast<size_t>(MagicType::Shield)).timerWhenPlayerCasting = -1.0f;
	info->magicEffect.at(static_cast<size_t>(MagicType::PhysicalShield)).costPerGameTurn = 15.0f;

	const auto* shield = magic::GetMagicInfoAs<GMagicShieldInfo>(*info, MagicType::Shield);
	const auto* physical = magic::GetMagicInfoAs<GMagicShieldInfo>(*info, MagicType::PhysicalShield);
	ASSERT_EQ(shield, &shieldRow);
	ASSERT_EQ(physical, &physicalRow);
	EXPECT_EQ(shield->particleType, ParticleType::Shield);
	EXPECT_FLOAT_EQ(shield->minRadius, 2.0f);
	EXPECT_FLOAT_EQ(shield->maxRadius, 500.0f);
	EXPECT_FLOAT_EQ(shield->radiusForNormalCost, 25.0f);
	EXPECT_FLOAT_EQ(physical->chantCostPerImpactMomentum, 10.0f);
	EXPECT_FLOAT_EQ(physical->shieldHeight, 1.0f);
	EXPECT_FLOAT_EQ(physical->raiseWithScale, -1.0f);
	EXPECT_FLOAT_EQ(physical->bobMagnitude, 2.0f);
	EXPECT_FLOAT_EQ(magic::GetMagicEffectInfo(*info, MagicType::Shield).costPerGameTurn, 12.0f);
	EXPECT_FLOAT_EQ(magic::GetMagicEffectInfo(*info, MagicType::PhysicalShield).costPerGameTurn, 15.0f);
	EXPECT_FLOAT_EQ(magic::GetTimerWhenPlayerCasting(*info, MagicType::Shield), -1.0f);
	// 50 m: 12 x (50 / 25)^2 = 48 chants a turn
	const float upkeep = magic::GetMagicEffectInfo(*info, MagicType::Shield).costPerGameTurn;
	EXPECT_FLOAT_EQ(magic::ShieldCostToMaintain(upkeep, 50.0f, shield->radiusForNormalCost), 48.0f);
	// the radius is clamped to the row's own limits
	EXPECT_FLOAT_EQ(magic::ClampShieldRadius(*shield, 1.0f), 2.0f);
	EXPECT_FLOAT_EQ(magic::ClampShieldRadius(*shield, 900.0f), 500.0f);
}

/// With OPENBLACK_GAME_PATH set to the install: creating a physical shield replaces the material types (5, 13)
/// then (4, 13) on MSH_S_SOLID_SHIELD (AllMeshes.g3d mesh 554): the inner layer (sub-mesh 1,
/// AlphaTextured) becomes additive without Z write, the outer one (sub-mesh 0, TexturedChroma, ALPHAREF 200) stays
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(Shield, physicalShieldMaterialTypes)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	for (const auto* name : {"game", "graphics"})
	{
		if (spdlog::get(name) == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>(name);
		}
	}
	// the config is the test's only when it had none: then it goes with the test
	const test::RestoreService<Locator::config> config;
	if (!Locator::config::has_value())
	{
		Locator::config::emplace();
	}
	pack::PackFile pack;
	ASSERT_EQ(pack.Open(std::filesystem::path(game) / "Data" / "AllMeshes.g3d"), pack::PackResult::Success);
	// the sub-meshes and skins build bgfx buffers and textures
	bgfx::renderFrame(); // single-threaded
	bgfx::Init init {};
	init.type = bgfx::RendererType::Noop;
	ASSERT_TRUE(bgfx::init(init));
	const test::BgfxShutdown bgfxShutdown;
	{
		using Type = l3d::L3DMaterial::Type;
		using Blend = graphics::L3DSubMesh::Primitive::BlendMode;
		graphics::L3DMesh mesh("MSH_S_SOLID_SHIELD");
		ASSERT_TRUE(mesh.LoadFromBuffer(pack.GetMesh(static_cast<uint32_t>(magic::map_shield::k_Mesh))));
		ASSERT_EQ(mesh.GetNumSubMeshes(), 3);
		const auto& outer = mesh.GetSubMeshes()[0]->GetPrimitives();
		const auto& inner = mesh.GetSubMeshes()[1]->GetPrimitives();
		ASSERT_EQ(outer.size(), 1);
		ASSERT_EQ(inner.size(), 1);
		// the replacement walks every sub-mesh, the physics one (2) included: no primitive of the mesh is of type 5, and only
		// the inner layer is of type 4 (checked in the audit, so that the (5, 13) call really is a no-op)
		for (uint8_t i = 0; i < mesh.GetNumSubMeshes(); ++i)
		{
			for (const auto& primitive : mesh.GetSubMeshes()[i]->GetPrimitives())
			{
				EXPECT_NE(primitive.materialType, 5u) << "sub-mesh " << static_cast<int>(i);
				EXPECT_TRUE(primitive.materialType != 4u || i == 1) << "sub-mesh " << static_cast<int>(i);
			}
		}
		// as loaded: inner AlphaTextured (SA / ISA, writes Z), outer TexturedChroma with ALPHAREF 200
		EXPECT_EQ(inner[0].materialType, static_cast<uint32_t>(Type::AlphaTextured));
		EXPECT_EQ(inner[0].blend, Blend::Standard);
		EXPECT_TRUE(inner[0].depthWrite);

		mesh.ReplaceMaterialType(5, 13); // no primitive of type 5
		EXPECT_EQ(inner[0].materialType, static_cast<uint32_t>(Type::AlphaTextured));
		mesh.ReplaceMaterialType(4, 13);

		// mode 13: SRCALPHA / ONE, no alpha test, no Z write; the other render flags stay (5: two-sided, wrap)
		EXPECT_EQ(inner[0].materialType, static_cast<uint32_t>(Type::AlphaTexturedAlphaAdditiveNz));
		EXPECT_EQ(inner[0].blend, Blend::Additive);
		EXPECT_FALSE(inner[0].depthWrite);
		EXPECT_FALSE(inner[0].alphaTest);
		EXPECT_FALSE(inner[0].thresholdAlpha);
		EXPECT_TRUE(inner[0].twoSided);
		EXPECT_TRUE(inner[0].wrap);
		// the outer layer is untouched: type 9, alpha test at 200 / 255, writes Z, two-sided
		EXPECT_EQ(outer[0].materialType, static_cast<uint32_t>(Type::TexturedChroma));
		EXPECT_EQ(outer[0].blend, Blend::Standard);
		EXPECT_TRUE(outer[0].depthWrite);
		EXPECT_TRUE(outer[0].thresholdAlpha);
		EXPECT_FLOAT_EQ(outer[0].alphaCutoutThreshold, 200.0f / 255.0f);
		EXPECT_TRUE(outer[0].twoSided);

		// idempotent, as the original's second shield on the shared mesh
		mesh.ReplaceMaterialType(4, 13);
		EXPECT_EQ(inner[0].materialType, static_cast<uint32_t>(Type::AlphaTexturedAlphaAdditiveNz));
	}
}

/// physicalShieldMaterialTypes on a mesh built here with the solid shield's layers: an outer layer of type 9 with an
/// alpha reference of 200 and an inner layer of type 4, both two-sided, the inner one tiled, and a physics sub-mesh of
/// type 4 too. Replacing 5 by 13 changes nothing; replacing 4 by 13 makes every type 4 primitive, in every sub-mesh,
/// additive without Z write, keeps the other material flags, and leaves the outer layer alone
TEST(Shield, physicalShieldMaterialTypesSynthetic)
{
	using Type = l3d::L3DMaterial::Type;
	using Blend = graphics::L3DSubMesh::Primitive::BlendMode;
	constexpr uint8_t k_TwoSided = 1;
	constexpr uint8_t k_Wrap = 4;
	constexpr uint8_t k_AlphaRef = 200;
	const auto bytes = SyntheticL3d({
	    {.primitives = {{.type = Type::TexturedChroma, .alphaRef = k_AlphaRef, .flags = k_TwoSided}}},
	    {.primitives = {{.type = Type::AlphaTextured, .flags = k_TwoSided | k_Wrap}}},
	    {.primitives = {{.type = Type::AlphaTextured}}, .physics = true},
	});
	for (const auto* name : {"game", "graphics"})
	{
		if (spdlog::get(name) == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>(name);
		}
	}
	// the config is the test's only when it had none: then it goes with the test
	const test::RestoreService<Locator::config> config;
	if (!Locator::config::has_value())
	{
		Locator::config::emplace();
	}
	// the sub-meshes build bgfx buffers
	bgfx::renderFrame(); // single-threaded
	bgfx::Init init {};
	init.type = bgfx::RendererType::Noop;
	ASSERT_TRUE(bgfx::init(init));
	const test::BgfxShutdown bgfxShutdown;
	{
		graphics::L3DMesh mesh("synthetic shield");
		ASSERT_TRUE(mesh.LoadFromBuffer(bytes));
		ASSERT_EQ(mesh.GetNumSubMeshes(), 3);
		const auto& outer = mesh.GetSubMeshes()[0]->GetPrimitives();
		const auto& inner = mesh.GetSubMeshes()[1]->GetPrimitives();
		const auto& physics = mesh.GetSubMeshes()[2]->GetPrimitives();
		ASSERT_EQ(outer.size(), 1);
		ASSERT_EQ(inner.size(), 1);
		ASSERT_EQ(physics.size(), 1);
		EXPECT_TRUE(mesh.GetSubMeshes()[2]->IsPhysics());
		// as loaded: inner AlphaTextured (writes Z), outer TexturedChroma
		EXPECT_EQ(inner[0].materialType, static_cast<uint32_t>(Type::AlphaTextured));
		EXPECT_EQ(inner[0].blend, Blend::Standard);
		EXPECT_TRUE(inner[0].depthWrite);

		mesh.ReplaceMaterialType(5, 13); // no primitive of type 5
		EXPECT_EQ(inner[0].materialType, static_cast<uint32_t>(Type::AlphaTextured));
		EXPECT_EQ(physics[0].materialType, static_cast<uint32_t>(Type::AlphaTextured));
		mesh.ReplaceMaterialType(4, 13);

		// mode 13: SRCALPHA / ONE, no alpha test, no Z write; the material flags stay
		EXPECT_EQ(inner[0].materialType, static_cast<uint32_t>(Type::AlphaTexturedAlphaAdditiveNz));
		EXPECT_EQ(inner[0].blend, Blend::Additive);
		EXPECT_FALSE(inner[0].depthWrite);
		EXPECT_FALSE(inner[0].alphaTest);
		EXPECT_FALSE(inner[0].thresholdAlpha);
		EXPECT_TRUE(inner[0].twoSided);
		EXPECT_TRUE(inner[0].wrap);
		// the physics sub-mesh is walked too, and keeps its own flags
		EXPECT_EQ(physics[0].materialType, static_cast<uint32_t>(Type::AlphaTexturedAlphaAdditiveNz));
		EXPECT_FALSE(physics[0].twoSided);
		EXPECT_FALSE(physics[0].wrap);
		// the outer layer is untouched: type 9, alpha test at 200 / 255, writes Z, two-sided
		EXPECT_EQ(outer[0].materialType, static_cast<uint32_t>(Type::TexturedChroma));
		EXPECT_EQ(outer[0].blend, Blend::Standard);
		EXPECT_TRUE(outer[0].depthWrite);
		EXPECT_TRUE(outer[0].thresholdAlpha);
		EXPECT_FLOAT_EQ(outer[0].alphaCutoutThreshold, 200.0f / 255.0f);
		EXPECT_TRUE(outer[0].twoSided);

		// idempotent, as a second shield on the shared mesh
		mesh.ReplaceMaterialType(4, 13);
		EXPECT_EQ(inner[0].materialType, static_cast<uint32_t>(Type::AlphaTexturedAlphaAdditiveNz));
	}
}

/// ReactToMagicShieldPriority (needs the real info.dat for reaction 13) and
/// CreatureMustAvoid
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(Shield, villagerReactionPriorityAndCreatureMustAvoid)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	std::ifstream in(std::filesystem::path(game) / "Scripts" / "info.dat", std::ios::binary);
	ASSERT_TRUE(in.is_open());
	const std::vector<char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	ASSERT_EQ(data.size(), 0x2C + sizeof(InfoConstants));
	auto info = std::make_unique<InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(InfoConstants));
	Locator::infoConstants::reset(info.release());
	Locator::entitiesRegistry::emplace<ecs::Registry>();
	openblack::test::EmplaceWorldSystems();
	ecs::effects::reactions::Clear();
	auto& registry = Locator::entitiesRegistry::value();

	// a PHYSICAL_SHIELD spell of player one at (100, 100), r 40: the initiator of REACTION 13. CreateReaction spreads at
	// once, which does nothing here (no entities map)
	const auto spell = registry.Create();
	{
		auto& component = registry.Assign<ecs::components::Spell>(spell);
		component.spellClass = ecs::components::SpellClass::Shield;
		component.magicType = MagicType::PhysicalShield;
		component.position = glm::vec3(100.0f, 0.0f, 100.0f);
		component.castPos = component.position;
		component.magnitude = 40.0f;
		component.player = PlayerNames::PLAYER_ONE;
		component.hasPlayer = true;
	}
	const auto reaction =
	    ecs::effects::reactions::CreateReaction(spell, Reaction::ReactToMagicShield, PlayerNames::PLAYER_ONE, false);
	ASSERT_NE(reaction, 0u);

	// a homeless villager: no town -> the priority of the reaction row
	const auto villager = registry.Create();
	{
		auto& component = registry.Assign<ecs::components::Villager>(villager);
		component.town = entt::null;
	}
	const auto expected = static_cast<uint8_t>(
	    Locator::infoConstants::value().reaction.at(static_cast<size_t>(Reaction::ReactToMagicShield)).priority & 0xFFu);
	EXPECT_GT(expected, 0);
	EXPECT_EQ(ecs::villager_shield::ReactToMagicShieldPriority(villager, reaction), expected);
	// an unknown reaction, and one whose initiator is no shield spell: 0
	EXPECT_EQ(ecs::villager_shield::ReactToMagicShieldPriority(villager, 0), 0);
	const auto other = registry.Create();
	{
		auto& component = registry.Assign<ecs::components::Spell>(other);
		component.spellClass = ecs::components::SpellClass::General;
		component.magicType = MagicType::LightningBolt;
	}
	const auto otherReaction =
	    ecs::effects::reactions::CreateReaction(other, Reaction::ReactToMagicShield, PlayerNames::PLAYER_ONE, false);
	EXPECT_EQ(ecs::villager_shield::ReactToMagicShieldPriority(villager, otherReaction), 0);
	// with a town, the desire for protection is 0 in openblack (not ported): no reaction
	const auto town = registry.Create();
	registry.Assign<ecs::components::Town>(town);
	registry.Get<ecs::components::Villager>(villager).town = town;
	EXPECT_EQ(ecs::villager_shield::ReactToMagicShieldPriority(villager, reaction), 0);

	// CreatureMustAvoid: the shield of player one
	const auto shield = registry.Create();
	{
		auto& component = registry.Assign<ecs::components::MagicShield>(shield);
		component.kind = ecs::components::MagicShield::Kind::Physical;
		component.spell = spell;
	}
	const auto creature = registry.Create();
	// another player's creature avoids it; its own player's does not; without a creature or a player, 0
	EXPECT_TRUE(magic::map_shield::CreatureMustAvoid(shield, creature, PlayerNames::PLAYER_TWO));
	EXPECT_FALSE(magic::map_shield::CreatureMustAvoid(shield, creature, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(magic::map_shield::CreatureMustAvoid(shield, creature, std::nullopt));
	EXPECT_FALSE(magic::map_shield::CreatureMustAvoid(shield, entt::null, PlayerNames::PLAYER_TWO));
	// a shield whose spell is gone belongs to the interface's player (PLAYER_ONE), not NULL
	registry.Get<ecs::components::MagicShield>(shield).spell = entt::null;
	EXPECT_FALSE(magic::map_shield::CreatureMustAvoid(shield, creature, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(magic::map_shield::CreatureMustAvoid(shield, creature, PlayerNames::PLAYER_TWO));
	EXPECT_TRUE(magic::map_shield::CreatureMustAvoid(shield, creature, std::nullopt));
	registry.Get<ecs::components::MagicShield>(shield).spell = spell;
	// controlled by a script: never
	ecs::script_held::SetControlledByScript(creature, true);
	EXPECT_FALSE(magic::map_shield::CreatureMustAvoid(shield, creature, PlayerNames::PLAYER_TWO));

	ecs::effects::reactions::Clear();
	Locator::entitiesRegistry::reset();
	openblack::test::ResetWorldSystems();
	Locator::infoConstants::reset();
}

/// villagerReactionPriorityAndCreatureMustAvoid on a zeroed info table whose only set value is the shield reaction's
/// priority. It has bits above the low byte, so the test also shows that only the low byte is the priority
TEST(Shield, villagerReactionPriorityAndCreatureMustAvoidSynthetic)
{
	constexpr uint32_t k_Priority = 0x1234u;
	constexpr uint8_t k_ExpectedPriority = 0x34u; // the low byte
	{
		auto info = std::make_unique<InfoConstants>();
		info->reaction.at(static_cast<size_t>(Reaction::ReactToMagicShield)).priority = k_Priority;
		Locator::infoConstants::reset(info.release());
	}
	Locator::entitiesRegistry::emplace<ecs::Registry>();
	openblack::test::EmplaceWorldSystems();
	ecs::effects::reactions::Clear();
	auto& registry = Locator::entitiesRegistry::value();

	// a physical shield spell of player one at (100, 100), radius 40, and the shield reaction it starts
	const auto spell = registry.Create();
	{
		auto& component = registry.Assign<ecs::components::Spell>(spell);
		component.spellClass = ecs::components::SpellClass::Shield;
		component.magicType = MagicType::PhysicalShield;
		component.position = glm::vec3(100.0f, 0.0f, 100.0f);
		component.castPos = component.position;
		component.magnitude = 40.0f;
		component.player = PlayerNames::PLAYER_ONE;
		component.hasPlayer = true;
	}
	const auto reaction =
	    ecs::effects::reactions::CreateReaction(spell, Reaction::ReactToMagicShield, PlayerNames::PLAYER_ONE, false);
	ASSERT_NE(reaction, 0u);

	// a homeless villager: the low byte of the reaction's priority
	const auto villager = registry.Create();
	{
		auto& component = registry.Assign<ecs::components::Villager>(villager);
		component.town = entt::null;
	}
	EXPECT_EQ(ecs::villager_shield::ReactToMagicShieldPriority(villager, reaction), k_ExpectedPriority);
	// an unknown reaction, and one started by a spell that is not a shield: 0
	EXPECT_EQ(ecs::villager_shield::ReactToMagicShieldPriority(villager, 0), 0);
	const auto other = registry.Create();
	{
		auto& component = registry.Assign<ecs::components::Spell>(other);
		component.spellClass = ecs::components::SpellClass::General;
		component.magicType = MagicType::LightningBolt;
	}
	const auto otherReaction =
	    ecs::effects::reactions::CreateReaction(other, Reaction::ReactToMagicShield, PlayerNames::PLAYER_ONE, false);
	EXPECT_EQ(ecs::villager_shield::ReactToMagicShieldPriority(villager, otherReaction), 0);
	// a villager with a town: the town has no desire for protection, so no reaction
	const auto town = registry.Create();
	registry.Assign<ecs::components::Town>(town);
	registry.Get<ecs::components::Villager>(villager).town = town;
	EXPECT_EQ(ecs::villager_shield::ReactToMagicShieldPriority(villager, reaction), 0);

	// CreatureMustAvoid: the shield of player one
	const auto shield = registry.Create();
	{
		auto& component = registry.Assign<ecs::components::MagicShield>(shield);
		component.kind = ecs::components::MagicShield::Kind::Physical;
		component.spell = spell;
	}
	const auto creature = registry.Create();
	// another player's creature avoids it, its own player's does not; with no player it avoids it, with no creature not
	EXPECT_TRUE(magic::map_shield::CreatureMustAvoid(shield, creature, PlayerNames::PLAYER_TWO));
	EXPECT_FALSE(magic::map_shield::CreatureMustAvoid(shield, creature, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(magic::map_shield::CreatureMustAvoid(shield, creature, std::nullopt));
	EXPECT_FALSE(magic::map_shield::CreatureMustAvoid(shield, entt::null, PlayerNames::PLAYER_TWO));
	// a shield whose spell is gone belongs to the interface's player (player one)
	registry.Get<ecs::components::MagicShield>(shield).spell = entt::null;
	EXPECT_FALSE(magic::map_shield::CreatureMustAvoid(shield, creature, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(magic::map_shield::CreatureMustAvoid(shield, creature, PlayerNames::PLAYER_TWO));
	EXPECT_TRUE(magic::map_shield::CreatureMustAvoid(shield, creature, std::nullopt));
	registry.Get<ecs::components::MagicShield>(shield).spell = spell;
	// a creature controlled by a script never avoids it
	ecs::script_held::SetControlledByScript(creature, true);
	EXPECT_FALSE(magic::map_shield::CreatureMustAvoid(shield, creature, PlayerNames::PLAYER_TWO));

	ecs::effects::reactions::Clear();
	Locator::entitiesRegistry::reset();
	openblack::test::ResetWorldSystems();
	Locator::infoConstants::reset();
}
