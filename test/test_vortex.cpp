/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The vortex's fade and land curves (docs/bw1-notes/vortex.md): FadeValue, LandFactorValue and LandOffset with the
// retail spline (every y 0), and the drawing's curves: the light map's strength, the ground effect's height, the decal's
// alpha reference and its change detector. The original computes them in float steps (its FPU runs at 24 bits); the
// bit patterns below are those float steps. And a deleted vortex's particle systems: ecs::ToBeDeleted, at once or
// deferred, deletes them from the particle manager (fake effects from a synthetic spell file). And what the renderer
// draws for a vortex (Graphics/VortexDraw.h): the depth box's placement, the decal's block, texture coordinates and
// textures, which effects are drawn before the land or not at all, and the solid funnel's material mode.

#define LOCATOR_IMPLEMENTATIONS

#include <array>
#include <bit>
#include <memory>
#include <string>
#include <string_view>

#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Unavailable.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Vortex.h"
#include "Graphics/RenderModes.h"
#include "Graphics/VortexDraw.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysManager.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using openblack::VortexStateType;
using openblack::ecs::components::VortexDecal;
namespace vortex = openblack::ecs::vortex;
namespace vortex_draw = openblack::graphics::vortex_draw;

TEST(Vortex, Fade)
{
	EXPECT_EQ(vortex::FadeValue(VortexStateType::Inactive, 3.0f), 0.0f);
	EXPECT_EQ(vortex::FadeValue(VortexStateType::Active, 0.0f), 1.0f);
	// FadeIn: 0 for 2 s, then smooth((e - 2) / 5)
	EXPECT_EQ(vortex::FadeValue(VortexStateType::FadeIn, 1.9f), 0.0f);
	EXPECT_EQ(vortex::FadeValue(VortexStateType::FadeIn, 2.0f), 0.0f);
	EXPECT_EQ(vortex::FadeValue(VortexStateType::FadeIn, 4.5f), 0.5f); // (3 - 1) 0.5 0.5
	EXPECT_EQ(vortex::FadeValue(VortexStateType::FadeIn, 7.0f), 1.0f);
	EXPECT_EQ(vortex::FadeValue(VortexStateType::FadeIn, 20.0f), 1.0f);
	// FadeOut: smooth(1 - e / 5) for 5 s, then 0
	EXPECT_EQ(vortex::FadeValue(VortexStateType::FadeOut, 0.0f), 1.0f);
	EXPECT_EQ(vortex::FadeValue(VortexStateType::FadeOut, 2.5f), 0.5f);
	EXPECT_EQ(vortex::FadeValue(VortexStateType::FadeOut, 5.0f), 0.0f);
}

TEST(Vortex, LandFactor)
{
	EXPECT_EQ(vortex::LandFactorValue(VortexStateType::FadeIn, 4.5f), 0.75f); // 1 - (1 - 0.5)^2
	EXPECT_EQ(vortex::LandFactorValue(VortexStateType::FadeIn, 1.0f), 0.0f);
	EXPECT_EQ(vortex::LandFactorValue(VortexStateType::Active, 0.0f), 1.0f);
	EXPECT_EQ(vortex::LandFactorValue(VortexStateType::FadeOut, 4.9f), 1.0f); // FadeOut takes s = 1
}

TEST(Vortex, LandOffset)
{
	// within 50 m: (mean - alt0) q
	EXPECT_EQ(vortex::LandOffset({30.0f, 0.0f}, 10.0f, 20.0f, 0.75f), 7.5f);
	EXPECT_EQ(vortex::LandOffset({0.0f, 0.0f}, 30.0f, 20.0f, 1.0f), -10.0f);
	// 50..56 m: (56 - r)(mean - alt0) / 6 q
	EXPECT_EQ(vortex::LandOffset({0.0f, 53.0f}, 10.0f, 22.0f, 0.5f), 3.0f); // 3 x 12 / 6 x 0.5
	EXPECT_EQ(vortex::LandOffset({56.0f, 0.0f}, 10.0f, 22.0f, 1.0f), 0.0f);
	// beyond 56 m: 0
	EXPECT_EQ(vortex::LandOffset({40.0f, 50.0f}, 10.0f, 22.0f, 1.0f), 0.0f);
}

