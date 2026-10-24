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

#include <list>
#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

// The fire graphic (a particle system): the burning object's flames (S_Fire.raw, mode 13), the steam of a hot object
// being cooled and the smoke of a fire that went out (S_SpriteSheet3.raw, modes 13 and 6). It is updated with the frame
// time when drawn and Z-sorted as one object. Also the tint the fire gives the burning object (tree colour, charring,
// glow).

namespace openblack::audio
{
struct PlayOptions;
} // namespace openblack::audio

namespace openblack::ecs::fire
{
struct FireEffect;

namespace graphic
{
struct SpritePos;

/// Whether a fire with no steam burst going starts one: cooled this turn (by water, rain or snow, or cooling of itself),
/// above 75 degrees, and hotter than when its last burst started
[[nodiscard]] bool StartsSteam(uint8_t fireFlags, float temperature, float lastSteamTemperature);
/// The most flames a fire shows at once: 2 on any kind of tree; otherwise 2 when the larger of its 2D radius and its
/// height is under 3, else 7
[[nodiscard]] int MaxFlames(bool anyTree, float radius2D, float height);
/// A flame's alpha at an age: it fades in over its first second and out over the rest of its 4.3 s, at most 250,
/// truncated. nullopt once it is older than 4.3 s: it is gone
[[nodiscard]] std::optional<uint8_t> FlameAlpha(float age);
/// Whether a burst of steam or smoke that started on turn `start` is over on turn `turn`: it goes on for the 30 turns
/// after its first
[[nodiscard]] bool BurstOver(uint32_t start, uint32_t turn);
/// The puffs a going burst lets out this frame: 4 a second of `dt` are added to `accumulator`, and `count` goes up by
/// one until it is no longer below it (so the first comes at once); the number it went up by
[[nodiscard]] int PuffsDue(float& accumulator, int& count, float dt);
/// The hiss a burst of steam starts with: the in-game steam sample (0x35, G_Steam_01) owned by the fire graphic's sound
/// number `owner`, in 3D at `at` and not following its owner
[[nodiscard]] audio::PlayOptions SizzleOptions(uint32_t owner, const glm::vec3& at);
/// One frame of the puffs of the steam (while the object is cooled, white: alpha0 100, rise 1) or the smoke (once it
/// went out, grey: alpha0 180, rise 2): each ages by dt and goes after 3 s; it grows from 0.2 to 2.6 times its base
/// scale and fades from alpha0 to 0, rounded to the nearest; its velocity eases towards half the wind, by a tenth of
/// the difference a second, and rises; it moves by its velocity times dt times its base scale
void StepPuffs(std::list<SpritePos>& puffs, float dt, float alpha0, float rise, const glm::vec3& wind);
/// FireGraphic's draw flags for a new fire on `object`: bit 0 morphed with the land, 1 flames, 2 smoke, 3 steam, 4 the
/// light map
[[nodiscard]] uint8_t InitialFlags(entt::entity object);
/// Whether a new fire's graphic lights the land around it (the S_LMFireBall light map): only on a multi-cell fixed
/// object (a building, a field, a rock...) whose 2D radius is over 2
[[nodiscard]] bool GetsLightMap(bool multiCellFixed, float radius2D);
/// Only for an object with a 3D object
void Create(FireEffect& fire);
void Destroy(FireEffect& fire);
/// Updates every fire, `seconds` of frame time
void Update(float seconds);
/// The turn hook of the fire graphics: OPENBLACK_FIRE_TRACE (the bursts read game_clock::Turn())
void SetTurn(uint32_t turn);
void Clear();

/// The colour a burning tree is drawn with (x/256 per channel of its colour):
/// min(life > 0.9 ? max(50, 255 - (1 - life) 2550) : 50, ecs::TreeBrightness()). nullopt when the object has no fire.
[[nodiscard]] std::optional<glm::u8vec3> TreeDrawColour(entt::entity object);
/// How much narrower a burning tree is drawn across (not in height) below 0.2 life: 1 - (0.2 - life) x 5, each step
/// rounded to float (0 at no life). nullopt from 0.2 up: drawn at its own width
[[nodiscard]] std::optional<float> BurntTreeWidth(float life);
/// The ALPHAREF a burning tree's draw forces on every alpha tested primitive of the tree (render_modes::AlphaRef
/// `forced`), from its fire's temperature T and its combustion temperature Tc: the heat is 255 above 1.5 Tc, else
/// (T - Tc) x 255 / (0.5 Tc) truncated toward zero; then 230 + 25 x heat / 255, at most 254, truncated toward zero
/// (180 for a cold tree, 230 at Tc, 254 from about 1.5 Tc). Every step rounded to float
[[nodiscard]] uint8_t BurningTreeAlphaRef(float temperature, float combustion);
/// BurningTreeAlphaRef of a tree with a fire; nullopt when the object has no fire
[[nodiscard]] std::optional<uint8_t> TreeAlphaRef(entt::entity object);
/// The charring grey 255 - ceil(int(charring x 255) x 175 / 256) (80 when fully charred)
[[nodiscard]] uint8_t CharringGrey(const FireEffect& fire);
/// The glow, (1 - charring) x clamp(0.001 T (1 + 0.2 noise)) x 255 as RGB(c 180 /
/// 256, c 60 / 256, 0)
[[nodiscard]] glm::u8vec3 CharringGlow(const FireEffect& fire, float turnTime);
} // namespace graphic
} // namespace openblack::ecs::fire
