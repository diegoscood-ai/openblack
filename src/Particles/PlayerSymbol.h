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

#include <glm/vec3.hpp>

#include "ParticleEffect.h"

/// A player's symbol as a particle draws it: two glows of the player's colour and white, and the player's symbol over
/// them, at the particle's place and size. The town centres' belief symbols are such particles.
namespace openblack::particles::player_symbol
{

/// One symbol's sprites and their frame
struct PlayerSymbol
{
	int player {0};
	/// the two glows' cell phases and the second glow's angle, all 0 at first (frame_anim::PlayerSymbolCell /
	/// PlayerSymbolSpin)
	float glowA {0.0f}, glowB {0.0f}, glowSpin {0.0f};
	/// this frame's glow cells and spin (Step)
	float cellA {0.0f}, cellB {0.0f}, spin {0.0f};
};

/// The colours (0xAARRGGBB) the three sprites are drawn with
struct Colours
{
	uint32_t symbol;
	uint32_t glow;
	uint32_t whiteGlow;

	[[nodiscard]] bool operator==(const Colours& other) const = default;
};

/// The player's colour, 0xRRGGBB, by its colour index on the current land
[[nodiscard]] uint32_t ColourOf(int player);
/// The symbol's colours for a particle colour: the symbol the player's colour (opaque) times the particle's, (c p) >> 8
/// in the four channels; the first glow that colour at an alpha of (the symbol's alpha x 100) >> 8; the second glow
/// white at the same alpha
[[nodiscard]] Colours ColoursOf(uint32_t playerRgb, uint32_t particleArgb) noexcept;
/// The glows' frame, once each time the symbol is drawn, by the frame's game milliseconds
void Step(PlayerSymbol& symbol, float milliseconds) noexcept;
/// The three sprites, in their draw order: the glow, the white glow turned by the spin, the symbol. The symbol is
/// `scale` across, the glows 1.5 times that
void Draw(const PlayerSymbol& symbol, const glm::vec3& position, float scale, const Colours& colours, const Atom* atom,
          std::vector<Effect::DrawAtom>& out);

/// Where a player's symbol is drawn from
enum class SymbolSource : uint8_t
{
	Profile, ///< the profile's symbol, its cell of ChooseSymbol
	Sheet,   ///< the player's cell of the players' symbol sheet (3D/PlayerSymbolSheet.h)
};
/// The local player's symbol is its profile's; every other player's is its own cell of the players' symbol sheet, which
/// holds its god's image when the computer played it at the game's start, and is empty otherwise
[[nodiscard]] constexpr SymbolSource SourceOf(int player, int localPlayer) noexcept
{
	return player == localPlayer ? SymbolSource::Profile : SymbolSource::Sheet;
}

/// The symbol of the profile played, 0 without one
[[nodiscard]] int ProfileSymbol();

/// The glows' sprite creator (S_SpriteSheet3) and the player's symbol's (SourceOf: the cell of the profile's symbol in
/// ChooseSymbol, or the player's cell of the players' symbol sheet), the local player's the one at this machine
[[nodiscard]] const Creator& GlowCreator();
[[nodiscard]] const Creator& SymbolCreator(int player);
[[nodiscard]] const Creator& SymbolCreator(int player, int localPlayer, int profileSymbol);

} // namespace openblack::particles::player_symbol
