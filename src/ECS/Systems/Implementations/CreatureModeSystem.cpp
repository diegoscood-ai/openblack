/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureModeSystem.h"

#include <algorithm>
#include <memory>
#include <tuple>
#include <utility>

#include <glm/gtx/vec_swizzle.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "Camera/CameraChecks.h"
#include "Camera/CreatureCameraModel.h"
#include "Camera/CreatureInteractCameraModel.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/ScriptCamera.h"
#include "Creature/CreatureMode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreaturePose.h"
#include "ECS/Events/CameraEvents.h"
#include "ECS/Events/Publish.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CameraStackInterface.h"
#include "Hand/HandDemo.h"
#include "Help/HelpSystem.h"
#include "Input/GameActionMapInterface.h"
#include "Input/InterfaceActive.h"
#include "Locator.h"

namespace openblack::ecs::systems
{

namespace
{
/// The local player, whose creature C locks onto
constexpr PlayerNames k_LocalPlayer = PlayerNames::PLAYER_ONE;

bool TempleActive()
{
	return Locator::temple::has_value() && Locator::temple::value().Active();
}

/// The camera's current mode cannot be left (a script's camera mode still going, or the creature interaction's camera),
/// or a hand demo has the camera: no follow starts. A script mode under a current dual camera does not hold it back
bool CameraTakenByScript()
{
	return (Locator::cameraStack::has_value() && !Locator::cameraStack::value().Modes().CanLeaveCurrent()) ||
	       (Locator::inputState::has_value() && hand_demo::IsPlaying());
}

/// A hand demo has the camera: the follow that has the view ends
bool HandDemoPlaying()
{
	return Locator::inputState::has_value() && hand_demo::IsPlaying();
}

/// The same, or a script's cinema bars, which turn the interface off: no follow starts. A current dual camera can be
/// left, so a follow goes on top of it, a script's camera mode under it or not
bool CameraKeptFromTheFollow()
{
	return CameraTakenByScript() || (Locator::inputState::has_value() && !interface_active::IsActive());
}

bool IsFollowable(const Registry& registry, entt::entity creature)
{
	return registry.Valid(creature) && registry.AllOf<components::Creature, components::Transform>(creature);
}

/// The camera's stack of modes, which holds the follow's entry; none without the camera stack service
camera_stack::ModeStack* Stack()
{
	return Locator::cameraStack::has_value() ? &Locator::cameraStack::value().Modes() : nullptr;
}

/// The script camera's modes are on the camera's stack of modes
bool ScriptCameraThere()
{
	return Locator::scriptState::has_value() && Locator::cameraStack::has_value();
}

/// The follow took the camera, or the player was given it back: in the original both put a new camera mode on the
/// camera's stack or take the follow off it, which starts the camera mode's seconds again
void CameraModeChanged()
{
	if (Locator::scriptState::has_value())
	{
		script_camera::ModeChanged();
	}
}
} // namespace

CreatureModeSystem::CreatureModeSystem(CreatureLookup playersCreature)
    : _playersCreature(std::move(playersCreature))
{
}

CreatureModeSystem::~CreatureModeSystem()
{
	// the follows' entries on the camera's stack go with them
	if (auto* stack = Stack(); stack != nullptr)
	{
		for (const auto& follow : _follows)
		{
			if (!follow.leaving)
			{
				std::ignore = stack->RemoveFollow(follow.key);
			}
		}
	}
	_follows.clear();
}

std::optional<std::size_t> CreatureModeSystem::NewestIndex() const
{
	for (auto i = _follows.size(); i-- > 0;)
	{
		if (!_follows[i].leaving)
		{
			return i;
		}
	}
	return std::nullopt;
}

const CreatureModeSystem::Follow* CreatureModeSystem::Newest() const
{
	const auto index = NewestIndex();
	return index.has_value() ? &_follows[*index] : nullptr;
}

CreatureModeSystem::Follow* CreatureModeSystem::Newest()
{
	const auto index = NewestIndex();
	return index.has_value() ? &_follows[*index] : nullptr;
}

bool CreatureModeSystem::HasCameraModel(const Follow& follow) const
{
	return follow.model != nullptr && Locator::camera::has_value() && &Locator::camera::value().GetModel() == follow.model;
}

bool CreatureModeSystem::OwnsCamera() const
{
	const auto* newest = Newest();
	return newest != nullptr && HasCameraModel(*newest);
}

bool CreatureModeSystem::HasTheView() const
{
	const auto* newest = Newest();
	if (newest == nullptr || !HasCameraModel(*newest))
	{
		return false;
	}
	const auto* stack = Stack();
	return stack == nullptr || stack->CurrentFollow() == newest->key;
}

bool CreatureModeSystem::TakenByAnother(const Follow& follow) const
{
	return follow.held == nullptr && !HasCameraModel(follow);
}

std::optional<entt::entity> CreatureModeSystem::GetCreature() const
{
	const auto* newest = Newest();
	return newest != nullptr ? std::optional(newest->followed) : std::nullopt;
}

std::optional<entt::entity> CreatureModeSystem::GetCreatureInView() const
{
	return HasTheView() ? std::optional(Newest()->followed) : std::nullopt;
}

std::optional<entt::entity> CreatureModeSystem::PlayersCreature() const
{
	return _playersCreature ? _playersCreature(k_LocalPlayer) : std::nullopt;
}

std::optional<creature_follow::View> CreatureModeSystem::GetView() const
{
	return OwnsCamera() ? std::optional(Newest()->model->GetView()) : std::nullopt;
}

std::optional<creature_follow::Style> CreatureModeSystem::GetStyle() const
{
	return OwnsCamera() ? std::optional(Newest()->model->GetStyle()) : std::nullopt;
}

std::optional<CreatureModeSystem::CurrentFollow> CreatureModeSystem::GetCurrentFollow() const
{
	const auto* newest = Newest();
	if (newest == nullptr || newest->model == nullptr)
	{
		return std::nullopt;
	}
	if (!OwnsCamera())
	{
		// Inside the citadel the temple keeps the camera's model aside, and the follow is still the current mode it was as
		// the player went in: the top of the stack, its model the one the temple took
		const auto* stack = Stack();
		if (!TempleActive() || !TakenByAnother(*newest) || (stack != nullptr && stack->CurrentFollow() != newest->key))
		{
			return std::nullopt;
		}
	}
	return CurrentFollow {.view = newest->model->GetView(), .style = newest->model->GetStyle()};
}

std::unique_ptr<CameraModel> CreatureModeSystem::TakeModelUnder(std::size_t index)
{
	auto& follow = _follows[index];
	if (follow.wentOver != nullptr)
	{
		return std::move(follow.wentOver);
	}
	// it took the camera over from the follow under it, which holds its own model meanwhile
	if (index > 0 && _follows[index - 1].under == 0)
	{
		return std::move(_follows[index - 1].held);
	}
	return nullptr;
}

CreatureModeSystem::Dropped CreatureModeSystem::Drop(std::size_t index)
{
	Dropped dropped;
	auto& follow = _follows[index];
	if (auto* stack = Stack(); stack != nullptr && !follow.leaving)
	{
		dropped.current = stack->RemoveFollow(follow.key);
	}
	// A follow that was the current mode over a dual camera leaves the dual camera the camera's zoomers as it left them
	const bool takeBack = dropped.current && ScriptCameraThere();
	if (HasCameraModel(follow))
	{
		// its cleanup: the camera's model goes back to the one under it
		dropped.hadCamera = true;
		if (auto under = TakeModelUnder(index); under != nullptr)
		{
			std::ignore = Locator::camera::value().SetModel(std::move(under));
		}
	}
	else if (follow.under != 0)
	{
		// Under a new player's mode: what was under the follow is what that mode has under it from then on, the player's
		// camera kept for it, or the follow under it
		if (follow.wentOver != nullptr)
		{
			if (Locator::scriptState::has_value())
			{
				script_camera::KeepUnder(follow.under, std::move(follow.wentOver));
			}
		}
		else if (index > 0 && _follows[index - 1].under == 0 && _follows[index - 1].held != nullptr)
		{
			_follows[index - 1].under = follow.under;
		}
	}
	else if (follow.held != nullptr)
	{
		// Under a newer follow, which took its model: that follow has what was under this one from then on. With none (the
		// new player's mode going over it deleted it from a full stack, the player's own mode gone before it), nothing
		if (index + 1 < _follows.size() && _follows[index + 1].wentOver == nullptr)
		{
			_follows[index + 1].wentOver = std::move(follow.wentOver);
		}
	}
	else
	{
		// The temple, the editor or the creature interaction took the camera from it: the camera is given back once they
		// return it
		follow.leaving = true;
		follow.followed = entt::null;
		if (takeBack)
		{
			script_camera::TakeBackFromAnotherMode();
		}
		return dropped;
	}
	_follows.erase(_follows.begin() + static_cast<std::ptrdiff_t>(index));
	if (takeBack)
	{
		script_camera::TakeBackFromAnotherMode();
	}
	return dropped;
}

void CreatureModeSystem::ReleaseLeaving()
{
	for (auto i = _follows.size(); i-- > 0;)
	{
		if (_follows[i].leaving && HasCameraModel(_follows[i]))
		{
			if (auto under = TakeModelUnder(i); under != nullptr)
			{
				std::ignore = Locator::camera::value().SetModel(std::move(under));
			}
			_follows.erase(_follows.begin() + static_cast<std::ptrdiff_t>(i));
		}
	}
}

bool CreatureModeSystem::FollowedThere(const Follow& follow) const
{
	if (follow.leaving || !Locator::entitiesRegistry::has_value())
	{
		return false;
	}
	// a thing that is not a creature stays followed while it is there and available, as the follow's own check has it
	if (follow.thing)
	{
		const auto thing = script_camera::Thing(follow.followed);
		return thing.has_value() && thing->available;
	}
	return IsFollowable(Locator::entitiesRegistry::value(), follow.followed);
}

void CreatureModeSystem::GoUnderPlayersCamera()
{
	auto& camera = Locator::camera::value();
	if (!Locator::scriptState::has_value())
	{
		// no stack of modes to keep it under
		Leave();
		CameraModeChanged();
		return;
	}
	auto* stack = Stack();
	const auto key = Newest()->key;
	// what sent it under is spent: the keys and its place are read afresh once it is the current mode again
	Newest()->model->GoneUnder();
	auto players = std::make_unique<DefaultWorldCameraModel>(camera.GetOrigin(), camera.GetFocus());
	auto* made = players.get();
	Newest()->held = camera.SetModel(std::move(players));
	made->Restart();
	// The new mode's push onto a full stack may delete the follow from the bottom
	const auto playersMode = script_camera::PushPlayersModeOver();
	const auto follow = std::ranges::find_if(_follows, [key](const Follow& candidate) { return candidate.key == key; });
	if (follow != _follows.end())
	{
		follow->under = playersMode;
	}
	// The new mode goes over the whole stack: the placed paths under the follow, or over it under the mode the new one
	// goes over, too, which have the camera again only once it is popped
	if (playersMode != 0 && stack != nullptr)
	{
		std::ignore = stack->CoverPaths(playersMode);
	}
}

bool CreatureModeSystem::TakeBackFromUnderPlayersCamera(uint32_t playersMode)
{
	if (playersMode == 0 || !Locator::camera::has_value())
	{
		return false;
	}
	const auto follow = std::ranges::find_if(
	    _follows, [playersMode](const Follow& candidate) { return !candidate.leaving && candidate.under == playersMode; });
	if (follow == _follows.end() || follow->held == nullptr)
	{
		return false;
	}
	follow->under = 0;
	// the new player's camera over it goes with this
	std::ignore = Locator::camera::value().SetModel(std::move(follow->held));
	return true;
}

bool CreatureModeSystem::KeepUnderPlayersMode(uint32_t playersMode)
{
	auto* newest = Newest();
	if (playersMode == 0 || newest == nullptr || !HasCameraModel(*newest))
	{
		return false;
	}
	auto& camera = Locator::camera::value();
	newest->held = camera.SetModel(std::make_unique<DefaultWorldCameraModel>(camera.GetOrigin(), camera.GetFocus()));
	newest->under = playersMode;
	return true;
}

bool CreatureModeSystem::GoUnderNewPlayersMode()
{
	if (!OwnsCamera())
	{
		return false;
	}
	GoUnderPlayersCamera();
	return true;
}

void CreatureModeSystem::DeleteFollow(uint64_t follow)
{
	const auto found = std::ranges::find_if(
	    _follows, [follow](const Follow& candidate) { return !candidate.leaving && candidate.key == follow; });
	if (found != _follows.end())
	{
		std::ignore = Drop(static_cast<std::size_t>(found - _follows.begin()));
	}
}

bool CreatureModeSystem::RemoveInvalid()
{
	// From the bottom of the stack: each follow whose thing is gone comes off; the mode's seconds run on
	bool went = false;
	for (std::size_t i = 0; i < _follows.size();)
	{
		if (_follows[i].leaving || FollowedThere(_follows[i]))
		{
			++i;
			continue;
		}
		const auto count = _follows.size();
		// The camera's model goes back to the one under it; the turn's checks restart whatever is current then
		went = Drop(i).hadCamera || went;
		if (_follows.size() == count)
		{
			++i; // waiting off the stack for the camera back
		}
	}
	return went;
}

bool CreatureModeSystem::MayTakeCamera() const
{
	return Locator::camera::has_value() && Locator::entitiesRegistry::has_value() && !TempleActive() &&
	       !CameraKeptFromTheFollow();
}

bool CreatureModeSystem::Enter(entt::entity creature)
{
	if (!MayTakeCamera())
	{
		return false;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	if (!IsFollowable(registry, creature))
	{
		return false;
	}
	const auto& transform = registry.Get<const components::Transform>(creature);
	// it frames the height of the size the creature is drawn at (the pen's in its temple's pen)
	return TakeCamera(creature, transform.position, creature_mode::CreatureHeight(creature_pose::DrawnSize(registry, creature)),
	                  false);
}

bool CreatureModeSystem::EnterThing(entt::entity thing)
{
	if (!MayTakeCamera())
	{
		return false;
	}
	const auto followed = script_camera::Thing(thing);
	if (!followed.has_value() || !followed->available)
	{
		return false;
	}
	if (!TakeCamera(thing, followed->mapPoint, followed->height, true))
	{
		return false;
	}
	auto* model = Newest()->model;
	model->SetFocusPoint(script_camera::FollowPoint(*followed, true));
	model->SetFacing(followed->wallHugAngle);
	return true;
}

bool CreatureModeSystem::TakeCamera(entt::entity followed, glm::vec3 position, float height, bool thing)
{
	auto& camera = Locator::camera::value();
	// A follow's camera taken meanwhile by a camera that cannot be left (the creature interaction's, the temple's or the
	// editor's): that camera keeps the view and the follow stays as it is under it
	if (std::ranges::any_of(_follows, [this](const Follow& follow) { return TakenByAnother(follow); }))
	{
		return false;
	}
	// Only the player's own camera, or a follow's, is taken over: not the temple's, nor the editor's while it follows
	// what it picked. A follow there already (current, under a dual camera or under a new player's mode) stays on the
	// stack under the new one, which goes on top of the current mode
	auto* newest = Newest();
	const bool overFollow = newest != nullptr && HasCameraModel(*newest);
	if (!overFollow && dynamic_cast<DefaultWorldCameraModel*>(&camera.GetModel()) == nullptr)
	{
		return false;
	}
	// A dual camera that drove the camera hands its zoomers back for the follow to start from
	if (ScriptCameraThere())
	{
		script_camera::HandOverToAnotherMode();
	}
	// From where the camera is heading for, as the follow starts from
	auto model = std::make_unique<CreatureCameraModel>(camera.GetOriginZoomer().GetDestination(),
	                                                   camera.GetFocusZoomer().GetDestination(), position, height);
	Follow follow {.key = _nextKey++, .followed = followed, .thing = thing, .model = model.get()};
	auto previous = camera.SetModel(std::move(model));
	if (overFollow)
	{
		newest->held = std::move(previous); // the follow under it keeps its model
	}
	else
	{
		follow.wentOver = std::move(previous);
	}
	CameraModeChanged();
	// its entry on top of the camera's stack of modes; a full stack deletes its bottom mode first, whatever it is, through
	// that mode's owner (a follow under this one handing what was under it to the follow over it)
	auto* stack = Stack();
	const bool dropped = stack != nullptr && stack->PushFollow(follow.key) != camera_stack::ModeStack::Dropped::None;
	_follows.push_back(std::move(follow));
	if (dropped)
	{
		script_camera::DeleteDroppedModes();
	}
	return true;
}

bool CreatureModeSystem::Restore(entt::entity thing, const creature_follow::View& view, const creature_follow::Style& style)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return false;
	}
	// a creature as Creature Mode follows it; any other thing as the follow reads things
	const bool creature = IsFollowable(Locator::entitiesRegistry::value(), thing);
	if (!(creature ? Enter(thing) : EnterThing(thing)))
	{
		return false;
	}
	// The saved follow's own view and style, not the ones it would start with
	auto* model = Newest()->model;
	model->SetView(view);
	model->SetStyle(style);
	return true;
}

