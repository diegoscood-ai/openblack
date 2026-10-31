/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The vortex's fade and land curves (docs/wiki/world/vortex.md#drawing): Openness and LevelAmount (Magic/VortexRules.h, whose
// levelled square is tested in test_vortex_rules), and the drawing's curves: the glow's strength, the swirl's depth under
// the ground, the hole's threshold and the decal's change detector. The original computes them in float steps (its FPU
// runs at 24 bits); the bit patterns below are those float steps. And a deleted vortex's particle systems:
// ecs::ToBeDeleted, at once or deferred, deletes them from the particle manager (fake effects from a synthetic spell
// file). And what the renderer draws for a vortex (Graphics/VortexDraw.h): the depth box's placement, the decal's block,
// texture coordinates and textures, which effects are drawn before the land or not at all, and the solid funnel's
// material mode. And the crossing file's reader (ECS/VortexSave.h): a line that makes nothing hands over again the
// object the line before made, as the original's reader does; the file's text is cut into lines at its new lines, and a
// last line with no new line after it runs but ends the file, handing nothing over.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "Audio/Game/BankTables.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Vortex.h"
#include "ECS/VortexSave.h"
#include "Enums.h"
#include "GameClock.h"
#include "Graphics/RenderModes.h"
#include "Graphics/VortexDraw.h"
#include "InfoConstants.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "Magic/VortexRules.h"
#include "Particles/ParticleManager.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/ParticleTypes.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using openblack::VortexStateType;
using openblack::ecs::components::VortexDecal;
namespace vortex = openblack::ecs::vortex;
namespace vortex_rules = openblack::vortex;
namespace vortex_draw = openblack::graphics::vortex_draw;

TEST(Vortex, Fade)
{
	EXPECT_EQ(vortex_rules::Openness(VortexStateType::Inactive, 3.0f), 0.0f);
	EXPECT_EQ(vortex_rules::Openness(VortexStateType::Active, 0.0f), 1.0f);
	// FadeIn: 0 for 2 s, then smooth((e - 2) / 5)
	EXPECT_EQ(vortex_rules::Openness(VortexStateType::FadeIn, 1.9f), 0.0f);
	EXPECT_EQ(vortex_rules::Openness(VortexStateType::FadeIn, 2.0f), 0.0f);
	EXPECT_EQ(vortex_rules::Openness(VortexStateType::FadeIn, 4.5f), 0.5f); // (3 - 1) 0.5 0.5
	EXPECT_EQ(vortex_rules::Openness(VortexStateType::FadeIn, 7.0f), 1.0f);
	EXPECT_EQ(vortex_rules::Openness(VortexStateType::FadeIn, 20.0f), 1.0f);
	// FadeOut: smooth(1 - e / 5) for 5 s, then 0
	EXPECT_EQ(vortex_rules::Openness(VortexStateType::FadeOut, 0.0f), 1.0f);
	EXPECT_EQ(vortex_rules::Openness(VortexStateType::FadeOut, 2.5f), 0.5f);
	EXPECT_EQ(vortex_rules::Openness(VortexStateType::FadeOut, 5.0f), 0.0f);
}

TEST(Vortex, LandFactor)
{
	EXPECT_EQ(vortex_rules::LevelAmount(VortexStateType::FadeIn, 4.5f), 0.75f); // 1 - (1 - 0.5)^2
	EXPECT_EQ(vortex_rules::LevelAmount(VortexStateType::FadeIn, 1.0f), 0.0f);
	EXPECT_EQ(vortex_rules::LevelAmount(VortexStateType::Active, 0.0f), 1.0f);
	EXPECT_EQ(vortex_rules::LevelAmount(VortexStateType::FadeOut, 4.9f), 1.0f); // FadeOut takes s = 1
}

TEST(Vortex, LightMapAlpha)
{
	EXPECT_EQ(vortex_rules::GlowBrightness(VortexStateType::Inactive, 3.0f), 0.0f);
	EXPECT_EQ(vortex_rules::GlowBrightness(VortexStateType::Active, 3.0f), 0.6f);
	// FadeIn: e / 2 for 2 s, then 1 + (0.6 - 1) c, c = (e - 2) / 5 kept to 0..1
	EXPECT_EQ(vortex_rules::GlowBrightness(VortexStateType::FadeIn, 1.0f), 0.5f);
	EXPECT_EQ(vortex_rules::GlowBrightness(VortexStateType::FadeIn, 2.0f), 1.0f);
	EXPECT_EQ(vortex_rules::GlowBrightness(VortexStateType::FadeIn, 3.3f), std::bit_cast<float>(0x3F656042u)); // 0.896
	EXPECT_EQ(vortex_rules::GlowBrightness(VortexStateType::FadeIn, 4.5f), std::bit_cast<float>(0x3F4CCCCDu)); // 0.8
	EXPECT_EQ(vortex_rules::GlowBrightness(VortexStateType::FadeIn, 7.0f), 0.6f);
	EXPECT_EQ(vortex_rules::GlowBrightness(VortexStateType::FadeIn, 20.0f), 0.6f);
	// FadeOut: 0.6 + (1 - 0.6) e / 5 for 5 s, then 1 - (e - 5) / 2 kept to 0..1
	EXPECT_EQ(vortex_rules::GlowBrightness(VortexStateType::FadeOut, 0.0f), 0.6f);
	EXPECT_EQ(vortex_rules::GlowBrightness(VortexStateType::FadeOut, 3.3f), std::bit_cast<float>(0x3F5D2F1Au)); // 0.864
	EXPECT_EQ(vortex_rules::GlowBrightness(VortexStateType::FadeOut, 5.0f), 1.0f);
	EXPECT_EQ(vortex_rules::GlowBrightness(VortexStateType::FadeOut, 6.0f), 0.5f);
	EXPECT_EQ(vortex_rules::GlowBrightness(VortexStateType::FadeOut, 7.0f), 0.0f);
	EXPECT_EQ(vortex_rules::GlowBrightness(VortexStateType::FadeOut, 9.0f), 0.0f);
	// as the effect's alpha: x 255, truncated
	EXPECT_EQ(vortex::AlphaByte(0.6f), 153);
	EXPECT_EQ(vortex::AlphaByte(std::bit_cast<float>(0x3F656042u)), 228);
	EXPECT_EQ(vortex::AlphaByte(1.0f), 255);
	EXPECT_EQ(vortex::AlphaByte(0.0f), 0);
}

