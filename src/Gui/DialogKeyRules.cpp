/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DialogKeyRules.h"

#include <algorithm>

#include <SDL_keycode.h>

namespace openblack::gui::dialog_keys
{

std::optional<size_t> NextFocus(std::span<const bool> canFocus, std::optional<size_t> focus, bool backwards)
{
	const auto count = canFocus.size();
	if (count == 0)
	{
		return focus;
	}
	auto step = [count, backwards](std::optional<size_t> from) -> size_t {
		if (!from.has_value())
		{
			return backwards ? count - 1 : 0;
		}
		if (backwards)
		{
			return *from == 0 ? count - 1 : *from - 1;
		}
		return *from + 1 == count ? 0 : *from + 1;
	};
	auto candidate = focus;
	for (size_t i = 0; i < count; ++i)
	{
		candidate = step(candidate);
		if (candidate == focus)
		{
			break;
		}
		if (canFocus[*candidate])
		{
			return candidate;
		}
	}
	// No other control can take the focus. The game keeps it where it was, or with no focus it never stops looking.
	return focus;
}

float MoveSlider(float value, SliderKey key)
{
	switch (key)
	{
	case SliderKey::Home:
		value = 0.0f;
		break;
	case SliderKey::End:
		value = 1.0f;
		break;
	case SliderKey::Left:
		value -= 0.1f;
		break;
	case SliderKey::Right:
		value += 0.1f;
		break;
	}
	return value > 0.0f ? std::min(value, 1.0f) : 0.0f;
}

std::optional<size_t> MoveList(std::optional<size_t> selection, size_t count, ListKey key)
{
	if (count == 0)
	{
		return std::nullopt;
	}
	switch (key)
	{
	case ListKey::Home:
		return 0;
	case ListKey::End:
		return count - 1;
	case ListKey::Up:
		return selection.has_value() && *selection > 0 ? *selection - 1 : count - 1;
	case ListKey::Down:
		return selection.has_value() && *selection + 1 < count ? *selection + 1 : 0;
	}
	return selection;
}

bool IsReturn(int keyCode)
{
	return keyCode == SDLK_RETURN || keyCode == SDLK_KP_ENTER;
}

namespace
{
/// The letters a to z made capitals, every other character kept
char16_t Upper(char16_t c)
{
	return c >= u'a' && c <= u'z' ? static_cast<char16_t>(c - u'a' + u'A') : c;
}
} // namespace

Answer AnswerOfLetter(char16_t typed, std::u16string_view yes, std::u16string_view no)
{
	const char16_t yesFirst = yes.empty() ? u'\0' : yes.front();
	const char16_t noFirst = no.empty() ? u'\0' : no.front();
	if (yesFirst == noFirst)
	{
		return Answer::None;
	}
	const auto letter = Upper(typed);
	if (letter == 0)
	{
		return Answer::None;
	}
	// the No is tested after the Yes, and wins
	auto answer = Answer::None;
	if (letter == Upper(yesFirst))
	{
		answer = Answer::Yes;
	}
	if (letter == Upper(noFirst))
	{
		answer = Answer::No;
	}
	return answer;
}

} // namespace openblack::gui::dialog_keys
