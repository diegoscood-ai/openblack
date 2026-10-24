/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What the renderer draws of a creature and of its leash, made from the components alone: the bodies with their pose
// and shape, the eyes, the hair ribbons, the painted skins, and the leash's rope and shadow

#include <cstdint>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "3D/CreatureBody.h"
#include "3D/ObjectMatrix.h"
#include "Creature/CreatureEyes.h"
#include "Creature/CreatureFizzLook.h"
#include "Creature/CreatureHair.h"
#include "Creature/LeashRope.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/FallingCreature.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/CreatureDraw.h"
#include "Graphics/LeashDraw.h"
#include "Graphics/RenderModes.h"
#include "Graphics/WorldTriangles.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::graphics;
using Appearance = creature::CreatureBody::Appearance;
using creature_draw::EntityInstances;

namespace
{
constexpr float k_Tolerance = 1e-5f;

size_t StorageCount(const Registry& registry)
{
	size_t storages = 0;
	registry.EachStorage([&storages](entt::id_type, const auto&) { ++storages; });
	return storages;
}

float Flat(glm::vec2 /*point*/)
{
	return 2.0f;
}

uint32_t AlphaOf(uint32_t abgr)
{
	return abgr >> 24u;
}

// A rope from a hand at (0, 20, 0) to a collar 30 along x, placed straight
leash_rope::Rope StraightRope()
{
	return leash_rope::Create({0.0f, 20.0f, 0.0f}, {30.0f, 20.0f, 0.0f}, 32.5f, 77.0f, {}, Flat);
}

// A creature of a species with its mesh drawn in a row of the instances
entt::entity AddCreature(Registry& registry, EntityInstances& instances, CreatureType species, uint32_t row)
{
	const auto entity = registry.Create();
	const auto meshId = creature::GetIdFromType(species, Appearance::Base);
	registry.AssignState<Creature>(entity, Creature {.owner = PlayerNames::PLAYER_ONE, .species = species, .mind = 0});
	registry.AssignState<components::Mesh>(entity, components::Mesh {meshId, 0, 0});
	instances.emplace(entity, ecs::systems::RenderContext::EntityInstance {
	                              .meshId = meshId, .index = row, .morphWithTerrain = false, .receivesDynamicShadow = true});
	return entity;
}
} // namespace

// ---- The leash ----------------------------------------------------------------------------------------------------

TEST(LeashDraw, TheRopeTakesTheLightAtEachPointAndIsTransparentAtItsEnds)
{
	const auto rope = StraightRope();
	const glm::vec3 eye {15.0f, 60.0f, -40.0f};
	const auto draw =
	    leash_draw::Build(rope, eye, Flat, [](const glm::vec3& point) { return point.x < 15.0f ? 0xFF336699u : 0x80102030u; });
	const auto ribbon = leash_rope::BuildRibbon(rope, eye, Flat);
	for (size_t i = 0; i < draw.rope.size(); ++i)
	{
		const auto& corner = draw.rope.at(i);
		EXPECT_EQ(corner.position, ribbon.rope.at(i).position);
		EXPECT_EQ(corner.uv, ribbon.rope.at(i).uv);
		// both corners take the light at their point, not at themselves, its alpha included, but the end points
		const auto point = i / 2;
		const bool atEnd = point == 0 || point == leash_rope::k_PointCount - 1;
		const uint32_t light = leash_rope::Point(rope, point).x < 15.0f ? 0xFF336699u : 0x80102030u;
		EXPECT_EQ(corner.abgr, world_triangles::ToAbgr(atEnd ? light & 0x00FFFFFFu : light));
		EXPECT_EQ(corner.specular, 0u);
	}
}

TEST(LeashDraw, BothCornersOfAPointHaveTheSameColour)
{
	const auto rope = StraightRope();
	// a light that differs across the rope: the corners still agree, as the light is taken at the rope's point
	const auto draw = leash_draw::Build(rope, {15.0f, 0.0f, 40.0f}, Flat,
	                                    [](const glm::vec3& point) { return point.y > 20.0f ? 0xFFFFFFFFu : 0xFF000000u; });
	for (size_t point = 0; point < leash_rope::k_PointCount; ++point)
	{
		EXPECT_EQ(draw.rope.at(point * 2).abgr, draw.rope.at((point * 2) + 1).abgr);
	}
}

TEST(LeashDraw, TheShadowIsBlackFaintAndFadesOutAtBothEnds)
{
	const auto rope = StraightRope();
	const auto draw = leash_draw::Build(rope, {15.0f, 60.0f, -40.0f}, Flat, [](const glm::vec3&) { return 0xFFFFFFFFu; });
	for (size_t i = 0; i < draw.shadow.size(); ++i)
	{
		const auto& corner = draw.shadow.at(i);
		const bool atEnd = i < 2 || i >= draw.shadow.size() - 2;
		EXPECT_EQ(AlphaOf(corner.abgr), atEnd ? 0u : leash_rope::k_ShadowAlpha);
		EXPECT_EQ(corner.abgr & 0x00FFFFFFu, 0u);
		EXPECT_NEAR(corner.position.y, 2.0f + leash_rope::k_ShadowLift, k_Tolerance);
	}
}