TEST(Vortex, PreLandscapeHeight)
{
	// ground - ((0.3 - 2.5) q + 2.5): 2.5 m under the land at q = 0, 0.3 m under it at q = 1
	EXPECT_EQ(10.0f - vortex_rules::SwirlDepth(0.0f), 7.5f);
	EXPECT_EQ(10.0f - vortex_rules::SwirlDepth(1.0f), std::bit_cast<float>(0x411B3333u));   // 9.7
	EXPECT_EQ(37.25f - vortex_rules::SwirlDepth(0.75f), std::bit_cast<float>(0x4211999Au)); // 36.4
	// in float steps; in double this one would round to 0xBFEB851F
	EXPECT_EQ(0.0f - vortex_rules::SwirlDepth(0.3f), std::bit_cast<float>(0xBFEB851Eu));
}

TEST(Vortex, DecalAlphaRef)
{
	EXPECT_EQ(vortex_rules::GroundHoleThreshold(0.0f), 0);
	EXPECT_EQ(vortex_rules::GroundHoleThreshold(0.3f), 76);
	EXPECT_EQ(vortex_rules::GroundHoleThreshold(0.5f), 127);
	EXPECT_EQ(vortex_rules::GroundHoleThreshold(0.92f), 234);
	// capped at 235
	EXPECT_EQ(vortex_rules::GroundHoleThreshold(0.9216f), 235);
	EXPECT_EQ(vortex_rules::GroundHoleThreshold(1.0f), 235);
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

/// A vortex is made belonging to nothing, so it is the neutral player's, and so are its object mover and its animals'
/// flock
TEST(Vortex, AVortexIsTheNeutralPlayers)
{
	EXPECT_EQ(vortex::k_Player, openblack::PlayerNames::NEUTRAL);
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
std::shared_ptr<const openblack::particles::File> FakeEffectFile()
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
	auto file = openblack::particles::File::Parse(text, "test_vortex_effect");
	EXPECT_TRUE(file.has_value());
	return file.has_value() ? std::make_shared<const openblack::particles::File>(std::move(*file)) : nullptr;
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
		openblack::particles::manager::Clear();
		openblack::Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		openblack::test::ResetMapAndVillagerDefaults();
		openblack::Locator::infoConstants::reset();
	}

	static openblack::ecs::Registry& Reg() { return openblack::Locator::entitiesRegistry::value(); }

	/// An In vortex holding four running fake effects in its four slots (the test's info rows name no effect of their own)
	entt::entity MakeVortex()
	{
		const auto entity = vortex::Create(glm::vec3(100.0f, 0.0f, 100.0f), openblack::VortexType::In);
		EXPECT_NE(entity, entt::entity(entt::null));
		auto& v = Reg().Get<openblack::ecs::components::LandscapeVortex>(entity);
		const auto file = FakeEffectFile();
		for (auto* id : {&v.objectMover, &v.preLandscape, &v.postLandscape, &v.lightMap})
		{
			*id = openblack::particles::manager::Start(file, glm::vec3(0.0f), 1.0f);
		}
		_ids = {v.objectMover, v.preLandscape, v.postLandscape, v.lightMap};
		for (const auto id : _ids)
		{
			EXPECT_NE(openblack::particles::manager::Find(id), nullptr);
		}
		return entity;
	}

	/// None of the four effects is held or stepped by the manager any more
	void ExpectEffectsGone() const
	{
		for (const auto id : _ids)
		{
			EXPECT_EQ(openblack::particles::manager::Find(id), nullptr);
			EXPECT_FALSE(openblack::particles::manager::ProcessForSpell(id, openblack::particles::ProcessInfo {}, 0.1f));
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

namespace
{
/// The sound tags: each thing's tag made is kept with its arguments; everything else does nothing
class RecordingSoundTags final: public openblack::ecs::systems::SoundTagSystemInterface
{
public:
	struct ThingTag
	{
		entt::entity thing;
		glm::vec3 offset;
		int sample;
		bool track;
		int mode;
		int loops;
		bool extra3DFlag;
		bool is3D;
		openblack::audio::SfxBank bank;
		int delay;
	};
	std::vector<ThingTag> tags;

	void ProcessTurn() override {}
	void SetActive(TagId, bool) override {}
	TagId CreatePointSound(const glm::vec3&, int, int, int, bool, bool, openblack::audio::SfxBank, int) override { return 0; }
	TagId CreatePointSound(const glm::vec3&, int, int, int, bool, bool, openblack::audio::AtmosType, int) override { return 0; }
	TagId Create(entt::entity thing, const glm::vec3& offset, int sample, bool track, int mode, int loops, bool extra3DFlag,
	             bool is3D, openblack::audio::SfxBank bank, int delay) override
	{
		tags.push_back({thing, offset, sample, track, mode, loops, extra3DFlag, is3D, bank, delay});
		return static_cast<TagId>(tags.size());
	}
	TagId CreateAtMarker(const glm::vec3&, int, int, int, bool, openblack::audio::SfxBank) override { return 0; }
	void Remove(entt::entity, int, openblack::audio::SfxBank, bool) override {}
	void Delete(TagId) override {}
	[[nodiscard]] bool Exists(TagId) const override { return false; }
	void Clear() override {}
	[[nodiscard]] std::optional<glm::vec3> TagSoundPoint(TagId) const override { return std::nullopt; }
	[[nodiscard]] std::optional<glm::vec3> Point(TagId) const override { return std::nullopt; }
};
} // namespace

/// Only the volcano tags itself as it is made: a loop on the vortex, untracked, mode 2, 3D, in the in-game bank, no
/// delay (the test has no bank loaded, so the tag's sample resolves to 0; the sound id is VortexSound's own test)
TEST_F(VortexDeletion, OnlyTheVolcanoTagsItselfWhenMade)
{
	openblack::test::RestoreService<openblack::Locator::soundTagSystem> restoreTags;
	auto& sounds = static_cast<RecordingSoundTags&>(openblack::Locator::soundTagSystem::emplace<RecordingSoundTags>());
	vortex::Create(glm::vec3(100.0f, 0.0f, 100.0f), openblack::VortexType::In);
	EXPECT_TRUE(sounds.tags.empty());
	const auto volcano = vortex::Create(glm::vec3(200.0f, 0.0f, 100.0f), openblack::VortexType::Volcano);
	ASSERT_NE(volcano, entt::entity(entt::null));
	ASSERT_EQ(sounds.tags.size(), 1u);
	const auto& tag = sounds.tags.front();
	EXPECT_EQ(tag.thing, volcano);
	EXPECT_EQ(tag.offset, glm::vec3(0.0f));
	EXPECT_FALSE(tag.track);
	EXPECT_EQ(tag.mode, 2);
	EXPECT_EQ(tag.loops, -1);
	EXPECT_FALSE(tag.extra3DFlag);
	EXPECT_TRUE(tag.is3D);
	EXPECT_EQ(tag.bank, openblack::audio::SfxBank::InGame);
	EXPECT_EQ(tag.delay, 0);
}

TEST(VortexSave, ALineThatMakesNothingHandsOverTheLastObjectAgain)
{
	using openblack::lhscriptx::Script;
	namespace vortex_save = openblack::ecs::vortex_save;
	// the object an earlier line made
	const auto made = entt::entity {42};
	Script::SetLastCreated(made);
	vortex_save::Reader reader {.lines = {"", ""}};
	for (int line = 0; line < 2; ++line)
	{
		const auto read = vortex_save::ReadNext(reader, {});
		ASSERT_TRUE(read.has_value());
		EXPECT_EQ(*read, made);
	}
	// then the end of the file
	EXPECT_FALSE(vortex_save::ReadNext(reader, {}).has_value());
	EXPECT_EQ(reader.reads, 3u);
	// no offset left behind
	EXPECT_FALSE(Script::PositionOffset().has_value());
	Script::SetLastCreated(entt::null);
}

TEST(VortexSave, TheFileIsCutIntoItsLinesAtItsNewLines)
{
	namespace vortex_save = openblack::ecs::vortex_save;
	// every line ended, a carriage return before a new line dropped, a blank line kept
	const auto ended = vortex_save::ReaderFrom("A(1)\r\n\nB(2)\n");
	EXPECT_EQ(ended.lines, (std::vector<std::string> {"A(1)", "", "B(2)"}));
	EXPECT_TRUE(ended.lastLineEnded);
	EXPECT_EQ(ended.next, 0u);
	EXPECT_EQ(ended.reads, 0u);
	// the last line with no new line after it
	const auto open = vortex_save::ReaderFrom("A(1)\nB(2)");
	EXPECT_EQ(open.lines, (std::vector<std::string> {"A(1)", "B(2)"}));
	EXPECT_FALSE(open.lastLineEnded);
	// an empty file has no line
	const auto empty = vortex_save::ReaderFrom("");
	EXPECT_TRUE(empty.lines.empty());
	EXPECT_TRUE(empty.lastLineEnded);
}

TEST(VortexSave, ALastLineWithNoNewLineRunsButEndsTheFile)
{
	using openblack::lhscriptx::Script;
	namespace vortex_save = openblack::ecs::vortex_save;
	const auto made = entt::entity {42};
	Script::SetLastCreated(made);
	auto reader = vortex_save::ReaderFrom("\n");
	auto open = vortex_save::ReaderFrom(" ");
	// an ended blank line hands over again what the line before made
	const auto read = vortex_save::ReadNext(reader, {});
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(*read, made);
	// the same with no new line after it: read, then the end of the file
	EXPECT_FALSE(vortex_save::ReadNext(open, {}).has_value());
	EXPECT_EQ(open.reads, 1u);
	EXPECT_EQ(open.next, 1u);
	EXPECT_FALSE(Script::PositionOffset().has_value());
	Script::SetLastCreated(entt::null);
}

TEST(VortexSave, TheFileIsKeptUnderTheGamesFolder)
{
	namespace vortex_save = openblack::ecs::vortex_save;
	EXPECT_EQ(vortex_save::SaveFolderUnder("C:/Games/Black & White"),
	          std::filesystem::path("C:/Games/Black & White") / "SaveGame");
	EXPECT_EQ(vortex_save::k_SaveFolderName, "SaveGame");
}

// The outgoing vortex's "before" effect is the incoming vortex's file, as the original's table points both at it, and
// the volcano has its own before, after and light-map files (docs/wiki/formats/info-dat.md#the-gvortexinfo-rows)
TEST(Vortex, TheWayIntoALandUsesTheWayOutsBeforeEffectAndTheVolcanoItsOwn)
{
	using openblack::ParticleType;
	namespace particles = openblack::particles;
	EXPECT_EQ(particles::ParticleTypeFile(ParticleType::LandscapeVortexInBefore), "SF_LandscapeVortexInBefore");
	EXPECT_EQ(particles::ParticleTypeFile(ParticleType::LandscapeVortexOutBefore), "SF_LandscapeVortexInBefore");
	EXPECT_EQ(particles::ParticleTypeFile(ParticleType::LandscapeVortexOutAfter), "SF_LandscapeVortexOutAfter");
	EXPECT_EQ(particles::ParticleTypeFile(ParticleType::VolcanoVortexBefore), "SF_LandscapeVolcanoBefore");
	EXPECT_EQ(particles::ParticleTypeFile(ParticleType::VolcanoVortexAfter), "SF_LandscapeVolcanoAfter");
	EXPECT_EQ(particles::ParticleTypeFile(ParticleType::VolcanoVortexLightmap), "SF_LandscapeVolcanoLightMap");
}

namespace
{
std::optional<std::filesystem::path> RealGamePath()
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		return std::nullopt;
	}
	return std::filesystem::path(game);
}

/// info.dat's table as the game reads it (after the file's header), or nothing when the file is missing or of another
/// size
std::unique_ptr<openblack::InfoConstants> ReadRealInfoDat(const std::filesystem::path& game)
{
	std::ifstream file(game / "Scripts" / "info.dat", std::ios::binary);
	if (!file.is_open())
	{
		return nullptr;
	}
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	if (data.size() != 0x2C + sizeof(openblack::InfoConstants))
	{
		return nullptr;
	}
	auto info = std::make_unique<openblack::InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(openblack::InfoConstants));
	return info;
}
} // namespace

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it. Its twin with made-up rows
// is VortexMaking.EachKindStartsInTheStateItsRowGives. The game's three rows: each kind's effects (the way into a land
// has no object mover and the way out's "before" effect), its starting state, the "fade when deleted" column and the
// limits nothing reads, as the original's table (docs/wiki/formats/info-dat.md#the-gvortexinfo-rows)
TEST(VortexTable, TheGamesRowsGiveEachKindItsEffectsItsStartAndLimitsNothingReads)
{
	using openblack::ParticleType;
	using openblack::VortexStatType;
	const auto game = RealGamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const auto info = ReadRealInfoDat(*game);
	ASSERT_NE(info, nullptr);
	const auto& in = info->vortex.at(static_cast<size_t>(openblack::VortexType::In));
	EXPECT_EQ(in.particleTypePreLandscape, ParticleType::LandscapeVortexInBefore);
	EXPECT_EQ(in.particleTypePostLandscape, ParticleType::LandscapeVortexInAfter);
	EXPECT_EQ(in.particleTypeObjectMover, ParticleType::LandscapeVortexObjectMover);
	EXPECT_EQ(in.particleTypeLightMap, ParticleType::LandscapeVortexLightmap);
	EXPECT_EQ(in.initialState, VortexStateType::FadeIn);
	EXPECT_EQ(in.fadeWhenDeleted, 0u);
	EXPECT_EQ(in.baseScale, 0.28f);
	const auto& out = info->vortex.at(static_cast<size_t>(openblack::VortexType::Out));
	EXPECT_EQ(out.particleTypePreLandscape, ParticleType::LandscapeVortexOutBefore);
	EXPECT_EQ(out.particleTypePostLandscape, ParticleType::LandscapeVortexOutAfter);
	EXPECT_EQ(out.particleTypeObjectMover, ParticleType::None);
	EXPECT_EQ(out.particleTypeLightMap, ParticleType::LandscapeVortexLightmap);
	EXPECT_EQ(out.initialState, VortexStateType::Active);
	EXPECT_EQ(out.fadeWhenDeleted, 1u);
	EXPECT_EQ(out.baseScale, 0.28f);
	EXPECT_EQ(out.maxToCreateTotalObjects, static_cast<VortexStatType>(1000));
	for (const auto limit :
	     {out.maxToCreateResourceFood, out.maxToCreateResourceWood, out.maxToCreateVillager, out.maxToCreateOneShot})
	{
		EXPECT_EQ(limit, static_cast<VortexStatType>(-1));
	}
	const auto& volcano = info->vortex.at(static_cast<size_t>(openblack::VortexType::Volcano));
	EXPECT_EQ(volcano.particleTypePreLandscape, ParticleType::VolcanoVortexBefore);
	EXPECT_EQ(volcano.particleTypePostLandscape, ParticleType::VolcanoVortexAfter);
	EXPECT_EQ(volcano.particleTypeObjectMover, ParticleType::None);
	EXPECT_EQ(volcano.particleTypeLightMap, ParticleType::VolcanoVortexLightmap);
	EXPECT_EQ(volcano.initialState, VortexStateType::Active);
	EXPECT_EQ(volcano.fadeWhenDeleted, 0u);
	EXPECT_EQ(volcano.baseScale, 0.75f);
}

