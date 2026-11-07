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
#include <functional>
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "GestureTemplates.h"
#include "PowerUpSystem.h"

// The gesture icons at the bottom right of the screen: one for each gesture the interface's pass looks for, its symbol
// in the player's colour and sparkles running along its shape, fading out while a help text is on the screen and back
// in after it. The pass's table of gestures looked for makes the list; the draw moves the fade and the sparkles on once
// a frame. Wiki: docs/wiki/magic/gestures.md#the-gesture-icons.

namespace openblack::magic::gestures
{
/// The icons are drawn this far along the view, in the plane of the screen
constexpr float k_IconDepth = 10.0f;
/// The symbol's half size there, and a sparkle's
constexpr float k_IconSymbolHalfSize = 0.2f;
constexpr float k_IconSparkleHalfSize = 0.05f * 2.0f;
/// The sparkle's cell of S_SpriteSheet2, eight cells a row
constexpr uint8_t k_IconSparkleCell = 0x28;
/// The symbol's sheet: S_Gesture<gesture / 16>, four cells a row, the gesture's cell gesture % 16
constexpr uint8_t k_IconSymbolCellsPerRow = 4;
/// A sparkle's colour before its fade: white at that alpha
constexpr uint32_t k_IconSparkleArgb = 0x96FFFFFFu;

/// The pixel box of the icon in a slot (0 at the right) on a screen of that size: square, 3.5% of the width across, on
/// the bottom edge, the slots 4% of the width apart
struct IconBox
{
	glm::vec2 min {0.0f};
	glm::vec2 max {0.0f};
};
[[nodiscard]] IconBox IconBoxOf(int slot, glm::ivec2 screen);
/// The pixel of a point of the box given as fractions of its width and height (x, z), each cut to a whole pixel
[[nodiscard]] glm::ivec2 IconPixel(const IconBox& box, float u, float v);

/// One icon
struct GestureIcon
{
	Gesture gesture {k_None};
	int order {0}; ///< the table's type: icons of a higher type come first
	int seed {-1};
	int powerUp {-1};
	int leash {-1};
	IconBox box;             ///< its place, for the screen the list was made on
	uint32_t symbolArgb {0}; ///< the player's colour, opaque, when the list was made
	/// Seconds for a sparkle to run the shape once: the shape's length over the circle's, twice
	float period {0.0f};
	float phase {0.0f};   ///< 0..1 along the shape
	float seconds {0.0f}; ///< how long it has been drawn
};

/// The icons the pass's table asks for, in their order
class GestureIconList
{
public:
	/// The table the pass leaves at its end. The same table as the last one changes nothing; any other makes the list
	/// again: an icon for each gesture whose type is not 0, ordered by type, the highest first and, of the same type, the
	/// later gesture first, each placed in its slot of this screen with the player's colour and its shape's length
	/// against the circle's. Whether the list was made again
	bool Update(const LookingForArray& lf, glm::ivec2 screen, uint32_t playerRgb,
	            const std::function<float(Gesture)>& shapeLength);
	/// The map is cleared: no icons, and the table kept is one with nothing looked for
	void Clear();
	[[nodiscard]] std::span<const GestureIcon> Icons() const { return _icons; }
	[[nodiscard]] std::span<GestureIcon> Icons() { return _icons; }

private:
	LookingForArray _kept = NothingLookedFor();
	std::vector<GestureIcon> _icons;

	[[nodiscard]] static LookingForArray NothingLookedFor();
};

/// The icons' alpha, once a frame before they are drawn. With a help system, while a help text is drawn on the screen
/// it goes down by 2 a second, and nothing is drawn the frame it reaches 0; with none drawn it goes up by 2 a second,
/// to 1. Whether the icons are drawn this frame: the alpha is not 0
[[nodiscard]] bool StepIconsAlpha(float& alpha, bool helpSystem, bool helpTextShown, uint32_t frameMs);

/// A drawn icon's sparkles this frame: its time and its phase move on by the frame, a phase past 1 goes back by 0.9,
/// and it is kept within 0..1. One sparkle at the phase and, while the phase is past 0.9, a second one 0.9 behind it
struct IconSparkles
{
	std::array<float, 2> at {};
	int count {0};
};
[[nodiscard]] IconSparkles StepIconSparkles(GestureIcon& icon, uint32_t frameMs);

/// The symbol's colour: each byte of the player's colour scaled by the alpha's byte
[[nodiscard]] uint32_t IconSymbolArgb(uint32_t argb, float alpha);
/// A sparkle's colour at `t` along the shape: brightest halfway, black at its ends, the alpha byte kept
[[nodiscard]] uint32_t IconSparkleArgb(float t, float alpha);
/// The point of a shape at `t` (0..1) of its points, straight between two of them: its first point at or below 0, its
/// last at or past 1
[[nodiscard]] glm::vec3 ShapePointAt(const std::vector<glm::vec3>& points, float t);

/// What is drawn of one icon this frame, in pixels
struct GestureIconDraw
{
	size_t icon {0};  ///< its place in the list
	uint8_t page {0}; ///< S_Gesture0 or S_Gesture1
	uint8_t cell {0};
	glm::ivec2 symbolPixel {0};
	uint32_t symbolArgb {0};
	std::array<glm::ivec2, 2> sparklePixels {};
	std::array<uint32_t, 2> sparkleArgb {};
	int sparkles {0};
};
/// The frame's step of the icons and what is drawn of them: nothing when the alpha is 0 after its step, nor for an
/// icon whose shape has no points (its time and its sparkles stand)
[[nodiscard]] std::vector<GestureIconDraw> StepIcons(std::span<GestureIcon> icons, float& alpha, bool helpSystem,
                                                     bool helpTextShown, uint32_t frameMs,
                                                     const std::function<const std::vector<glm::vec3>&(Gesture)>& shape);

/// The interface's icons: the list, their alpha and whether a help text was drawn on the screen last frame
struct GestureIconState
{
	GestureIconList list;
	float alpha {1.0f};
	bool helpTextShown {false};
};
/// The miracles' service's hand magic state's icons (the game's one list)
[[nodiscard]] GestureIconState& IconState();
} // namespace openblack::magic::gestures