TEST(LeashDraw, TheRopeAndItsShadowShareOneAlphaTestedMaterialThatWritesDepth)
{
	const auto& material = leash_draw::k_Material;
	EXPECT_EQ(material.mode, render_modes::Mode::TexturedChromaAlpha);
	EXPECT_TRUE(render_modes::Desc(material.mode).zWrite);
	EXPECT_TRUE(render_modes::Desc(material.mode).alphaTest);
	EXPECT_TRUE(material.TwoSided());
	EXPECT_EQ(material.flags & render_modes::k_Tiling, render_modes::k_Tiling);
	EXPECT_EQ(material.alphaRef, 0x38);
	// through the global alpha table the mode stays and its ALPHAREF is scaled by an object alpha of 255: 51
	EXPECT_EQ(leash_draw::k_Table, render_modes::Table::GlobalAlpha);
	const auto drawn = render_modes::Select(material.mode, leash_draw::k_Table);
	EXPECT_EQ(drawn, material.mode);
	EXPECT_EQ(render_modes::AlphaRef(drawn, leash_draw::k_Table, material.alphaRef), 51);
	const auto alpha = render_modes::PrimitiveAlpha(drawn, leash_draw::k_Table, material.alphaRef);
	EXPECT_FLOAT_EQ(alpha.ref, 51.0f / 255.0f);
	EXPECT_EQ(alpha.source, render_modes::AlphaSource::Modulate);
}

TEST(LeashDraw, TheStripHasTheRopesCornersThenTheShadowsAtEachPoint)
{
	const auto rope = StraightRope();
	const auto draw = leash_draw::Build(rope, {15.0f, 60.0f, -40.0f}, Flat, [](const glm::vec3&) { return 0xFF808080u; });
	const auto strip = leash_draw::StripOf(draw);
	ASSERT_EQ(strip.size(), leash_rope::k_PointCount * 4);
	for (size_t point = 0; point < leash_rope::k_PointCount; ++point)
	{
		for (size_t corner = 0; corner < 2; ++corner)
		{
			const auto& rope0 = strip.at((point * 4) + corner);
			const auto& shadow0 = strip.at((point * 4) + 2 + corner);
			EXPECT_EQ(rope0.position, draw.rope.at((point * 2) + corner).position);
			EXPECT_EQ(rope0.abgr, draw.rope.at((point * 2) + corner).abgr);
			EXPECT_EQ(shadow0.position, draw.shadow.at((point * 2) + corner).position);
			EXPECT_EQ(shadow0.abgr, draw.shadow.at((point * 2) + corner).abgr);
		}
	}
}

TEST(LeashDraw, EachSegmentDrawsTheRopesQuadThenTheShadows)
{
	const auto indices = leash_draw::StripIndices();
	// 164 triangles over 168 corners, as the game's one call
	EXPECT_EQ(indices.size(), 164u * 3u);
	EXPECT_EQ(*std::ranges::max_element(indices), leash_draw::k_StripVertexCount - 1);
	const std::array<uint16_t, 12> first {5, 4, 0, 0, 1, 5, 7, 6, 2, 2, 3, 7};
	for (size_t segment = 0; segment + 1 < leash_rope::k_PointCount; ++segment)
	{
		for (size_t i = 0; i < first.size(); ++i)
		{
			EXPECT_EQ(static_cast<size_t>(indices.at((segment * 12) + i)), first.at(i) + (segment * 4)) << segment << " " << i;
		}
		// the first six reach only the rope's corners, the last six only the shadow's
		for (size_t i = 0; i < 12; ++i)
		{
			EXPECT_EQ((indices.at((segment * 12) + i) % 4) >= 2, i >= 6);
		}
	}
}

TEST(LeashDraw, OnlyAWornRopeThatHasBeenPlacedIsDrawn)
{
	CreatureLeash leash;
	EXPECT_FALSE(leash_draw::IsDrawn(leash));
	leash.worn.emplace();
	EXPECT_FALSE(leash_draw::IsDrawn(leash));
	leash.worn->ropeStarted = true;
	EXPECT_TRUE(leash_draw::IsDrawn(leash));
}

TEST(LeashDraw, TheRopesAreThoseDrawnAndNoneWhileHidden)
{
	Registry registry;
	const auto led = registry.Create();
	registry.AssignState<CreatureLeash>(led);
	registry.Get<CreatureLeash>(led).worn.emplace();
	registry.Get<CreatureLeash>(led).worn->ropeStarted = true;
	const auto free = registry.Create();
	registry.AssignState<CreatureLeash>(free);

	const auto ropes = leash_draw::Ropes(std::as_const(registry), true);
	ASSERT_EQ(ropes.size(), 1u);
	EXPECT_EQ(ropes.front(), &registry.Get<CreatureLeash>(led).worn->rope);
	EXPECT_TRUE(leash_draw::Ropes(std::as_const(registry), false).empty());
}