TEST(Vortex, LightMapAlpha)
{
	EXPECT_EQ(vortex::LightMapAlphaValue(VortexStateType::Inactive, 3.0f), 0.0f);
	EXPECT_EQ(vortex::LightMapAlphaValue(VortexStateType::Active, 3.0f), 0.6f);
	// FadeIn: e / 2 for 2 s, then 1 + (0.6 - 1) c, c = (e - 2) / 5 kept to 0..1
	EXPECT_EQ(vortex::LightMapAlphaValue(VortexStateType::FadeIn, 1.0f), 0.5f);
	EXPECT_EQ(vortex::LightMapAlphaValue(VortexStateType::FadeIn, 2.0f), 1.0f);
	EXPECT_EQ(vortex::LightMapAlphaValue(VortexStateType::FadeIn, 3.3f), std::bit_cast<float>(0x3F656042u)); // 0.896
	EXPECT_EQ(vortex::LightMapAlphaValue(VortexStateType::FadeIn, 4.5f), std::bit_cast<float>(0x3F4CCCCDu)); // 0.8
	EXPECT_EQ(vortex::LightMapAlphaValue(VortexStateType::FadeIn, 7.0f), 0.6f);
	EXPECT_EQ(vortex::LightMapAlphaValue(VortexStateType::FadeIn, 20.0f), 0.6f);
	// FadeOut: 0.6 + (1 - 0.6) e / 5 for 5 s, then 1 - (e - 5) / 2 kept to 0..1
	EXPECT_EQ(vortex::LightMapAlphaValue(VortexStateType::FadeOut, 0.0f), 0.6f);
	EXPECT_EQ(vortex::LightMapAlphaValue(VortexStateType::FadeOut, 3.3f), std::bit_cast<float>(0x3F5D2F1Au)); // 0.864
	EXPECT_EQ(vortex::LightMapAlphaValue(VortexStateType::FadeOut, 5.0f), 1.0f);
	EXPECT_EQ(vortex::LightMapAlphaValue(VortexStateType::FadeOut, 6.0f), 0.5f);
	EXPECT_EQ(vortex::LightMapAlphaValue(VortexStateType::FadeOut, 7.0f), 0.0f);
	EXPECT_EQ(vortex::LightMapAlphaValue(VortexStateType::FadeOut, 9.0f), 0.0f);
	// as the effect's alpha: x 255, truncated
	EXPECT_EQ(vortex::AlphaByte(0.6f), 153);
	EXPECT_EQ(vortex::AlphaByte(std::bit_cast<float>(0x3F656042u)), 228);
	EXPECT_EQ(vortex::AlphaByte(1.0f), 255);
	EXPECT_EQ(vortex::AlphaByte(0.0f), 0);
}

TEST(Vortex, PreLandscapeHeight)
{
	// ground - ((0.3 - 2.5) q + 2.5): 2.5 m under the land at q = 0, 0.3 m under it at q = 1
	EXPECT_EQ(vortex::PreLandscapeHeight(10.0f, 0.0f), 7.5f);
	EXPECT_EQ(vortex::PreLandscapeHeight(10.0f, 1.0f), std::bit_cast<float>(0x411B3333u));   // 9.7
	EXPECT_EQ(vortex::PreLandscapeHeight(37.25f, 0.75f), std::bit_cast<float>(0x4211999Au)); // 36.4
	// in float steps; in double this one would round to 0xBFEB851F
	EXPECT_EQ(vortex::PreLandscapeHeight(0.0f, 0.3f), std::bit_cast<float>(0xBFEB851Eu));
}

TEST(Vortex, DecalAlphaRef)
{
	EXPECT_EQ(vortex::DecalAlphaRef(0.0f), 0);
	EXPECT_EQ(vortex::DecalAlphaRef(0.3f), 76);
	EXPECT_EQ(vortex::DecalAlphaRef(0.5f), 127);
	EXPECT_EQ(vortex::DecalAlphaRef(0.92f), 234);
	// capped at 235
	EXPECT_EQ(vortex::DecalAlphaRef(0.9216f), 235);
	EXPECT_EQ(vortex::DecalAlphaRef(1.0f), 235);
}

