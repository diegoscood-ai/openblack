/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

// The vortex attract rule, the rule of the effect that draws things into a way out of a land (the vortex's ObjectMover,
// Particles/Rules/VortexAttract.cpp). Wiki: docs/wiki/world/vortex.md#taking-objects-in-the-in-vortex.

namespace openblack::particles
{
struct Atom;
class Effect;
class Modifier;
} // namespace openblack::particles

namespace openblack::particles::vortex_attract
{
/// What the rule keeps on each particle it carries, as a save writes it
struct AtomData
{
	/// What the take keeps of the thing's 3D object to give it back: whether it was drawn as paper and faded with the
	/// distance, and its draw importance (the take clears the first two and sets the importance to 10). Always set by a
	/// take or a load
	struct TakenLook
	{
		bool wasPaper {false};
		bool wasDisappearing {false};
		float importance {0.0f};
	};

	/// The velocity the thing was taken with, and its height above the land then
	glm::vec3 takenVelocity {0.0f};
	float startHeight {0.0f};
	bool indestructible {false};
	bool started {false};
	std::optional<TakenLook> look;
	/// The vortex a thing that can't be destroyed is thrown back out of at the middle; only a load gives one
	entt::entity outVortex {entt::null};
};

/// The game object a particle carries, as a save writes it: the thing, the player of the effect that took it, the game
/// turn it was last found available on, whether it has been let go since (found unavailable), and the byte the take
/// clears
struct CarriedObject
{
	entt::entity object {entt::null};
	int player {-1};
	uint32_t availableTurn {0};
	bool letGo {false};
	uint8_t byte {1};
};

/// The rule's data on an atom, if the rule has made it there
[[nodiscard]] const AtomData* DataOf(const Atom& atom, const Modifier& rule);
/// The thing an atom carries, if its particle is a carried game object of this rule
[[nodiscard]] std::optional<CarriedObject> CarriedOf(const Atom& atom);
/// For a load: the atom's particle is the game object it carried, as the save kept it. The thing is not yet the atom's
/// (TakeRestoredCarriers)
void RestoreCarrier(Atom& atom, const CarriedObject& carried);
/// For a load: the rule's data on the atom as the save kept it, noted as made
void RestoreData(Atom& atom, const Modifier& rule, const AtomData& data);
/// For a load, once the effect is made: each carrier it restored takes its thing, carried as the save left it (out of
/// the map cells, in the physics with no body, marked carried), as the original's load reads them back with the thing's
/// own flags
void TakeRestoredCarriers(const Effect& effect);
/// Once a frame: each thing a vortex draws in takes its particle's drawn place, turn and size
void UpdateCarriedObjects();
/// How many things the vortices are drawing in now (the Vortices debug window)
[[nodiscard]] size_t CarriedCount();
} // namespace openblack::particles::vortex_attract
