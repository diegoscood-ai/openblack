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
#include <optional>
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "3D/LandMorph.h"
#include "Enums.h"

namespace openblack
{
struct GInfluenceInfo;
}

// A player's influence (Influence.cpp of the original): where the player can act and cast. Sources: the citadel and the
// player's towns (a radius each: inside it the influence is 1) and the influence rings (a gradient). Research:
// dev\tmp_dis\miracles\influence.md; wiki: docs/bw1-notes/magic.md ("Influencia").
//   Influence.cpp            the queries and the land's globals
//   InfluenceSources.cpp     town (Town::Process part) and citadel radii
//   InfluenceRings.cpp       InfluenceRing
//   InfluenceCircles.cpp     InfluenceCircle (the border drawn in the world) and the hand's crossing of it
//   InfluenceDebugHooks.cpp  OPENBLACK_TEST_INFLUENCE

namespace openblack::influence
{
/// INFL_CALC_TYPE: only the virtual influence reads it (not ported). The hand's in-influence test passes Interface.
enum class CalcType : int32_t
{
	Default = 0,
	Interface = 1,
};

/// Influence::CalculatePlayerInfluence 0x5CD170 (pos, player, 0, type, allies), -1..1; > 0 = in the player's
/// influence. The cast rules (fn_005FB5D0) and the hand (fn_005D1120) test `> 0` with allies on.
[[nodiscard]] float CalculatePlayerInfluence(PlayerNames player, const glm::vec3& position,
                                             CalcType type = CalcType::Default, bool includeAllies = true);
[[nodiscard]] float CalculatePlayerInfluence(entt::entity player, const glm::vec3& position,
                                             CalcType type = CalcType::Default, bool includeAllies = true);
/// Influence::CalculatePlayerRawInfluence 0x5CD230: citadel + towns + rings, clamped to -1..1 (an anti ring of the
/// player covering the point gives 0)
[[nodiscard]] float CalculatePlayerRawInfluence(PlayerNames player, const glm::vec3& position);
/// Influence::IsInPlayerRawInfluence 0x5CD460
[[nodiscard]] bool IsInPlayerRawInfluence(PlayerNames player, const glm::vec3& position);
/// Influence::IsInAntiInfluence 0x5CD490: inside an anti-influence ring of that player
[[nodiscard]] bool IsInAntiInfluence(PlayerNames player, const glm::vec3& position);
[[nodiscard]] bool IsInAntiInfluence(entt::entity player, const glm::vec3& position);
/// Influence::CalculateInfluenceOnRange 0x5CD560 (GInfluenceInfo): 1 up to 0.4 r, 0.8 -> 0 up to 0.6 r, 0.2 -> 0 up to r
[[nodiscard]] float CalculateInfluenceOnRange(float distance, float radius);
[[nodiscard]] float CalculateInfluenceOnRange(float distance, float radius, const GInfluenceInfo& info);

// ---- rings (InfluenceRings.cpp) ----

/// InfluenceRing::Create 0x5CD9D0: a fixed ring (CREATE_INFLUENCE_RING, INFLUENCE_POSITION, shields)
entt::entity CreateRing(const glm::vec3& position, PlayerNames player, float radius, bool anti);
/// fn_005CD990 -> fn_005CD800: a ring that follows an object (INFLUENCE_OBJECT); entt::null if the object is not valid
entt::entity CreateRingOnObject(entt::entity object, PlayerNames player, float radius, bool anti);
/// InfluenceRing::ToBeDeleted 0x5CD8A0
void DeleteRing(entt::entity ring);
/// InfluenceRing::ProcessRings 0x5CDB90: attached rings follow their object, or go with it
void ProcessRings();

// ---- towns and citadel (InfluenceSources.cpp) ----

/// The influence part of Town::Process 0x747380 for every town (GetBaseInfluence 0x73FD40 + Abode::GetInfluence
/// 0x4072A0 of its abodes, x townInfluenceMultiplier)
void ProcessTowns();
/// Town +0x5C8 of a town entity (0 if it has no influence yet)
[[nodiscard]] float TownRadius(entt::entity town);
/// Citadel::GetInfluence 0x464090 of a temple entity
[[nodiscard]] float CitadelRadius(entt::entity temple);

// ---- the player's influence power (GPlayer +0x8C; InfluenceSources.cpp) ----

/// GPlayer::CalculateInfluencePower 0x64AD00: +0x8C = Citadel::GetInfluence of the player's citadel (+0xA48, with its
/// heart +0x30), + each town's +0x5C8 (fn_0073FBC0, the town list +0xA50), + the radius +0x38 of every influence ring of
/// the player (g_game+0x205C4C, anti rings too), each a float sum; stored and returned. The rest of the function (+0x90
/// = (+0x90 + +0x8C) x 0.5 and the GameStats history +0xAC) has no reader in openblack and is not ported.
float CalculateInfluencePower(PlayerNames player);
/// GPlayer::Process 0x64971D for every player and the neutral one (GetNextPlayerAndNeutral, GPlayer::ProcessPlayers
/// 0x649B1D), after its alignment: MagicLoop's slot 3
void CalculateInfluencePowers();
/// GPlayer +0x8C as the last CalculateInfluencePower left it (0 before the first one of the land)
[[nodiscard]] float InfluencePower(PlayerNames player);
/// fn_0064B700: the sum (from 0, in float) of +0x8C over GGame::GetNextActivePlayerAndNeutral 0x550930 (the players of
/// type +0x8E0 != 0, then the neutral one), divided by the player's own +0x8C (fdivr 0x64B74F); 0 when the own is 0
/// (fcom 0; test ah, 0x40). (inferido) a player is active when the land made it (magic::players::EntityOf): openblack
/// keeps +0x8E0 only for the human one
[[nodiscard]] float InfluencePowerRatio(PlayerNames player);

/// The influence part of Citadel::Process for every temple (one per player, GPlayer +0xA48): fn_00555240 on
/// GetInfluence against +0x78 (0x4630C6), and the border latch the citadel object sets (ShowBoundary)
void ProcessCitadels();

/// What the original does in GGame::ProcessTurn at the influence slot: InfluenceRing::ProcessRings, plus the towns'
/// influence (Town::Process, run in the towns' own loop in the original) and the citadels' (Citadel::Process)
void ProcessTurn();

// ---- the influence circles and the hand that crosses them (InfluenceCircles.cpp) ----

/// g_players_color [0xEA9EFC], written by fn_00826510 (0x826520..0x8265E5): the colour of a player's circle and ripple
/// by colour index, D3DCOLOR 0xAARRGGBB with alpha 0 (the draw writes the alpha). 5 and 7 differ from the generic
/// player colours 0xBFF0B8 (0x4777FF and black there). The colour index is GetRemapedPlayer 0x64D790 of the player:
/// (inferido) the identity, as every openblack user of the player colours (SurfRevol.cpp, TownBelief.cpp) takes it
inline constexpr std::array<uint32_t, 8> k_CircleColours = {
    0x00FF4646u, // red
    0x0047FF54u, // green
    0x00E347FFu, // magenta
    0x0047F9FFu, // cyan
    0x00FFFD47u, // yellow
    0x004664FFu, // blue
    0x00FFA247u, // orange
    0x00FFFFFFu, // white
};

/// One InfluenceCircle of the list [0xEB9A14] (0x38 bytes, new(0x38) at 0x826FB6): the curtain of
/// land_morph::InfluenceCurtain (fn_008265F0) and its overlap state
struct Circle
{
	PlayerNames player;          ///< +0x2C: the colour index & 7 (0x826632)
	glm::vec3 centre;            ///< +0x04..+0x0C: the citadel's / town's Pos (y only counts in the containment test)
	float radius;                ///< +0x10
	land_morph::Curtain curtain; ///< +0x1C positions, +0x20 colours, +0x24 indices, +0x28 UVs (3N + 3 vertices, 4N triangles)
	std::vector<uint8_t> hidden; ///< +0x18: one per column (N + 1); 1 = inside another circle of the player (fn_00827110)
	bool dead {false};           ///< +0x30: inside another circle of the player, deleted by Add (0x826FFA..0x82701F)
	uint32_t alphaCache {0xFFFFFFFFu}; ///< +0x34: the alpha Draw last gave the middle row

