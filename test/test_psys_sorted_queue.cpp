/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The Renderer side of the PSys draw paths (Graphics/Renderer.cpp DrawPass) against the original's queue: a Sorted
// effect (Draw_(t, 1), fn_00679840) has no Z object, each sprite is one at its own point (LH3DSprite::AddDrawing
// 0x840C70 from 0x67B0D2) and each chain one at its joint n / 2 (fn_0067B380), so a Queued effect (PSysManager::
// AddDrawing 0x6797D0, one Z object at GetOrigin) can be drawn between two sprites of the same Sorted effect. The
// entries are made as DrawPass makes them (zsorter::Key, (x^2 + y^2) + z^2, then zsorter::Queue).

#include <cmath>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Graphics/ZSorter.h"
#include "PSys/PSys.h"
#include "PSys/PSysFile.h"
#include "PSys/PSysManager.h"

using namespace openblack;
using namespace openblack::graphics;
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

std::string CreateRule(const char* name, int group, const char* creator)
{
	return std::string("BEGINCLASS CreateRuleAnAtom ") + name + "\nBEGINPROPERTIES\n" + "PROPERTY Group INTEGER " +
	       std::to_string(group) + "\nPROPERTY NextGroups ARRAY SIZE 0\nPROPERTY PCreator PERSIS_PNTR " + creator +
	       "\nENDPROPERTIES\nENDCLASS\n";
}

/// Group 0: a sprite A, a sprite E with CentreAtBase (raised by height x size x 0.5, 0x67AFAB..0x67AFD6) and two chain
/// joints B, C; one atom each at the effect's origin
std::shared_ptr<const File> SortedFile()
{
	std::string text = "BEGINPROPERTIES\nPROPERTY DeleteOnCloseDown BOOL 1\nPROPERTY Hierarchies ARRAY SIZE 25 0" + Zeros(1) +
	                   "\nPROPERTY InitiallyCreated ARRAY SIZE 25 1" + Zeros(1) +
	                   "\nPROPERTY MaxSpellAge FLOAT 100\nENDPROPERTIES\n"
	                   "BEGINCLASS ParticleSpriteCreator SpriteA\nBEGINPROPERTIES\nPROPERTY InitialScale FLOAT 1\n"
	                   "ENDPROPERTIES\nENDCLASS\n"
	                   "BEGINCLASS ParticleSpriteCreator SpriteE\nBEGINPROPERTIES\nPROPERTY InitialScale FLOAT 2\n"
	                   "PROPERTY CentreAtBase BOOL 1\nENDPROPERTIES\nENDCLASS\n"
	                   "BEGINCLASS ParticleChainCreator Joint\nBEGINPROPERTIES\nPROPERTY InitialScale FLOAT 1\n"
	                   "ENDPROPERTIES\nENDCLASS\n";
	text += CreateRule("A", 0, "SpriteA");
	text += CreateRule("E", 0, "SpriteE");
	text += CreateRule("B", 0, "Joint");
	text += CreateRule("C", 0, "Joint");
	auto file = File::Parse(text, "test_sorted_queue");
	EXPECT_TRUE(file.has_value());
	return file.has_value() ? std::make_shared<const File>(std::move(*file)) : nullptr;
}

uint32_t StartStepped(glm::vec3 origin)
{
	const auto id = manager::Start(SortedFile(), origin, 1.0f);
	EXPECT_NE(id, 0u);
	if (auto* effect = manager::Find(id); effect != nullptr)
	{
		effect->Step(0.1f);
	}
	return id;
}

/// What a drained entry is (Renderer.cpp's ZObject, reduced)
struct Entry
{
	char kind;      ///< 's' a Sorted sprite, 'c' a Sorted chain, 'q' a Queued effect
	float initial;  ///< the sprite's creator's InitialScale (which sprite it is)
	uint32_t effect;
};
} // namespace

