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

#include <array>
#include <memory>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "Graphics/RenderPass.h"

namespace openblack::graphics
{
class Texture2D;
}

namespace openblack::gui
{

/// Draws the game's interface over the scene: textured, tinted rectangles in screen pixels, batched into as few draw
/// calls as the textures allow and submitted to a view, RenderPass::Interface unless another is given.
class Canvas
{
public:
	/// Whether the canvas sets its view up (its rectangle, its pixel projection, the sequential order) or submits into a
	/// view another draw has set up with no transform, in its order there, its vertices given in clip space
	enum class ViewUse : uint8_t
	{
		Own,
		Shared,
	};
	/// How what is drawn goes over what is under it
	enum class Blend : uint8_t
	{
		Alpha,
		/// Added, by its alpha, as the game's additive weather material is
		Additive,
	};

	explicit Canvas(graphics::RenderPass view = graphics::RenderPass::Interface, ViewUse use = ViewUse::Own);
	~Canvas();
	Canvas(const Canvas&) = delete;
	Canvas& operator=(const Canvas&) = delete;

	/// Starts a frame of the interface for a window of a size, blending by alpha
	void Begin(glm::u16vec2 resolution);
	/// How what is drawn from now on blends
	void SetBlend(Blend blend) noexcept { _blend = blend; }
	/// A rectangle of a texture, tinted by colour (red, green, blue and alpha from 0 to 1). Without a texture it is
	/// filled with the colour. Corners whose minimum is beyond the maximum flip the texture.
	void DrawQuad(glm::vec2 min, glm::vec2 max, glm::vec2 uvMin, glm::vec2 uvMax, glm::vec4 colour,
	              const graphics::Texture2D* texture, bool coverage = false);
	/// A filled four sided shape, its corners in order round it, each with its own colour
	void DrawShape(const std::array<glm::vec2, 4>& corners, const std::array<glm::vec4, 4>& colours);
	/// A four sided shape of a texture, its corners in order round it, each with its own place on the texture and colour;
	/// `coverage` as DrawQuad's, for a glyph of the font
	void DrawShape(const std::array<glm::vec2, 4>& corners, const std::array<glm::vec2, 4>& uvs,
	               const std::array<glm::vec4, 4>& colours, const graphics::Texture2D* texture, bool coverage = false);
	/// A one pixel wide line between the centres of two pixels, along a row or a column
	void DrawLine(glm::ivec2 from, glm::ivec2 to, glm::vec4 colour);
	/// DrawQuad, DrawShape and DrawLine of what the original draws with its own box, quad and line draws, whose corners
	/// go half a pixel on (screen_point::BoxDrawCorner)
	void DrawBoxQuad(glm::vec2 min, glm::vec2 max, glm::vec2 uvMin, glm::vec2 uvMax, glm::vec4 colour,
	                 const graphics::Texture2D* texture);
	void DrawBoxShape(const std::array<glm::vec2, 4>& corners, const std::array<glm::vec4, 4>& colours);
	void DrawBoxLine(glm::ivec2 from, glm::ivec2 to, glm::vec4 colour);
	/// Whether the 2D draws take the original's half pixels (the View menu's "2D half pixel" switch, on unless the
	/// config turns it off)
	[[nodiscard]] static bool HalfPixel() noexcept;
	/// Submits the frame's interface
	void End();

	[[nodiscard]] glm::u16vec2 GetResolution() const noexcept { return _resolution; }

private:
	struct Vertex
	{
		float x;
		float y;
		float u;
		float v;
		uint32_t abgr;
	};
	struct Batch
	{
		const graphics::Texture2D* texture;
		Blend blend;
		bool coverage; ///< an R8 coverage texture: graphics::GameFont's atlas
		uint32_t firstVertex;
		uint32_t vertexCount;
	};

	graphics::RenderPass _view;
	ViewUse _use;
	glm::u16vec2 _resolution {0, 0};
	Blend _blend {Blend::Alpha};
	std::vector<Vertex> _vertices;
	std::vector<Batch> _batches;
	std::unique_ptr<graphics::Texture2D> _white;
};

} // namespace openblack::gui
