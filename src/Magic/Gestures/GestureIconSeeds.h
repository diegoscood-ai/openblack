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

#include <array>
#include <optional>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"
#include "GestureIcons.h"
#include "Magic/MagicTables.h"
#include "Particles/ParticleEffect.h"

// The seed's graphic inside a seed's gesture icon: the seed's mesh turning (a creature spell phial with its frames and
// its pulse), the holder effect of its seed and its power-up bands, all drawn at once with the models' camera
// (GestureIconModels.h) and moved on the screen to the box above the icon. One graphic for each magic type, made at its
// first draw and kept until the map is cleared; it is not one of the world's seed graphics and the world never draws it.
// Wiki: docs/wiki/magic/gestures.md#the-gesture-icons.

namespace openblack::magic::gestures
{
/// The size every icon's graphic is drawn at, around k_IconModelFocus
inline constexpr float k_IconSeedScale = 0.12f;
/// The bands' size factor of an icon's graphic (a world graphic's is 1)
inline constexpr float k_IconSeedBandScale = 0.5f;
/// The bands' alpha before the icons' (a world graphic's is 60)
inline constexpr uint32_t k_IconSeedBandAlpha = 120;

/// The graphic's places, as its map position keeps them: the point it floats at, its mesh and its holder effect. The
/// point goes through a map position (x and z in fixed point, the height above the ground), so the land's height under
/// it is taken away and added back as the game does
struct IconSeedPlaces
{
	glm::vec3 point {0.0f};  ///< the bands' centre
	glm::vec3 mesh {0.0f};   ///< the point raised by the seed's mesh height times the scale
	glm::vec3 effect {0.0f}; ///< the point raised by the seed's holder height times the scale
};
/// `ground`: the land's height under the point
[[nodiscard]] IconSeedPlaces IconSeedPlacesAt(glm::vec3 point, float scale, float ground, float meshHeight, float holderHeight);

/// What a seed's graphic is made of, from the seed's row of the tables
struct IconSeedLook
{
	float drawScale {1.0f}; ///< the seed's scale
	float meshHeight {0.0f};
	float holderHeight {0.0f};
	bool useMesh {true};
	/// A creature spell phial: the receive type of the seed's spell
	std::optional<CreatureReceiveSpellType> phial;
	float meshHalfDiagonal {0.0f}; ///< the seed mesh's, for the far cut
	float bandHalfDiagonal {0.0f}; ///< the power-up band mesh's
};

/// One icon graphic: what is kept between its draws
struct IconSeedGraphic
{
	SpellSeedType seed {SpellSeedType::None};
	int powerUp {-1}; ///< the power-up level it was made with: its bands, none at -1
	PlayerNames player {PlayerNames::PLAYER_ONE};
	uint32_t mesh {0};   ///< the mesh's resource id (the flying flock's follows the player's alignment)
	uint32_t effect {0}; ///< the holder effect (0: none)
	float spin {0.0f};   ///< the mesh's turn about y, 2 rad/s
	float uvFrame {0.0f};
	float pulsePhase {0.0f};
	float bandAngle {0.0f};        ///< 10.3 rad/s
	float bandSpin {0.0f};         ///< 1 rad/s
	uint32_t colour {0xFFFFFFFFu}; ///< its mesh's colour, kept between draws
};
/// One graphic for each magic type
using IconSeedGraphics = std::array<std::optional<IconSeedGraphic>, k_MagicTypeCount>;

/// One draw of the graphic's mesh, in the models' camera's space
struct IconSeedMeshDraw
{
	glm::mat4 matrix {1.0f};
	float scale {1.0f};     ///< the object's scale, for the far cut
	uint32_t colour {0u};   ///< ARGB: its rgb set as the colour, its alpha byte
	uint32_t specular {0u}; ///< RGB
	bool fading {false};    ///< drawn with its own alpha
	bool landLight {false}; ///< the land's light where it stands instead of its colour (the invisible phial)
	glm::vec2 uv {0.0f};    ///< the texture's offset (a phial's frame)
	bool inReach {true};    ///< not left out by the far cut
};
/// One draw of a power-up band
struct IconSeedBandDraw
{
	glm::mat4 matrix {1.0f};
	float scale {1.0f};
	uint32_t colour {0u}; ///< ARGB
	bool inReach {true};
};
inline constexpr uint32_t k_IconSeedBandSpecular = 0x141414u;
/// What one draw of an icon's graphic draws, in its order: the mesh, the holder effect, the bands
struct IconSeedDraw
{
	std::vector<IconSeedMeshDraw> meshes;
	/// The freeze phial: its frozen look (the land's light, then the frozen draw with the pulse) instead of a mesh draw
	bool frozen {false};
	glm::vec3 effectOrigin {0.0f};
	float effectMagnitude {1.0f};
	uint8_t effectAlpha {255};
	std::vector<IconSeedBandDraw> bands;
};

/// One draw of the graphic at `places` (the point at k_IconModelFocus, k_IconSeedScale), with the icons' alpha byte and
/// the player's colour: the mesh turns on by the frame and, for a phial, its frames and its pulse; the bands' angles
/// turn while it has a power-up. Every step as the original rounds it
[[nodiscard]] IconSeedDraw StepIconSeed(IconSeedGraphic& graphic, const IconSeedLook& look, const IconSeedPlaces& places,
                                        uint32_t frameMs, uint8_t alpha, uint32_t playerColour);
/// Whether a model is drawn: the object draw leaves it out when its half diagonal times its scale times 300, squared, is
/// short of its squared distance from the models' camera
[[nodiscard]] bool IconModelInReach(glm::vec3 position, float halfDiagonal, float scale);

/// A seed icon's graphic this frame, for the draw
struct IconSeedFrame
{
	IconSeedDraw draw;
	uint32_t mesh {0};
	/// The holder effect's sprites where its step of this draw left them, in its draw order
	std::vector<particles::Effect::DrawAtom> sprites;
};
/// The seed graphic of a drawn seed icon (IconModelKind::SeedGraphic): the graphic of the seed's magic at the icon's
/// power-up level, made at its first draw (by the local player, its holder effect started there and hidden from the
/// world's draw), moved to the models' camera's focus, its holder effect stepped by the frame, then drawn. Nothing for a
/// seed or magic type out of the tables
[[nodiscard]] std::optional<IconSeedFrame> DrawIconSeed(const GestureIcon& icon, uint8_t alpha, uint32_t frameMs);
/// Every 30 turns, with the world's seed graphics: the flying flock's graphic takes the bat or the dove again for its
/// player's alignment
void RefreshIconSeedMeshes();
/// The map is cleared: every graphic goes with its holder effect
void ResetIconSeeds();
/// The graphics, for the gesture window
[[nodiscard]] const IconSeedGraphics& IconSeeds();
} // namespace openblack::magic::gestures
