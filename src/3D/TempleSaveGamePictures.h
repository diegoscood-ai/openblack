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

#include <array>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "3D/TempleInteriorInterface.h"
#include "3D/TempleSaveGameControls.h"
#include "Graphics/GraphicsHandle.h"

namespace openblack
{

/// The save game room's pictures on its walls (temple::save_game_room::PictureOf): a slot with a save shows its picture
/// from the files of the saved games' pictures, the slot looked at closer its large picture, and any other slot the
/// empty picture's half by the player's alignment. The files are read as the player comes into the room, after a save
/// and as the camera is sent to a picture; the textures go as the player leaves the temple.
class TempleSaveGamePictures
{
public:
	/// A file of the saved games' pictures by its name, none when it is not there
	using ReadFile = std::function<std::optional<std::vector<uint8_t>>(const std::string& name)>;

	TempleSaveGamePictures() = default;
	~TempleSaveGamePictures();
	TempleSaveGamePictures(const TempleSaveGamePictures&) = delete;
	TempleSaveGamePictures& operator=(const TempleSaveGamePictures&) = delete;
	TempleSaveGamePictures(TempleSaveGamePictures&&) = delete;
	TempleSaveGamePictures& operator=(TempleSaveGamePictures&&) = delete;

	/// The empty picture, 256 by 256 RGB with its top row first (Data/Textures/PictureTexture.raw); a picture of
	/// another size leaves none
	void SetEmptyPicture(std::span<const uint8_t> rgb);
	/// Coming into the room: the small pictures are read again, and no slot is shown in high detail
	void Enter(const ReadFile& read);
	/// The camera is sent to a slot's picture: its large picture is read and shown in high detail
	void LookAt(uint32_t slot, const ReadFile& read);
	/// After a save into a slot, or the pictures' files written by a save by itself: the small pictures are read again,
	/// and the large one when the slot is shown in high detail (none: whichever slot is)
	void AfterSave(std::optional<uint32_t> slot, const ReadFile& read);
	/// The slot shown in high detail, the picture the camera was last sent to since the player came into the room
	[[nodiscard]] std::optional<uint32_t> GetHighDetailSlot() const { return _highDetail; }

	/// The pictures' textures on the room's picture submeshes
	[[nodiscard]] std::vector<TempleSubMeshTexture> GetTextures(std::span<const TempleSaveGameControls::Picture> pictures,
	                                                            const TempleSaveGameControls& controls, bool inTheRoom,
	                                                            float alignment) const;
	/// Every texture goes
	void Clear();

	/// The files of small pictures the room's 21 slots take
	static constexpr uint32_t k_SmallFiles = 3;

private:
	void ReadSmall(const ReadFile& read);
	void ReadLarge(uint32_t slot, const ReadFile& read);

	std::optional<graphics::TextureHandle> _empty;
	std::array<std::optional<graphics::TextureHandle>, k_SmallFiles> _small;
	/// The file of large pictures of the slot shown in high detail
	std::optional<graphics::TextureHandle> _large;
	std::optional<uint32_t> _highDetail;
};

} // namespace openblack