namespace
{
/// A vortex's world as VortexDeletion gives it, with rows of the vortex table a test sets itself and the sound tags
/// the test puts in given back after it
class VortexMaking: public VortexDeletion
{
protected:
	/// The vortex table's three rows, the rest of the info empty
	static void SetRows(const std::array<openblack::GVortexInfo, 3>& rows)
	{
		auto info = std::make_unique<openblack::InfoConstants>();
		info->vortex = rows;
		openblack::Locator::infoConstants::reset(info.release());
	}

	static openblack::ecs::components::LandscapeVortex& VortexOf(entt::entity vortex)
	{
		return Reg().Get<openblack::ecs::components::LandscapeVortex>(vortex);
	}

	/// A rock lying at a place
	static entt::entity Rock(glm::vec3 position)
	{
		const auto rock = Reg().Create();
		Reg().Assign<openblack::ecs::components::Transform>(rock, position, glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<openblack::ecs::components::MobileStatic>(rock).type = openblack::MobileStaticInfo::Rock;
		return rock;
	}

	openblack::test::RestoreService<openblack::Locator::soundTagSystem> _restoreTags;
};

/// A fresh folder of a test's own under the system's temporary folder, removed with what is in it when it goes
class ScratchFolder
{
public:
	explicit ScratchFolder(std::string_view name)
	    : _path(std::filesystem::temp_directory_path() / name)
	{
		std::error_code error;
		std::filesystem::remove_all(_path, error);
		std::filesystem::create_directories(_path, error);
	}
	~ScratchFolder()
	{
		std::error_code error;
		std::filesystem::remove_all(_path, error);
	}
	ScratchFolder(const ScratchFolder&) = delete;
	ScratchFolder& operator=(const ScratchFolder&) = delete;
	ScratchFolder(ScratchFolder&&) = delete;
	ScratchFolder& operator=(ScratchFolder&&) = delete;