TEST(PSysSortedQueue, SpritesAndChainsKeyedOnTheirOwn)
{
	const glm::vec3 camera(0.0f);
	const auto sorted = StartStepped(glm::vec3(0.0f, 0.0f, 10.0f));
	const auto frame = manager::CollectSorted();

	zsorter::Queue<Entry> queue;
	queue.Begin();
	int sprites = 0;
	float keyA = 0.0f;
	float keyE = 0.0f;
	for (const auto& atom : frame.sprites)
	{
		if (atom.effect != sorted)
		{
			continue;
		}
		++sprites;
		// LH3DSprite::AddDrawing 0x840C95..0x840CA3: the sprite's +0/+4/+8, the atom's position raised with CentreAtBase
		glm::vec3 point = atom.atom.position;
		if (atom.atom.creator->centreAtBase)
		{
			point.y += atom.atom.stretch * std::max(atom.atom.scale, 1e-4f) * 0.5f;
		}
		EXPECT_EQ(atom.key, point);
		const float key = zsorter::Key(atom.key, camera);
		EXPECT_FLOAT_EQ(key, (atom.key.x * atom.key.x + atom.key.y * atom.key.y) + atom.key.z * atom.key.z);
		(atom.atom.creator->centreAtBase ? keyE : keyA) = key;
		queue.Submit({'s', atom.atom.creator->initialScale, atom.effect}, key);
	}
	ASSERT_EQ(sprites, 2); // one Z object per sprite, none for the effect
	ASSERT_GT(keyE, keyA); // E raised: farther from a camera below it
	int chains = 0;
	for (const auto& chain : frame.chains)
	{
		if (chain.effect == sorted)
		{
			++chains;
			// fn_0067B380 0x67B389..0x67B3A1: the joint (n - (n >> 31)) >> 1
			ASSERT_EQ(chain.chain.joints.size(), 2u);
			EXPECT_EQ(chain.key, chain.chain.joints[1].position);
			queue.Submit({'c', 0.0f, chain.effect}, zsorter::Key(chain.key, camera));
		}
	}
	EXPECT_EQ(chains, 1);

	// a Queued effect whose origin lies between the two sprites: its single Z object (0x6797E5..0x679834) goes between
	// them, which one Z object for the whole Sorted effect could not give
	const float between = std::sqrt((keyA + keyE) * 0.5f);
	const auto queued = StartStepped(glm::vec3(0.0f, 0.0f, between));
	manager::SetDrawPath(queued, DrawPath::Queued);
	int queuedEntries = 0;
	for (const auto& effect : manager::CollectQueued())
	{
		if (effect.effect == queued)
		{
			++queuedEntries;
			queue.Submit({'q', 0.0f, effect.effect}, zsorter::Key(effect.origin, camera));
		}
	}
	EXPECT_EQ(queuedEntries, 1);
	// the hand's effects are not in the queue (CHand::Draw 0x46D2AE draws them inside the hand's entry)
	for (const auto& effect : manager::HandEffects())
	{
		EXPECT_NE(effect.effect, sorted);
		EXPECT_NE(effect.effect, queued);
	}

	// far to near (fn_0082F280): E, then the Queued effect, then A and the chain at the effect's origin (equal keys: the
	// order they came in, NewZObject 0x83F36A..0x83F376)
	std::vector<Entry> order;
	for (const auto& entry : queue.Drain())
	{
		order.push_back(*entry.item);
	}
	ASSERT_EQ(order.size(), 4u);
	EXPECT_EQ(order[0].kind, 's');
	EXPECT_FLOAT_EQ(order[0].initial, 2.0f); // E
	EXPECT_EQ(order[1].kind, 'q');
	EXPECT_EQ(order[1].effect, queued);
	EXPECT_EQ(order[2].kind, 's');
	EXPECT_FLOAT_EQ(order[2].initial, 1.0f); // A
	EXPECT_EQ(order[3].kind, 'c');

	manager::Delete(sorted);
	manager::Delete(queued);
}
