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
#include <vector>

// The recogniser's keypoint lists (GestureSystemData, 0x65C bytes) and the templates of Data\Gestures.jty
// (GestureSystemDataList::Load 0x579AF0). Wiki: docs/bw1-notes/magic.md, "Gestos".

namespace openblack::magic::gestures
{
/// GESTURE_TYPE as the recogniser stores it (a byte). 22 and 23 are not in openblack's GestureType (Creature Isle), but
/// Gestures.jty has templates for them.
using Gesture = uint8_t;
constexpr Gesture k_None = 0;
constexpr Gesture k_Spiral = 1;
constexpr Gesture k_InverseSpiral = 2;
constexpr Gesture k_Circle = 4;
constexpr Gesture k_Scribble = 5;
constexpr Gesture k_RShape = 14;
constexpr size_t k_GestureCount = 24; ///< one entry per gesture: GInterface activeGesture[24] +0x346, LookingFor lf[24]

constexpr size_t k_MaxSamples = 80;

/// One keypoint (0x14 bytes): the screen position (x, y unused, z = screen y down), the signed turn there (rad, positive
/// = clockwise on the screen) and the octant of the segment that leaves it (0 up, 2 right, 4 down, 6 left)
struct KeySample
{
	float x {0.0f};
	float y {0.0f};
	float z {0.0f};
	float turn {0.0f};
	uint32_t direction {0};
};

/// GestureSystemData (vtable 0x8DF7E0): a template, or the keypoints of the live buffer (BuildFromSystem 0x578C20)
struct GestureData
{
	std::array<KeySample, k_MaxSamples> samples {}; ///< +0x08
	uint8_t count {0};                              ///< +0x648
	Gesture gesture {k_None};                       ///< +0x649
	uint8_t positionMode {0};                       ///< +0x64A: 2 in every template (fn_0057A5E0)
	float aspect {0.0f};                            ///< +0x64C
	bool checkDirection {false};                    ///< +0x650
	bool allowReverse {false};                      ///< +0x654 (really "allow mirrored")
	bool checkAspect {false};                       ///< +0x658

	/// SetToZero 0x578BE0
	void SetToZero();
	/// fn_00578D30: the next keypoint (no bounds check in the original; 80 at most here)
	void Append(const KeySample& sample);
};

/// GestureSystemDataList::Load 0x579AF0 / the record reader fn_005790A0: u32 count, then count records of 0x65C bytes
/// (80 samples, then u32 count, gesture, positionMode (low bytes), checkDirection, allowReverse, checkAspect, f32 aspect).
/// False if the data is short.
bool LoadTemplates(const std::vector<uint8_t>& bytes, std::vector<GestureData>& out);

/// The game's list (g_game +0x250064), read once from Data\Gestures.jty (GGame::LoadFiles); empty if missing
[[nodiscard]] const std::vector<GestureData>& Templates();
/// Tests: replace the list
void SetTemplates(std::vector<GestureData> templates);
} // namespace openblack::magic::gestures
