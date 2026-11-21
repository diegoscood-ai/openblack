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

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Graphics/ScreenshotCapture.h"

namespace openblack::ecs::systems
{

/// The picture of a screen a saved game keeps, as the game samples its screen: the large picture, 256 by 128 texels,
/// rows from the top, each texel the mean of its screen pixel and the one before it in its row (empty when the screen
/// has no pixels). It uses no game data, so the renderer can call it on its own thread
[[nodiscard]] std::vector<uint8_t> SaveScreenPicture(const graphics::ScreenView& screen);

/// The pictures of the screen the saved games keep (docs/wiki/engine/saved-games.md#the-pictures). The game takes a
/// picture of the screen as the player goes into the temple from the land, and as it saves into a slot by itself; the
/// picture goes into the temporary picture file of the current profile, and a save by itself puts it into its slot's
/// cells of the shared picture files. The renderer gives a picture back a frame or more after it is asked, so a save
/// by itself writes its slot's cells again once the picture is back. With no profile kept (a run of the checks) it asks
/// for nothing and writes nothing
class SavePictureSystemInterface
{
public:
	virtual ~SavePictureSystemInterface() = default;

	/// Asks for a picture of the frame on the screen, for the temporary picture file. With a slot, the slot's cells take
	/// it once it is back, as a save into the slot by itself puts the picture just taken into them
	virtual void TakePicture(std::optional<uint32_t> slot) = 0;
	/// Whether the frames drawn are to be kept aside, so that a picture is of the frame on the screen: while a profile is
	/// kept
	[[nodiscard]] virtual bool KeepsShownFrames() const = 0;
	/// The names of the pictures to ask of the next frame drawn (those asked while the frame on the screen was not kept),
	/// given once
	[[nodiscard]] virtual std::vector<std::string> TakeRequests() = 0;

	/// What the renderer has done with the pictures asked for
	struct Screens
	{
		/// The picture made for a name, when it is back
		std::function<std::optional<std::vector<uint8_t>>(const std::string& name)> take;
		/// Whether the picture asked for under a name was given up
		std::function<bool(const std::string& name)> failed;
	};
	/// Writes the pictures that are back, in the order they were asked for, and the cells of the slots waiting for them
	virtual void Collect(const Screens& screens) = 0;

	/// A file of the profile's saved games' pictures, by its name; none with no profile kept or no such file
	[[nodiscard]] virtual std::optional<std::vector<uint8_t>> ReadPictureFile(std::string_view name) const = 0;
	/// How many times the pictures' files were written since the start: the save game room reads them again when it
	/// changes
	[[nodiscard]] virtual uint32_t Changes() const = 0;
	/// (debug) the pictures asked for and not yet back
	[[nodiscard]] virtual size_t Waiting() const = 0;
};

} // namespace openblack::ecs::systems
