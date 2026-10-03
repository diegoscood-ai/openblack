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

#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace openblack::graphics
{
class Texture2D;

/// A font of the original (GatheringText, Data\j0 / f1 / f3: .met metrics + .fnt glyphs), rasterised like the
/// original's glyph cache (CachePage::RenderChar 0x830C10) and laid out like DrawTextRaw 0x832C60.
class GameFont
{
public:
	struct Vertex
	{
		float x, y, u, v; ///< pixels from the top left of the screen, atlas uv
		uint32_t abgr;
	};

	GameFont();
	~GameFont();

	/// Reads <base>.met and <base>.fnt; false if they are missing or bad
	bool Load(const std::filesystem::path& base);
	[[nodiscard]] bool IsLoaded() const { return _texture != nullptr; }
	[[nodiscard]] const Texture2D& GetTexture() const { return *_texture; }

	/// GetStringWidth 0x831130: sum of (left + width + right) x size / 80
	[[nodiscard]] float GetStringWidth(const std::u16string& text, float size) const;
	/// DrawTextRaw: two triangles per glyph at (x, y) (top left) of `size` pixels; rgba 0..1. clipTop / clipBottom: its
	/// height clip (0x832DAB..0x832E20; HelpText passes the box's top and bottom, 0x5CBE04..0x5CBE7F)
	void AddText(std::vector<Vertex>& out, const std::u16string& text, float x, float y, float size, const glm::vec4& rgba,
	             float clipTop = -std::numeric_limits<float>::max(), float clipBottom = std::numeric_limits<float>::max()) const;

private:
	struct Glyph
	{
		uint16_t bitmapWidth;
		float left, width, right;
		uint32_t slotX; ///< texel column of the glyph's slot in the atlas
		uint32_t line;  ///< 64-texel line of the atlas
	};
	[[nodiscard]] const Glyph* Find(char16_t code) const;

	static constexpr uint32_t k_CellHeight = 80;
	std::unordered_map<char16_t, Glyph> _glyphs;
	float _spaceAdvance {15.0f};
	glm::uvec2 _atlasSize {0, 0};
	std::unique_ptr<Texture2D> _texture;
};

} // namespace openblack::graphics
