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
#include <memory>
#include <optional>
#include <string_view>

#include <glm/vec3.hpp>

#include "CameraModel.h"

/// Where the camera goes while the hand is held to a creature: in front of it, looking at its middle from one and a half
/// of its heights away and a little above, gliding there over 0.7 seconds and following it as it moves and turns.
/// Letting go gives the camera back to the camera it took over from.
namespace openblack::creature_interact
{
/// The camera glides to its place in this long, setting off afresh each frame
constexpr float k_EaseSeconds = 0.7f;
/// The camera looks at this share of the creature's height up its middle
constexpr float k_FocusHeightShare = 0.5f;
/// It stands this many of the creature's heights from where it looks
constexpr float k_DistanceHeights = 1.5f;
/// Its direction from the creature, in the creature's own space (in front of it is -z): 30 units ahead and 15 up, less
/// 0.7 of the creature's height, so it looks up at a tall creature and down at a small one
constexpr float k_Ahead = 30.0f;
constexpr float k_Up = 15.0f;
constexpr float k_UpLessPerHeight = 0.7f;

struct View
{
	glm::vec3 origin;
	glm::vec3 focus;
};
/// The camera's place for a creature standing at a position, so tall, turned about the vertical by a heading as its body
/// is drawn (the heading it is turning to while it turns)
[[nodiscard]] View ViewOf(const glm::vec3& position, float height, float heading);

/// The interaction asks for the camera once, as it starts: a camera that does not give way then (a script's, the
/// temple's or the editor's) keeps it for the whole hold, even once it would give way, until the hand lets go
[[nodiscard]] constexpr bool AsksForTheCamera(bool askedThisHold)
{
	return !askedThisHold;
}

/// The camera that has the view as the interaction asks for it
enum class Holder : uint8_t
{
	/// The player's own camera on the world (watching a fight too)
	PlayersCamera,
	/// Creature Mode's camera, following a creature
	CreatureMode,
	/// A miracle's camera path placed in the world
	PlacedPath,
	/// A script's dual camera, on two things or on a thing and a point
	DualCamera,
	/// A script's camera while it lives
	ScriptCamera,
	/// Any other: the temple's rooms' cameras, which are not the game camera's, or the editor's
	Other,
};
/// Which camera a camera model is: the player's, Creature Mode's or another
[[nodiscard]] Holder HolderOf(const CameraModel& model);
/// Which camera has the view: a script's camera or dual camera that is current over a placed path that has the camera,
/// over the camera's model (`model`, HolderOf)
[[nodiscard]] constexpr Holder CurrentHolder(bool scriptCameraCurrent, bool dualCameraCurrent, bool placedPathHasCamera,
                                             Holder model)
{
	if (dualCameraCurrent)
	{
		return Holder::DualCamera;
	}
	if (scriptCameraCurrent)
	{
		return Holder::ScriptCamera;
	}
	return placedPathHasCamera ? Holder::PlacedPath : model;
}
/// Which camera has the view in the running game (CurrentHolder over the script camera, the placed paths and the model)
[[nodiscard]] Holder CurrentHolder(const Camera& camera);
[[nodiscard]] constexpr std::string_view Name(Holder holder)
{
	switch (holder)
	{
	case Holder::PlayersCamera:
		return "the player's camera";
	case Holder::CreatureMode:
		return "Creature Mode's camera";
	case Holder::PlacedPath:
		return "a miracle's camera path";
	case Holder::DualCamera:
		return "a script's dual camera";
	case Holder::ScriptCamera:
		return "a script's camera";
	case Holder::Other:
		return "another camera (the temple's or the editor's)";
	}
	return "";
}

/// Whether a camera can be left for another: the player's camera, Creature Mode's, a placed path and a dual camera can,
/// a script's camera cannot while it lives. The temple's rooms have cameras of their own that no creature is held in,
/// and the editor's is a developer's tool, so they keep the view
[[nodiscard]] constexpr bool CanBeLeft(Holder holder)
{
	return holder == Holder::PlayersCamera || holder == Holder::CreatureMode || holder == Holder::PlacedPath ||
	       holder == Holder::DualCamera;
}

/// Asked, the camera gives way to the interaction when it can be left and the creature is still in the world
[[nodiscard]] constexpr bool CameraGivesWay(Holder holder, bool creatureAvailable)
{
	return CanBeLeft(holder) && creatureAvailable;
}

/// What the end of a hold does to the camera: the camera that has the view then is taken off at the end of every hold
/// that asked for it, the interaction's own when it was given the view, else whichever camera has the view, even the
/// one that kept it
enum class HoldEnd : uint8_t
{
	/// The hold never asked (the camera's switch off, or no creature)
	Nothing,
	/// The interaction's camera goes, and the one it took over from has the view again
	GiveBack,
	/// The camera that kept the view goes
	TakeOff,
};
[[nodiscard]] constexpr HoldEnd AtTheEndOfTheHold(bool askedThisHold, bool viewTaken)
{
	if (viewTaken)
	{
		return HoldEnd::GiveBack;
	}
	return askedThisHold ? HoldEnd::TakeOff : HoldEnd::Nothing;
}

/// Taken off at the end of a hold that it kept the view through: a script's camera, a dual camera, a placed path or
/// Creature Mode's, each giving the view to the camera under it. The player's own camera is not: what has the view after
/// it depends on the cameras it was put over before, which openblack does not keep. The temple's and the editor's are
/// not the game's camera
[[nodiscard]] constexpr bool TakenOffAtTheEnd(Holder holder)
{
	return holder == Holder::ScriptCamera || holder == Holder::DualCamera || holder == Holder::PlacedPath ||
	       holder == Holder::CreatureMode;
}

/// Given back, the player's camera flies back to where it was; it wooshes when it starts further than this from there
constexpr float k_WooshDistance = 150.0f;
[[nodiscard]] bool WooshesFlyingBack(const glm::vec3& from, const glm::vec3& to);
} // namespace openblack::creature_interact

namespace openblack
{

/// The camera while the hand is held to a creature (see creature_interact). It keeps the camera it took over from, to
/// give it back. It glides on the camera's own zoomers: each frame it has a target it sends the camera there in
/// creature_interact::k_EaseSeconds.
class CreatureInteractCameraModel final: public CameraModel
{
public:
	/// Starts from where the camera is
	CreatureInteractCameraModel(glm::vec3 origin, glm::vec3 focus);

	/// Where the camera now wants to be for the creature; none once the creature is gone, when the camera glides on to
	/// where it was going and stays
	void SetTarget(std::optional<creature_interact::View> view);

	void SetPlayerModel(std::unique_ptr<CameraModel> model) { _playerModel = std::move(model); }
	[[nodiscard]] std::unique_ptr<CameraModel> TakePlayerModel() { return std::move(_playerModel); }
	[[nodiscard]] const CameraModel* GetPlayerModel() const { return _playerModel.get(); }

	std::optional<CameraInterpolationUpdateInfo> Update(std::chrono::microseconds dt, const Camera& camera) override;
	/// The player's controls don't move it
	void HandleActions(std::chrono::microseconds /*dt*/) override {}
	void SetFlight(glm::vec3 origin, glm::vec3 focus) override;
	[[nodiscard]] glm::vec3 GetTargetOrigin() const override { return _last.origin; }
	[[nodiscard]] glm::vec3 GetTargetFocus() const override { return _last.focus; }

private:
	std::optional<creature_interact::View> _target;
	/// The last place it sent the camera to, or where the camera was when it took over
	creature_interact::View _last;
	std::unique_ptr<CameraModel> _playerModel;
};

} // namespace openblack