void CreatureModeSystem::Leave()
{
	std::ignore = PopNewest();
}

bool CreatureModeSystem::PopNewest()
{
	const auto index = NewestIndex();
	if (!index.has_value())
	{
		return false;
	}
	if (!Drop(*index).current)
	{
		return false;
	}
	// Popped off a stack it leaves empty, with the player's own mode deleted by a full stack: a new player's mode starts
	// from the camera as it is
	if (ScriptCameraThere() && script_camera::StartNewPlayersModeIfGone())
	{
		return false;
	}
	// The follow under it that has the camera's model again, the current mode, starts again with its time from 0, as the
	// pop of a mode restarts the one under it
	if (HasTheView())
	{
		Newest()->model->Restart();
		return false;
	}
	return true;
}

void CreatureModeSystem::RestartPlayersModeUnder()
{
	if (!Locator::camera::has_value())
	{
		return;
	}
	// Only the player's own mode is started again: a placed path under it sets its close near plane again, which the
	// frame's camera update does, and a dual camera's or a script mode's restart does nothing
	if (const auto* stack = Stack();
	    stack != nullptr && (stack->OtherCurrent() || (!stack->Empty() && !stack->PlayerCurrent())))
	{
		return;
	}
	auto& camera = Locator::camera::value();
	if (dynamic_cast<DefaultWorldCameraModel*>(&camera.GetModel()) == nullptr)
	{
		return;
	}
	// It starts again keeping a watched fight whose fighters are both there, and flies back to the view it had
	camera.GetModel().Resume(camera.GetOriginZoomer().GetCurrentValue());
	ecs::events::Publish(ecs::events::PlayerCameraRestarted {});
}

