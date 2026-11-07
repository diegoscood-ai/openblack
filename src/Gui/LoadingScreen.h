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
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Graphics/Texture2D.h"
#include "Gui/Canvas.h"
#include "Gui/DialogPainter.h"
#include "Gui/GameFont.h"
#include "Gui/LoadingScreenRules.h"
#include "Gui/TextDatabase.h"
#include "Video/StillPicture.h"

namespace openblack::gui
{

/// The screens the game draws on its own, outside its frames, as it starts and while a land loads: the logo pictures,
/// the pre-intro film's frames and the tips screen with its tip of the day, picture, version number and loading bar,
/// or the "please wait" banner of a script's map. Each call draws one frame of the interface view; the caller presents
/// it.
class LoadingScreen
{
public:
	/// Reads the game's texts, the front end's font and button atlas through the resource caches. Null without them
	/// The key the constructor takes: only LoadingScreen makes one, so only Create makes a LoadingScreen, through
	/// std::make_unique
	class Key
	{
		friend class LoadingScreen;
		Key() = default;
	};

	static std::unique_ptr<LoadingScreen> Create();
	LoadingScreen(Key key, TextDatabase texts, GameFont font, std::shared_ptr<graphics::Texture2D> atlas,
	              glm::vec3 bevelColour);
	~LoadingScreen();
	LoadingScreen(const LoadingScreen&) = delete;
	LoadingScreen& operator=(const LoadingScreen&) = delete;

	/// Picks the tip for a time in milliseconds and makes its picture and text, unless its picture is still there from
	/// the last time
	void PrepareTip(uint32_t milliseconds, bool hasProfiles);
	/// A tip has been prepared at least once: until then the screen draws nothing while loading
	[[nodiscard]] bool TipShowing() const noexcept { return _tipShowing; }
	/// The tip's picture is ready
	[[nodiscard]] bool HasTipPicture() const noexcept { return _picture.has_value(); }
	/// The tip's picture goes once the game starts or a film plays; the next load picks another tip
	void ClearTip() noexcept { _picture.reset(); }
	[[nodiscard]] int32_t GetTip() const noexcept { return _tip; }
	[[nodiscard]] const std::u16string& GetTipText() const noexcept { return _tipText; }

	/// The tips screen at a fade from 0 to 1, its bar along by `progress` bar periods. Nothing at no fade
	void DrawTips(glm::u16vec2 resolution, float fade, float progress);
	/// The "please wait" banner a script's map shows after loading for five seconds, over black
	void DrawPleaseWait(glm::u16vec2 resolution, uint8_t alpha);
	/// A film's picture over a rectangle of the screen at an alpha, 0 to 255, drawn as the full-screen player draws its
	/// textures. `rgba` is the picture as it is sampled, `size` texels; it is uploaded again when it or its `serial`
	/// changes
	void DrawPicture(glm::u16vec2 resolution, std::span<const uint8_t> rgba, glm::u16vec2 size, uint32_t serial,
	                 video::ScreenRect rect, uint8_t alpha);

	/// The time spent loading, which moves the bar on
	[[nodiscard]] loading::LoadingClock& GetClock() noexcept { return _clock; }

private:
	struct TipPicture
	{
		std::optional<video::StillPicture> still;
		std::unique_ptr<graphics::Texture2D> sharp;
		/// The blurred picture, in the top left of a 256 by 256 texture
		std::unique_ptr<graphics::Texture2D> blur;
		glm::uvec2 blurSize {0, 0};
	};

	/// The tip's picture from tips.bik and its blurred copy
	void MakePicture();
	/// A picture's tiles over a rectangle of the screen, into the canvas
	void DrawTiles(const graphics::Texture2D& texture, glm::u16vec2 size, video::ScreenRect rect, uint8_t alpha);
	/// A box with a colour at each corner, top left, top right, bottom right, bottom left, as 0xAARRGGBB, each alpha
	/// taken down by the screen's
	void DrawColourBox(loading::Rect rect, const std::array<uint32_t, 4>& colours, uint8_t drawAlpha);
	/// The text broken into lines `size` high centred in a box, from `top`; returns the height it takes
	float DrawText(std::u16string_view text, const loading::Rect& box, float top, float size, glm::vec4 colour, bool draw);
	/// Text centred both ways in a box of the 800 by 600 layout
	void DrawLayoutText(std::u16string_view text, loading::Rect layout, glm::vec4 colour);
	/// A rectangle of the 800 by 600 layout on the screen
	[[nodiscard]] loading::Rect ToScreen(const loading::Rect& layout) const noexcept;
	void DrawLayoutLine(glm::ivec2 from, glm::ivec2 to, glm::vec4 colour);
	void DrawBar(const loading::Layout& layout, float progress, uint8_t alpha);

	TextDatabase _texts;
	GameFont _font;
	std::shared_ptr<graphics::Texture2D> _atlas;
	/// The bar's body: the colour of one texel of the button atlas
	glm::vec3 _bevelColour;
	Canvas _canvas;
	DialogPainter _painter;
	glm::u16vec2 _resolution {0, 0};
	int32_t _tip {0};
	std::u16string _tipText;
	std::optional<TipPicture> _picture;
	bool _tipShowing {false};
	std::u16string _version;
	loading::LoadingClock _clock;
	/// The film's picture last drawn, and what it was made from
	std::unique_ptr<graphics::Texture2D> _film;
	glm::u16vec2 _filmSize {0, 0};
	const uint8_t* _filmSource {nullptr};
	std::optional<uint32_t> _filmSerial;
};

} // namespace openblack::gui