TEST(Vortex, DecalChanges)
{
	// a new vortex's first frame always counts as a change, even at f = 0
	VortexDecal decal;
	decal = vortex::NextDecal(decal, 0.0f);
	EXPECT_EQ(decal.lastFade, 0.0f);
	EXPECT_FALSE(decal.made);
	// made once f is not 0, its reference following f
	decal = vortex::NextDecal(decal, 0.5f);
	EXPECT_TRUE(decal.made);
	EXPECT_EQ(decal.alphaRef, 127);
	// the same f: nothing changes
	auto same = decal;
	same.alphaRef = 9;
	same = vortex::NextDecal(same, 0.5f);
	EXPECT_EQ(same.alphaRef, 9);
	decal = vortex::NextDecal(decal, 1.0f);
	EXPECT_TRUE(decal.made);
	EXPECT_EQ(decal.alphaRef, 235);
	// released at 0
	decal = vortex::NextDecal(decal, 0.0f);
	EXPECT_FALSE(decal.made);
	EXPECT_EQ(decal.lastFade, 0.0f);
}

TEST(Vortex, ZBoxModel)
{
	// an In (base scale 0.28) on the land at 11.4 m: the box's top stays at height 0, its walls 0.28 x as wide and deep
	const auto model = vortex_draw::ZBoxModel({1700.0f, 11.4f, 2520.0f}, 0.28f);
	const glm::vec4 top = model * glm::vec4(89.5f, 0.0f, -90.0f, 1.0f);
	EXPECT_FLOAT_EQ(top.x, 1700.0f + 89.5f * 0.28f);
	EXPECT_EQ(top.y, 0.0f);
	EXPECT_FLOAT_EQ(top.z, 2520.0f - 90.0f * 0.28f);
	EXPECT_EQ(top.w, 1.0f);
	const glm::vec4 bottom = model * glm::vec4(0.0f, -400.0f, 0.0f, 1.0f);
	EXPECT_EQ(bottom.x, 1700.0f);
	EXPECT_FLOAT_EQ(bottom.y, -112.0f);
	EXPECT_EQ(bottom.z, 2520.0f);
	// not turned
	EXPECT_EQ(model[0][1], 0.0f);
	EXPECT_EQ(model[0][2], 0.0f);
	EXPECT_EQ(model[2][0], 0.0f);
}

TEST(Vortex, DecalBlock)
{
	// Land 1's leaving vortex: cells (170, 252), block (10, 15)
	EXPECT_EQ(vortex_draw::DecalBlock({1700.23f, 11.4f, 2520.18f}), glm::ivec2(10, 15));
	// the cell is truncated towards 0: a centre just below 0 is still on cell 0, one 10 m below is off the land
	EXPECT_EQ(vortex_draw::DecalBlock({-5.0f, 0.0f, 0.0f}), glm::ivec2(0, 0));
	EXPECT_EQ(vortex_draw::DecalBlock({-10.5f, 0.0f, 100.0f}), std::nullopt);
	EXPECT_EQ(vortex_draw::DecalBlock({5119.9f, 0.0f, 159.9f}), glm::ivec2(31, 0));
	EXPECT_EQ(vortex_draw::DecalBlock({100.0f, 0.0f, 5120.0f}), std::nullopt);
}

TEST(Vortex, DecalUv)
{
	// an In (base scale 0.28) at (1700, 2520) on block (10, 15), corner (1600, 2400): the land's u runs along z, v along x
	const auto uv = vortex_draw::DecalUvTransform({1700.0f, 11.4f, 2520.0f}, {10, 15}, 0.28f);
	EXPECT_FLOAT_EQ(uv.scale, 1.0f / 0.28f);
	// (2400 - 2520) / 160 = -0.75 and (1600 - 1700) / 160 = -0.625, / 0.28, + 0.5
	EXPECT_FLOAT_EQ(uv.offset.x, -0.75f / 0.28f + 0.5f);
	EXPECT_FLOAT_EQ(uv.offset.y, -0.625f / 0.28f + 0.5f);
	// the vortex's centre is the textures' centre: the land's (0.75, 0.625) there
	EXPECT_NEAR(0.75f * uv.scale + uv.offset.x, 0.5f, 1e-5f);
	EXPECT_NEAR(0.625f * uv.scale + uv.offset.y, 0.5f, 1e-5f);
	// 160 x 0.28 = 44.8 m across: 22.4 m from the centre along z is the texture's edge
	EXPECT_NEAR((0.75f + 22.4f / 160.0f) * uv.scale + uv.offset.x, 1.0f, 1e-5f);
	// the Volcano's 0.75: 120 m across
	const auto volcano = vortex_draw::DecalUvTransform({1700.0f, 0.0f, 2520.0f}, {10, 15}, 0.75f);
	EXPECT_NEAR((0.625f - 60.0f / 160.0f) * volcano.scale + volcano.offset.y, 0.0f, 1e-5f);
}