bool CreatureModeSystem::LetGoOfScriptFollow(entt::entity yours)
{
	if (!ScriptCameraThere() || !Locator::camera::has_value() || !script_camera::ScriptModeCurrent() ||
	    script_camera::Get().positionThing != yours)
	{
		return false;
	}
	// The script mode is popped as another camera takes it off: the script is not told, and still thinks it has the
	// camera. The mode under it starts again: a follow under it from the camera as it is, the player's camera flying back
	// to its own view
	auto& camera = Locator::camera::value();
	const auto popped = script_camera::TakeOffScriptMode(camera.GetOriginZoomer(), camera.GetFocusZoomer());
	if (popped == camera_stack::ModeStack::Popped::None)
	{
		return false;
	}
	if (popped == camera_stack::ModeStack::Popped::NewPlayersMode)
	{
		// a new player's mode on the emptied stack starts from the camera as it is, with no flight
		camera.GetModel().Restart();
		ecs::events::Publish(ecs::events::PlayerCameraRestarted {});
	}
	else
	{
		RestartPlayersModeUnder();
	}
	// Then a new player's mode on top of the mode now current unless that is a player's mode, as C's letting go of a
	// follow makes
	std::ignore = camera_checks::PutNewPlayersModeOver(this);
	CameraModeChanged();
	return true;
}

