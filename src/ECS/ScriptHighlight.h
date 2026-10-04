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
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "3D/Billboard.h"
#include "Enums.h"

/// ScriptHighlight (runblack.exe W120, ScriptHighlight.cpp 0x7095D0..0x70AE60; class layout in
/// ECS/Components/ScriptHighlight.h): the challenge scrolls and the did-you-know signs. A script makes one with
/// CREATE_HIGHLIGHT 272 (GScript::CreateHighlight 0x6F1C20), names it with HIGHLIGHT_PROPERTIES 334 (0x6FE1F0), lifts
/// it with SET_PROPERTY YPOS (0x70EF9B), lights it with SET_ACTIVE 255 (0x6FD720) and waits for GAME_THING_CLICKED 016
/// (0x70AEB0) on it; SET_DRAW_HIGHLIGHT 306 (0x708CB0) hides the scrolls. The finders see it as SCRIPT_OBJECT_TYPE 37
/// (0x70AE30) with its info row as the sub-type (fn_006F6D00 case 17), so FollowUs' CALL_NEAR(HIGHLIGHT, 1, ...) at
/// CHL L53068 / L53085 finds DidYouKnow's sign. Research: dev\documentacion\intro\spec_highlight.md.
namespace openblack::ecs::script_highlight
{

/// The four GScriptHighlightInfo rows of W120 (GetBaseInfo 0x709640: 4 at 0xD96390, 0x110 bytes; info.dat order).
/// The CHL HIGHLIGHT_INFO enum of the later games (bronze, silver, gold, scoreboard...) does not match them
enum class Info : uint32_t
{
	Bronze = 0,     ///< "Script Hightlight": MSH_I_MINISCROLL / _ACTIVE, no particles
	DidYouKnow = 1, ///< "Script Did You Know Sign": MSH_O_INFO_SIGN (both), no particles (IsDidYouKnow 0x70AC20)
	Silver = 2,     ///< "Script Hightlight Silver inactive": MINISCROLL_SILVER / _ACTIVE, glints 100, active 103
	Gold = 3,       ///< "Script Hightlight gold inactive": MINISCROLL_GOLD / _ACTIVE, glints 99, active 102
};
constexpr uint32_t k_InfoCount = 4;
/// GetSaveType 0x709850
constexpr uint32_t k_SaveType = 0x3F;
/// The glow object's mesh (CallVirtualFunctionsForCreation 0x709B07: MeshPack[0x20F], MSH_S_BLAST_CENTRE)
constexpr uint32_t k_GlowMesh = 0x20F;
/// The sprite's texture (fn_0057D590: LH3DRender::CreateMaterial(13, ".\data\textures\S_SpriteSheet3.raw"), material
/// +5 |= 1), cell 0 (LH3DSprite::Create(1, 1) 0x8404A0: one sprite, +0x28 |= 0x80)
constexpr const char* k_SpriteTexture = "S_SpriteSheet3";
/// The sprite's render mode (fn_0057D5A4 push 0xD)
constexpr uint32_t k_SpriteRenderMode = 13;

// ---- Pure rules (test_script_highlight) -----------------------------------------------------------------------------

/// IsDidYouKnow 0x70AC20: the row index == 1 (`dec; neg; sbb; inc`)
[[nodiscard]] constexpr bool IsDidYouKnowInfo(uint32_t index)
{
	return index == static_cast<uint32_t>(Info::DidYouKnow);
}
/// Draw 0x70A3E2..0x70A416: the sprite's alpha by row, `dec eax` three times: 1 -> 0x32, 2 -> 0x96, 3 -> 0x64, else 0
[[nodiscard]] uint8_t SpriteAlpha(uint32_t index);
/// Draw 0x709E5D..0x709EA6: d = ftol(GetDistance(camera, point)) bound to [10, 30] (unsigned `jbe` / `jae`); a
/// scroll's scale is scale x d x (1 / 30) [0x8CF3F8], a did-you-know keeps its scale
[[nodiscard]] float DistanceScale(float scale, float distanceToCamera, bool didYouKnow);
/// Draw 0x709CE8..0x709DBD for a turning highlight: the 3D object's Y angle + g_game_time_inc x 0.00314159 [0x942AF4],
/// then minus ftol(a x (1 / 2 pi) [0x8C6CAC]) x 2 pi [0x8AB210] (__ftol truncates)
[[nodiscard]] float SpinAngle(float angle, uint32_t frameMs);
/// fn_0070AC50: the challenge ids whose scroll saves the game when it is clicked (GAME_THING_CLICKED 0x70AF67): 0x38,
/// 0x3B, 0x3C, 0x3D
[[nodiscard]] bool SavesGameWhenClicked(uint32_t scriptId);
/// InterfaceValidToTap 0x70ADD0: a did-you-know with a script id; a scroll only while active and with a script id
[[nodiscard]] bool ValidToTap(bool didYouKnow, bool active, uint32_t scriptId);
/// GetOverwriteTapToolTip 0x70AE10: 0xEF2 when the info's OBJECT_TYPE (+0x10) is 1, else 0 (every W120 row is 35: 0)
[[nodiscard]] uint32_t OverwriteTapToolTip(ObjectType infoType);

/// The shared pulse of ProcessHighlights 0x70A460 ([0xD967D4] phase, [0xD967D8] value, [0xD967DC] last value; all 0
/// from OnClearMap 0x7096E0)
struct Pulse
{
	float phase {0.0f};
	float value {0.0f};
	float previous {0.0f};
};
/// 0x70A463..0x70A4D6: phase += msPerTurn [0xD01A38] x 5 [0x942244] x 0.001, less 2 pi once when above (`test ah,
/// 0x41`); previous = value; value = (1 - cos phase) x 0.5
void StepPulse(Pulse& pulse, uint32_t msPerTurn);
/// fn_0070A510: lerp(previous, value, turn fraction g_game+0x205D64), bound to [0, 1], x 0.6 + 0.4
[[nodiscard]] float ActivePulse(const Pulse& pulse, float turnFraction);
/// Draw 0x70A0BD..0x70A0E6: the glow's ARGB, 0x14B4DCFF for Silver (row 2), else ((byte)ftol(ActivePulse) x 0x50) << 24
/// | 0xFFFF00 (literal: the pulse is 0.4..1, so the alpha is 0x50 only at 1.0, 0 otherwise)
[[nodiscard]] uint32_t ActiveGlowArgb(uint32_t index, float activePulse);

/// The did-you-know texts already read (fn_0078CC10 asks, fn_0078CD20 adds; HelpSystem::SetBubbleProperties 0x5C8330):
/// one list of at most 48 (`cmp edx, 0x30`) per DYK_CATEGORY 0..4 (counts [0xE01E6C + 4 c], entries 0xDF9D1C,
/// 0xDF5C58, ...). (pending) where the original saves them (the help profile)
namespace dyk_read
{
[[nodiscard]] bool IsRead(uint32_t text, DykCategory category);
/// fn_0078CD20 (not read in detail: inferred an append when the list has room)
void MarkRead(uint32_t text, DykCategory category);
/// The sum of the five counts (SetBubbleProperties 0x5C834F..0x5C8373: 0 -> "FirstDYKExplained")
[[nodiscard]] uint32_t Total();
void Clear();
} // namespace dyk_read

// ---- The highlights ------------------------------------------------------------------------------------------------

/// ScriptHighlight::Create 0x709A40(coords, info, scriptId, yAngle, scale): new (0x8C bytes), the ctor 0x7098A0
/// (FixedObject 0x52DDC0, the zeros of fn_00709910, the head of the list g_game+0x205C94, +0x78 = scriptId), then
/// CallVirtualFunctionsForCreation 0x709AA0. entt::null for an info index past the 4 rows (openblack: the original
/// reads past the array)
entt::entity Create(const glm::vec3& position, uint32_t infoIndex, uint32_t scriptId, float yAngle, float scale);

[[nodiscard]] bool IsHighlight(entt::entity thing);
/// The info row (fn_006F6D00 case 17); script_type::k_NoSubtype for a thing that is not a highlight
[[nodiscard]] uint32_t InfoIndexOf(entt::entity thing);
[[nodiscard]] bool IsDidYouKnow(entt::entity thing);
[[nodiscard]] bool IsActive(entt::entity thing);
/// +0x78 (0 for a thing that is not a highlight)
[[nodiscard]] uint32_t ScriptIdOf(entt::entity thing);
/// SetScriptId 0x709A20: +0x78 = scriptId, +0x84 = category
void SetScriptId(entt::entity thing, uint32_t scriptId, DykCategory category);
/// SET_PROPERTY YPOS on a highlight (0x70EF9B..0x70EFBF): SetDrawHeight 0x709C40 (+0x7C = 1, +0x80 = h), then the
/// Pos.altitude +0x1C = h and the 3D object moved there
void SetYPos(entt::entity thing, float height);
/// GET_PROPERTY YPOS (0x70E70A): Pos.altitude +0x1C
[[nodiscard]] float GetYPos(entt::entity thing);
/// SetActivated 0x70A630
void SetActivated(entt::entity thing, bool on);

/// ProcessHighlights 0x70A460 (GGame::ProcessTurn 0x54E6D5, after Bookmark::ProcessAll and before
/// GClimate::ProcessAll): the pulse, then Process 0x70A580 for every available highlight of the list, from its head
void ProcessHighlights();
/// The per-frame half of Draw 0x709C60 that moves the 3D object (its angle, scale and position) and the effects
void UpdateFrame(uint32_t frameMs, const glm::vec3& eye);

/// What Draw adds around the mesh this frame, for the renderer and the interface (pending: nothing draws these yet)
struct DrawExtras
{
	bool drawn {false};                                    ///< false: Draw returned before SingleMapFixed::Draw
	glm::vec3 centre {0.0f};                               ///< the mesh's centre in the world
	float radius {0.0f};                                   ///< |the 3D object's matrix x the mesh's half extents|
	std::optional<graphics::billboard::Sprite> sprite;     ///< +0x64, Screen mode (AddDrawing 0x70A447)
	std::optional<glm::mat4> glow;                         ///< +0x68 (mesh k_GlowMesh), an active scroll only
	uint32_t glowArgb {0};                                 ///< its colour (ActiveGlowArgb)
	std::optional<float> clickRadius;                      ///< SendInvisibleDrawCollision(this, centre, r + 0.5)
};
[[nodiscard]] DrawExtras ExtrasOf(entt::entity thing, const glm::vec3& eye);

/// InterfaceTap 0x70AC70 (a hand tap on it, GInterface fn_005D38A0 -> packet 0x20); `byLocalPlayer`: the tapping
/// status is GGame::MyInterfaceStatus (0x70ACFE) and its player the local one (0x70ACA7). Always 1
bool InterfaceTap(entt::entity thing, bool byLocalPlayer);

/// ToBeDeleted 0x709980: out of the list, the two effects closed (and the +0x74 particle), then Object::ToBeDeleted
void OnToBeDeleted(entt::entity thing);
/// OnClearMap 0x7096E0 (GGame::ClearMap, after Bookmark::ClearAll): the pulse back to 0; openblack also empties its
/// list
void OnClearMap();
/// The list g_game+0x205C94 (head first)
[[nodiscard]] const std::vector<entt::entity>& All();

} // namespace openblack::ecs::script_highlight