TEST(Vortex, DecalTextures)
{
	using openblack::VortexType;
	EXPECT_EQ(vortex_draw::DecalTexturesFor(VortexType::In).mask, std::string_view("S_VortexBaseAlphacopy"));
	EXPECT_EQ(vortex_draw::DecalTexturesFor(VortexType::Out).ring, std::string_view("S_VortexBaseMultiRing"));
	EXPECT_EQ(vortex_draw::DecalTexturesFor(VortexType::Volcano).mask, std::string_view("S_Volcano_Base_Alpha"));
	EXPECT_EQ(vortex_draw::DecalTexturesFor(VortexType::Volcano).ring, std::string_view("S_Volcano_Base"));
}

TEST(Vortex, EffectRoles)
{
	using vortex_draw::EffectRole;
	// without a vortex every effect is drawn as before
	EXPECT_EQ(vortex_draw::RoleOf(7, {}), EffectRole::Other);
	// a shown vortex: its ground effect before the land, its effect over the land as any other; one that does not show:
	// neither; a vortex without effects (ids 0) matches no effect, not even 0 (the town belief's)
	const std::array<vortex_draw::VortexEffects, 3> vortices {{
	    {.preLandscape = 7, .postLandscape = 8, .shows = true},
	    {.preLandscape = 9, .postLandscape = 10, .shows = false},
	    {.preLandscape = 0, .postLandscape = 0, .shows = false},
	}};
	EXPECT_EQ(vortex_draw::RoleOf(7, vortices), EffectRole::Ground);
	EXPECT_EQ(vortex_draw::RoleOf(8, vortices), EffectRole::Other);
	EXPECT_EQ(vortex_draw::RoleOf(9, vortices), EffectRole::Hidden);
	EXPECT_EQ(vortex_draw::RoleOf(10, vortices), EffectRole::Hidden);
	EXPECT_EQ(vortex_draw::RoleOf(0, vortices), EffectRole::Other);
	EXPECT_EQ(vortex_draw::RoleOf(11, vortices), EffectRole::Other);
}

TEST(Vortex, FunnelMaterials)
{
	using namespace openblack::graphics::render_modes;
	// ZR_SurfRevol's material starts in mode 6: the solid funnel (Z write, MaterialUseTextureAlpha 0) is mode 3, the
	// alpha funnel and the stars (additive, no Z) mode 13, the teleport's pool (texture alpha, no Z) mode 6
	const auto solid = ModeFromProperties(Mode::AlphaTexturedAlphaNoZWrite, {.zWrite = true, .alpha = false});
	EXPECT_EQ(solid, Mode::TexturedAlpha);
	EXPECT_EQ(ModeFromProperties(Mode::AlphaTexturedAlphaNoZWrite, {.additive = true, .alpha = true}),
	          Mode::AlphaTexturedAlphaAdditiveNoZWrite);
	EXPECT_EQ(ModeFromProperties(Mode::AlphaTexturedAlphaNoZWrite, {.alpha = true}), Mode::AlphaTexturedAlphaNoZWrite);
	// without the texture's alpha and without Z: 8; additive wins over it: 13
	EXPECT_EQ(ModeFromProperties(Mode::AlphaTexturedAlphaNoZWrite, {.alpha = false}), Mode::TexturedAlphaNoZWrite);
	EXPECT_EQ(ModeFromProperties(Mode::AlphaTexturedAlphaNoZWrite, {.additive = true, .alpha = false}),
	          Mode::AlphaTexturedAlphaAdditiveNoZWrite);
	// 3 sets the same states as 5, and 8 as 6
	EXPECT_EQ(State(solid), State(Mode::AlphaTexturedAlpha));
	EXPECT_EQ(State(Mode::TexturedAlphaNoZWrite), State(Mode::AlphaTexturedAlphaNoZWrite));
}