void CreatureModeSystem::LeaveAll()
{
	// from the newest down, each giving the camera's model back to the one under it, nothing restarted
	for (auto index = NewestIndex(); index.has_value(); index = NewestIndex())
	{
		std::ignore = Drop(*index);
	}
}

void CreatureModeSystem::LeaveAllBehindTemple(Camera& world)
{
	// from the newest down, as LeaveAll; the island's camera behind the temple has the follow's model, so it is given the
	// model under it there, rather than waiting off the stack for the temple to hand the model back
	for (auto index = NewestIndex(); index.has_value(); index = NewestIndex())
	{
		auto& follow = _follows[*index];
		if (follow.model == nullptr || &world.GetModel() != follow.model)
		{
			std::ignore = Drop(*index);
			continue;
		}
		if (auto* stack = Stack(); stack != nullptr)
		{
			std::ignore = stack->RemoveFollow(follow.key);
		}
		if (auto under = TakeModelUnder(*index); under != nullptr)
		{
			std::ignore = world.SetModel(std::move(under));
		}
		_follows.erase(_follows.begin() + static_cast<std::ptrdiff_t>(*index));
	}
}

void CreatureModeSystem::PressCreatureKey()
{
	const auto yours = PlayersCreature();
	// A script's camera mode is a follow too: following the player's creature, C lets go of it as of Creature Mode's own
	if (yours.has_value() && LetGoOfScriptFollow(*yours))
	{
		return;
	}
	// The creature interaction's camera cannot be left: while it has the view, C does nothing
	const bool interactionHasTheView = Locator::camera::has_value() && dynamic_cast<const CreatureInteractCameraModel*>(
	                                                                       &Locator::camera::value().GetModel()) != nullptr;
	// Only the follow that is the current mode is let go of (or one whose camera another took, which then keeps the
	// view): one under another mode is not, and C locks on afresh over that mode
	const auto* newest = Newest();
	const bool letGo = newest != nullptr && (TakenByAnother(*newest) || HasTheView());
	const auto following = letGo ? std::optional(newest->followed) : std::nullopt;
	switch (creature_mode::OnCreatureKey(following, yours, interactionHasTheView))
	{
	case creature_mode::KeyAction::Enter:
		Enter(*yours);
		break;
	case creature_mode::KeyAction::Leave:
	{
		const bool given = OwnsCamera();
		// The follow popped, the mode under it starts again, as a pop restarts the mode under: the player's camera flies
		// back to its own view
		if (PopNewest() && given)
		{
			RestartPlayersModeUnder();
		}
		// The follow popped, a new player's mode goes on top of the mode then current unless that is a player's mode: a
		// follow under it, a dual camera or a placed path, which stays under it
		if (given)
		{
			std::ignore = camera_checks::PutNewPlayersModeOver(this);
			CameraModeChanged();
		}
		break;
	}
	case creature_mode::KeyAction::None:
		break;
	}
}

