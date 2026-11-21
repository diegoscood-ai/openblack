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

// A seed graphic's holder effect inside a gesture icon draws its sprites a run at a time and its mists between them, in
// the effect's own order; its meshes and chains draw nothing there, and neither does a sprite laying a glowing shell
TEST(GestureIconEffect, TheSpritesAndTheMistsAreDrawnInTheEffectsOrder)
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
	const auto effect = graphics::IconEffectOf(items, [&mist](const particles::Effect::DrawAtom& atom) {
		return atom.creator == &mist ? std::optional(mists::MistDesc {.position = atom.position}) : std::nullopt;
	});
	ASSERT_EQ(effect.draws.size(), 4u);
	ASSERT_EQ(effect.draws[0].sprites.size(), 2u);
	EXPECT_EQ(effect.draws[0].sprites[1].position.x, 2.0f);
	ASSERT_TRUE(effect.draws[1].mist.has_value());
	EXPECT_EQ(effect.draws[1].mist->position.x, 3.0f);
	ASSERT_EQ(effect.draws[2].sprites.size(), 1u);
	EXPECT_EQ(effect.draws[2].sprites[0].position.x, 5.0f);
	ASSERT_TRUE(effect.draws[3].mist.has_value());
	EXPECT_EQ(effect.draws[3].mist->position.x, 6.0f);
}
