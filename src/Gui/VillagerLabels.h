/*******************************************************************************
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
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The names over named villagers' heads that the Show Villager Names key brings up.
///
/// The names display grows in and out by the game's time while the land is drawn. A named villager shows its name at
/// the display's size: a dark, see-through rounded box standing on a pointer down to a point over its head, its words
/// half way to white from the colour of the player who owns its town. Names that would cover one another are stacked
/// upwards; one pushed off the top of the screen is not shown. The sizes are the game's own, in the screen's pixels.
/// Pure: values only. The villagers' side is ECS/VillagerLabelSource.h, the draw Graphics/Renderer.
namespace openblack::gui::villager_labels
{

/// The name's point is this far above the villager's drawn position
constexpr float k_HeadHeight = 1.75f;
/// The names display grows in and out by this much of its full size a millisecond of game time
constexpr float k_NameFadePerMillisecond = 0.003f;
/// A name shows only while the display is bigger than this, and a label smaller than this is not made
constexpr float k_SmallestSize = 0.01f;

/// At full size: a line of words is this tall, the box is this much wider than its widest line, its words start this
/// far in, it is this much taller than its lines, and its bottom stands this far above the point
constexpr float k_LineHeight = 16.0f;
constexpr float k_ExtraWidth = 6.0f;
constexpr float k_Inset = 2.0f;
constexpr float k_ExtraHeight = 4.0f;
constexpr float k_RaisedBy = 50.0f;
/// A label holds this many lines at most, and this many characters
constexpr size_t k_MaxLines = 8;
constexpr size_t k_MaxCharacters = 255;
/// The alpha of the box and of the pointer at full size, out of 255, and the alpha of the words
constexpr float k_BoxAlpha = 120.0f;
constexpr float k_WordsAlpha = 255.0f;
/// The pointer: a stalk this wide either side from the box down to this far above the point, then a head this wide
/// either side down to it. These keep their size whatever the label's size
constexpr float k_StalkHalfWidth = 3.0f;
constexpr float k_HeadHalfWidth = 6.0f;
constexpr float k_HeadHeightPixels = 16.0f;

/// The names display's size after a frame of so many milliseconds of game time: towards 1 while shown, towards 0
/// while not, by a step of the milliseconds times k_NameFadePerMillisecond
[[nodiscard]] float StepNameFade(float fade, bool shown, uint32_t milliseconds);

/// The names display in one frame
struct NameFadeFrame
{
	/// The size that the names of the frame use: the size before the step of the frame
	float drawn {0.0f};
	/// The size after the step of the frame
	float next {0.0f};
};
/// The names display's frame. The names are placed while the land is drawn, then the display steps, so a frame's
/// names have the size from before its step. It steps only on a frame that draws the land; a paused frame has no game
/// time, so it stands still too.
[[nodiscard]] NameFadeFrame StepNameFadeFrame(float fade, bool shown, uint32_t milliseconds, bool landDrawn);

/// What decides whether a villager shows its name
struct Who
{
	/// It is a named villager
	bool named {false};
	/// A script holds it
	bool inScript {false};
	/// A script made it: its name is hidden for good
	bool nameHidden {false};
	/// It speaks in its own speech bubble, which shows instead
	bool speaking {false};
	/// The names display's size
	float nameFade {0.0f};
};
/// The size its name shows at, none when it shows none
[[nodiscard]] std::optional<float> NameSize(const Who& who);

/// The colour of a name: white, or the colour (RGB) of the player who owns the villager's town. A colour all but
/// black is taken as white, and every colour is then put half way to white
[[nodiscard]] glm::u8vec3 NameColour(std::optional<glm::u8vec3> player);

/// The width of a line of words at a line height, as the game measures it
using WidthFn = std::function<float(std::u16string_view text, float lineHeight)>;

/// A name wanted this frame
struct Request
{
	/// The whole pixel the point over the head falls in on the screen, and its depth in front of the camera
	glm::ivec2 pixel {0};
	float depth {0.0f};
	/// The point in the world, which the label is sorted by among the see-through things
	glm::vec3 point {0.0f};
	float size {1.0f};
	std::u16string text;
	glm::u8vec3 colour {255, 255, 255};
};
/// A label placed on the screen
struct Label
{
	float left {0.0f};
	float top {0.0f};
	float right {0.0f};
	float bottom {0.0f};
	glm::vec2 pixel {0.0f};
	float depth {0.0f};
	glm::vec3 point {0.0f};
	/// Its size, 0 to 1
	float size {0.0f};
	float lineHeight {0.0f};
	float inset {0.0f};
	std::vector<std::u16string> lines;
	glm::u8vec3 colour {255, 255, 255};
	/// The alpha of the box and of the pointer, and of the words, 0 to 255
	uint8_t boxAlpha {0};
	uint8_t wordsAlpha {0};
};
/// The labels of a frame, placed as they come: each goes over its point, kept on the screen across, and moved up over
/// any placed label it would cover until it covers none; one pushed off the top of the screen is dropped
class Board
{
public:
	/// Places a label on a screen so many pixels wide; false when it isn't shown
	bool Add(const Request& request, uint16_t screenWidth, const WidthFn& width);
	/// The labels placed, from left to right: the order they are queued among the see-through things
	[[nodiscard]] const std::vector<Label>& GetLabels() const noexcept { return _labels; }

private:
	std::vector<Label> _labels;
};

/// A rectangle of the pointer in whole pixels, and its place on the bubble texture
struct PointerBox
{
	int32_t x0 {0};
	int32_t y0 {0};
	int32_t x1 {0};
	int32_t y1 {0};
	glm::vec2 uv0 {0.0f};
	glm::vec2 uv1 {0.0f};
};
/// The pointer from the box's bottom to the point: the stalk, then its head, each corner cut towards zero to a pixel
[[nodiscard]] std::array<PointerBox, 2> PointerShape(const Label& label);

} // namespace openblack::gui::villager_labels