	/// +0x14 N, the segments
	[[nodiscard]] size_t Segments() const { return curtain.positions.size() / 3 - 1; }
};

/// fn_00555240(now, last) 0x555240: the circles are rebuilt at the next Update3DInfluence when |now - last| > 0.01
/// ([0x8C5840], strict). Citadel::Process 0x4630C6 (GetInfluence against +0x78) and Town::Process 0x74759E (+0x5C8
/// against +0xF24) call it
void NoteInfluence(float now, float last);
/// GGame::ForceNeedUpdateInfluence 0x555270: the dirty byte g_game+0x250174 = 1. The original calls it from
/// Citadel::ToBeDeleted 0x462C9B, a town changing owner (fn_00649810 0x6499E2), Town::ToBeDeleted 0x739993 and
/// TownCentre::Process 0x743E63. openblack deletes no town or temple and no town changes hands yet: no caller
void ForceNeedUpdateInfluence();
/// GGame::Update3DInfluence 0x555280, from GGame::ProcessTurn 0x54E738: only when the dirty byte is set and
/// GameTurn % 10 == 0 (0x555286). InfluenceCircle::Reset 0x826C50, then for every player GetNextPlayer 0x5508A0 gives
/// (it stops at the neutral one, 0x5508C6) a circle for the citadel when Citadel::GetInfluence 0x464090 is not 0
/// (0x55530E) and one per town of its list (GPlayer +0xA50) whose +0x5C8 is not 0 (0x555354); then the dirty byte is
/// cleared (0x555384)
void Update3DInfluence();
/// The list [0xEB9A14], newest first (the ctor links each circle at the head, 0x826608..0x82661B). Empty without a land
[[nodiscard]] std::span<const Circle> Circles();

/// [0xEB9A1C + 4 player]: the player's border is shown. fn_00828A50 clears the eight of them on every land load (from
/// LH3DIsland::Create 0x803E85; here the land registry's reset); fn_00883120 (vt 0x9A2BFC +0x200, the CITADEL 3D
/// object) sets it when its fade +0x9C reaches 1 (0x8831AD). Until then the player's circles are drawn with alpha 0
/// (0x826F12..0x826F1E) and crossing them makes no ripple and no sound (0x8278E0..0x8278EC)
[[nodiscard]] bool BoundaryShown(PlayerNames player);
/// fn_00883120 0x8831AD: [0xEB9A1C + 4 player] = 1
void ShowBoundary(PlayerNames player);

/// InfluenceCircle::Draw(1) 0x826C90, the world view: nothing at all (no clock step, no material) when g_camera.y
/// [0xEA1DBC] <= 100 ([0x8AB41C], 0x826CA9..0x826CC1); else the middle row's alpha: 120 (0x78, 0x826CA2) from y = 200
/// ([0x8C7B34]) up, below that ftol((y - 100) x 0.01 ([0x8C4B10]) x 120 ([0x8CF338])) (0x826CC7..0x826CF3)
[[nodiscard]] std::optional<uint8_t> CurtainAlpha(float cameraY);
/// InfluenceCircle::Draw 0x826F0D..0x826F57, every circle before it is drawn: when `alpha` differs from its +0x34 and
/// its player's border is shown, +0x34 = alpha and the middle vertex (3j + 1) of every column j <= N not hidden gets
/// (rgb) | alpha << 24 (0x826F3B..0x826F4B); the ground and top rows keep alpha 0
void SetCurtainAlpha(uint8_t alpha);

/// The ripple a hand crossing a border makes (fn_00827250, new(0x68)): 7 LH3DSprites (LH3DSprite::Create(7, 1)
/// 0x8404A0, stride 0x34) of smoke.raw cell 63 in the plane of the border, growing and fading for 2 s
struct Ripple
{
	static constexpr size_t k_Sprites = 7;
	/// (aproximado) the Z-sorter's point: the ctor never writes ripple +0..+8 (0x827250..0x82743F), so the original's
	/// key reads whatever the heap held there; the crossing point here
	glm::vec3 point {0.0f};
	int32_t life {0};    ///< +0x0C, ms (2000 from the ctor, 0x827273). (inferido) an int: g_game_time_inc is taken off it
	uint32_t colour {0}; ///< +0x34: g_players_color[player]; the draw writes its alpha byte +0x37 for each sprite
	/// +0x38..+0x64: rows (c, 0, s), (s, 0, -c), (0, 1, 0) and the point (glm columns), c and s of the yaw: its XZ plane
	/// stands upright along the border
	glm::mat4 matrix {1.0f};
	std::array<float, k_Sprites> sizes {};   ///< each sprite's +0x0C
	std::array<float, k_Sprites> angles {};  ///< each sprite's +0x14
	std::array<int32_t, k_Sprites> flags {}; ///< +0x18[i]: 1 from the ctor, 0 on a wrap; nothing traced reads it
};
/// One sprite of a ripple as fn_00827500 hands it to LH3DSprite::DrawSpecial1 0x840CC0
struct RippleSprite
{
	float size;    ///< +0x0C
	float angle;   ///< +0x14
	uint32_t argb; ///< +0x20: the ripple's colour with this sprite's alpha
};
/// fn_008274A0 for every ripple of [0xEB9A44] (from fn_0x005e5cd0 0x5E6264..0x5E628D), once a frame: life -=
/// g_game_time_inc; below 0 fn_00827450 unlinks it and releases its sprites. The Z object each one left then queues
/// (NewZObject with the callback 0x827500) is the renderer's (Renderer::CollectInfluenceRipples)
void UpdateRipples(uint32_t gameTimeIncMs);
/// The list [0xEB9A44], newest first (the ctor links at the head through +0x10)
[[nodiscard]] std::span<const Ripple> Ripples();
/// fn_00827500, the Z object's callback, for ripple `index` of Ripples(): every sprite grows by g_game_time_inc x 0.001
/// x 10 ([0x8AB414]), wraps past 14 ([0x99C9EC]) and takes the alpha ftol((1 - s / 14) x fade x 255) ([0x8AB270]),
/// fade = life x 0.001 ([0x8AC418]) under 1000 ms, else 1. The sizes are kept: the next draw grows them again
[[nodiscard]] std::array<RippleSprite, Ripple::k_Sprites> DrawRipple(size_t index, uint32_t gameTimeIncMs);

/// fn_0x005e5cd0 0x5E61A1..0x5E6230, once a frame while the game is not paused (g_game+0x14 & 4): the hand's point goes
/// to fn_00827820, which compares it against the circles GGame::Update3DInfluence 0x555280 keeps (one per citadel and per
/// town with influence) and, for each player whose "the hand is inside" changed since the last frame and who has a
/// circle whose edge the hand crossed since the previous call (fn_008277B0), makes a ripple at the crossing; then
/// G_HandThroughInfluence_01 (InGame 52) plays once, 3D at the hand
void ProcessHandCrossing(const glm::vec3& handPosition);

/// fn_00827820 without the sound: true when it set [0xEB9A6C] (a crossing made a ripple)
[[nodiscard]] bool HandCrossedInfluence(const glm::vec3& handPosition);

namespace detail
{
/// For the tests only: the statics of fn_00827820 ([0xEB9A48], [0xEB9A68], [0xEA9EF0]) back to the process start;
/// nothing in the original clears them
void ResetHandCrossing();
/// fn_00827670(circle, inside, outside): the point of the circle's edge between the two (both at y = 0), by halves:
/// while dx^2 + dz^2 > 1 ([0x8AA390]) the midpoint (a + b) x 0.5 ([0x8AA3B4]) replaces the end on its side
/// (InfluenceCircle Contains, fn_00827210); then the midpoint of the last two
[[nodiscard]] glm::vec3 CrossingPoint(const glm::vec3& centre, float radius, glm::vec3 inside, glm::vec3 outside);
} // namespace detail

// ---- the land's globals (GGame fields) ----

/// g_game+0x205A08, SET_LAND_NUMBER (0 = no story land): Game::GetMapScriptGlobals().landNumber
[[nodiscard]] int32_t LandNumber();
/// g_game+0x250078 / +0x25007C, SET_TOWN_INFLUENCE_MULTIPLIER / SET_PLAYER_INFLUENCE_MULTIPLIER (1 before each map
/// script): Game::GetMapScriptGlobals()
[[nodiscard]] float TownInfluenceMultiplier();
[[nodiscard]] float PlayerInfluenceMultiplier();
/// g_game+0x14 & 0x2000, set at start from the registry value "GatheringFlag" (start_system 0x6433B1): every player
/// has influence 1 everywhere. Only a test hook or a mod sets it.
void SetInfluenceEverywhere(bool on);
[[nodiscard]] bool IsInfluenceEverywhere();

/// OPENBLACK_TEST_INFLUENCE / OPENBLACK_INFLUENCE_EVERYWHERE (InfluenceDebugHooks.cpp); call once per turn
void RunDebugHooks();
} // namespace openblack::influence
