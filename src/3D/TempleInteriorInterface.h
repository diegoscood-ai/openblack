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

#include <chrono>
#include <optional>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/TempleLight.h"
#include "3D/TempleMap.h"
#include "3D/TempleScrollPictures.h"
#include "Graphics/GraphicsHandle.h"

namespace openblack
{
namespace graphics
{
class Texture2D;
}
namespace gui
{
class GameInterface;
} // namespace gui
struct OrientedTextVertex;
class TempleDoors;

enum class TempleRoom
{
	Challenge,
	CreatureCave,
	Credits,
	Main,
	Multi,
	Options,
	SaveGame,
	Unknown
};

/// A belt or medal of the creature's room: its icon's mesh, where it is, and its colour
struct TempleCaveTrophy
{
	/// The texture every belt and medal is drawn with: icons.raw with iconsa.raw's alpha, in the texture cache
	static constexpr entt::id_type k_Texture = entt::hashed_string("temple/icons").value();
	uint32_t mesh;
	glm::mat4 model;
	uint32_t colour;
	/// Whether it is drawn with an environment map added
	bool environmentMapped;
};

/// Where the cursor meets the temple's room, and the way the surface there faces
struct TempleCursorHit
{
	glm::vec3 point;
	glm::vec3 normal;
	/// The room whose mesh the cursor is over, and the submesh of it, when it is over one
	TempleRoom room {TempleRoom::Unknown};
	std::optional<uint32_t> subMesh;
};

/// A texture a room's mesh draws one of its submeshes with, in place of its skin
struct TempleSubMeshTexture
{
	uint32_t subMesh;
	graphics::TextureHandle texture;
};

/// A colour added to one of a room's submeshes, from 0 to 1, as a control glows under the cursor
struct TempleSubMeshGlow
{
	uint32_t subMesh;
	glm::vec3 colour;
};

/// What the rooms that open dialogs or show words are doing, for the debug window
struct TempleRoomDialogs
{
	/// The options room has opened the game's options and waits for them to close
	bool optionsOpened {false};
	/// The future room has shown its words in this visit, and their alpha this frame out of 255 while they show
	bool futureWordsShown {false};
	std::optional<float> futureAlpha;
};

class TempleInteriorInterface
{
public:
	virtual ~TempleInteriorInterface() = default;

