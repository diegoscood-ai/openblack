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

#include <optional>
#include <string>

#include "Creature/CreaturePhysiology.h"
#include "Creature/CreatureTattoo.h"
#include "Enums.h"

namespace openblack::creaturemind
{
struct MindFileData;
struct PhysiqueFileData;
} // namespace openblack::creaturemind

/// The creature a mind file describes besides its mind: a saved creature carries its species, name, alignment,
/// strength, size and tattoos, so that a creature can be made again from the file alone. Older versions leave out the
/// size, alignment or tattoos, which then stay as the species starts them.
namespace openblack::creature_mind_body
{

/// The body's needs as a file keeps them
struct SavedNeeds
{
	/// Game turns lived, which the age counts up from
	uint32_t turns {0};
	uint32_t age {0};
	float energy {1.0f};
	/// None before version 6, which leaves it as the creature starts it
	std::optional<float> itchiness;
	float poo {0.0f};
	float exhaustion {0.0f};
	float dehydration {0.0f};
};

struct Body
{
	CreatureType species {CreatureType::Unknown};
	std::u16string name;
	/// -1 (evil) to 1 (good)
	std::optional<float> alignment;
	/// 0 to 1
	float strength {0.5f};
	std::optional<float> size;
	std::optional<creature_tattoo::Slots> tattoos;
	/// As kept, and the fatness its body showed when it was saved
	float fatness {0.5f};
	float previousFatness {0.5f};
	SavedNeeds needs {};
};

/// The species of a row of the game's creature tables, which start with the Giant Ape
[[nodiscard]] std::optional<CreatureType> SpeciesFromRow(uint32_t row);

/// The creature the file describes, or none when its species row isn't one of the game's
[[nodiscard]] std::optional<Body> FromMindFile(const creaturemind::MindFileData& file);

/// The creature the file describes as one of the given species, the file's own species row not read
[[nodiscard]] Body FromMindFile(const creaturemind::MindFileData& file, CreatureType species);

/// The fatness a loaded creature's body shows: the file's previous fatness brought twice towards its fatness, by at
/// most creature_morph::k_MaxFatnessStep each time and kept to 0..1, once as the body is read and once after it
[[nodiscard]] float LoadedShownFatness(float previousFatness, float fatness);

/// A body's needs with the saved ones put back, as kept; the warmth, the life and what a file does not keep stay as
/// they are
[[nodiscard]] creature_physiology::Needs WithSavedNeeds(creature_physiology::Needs needs, const SavedNeeds& saved);

/// The body a live creature has now, as its files keep it
struct BodyNow
{
	uint32_t speciesRow {0};
	float size {1.0f};
	float strength {0.5f};
	float fatness {0.5f};
	/// As the creature is saved: before any spell that changed it
	float alignment {0.0f};
};

/// The leashes a creature knows, as a file keeps them: a flag for each leash type
struct Leashes
{
	bool evil {false};
	bool rope {false};
	bool good {false};
};

/// What a live creature's mind file keeps of it besides its mind, as the creature is now
struct LiveBody
{
	float fatness {0.5f};
	/// The fatness its body shows, kept as the file's previous fatness
	float shownFatness {0.5f};
	/// Its body's turns lived, age, energy, itchiness, amount of poo, exhaustion and dehydration; none without a body
	/// to ask, which leaves the file's
	std::optional<creature_physiology::Needs> needs;
	/// Its fighter's in a fight, else as it left its last one
	float fightHealth {1.0f};
	bool inDevScript {false};
	/// None without the leash rules to ask, which leaves the file's
	std::optional<Leashes> leashes;
	creature_tattoo::Slots tattoos {};
};

/// A live creature's mind file: the mind as its mind system saves it (desires, learning, size, strength, alignment),
/// with the body as it is now: the fatness, the shown fatness as the previous fatness, the needs, the fight health,
/// whether it is in its growing-up scripts, the leashes it knows and the tattoo slots. The flag of the first leash
/// type, the tattoo's bytes and what the creature does not model stay as the file it was loaded from had them
[[nodiscard]] creaturemind::MindFileData ToMindFile(creaturemind::MindFileData mind, const LiveBody& body);

/// A live creature's physique file: its species row, the size it is drawn at as its 3D body keeps it
/// (creature_morph::ClampScale), strength, fatness and alignment, and the skin's two lists kept from the file it
/// replaces (empty without one)
[[nodiscard]] creaturemind::PhysiqueFileData ToPhysique(const BodyNow& body,
                                                        const std::optional<creaturemind::PhysiqueFileData>& previous);

} // namespace openblack::creature_mind_body
