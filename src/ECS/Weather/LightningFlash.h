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

#include <glm/vec3.hpp>

#include "Storms.h"

// The lightning's light flash of the registered storms (the 0x24-byte object at GWeather +0x70, fn_00837110..
// fn_008372D0) and what the camera sees of it (LH3DAtmos::Update3D 0x8357A0 -> [0xFA2768]). Wiki: docs/bw1-notes/magic.md,
// "Tormenta" (the flash).

namespace openblack::weather
{
namespace flash
{
/// fn_00837290 (pos, radius, intensity): +0x20 intensity, age f1 f3 0, active, the point and the radius
void Start(storms::Storm::Flash& flash, const glm::vec3& position, float radius, float intensity);
/// fn_008372D0 (seconds), every GWeather::Update: age += seconds; an active flash past 0.8 s (0x8C4A04) goes off
void Age(storms::Storm::Flash& flash, float seconds);
/// fn_00837200, every frame from LH3DAtmos::Update3D for each storm not marked for deletion: active -> s = 1 - age,
/// f1 = s, f3 = s^3, both 0.1 (0x3DCCCCCD) while 0.2 < age < 0.5 (0x8AB244 / 0x8AA3B4), then x intensity; off -> 0, 0.
/// Returns f3, the strength of the land light stamp fn_0086CFF0 (64 x 64 radial bitmap 0xED92F0, mode 1), when active.
float Frame(storms::Storm::Flash& flash);

/// fn_00837110's bitmap (0xED92F0, 64 x 64, built once for all flashes, refcount 0xEDD3A8): at d = |(x, y)|, x, y in
/// -32..31, ftol((32 - d) x 9) capped at 255 inside d < 32 (0x8CF134, 0x8FFEC8), 0 outside; the three channels equal
[[nodiscard]] uint8_t BitmapTexel(int x, int y);

/// LH3DAtmos::Update3D 0x8357D8..0x835903: the storm nearest the camera in x, z (descriptor position, squared distance,
/// the first of equals; only those not marked for deletion), "inside" when its d^2 < ((inner + outer) / 2)^2 of the
/// descriptor's radii — a flag that stays set when a later, nearer storm is outside (bl at 0x83585C is never cleared) —
/// and then [0xFA2768] = inside ? ftol(clamp(nearest flash f1, 0, 1) x 255) : 0
[[nodiscard]] uint8_t AtCamera(const glm::vec3& camera);
/// Every frame (weather::UpdateFrame, the first part of LH3DAtmos::Update3D): fn_00837200 of every storm not marked
void UpdateFrame();
} // namespace flash

/// [0xFA2768] of the last frame: the flash of the nearest storm at the camera, 0..255 (for the sky and the water: agua's
/// 3D/SkyWeather and LandLightTable::Build read it)
[[nodiscard]] uint8_t LightningFlashAtCamera(const glm::vec3& camera);
} // namespace openblack::weather
