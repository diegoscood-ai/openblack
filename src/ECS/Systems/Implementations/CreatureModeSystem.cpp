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

#include <memory>
#include <tuple>
#include <utility>

#include <glm/gtx/vec_swizzle.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
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

/// A script's camera mode (on the camera's stack, current or under a dual camera) or a hand demo has the camera, so a
/// follow ends. A dual camera over the follow leaves it under it, to have the view again once the dual camera goes
bool CameraTakenByScript()
{
	return (Locator::scriptState::has_value() && script_camera::Active()) ||
	       (Locator::inputState::has_value() && hand_demo::IsPlaying());
}

/// The same, a dual camera that has the view, or a script's cinema bars, which turn the interface off: no follow starts
bool CameraKeptFromTheFollow()
{
	return CameraTakenByScript() || (Locator::scriptState::has_value() && script_camera::HasMode()) ||
	       (Locator::inputState::has_value() && !interface_active::IsActive());
}

bool IsFollowable(const Registry& registry, entt::entity creature)
{
	return registry.Valid(creature) && registry.AllOf<components::Creature, components::Transform>(creature);
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

CreatureModeSystem::~CreatureModeSystem() = default;

std::optional<entt::entity> CreatureModeSystem::PlayersCreature() const
{
	return _playersCreature ? _playersCreature(k_LocalPlayer) : std::nullopt;
}

std::optional<creature_follow::View> CreatureModeSystem::GetView() const
{
	return OwnsCamera() ? std::optional(_model->GetView()) : std::nullopt;
}

std::optional<creature_follow::Style> CreatureModeSystem::GetStyle() const
{
	return OwnsCamera() ? std::optional(_model->GetStyle()) : std::nullopt;
}

bool CreatureModeSystem::OwnsCamera() const
{
	return _model != nullptr && Locator::camera::has_value() && &Locator::camera::value().GetModel() == _model;
}

void CreatureModeSystem::ReleaseCamera()
{
	if (OwnsCamera() && _playerModel != nullptr)
	{
		Locator::camera::value().SetModel(std::move(_playerModel));
	}
	_model = nullptr;
	_playerModel.reset();
	_leaving = false;
}

bool CreatureModeSystem::StillValid() const
{
	return FollowedThere() && !CameraTakenByScript();
}

bool CreatureModeSystem::FollowedThere() const
{
	if (!_creature.has_value() || !Locator::entitiesRegistry::has_value())
	{
		return false;
	}
	// a thing that is not a creature stays followed while it is there and available, as the follow's own check has it
	if (_thing)
	{
		const auto thing = script_camera::Thing(*_creature);
		return thing.has_value() && thing->available;
	}
	return IsFollowable(Locator::entitiesRegistry::value(), *_creature);
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
	auto players = std::make_unique<DefaultWorldCameraModel>(camera.GetOrigin(), camera.GetFocus());
	auto* made = players.get();
	_buried = camera.SetModel(std::move(players));
	made->Restart();
	_buriedUnder = script_camera::PushPlayersModeOver();
}

void CreatureModeSystem::DropBuriedFollow()
{
	if (Locator::scriptState::has_value())
	{
		script_camera::KeepUnder(_buriedUnder, std::move(_playerModel));
	}
	_buried.reset();
	_buriedUnder = 0;
	_model = nullptr;
	_playerModel.reset();
	_leaving = false;
}

bool CreatureModeSystem::UncoverFromPlayersCamera(uint32_t playersMode)
{
	if (_buriedUnder == 0 || playersMode != _buriedUnder || _buried == nullptr || !Locator::camera::has_value())
	{
		return false;
	}
	_buriedUnder = 0;
	// the new player's camera over it goes with this
	std::ignore = Locator::camera::value().SetModel(std::move(_buried));
	return true;
}

void CreatureModeSystem::CheckFollow()
{
	if (!_creature.has_value() || FollowedThere())
	{
		return;
	}
	// The follow comes off the camera's stack; the mode's seconds run on
	const bool current = OwnsCamera();
	_creature.reset();
	if (_model == nullptr)
	{
		return;
	}
	if (_buriedUnder != 0)
	{
		// under a new player's mode: it goes quietly
		DropBuriedFollow();
		return;
	}
	if (!current)
	{
		// under the temple's, the editor's or the creature interaction's camera: the camera goes back once they hand it on
		_leaving = true;
		return;
	}
	ReleaseCamera();
	// The player's camera under it is current again and restarted: it flies back to the view it had
	auto& camera = Locator::camera::value();
	camera.GetModel().Resume(camera.GetOriginZoomer().GetCurrentValue());
	ecs::events::Publish(ecs::events::PlayerCameraRestarted {});
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
	_model->SetFocusPoint(script_camera::FollowPoint(*followed, true));
	_model->SetFacing(followed->wallHugAngle);
	return true;
}

bool CreatureModeSystem::TakeCamera(entt::entity followed, glm::vec3 position, float height, bool thing)
{
	auto& camera = Locator::camera::value();
	// A follow under a new player's mode stays there in the original, under the new follow; openblack keeps one follow,
	// so that one comes off the stack (approximate)
	if (_buriedUnder != 0)
	{
		DropBuriedFollow();
	}
	if (OwnsCamera())
	{
		// Already locked on, it moves over to the other creature from where the camera is
		ReleaseCamera();
	}
	// Its camera taken meanwhile by a camera that cannot be left (the creature interaction's, the temple's or the
	// editor's), that camera keeps the view and this mode stays as it is under it
	if (_model != nullptr)
	{
		return false;
	}
	// Only the player's own camera is taken over: not the temple's, nor the editor's while it follows what it picked
	if (dynamic_cast<DefaultWorldCameraModel*>(&camera.GetModel()) == nullptr)
	{
		_creature.reset();
		return false;
	}
	// From where the camera is heading for, as the follow starts from
	auto model = std::make_unique<CreatureCameraModel>(camera.GetOriginZoomer().GetDestination(),
	                                                   camera.GetFocusZoomer().GetDestination(), position, height);
	_model = model.get();
	_playerModel = camera.SetModel(std::move(model));
	CameraModeChanged();
	_creature = followed;
	_pushed = Locator::scriptState::has_value() ? script_camera::OtherModePushed() : 0U;
	_thing = thing;
	_leaving = false;
	// The button that double clicked is still down: only a grip of the land after this one gives the camera back
	_wasGripping = true;
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
	// The saved follow's own view and style, not the ones it would start with; and no button is down from a double click
	_model->SetView(view);
	_model->SetStyle(style);
	_wasGripping = false;
	return true;
}

void CreatureModeSystem::Leave()
{
	_creature.reset();
	if (_model == nullptr)
	{
		return;
	}
	if (_buriedUnder != 0)
	{
		DropBuriedFollow();
		return;
	}
	if (OwnsCamera())
	{
		ReleaseCamera();
		return;
	}
	// The temple, the editor or the creature interaction took the camera from this mode's: it is given back once they
	// return it
	_leaving = true;
}

void CreatureModeSystem::PressCreatureKey()
{
	const auto yours = PlayersCreature();
	// The creature interaction's camera cannot be left: while it has the view, C does nothing
	const bool interactionHasTheView = Locator::camera::has_value() && dynamic_cast<const CreatureInteractCameraModel*>(
	                                                                       &Locator::camera::value().GetModel()) != nullptr;
	// Only a current follow is let go of: one under a new player's mode is not, and C locks on afresh
	const auto following = _buriedUnder != 0 ? std::nullopt : _creature;
	switch (creature_mode::OnCreatureKey(following, yours, interactionHasTheView))
	{
	case creature_mode::KeyAction::Enter:
		Enter(*yours);
		break;
	case creature_mode::KeyAction::Leave:
	{
		const bool given = OwnsCamera();
		Leave();
		if (given)
		{
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
	const auto& land = Locator::terrainSystem::value();
	const auto focus = _model->GetTargetFocus();
	_model->ClearView(land.GetNormalAt(glm::xz(focus)), [&land](glm::vec2 point) { return land.GetHeightAt(point); });
}

void CreatureModeSystem::ReadKeys([[maybe_unused]] const Frame& frame)
{
	if (!Locator::gameActionSystem::has_value() || TempleActive())
	{
		return;
	}
	using input::BindableActionMap;
	using input::UnbindableActionMap;
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
	// The camera was handed back to this mode after leaving was asked for
	if (_leaving && OwnsCamera())
	{
		ReleaseCamera();
	}
	// The new player's mode over the follow is no longer on the stack (a land's change, a full stack): the follow went
	// with it
	if (_buriedUnder != 0 && Locator::scriptState::has_value() && !script_camera::HasPlayersMode(_buriedUnder))
	{
		_creature.reset();
		DropBuriedFollow();
	}

	ReadKeys(frame);

	if (!_creature.has_value())
	{
		return;
	}
	// The temple, the editor and the creature interaction take the camera, and the camera stays with them until they hand
	// it back
	if (TempleActive())
	{
		Leave();
		return;
	}
	if (!OwnsCamera())
	{
		return;
	}
	// The cursor keys alone, or the hand taking a fresh grip of the land, give the player the camera back
	const bool gripped = frame.handGripping && !_wasGripping;
	_wasGripping = frame.handGripping;
	// A script's camera taking over has started the seconds again itself
	if (CameraTakenByScript())
	{
		Leave();
		return;
	}
	// Its place left the camera fence: a new player's camera on top, the follow under it
	if (_model->LeftTheZone())
	{
		GoUnderPlayersCamera();
		return;
	}
	if (_model->WantsToLeave() || gripped)
	{
		Leave();
		CameraModeChanged();
		return;
	}
	// The thing gone: the follow reads nothing until the turn's camera checks take it off (CheckFollow)
	if (!FollowedThere())
	{
		return;
	}

	if (_thing)
	{
		const auto thing = script_camera::Thing(*_creature);
		_model->SetFocusPoint(script_camera::FollowPoint(*thing, true));
		_model->SetFacing(thing->wallHugAngle);
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<const components::Transform>(*_creature);
	_model->SetTarget(transform.position, creature_mode::CreatureHeight(creature_pose::DrawnSize(registry, *_creature)));
	// a heading kept behind the creature (a script's follow of it, saved and loaded) turns with the way it faces
	if (_model->GetStyle().behind)
	{
		const auto thing = script_camera::Thing(*_creature);
		_model->SetFacing(thing.has_value() ? thing->wallHugAngle : std::nullopt);
	}
}

} // namespace openblack::ecs::systems
