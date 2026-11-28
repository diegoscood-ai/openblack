/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <span>
#include <vector>

#include <glm/gtc/type_precision.hpp>

/// The marks fights and fire leave on a creature's skin: wounds and burns, each a small picture from an atlas of
/// damage that starts fresh and fades towards an old scar as it ages, and trails of blood, a texel at a time, darkening
/// as they dry. Marks age as the creature heals and go once they are old enough. They are painted over the skin after
/// its tattoos, the wounds first.
namespace openblack::creature_marks
{
/// A creature keeps at most this many of each kind of mark; a new one past that takes the place of the oldest
constexpr size_t k_MaxMarks = 1024;

/// A wound, burn or drop of blood on a skin
struct Mark
{
	/// Where, in texels of a skin 256 wide, and which of the base mesh's skins, 0 to 3
	uint8_t u {0};
	uint8_t v {0};
	uint8_t skin {0};
	/// How many steps of healing it has been through, 0 to k_OldestAge
	uint8_t age {0};
	/// A wound's kind, 0 to 7, the row of the damage atlas it is drawn from, and its column, 0 to 7
	uint8_t type {0};
	uint8_t column {0};

	bool operator==(const Mark&) const = default;
};

/// How many steps of healing a wound of each kind lasts, which it fades from fresh to old over. A mark's age stops at
/// k_OldestAge, so a kind that lasts longer than that never goes: it stays at its oldest look
constexpr std::array<uint8_t, 8> k_WoundLifetimes {32, 64, 64, 64, 64, 64, 64, 1};
/// The oldest a mark gets. A drop of blood this old goes at the next step
constexpr uint8_t k_OldestAge = 63;
/// Healing comes in counts: a mark ages a step every so many
constexpr uint32_t k_CountsPerStep = 600;
/// The counts a game turn heals by
constexpr uint32_t k_CountsPerTurn = 1;
/// A heal effect heals by this many counts for each of its strength
constexpr uint32_t k_HealEffectCounts = 9600;

struct Marks
{
	std::vector<Mark> wounds;
	std::vector<Mark> blood;
	/// Counts of healing since the marks last aged
	uint32_t counts {0};
};

/// A mark added to a list, in place of the oldest when the list is full
void Add(std::vector<Mark>& marks, const Mark& mark);

/// The two flag bits a wound is kept with in the creature's files; a drop of blood keeps none
constexpr uint8_t k_WoundFileFlags = 3;
/// A mark as the creature's files keep it, one word: its place across (the low byte) and down, then its age in 6 bits,
/// its skin in 2, its kind in 3, its column in 3 and the two flag bits
[[nodiscard]] uint32_t ToFileWord(const Mark& mark, uint8_t flags);
/// A mark read back from its word in the creature's files; the flag bits are not kept
[[nodiscard]] Mark FromFileWord(uint32_t word);

/// The marks healed by some counts. Every k_CountsPerStep, each drop of blood that is already k_OldestAge goes and the
/// others age a step; each wound younger than k_OldestAge ages a step and goes when that makes it as old as its kind
/// lasts. The last mark of the list takes the place of one that goes, and has its own step there. Whether they aged, so
/// the skin is to be painted again.
bool Heal(Marks& marks, uint32_t counts);

/// The damage atlases are 8 by 8 cells of 32 texels, a wound's kind picking the row and its column the column
constexpr uint32_t k_AtlasSize = 256;
constexpr uint32_t k_CellSize = 32;
constexpr uint32_t k_CellsPerRow = 8;

/// A damage atlas: colours and alpha, a row at a time, k_AtlasSize square
struct Atlas
{
	std::vector<std::array<uint8_t, 3>> colours;
	std::vector<uint8_t> alpha;
};

/// The fresh and the old look of wounds
struct DamageArt
{
	Atlas fresh;
	Atlas old;
};

/// A wound's colour and alpha at a texel of its picture: the fresh one moved towards the old by its age over its kind's
/// lifetime, in 256ths
[[nodiscard]] glm::u8vec4 WoundTexel(const DamageArt& art, const Mark& wound, uint32_t x, uint32_t y);

/// A texel of the skin, 4 bits a channel with blue the lowest, with a colour blended over it by an alpha of 255, each
/// channel blended at 8 bits and cut back to 4; opaque
[[nodiscard]] uint16_t BlendTexel(uint16_t skin, const glm::u8vec3& colour, uint8_t alpha);

/// A wound painted onto a skin of side by side texels: its picture centred on its place, cut off at the skin's edges.
/// On a skin of 128 (the detail levels 0 and 1) it is centred on half its place at half size, each skin texel taking
/// the second of each two rows and columns of the picture; on a skin of another size than 256 or 128 nothing is painted
void PaintWound(std::span<uint16_t> skin, uint32_t side, const DamageArt& art, const Mark& wound);

/// Blood is red when fresh, browner as it dries, and covers three quarters of the skin under it
[[nodiscard]] glm::u8vec3 BloodColour(uint8_t age);
/// A texel of the skin with blood over it: each channel is the skin's 4 bits widened to 8, a quarter of it added to
/// three quarters of the blood's channel, rounded down and cut back to the top 4 bits; opaque
[[nodiscard]] uint16_t BloodTexel(uint16_t skin, const glm::u8vec3& colour);
/// A drop of blood painted onto a skin of side by side texels; on a skin of 128 at half its place, on a skin of another
/// size than 256 or 128 nowhere
void PaintBlood(std::span<uint16_t> skin, uint32_t side, const Mark& blood);

/// A spot on a creature's mesh as the game's mesh test keeps it: the primitive's number in its sub-mesh, the triangle's
/// three corners numbered within that primitive, and how far along its first edge (u) and its second (v) the spot is
struct MeshSpot
{
	uint32_t primitive {0};
	std::array<uint32_t, 3> corners {};
	float u {0.0f};
	float v {0.0f};

	bool operator==(const MeshSpot&) const = default;
};

/// A spot on a creature's mesh with the sub-mesh it is in
struct MeshCut
{
	uint32_t subMesh {0};
	MeshSpot spot;

	bool operator==(const MeshCut&) const = default;
};

/// A wound that keeps bleeding runs a trail of blood down the skin: each turn for twelve seconds of the creature's own
/// time, the trail's spot moves to where the skin is met along its inward normal from a little below it, and a fresh
/// drop of blood is left there. A trail that meets no skin stops. A creature runs at most eight at once.
/// Wiki: docs/wiki/creature/body.md#meshes-morph-layers-and-skin.
constexpr size_t k_BloodTrails = 8;
/// How long a trail runs, in milliseconds of the creature's own time; a trail this old is free to start again
constexpr int32_t k_TrailRunsMs = 12000;
/// How far below the trail's spot the skin is looked for again, in metres
constexpr float k_TrailDrop = 0.012f;
struct BloodTrail
{
	MeshCut cut;
	int32_t ms {k_TrailRunsMs};

	bool operator==(const BloodTrail&) const = default;
};
using BloodTrails = std::array<BloodTrail, k_BloodTrails>;
/// A trail started at a cut in the first free one, nothing when all eight run
void StartTrail(BloodTrails& trails, const MeshCut& cut);
/// A turn of the creature's own milliseconds on a trail: whether it was running, and so takes its step this turn
[[nodiscard]] bool RunTrail(BloodTrail& trail, int32_t ms);
} // namespace openblack::creature_marks
