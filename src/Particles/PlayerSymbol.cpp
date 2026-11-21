/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlayerSymbol.h"

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <string_view>

#include "3D/FrameAnim.h"
#include "3D/ObjectMatrix.h"
#include "3D/PlayerSymbolSheet.h"
#include "ECS/PlayerColours.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "Graphics/ArgbColour.h"
#include "Input/LocalPlayer.h"
#include "Locator.h"
#include "ParticleRegistry.h"

using namespace openblack;
using namespace openblack::particles;

namespace
{
/// the player colours, by the player's colour index on the land (ecs::player_colours)
constexpr std::array<uint32_t, 7> k_PlayerColours = {0xFF4646, 0x47FF54, 0xE347FF, 0x47F9FF, 0xFFFD47, 0x4777FF, 0xFFA247};
/// the glows' alpha, in 1/256 of the symbol's
constexpr uint32_t k_GlowAlpha = 100;
/// the glows' size, times the symbol's
constexpr float k_GlowSize = 1.5f;

/// A sprite of a colour, its alpha the colour's
Effect::DrawAtom Sprite(const Creator& creator, const glm::vec3& position, const glm::mat3& rotation, float scale, float frame,
                        uint32_t argb, const Atom* atom)
{
	return {&creator,
	        position,
	        rotation,
	        scale,
	        1.0f,
	        static_cast<float>(argb_colour::Alpha(argb)),
	        frame,
	        {static_cast<uint8_t>(argb_colour::Red(argb)), static_cast<uint8_t>(argb_colour::Green(argb)),
	         static_cast<uint8_t>(argb_colour::Blue(argb))},
	        0,
	        atom};
}

/// ParticleSymbolSpriteCreator: each atom it makes is drawn as a player's symbol, its effect's player's (the local
/// player's for an effect with none)
struct SymbolSpriteCreator final: Creator
{
	void InitAtom(Effect& effect, Atom& atom) const override
	{
		const int player = effect.GetPlayer() >= 0 ? effect.GetPlayer() : static_cast<int>(creature::LocalPlayer());
		atom.playerSymbol = std::make_shared<player_symbol::PlayerSymbol>(player_symbol::PlayerSymbol {.player = player});
	}
};

std::unique_ptr<Creator> MakeSymbolSpriteCreator(const Object& object)
{
	auto creator = std::make_unique<SymbolSpriteCreator>();
	ReadCreatorProperties(object, *creator);
	return creator;
}
} // namespace

void openblack::particles::RegisterPlayerSymbolCreator()
{
	RegisterCreator("ParticleSymbolSpriteCreator", MakeSymbolSpriteCreator);
}

uint32_t player_symbol::ColourOf(int player)
{
	const auto index = static_cast<size_t>(ecs::player_colours::OnThisLand(static_cast<PlayerNames>(player)));
	return k_PlayerColours.at(index % k_PlayerColours.size());
}

player_symbol::Colours player_symbol::ColoursOf(uint32_t playerRgb, uint32_t particleArgb) noexcept
{
	const uint32_t symbol = argb_colour::MultiplyArgbShift8(playerRgb | 0xFF000000u, particleArgb);
	const uint32_t alpha = (argb_colour::Alpha(symbol) * k_GlowAlpha) >> 8u;
	return {.symbol = symbol, .glow = (symbol & 0x00FFFFFFu) | alpha << 24u, .whiteGlow = 0x00FFFFFFu | alpha << 24u};
}

void player_symbol::Step(PlayerSymbol& symbol, float milliseconds) noexcept
{
	symbol.spin = graphics::frame_anim::PlayerSymbolSpin(symbol.glowSpin, milliseconds);
	symbol.cellA = static_cast<float>(graphics::frame_anim::PlayerSymbolCell(symbol.glowA, milliseconds, 0));
	symbol.cellB = static_cast<float>(graphics::frame_anim::PlayerSymbolCell(symbol.glowB, milliseconds, 1));
}

void player_symbol::Draw(const PlayerSymbol& symbol, const glm::vec3& position, float scale, const Colours& colours,
                         const Atom* atom, std::vector<Effect::DrawAtom>& out)
{
	const glm::mat3 still(1.0f);
	// the second glow's spin carried as the atom's Y angle matrix (affine::AngleY), whose roll atan2(M[0][2], M[0][0]) =
	// +spin (billboard::Screen turns it clockwise)
	const glm::mat3 spun = affine::AngleY(symbol.spin);
	out.push_back(Sprite(GlowCreator(), position, still, k_GlowSize * scale, symbol.cellA, colours.glow, atom));
	out.push_back(Sprite(GlowCreator(), position, spun, k_GlowSize * scale, symbol.cellB, colours.whiteGlow, atom));
	out.push_back(Sprite(SymbolCreator(symbol.player), position, still, scale, 0.0f, colours.symbol, atom));
}

const Creator& player_symbol::GlowCreator()
{
	static const Creator creator = [] {
		Creator c;
		c.kind = Creator::Kind::Sprite;
		c.texture = "S_SpriteSheet3";
		c.spritesPerRow = 8;
		c.numFrames = 32;
		return c;
	}();
	return creator;
}

int player_symbol::ProfileSymbol()
{
	if (!Locator::playerProfileSystem::has_value())
	{
		return 0;
	}
	const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
	return profile.has_value() ? profile->symbol : 0;
}

const Creator& player_symbol::SymbolCreator(int player)
{
	return SymbolCreator(player, static_cast<int>(creature::LocalPlayer()), ProfileSymbol());
}

const Creator& player_symbol::SymbolCreator(int player, int localPlayer, int profileSymbol)
{
	// Each player's symbol is its cell of the players' symbol sheet (4 x 4 cells of 64 x 64). The local player's cell
	// is the cell of its profile's symbol in the symbols the player chooses from, copied texel for texel. ChooseSymbol is
	// that same file, the player's own pictures pasted in, loaded the same way, so drawing the local player's symbol
	// straight from it gives the same texels
	constexpr int k_Cells = 16;
	const auto cellsOf = [](std::string_view texture) {
		std::array<Creator, k_Cells> c;
		for (int i = 0; i < k_Cells; ++i)
		{
			c[static_cast<size_t>(i)].kind = Creator::Kind::Sprite;
			c[static_cast<size_t>(i)].texture = std::string(texture);
			c[static_cast<size_t>(i)].spritesPerRow = 4;
			c[static_cast<size_t>(i)].fileOffset = i;
			c[static_cast<size_t>(i)].numFrames = 1;
		}
		return c;
	};
	static const std::array<Creator, k_Cells> profile = cellsOf("ChooseSymbol");
	static const std::array<Creator, k_Cells> sheet = cellsOf(player_symbol_sheet::k_TextureName);
	if (SourceOf(player, localPlayer) == SymbolSource::Profile)
	{
		// a symbol past the sixteen is the first, as a profile keeps it
		return profile[static_cast<size_t>(profileSymbol >= 0 && profileSymbol < k_Cells ? profileSymbol : 0)];
	}
	return sheet[static_cast<size_t>(std::clamp(player, 0, k_Cells - 1))];
}