TEST(LeashDraw, ALandWithoutACreatureDrawsNoRopeAndGainsNoStorage)
{
	Registry registry;
	const auto entity = registry.Create();
	registry.AssignState<Transform>(entity, Transform {glm::vec3(1.0f), glm::mat3(1.0f), glm::vec3(1.0f)});
	const auto before = StorageCount(registry);
	EXPECT_TRUE(leash_draw::Ropes(std::as_const(registry), true).empty());
	EXPECT_EQ(StorageCount(registry), before);
}

// ---- The bodies ---------------------------------------------------------------------------------------------------

TEST(CreatureDrawFalling, TheCopyIsDrawnAsItsSpeciesWithItsCreaturesSkins)
{
	Registry registry;
	const creature_draw::MeshBones loaded = [](entt::id_type) { return std::optional<size_t>(0); };
	EXPECT_FALSE(creature_draw::FallingBody(registry, loaded).has_value());
	const auto source = registry.Create();
	const auto copy = registry.Create();
	registry.Assign<components::FallingCreature>(
	    copy, components::FallingCreature {.source = source, .species = CreatureType::Tiger});
	const auto body = creature_draw::FallingBody(registry, loaded);
	ASSERT_TRUE(body.has_value());
	EXPECT_EQ(body->entity, copy);
	EXPECT_EQ(body->paintedSkins, source);
	EXPECT_EQ(body->mesh, creature::GetIdFromType(CreatureType::Tiger, Appearance::Base));
	EXPECT_TRUE(body->bones.empty());
	// no mesh loaded: nothing
	EXPECT_FALSE(creature_draw::FallingBody(registry, [](entt::id_type) { return std::optional<size_t> {}; }).has_value());
}

TEST(CreatureDrawFalling, TheCopysRowSetsItsColourAndSpecular)
{
	const glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 3.0f));
	const auto row = creature_draw::FallingRow(model, 0xFF804000u, 0xFF102030u);
	EXPECT_EQ(row[3], model[3]);
	// a set colour: x = 1 + its rgb, so no land light and no haze; y the specular's rgb
	EXPECT_EQ(argb_colour::InstanceTint(row[4]), std::nullopt);
	EXPECT_GT(row[4].x, 0.5f);
	EXPECT_EQ(row[4].w, 0.0f);
}

TEST(CreatureDrawFalling, AnEyelidOnASetColourIsTheSkinTimesTheColourOver255)
{
	EXPECT_EQ(creature_draw::SetColourLid(0xFFFFFFFFu, 0xFF102030u), 0xFF102030u);
	EXPECT_EQ(creature_draw::SetColourLid(0xFF804000u, 0xFFFFFFFFu), 0xFF804000u);
	// (0x80 x 0x80) / 255 = 64, (0x40 x 0xFF) / 255 = 64, 0
	EXPECT_EQ(creature_draw::SetColourLid(0xFF80FF00u, 0xFF8040FFu), 0xFF404000u);
	EXPECT_EQ(creature_draw::SetColourLid(0x00000000u, 0x00FFFFFFu), 0xFF000000u);
}

TEST(CreatureDrawBodies, ALandWithoutACreatureHasNoBodyAndGainsNoStorage)
{
	Registry registry;
	const auto entity = registry.Create();
	registry.AssignState<Transform>(entity, Transform {glm::vec3(1.0f), glm::mat3(1.0f), glm::vec3(1.0f)});
	const auto before = StorageCount(registry);
	const EntityInstances instances;
	const auto bodies =
	    creature_draw::Bodies(std::as_const(registry), instances, [](entt::id_type) { return std::optional<size_t>(4); });
	EXPECT_TRUE(bodies.empty());
	EXPECT_EQ(StorageCount(registry), before);
}

TEST(CreatureDrawBodies, OnlyCreaturesWithARowAndALoadedMeshAreDrawn)
{
	Registry registry;
	EntityInstances instances;
	const auto drawn = AddCreature(registry, instances, CreatureType::Lion, 7);
	AddCreature(registry, instances, CreatureType::Tiger, 3);
	// a creature with no row this frame
	const auto unseen = registry.Create();
	registry.AssignState<Creature>(unseen,
	                               Creature {.owner = PlayerNames::PLAYER_ONE, .species = CreatureType::Lion, .mind = 0});
	registry.AssignState<components::Mesh>(
	    unseen, components::Mesh {creature::GetIdFromType(CreatureType::Lion, Appearance::Base), 0, 0});

	const auto lion = creature::GetIdFromType(CreatureType::Lion, Appearance::Base);
	const auto bodies = creature_draw::Bodies(std::as_const(registry), instances, [lion](entt::id_type id) {
		return id == lion ? std::optional<size_t>(4) : std::nullopt;
	});
	ASSERT_EQ(bodies.size(), 1u);
	EXPECT_EQ(bodies.front().entity, drawn);
	EXPECT_EQ(bodies.front().instance, 7u);
	EXPECT_EQ(bodies.front().mesh, lion);
	// no animation and no shape yet: the rest pose and the base mesh alone
	EXPECT_TRUE(bodies.front().bones.empty());
	EXPECT_FALSE(bodies.front().morph.has_value());
}

