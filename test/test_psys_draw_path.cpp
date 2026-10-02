/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The three draw paths of a PSys effect (psys::DrawPath, PSys/PSysManager.h; dev\tmp_dis\miracles\polish\
// psys_draw_paths_verdict.md): Draw_(t, 1) = Sorted (fn_00679840, +0xAE = 1 at 0x67984E), AddDrawing = Queued
// (0x6797D0, +0xAE = 0 at 0x6797DE), Draw_(t, 0) = Immediate (the hand, 0x46E76A); and the order fn_006798B0 walks an
// effect's atoms in (0x6798B0..0x679912): the collection's atoms, its chain, then each atom's child collections.

#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "PSys/PSys.h"
#include "PSys/PSysFile.h"
#include "PSys/PSysManager.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
std::string Zeros(int from)
{
	std::string zeros;
	for (int i = from; i < 25; ++i)
	{
		zeros += " 0";
	}
	return zeros;
}

std::string CreateRule(const char* name, int group, const char* creator, const char* nextGroups)
{
	return std::string("BEGINCLASS CreateRuleAnAtom ") + name + "\nBEGINPROPERTIES\n" +
	       "PROPERTY Group INTEGER " + std::to_string(group) + "\n" + "PROPERTY NextGroups ARRAY " + nextGroups + "\n" +
	       "PROPERTY PCreator PERSIS_PNTR " + creator + "\n" + "ENDPROPERTIES\nENDCLASS\n";
}

/// Group 0 (initially created): a sprite A whose atom opens group 1, two chain joints B and C, a point D; group 1: a
/// sprite E. Each create rule makes one atom at its first step, in file order.
std::shared_ptr<const File> OrderFile()
{
	std::string text = "BEGINPROPERTIES\n"
	                   "PROPERTY DeleteOnCloseDown BOOL 1\n"
	                   "PROPERTY Hierarchies ARRAY SIZE 25 0" +
	                   Zeros(1) +
	                   "\n"
	                   "PROPERTY InitiallyCreated ARRAY SIZE 25 1" +
	                   Zeros(1) +
	                   "\n"
	                   "PROPERTY MaxSpellAge FLOAT 100\n"
	                   "ENDPROPERTIES\n"
	                   "BEGINCLASS ParticleSpriteCreator SpriteA\nBEGINPROPERTIES\nPROPERTY InitialScale FLOAT 1\n"
	                   "ENDPROPERTIES\nENDCLASS\n"
	                   "BEGINCLASS ParticleSpriteCreator SpriteE\nBEGINPROPERTIES\nPROPERTY InitialScale FLOAT 2\n"
	                   "ENDPROPERTIES\nENDCLASS\n"
	                   "BEGINCLASS ParticleChainCreator Joint\nBEGINPROPERTIES\nPROPERTY InitialScale FLOAT 1\n"
	                   "ENDPROPERTIES\nENDCLASS\n"
	                   "BEGINCLASS ParticlePointCreator Point\nBEGINPROPERTIES\nPROPERTY InitialScale FLOAT 1\n"
	                   "ENDPROPERTIES\nENDCLASS\n";
	text += CreateRule("A", 0, "SpriteA", "SIZE 1 1");
	text += CreateRule("B", 0, "Joint", "SIZE 0");
	text += CreateRule("C", 0, "Joint", "SIZE 0");
	text += CreateRule("D", 0, "Point", "SIZE 0");
	text += CreateRule("E", 1, "SpriteE", "SIZE 0");
	auto file = File::Parse(text, "test_draw_path");
	EXPECT_TRUE(file.has_value());
	return file.has_value() ? std::make_shared<const File>(std::move(*file)) : nullptr;
}

/// An effect of the manager stepped twice (the child collection is made by A's first step, its atom by the next)
uint32_t StartStepped(glm::vec3 origin)
{
	const auto id = manager::Start(OrderFile(), origin, 1.0f);
	EXPECT_NE(id, 0u);
	auto* effect = manager::Find(id);
	if (effect != nullptr)
	{
		effect->Step(0.1f);
		effect->Step(0.1f);
	}
	return id;
}

bool InSorted(const manager::SortedFrame& frame, uint32_t id)
{
	for (const auto& atom : frame.sprites)
	{
		if (atom.effect == id)
		{
			return true;
		}
	}
	return false;
}

bool InList(const std::vector<manager::OrderedEffect>& effects, uint32_t id)
{
	for (const auto& effect : effects)
	{
		if (effect.effect == id)
		{
			return true;
		}
	}
	return false;
}
} // namespace

TEST(PSysDrawPath, DefaultIsSorted)
{
	// Spell::Draw 0x720441 (push 1; vt+0x108 -> Draw_(t, 1)) and every Draw_(1) site: Start and StartForSpell
	const auto id = manager::Start(OrderFile(), glm::vec3(0.0f), 1.0f);
	ASSERT_NE(id, 0u);
	EXPECT_EQ(manager::GetDrawPath(id), DrawPath::Sorted);
	manager::Delete(id);
	EXPECT_EQ(manager::GetDrawPath(id), DrawPath::Sorted); // a missing id
}

