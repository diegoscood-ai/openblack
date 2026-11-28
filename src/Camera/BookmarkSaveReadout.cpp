/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BookmarkSaveReadout.h"

#include <fmt/format.h>

#include "3D/MapCoords.h"

using namespace openblack;

std::string bookmark_save_readout::Describe(const ecs::systems::SavedBookmarks& saved)
{
	std::string set;
	for (size_t index = 0; index < saved.bookmarks.size(); ++index)
	{
		const auto& bookmark = saved.bookmarks.at(index);
		if ((bookmark.flags & ecs::systems::k_BookmarkSet) == 0)
		{
			continue;
		}
		set +=
		    fmt::format("{}{} at ({:.1f}, {:.1f}){}", set.empty() ? "" : ", ", index, map_coords::ToMetres(bookmark.position.x),
		                map_coords::ToMetres(bookmark.position.z), bookmark.target.has_value() ? " on a thing" : "");
	}
	return fmt::format("{}; followed {}, current {}, {}", set.empty() ? "none set" : set,
	                   saved.followed.has_value() ? fmt::format("{}", *saved.followed) : std::string("none"), saved.current,
	                   saved.enabled ? "on" : "off");
}

std::string bookmark_save_readout::DescribeCameraHelp(int32_t features, float autoPitchAngle, float autoPitchHeight)
{
	return fmt::format("features 0x{:03X}, auto-pitch angle {:.4f}, auto-pitch height {:.2f}", features, autoPitchAngle,
	                   autoPitchHeight);
}
