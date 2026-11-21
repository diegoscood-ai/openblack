/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The fireball's rules (Rules/Fireball.cpp): what the game reads of a ball's atom once the effects have stepped.
// Wiki: docs/wiki/magic/fireball-and-lightning.md#fireball-magic_type-1-3-seed-2-fire.

#include <cstdint>

#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

namespace openblack::particles
{
struct Atom;
struct Collection;
class Modifier;
} // namespace openblack::particles

namespace openblack::particles::fireball_rules
{
/// A thrown ball's direction from the hand's (a unit vector, or zero): its heading kept, pitched up 0.3 rad and kept
/// between -pi/2 and pi/2, as the game's FPU works it out
[[nodiscard]] glm::vec3 PitchedUp(const glm::vec3& d);
/// The size class of a thrown ball's whoosh (1 large, 2 medium, 3 small) from its speed: the speed over 200, more than
/// 0.6 large, more than 0.3 medium
[[nodiscard]] int32_t ThrowSoundSize(float speed);

/// Once the spells have stepped their effects: each ball gets where the hand sees it and its radius from its atom as the
/// step's post-update left it (the drawn position while the atom is drawn and not faint, the rule scale times the base
/// scale). A ball whose atom is gone is skipped
void UpdateHandTargets();

/// UpdateRuleGravityWithFloor's velocity in the wind (the climate's smoothed wind at the atom): towards the wind times
/// WindMagnification times 0.1, by Damping times the difference times dt
[[nodiscard]] glm::vec3 WindVelocity(glm::vec3 velocity, glm::vec3 wind, float windMagnification, float damping, float dt);
/// FadeAlphaWithHeight's alpha at `height` above the land: alphaAtZero below the land, alphaAtRef above
/// refHeight, the straight line between them in between, kept in 0..255 and truncated
[[nodiscard]] uint8_t AlphaWithHeight(float alphaAtZero, float alphaAtRef, float refHeight, float height) noexcept;

/// One place a trail keeps of its parent atom: its position, scale and turn
struct TrailPlace
{
	glm::vec3 position {0.0f};
	float scale {1.0f};
	glm::mat3 rotation {1.0f};
};

/// The places the trail rule keeps for the collection, the newest first; none before its first update
[[nodiscard]] std::vector<TrailPlace> TrailPlaces(const Collection& collection);
/// Puts back the places a trail kept for the collection, the newest first, as a saved game holds them, in a ring of the
/// trail rule's size (the places beyond it dropped); nothing for a rule that is no trail
void RestoreTrailPlaces(const Collection& collection, const Modifier& rule, const std::vector<TrailPlace>& places);

/// What the fireball's rule keeps on an atom, as a save writes it: whether its first update is still to come, the game
/// turn its ball was last found there and the ball (none once it has gone)
struct BallData
{
	bool firstUpdate {true};
	uint32_t validatedTurn {0};
	entt::entity ball {entt::null};
};
/// The fireball's rule's data on the atom, if the rule is that rule and has made its data on the atom
[[nodiscard]] std::optional<BallData> BallDataOf(const Atom& atom, const Modifier& rule);
/// For a load: the fireball's rule's data on the atom as the save kept it, noted as made, its ball (made already) among
/// the balls under a key of its own; false, with nothing changed, for a rule of another class
bool RestoreBallData(Atom& atom, const Modifier& rule, const BallData& data);

/// Whether the sub-collections rule has put its groups under the atom, if the rule is that rule and has made its data on
/// the atom
[[nodiscard]] std::optional<bool> SubCollectionsAddedOf(const Atom& atom, const Modifier& rule);
/// For a load: the sub-collections rule's data on the atom as the save kept it, noted as made; false, with nothing
/// changed, for a rule of another class
bool RestoreSubCollectionsAdded(Atom& atom, const Modifier& rule, bool added);
} // namespace openblack::particles::fireball_rules