TEST(CreatureDrawBodies, TheBodyTakesItsPoseOnlyWhenItFitsTheMesh)
{
	Registry registry;
	EntityInstances instances;
	const auto entity = AddCreature(registry, instances, CreatureType::Lion, 0);
	registry.AssignState<CreatureAnimation>(entity);
	auto& animation = registry.Get<CreatureAnimation>(entity);
	animation.boneMatrices.assign(4, glm::mat4(2.0f));

	const auto bonesOf = [](size_t count) { return [count](entt::id_type) { return std::optional<size_t>(count); }; };
	auto bodies = creature_draw::Bodies(std::as_const(registry), instances, bonesOf(4));
	ASSERT_EQ(bodies.size(), 1u);
	ASSERT_EQ(bodies.front().bones.size(), 4u);
	EXPECT_EQ(bodies.front().bones.data(), animation.boneMatrices.data());

	bodies = creature_draw::Bodies(std::as_const(registry), instances, bonesOf(5));
	ASSERT_EQ(bodies.size(), 1u);
	EXPECT_TRUE(bodies.front().bones.empty());
}

TEST(CreatureDrawBodies, TheShapeIsBlendedTowardsTheLoadedMeshesByHowFarAlongEachAxis)
{
	Registry registry;
	EntityInstances instances;
	const auto entity = AddCreature(registry, instances, CreatureType::Lion, 0);
	registry.AssignState<CreatureMorph>(
	    entity, CreatureMorph {.shownFatness = 0.5f, .drawn = {.evilGood = 0.5f, .thinFat = -0.25f, .weakStrong = -1.0f}});

	const auto id = [](Appearance appearance) { return creature::GetIdFromType(CreatureType::Lion, appearance); };
	// the good and weak meshes are loaded, the thin one isn't
	const auto bodies = creature_draw::Bodies(std::as_const(registry), instances, [&id](entt::id_type mesh) {
		const bool loaded = mesh == id(Appearance::Base) || mesh == id(Appearance::Good) || mesh == id(Appearance::Weak);
		return loaded ? std::optional<size_t>(4) : std::nullopt;
	});
	ASSERT_EQ(bodies.size(), 1u);
	ASSERT_TRUE(bodies.front().morph.has_value());
	const auto& morph = *bodies.front().morph;
	EXPECT_EQ(morph.meshes.at(0), id(Appearance::Good));
	EXPECT_EQ(morph.meshes.at(1), id(Appearance::Base));
	EXPECT_EQ(morph.meshes.at(2), id(Appearance::Weak));
	EXPECT_EQ(morph.weights, glm::vec3(0.5f, 0.25f, 1.0f));
}

TEST(CreatureDrawBodies, TheBodyIsDrawnAsFizzedAndFrozenAsItsSpellsLeaveIt)
{
	Registry registry;
	EntityInstances instances;
	const auto plain = AddCreature(registry, instances, CreatureType::Lion, 1);
	const auto spelled = AddCreature(registry, instances, CreatureType::Lion, 2);
	registry.AssignState<CreatureSpells>(spelled, CreatureSpells {.freeze = 0.25f, .fizz = 0.6f});
	const auto bodies =
	    creature_draw::Bodies(std::as_const(registry), instances, [](entt::id_type) { return std::optional<size_t>(1); });
	ASSERT_EQ(bodies.size(), 2u);
	ASSERT_NE(creature_draw::Find(bodies, 1), nullptr);
	EXPECT_EQ(creature_draw::Find(bodies, 1)->entity, plain);
	EXPECT_EQ(creature_draw::Find(bodies, 1)->fizz, 0.0f);
	EXPECT_EQ(creature_draw::Find(bodies, 1)->freeze, 0.0f);
	ASSERT_NE(creature_draw::Find(bodies, 2), nullptr);
	EXPECT_EQ(creature_draw::Find(bodies, 2)->fizz, 0.6f);
	EXPECT_EQ(creature_draw::Find(bodies, 2)->freeze, 0.25f);
}

TEST(CreatureDrawFizz, TheDepthIsDrawnAloneWhereTheStaticReachesTheLevelBackFacesCulled)
{
	// A creature drawn in mode 15 with the usual Z test, through the normal table at its full alpha
	const creature_draw::PassState own {
	    .mode = render_modes::Mode::TexturedChromaAlpha,
	    .options = {.zFunc = render_modes::ZFunc::LessEqual, .cull = render_modes::Cull::None, .msaa = true},
	    .table = render_modes::Table::Normal,
	    .globalAlpha = 255,
	};
	const auto passes = creature_draw::FizzPasses(own, 155, glm::vec2(0.25f, 0.5f), render_modes::Cull::Ccw);
	const auto& depth = passes[0];
	ASSERT_TRUE(depth.state.mode.has_value());
	EXPECT_EQ(*depth.state.mode, render_modes::Mode::ChromaDepthOnly);
	EXPECT_EQ(depth.state.options.cull, render_modes::Cull::Ccw);
	EXPECT_EQ(depth.state.options.zFunc, render_modes::ZFunc::LessEqual);
	EXPECT_TRUE(depth.state.options.msaa);
	EXPECT_EQ(depth.state.table, render_modes::Table::Normal);
	EXPECT_EQ(depth.state.globalAlpha, 255);
	// the static's slide, then the depth pass, then ALPHAREF: the level itself
	EXPECT_EQ(depth.spellLook, glm::vec4(0.25f, 0.5f, 1.0f, 155.0f / 255.0f));
}

