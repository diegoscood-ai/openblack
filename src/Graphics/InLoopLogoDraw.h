/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Graphics/WorldTriangles.h"
#include "Gui/InLoopLogo.h"

namespace openblack::gui
{
class Canvas;
class GameFont;
} // namespace openblack::gui

namespace openblack::graphics
{
class FrameBuffer;
class GameFont;
class ShaderManager;
class Texture2D;

/// Draws the in-loop logo (in_loop_logo): the sky (data\D_SKY.l3d) and the stars in the logo's own camera, then its
/// name, the trademark sign and the black fade, into a picture of its own that is never cleared, only its depth: each
/// frame goes over the last one, as the original clears only the depth while it plays. The picture is then copied to
/// the window. Wiki: docs/wiki/engine/game-clock.md#the-in-loop-logo.
class InLoopLogoDraw
{
public:
	/// The camera and what one frame shows
	struct Frame
	{
		glm::u16vec2 resolution {0, 0};
		glm::vec3 eye {0.0f};
		glm::vec3 focus {0.0f};
		/// The game camera's field of view across, in radians, and its far clip
		float horizontalFov {1.0f};
		float farClip {10000.0f};
		uint32_t skyArgb {0};
		std::span<const in_loop_logo::Star> stars;
		std::u16string_view name;
		/// 0 to 1
		float nameFade {0.0f};
		bool trademark {false};
		uint8_t trademarkAlpha {0};
		uint8_t fade {0};
	};

	/// Reads the sky, the stars' texture and the font; none without the stars' texture and the font
	static std::unique_ptr<InLoopLogoDraw> Create();
	~InLoopLogoDraw();
	InLoopLogoDraw(const InLoopLogoDraw&) = delete;
	InLoopLogoDraw& operator=(const InLoopLogoDraw&) = delete;

	/// One frame into the logo's picture and the picture onto the window; the caller presents it
	void Draw(const Frame& frame);
	/// A black frame, as the game draws while a button is still held after the logo
	static void DrawBlack(glm::u16vec2 resolution);
	/// The views the logo took go back to the window
	static void ReleaseViews();

	/// The width of the name at a size, in the font the name is drawn with
	[[nodiscard]] float NameWidth(std::u16string_view name, float size) const;

	/// The sky's skin: its 16-bit texels taken as 5 bits each of red, green and blue (bit 15 unused), each widened to 8
	/// bits, opaque. RGBA, 4 bytes a texel. Pure
	[[nodiscard]] static std::vector<uint8_t> SkyTexels(std::span<const uint16_t> texels);

private:
	struct Sky
	{
		std::vector<world_triangles::Vertex> vertices;
		std::vector<uint16_t> indices;
		std::unique_ptr<Texture2D> skin;
		bool twoSided {false};
		bool tiling {false};
	};
	InLoopLogoDraw();
	void MakeTarget(glm::u16vec2 resolution);

	std::optional<Sky> _sky;
	std::unique_ptr<Texture2D> _white;
	std::unique_ptr<gui::GameFont> _font;
	std::unique_ptr<gui::Canvas> _overlay;
	std::unique_ptr<gui::Canvas> _copy;
	std::unique_ptr<FrameBuffer> _target;
	glm::u16vec2 _targetSize {0, 0};
	bool _cleared {false};
};

} // namespace openblack::graphics
