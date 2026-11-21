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

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

#include "Creature/CreatureLayers.h"
#include "Creature/CreatureMorph.h"
#include "Creature/TattooEditor.h"
#include "Enums.h"

namespace openblack::creaturemind
{
struct PhysiqueFileData;
}

/// The tattoo editor that the options' Players page opens with its Edit Tattoo button. It works on a creature body of
/// its own, which is not in the world: the body that the player's profile keeps in its physique file, beside the mind
/// file, or a stand-in Giant Ape when there is no such file. The body is shown at the size of 1, whatever the size kept
/// in the file.
namespace openblack::tattoo_menu
{

/// The body's size, whatever the size kept in the file
constexpr float k_Size = 1.0f;

/// The body that the editor shows
struct Shown
{
	CreatureType species {CreatureType::GiantApe};
	creature_morph::Morph morph;
	/// The two lists of skin marks the file keeps, word for word
	std::vector<uint32_t> firstMarks;
	std::vector<uint32_t> secondMarks;

	[[nodiscard]] bool operator==(const Shown& other) const
	{
		return species == other.species && morph.evilGood == other.morph.evilGood && morph.thinFat == other.morph.thinFat &&
		       morph.weakStrong == other.morph.weakStrong && firstMarks == other.firstMarks && secondMarks == other.secondMarks;
	}
};

/// The stand-in: a Giant Ape half fat and half strong, neither evil nor good, with no marks. These are the body's own
/// values, not a fatness and strength put through the creature's attributes.
[[nodiscard]] Shown StandIn();

/// How strong each species is of itself, which counts towards how strong its body looks
using SpeciesStrength = std::function<float(CreatureType)>;

/// The body from the profile's physique file: its species, its shape from its alignment, fatness and strength as the
/// game shapes any creature's body (creature_morph::FromAttributes), and the file's two lists of skin marks. Without a
/// file, or with a species row that is not one of the game's, the stand-in.
[[nodiscard]] Shown FromPhysique(const std::optional<creaturemind::PhysiqueFileData>& physique,
                                 const SpeciesStrength& speciesStrength);

/// The Edit Tattoo button shows only while the profile has a physique file, the scripts let the temple show the
/// creature, and leaving the Players page does not start the game again
[[nodiscard]] bool ButtonShown(bool physiqueKept, bool creatureShown, bool restartPending);

/// The body looks at the point under the pointer this far into the screen, where the cave's copy looks 3 units in
constexpr float k_LookDepth = 10.0f;

/// What a frame of the editor goes by: whether any mouse button is held, and the left one; the point under the pointer
/// k_LookDepth into the screen; the system's milliseconds and the frame's
struct Input
{
	bool buttonHeld {false};
	bool leftHeld {false};
	glm::vec3 pointer {0.0f};
	uint32_t ticks {0};
	int32_t frameMs {0};
};

/// The body's frame, each frame the editor is drawn: as the Creature Cave's copy has it while the editor is open on it
/// (creature_tattoo_editor::CopyStep with the editor open, looking from the editor's view), its first frame drawing the
/// wait for its first idle action, then an idle action each time the wait runs out and the sit from the tenth on, the
/// left button drawing the wait again and ending what it plays, and its look at the pointer for the first second and a
/// half of a press. It never turns: the turn to the editor's eye is the cave room's. `life` starts afresh with each
/// opening.
void Step(creature_tattoo_editor::CopyLife& life, creature_layers::BodyAction& body, creature_layers::GestureLayer& gesture,
          std::optional<glm::vec3>& lookAt, const Input& input, const creature_tattoo_editor::Orbit& view,
          const std::function<bool(size_t)>& hasAnimation, const std::function<uint32_t(int32_t)>& localRand);

/// What is drawn of the body, in the order it is drawn
enum class Layer : uint8_t
{
	Body,
	Hair,
	Eyes,
};

/// The body first, then its hair, at the end of the body's own draw, then its eyes, as any creature's are drawn; the
/// hair only while the body has hair of its own and the hair is shown, the eyes only while it has them
[[nodiscard]] std::vector<Layer> DrawOrder(bool hasHair, bool hairShown, bool hasEyes);

/// The debug readout's line for the body the editor shows: its species, its shape, how many words each list of skin
/// marks keeps, the wait for its next idle action and how many it has had
[[nodiscard]] std::string Readout(const Shown& shown, const creature_tattoo_editor::CopyLife& life);

} // namespace openblack::tattoo_menu