void CreatureModeSystem::ClearView()
{
	if (!OwnsCamera() || !Locator::terrainSystem::has_value())
	{
		return;
	}
	auto* model = Newest()->model;
	const auto& land = Locator::terrainSystem::value();
	const auto focus = model->GetTargetFocus();
	model->ClearView(land.GetNormalAt(glm::xz(focus)), [&land](glm::vec2 point) { return land.GetHeightAt(point); });
}

void CreatureModeSystem::ReadCreatureKey()
{
	if (!Locator::gameActionSystem::has_value() || TempleActive())
	{
		return;
	}
	using input::BindableActionMap;
	const auto& actions = Locator::gameActionSystem::value();
	// C locks onto the player's creature, and lets go of it again (the script's interface level holds it back in the
	// action map, which then reads it as not held)
	// nor while a script holds the wide screen
	const auto* helpSystem = help::Get();
	if (creature_mode::CreatureKeyActs(actions.Get(BindableActionMap::ZOOM_TO_CREATURE) &&
	                                       actions.GetChanged(BindableActionMap::ZOOM_TO_CREATURE),
	                                   helpSystem != nullptr && helpSystem->IsScriptWideScreen()))
	{
		PressCreatureKey();
	}
}

void CreatureModeSystem::ReadKeys([[maybe_unused]] const Frame& frame)
{
	if (!Locator::gameActionSystem::has_value() || TempleActive())
	{
		return;
	}
	ReadCreatureKey();
	// A double click on a creature, anyone's, that the player's camera took in its last update locks onto it, before
	// the camera moves this frame
	if (const auto creature = std::exchange(_lockOnNext, std::nullopt); creature.has_value())
	{
		Enter(*creature);
	}
}

