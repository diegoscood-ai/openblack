/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ClipSounds.h"

#include <sstream>
#include <utility>

using namespace openblack::audio::clip_sounds;

ClipSoundTable openblack::audio::clip_sounds::Parse(std::span<const uint8_t> bytes)
{
	ClipSoundTable table;
	std::istringstream in(std::string(bytes.begin(), bytes.end()));
	int hasGroup = 0;
	in >> hasGroup;
	std::string name;
	while (in >> name && name != "END")
	{
		Clip clip {.name = name};
		if (hasGroup != 0)
		{
			in >> clip.group;
		}
		int more = 1;
		while (more != 0)
		{
			Event event {};
			if (!(in >> event.time >> event.soundId >> event.action >> more))
			{
				break;
			}
			clip.events.push_back(event);
		}
		table.clips.push_back(std::move(clip));
	}
	return table;
}
