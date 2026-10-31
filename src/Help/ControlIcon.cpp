/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ControlIcon.h"

#include "ClickCue.h"

namespace openblack::help::control_icon
{

int KeyIconRow(int top, int bottom)
{
	return top + click_cue::CueSize(top, bottom) / 2;
}

void AppendKeyName(std::u16string& text, int& count, std::u16string_view name)
{
	++count;
	if (count == 2)
	{
		text += u" + ";
	}
	else if (count > 2)
	{
		text += u" / ";
	}
	text += name;
}

} // namespace openblack::help::control_icon