void CreatureModeSystem::Update([[maybe_unused]] std::chrono::microseconds dt, const Frame& frame)
{
	if (!Locator::camera::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	// The camera was handed back to a follow after leaving was asked for
	ReleaseLeaving();
	// A new player's mode over a follow is no longer on the stack (a land's change, a full stack): the follow went with it
	if (Locator::scriptState::has_value())
	{
		for (auto i = _follows.size(); i-- > 0;)
		{
			if (i < _follows.size() && !_follows[i].leaving && _follows[i].under != 0 &&
			    !script_camera::HasPlayersMode(_follows[i].under))
			{
				std::ignore = Drop(i);
			}
		}
	}

	ReadKeys(frame);

	if (!IsActive())
	{
		return;
	}
	// Inside the temple its rooms have the camera, its model kept aside, and every follow stays on the stack as it was: the
	// way in touches no camera mode, and the world a follow reads stands still there, with no camera keys. Given its model
	// back on the way out, the follow goes on
	if (TempleActive())
	{
		return;
	}
	// Only the current mode is updated: a follow under another mode (a script's camera mode, a dual camera, a path, a new
	// player's mode, a newer follow) or whose camera the editor or the creature interaction took does nothing meanwhile
	if (!HasTheView())
	{
		return;
	}
	auto& follow = *Newest();
	auto& model = *follow.model;
	// The cursor keys alone, or the hand gripping the land, give the player a camera of their own: the grip is read at
	// every update from the interface's hand state, a grip held from before this follow had the view too
	const bool gripped = creature_mode::GripPutsPlayersCameraOver(frame.interfaceHandState);
	// A hand demo takes the camera
	if (HandDemoPlaying())
	{
		Leave();
		return;
	}
	// Its place left the camera fence, the cursor keys alone or a grip of the land: a new player's camera goes on
	// top and the follow stays under it, until the turn's camera checks find its thing gone or that mode is popped
	if (model.LeftTheZone() || model.WantsToLeave() || gripped)
	{
		GoUnderPlayersCamera();
		return;
	}
	// The thing gone: the follow reads nothing until the turn's camera checks take it off (RemoveInvalid)
	if (!FollowedThere(follow))
	{
		return;
	}

	if (follow.thing)
	{
		const auto thing = script_camera::Thing(follow.followed);
		model.SetFocusPoint(script_camera::FollowPoint(*thing, true));
		model.SetFacing(thing->wallHugAngle);
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<const components::Transform>(follow.followed);
	model.SetTarget(transform.position, creature_mode::CreatureHeight(creature_pose::DrawnSize(registry, follow.followed)));
	// a heading kept behind the creature (a script's follow of it, saved and loaded) turns with the way it faces
	if (model.GetStyle().behind)
	{
		const auto thing = script_camera::Thing(follow.followed);
		model.SetFacing(thing.has_value() ? thing->wallHugAngle : std::nullopt);
	}
}

} // namespace openblack::ecs::systems
