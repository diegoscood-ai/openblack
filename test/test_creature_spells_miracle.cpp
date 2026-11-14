/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Creature/CreatureSpells.h"

using namespace openblack;
using namespace openblack::creature_spells;

namespace
{
/// The value a spell pulls after its ease and after its finish
struct PullAndPutBack
{
	float eased;
	float finished;
};

/// Runs one spell through its start, an ease of the given ratio, and its finish, on a body with the given strength and
/// alignment; something else moves the value while the spell holds
PullAndPutBack RunSpell(Spell spell, float strength, float alignment, float ratio)
{
	Slot slot;
	BodyValues body {.size = 1.0f, .strength = strength, .fatness = 0.5f, .alignment = alignment};
	SpellLook look;
	const bool pullsStrength = spell == Spell::Weak || spell == Spell::Strong;
	float& value = pullsStrength ? body.strength : body.alignment;

	static_cast<void>(Apply({.spell = spell, .event = Event::Start, .ratio = 0.0f}, slot, body, look, nullptr));
	static_cast<void>(Apply({.spell = spell, .event = Event::Ease, .ratio = ratio}, slot, body, look, nullptr));
	const float eased = value;
	value = 0.42f;
	static_cast<void>(Apply({.spell = spell, .event = Event::Finish, .ratio = 0.0f}, slot, body, look, nullptr));
	return {.eased = eased, .finished = value};
}
} // namespace

// The original keeps the creature's strength as the weak and strong spells start, eases it from there towards a tenth
// (weak) or full strength (strong) by the spell's ratio, and sets the kept strength back as they finish
TEST(CreatureSpellsMiracle, WeakAndStrongPullTheStrengthToATenthOrFullAndPutItBack)
{
	const auto weak = RunSpell(Spell::Weak, 0.6f, 0.0f, 1.0f);
	EXPECT_FLOAT_EQ(weak.eased, 0.1f);
	EXPECT_FLOAT_EQ(weak.finished, 0.6f);

	const auto halfWeak = RunSpell(Spell::Weak, 0.6f, 0.0f, 0.5f);
	EXPECT_FLOAT_EQ(halfWeak.eased, 0.35f);
	EXPECT_FLOAT_EQ(halfWeak.finished, 0.6f);

	const auto strong = RunSpell(Spell::Strong, 0.3f, 0.0f, 1.0f);
	EXPECT_FLOAT_EQ(strong.eased, 1.0f);
	EXPECT_FLOAT_EQ(strong.finished, 0.3f);

	const auto quarterStrong = RunSpell(Spell::Strong, 0.2f, 0.0f, 0.25f);
	EXPECT_FLOAT_EQ(quarterStrong.eased, 0.4f);
	EXPECT_FLOAT_EQ(quarterStrong.finished, 0.2f);
}

// The original keeps the creature's alignment as compassion and anger start, eases it from there towards fully good
// (1) or fully evil (-1) by the spell's ratio, and sets the kept alignment back as they finish
TEST(CreatureSpellsMiracle, CompassionAndAngerSwingTheAlignmentFullyAndPutItBack)
{
	const auto nice = RunSpell(Spell::Nice, 0.5f, -0.2f, 1.0f);
	EXPECT_FLOAT_EQ(nice.eased, 1.0f);
	EXPECT_FLOAT_EQ(nice.finished, -0.2f);

	const auto halfNice = RunSpell(Spell::Nice, 0.5f, -0.2f, 0.5f);
	EXPECT_FLOAT_EQ(halfNice.eased, 0.4f);
	EXPECT_FLOAT_EQ(halfNice.finished, -0.2f);

	const auto nasty = RunSpell(Spell::Nasty, 0.5f, 0.3f, 1.0f);
	EXPECT_FLOAT_EQ(nasty.eased, -1.0f);
	EXPECT_FLOAT_EQ(nasty.finished, 0.3f);

	const auto halfNasty = RunSpell(Spell::Nasty, 0.5f, 0.3f, 0.5f);
	EXPECT_FLOAT_EQ(halfNasty.eased, -0.35f);
	EXPECT_FLOAT_EQ(halfNasty.finished, 0.3f);
}