TEST(CreatureDrawFizz, TheBodyIsDrawnInItsOwnModesAtTheAlphaTheLevelLeavesOverThatDepth)
{
	const creature_draw::PassState own {
	    .mode = std::nullopt,
	    .options = {.zFunc = render_modes::ZFunc::LessEqual, .cull = render_modes::Cull::None},
	    .table = render_modes::Table::Normal,
	    .globalAlpha = 255,
	};
	const auto passes = creature_draw::FizzPasses(own, 155, glm::vec2(0.25f, 0.5f), render_modes::Cull::Ccw);
	const auto& body = passes[1];
	EXPECT_FALSE(body.state.mode.has_value());
	EXPECT_EQ(body.state.options.cull, render_modes::Cull::None);
	EXPECT_EQ(body.state.options.zFunc, render_modes::ZFunc::Equal);
	EXPECT_EQ(body.state.table, render_modes::Table::GlobalAlpha);
	// the alpha byte 255 - level: 100 at 155
	EXPECT_EQ(body.state.globalAlpha, 100);
	EXPECT_EQ(body.spellLook, glm::vec4(0.0f, 0.0f, 2.0f, 100.0f / 255.0f));
}

TEST(CreatureDrawFizz, TheAlphaByteIsWhatTheLevelLeavesOfAWholeByte)
{
	const creature_draw::PassState own {};
	for (const uint8_t level : {uint8_t {1}, uint8_t {5}, uint8_t {128}, uint8_t {254}})
	{
		const auto passes = creature_draw::FizzPasses(own, level, glm::vec2(0.0f), render_modes::Cull::Cw);
		EXPECT_EQ(passes[1].state.globalAlpha, 255 - level);
		EXPECT_EQ(passes[0].spellLook.w, static_cast<float>(level) / 255.0f);
		EXPECT_EQ(passes[0].state.options.cull, render_modes::Cull::Cw);
	}
}

TEST(CreatureDrawFizz, TheDepthCarriesTheStaticAsItHasSlidTheBodyNone)
{
	// Two and a half seconds of game time: a quarter across and half down, from where it was made
	const auto scroll = creature_fizz_look::Scroll(glm::vec2(0.0f), 2.5f);
	const auto passes = creature_draw::FizzPasses({}, 64, scroll, render_modes::Cull::Ccw);
	EXPECT_FLOAT_EQ(passes[0].spellLook.x, 0.25f);
	EXPECT_FLOAT_EQ(passes[0].spellLook.y, 0.5f);
	EXPECT_EQ(glm::vec2(passes[1].spellLook), glm::vec2(0.0f));
}

TEST(CreatureDrawBodies, ABodyIsFoundByItsRow)
{
	Registry registry;
	EntityInstances instances;
	const auto second = AddCreature(registry, instances, CreatureType::Lion, 9);
	const auto first = AddCreature(registry, instances, CreatureType::Lion, 2);
	const auto bodies =
	    creature_draw::Bodies(std::as_const(registry), instances, [](entt::id_type) { return std::optional<size_t>(1); });
	ASSERT_EQ(bodies.size(), 2u);
	ASSERT_NE(creature_draw::Find(bodies, 2), nullptr);
	EXPECT_EQ(creature_draw::Find(bodies, 2)->entity, first);
	ASSERT_NE(creature_draw::Find(bodies, 9), nullptr);
	EXPECT_EQ(creature_draw::Find(bodies, 9)->entity, second);
	EXPECT_EQ(creature_draw::Find(bodies, 5), nullptr);
}

TEST(CreatureDrawBodies, TheCreatureRoomsCopyIsDrawnAsItsCreaturesSpeciesWithItsCreaturesSkins)
{
	Registry registry;
	EntityInstances instances;
	const auto creature = AddCreature(registry, instances, CreatureType::Lion, 4);
	// the copy is no creature and has no mesh of its own: its row is in its creature's mesh
	const auto copy = registry.Create();
	registry.AssignState<CreatureCopy>(copy, CreatureCopy {.source = creature, .species = CreatureType::Tiger});
	registry.AssignState<CreatureMorph>(copy,
	                                    CreatureMorph {.shownFatness = 0.5f, .drawn = {.evilGood = 0.5f, .thinFat = 0.0f}});
	const auto tiger = creature::GetIdFromType(CreatureType::Tiger, Appearance::Base);
	instances.emplace(copy, ecs::systems::RenderContext::EntityInstance {
	                            .meshId = tiger, .index = 2, .morphWithTerrain = false, .receivesDynamicShadow = false});

	const auto bodies =
	    creature_draw::Bodies(std::as_const(registry), instances, [](entt::id_type) { return std::optional<size_t>(4); });
	ASSERT_EQ(bodies.size(), 2u);
	const auto* drawn = creature_draw::Find(bodies, 2);
	ASSERT_NE(drawn, nullptr);
	EXPECT_EQ(drawn->entity, copy);
	EXPECT_EQ(drawn->mesh, tiger);
	// its shape towards its species' meshes, and its creature's painted skins
	ASSERT_TRUE(drawn->morph.has_value());
	EXPECT_EQ(drawn->morph->meshes.at(0), creature::GetIdFromType(CreatureType::Tiger, Appearance::Good));
	EXPECT_EQ(drawn->paintedSkins, creature);
	// a creature is drawn with its own
	ASSERT_NE(creature_draw::Find(bodies, 4), nullptr);
	EXPECT_EQ(creature_draw::Find(bodies, 4)->paintedSkins, creature);
}

