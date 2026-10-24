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
#include <span>
#include <vector>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Enums.h"

/// The rules of disciples: a villager held in the player's hand takes up the work of what it is held near, a field, a
/// tree, a fish farm, a building to build, a house to move into, another town to preach to or trade with, a villager to
/// breed with, and keeps at it once put down. Free of the game's state so they can be tested on their own. What each
/// kind of disciple does once made is the disciple table (ECS/Villager/VillagerDisciple.h).
namespace openblack::disciple
{

/// Things are looked for in this many map cells round the held villager, its own cell first
inline constexpr size_t k_CellsSearched = 9;
/// How far from a thing's edge its interest falls away to nothing, in metres
inline constexpr float k_InterestReach = 6.0f;
/// A thing of less interest than this isn't looked at at all
inline constexpr float k_LeastInterest = 0.001f;

/// How much a thing held near draws the villager: the game's distance falloff of the gap between their edges (none when
/// they overlap) out to six metres, times how much its kind draws villagers
[[nodiscard]] float Interest(float edgeGap, float kindDesire);

/// The things worth looking at, in the order they are looked at: the most interesting first, those of equal interest in
/// the order they were found. Things of too little interest are left out.
[[nodiscard]] std::vector<size_t> OrderByInterest(std::span<const float> interests);

/// What a thing near the held villager is, as the choice reads it
enum class NearKind : uint8_t
{
	/// Any building of a town other than those below, such as a creche, a graveyard, a wonder or the totem
	Building,
	/// A house people live in
	House,
	StoragePit,
	TownCentre,
	Workshop,
	WorshipSite,
	Field,
	Tree,
	/// A wood the land is laid out with
	Forest,
	FishFarm,
	Villager,
};

/// What the choice needs to know of a thing held near
struct NearFacts
{
	NearKind kind {NearKind::Building};
	/// The player the thing belongs to, its town's; none for a thing of no town
	std::optional<PlayerNames> owner;
	/// Whether it belongs to a town, and whether that town can be lived in
	bool hasTown {false};
	bool townHabitable {true};
	/// A building or field: whether it is built up, whether it is whole, and whether it works (built and not broken)
	bool built {true};
	bool repaired {true};
	bool functional {true};
	/// A house: whether it has room for another adult, and whether it is the held villager's own home
	bool roomForAdults {false};
	bool heldVillagersHome {false};
	/// Whether it is of the held villager's town
	bool heldVillagersTown {false};
	/// A town centre: whether its town has room for another villager
	bool townHasRoom {false};
	/// A worship site: whether it is the held villager's town's
	bool heldVillagersWorshipSite {false};
	/// Another villager: whether it is about its life, of an age to make love, and a man
	bool villagerFunctional {false};
	bool villagerSexuallyActive {false};
	bool villagerMale {false};
};

/// What the choice needs to know of the held villager and the hand holding it
struct HeldFacts
{
	/// The player of the hand holding it, and its own player
	std::optional<PlayerNames> hand;
	std::optional<PlayerNames> villager;
	bool sexuallyActive {false};
	bool male {false};
	bool child {false};
	/// A script holds it, it isn't about its life, it is dead or dying: it takes up no work but worship (for a child)
	bool otherwiseRefused {false};
};

/// The work a thing offers the held villager, if any
[[nodiscard]] VillagerDisciple Offered(const NearFacts& near, const HeldFacts& held);

/// The work the held villager takes up, and which of the things looked at (by its index there) offered it
struct Choice
{
	VillagerDisciple type {VillagerDisciple::None};
	std::optional<size_t> from;
};

/// Each game turn in the hand the villager takes up the work of the first thing, in the order looked at, that offers it
/// some it can take. A child, or one otherwise refused, takes up only worship, and only a child that. With nothing
/// offering, it goes back to the work it had when it was picked up.
[[nodiscard]] Choice ChooseWhileHeld(std::span<const NearFacts> ordered, const HeldFacts& held, VillagerDisciple atPickUp);

/// The work a villager is taken to have had as it is picked up: a missionary picked up has given up preaching
[[nodiscard]] VillagerDisciple AtPickUp(VillagerDisciple current);

/// The cell of the hand's symbol sheet showing a disciple of a kind, eight to a row; none for a kind with no symbol
[[nodiscard]] std::optional<uint32_t> SymbolCell(VillagerDisciple type);

/// The colour the symbol is drawn in, stored as alpha, red, green and blue: an opaque yellow
inline constexpr uint32_t k_SymbolColour = 0xFFFFFF00;

/// A colour stored as alpha, red, green and blue, one byte each, as a red, green, blue and alpha tint from 0 to 1
[[nodiscard]] glm::vec4 TintOf(uint32_t argb);

/// The symbol stands this many of the villager's sizes over its glow
inline constexpr float k_SymbolRise = 2.5f;

/// Where a disciple's glow is drawn: over the villager as it is drawn this frame (`drawn`, not where its last turn left
/// it), raised by its size
[[nodiscard]] glm::vec3 GlowPoint(const glm::vec3& drawn, float size);

/// Where the symbol of the disciple in or under the hand is drawn: k_SymbolRise of the villager's sizes over its glow
[[nodiscard]] glm::vec3 SymbolPoint(const glm::vec3& glow, float size);

/// How making a disciple moves its player's alignment, before the player's alignment damps it
/// (effects::alignment::ScaleChange): good when its town wants what it does more than the good threshold, evil when less
/// than the evil one, nothing between or for a kind serving no desire
[[nodiscard]] float AlignmentForMaking(std::optional<float> townDesire, float evilBelow, float goodAbove, float change);
/// A villager made a disciple walks up to the thing it was put by when farther than this from its edge
inline constexpr float k_InspectReach = 5.0f;

} // namespace openblack::disciple