	[[nodiscard]] const std::filesystem::path& Path() const { return _path; }

private:
	std::filesystem::path _path;
};

/// A file's whole text, empty when there is none
std::string ReadText(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
} // namespace

// CREATE of a vortex makes an In, an Out or a Volcano, each with its own row and Create's radius of 50, and nothing of
// any other kind, as the original (docs/wiki/world/vortex.md#the-vortex-objects)
TEST_F(VortexMaking, OnlyAnInAnOutOrAVolcanoIsMade)
{
	using openblack::VortexType;
	static_cast<void>(openblack::Locator::soundTagSystem::emplace<RecordingSoundTags>());
	for (const auto type : {VortexType::In, VortexType::Out, VortexType::Volcano})
	{
		const auto made = vortex::Create(glm::vec3(100.0f, 0.0f, 100.0f), type);
		ASSERT_NE(made, entt::entity(entt::null)) << static_cast<int>(type);
		const auto& v = VortexOf(made);
		EXPECT_EQ(v.type, type);
		EXPECT_EQ(v.info, static_cast<int32_t>(type));
		EXPECT_EQ(v.radius, 50.0f);
	}
	EXPECT_EQ(vortex::Create(glm::vec3(100.0f, 0.0f, 100.0f), static_cast<VortexType>(3)), entt::entity(entt::null));
}

// Each kind starts in the state its row of the vortex table gives, a fade-in from the turn it is made, as the original
// (docs/wiki/world/vortex.md#the-vortex-objects); the rows here are made up (the game's are VortexTable's)
TEST_F(VortexMaking, EachKindStartsInTheStateItsRowGives)
{
	using openblack::VortexType;
	std::array<openblack::GVortexInfo, 3> rows {};
	rows.at(0).initialState = VortexStateType::FadeIn;
	rows.at(1).initialState = VortexStateType::Active;
	rows.at(2).initialState = VortexStateType::Inactive;
	SetRows(rows);
	static_cast<void>(openblack::Locator::soundTagSystem::emplace<RecordingSoundTags>());
	openblack::game_clock::SetTurn(40);
	const auto in = vortex::Create(glm::vec3(100.0f, 0.0f, 100.0f), VortexType::In);
	const auto out = vortex::Create(glm::vec3(300.0f, 0.0f, 100.0f), VortexType::Out);
	const auto volcano = vortex::Create(glm::vec3(500.0f, 0.0f, 100.0f), VortexType::Volcano);
	EXPECT_EQ(VortexOf(in).state, VortexStateType::FadeIn);
	EXPECT_EQ(VortexOf(in).stateTurn, 40u);
	EXPECT_EQ(VortexOf(out).state, VortexStateType::Active);
	EXPECT_EQ(VortexOf(volcano).state, VortexStateType::Inactive);
}

// The table's "fade when deleted" column is never read: a vortex whose row sets it stays fully open until it is told
// to fade out, and only then goes, once its fade is over, as the original (docs/wiki/world/vortex.md#states-and-fades)
TEST_F(VortexMaking, AVortexGoesOnlyAfterItIsFadedOutWhateverItsRowSays)
{
	std::array<openblack::GVortexInfo, 3> rows {};
	for (auto& row : rows)
	{
		row.initialState = VortexStateType::Active;
		row.fadeWhenDeleted = 1;
	}
	SetRows(rows);
	openblack::game_clock::SetTurn(10);
	const auto in = vortex::Create(glm::vec3(100.0f, 0.0f, 100.0f), openblack::VortexType::In);
	for (uint32_t turn = 11; turn <= 200; ++turn)
	{
		openblack::game_clock::SetTurn(turn);
		vortex::ProcessAll(turn);
	}
	ASSERT_TRUE(Reg().Valid(in));
	EXPECT_EQ(VortexOf(in).state, VortexStateType::Active);
	EXPECT_TRUE(vortex::StartFadeOut(in));
	// six seconds into the fade it is still there, eight seconds in it is gone
	openblack::game_clock::SetTurn(260);
	vortex::ProcessAll(260);
	ASSERT_TRUE(Reg().Valid(in));
	EXPECT_EQ(VortexOf(in).state, VortexStateType::FadeOut);
	openblack::game_clock::SetTurn(280);
	vortex::ProcessAll(280);
	EXPECT_FALSE(Reg().Valid(in));
}

// A fade-out starts on a vortex from the turn it is told; a thing that is not a vortex is answered as one, as the
// original (docs/wiki/world/vortex.md#states-and-fades)
TEST_F(VortexMaking, OnlyAVortexStartsToFadeOut)
{
	openblack::game_clock::SetTurn(25);
	const auto in = vortex::Create(glm::vec3(100.0f, 0.0f, 100.0f), openblack::VortexType::In);
	EXPECT_TRUE(vortex::StartFadeOut(in));
	EXPECT_EQ(VortexOf(in).state, VortexStateType::FadeOut);
	EXPECT_EQ(VortexOf(in).stateTurn, 25u);
	EXPECT_FALSE(vortex::StartFadeOut(Rock(glm::vec3(120.0f, 0.0f, 100.0f))));
	EXPECT_FALSE(vortex::StartFadeOut(entt::null));
}

// A way out of a land takes nothing without its object mover. With it, each thing that could fly is handed to the
// mover at rest, an indestructible one marked so, and a creature never, as the original
// (docs/wiki/world/vortex.md#taking-objects-in-the-in-vortex)
TEST_F(VortexMaking, NothingIsTakenWithoutTheObjectMoverAndACreatureNever)
{
	const auto in = vortex::Create(glm::vec3(100.0f, 0.0f, 100.0f), openblack::VortexType::In);
	const auto rock = Rock(glm::vec3(110.0f, 0.0f, 100.0f));
	vortex::TakeIn(in, rock, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(110.0f, 0.0f, 100.0f), false);
	EXPECT_TRUE(VortexOf(in).queue.empty());

	VortexOf(in).objectMover = openblack::particles::manager::Start(FakeEffectFile(), glm::vec3(0.0f), 1.0f);
	ASSERT_NE(openblack::particles::manager::Find(VortexOf(in).objectMover), nullptr);
	vortex::TakeIn(in, rock, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(110.0f, 0.0f, 100.0f), false);
	const auto stone = Rock(glm::vec3(90.0f, 0.0f, 100.0f));
	Reg().Assign<openblack::ecs::components::Indestructible>(stone);
	vortex::TakeIn(in, stone, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(90.0f, 0.0f, 100.0f), false);
	const auto creature = Reg().Create();
	Reg().Assign<openblack::ecs::components::Transform>(creature, glm::vec3(200.0f, 0.0f, 100.0f), glm::mat3(1.0f),
	                                                    glm::vec3(1.0f));
	Reg().Assign<openblack::ecs::components::Creature>(creature);
	vortex::TakeIn(in, creature, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(200.0f, 0.0f, 100.0f), false);

	const auto& queue = VortexOf(in).queue;
	ASSERT_EQ(queue.size(), 2u);
	EXPECT_EQ(queue.at(0).object, rock);
	EXPECT_FALSE(queue.at(0).fromPhysics);
	EXPECT_FALSE(queue.at(0).indestructible);
	EXPECT_EQ(queue.at(0).vortex, in);
	EXPECT_EQ(queue.at(1).object, stone);
	EXPECT_TRUE(queue.at(1).indestructible);
}

// What a way out of a land takes goes into the crossing file in the save folder, one land-script line each, in the
// order it is caught and each thing once, placed relative to the vortex, as the original
// (docs/wiki/world/vortex.md#the-land-script-round-trip-of-vortextxt)
TEST_F(VortexMaking, EachThingTakenIsWrittenOnceInTheOrderItIsCaught)
{
	const ScratchFolder folder("openblack_test_vortex_written_once");
	namespace vortex_save = openblack::ecs::vortex_save;
	vortex_save::SetSaveFolder(folder.Path());
	vortex_save::SetWritable(true);
	const auto in = vortex::Create(glm::vec3(100.0f, 0.0f, 100.0f), openblack::VortexType::In);
	ASSERT_NE(VortexOf(in).writer, nullptr);
	const auto first = Rock(glm::vec3(80.0f, 0.0f, 100.0f));
	const auto second = Rock(glm::vec3(100.0f, 0.0f, 130.0f));
	vortex::HandOff(in, first);
	vortex::HandOff(in, first);
	vortex::HandOff(in, second);
	EXPECT_EQ(VortexOf(in).writer->lines, 2u);
	VortexOf(in).writer.reset();

	const auto text = ReadText(folder.Path() / "vortex.txt");
	const auto end = text.find('\n');
	ASSERT_NE(end, std::string::npos);
	EXPECT_TRUE(text.starts_with("CREATE_MOBILE_STATIC(\"-20.00,0.00\""));
	EXPECT_TRUE(text.substr(end + 1).starts_with("CREATE_MOBILE_STATIC(\"0.00,30.00\""));
	EXPECT_EQ(text.find('\n', end + 1), text.size() - 1);
}

// A thing is handed over the moment it is caught: a villager leaves its town first, so its line names none, and an
// animal leaves its town and its flock, then each one's line is written, as the original
// (docs/wiki/world/vortex.md#taking-objects-in-the-in-vortex)
TEST_F(VortexMaking, WhatIsCaughtLeavesItsTownAndFlockBeforeItsLineIsWritten)
{
	using openblack::ecs::components::Animal;
	using openblack::ecs::components::Flock;
	using openblack::ecs::components::Town;
	using openblack::ecs::components::Villager;
	const ScratchFolder folder("openblack_test_vortex_hand_off");
	namespace vortex_save = openblack::ecs::vortex_save;
	vortex_save::SetSaveFolder(folder.Path());
	vortex_save::SetWritable(true);
	const auto in = vortex::Create(glm::vec3(100.0f, 0.0f, 100.0f), openblack::VortexType::In);
	ASSERT_NE(VortexOf(in).writer, nullptr);
	const auto town = Reg().Create();
	Reg().Assign<Town>(town).id = 4;

	const auto villager = Reg().Create();
	Reg().Assign<openblack::ecs::components::Transform>(villager, glm::vec3(110.0f, 0.0f, 120.0f), glm::mat3(1.0f),
	                                                    glm::vec3(1.0f));
	auto& man = Reg().Assign<Villager>(villager);
	man.tribe = openblack::Tribe::CELTIC;
	man.number = openblack::VillagerNumber::Farmer;
	man.town = town;
	man.abode = entt::null;
	vortex::HandOff(in, villager);
	EXPECT_EQ(Reg().Get<Villager>(villager).town, entt::entity(entt::null));
	EXPECT_EQ(VortexOf(in).writer->lines, 1u);

	const auto flock = Reg().Create();
	const auto cow = Reg().Create();
	Reg().Assign<openblack::ecs::components::Transform>(cow, glm::vec3(90.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& animal = Reg().Assign<Animal>(cow);
	animal.type = openblack::AnimalInfo::Cow;
	animal.flock = flock;
	animal.town = town;
	Reg().Assign<Flock>(flock).members = {cow};
	Reg().Get<Town>(town).animals = {cow};
	vortex::HandOff(in, cow);
	EXPECT_EQ(Reg().Get<Animal>(cow).town, entt::entity(entt::null));
	EXPECT_EQ(Reg().Get<Animal>(cow).flock, entt::entity(entt::null));
	EXPECT_TRUE(Reg().Get<Town>(town).animals.empty());
	EXPECT_TRUE(Reg().Get<Flock>(flock).members.empty());
	EXPECT_EQ(VortexOf(in).writer->lines, 2u);
	VortexOf(in).writer.reset();

	// the villager's line names no town
	EXPECT_TRUE(ReadText(folder.Path() / "vortex.txt").starts_with("CREATE_TOWN_VILLAGER(-1, "));
}

// An open way out of a land takes, once a turn, what lies in the map's cells within its reach, another god's thing as
// the player's, and nothing that is in no cell, as a thing in the hand is not, as the original
// (docs/wiki/world/vortex.md#taking-objects-in-the-in-vortex)
TEST_F(VortexMaking, AnOpenWayOutTakesWhatLiesInTheMapsCellsWhoeverItBelongsTo)
{
	using openblack::ecs::components::DeadTree;
	std::array<openblack::GVortexInfo, 3> rows {};
	rows.at(0).initialState = VortexStateType::Active;
	SetRows(rows);
	const auto in = vortex::Create(glm::vec3(100.0f, 0.0f, 100.0f), openblack::VortexType::In);
	ASSERT_EQ(VortexOf(in).state, VortexStateType::Active);
	VortexOf(in).objectMover = openblack::particles::manager::Start(FakeEffectFile(), glm::vec3(0.0f), 1.0f);
	ASSERT_NE(openblack::particles::manager::Find(VortexOf(in).objectMover), nullptr);
	const auto lying = Rock(glm::vec3(112.0f, 0.0f, 100.0f));
	openblack::ecs::map_cells::InsertMapObject(lying);
	// in no cell, as what the hand holds
	const auto held = Rock(glm::vec3(105.0f, 0.0f, 105.0f));
	const auto deadLog = [](glm::vec3 position, openblack::PlayerNames owner) {
		const auto e = Reg().Create();
		Reg().Assign<openblack::ecs::components::Transform>(e, position, glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<DeadTree>(e, DeadTree {.type = openblack::TreeInfo::Beech, .owner = owner});
		openblack::ecs::map_cells::InsertMapObject(e);
		return e;
	};
	const auto players = deadLog(glm::vec3(100.0f, 0.0f, 112.0f), openblack::PlayerNames::PLAYER_ONE);
	const auto others = deadLog(glm::vec3(88.0f, 0.0f, 100.0f), openblack::PlayerNames::PLAYER_TWO);

	openblack::game_clock::SetTurn(30);
	vortex::ProcessAll(30);
	const auto& queue = VortexOf(in).queue;
	const auto taken = [&queue](entt::entity thing) {
		return std::ranges::any_of(queue, [thing](const auto& record) { return record.object == thing; });
	};
	EXPECT_TRUE(taken(lying));
	EXPECT_TRUE(taken(players));
	EXPECT_TRUE(taken(others));
	EXPECT_FALSE(taken(held));
	EXPECT_EQ(queue.size(), 3u);
}

// Every new way out of a land empties the crossing file as it is made, so only the last one of a land counts, as the
// original (docs/wiki/world/vortex.md#vortex-save-file)
TEST_F(VortexMaking, EveryNewWayOutEmptiesTheCrossingFile)
{
	const ScratchFolder folder("openblack_test_vortex_emptied");
	namespace vortex_save = openblack::ecs::vortex_save;
	const auto file = folder.Path() / "vortex.txt";
	{
		std::ofstream before(file, std::ios::binary);
		before << "CREATE_MOBILE_STATIC(\"0.00,0.00\", 2, 0.000000, 0.000000, 0.000000, 0.000000, 1.000000)\n";
	}
	vortex_save::SetSaveFolder(folder.Path());
	vortex_save::SetWritable(true);
	const auto first = vortex::Create(glm::vec3(100.0f, 0.0f, 100.0f), openblack::VortexType::In);
	ASSERT_NE(VortexOf(first).writer, nullptr);
	EXPECT_EQ(ReadText(file), "");
	vortex_save::Write(*VortexOf(first).writer, "A(1)\n");
	EXPECT_EQ(ReadText(file), "A(1)\n");
	const auto second = vortex::Create(glm::vec3(300.0f, 0.0f, 100.0f), openblack::VortexType::In);
	ASSERT_NE(VortexOf(second).writer, nullptr);
	EXPECT_EQ(ReadText(file), "");
	VortexOf(first).writer.reset();
	VortexOf(second).writer.reset();
}

// A way into a land opens the crossing file for reading as it is made; with no file it has nothing to read, so it only
// makes its new villagers, as the original (docs/wiki/world/vortex.md#vortex-save-file)
TEST_F(VortexMaking, AWayIntoALandReadsTheCrossingFileAsItIsMade)
{
	const ScratchFolder folder("openblack_test_vortex_read");
	const openblack::test::ScopedDefaultFileSystem files;
	namespace vortex_save = openblack::ecs::vortex_save;
	vortex_save::SetSaveFolder(folder.Path());
	// no file: a reader with no line
	const auto bare = vortex::Create(glm::vec3(100.0f, 0.0f, 100.0f), openblack::VortexType::Out);
	ASSERT_NE(VortexOf(bare).reader, nullptr);
	EXPECT_TRUE(VortexOf(bare).reader->lines.empty());
	{
		std::ofstream crossing(folder.Path() / "vortex.txt", std::ios::binary);
		crossing << "A(1)\nB(2)\n";
	}
	const auto out = vortex::Create(glm::vec3(300.0f, 0.0f, 100.0f), openblack::VortexType::Out);
	ASSERT_NE(VortexOf(out).reader, nullptr);
	EXPECT_EQ(VortexOf(out).reader->lines, (std::vector<std::string> {"A(1)", "B(2)"}));
	EXPECT_EQ(VortexOf(out).reader->next, 0u);
}