// ---- The eyes -----------------------------------------------------------------------------------------------------

TEST(CreatureDrawEyes, EachEyeballComesBeforeItsEyelidInTheSkinsColour)
{
	CreatureEyes eyes;
	eyes.drawn.at(0) = {.eyeball = glm::mat4(1.0f), .eyelid = glm::mat4(2.0f)};
	eyes.drawn.at(1) = {.eyeball = glm::mat4(3.0f), .eyelid = glm::mat4(4.0f)};
	eyes.lidColour = 0xFFF08000u;
	const auto parts = creature_draw::Eyes(eyes, [](entt::id_type) { return true; });
	ASSERT_EQ(parts.size(), 4u);
	EXPECT_EQ(parts.at(0).mesh, CreatureEyes::k_EyeballMeshId);
	EXPECT_FALSE(parts.at(0).tint.has_value());
	EXPECT_EQ(parts.at(1).mesh, CreatureEyes::k_EyelidMeshId);
	EXPECT_EQ(parts.at(1).model, glm::mat4(2.0f));
	EXPECT_EQ(parts.at(1).tint, std::optional<uint32_t>(0xFFF08000u));
	// the body's colour multiplies the first lid drawn once and the second twice
	EXPECT_EQ(parts.at(1).lidTimes, 1u);
	EXPECT_EQ(parts.at(3).lidTimes, 2u);
	EXPECT_EQ(parts.at(2).model, glm::mat4(3.0f));
	// with the first eye's lid not drawn, the second's is the first
	eyes.drawn.at(0).eyelid.reset();
	const auto one = creature_draw::Eyes(eyes, [](entt::id_type) { return true; });
	ASSERT_EQ(one.size(), 3u);
	EXPECT_EQ(one.at(2).lidTimes, 1u);
}

TEST(CreatureDrawEyes, AnEyelidsColourIsTheSkinTimesTheBodysAsTheOriginals)
{
	// the original's answers (skin or the first lid, the body's colour, the lid's colour)
	struct Case
	{
		uint32_t lid;
		uint32_t body;
		uint32_t expected;
	};
	constexpr Case k_Cases[] = {
	    {0x612E7696u, 0xA6CECC1Bu, 0xFF255E0Fu}, {0xB3FA7AA7u, 0xE1FAB9D7u, 0xFFF5588Cu},
	    {0x47FC816Au, 0xC16E2284u, 0xFF6C1136u}, {0x8CDECE75u, 0x921EBCE6u, 0xFF1A9769u},
	    {0xCEEA590Bu, 0x05373B76u, 0xFF321405u}, {0x61E1E80Du, 0xD9DB30AFu, 0xFFC12B08u},
	};
	for (const auto& c : k_Cases)
	{
		EXPECT_EQ(creature_draw::LidColour(c.lid, c.body, 1), c.expected) << std::hex << c.lid;
	}
	// the second lid takes the first's
	EXPECT_EQ(creature_draw::LidColour(0xFFF0F0F0u, 0xFF898989u, 2),
	          creature_draw::LidColour(creature_draw::LidColour(0xFFF0F0F0u, 0xFF898989u, 1), 0xFF898989u, 1));
	EXPECT_EQ(creature_draw::LidColour(0xFFF0F0F0u, 0xFF898989u, 2), 0xFF444444u);
	EXPECT_EQ(creature_draw::LidColour(0x12345678u, 0xFFFFFFFFu, 0), 0x12345678u);
}

TEST(CreatureDrawEyes, AnEyeMeshNotLoadedIsLeftOut)
{
	CreatureEyes eyes;
	eyes.drawn.at(0) = {.eyeball = glm::mat4(1.0f), .eyelid = glm::mat4(2.0f)};
	const auto parts = creature_draw::Eyes(eyes, [](entt::id_type id) { return id == CreatureEyes::k_EyelidMeshId; });
	ASSERT_EQ(parts.size(), 1u);
	EXPECT_EQ(parts.front().mesh, CreatureEyes::k_EyelidMeshId);
}

