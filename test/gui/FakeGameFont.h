/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A made up script and font in the game's formats, and bgfx's renderer of nothing that a font's texture needs, for the
// interface's tests

#pragma once

#include <cstdint>
#include <cstring>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Gui/GameFont.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Resources/ResourcesInterface.h"
#include "support/BgfxShutdown.h"

namespace openblack::gui::test
{
inline std::vector<uint8_t> Utf16Script(std::u16string_view text, bool byteOrderMark = true)
{
	std::vector<uint8_t> bytes;
	if (byteOrderMark)
	{
		bytes.insert(bytes.end(), {0xFF, 0xFE});
	}
	for (const auto c : text)
	{
		bytes.push_back(static_cast<uint8_t>(c & 0xFF));
		bytes.push_back(static_cast<uint8_t>(c >> 8));
	}
	return bytes;
}

/// The font loader reports a bad font in the graphics log
inline void MakeGraphicsLog()
{
	if (spdlog::get("graphics") == nullptr)
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("graphics");
	}
}

/// A font makes its texture, here with bgfx's renderer of nothing. Made before the font, so the font's texture goes
/// first.
struct NoopRenderer
{
	NoopRenderer()
	{
		MakeGraphicsLog();
		bgfx::renderFrame(); // single threaded
		bgfx::Init init {};
		init.type = bgfx::RendererType::Noop;
		if (bgfx::init(init))
		{
			shutdown.emplace();
		}
		EXPECT_TRUE(shutdown.has_value());
	}

	std::optional<openblack::test::BgfxShutdown> shutdown;
};

struct FakeGlyph
{
	char16_t character;
	uint16_t width;
	float left;
	float ink;
	float right;
	/// Rows of '#' and '.', each repeated to make the font's height
	std::vector<std::string> rows;
};

/// A font in the format of data/j0.met and data/j0.fnt
struct FakeFont
{
	uint32_t height;
	std::vector<FakeGlyph> glyphs;
	std::vector<uint8_t> met;
	std::vector<uint8_t> fnt;

	template <typename T>
	static void Append(std::vector<uint8_t>& data, T value)
	{
		const auto offset = data.size();
		data.resize(offset + sizeof(T));
		std::memcpy(data.data() + offset, &value, sizeof(T));
	}

	void Build()
	{
		met.clear();
		fnt.clear();
		Append(met, height);
		std::vector<uint8_t> name(0x100, 0);
		const std::u16string_view fontName = u"Fake Sans";
		for (size_t i = 0; i < fontName.size(); ++i)
		{
			name[i * 2] = static_cast<uint8_t>(fontName[i]);
		}
		met.insert(met.end(), name.begin(), name.end());
		Append(met, static_cast<uint32_t>(glyphs.size()));
		for (const auto& glyph : glyphs)
		{
			// Runs of clear and set pixels in turn, starting with clear
			const auto offset = static_cast<uint32_t>(fnt.size());
			bool set = false;
			uint32_t run = 0;
			auto flush = [this, &run]() {
				if (run >= 0xFF)
				{
					fnt.push_back(0xFF);
					Append(fnt, static_cast<uint16_t>(run));
				}
				else
				{
					fnt.push_back(static_cast<uint8_t>(run));
				}
				run = 0;
			};
			const auto repeat = height / static_cast<uint32_t>(glyph.rows.size());
			for (const auto& row : glyph.rows)
			{
				for (uint32_t copy = 0; copy < repeat; ++copy)
				{
					for (const auto pixel : row)
					{
						if ((pixel == '#') != set)
						{
							flush();
							set = !set;
						}
						++run;
					}
				}
			}
			flush();

			Append(met, static_cast<uint16_t>(glyph.character));
			Append(met, glyph.width);
			Append(met, static_cast<uint32_t>(0xCCCC0000));
			Append(met, glyph.left);
			Append(met, glyph.ink);
			Append(met, glyph.right);
			Append(met, offset);
			Append(met, static_cast<uint32_t>(fnt.size() - offset));
		}
	}
};

/// 80 pixels high, as the game's fonts must be, each row of the glyphs drawn 20 times: a solid A, a checkered B, a
/// space, a question mark, a hyphen and a wide W. Their metrics are 20 times those of a 4 pixel font, so that a size
/// measures the same.
inline FakeFont MakeFont()
{
	FakeFont font {.height = 80, .glyphs = {}, .met = {}, .fnt = {}};
	font.glyphs = {
	    {.character = u'A', .width = 2, .left = 20.0f, .ink = 40.0f, .right = 20.0f, .rows = {"##", "##", "##", "##"}},
	    {.character = u'B', .width = 2, .left = 0.0f, .ink = 40.0f, .right = 0.0f, .rows = {"#.", ".#", "#.", ".#"}},
	    {.character = u' ', .width = 0, .left = 20.0f, .ink = 0.0f, .right = 20.0f, .rows = {"", "", "", ""}},
	    {.character = u'?', .width = 2, .left = 0.0f, .ink = 40.0f, .right = 40.0f, .rows = {"##", "..", "#.", ".."}},
	    {.character = u'-', .width = 2, .left = 0.0f, .ink = 40.0f, .right = 0.0f, .rows = {"..", "##", "..", ".."}},
	    {.character = u'W', .width = 4, .left = 0.0f, .ink = 160.0f, .right = 0.0f, .rows = {"####", "####", "####", "####"}},
	};
	font.Build();
	return font;
}

/// Through the font cache's loader, as the game reads its fonts
inline GameFont LoadFont()
{
	MakeGraphicsLog();
	auto fake = MakeFont();
	auto font =
	    GameFont::From(resources::GameFontLoader {}(resources::GameFontLoader::FromBufferTag {}, fake.met, fake.fnt, "fake"));
	EXPECT_TRUE(font.has_value());
	return std::move(font.value());
}

} // namespace openblack::gui::test