TEST(PSysDrawPath, SpotVisualContainers)
{
	// fn_0063E0F0 0x63E190: container +0x38 = (GSpotVisualInfo +0x4C == 1); fn_0063E240: AddDrawing 0x63E26A, else
	// Draw_(1) 0x63E277
	EXPECT_EQ(manager::SpotVisualDrawPath(1), DrawPath::Queued);
	EXPECT_EQ(manager::SpotVisualDrawPath(0), DrawPath::Sorted);
	EXPECT_EQ(manager::SpotVisualDrawPath(2), DrawPath::Sorted);
}

TEST(PSysDrawPath, EachPathInItsOwnList)
{
	const auto sorted = StartStepped(glm::vec3(1.0f, 0.0f, 0.0f));
	const auto queued = StartStepped(glm::vec3(2.0f, 0.0f, 0.0f));
	const auto hand = StartStepped(glm::vec3(3.0f, 0.0f, 0.0f));
	// the seed graphic on a ball / an icon (0x51A2CA) and the hand's effect (0x46E76A)
	manager::SetDrawPath(queued, DrawPath::Queued);
	manager::SetDrawPath(hand, DrawPath::Immediate);
	EXPECT_EQ(manager::GetDrawPath(queued), DrawPath::Queued);
	EXPECT_EQ(manager::GetDrawPath(hand), DrawPath::Immediate);

	const auto frame = manager::CollectSorted();
	EXPECT_TRUE(InSorted(frame, sorted));
	EXPECT_FALSE(InSorted(frame, queued));
	EXPECT_FALSE(InSorted(frame, hand));
	const auto queuedList = manager::CollectQueued();
	EXPECT_FALSE(InList(queuedList, sorted));
	EXPECT_TRUE(InList(queuedList, queued));
	EXPECT_FALSE(InList(queuedList, hand));
	const auto handList = manager::HandEffects();
	EXPECT_FALSE(InList(handList, sorted));
	EXPECT_FALSE(InList(handList, queued));
	EXPECT_TRUE(InList(handList, hand));
	for (const auto& effect : queuedList)
	{
		if (effect.effect == queued)
		{
			// the single Z object's point: GetOrigin (0x6797E5..0x679834)
			EXPECT_FLOAT_EQ(effect.origin.x, 2.0f);
			EXPECT_EQ(effect.path, DrawPath::Queued);
		}
	}

	// the old entry point still gives every effect, now with its path and id
	int seen = 0;
	for (const auto& drawable : manager::Collect())
	{
		if (drawable.effect == sorted || drawable.effect == queued || drawable.effect == hand)
		{
			++seen;
			EXPECT_EQ(drawable.path, manager::GetDrawPath(drawable.effect));
		}
	}
	EXPECT_EQ(seen, 3);

	// a Sorted effect's sprites: one entry each, at the sprite's own point (LH3DSprite::AddDrawing 0x840C70), and its
	// chain at the joint n / 2 (fn_0067B380)
	int sprites = 0;
	for (const auto& atom : frame.sprites)
	{
		if (atom.effect == sorted)
		{
			++sprites;
			EXPECT_EQ(atom.key, atom.atom.position);
		}
	}
	EXPECT_EQ(sprites, 2);
	int chains = 0;
	for (const auto& chain : frame.chains)
	{
		if (chain.effect == sorted)
		{
			++chains;
			ASSERT_EQ(chain.chain.joints.size(), 2u);
			EXPECT_EQ(chain.key, chain.chain.joints[1].position);
			EXPECT_EQ(chain.chain.path, DrawPath::Sorted);
		}
	}
	EXPECT_EQ(chains, 1);

	manager::Delete(sorted);
	manager::Delete(queued);
	manager::Delete(hand);
}

TEST(PSysDrawPath, QueuedOrderIsFn006798B0)
{
	const auto id = StartStepped(glm::vec3(0.0f));
	manager::SetDrawPath(id, DrawPath::Queued);
	const auto list = manager::CollectQueued();
	const manager::OrderedEffect* effect = nullptr;
	for (const auto& e : list)
	{
		if (e.effect == id)
		{
			effect = &e;
		}
	}
	ASSERT_NE(effect, nullptr);
	// the root collection's atoms A and D (the joints B, C are not drawn by DrawAt), then its chain (0x6798CF), then
	// A's child collection with E (0x6798EB..0x67990D)
	ASSERT_EQ(effect->items.size(), 4u);
	EXPECT_EQ(effect->items[0].chain, -1);
	EXPECT_EQ(effect->items[0].atom.creator->kind, Creator::Kind::Sprite);
	EXPECT_FLOAT_EQ(effect->items[0].atom.creator->initialScale, 1.0f); // A
	EXPECT_EQ(effect->items[1].atom.creator->kind, Creator::Kind::Point); // D
	EXPECT_EQ(effect->items[2].chain, 0);                                 // B-C
	ASSERT_EQ(effect->chains.size(), 1u);
	EXPECT_EQ(effect->chains[0].joints.size(), 2u);
	EXPECT_EQ(effect->chains[0].effect, id);
	EXPECT_EQ(effect->items[3].atom.creator->kind, Creator::Kind::Sprite);
	EXPECT_FLOAT_EQ(effect->items[3].atom.creator->initialScale, 2.0f); // E
	manager::Delete(id);
}