TEST(CreatureDrawEyes, AnEyelidsRowIsTheBodysWithItsColourSet)
{
	glm::mat4 body(1.0f);
	body[3] = glm::vec4(10.0f, 20.0f, 30.0f, 1.0f);
	body[0].w = 0.25f;
	const glm::vec4 colours {0.0f, 7.0f, 0.0f, 0.0f};
	const auto eyeball = creature_draw::EyeRow(body, colours, std::nullopt, 0xFF102030u);
	EXPECT_EQ(eyeball.at(0), body[0]);
	EXPECT_EQ(eyeball.at(3), body[3]);
	EXPECT_EQ(eyeball.at(4), colours);

	// an eyelid's colour is set instead of the land's light, with the body's specular
	const auto eyelid = creature_draw::EyeRow(body, colours, 0xFF804020u, 0xFF102030u);
	glm::vec4 expected = colours;
	argb_colour::PackInstanceColour(expected, 0xFF804020u);
	argb_colour::PackInstanceSpecular(expected, 0xFF102030u);
	EXPECT_EQ(eyelid.at(4), expected);
	EXPECT_EQ(eyelid.at(0), body[0]);
}

TEST(CreatureDrawEyes, AnEyeIsDrawnWhereItWasPlaced)
{
	// The creature in its pen on Land 1, drawn small and fading, with a texture offset in the row too
	const glm::vec3 home {1895.97f, 30.0f, 2520.75f};
	const auto heading = glm::mat3(glm::rotate(glm::mat4(1.0f), 1.0f, glm::vec3(0.0f, 1.0f, 0.0f)));
	auto body = affine::Model(home, heading, glm::vec3(0.22f));
	body[0].w = 0.5f;
	body[1].w = 0.75f;
	const auto eye =
	    creature_eyes::ToMatrix(creature_eyes::EyeballFrame(home + glm::vec3(0.0f, 2.0f, 0.5f), {0.0f, 0.0f, 1.0f}), 0.0388f);

	// The row as the shader rebuilds it: the w components dropped, the origin's w 1
	const glm::mat4 shaderRow(glm::vec4(glm::vec3(body[0]), 0.0f), glm::vec4(glm::vec3(body[1]), 0.0f),
	                          glm::vec4(glm::vec3(body[2]), 0.0f), glm::vec4(glm::vec3(body[3]), 1.0f));
	const auto drawn = shaderRow * creature_draw::InBodySpace(body, eye);
	for (int column = 0; column < 4; ++column)
	{
		for (int element = 0; element < 4; ++element)
		{
			EXPECT_NEAR(drawn[column][element], eye[column][element], 1e-3f) << column << ", " << element;
		}
	}

	// The world matrix itself under the body's row puts the eye hundreds of metres away
	const auto doubled = shaderRow * eye;
	EXPECT_GT(glm::distance(glm::vec3(doubled[3]), glm::vec3(eye[3])), 600.0f);
}

// ---- The hair -----------------------------------------------------------------------------------------------------

TEST(CreatureDrawHair, EachStrandIsARibbonInItsTuftsLitColour)
{
	CreatureHair hair;
	CreatureHair::Group textured {.colour = {200, 100, 50}, .halfWidth = 0.5f, .textured = true};
	textured.strands.push_back({.positions = {{0, 0, 0}, {0, 1, 0}, {0, 2, 0}}, .velocities = {}});
	textured.strands.push_back({.positions = {{1, 0, 0}, {1, 1, 0}}, .velocities = {}});
	CreatureHair::Group plain {.colour = {10, 20, 30}, .halfWidth = 0.25f, .textured = false};
	plain.strands.push_back({.positions = {{5, 0, 0}, {5, 1, 0}}, .velocities = {}});
	// a strand of one point has no ribbon
	plain.strands.push_back({.positions = {{6, 0, 0}}, .velocities = {}});
	hair.groups = {textured, plain};

	const glm::vec3 eye {0.0f, 1.0f, -10.0f};
	const glm::ivec3 light {128, 255, 255};
	const glm::ivec3 added {10, 0, 0};
	const auto drawn = creature_draw::BuildHair(hair, eye, light, added);

	ASSERT_EQ(drawn.textured.vertices.size(), 10u);
	ASSERT_EQ(drawn.textured.indices.size(), 18u);
	ASSERT_EQ(drawn.plain.vertices.size(), 4u);
	ASSERT_EQ(drawn.plain.indices.size(), 6u);
	// the second strand's triangles start at its own first corner
	EXPECT_EQ(drawn.textured.indices.at(12), 6u);

	const auto colour = creature_hair::StrandColour(textured.colour, light, added);
	const uint32_t argb = 0xFF000000u | (static_cast<uint32_t>(colour.r) << 16u) | (static_cast<uint32_t>(colour.g) << 8u) |
	                      static_cast<uint32_t>(colour.b);
	EXPECT_EQ(drawn.textured.vertices.front().abgr, world_triangles::ToAbgr(argb));

	std::vector<creature_hair::RibbonVertex> corners(6);
	creature_hair::BuildRibbon(textured.strands.front().positions, eye, textured.halfWidth, corners);
	for (size_t i = 0; i < corners.size(); ++i)
	{
		EXPECT_EQ(drawn.textured.vertices.at(i).position, corners.at(i).position);
		EXPECT_EQ(drawn.textured.vertices.at(i).uv, corners.at(i).uv);
	}
}

