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

#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/OrientedText.h"

// The miracle's look on the player's hand: the glowing S_Hand_Flow second pass, the Power_Up_Band rings, the
// in-hand particle effect of the seed and the tribe's name spinning round it. The game reaches it through the miracle FX
// system (Locator::miracleFxSystem); the renderer reads the tribes' names here. Wiki: docs/bw1-notes/magic.md, "The hand".

namespace openblack
{
enum class PlayerNames : uint8_t;
}
namespace openblack::graphics
{
class Texture2D;
}
namespace openblack::gui
{
class GameInterface;
}
namespace openblack::magic::tribal_spin
{
class Runner;
}

namespace openblack::magic::hand_fx
{
/// Every permanent band goes
void RemoveAllPermanentBands();
/// Every scribble cancel: G_ShakeHand_01 and one band flying off
void RemoveHandSpellVisuals();
/// Five temporary bands 0.1 s apart (2.4 s later if delayed) and
/// G_SpellPowerUpBand
void AddSpellToHandVisuals(bool delayed);
/// Permanent bands added or removed until there are `level` (at most 5)
void SetPowerUpLevel(int level, bool delayed);
/// The number of permanent bands
[[nodiscard]] int PowerUpLevel();
/// The tribe's power behind the miracle in the hand, as the tribe's name spinning round the hand in the local player's
/// colour (Magic/TribalPowerSpin.h). Only for a tribe whose tribal power is above 1, which none gains in the original
/// game, so it never shows there. A new ring replaces the old one.
void StartTribalPowerRing(int tribe);
/// The ring round the hand goes
void StopTribalPowerRing();
/// The miracle is cast: the ring round the hand is let go and rises as a column from where the hand is, or, with no
/// ring, a column of the tribe's name rises there in the local player's colour
void ReleaseOrCreateTribalPowerRing(int tribe, const glm::vec3& handPosition);
/// A column of the tribe's name rising from a place, in a player's colour (a miracle another interface cast)
void CreateTribalPowerColumn(int tribe, const glm::vec3& position, PlayerNames player);
/// The interface whose text names the tribes and whose font writes them, or none (nothing is written)
void SetInterface(const gui::GameInterface* interface);
/// The ring round the hand, if there is one, then the rising columns; each is drawn as its own see-through object,
/// sorted by its Position() (none without the hand's magic state)
[[nodiscard]] std::vector<const tribal_spin::Runner*> GetTribalPowerRunners();
/// A ring's or column's letters as triangles in the world, and the font texture they are drawn with (none without
/// an interface)
[[nodiscard]] std::vector<OrientedTextVertex> GetTribalPowerText(const tribal_spin::Runner& runner);
[[nodiscard]] const graphics::Texture2D* GetTribalPowerTexture();

/// Every frame (seconds = the game time step x 0.001, 0 while paused)
void Update(float seconds);

/// The hand's second pass with the flowing texture (for the renderer): alpha 0 = not drawn
struct Glow
{
	float alpha {0.0f};        ///< 0.8 while a miracle is in the hand
	glm::vec2 uvOffset {0.0f}; ///< the 8 x 4 atlas cell of the frame
};
[[nodiscard]] Glow GetGlow();

/// The seed's in-hand effect (GMagicInfo.particleTypeInHand of its level), replacing the old one
void CreateInHandEffect(entt::entity seed);
/// The in-hand effect goes
void ReleaseInHandEffect();
/// Every frame: to the hand (or its bone), strength = the seed's PSys power, magnitude = the hand's scale, a step of
/// max(1, the game time step) ms; drawn only once the seed is ready
void UpdateInHandEffect(float milliseconds);

/// A land is loaded: the bands, the effect and the tribes' names go
void Reset();
} // namespace openblack::magic::hand_fx