	[[nodiscard]] virtual bool Active() const = 0;
	[[nodiscard]] virtual glm::vec3 GetPosition() const = 0;
	/// The room the player is in. The temple draws it, and the main room when it is another.
	[[nodiscard]] virtual TempleRoom GetCurrentRoom() const = 0;
	virtual void SetCurrentRoom(TempleRoom room) = 0;
	/// The room the camera is on its way into through a door, drawn as well while it is
	[[nodiscard]] virtual std::optional<TempleRoom> GetTransitionRoom() const = 0;
	virtual void SetTransitionRoom(std::optional<TempleRoom> room) = 0;
	/// Cuts to a room
	virtual void GoToRoom(TempleRoom room) = 0;
	/// Walks through the main room's door to a room, or cuts to it from elsewhere
	virtual void EnterRoom(TempleRoom room) = 0;
	/// The camera looks at a scroll of the current room, by its place among the room's scrolls, as a press on it does
	virtual void LookAtScroll(size_t place) = 0;
	/// Whether a room is drawn whole: the room the player is in, the room the camera is on its way into, and the main
	/// room from the others while one of its doors is open
	[[nodiscard]] virtual bool IsRoomDrawn(TempleRoom room) const = 0;
	/// Whether the player can go into a room. The player cannot go to a room that is not open; its door says so.
	[[nodiscard]] virtual bool IsRoomAvailable(TempleRoom /*room*/) const { return true; }
	/// The player clicked the door of a room that is not open: the help that says so starts
	virtual void TellRoomNotAvailable() {}
	/// Whether the main room has been drawn in this game, which opens the other rooms
	[[nodiscard]] virtual bool RoomsOpened() const { return true; }
	/// The other rooms open as the main room's first drawing opens them (the debug tools' trigger)
	virtual void OpenRooms() {}
	/// The main room's doors, which swing open as the camera walks through them
	[[nodiscard]] virtual TempleDoors& GetDoors() = 0;
	[[nodiscard]] virtual const TempleDoors& GetDoors() const = 0;
	/// Where the cursor last met the room, which the hand is put by in the temple
	[[nodiscard]] virtual std::optional<TempleCursorHit> GetCursorHit() const = 0;
	/// The game's interface, whose text and font the rooms write their scrolls and signs with, whose options the options
	/// room opens and over which the future room shows its words, or null without one
	virtual void SetInterface(gui::GameInterface* interface) = 0;
	/// A press on one of the rooms' controls takes the mouse: on a scroll, dragging the mouse up
	/// and down turns it, and letting go over a button presses it. True while a control has the press, which the camera
	/// then leaves alone.
	virtual bool HoldControl(bool pressed, float mouseY) = 0;
	/// The scrolls' textures for a room's mesh
	[[nodiscard]] virtual std::vector<TempleSubMeshTexture> GetScrollTextures(TempleRoom room) const = 0;
	/// The submeshes of a room's mesh its controls leave undrawn, as each of the main room's buttons draws only one of
	/// its pair
	[[nodiscard]] virtual std::vector<uint32_t> GetHiddenSubMeshes(TempleRoom room) const = 0;
	/// The glow of the control the cursor is over in a room's mesh, if it glows
	[[nodiscard]] virtual std::vector<TempleSubMeshGlow> GetControlGlows(TempleRoom room) const = 0;
	/// The text written in the rooms this frame, the signs' labels and the text of the scroll the camera is close to, in
	/// triangles of the font's texture, in the temple
	[[nodiscard]] virtual const std::vector<OrientedTextVertex>& GetText() const = 0;
	/// The font's texture the text is drawn from, null without the game's interface
	[[nodiscard]] virtual const graphics::Texture2D* GetTextTexture() const = 0;
	/// The island over the main room's pool this frame, in triangles in the room, textured with the land seen from above
	/// as a map. Empty while the main room isn't drawn.
	[[nodiscard]] virtual const std::vector<OrientedTextVertex>& GetMap() const = 0;
	/// Counts the visits to the temple, on each of which the map's texture of the land is drawn afresh
	[[nodiscard]] virtual uint32_t GetVisits() const = 0;
	/// The markers on the map this frame, in the main room, of the kinds its buttons show
	[[nodiscard]] virtual const std::vector<TempleMapMarker>& GetMapMarkers() const = 0;
	/// Where the players' influence circles are drawn over the map this frame, in the world's frame (the temple's
	/// position added); none when they are not
	[[nodiscard]] virtual std::optional<TempleMapInfluence> GetMapInfluence() const { return std::nullopt; }
	/// The task whose dialogue the temple keeps while the player is inside, none when it keeps none
	[[nodiscard]] virtual std::optional<uint32_t> GetKeptDialogueOwner() const { return std::nullopt; }
	/// The lines of the scroll the camera is close to that picture a control, in the temple's own frame, this frame
	[[nodiscard]] virtual std::vector<temple_scroll_pictures::Line> GetScrollPictures() const { return {}; }
	/// What the options room and the future room are doing
	[[nodiscard]] virtual TempleRoomDialogs GetRoomDialogs() const { return {}; }
	/// The temple's light this frame, which its rooms and what is in them are drawn in
	[[nodiscard]] virtual const TempleLight& GetLight() const = 0;
	/// The belts and medals the creature's room shows, while it is drawn
	[[nodiscard]] virtual const std::vector<TempleCaveTrophy>& GetCaveTrophies() const = 0;
	/// How far the markers have turned, in radians
	[[nodiscard]] virtual float GetMapMarkerTurn() const = 0;
	/// How long the main room's pool has shimmered, in seconds, which its two layers' alpha and texture follow
	[[nodiscard]] virtual float GetPoolTime() const = 0;
	/// Escape takes the player back to the main room, and out of the temple from there, as the temple's keys do
	virtual void Escape() = 0;
	/// Leaves the temple once the frame is done with it
	virtual void RequestLeave() = 0;
	/// Fades the screen to white over a second, as the temple does before the player leaves it for the island
	virtual void FadeToWhite() = 0;
	/// Carries out what the frame asked of the temple, and moves on what goes on in its rooms by themselves
	virtual void Update(std::chrono::microseconds dt) = 0;
	/// The temple's turn, every 100 ms while the world is paused inside it, before the help system's: submits the
	/// tooltip the hand shows in the room
	virtual void ProcessTurn() = 0;
	/// How far the creature's room's waterfall has slid down it, the offset of its texture
	[[nodiscard]] virtual glm::vec2 GetWaterfallSlide() const = 0;
	virtual void Activate() = 0;
	virtual void Activate(TempleRoom room) = 0;
	virtual void Deactivate() = 0;
};
} // namespace openblack