TEST(CreatureDrawHair, AHiddenTuftIsNotDrawn)
{
	CreatureHair hair;
	CreatureHair::Group shown {.colour = {200, 100, 50}, .halfWidth = 0.5f, .textured = false};
	shown.strands.push_back({.positions = {{0, 0, 0}, {0, 1, 0}}, .velocities = {}});
	auto hidden = shown;
	hidden.textured = true;
	hidden.shown = false;
	hair.groups = {shown, hidden};
	const auto drawn = creature_draw::BuildHair(hair, {0.0f, 1.0f, -10.0f}, {255, 255, 255}, {0, 0, 0});
	EXPECT_EQ(drawn.plain.vertices.size(), 4u);
	EXPECT_TRUE(drawn.textured.vertices.empty());
	EXPECT_TRUE(drawn.textured.indices.empty());
}

// ---- The painted skins --------------------------------------------------------------------------------------------

namespace
{
struct FakeTexture
{
	int uploads {0};
	uint16_t firstTexel {0};
};

struct SkinsFixture
{
	Registry registry;
	creature_draw::PaintedSkins<FakeTexture> skins;
	int made {0};
	std::optional<int> textureLimit;

	void Update()
	{
		skins.Update(
		    std::as_const(registry),
		    [this]() -> std::unique_ptr<FakeTexture> {
			    if (textureLimit.has_value() && made >= *textureLimit)
			    {
				    return nullptr;
			    }
			    ++made;
			    return std::make_unique<FakeTexture>();
		    },
		    [](FakeTexture& texture, std::span<const uint16_t> texels) {
			    ++texture.uploads;
			    texture.firstTexel = texels.empty() ? 0 : texels.front();
		    });
	}

	entt::entity Paint(std::vector<CreatureSkin::Skin> painted, uint32_t revision)
	{
		const auto entity = registry.Create();
		registry.AssignState<CreatureSkin>(entity);
		auto& skin = registry.Get<CreatureSkin>(entity);
		skin.skins = std::move(painted);
		skin.revision = revision;
		return entity;
	}
};
} // namespace

TEST(CreatureDrawSkins, ASkinIsUploadedOnceUntilItIsPaintedAgain)
{
	SkinsFixture fixture;
	const auto entity = fixture.Paint({{.id = 11, .texels = {5}}, {.id = 12, .texels = {6}}}, 1);
	fixture.Update();
	fixture.Update();
	ASSERT_NE(fixture.skins.Find(entity, 11), nullptr);
	EXPECT_EQ(fixture.skins.Find(entity, 11)->uploads, 1);
	EXPECT_EQ(fixture.skins.Find(entity, 12)->firstTexel, 6u);
	EXPECT_EQ(fixture.skins.Find(entity, 13), nullptr);
	EXPECT_EQ(fixture.made, 2);

	auto& skin = fixture.registry.Get<CreatureSkin>(entity);
	skin.skins.at(0).texels = {9};
	skin.revision = 2;
	fixture.Update();
	EXPECT_EQ(fixture.skins.Find(entity, 11)->uploads, 2);
	EXPECT_EQ(fixture.skins.Find(entity, 11)->firstTexel, 9u);
	// the textures are kept, only painted again
	EXPECT_EQ(fixture.made, 2);
}

TEST(CreatureDrawSkins, ACreatureGoneTakesItsSkinsWithIt)
{
	SkinsFixture fixture;
	const auto gone = fixture.Paint({{.id = 11, .texels = {1}}}, 1);
	const auto kept = fixture.Paint({{.id = 11, .texels = {2}}}, 1);
	fixture.Update();
	EXPECT_EQ(fixture.skins.Size(), 2u);
	fixture.registry.Destroy(gone);
	fixture.Update();
	EXPECT_EQ(fixture.skins.Size(), 1u);
	EXPECT_EQ(fixture.skins.Find(gone, 11), nullptr);
	EXPECT_NE(fixture.skins.Find(kept, 11), nullptr);
}

TEST(CreatureDrawSkins, OutOfTexturesASkinKeepsTheSpeciesOwn)
{
	SkinsFixture fixture;
	fixture.textureLimit = 1;
	const auto entity = fixture.Paint({{.id = 11, .texels = {1}}, {.id = 12, .texels = {2}}}, 1);
	fixture.Update();
	EXPECT_NE(fixture.skins.Find(entity, 11), nullptr);
	EXPECT_EQ(fixture.skins.Find(entity, 12), nullptr);
}

TEST(CreatureDrawSkins, ALandWithoutACreatureHasNoSkinsAndGainsNoStorage)
{
	SkinsFixture fixture;
	const auto entity = fixture.registry.Create();
	fixture.registry.AssignState<Transform>(entity, Transform {glm::vec3(1.0f), glm::mat3(1.0f), glm::vec3(1.0f)});
	const auto before = StorageCount(fixture.registry);
	fixture.Update();
	EXPECT_EQ(fixture.skins.Size(), 0u);
	EXPECT_EQ(fixture.made, 0);
	EXPECT_EQ(StorageCount(fixture.registry), before);
}
