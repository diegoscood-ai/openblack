/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/vec3.hpp>

namespace openblack::ecs::smoky_stuff
{

/// SmokyStuff (LH3D, 0xCC bytes, list 0xEB99CC): a puff of 15 camera-facing smoke.raw sprites (material [0xEA1ABC],
/// render mode 6) flying apart from a point while they grow and fade. Made by SmokyStuff::Create 0x823C90 (the boat's
/// dust and splashes, Object::CreateSmokyStuff 0x63A810...), moved and drawn once per frame by fn_00824140 from
/// fn_005E5CD0 0x5E619C.
struct Puff
{
	glm::vec3 position; ///< sprite +0x00
	glm::vec3 velocity; ///< +0x00 + 12 i of the SmokyStuff, units per second
	float angle {0.0f}; ///< sprite +0x14: the turn on the screen
	float half {1.0f};  ///< sprite +0x0C: half the side
	uint8_t cell {0x10}; ///< sprite +0x28 & 0x3F: cell of smoke.raw's 8 x 8 sheet
	uint32_t argb {0xFFFFFFFFu}; ///< sprite +0x20
};

struct Cloud
{
	static constexpr size_t k_Puffs = 15;
	std::array<Puff, k_Puffs> puffs {};
	float life {1.0f};      ///< +0xBC: 1 when made, gone below 0
	int32_t mode {0};       ///< +0xC0: the second argument of Create (0 for every caller here)
	float size {1.0f};      ///< +0xC4
	uint32_t colour {0};    ///< +0xC8: 0xAARRGGBB, its rgb replaces the puffs' unless it is -1
};

/// Random(a, b) 0x81D180: a + (b - a) x rand() / 32768 (rand() as MSVC's, from its own seed)
[[nodiscard]] float Random(float from, float to);

/// SmokyStuff::Create 0x823C90(pos, mode, size, colour), mode 0: each puff at pos + (c, b, a) with a, b, c =
/// Random(-size, size), turned Random(0, 2 pi), cell 0x10, flying along norm(e, size, d) (d, e = Random(-size, size)) at
/// Random(0.3, 1) x size units per second. Mode != 0 (0x823DA7, the RootsPile's dust of ECS/RootsPile): 1.5 x size
/// along that direction.
void Create(const glm::vec3& position, int32_t mode, float size, uint32_t colour);

/// fn_00824140 (dt = g_game_time_inc x 0.001) -> fn_00823F70 per SmokyStuff, then the ones with life < 0 are freed:
/// life -= dt / 3 (mode 0; 2 dt / 3 otherwise); nothing is drawn once life <= 0. Colour: mode 0 (life x 100) << 24 |
/// 0x808080 (else min(life / 0.7, 1) x 255 << 24 | 0x68503D), the rgb of `colour` over it; per puff angle = +-5 life +
/// velocity.x (- when velocity.x > velocity.z), half = ((1 - life) x 2 + 1) x size / 2 (at least 0.0001),
/// position += velocity x dt, cell = (int)(life x 15).
void Update(float seconds);

[[nodiscard]] const std::vector<Cloud>& Get();

/// Every SmokyStuff goes (a new map: the LH3D list is emptied with the landscape)
void Clear();

} // namespace openblack::ecs::smoky_stuff

namespace openblack::ecs
{

/// Object::CreateSmokyStuff 0x63A810 (research dev\tmp_dis\animals\misc.md §3): the puff an object leaves when it goes,
/// SmokyStuff::Create(point, type 0, size, colour 0xFFFFFFFF, so the grey 0x808080 of type 0 stays). The point is
/// usually half the object's height up; `size` is the original's size argument (an animal corpse: 1). Used by the
/// animal corpses that time out; the original also uses it for villagers, buildings and spells. The puffs live in
/// smoky_stuff (moved by fn_00824140 in ecs::petit_navire's frame step, drawn with the boat's sprites).
class SmokyStuff
{
public:
	static void Create(glm::vec3 at, float size) { smoky_stuff::Create(at, 0, size, 0xFFFFFFFFu); }
	static void Clear() { smoky_stuff::Clear(); }
	SmokyStuff() = delete;
};

} // namespace openblack::ecs
