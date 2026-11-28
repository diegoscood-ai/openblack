/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "Graphics/GestureIconEffect.h"

using namespace openblack;

namespace
{
using Item = particles::Effect::OrderedItem;

Item AtomOf(const particles::Creator& creator, float x)
{
	Item item;
	item.atom.creator = &creator;
	item.atom.position = glm::vec3(x, 0.0f, 0.0f);
	return item;
}
} // namespace

// A seed graphic's holder effect inside a gesture icon draws its sprites a run at a time, its mists and its meshes
// between them, in the effect's own order; its chains draw nothing there, and neither does a sprite laying a glowing
// shell
TEST(GestureIconEffect, TheSpritesMistsAndMeshesAreDrawnInTheEffectsOrder)
{
	particles::Creator sprite;
	sprite.kind = particles::Creator::Kind::Sprite;
	particles::Creator mist;
	mist.kind = particles::Creator::Kind::Other;
	particles::Creator mesh;
	mesh.kind = particles::Creator::Kind::Mesh;
	std::vector<Item> items {AtomOf(sprite, 1.0f), AtomOf(sprite, 2.0f), AtomOf(mist, 3.0f),
	                         AtomOf(mesh, 4.0f),   AtomOf(sprite, 5.0f), AtomOf(mist, 6.0f)};
	items.push_back(AtomOf(sprite, 7.0f));
	items.back().chain = 0;
	items.push_back(AtomOf(sprite, 8.0f));
	items.back().atom.objectGlow.emplace();
	const auto effect = graphics::IconEffectOf(
	    items,
	    [&mist](const particles::Effect::DrawAtom& atom) {
		    return atom.creator == &mist ? std::optional(mists::MistDesc {.position = atom.position}) : std::nullopt;
	    },
	    [](const particles::Effect::DrawAtom& atom) {
		    graphics::GestureIconModelOverlay model;
		    model.matrix[3] = glm::vec4(atom.position, 1.0f);
		    return std::optional(model);
	    });
	ASSERT_EQ(effect.draws.size(), 5u);
	ASSERT_EQ(effect.draws[0].sprites.size(), 2u);
	EXPECT_EQ(effect.draws[0].sprites[1].position.x, 2.0f);
	ASSERT_TRUE(effect.draws[1].mist.has_value());
	EXPECT_EQ(effect.draws[1].mist->position.x, 3.0f);
	ASSERT_TRUE(effect.draws[2].mesh.has_value());
	EXPECT_EQ(effect.draws[2].mesh->matrix[3].x, 4.0f);
	ASSERT_EQ(effect.draws[3].sprites.size(), 1u);
	EXPECT_EQ(effect.draws[3].sprites[0].position.x, 5.0f);
	ASSERT_TRUE(effect.draws[4].mist.has_value());
	EXPECT_EQ(effect.draws[4].mist->position.x, 6.0f);
}

// A holder effect's mesh atom is drawn in the icon as a model of the models' camera: its mesh, matrix, colour, alpha
// and specular, lit as its creator says, with its own alpha when it takes the global alpha; one coloured by the land's
// light tints that light with its colour, as in the world
TEST(GestureIconEffect, AHolderEffectsMeshIsDrawnAsAModelOfTheIcon)
{
	particles::mesh_atoms::Instance instance {.meshId = 7,
	                                          .model = glm::mat4(2.0f),
	                                          .alpha = 0.5f,
	                                          .uv = {0.25f, 0.0f},
	                                          .translucent = true,
	                                          .additive = true,
	                                          .colour = {0x12, 0x34, 0x56},
	                                          .landscapeColour = false,
	                                          .specular = 0xFF102030u,
	                                          .globalAlpha = true};
	instance.lit = false;
	const auto model = graphics::IconMeshOf(instance);
	ASSERT_TRUE(model.has_value());
	EXPECT_EQ(model->mesh, 7u);
	EXPECT_EQ(model->matrix, glm::mat4(2.0f));
	EXPECT_EQ(model->uv, glm::vec2(0.25f, 0.0f));
	EXPECT_EQ(model->alpha, 128);
	EXPECT_EQ(model->colour, 0x123456u);
	EXPECT_EQ(model->specular, 0x102030u);
	EXPECT_FALSE(model->lit);
	EXPECT_TRUE(model->fading);
	EXPECT_FALSE(model->landTint);
	// coloured by the land's light: its colour tints the land's light where it stands, as in the world
	instance.landscapeColour = true;
	const auto tinted = graphics::IconMeshOf(instance);
	ASSERT_TRUE(tinted.has_value());
	EXPECT_TRUE(tinted->landTint);
	EXPECT_EQ(tinted->colour, 0x123456u);
}
