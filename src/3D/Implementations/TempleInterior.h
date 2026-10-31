/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "3D/OrientedText.h"
#include "3D/TempleChallengeControls.h"
#include "3D/TempleChallengePictures.h"
#include "3D/TempleDoors.h"
#include "3D/TempleInteriorInterface.h"
#include "3D/TempleMap.h"
#include "3D/TempleToggles.h"
#include "3D/TempleToolTips.h"
#include "Creature/TattooEditor.h"
#include "Temple/FutureRoomRules.h"
#include "Temple/TempleHelp.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack
{
class CameraModel;
class CreatureCaveEffects;
class TempleCameraModel;
class TempleScrolls;
class TempleSigns;

class TempleInterior final: public TempleInteriorInterface
{
public:
	TempleInterior();
	~TempleInterior() override;

	[[nodiscard]] bool Active() const override { return _active; }
	[[nodiscard]] glm::vec3 GetPosition() const override { return _templePosition; }
	[[nodiscard]] TempleRoom GetCurrentRoom() const override { return _currentRoom; }
	void SetCurrentRoom(TempleRoom room) override;
	[[nodiscard]] std::optional<TempleRoom> GetTransitionRoom() const override { return _transitionRoom; }
	void SetTransitionRoom(std::optional<TempleRoom> room) override { _transitionRoom = room; }
	void GoToRoom(TempleRoom room) override;
	void EnterRoom(TempleRoom room) override;
	void LookAtScroll(size_t place) override;
	[[nodiscard]] bool IsRoomDrawn(TempleRoom room) const override;
	[[nodiscard]] bool IsRoomAvailable(TempleRoom room) const override;
	void TellRoomNotAvailable() override;
	[[nodiscard]] bool RoomsOpened() const override { return _roomsOpened; }
	void OpenRooms() override { _roomsOpened = true; }
	[[nodiscard]] TempleDoors& GetDoors() override { return _doors; }
	[[nodiscard]] const TempleDoors& GetDoors() const override { return _doors; }
	[[nodiscard]] std::optional<TempleCursorHit> GetCursorHit() const override;
	void SetInterface(gui::GameInterface* interface) override;
	bool HoldControl(bool pressed, float mouseY) override;
	[[nodiscard]] std::vector<TempleSubMeshTexture> GetScrollTextures(TempleRoom room) const override;
	[[nodiscard]] std::vector<uint32_t> GetHiddenSubMeshes(TempleRoom room) const override;
	[[nodiscard]] const std::vector<OrientedTextVertex>& GetText() const override { return _text; }
	[[nodiscard]] std::vector<TempleSubMeshGlow> GetControlGlows(TempleRoom room) const override;
	[[nodiscard]] const graphics::Texture2D* GetTextTexture() const override;
	[[nodiscard]] const std::vector<OrientedTextVertex>& GetMap() const override { return _mapTriangles; }
	[[nodiscard]] uint32_t GetVisits() const override { return _visits; }
	[[nodiscard]] const std::vector<TempleMapMarker>& GetMapMarkers() const override { return _mapMarkers; }
	[[nodiscard]] std::optional<TempleMapInfluence> GetMapInfluence() const override { return _mapInfluence; }
	[[nodiscard]] std::optional<uint32_t> GetKeptDialogueOwner() const override
	{
		return _heldDialogue.has_value() ? std::optional<uint32_t>(_heldDialogue->owner) : std::nullopt;
	}
	[[nodiscard]] std::vector<temple_scroll_pictures::Line> GetScrollPictures() const override { return _scrollPictures; }
	[[nodiscard]] TempleRoomDialogs GetRoomDialogs() const override
	{
		return {.optionsOpened = _optionsShown, .futureWordsShown = _futureWords.shown, .futureAlpha = _futureAlpha};
	}
	[[nodiscard]] const std::vector<TempleCaveTrophy>& GetCaveTrophies() const override { return _caveTrophies; }
	[[nodiscard]] const TempleLight& GetLight() const override { return _light; }
	[[nodiscard]] float GetMapMarkerTurn() const override { return _mapMarkerTurn; }
	[[nodiscard]] float GetPoolTime() const override { return _poolTime; }
	void Escape() override;
	void RequestLeave() override { _leaveRequested = true; }
	void FadeToWhite() override;
	void Update(std::chrono::microseconds dt) override;
	void ProcessTurn() override;
	[[nodiscard]] glm::vec2 GetWaterfallSlide() const override;
	void Activate() override { Activate(TempleRoom::Main); }
	void Activate(TempleRoom room) override;
	void Deactivate() override;

private:
	/// Looks through the camera model's lens, or the configured one without
	void ApplyLens() const;

	bool _active {false};
	/// The room the player last came into on this visit, whose help was asked for then
	std::optional<TempleRoom> _enteredRoom;
	bool _leaveRequested {false};
	/// Whether the game was paused when the player went in, which leaving gives back
	bool _pausedOutside {false};
	/// The temple starts in the main room unless told of another
	TempleRoom _currentRoom {TempleRoom::Main};
	std::optional<TempleRoom> _transitionRoom;
	/// Set once the main room has been drawn in this game, which opens the other rooms. The game keeps it in its saves.
	bool _roomsOpened {false};
	/// The game places each room at the origin, unturned: the temple is a scene of its own
	glm::vec3 _templePosition {0.0f};
	glm::vec3 _templeRotation {0.0f};
	glm::vec3 _playerPositionOutside {0.0f};
	glm::vec3 _playerRotationOutside {0.0f};
	/// The camera's model outside, while the temple's has the camera
	std::unique_ptr<CameraModel> _outsideCameraModel;
	TempleCameraModel* _cameraModel {nullptr};
	TempleDoors _doors;
	/// The main room's buttons of what its map shows, which keep their state from one visit to the next
	TempleToggles _toggles;
	/// The challenge room's pictures, frames and buttons, shown for the challenges found so far
	TempleChallengeControls _challengeControls;
	/// The textures of the challenge room's pictures, made while the room is drawn and gone as the player leaves
	TempleChallengePictures _challengePictures;
	/// The challenges found so far given to the challenge room's controls, as the records are this frame
	void UpdateChallengeControls();
	/// A click on a challenge's picture or button
	void ClickChallenge(const TempleChallengeControls::Click& click);
	/// The record of the challenge chosen in the challenge room, whose reminder the player leaves the temple to see
	/// again once the screen is white
	std::optional<entt::entity> _replay;
	/// Leaves the temple and starts the chosen challenge's reminder, the camera put where its picture was taken
	void LeaveToReplayChallenge();
	/// The title and how it went of the challenge whose picture the camera is close to, at the top of the screen
	void UpdateChallengeCaption();
	/// The island over the main room's pool, and its triangles this frame
	TempleMap _map;
	std::vector<OrientedTextVertex> _mapTriangles;
	/// The markers on the map this frame, and how far they have turned
	std::vector<TempleMapMarker> _mapMarkers;
	/// Where the players' influence circles stand over the map this frame, none while they are not shown
	std::optional<TempleMapInfluence> _mapInfluence;
	/// The room's press (TempleToolTipInput::pressHeld), the button last frame, and a change of room since then
	bool _pressHeld {false};
	bool _wasPressed {false};
	bool _pressRoomChanged {false};
	/// The world's dialogue the temple keeps while the player is inside, given back as the player leaves
	std::optional<temple_help::HeldDialogue> _heldDialogue;
	/// The creature's room's belts for how the creature fights and the medals for its miracles, at the points of the
	/// room's mesh, while the room is drawn: none without a creature
	std::vector<TempleCaveTrophy> _caveTrophies;
	/// Whether the belts' and medals' meshes and their texture have been asked of the caches, the first time they are
	/// shown
	bool _caveIconsLoaded {false};
	/// Chooses the belts and medals of the player's creature
	void UpdateCaveTrophies();
	/// The creature's room's flames, smoke, spray and mist, set up the first time the room is drawn in a visit
	std::unique_ptr<CreatureCaveEffects> _creatureCaveEffects;
	/// Whether the creature's room's fire and water are playing, which stop once the room is no longer drawn
	bool _creatureCaveSounds {false};
	/// Whether the creature's room has started in this visit to the temple: it does the first time it is drawn
	bool _creatureCaveStarted {false};
	/// Whether the creature's room has been drawn, which sets its own fade from then on, for the whole game
	bool _creatureCaveFadeSet {false};
	/// What the creature's room keeps of its copy of the creature from frame to frame, for the whole game
	creature_tattoo_editor::CopyLife _copyLife;
	/// Each frame: the room's sounds, and its effects set up and moved on, while the room is drawn
	void UpdateCreatureCave();
	void StopCreatureCaveSounds();
	TempleLight _light;
	float _mapMarkerTurn {0.0f};
	/// How long the main room's pool has shimmered, in seconds
	float _poolTime {0.0f};
	/// The main room's markers of the temples and creatures
	void UpdateMapMarkers(float seconds);
	/// Where the camera is to look from and at as the player leaves for a place double clicked on the map
	std::optional<std::pair<glm::vec3, glm::vec3>> _leaveTo;
	/// Whether the temple is fading to white to leave for the place on the map, which it does once the screen is white
	bool _leavingForMapPoint {false};
	/// A double click on the main room's map: leaves the temple for that place
	void LeaveForMapPoint(glm::vec3 point);
	/// Covers the screen as the player is put straight into a room, and fades it back over a second and a fifth
	void FadeIntoRoom();
	uint32_t _visits {0};
	/// How far through its slide the waterfall's texture is, from 0 to 1, kept for the whole game
	float _waterfallSlide {0.0f};
	/// The game's interface, whose text and font the scrolls and signs are written with
	gui::GameInterface* _interface {nullptr};
	/// The rooms' scrolls, written as the temple opens, and the labels of their signs
	std::unique_ptr<TempleScrolls> _scrolls;
	std::unique_ptr<TempleSigns> _signs;
	/// As each room is set up: makes and writes the scrolls, and finds the signs
	void CreateScrolls();
	/// This frame's text in the rooms
	std::vector<OrientedTextVertex> _text;
	/// and the lines of the scroll looked at that picture a control
	std::vector<temple_scroll_pictures::Line> _scrollPictures;
	/// Whether the options room has opened the game's options
	bool _optionsShown {false};
	/// The future room's words in this visit to the temple, which fade in
	temple::future_room::Words _futureWords;
	/// Their alpha this frame, out of 255, while they show
	std::optional<float> _futureAlpha;
	/// The submesh the cursor is over, and how long is left of the glow it set off, in milliseconds. Moving onto
	/// another submesh sets the glow off again, unless a control is being dragged.
	std::optional<std::pair<TempleRoom, uint32_t>> _hovered;
	float _hoverGlow {0.0f};
	/// What the hand shows, which the rooms choose every frame and submit every temple turn
	TempleToolTip _toolTip {k_FirstTempleToolTip};
	/// Moves on the options room and the future room, and what the future room draws
	void UpdateOptionsAndFutureRooms();
	/// The tooltip the hand shows, and where on the screen the hand is, from this frame's pick under the cursor
	void UpdateToolTips(const std::optional<TempleCursorHit>& hit);
	/// Whether a submesh of a room's mesh is one of its controls drawn, and whether a control has the mouse
	[[nodiscard]] bool IsControl(TempleRoom room, uint32_t subMesh) const;
	[[nodiscard]] bool IsControlHeld() const;
};
} // namespace openblack
