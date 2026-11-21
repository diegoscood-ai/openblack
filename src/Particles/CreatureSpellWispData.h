/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <glm/vec3.hpp>

// The creature spell wisps rule's data, as a save writes it: on its collection, where the creature and the caster's
// hand were as the wisps began and how many are due and made; on each wisp, its angles, their rates and its size
// (Particles/ParticleCreatureSpellRules.cpp). Wiki: docs/wiki/creature/casting.md#the-creature-spells-particles.

namespace openblack::particles
{
struct Atom;
struct Collection;
class Modifier;
} // namespace openblack::particles

namespace openblack::particles::creature_spell
{

/// A wisp made by another rule turns at these rates and keeps this size until the rule has given it its own
inline constexpr float k_DefaultThetaRate = 0.0f;
inline constexpr float k_DefaultPhiRate = 1.0f;
inline constexpr float k_DefaultWispScale = 1.0f;

/// What the rule keeps on its collection: where the creature stood and the caster's hand was as the wisps began (set on
/// its first update), whether that update is still to come, the wisps due (a fraction) and the wisps made
struct WispsData
{
	glm::vec3 creature {0.0f};
	glm::vec3 hand {0.0f};
	bool first {true};
	float due {0.0f};
	int made {0};
};

/// What the rule keeps on a wisp: its angles round and up the box, the rates they turn at, and the size it was made at
struct WispData
{
	float theta {0.0f};
	float phi {0.0f};
	float thetaRate {k_DefaultThetaRate};
	float phiRate {k_DefaultPhiRate};
	float scale {k_DefaultWispScale};
};

/// The rule's data on its collection, if the rule is the wisps rule and has made it there
[[nodiscard]] std::optional<WispsData> WispsOf(const Collection& collection, const Modifier& rule);

/// The rule's data on a wisp, if the rule is the wisps rule and has made it there
[[nodiscard]] const WispData* WispOf(const Atom& atom, const Modifier& rule);

/// For a load: the rule's data on its collection as the save kept it, noted as made; false, with nothing changed, for a
/// rule that is not the wisps rule or is not on the collection
bool RestoreWisps(Collection& collection, const Modifier& rule, const WispsData& data);

/// For a load: the rule's data on a wisp as the save kept it, noted as made; false, with nothing changed, for a rule
/// that is not the wisps rule
bool RestoreWisp(Atom& atom, const Modifier& rule, const WispData& data);

} // namespace openblack::particles::creature_spell