namespace
{
/// One point atom made at the first step; the effect lives 100 s
std::shared_ptr<const openblack::psys::File> FakeEffectFile()
{
	std::string zeros;
	for (int i = 1; i < 25; ++i)
	{
		zeros += " 0";
	}
	const std::string text = "BEGINPROPERTIES\n"
	                         "PROPERTY DeleteOnCloseDown BOOL 1\n"
	                         "PROPERTY Hierarchies ARRAY SIZE 25 0" +
	                         zeros +
	                         "\n"
	                         "PROPERTY InitiallyCreated ARRAY SIZE 25 1" +
	                         zeros +
	                         "\n"
	                         "PROPERTY MaxSpellAge FLOAT 100\n"
	                         "ENDPROPERTIES\n"
	                         "BEGINCLASS ParticlePointCreator Point\nBEGINPROPERTIES\nPROPERTY InitialScale FLOAT 1\n"
	                         "ENDPROPERTIES\nENDCLASS\n"
	                         "BEGINCLASS CreateRuleAnAtom A\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
	                         "PROPERTY NextGroups ARRAY SIZE 0\nPROPERTY PCreator PERSIS_PNTR Point\nENDPROPERTIES\n"
	                         "ENDCLASS\n";
	auto file = openblack::psys::File::Parse(text, "test_vortex_effect");
	EXPECT_TRUE(file.has_value());
	return file.has_value() ? std::make_shared<const openblack::psys::File>(std::move(*file)) : nullptr;
}

class VortexDeletion: public ::testing::Test
{
protected:
	void SetUp() override
	{
		openblack::test::EmplaceMapAndVillagerDefaults();
		// every vortex row empty: Create makes no effect of its own
		openblack::Locator::infoConstants::reset(std::make_unique<openblack::InfoConstants>().release());
		openblack::Locator::entitiesRegistry::emplace<openblack::ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
	}

	void TearDown() override
	{
		openblack::ecs::ProcessDeadList(true);
		openblack::ecs::SetDeferredDeletion(false);
		openblack::psys::manager::Clear();
		openblack::Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		openblack::test::ResetMapAndVillagerDefaults();
		openblack::Locator::infoConstants::reset();
	}

	static openblack::ecs::Registry& Reg() { return openblack::Locator::entitiesRegistry::value(); }

	/// An In vortex holding four running fake effects in its four slots (the game never fills the object mover yet)
	entt::entity MakeVortex()
	{
		const auto entity = vortex::Create(glm::vec3(100.0f, 0.0f, 100.0f), openblack::VortexType::In);
		EXPECT_NE(entity, entt::entity(entt::null));
		auto& v = Reg().Get<openblack::ecs::components::LandscapeVortex>(entity);
		const auto file = FakeEffectFile();
		for (auto* id : {&v.objectMover, &v.preLandscape, &v.postLandscape, &v.lightMap})
		{
			*id = openblack::psys::manager::Start(file, glm::vec3(0.0f), 1.0f);
		}
		_ids = {v.objectMover, v.preLandscape, v.postLandscape, v.lightMap};
		for (const auto id : _ids)
		{
			EXPECT_NE(openblack::psys::manager::Find(id), nullptr);
		}
		return entity;
	}

	/// None of the four effects is held or stepped by the manager any more
	void ExpectEffectsGone() const
	{
		for (const auto id : _ids)
		{
			EXPECT_EQ(openblack::psys::manager::Find(id), nullptr);
			EXPECT_FALSE(openblack::psys::manager::ProcessForSpell(id, openblack::psys::ProcessInfo {}, 0.1f));
		}
	}

	std::array<uint32_t, 4> _ids {};
};
} // namespace

TEST_F(VortexDeletion, AtOnce)
{
	const auto entity = MakeVortex();
	openblack::ecs::ToBeDeleted(entity, true);
	EXPECT_FALSE(Reg().Valid(entity));
	ExpectEffectsGone();
}

TEST_F(VortexDeletion, Deferred)
{
	const auto entity = MakeVortex();
	openblack::ecs::SetDeferredDeletion(true);
	openblack::ecs::ToBeDeleted(entity);
	// marked, still in the registry, its four ids released and its decal gone
	ASSERT_TRUE(Reg().Valid(entity));
	EXPECT_TRUE(Reg().AllOf<openblack::ecs::components::Unavailable>(entity));
	const auto& v = Reg().Get<const openblack::ecs::components::LandscapeVortex>(entity);
	EXPECT_EQ(v.objectMover, 0u);
	EXPECT_EQ(v.preLandscape, 0u);
	EXPECT_EQ(v.postLandscape, 0u);
	EXPECT_EQ(v.lightMap, 0u);
	EXPECT_FALSE(v.decal.made);
	ExpectEffectsGone();
	openblack::ecs::ProcessDeadList(true);
	EXPECT_FALSE(Reg().Valid(entity));
}
