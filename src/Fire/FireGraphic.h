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
#include <list>
#include <optional>
#include <span>

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

namespace openblack::graphics
{
struct ObjectListFrame;
} // namespace openblack::graphics

namespace openblack::ecs::fire
{
struct FireEffect;

namespace graphic
{
struct SpritePos;
struct Graphic;

/// The colours the fire's sprites are drawn with: orange flames, white steam and grey smoke
constexpr std::array<uint8_t, 3> k_FlameColour {0xFF, 0x71, 0x3C};
constexpr std::array<uint8_t, 3> k_SteamColour {0xFF, 0xFF, 0xFF};
constexpr std::array<uint8_t, 3> k_SmokeColour {0x70, 0x70, 0x70};

/// Whether a fire with no steam burst going starts one: cooled this turn (by water, rain or snow, or cooling of itself),
/// above 75 degrees, and hotter than when its last burst started
[[nodiscard]] bool StartsSteam(uint8_t fireFlags, float temperature, float lastSteamTemperature);
/// The most flames a fire shows at once: 2 on any kind of tree; otherwise 2 when the larger of its 2D radius and its
/// height is under 3, else 7
[[nodiscard]] int MaxFlames(bool anyTree, float radius2D, float height);
/// The range of the draw that picks the bone an animated body's flame sits at: its bone count's low byte, as the
/// original takes it
[[nodiscard]] constexpr int32_t FlameBoneRange(size_t boneCount)
{
	return static_cast<int32_t>(boneCount & 0xFFu);
}
/// The flames' size, a share of the object's height: 0.2 on any kind of tree, 0.3 on a complex model (the creature) or a
/// citadel part (the temple and the things of its type), 0.5 on anything else
[[nodiscard]] float LocalFlameScale(bool anyTree, bool complexOrCitadelPart, float height);
/// LocalFlameScale for an object of the world, by its kind and its height: the size its flames are made at
[[nodiscard]] float ObjectFlameScale(entt::entity object);
/// A flame's alpha at an age: it fades in over its first second and out over the rest of its 4.3 s, at most 250,
/// truncated. nullopt once it is older than 4.3 s: it is gone
[[nodiscard]] std::optional<uint8_t> FlameAlpha(float age);
/// How many flames a fire wants by now, in `accumulator` (flames are added while the count is below it): while it
/// burns (`fraction` not 0), its most flames over a flame's 4.3 s life, times the fraction and `dt`; the first turn it
/// is very hot, its most flames once more (`veryHotDone` remembers it); the turn it catches, half its most flames
/// rounded up
void AccumulateFlames(float& accumulator, bool& veryHotDone, int maxFlames, float fraction, float dt, bool veryHot,
                      bool justIgnited);
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
/// A drawn object's place outside the object list's parts (held in the hand, out of the map): after all of them
constexpr uint32_t k_DrawnOutsideTheList = UINT32_MAX;
/// Whether a burning object's draw runs this frame, so that it draws its fire, and its place in the frame's object draw:
/// the object list's objects in the order it drew them, then its tail's, then the physics objects', then
/// k_DrawnOutsideTheList for any other; none for an object the list leaves out (as its model is) or a physics object
/// sunk wholly under the water, which nothing draws. An object in two parts keeps the first
[[nodiscard]] std::optional<uint32_t> DrawRank(const graphics::ObjectListFrame& list, entt::entity object);
/// Once a frame, before the draw: which fires are drawn (their objects' draws run, DrawRank), in what order they go
/// to the depth sort, and the origin each one's steam and smoke are drawn with (CarrySpriteOrigin, from and back into
/// the fire system's SpriteOriginY), with the draw's camera at `camera`
void PrepareDraw(const graphics::ObjectListFrame& list, const glm::vec3& camera);
/// The y origin the shared fire sprite is left with once the fire's flames are drawn: each flame sets it to its height
/// (2) x its drawn size (the flame's scale x the graphic's multiplier) x -1, newest first, so the oldest flame's stays;
/// `carried` (what the flames drawn before left) with no flame. The steam and smoke are drawn with it
[[nodiscard]] float SpriteOriginYAfterFlames(const Graphic& graphic, float carried);
/// The drawn fires, in the order they go to the depth sort, with the point each one's Z object is keyed at: in the order
/// the depth sort draws them (far to near, equal keys in the order they came), each one's puffOriginY is the origin the
/// flames drawn before its steam left (SpriteOriginYAfterFlames from `carried`), and `carried` ends as the last fire
/// leaves it
void CarrySpriteOrigin(std::span<Graphic* const> inSubmitOrder, std::span<const glm::vec3> keyPoints, const glm::vec3& camera,
                       float& carried);
/// The glow's noise at the turn time (turn + fraction): the lattice noise blended linearly
/// (particles::noise::LinearValueNoise) at 0.6 x the turn time + the fire's low 16 bits of identity, each step rounded
/// to a float
[[nodiscard]] float GlowNoise(float turnTime, uint32_t id);
/// The strength the light map under a big fire is stamped into the land's light with, at the turn time (turn +
/// fraction): 0.6 x the fire's fraction x (1 + 0.2 GlowNoise) x (1 - charring), each step rounded to a float, at most 1;
/// none when it is not above 0 (or compares with nothing): no stamp
[[nodiscard]] std::optional<float> LightMapAlpha(float fireFraction, float charring, float turnTime, uint32_t id);
/// The glow, (1 - charring) x clamp(0.001 T (1 + 0.2 GlowNoise)) x 255 truncated, as RGB(c 180 / 256, c 60 / 256, 0);
/// a value that compares with nothing counts as 0
[[nodiscard]] glm::u8vec3 CharringGlow(const FireEffect& fire, float turnTime);
} // namespace graphic
} // namespace openblack::ecs::fire
