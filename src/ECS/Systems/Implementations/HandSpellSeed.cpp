/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A spell seed in the player's hand: the interface's seed branches (the action pressed while holding, the apply
// states, the apply to a map point or an object, the failed apply, the forced drop) and the seed's interface
// behaviour. Wiki: docs/wiki/magic/hand-casting.md#casting-from-the-hand-gestures-and-hand-effects.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <array>
#include <optional>
#include <string>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Camera/Camera.h"
#include "Debug/DebugEnv.h"
#include "Debug/TraceLog.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/HandSeedUnlock.h"
#include "ECS/HeldApply.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/ScreenshotRequestSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "Game.h"
#include "GameClock.h"
#include "HandSystem.h"
#include "HandSystemDetail.h"
#include "Help/HelpProfile.h"
#include "InfoConstants.h"
#include "Input/GameCursor.h"
#include "Input/GamePackets.h"
#include "Locator.h"
#include "Magic/CastRules.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellSeed.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Magic/MagicTables.h"
#include "Magic/MagicWorldInterface.h"
#include "Magic/Objects/MagicFireBall.h"
#include "Particles/PSysManager.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Worship/Worship.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::hand_detail;

namespace
{
namespace gestures = magic::gestures;

/// ApplyThisTo* results (HandleSeedApplyResult)
constexpr int k_ResultConsumed = 3;
constexpr int k_ResultNothing = 5;
constexpr int k_ResultRemoved = 0x16;

/// SPOT_VISUAL_SUCEED_CAST / SPOT_VISUAL_FAIL_CAST
constexpr auto k_SpotVisualSucceedCast = SpotVisualType::SpellSucceedCast;
constexpr int k_SpotVisualFailCast = 4;

bool SeedTrace()
{
	static const bool trace = debug_env::SpellTrace() || debug_env::GestureTrace();
	return debug_env::TraceLogs(trace);
}

SpellSeed& SeedOf(entt::entity seed)
{
	return Locator::entitiesRegistry::value().Get<SpellSeed>(seed);
}

const GSpellSeedInfo& InfoOf(entt::entity seed)
{
	return magic::seed::InfoOf(SeedOf(seed));
}

/// castType HAND_GESTURE: applied only after the gesture is recognised
bool ApplyOnlyAfterRecSystem(entt::entity seed)
{
	return InfoOf(seed).castType == SpellCastType::SpellCastHandGesture;
}

/// castType IN_HAND: the locked apply, repeated while the button is held
bool ValidForLockedApplyProcess(entt::entity seed)
{
	return InfoOf(seed).castType == SpellCastType::SpellCastInHand;
}

/// The spell stays in the hand after the cast
bool IsSpellKeptInHand(entt::entity seed)
{
	return InfoOf(seed).isKeptInHand != 0;
}

/// The sizing gesture (CIRCLE for storm and the shields)
uint32_t SizingGesture(entt::entity seed)
{
	return static_cast<uint32_t>(InfoOf(seed).sizingGesture);
}

/// A seed with a sizing gesture needs that gesture in the packet
bool GestureAllowsCast(entt::entity seed, gestures::Gesture gesture)
{
	const auto sizing = SizingGesture(seed);
	return sizing == 0 || (gesture & 0xFFu) == sizing;
}

/// Ready, and CanCast there (the cast rule and the class check)
bool ValidToApplyThisToMapCoord(entt::entity seed, const glm::vec3& mapPosition)
{
	return SeedOf(seed).ready && magic::seed::CanCast(seed, mapPosition);
}

/// A return point of the seed's player (a spell dispenser or a worship site), a magic fireball, else a seed with
/// castOnObject, the cast rule at the object and the class's object check.
bool CanCastOnObject(entt::entity seed, entt::entity object)
{
	const auto& component = SeedOf(seed);
	if (worship::IsSeedReturnPoint(object, component.creator.player))
	{
		return true; // Worship/Worship.cpp: a dispenser, a WorshipTotem or a spell icon always takes the seed back
	}
	auto& registry = Locator::entitiesRegistry::value();
	// a magic fireball answers for itself: a FIRE seed takes it in, whoever's it is, and no other seed is cast on it
	if (registry.AllOf<MagicFireBall>(object))
	{
		return magic::seed::ValidToApplyToMagicFireBall(component.seedType, true);
	}
	const auto& info = magic::seed::InfoOf(component);
	if (info.castOnObject == 0)
	{
		return false;
	}
	const auto* transform = registry.TryGet<const Transform>(object);
	if (transform == nullptr)
	{
		return false;
	}
	const auto type = magic::seed::MagicTypeOf(component);
	const auto position = magic::ToMap(transform->position);
	if (!magic::cast_rules::CanCastRule(magic::GetMagicInfo(Locator::infoConstants::value(), type), position,
	                                    component.creator.player))
	{
		return false;
	}
	return magic::cast_rules::CanCastOn(type, object);
}

/// Ready, the target not highlighted by the script, CanCast(object).
/// (approximate) the script highlight test is not ported
bool ValidToApplyThisToObject(entt::entity seed, entt::entity target)
{
	return SeedOf(seed).ready && CanCastOnObject(seed, target);
}

/// The magic info's isSpellSeedDrawnInHand == 1 (the fire, lightning, heal and storm seeds are only the in-hand
/// effect)
bool IsG3DObjectDrawnInHand(entt::entity seed)
{
	const auto type = magic::seed::MagicTypeOf(SeedOf(seed));
	return magic::GetMagicInfo(Locator::infoConstants::value(), type).isSpellSeedDrawnInHand == 1;
}

void ShowSeedMesh(entt::entity seed, bool show)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(seed) || !registry.AllOf<SpellSeed>(seed))
	{
		return;
	}
	const bool has = registry.AllOf<Mesh>(seed);
	if (show && !has)
	{
		registry.Assign<Mesh>(seed, resources::HashIdentifier(InfoOf(seed).mesh), static_cast<int8_t>(0),
		                      static_cast<int8_t>(0));
		registry.SetDirty();
	}
	else if (!show && has)
	{
		registry.Remove<Mesh>(seed);
		registry.SetDirty();
	}
}

/// OPENBLACK_TEST_CAST="press@t0,release@t1[,press@t2,release@t3...][,shot@t]" (seconds after the land exists); shot
/// takes a screenshot then into OPENBLACK_TEST_SHOT_PATH (for screenshots at a game time rather than a frame; a "{}" in
/// the path is replaced by the shot's time in tenths of a second, for several shots in one run)
struct TestCastEvent
{
	enum class Kind
	{
		Press,
		Release,
		Shot,
	};
	Kind kind;
	float time;
	bool done {false};
};

/// What the test cast keeps between calls, in the debug hooks' store (Locator::debugHooks)
struct HandSpellSeedDebugHooksState
{
	std::optional<std::vector<TestCastEvent>> events; // OPENBLACK_TEST_CAST, parsed on first use
};

HandSpellSeedDebugHooksState& HandSpellSeedDebugHooksData()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("ecs::systems::HandSystem: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<HandSpellSeedDebugHooksState>();
}

std::vector<TestCastEvent>& TestCastEvents()
{
	auto& events = HandSpellSeedDebugHooksData().events;
	if (events.has_value())
	{
		return *events;
	}
	events = [] {
		std::vector<TestCastEvent> list;
		const char* value = std::getenv("OPENBLACK_TEST_CAST");
		if (value == nullptr)
		{
			return list;
		}
		std::string text(value);
		size_t start = 0;
		while (start < text.size())
		{
			const size_t end = std::min(text.find(',', start), text.size());
			const auto item = text.substr(start, end - start);
			std::array<char, 16> kind = {};
			float time = 0.0f;
			if (std::sscanf(item.c_str(), "%15[^@]@%f", kind.data(), &time) == 2)
			{
				const auto type = std::strcmp(kind.data(), "press") == 0  ? TestCastEvent::Kind::Press
				                  : std::strcmp(kind.data(), "shot") == 0 ? TestCastEvent::Kind::Shot
				                                                          : TestCastEvent::Kind::Release;
				list.push_back({type, time});
			}
			start = end + 1;
		}
		return list;
	}();
	return *events;
}
} // namespace

bool hand_detail::SeedValidToApplyToObject(entt::entity seed, entt::entity target)
{
	return ValidToApplyThisToObject(seed, target);
}

bool hand_detail::SeedValidToApplyToMapCoord(entt::entity seed, const glm::vec3& mapPosition)
{
	return ValidToApplyThisToMapCoord(seed, mapPosition);
}

bool HandSystem::IsHoldingSeed() const noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	return _held && ecs::IsAvailable(*_held) && registry.AllOf<SpellSeed>(*_held);
}

bool HandSystem::IsHandReadyForObject() const noexcept
{
	// space in the hands and no locked hand (a tug or a locked select here)
	return !_held && !_tug && !_pickSource;
}

void HandSystem::GetSpellInfo(glm::vec3& interfacePos, glm::vec3& handPos, glm::vec3& cameraForward,
                              glm::vec3& velocity) const noexcept
{
	// The spell info from the synced status: the interface's map point as a world point, the hand, the camera's
	// forward (focus - position, normalised) and the hand's velocity
	interfacePos = SyncMapPoint();
	handPos = _turnHand;
	const glm::vec3 forward = _syncCameraFocus - _syncCameraPosition;
	cameraForward = glm::length(forward) > 0.0f ? glm::normalize(forward) : glm::vec3(0.0f);
	velocity = _turnVelocity;
}

void HandSystem::PushSeedThrowData(glm::vec3 velocity, glm::vec3 handPos) noexcept
{
	// the hand's throw data as it is: v, h (sent negated: the status holds this port's sign, as PredictRelease's L),
	// HandPos, and the held seed's YXZ angles (from the matrix the hand drew it with)
	float y = 0.0f;
	float x = 0.0f;
	float z = 0.0f;
	auto& registry = Locator::entitiesRegistry::value();
	if (_recordedHandPose)
	{
		// a hand demo's record, copied in this frame before its message
		handPos = _recordedHandPose->first;
		x = _recordedHandPose->second.x;
		y = _recordedHandPose->second.y;
		z = _recordedHandPose->second.z;
	}
	else if (_held && registry.Valid(*_held))
	{
		const auto* pose = registry.TryGet<const HandDrawPose>(*_held);
		affine::DecomposeYXZ(pose != nullptr ? pose->rotation : registry.Get<const Transform>(*_held).rotation, y, x, z);
	}
	const glm::vec3 h = -_handAngularVelocity;
	game_packets::Packet data {game_packets::Type::ThrowData};
	data.data = {velocity.x, velocity.y, velocity.z, h.x, h.y, h.z, handPos.x, handPos.y, handPos.z, x, y, z};
	game_packets::Push(data);
}

void HandSystem::LiveHandThrowData(glm::vec3& handPos, glm::vec3& velocity) const noexcept
{
	// the hand's velocity and HandPos: while holding, HandPos is the held object's drawn translation + 0.2 x the hold
	// spring's velocity; the hand's position without one
	auto& registry = Locator::entitiesRegistry::value();
	const auto& source = _held && registry.Valid(*_held) ? *_held : _hands[static_cast<size_t>(Side::Left)];
	// HandDrawPose is the matrix the hand drew the held object with this frame
	const auto* pose = registry.TryGet<const HandDrawPose>(source);
	handPos = pose != nullptr ? pose->position + 0.2f * _springVelocity : registry.Get<const Transform>(source).position;
	velocity = _handVelocity;
}

glm::mat4 HandSystem::GetHandMatrix() const noexcept
{
	const auto& hand = Locator::entitiesRegistry::value().Get<const Transform>(_hands[static_cast<size_t>(Side::Left)]);
	return glm::translate(glm::mat4(1.0f), hand.position) * glm::mat4(hand.rotation) * glm::scale(glm::mat4(1.0f), hand.scale);
}

void HandSystem::EndAction() noexcept
{
	// the state's exit, the action state reset, the message buffer emptied; with an object the hand holds that the
	// status's hand does not, RenderHandRelease (the grab abandoned)
	if (_renderHandHeld && !_held)
	{
		RenderHandRelease();
	}
	EndApplyOnRelease();
	// leaving a locked apply unlocks the held seed through the same packet as its release
	if ((_seedAction == SeedAction::LockedApplyMap || _seedAction == SeedAction::LockedApplyObject) && IsHoldingSeed())
	{
		SendSeedUnlock(*_held);
	}
	_seedAction = SeedAction::None;
	_applyObjectState = false;
	_releaseArmed = false;
	_applySentTurn = 0;
	_applyHandledTurn = 0;
	// the objects under the hand go
	_collideObject.reset();
	_renderCollideObject.reset();
}

void HandSystem::ForceDropHeld() noexcept
{
	// a ThrowData packet with zero velocity and the held object's matrix position and YXZ angles, then a ThrowHeld with
	// no fields; the next turn applies them (ApplyForceDropHeld)
	if (!_held)
	{
		// no held object: the status's throw data is sent again with the velocity and the angular part zeroed: HandPos
		// and the angles survive
		const auto& p = _statusThrowHandPosition;
		const auto& a = _statusThrowAngles;
		game_packets::Packet data {game_packets::Type::ThrowData};
		data.data = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, p.x, p.y, p.z, a.x, a.y, a.z};
		game_packets::Push(data);
	}
	else
	{
		PushThrowData(glm::vec3(0.0f), false);
	}
	game_packets::Push({game_packets::Type::ThrowHeld});
}

void HandSystem::ApplyForceDropHeld() noexcept
{
	// the ThrowHeld packet: the hand holds an available object, then its ThrowObjectFromHand(status, forced) and
	// HandleSeedApplyResult(result, held)
	auto& registry = Locator::entitiesRegistry::value();
	if (!_held || !ecs::IsAvailable(*_held))
	{
		return;
	}
	const auto entity = *_held;
	if (!IsHoldingSeed())
	{
		// the throw from the hand with the ThrowData's zero velocity and pose: it falls (from_hand::Throw with
		// dont_replant 1, as from_hand::ForceDrop)
		HandleApplyResult(ThrowObjectFromHand(_statusThrowVelocity, true, true), entity, std::nullopt);
		return;
	}
	// a seed forced out of the hand: the charge goes back to the icon's worship site (Worship/Worship.cpp), and the
	// seed is deleted either way (3)
	if (SeedTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand seed: {} shaken out of the hand", static_cast<uint32_t>(entity));
	}
	SeedLeftHand(entity);
	_held.reset();
	_seedAction = SeedAction::None;
	worship::ReturnSeedToItsSite(entity); // deleted, with the refund
	registry.SetDirty();
}

int HandSystem::FailApply(glm::vec3 point) noexcept
{
	// a SpotVisual packet (point, 4): the fail spot visual at the next turn's start; the sample
	// LH_SAMPLE_G_SPELLCASTFAILURE at once
	game_packets::Push({game_packets::Type::SpotVisual, entt::null, point, k_SpotVisualFailCast});
	PlaySample(audio::SoundId::G_SpellCastFailure);
	if (SeedTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand seed: FailApply at ({:.1f}, {:.1f}, {:.1f})", point.x, point.y, point.z);
	}
	return 1;
}

void HandSystem::BeginApplyOnRelease(SeedAction state) noexcept
{
	// the gesture buffer cleared and the mouse sampled again, the state, and a gesture sound
	// (LH_SAMPLE_G_HANDGESTURE_02) looping while the button is down
	gestures::ReseedBuffer();
	_seedAction = state;
	_seedTarget = _cursorObject.value_or(entt::null);
	// a sound tag of the interface (track 3, mode 2, loops -1, 3D, IN_GAME), replayed every turn (mode 2: nothing while
	// the loop plays). (approximate) the hand's entity stands for the interface: its Transform for the interface's 3D
	// sound position (the newest point of the mouse sample buffer)
	audio::tags::Create(_hands[static_cast<size_t>(Side::Left)], 3, false, 2, -1, false, true, audio::SfxBank::InGame, 0);
	if (SeedTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand seed: armed (state {})", static_cast<int>(state));
	}
}

void HandSystem::EndApplyOnRelease() noexcept
{
	// every such tag deleted, its playing loop released (it ends with its pass)
	audio::tags::Remove(_hands[static_cast<size_t>(Side::Left)], 3, audio::SfxBank::InGame);
}

ecs::hand_seed::Step HandSystem::SeedActionPressed() noexcept
{
	const auto seed = *_held;
	const auto target = _cursorObject && ecs::IsAvailable(*_cursorObject) ? *_cursorObject : entt::null;
	const bool inInfluence = gestures::GetHandStatus().inInfluence;
	// the creature to give to, then the object under the hand; any object is a valid interface target. Giving to a
	// creature needs a creature. Interaction must be in the player's influence.
	if (target != entt::null && inInfluence && ValidToApplyThisToObject(seed, target))
	{
		// a seed return point (worship sites and dispensers)
		if (ApplyOnlyAfterRecSystem(seed))
		{
			BeginApplyOnRelease(SeedAction::ApplyOnReleaseObject);
			return hand_seed::Step::Armed;
		}
		_seedTarget = target;
		if (ValidForLockedApplyProcess(seed))
		{
			_seedAction = SeedAction::LockedApplyObject;
			_seedLockTurn = game_clock::Turn();
		}
		return SendSeedApplyToObject();
	}
	// no target: the land under the hand, which must be in the player's influence
	if (!inInfluence)
	{
		return hand_seed::Step::Nothing;
	}
	// (inferred) the original always has the hand's map point; with no point under the hand openblack tries (0, 0, 0)
	const auto point = _interactionPoint.value_or(glm::vec3(0.0f));
	const auto position = magic::ToMap(point);
	if (ApplyOnlyAfterRecSystem(seed))
	{
		if (magic::TheWorld().InBounds(position) && ValidToApplyThisToMapCoord(seed, position))
		{
			BeginApplyOnRelease(SeedAction::ApplyOnReleaseMap);
			return hand_seed::Step::Armed;
		}
		FailApply(point);
		return hand_seed::Step::Failed;
	}
	// the drop on the map point (a seed is not applied only after release)
	if (!ValidToApplyThisToMapCoord(seed, position))
	{
		FailApply(point);
		return hand_seed::Step::Failed;
	}
	if (ValidForLockedApplyProcess(seed))
	{
		_seedAction = SeedAction::LockedApplyMap;
		_seedLockTurn = game_clock::Turn();
	}
	return SendSeedApplyToMapCoord();
}

ecs::hand_seed::Step HandSystem::SendSeedApplyToMapCoord() noexcept
{
	if (!IsHoldingSeed())
	{
		return hand_seed::Step::Nothing;
	}
	const auto turn = game_clock::Turn();
	if (_applySentTurn != 0)
	{
		return hand_seed::Step::Waiting; // an apply packet still waits for its handler
	}
	const auto seed = *_held;
	auto& state = gestures::State();
	glm::vec3 point = _interactionPoint.value_or(glm::vec3(0.0f));
	std::optional<glm::vec3> circleCoords;
	if (state.circlePending)
	{
		state.gesture.position = state.circlePosition;
		state.gesture.size = state.circleSize;
		state.gesture.gesture = state.circleGesture;
		if (SizingGesture(seed) != 0)
		{
			// map coords from the circle's position in metres, altitude 0
			const auto coords = map_coords::FromMetres(glm::vec2(state.circlePosition.x, state.circlePosition.z));
			circleCoords = glm::vec3(map_coords::ToMetres(coords.x), 0.0f, map_coords::ToMetres(coords.z));
			point = glm::vec3(state.circlePosition.x, 0.0f, state.circlePosition.z);
			point.y = magic::ToWorld(glm::vec3(point.x, 0.0f, point.z)).y;
		}
	}
	else
	{
		state.gesture.gesture = gestures::k_None;
	}
	const auto position = circleCoords.value_or(magic::ToMap(point));
	if (!magic::TheWorld().InBounds(position) || !ValidToApplyThisToMapCoord(seed, position) ||
	    !gestures::GetHandStatus().inInfluence)
	{
		FailApply(point);
		return hand_seed::Step::Failed;
	}
	if (!GestureAllowsCast(seed, state.gesture.gesture))
	{
		FailApply(point); // a storm or a shield without its circle
		return hand_seed::Step::Failed;
	}
	gestures::ClearBuffer();
	state.circlePending = false;
	// the power-up is shown while in the hand: a level being charged goes with the cast (help 0x17)
	if (const auto level = gestures::PowerUpLevelGesture(); level != gestures::k_None)
	{
		gestures::TrySetupPowerUpGestures();
		state.gesture.gesture = level;
	}
	// a ThrowData packet: the hand's throw data goes into the status (ThrowVelocity, HandPos)
	state.gesture.position = point;
	_applySentTurn = turn;
	// the apply to a map point: the ThrowData (the hand's throw block as it is), a Hand packet (the apply point and
	// HandPos; the last-sent values untouched) and the ApplyToMapCoord
	glm::vec3 handPos;
	glm::vec3 velocity;
	LiveHandThrowData(handPos, velocity);
	if (const char* test = std::getenv("OPENBLACK_TEST_THROW_VEL"); test != nullptr)
	{
		std::sscanf(test, "%f,%f,%f", &velocity.x, &velocity.y, &velocity.z);
	}
	PushSeedThrowData(velocity, handPos);
	game_packets::Packet hand {game_packets::Type::Hand};
	hand.coords = map_coords::FromWorld(point);
	hand.data[0] = handPos.x;
	hand.data[1] = handPos.y;
	hand.data[2] = handPos.z;
	hand.value = NextPingIndex();
	game_packets::Push(hand);
	// the ApplyToMapCoord packet with the gesture (type, size, position)
	game_packets::Packet apply {game_packets::Type::ApplyToMapCoord, entt::null, position, state.gesture.gesture};
	apply.data = {state.gesture.size, point.x, point.y, point.z};
	game_packets::Push(apply);
	return hand_seed::Step::Sent;
}

void HandSystem::ApplySeedToMapCoord(const game_packets::Packet& packet) noexcept
{
	// the ApplyToMapCoord packet: the held seed is available, ValidToApplyThisToMapCoord, then the cast
	const auto seed = *_held;
	const auto position = packet.position;
	const float size = packet.data[0];
	const glm::vec3 point(packet.data[1], packet.data[2], packet.data[3]);
	// the spell info's status fields (the synced map point and camera), the ThrowData's hand and velocity
	glm::vec3 interfacePos;
	glm::vec3 turnHand;
	glm::vec3 cameraForward;
	glm::vec3 turnVelocity;
	GetSpellInfo(interfacePos, turnHand, cameraForward, turnVelocity);
	const glm::vec3 handPos = _statusThrowHandPosition;
	const glm::vec3 velocity = _statusThrowVelocity;
	int result = 0;
	if (ValidToApplyThisToMapCoord(seed, position) && magic::seed::CanCast(seed, position))
	{
		psys::ProcessInfo handInfo {
		    .interfacePos = interfacePos,
		    .handPos = handPos,
		    .cameraForward = cameraForward,
		    .direction = velocity, // the status's ThrowVelocity
		};
		entt::entity spell = entt::null;
		if (magic::seed::Cast(seed, position, &spell, size, handInfo) != 0)
		{
			if (spell != entt::null && IsSpellKeptInHand(seed))
			{
				result = 1;
			}
			else
			{
				auto& registry = Locator::entitiesRegistry::value();
				const auto at = spell != entt::null && registry.Valid(spell)
				                    ? magic::ToWorld(registry.Get<const ecs::components::Spell>(spell).position)
				                    : point;
				// the success spot visual: the original's 1.0 is the effect's strength, the duration is the entry's
				// own, which 0 seconds selects here
				Locator::particleSystem::value().StartSpotVisual(k_SpotVisualSucceedCast, at, std::nullopt, spell);
				result = k_ResultRemoved;
			}
			if (SeedTrace())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"),
				                   "Hand seed: cast {} at ({:.1f}, {:.1f}) gesture {} size {:.1f} velocity ({:.1f}, {:.1f}, "
				                   "{:.1f}) -> spell {}",
				                   InfoOf(seed).debugString.data(), position.x, position.z, packet.value, size, velocity.x,
				                   velocity.y, velocity.z, spell == entt::null ? -1 : static_cast<int>(spell));
			}
		}
	}
	HandleSeedApplyResult(result, seed);
}

ecs::hand_seed::Step HandSystem::SendSeedApplyToObject() noexcept
{
	if (!IsHoldingSeed())
	{
		return hand_seed::Step::Nothing;
	}
	const auto seed = *_held;
	auto& registry = Locator::entitiesRegistry::value();
	const auto target = _cursorObject && ecs::IsAvailable(*_cursorObject) ? *_cursorObject : entt::null;
	if (target == entt::null || !ValidToApplyThisToObject(seed, target))
	{
		return hand_seed::Step::Nothing;
	}
	// the apply to an object for every seed target, a return point too (its branch is the handler's): the sent turn and
	// GestureAllowsCast at the send
	const auto turn = game_clock::Turn();
	auto& state = gestures::State();
	if (_applySentTurn != 0)
	{
		return hand_seed::Step::Waiting; // an apply packet still waits for its handler
	}
	if (!GestureAllowsCast(seed, state.gesture.gesture))
	{
		FailApply(registry.Get<const Transform>(target).position);
		return hand_seed::Step::Failed;
	}
	gestures::ClearBuffer();
	if (const auto level = gestures::PowerUpLevelGesture(); level != gestures::k_None)
	{
		gestures::TrySetupPowerUpGestures();
		state.gesture.gesture = level;
	}
	_applySentTurn = turn;
	// a ThrowData packet (the hand's velocity and position into the status; (approximate) the live hand), then
	// the ApplyToObject with the gesture
	glm::vec3 handPos;
	glm::vec3 velocity;
	LiveHandThrowData(handPos, velocity);
	PushSeedThrowData(velocity, handPos);
	const auto at = registry.Get<const Transform>(target).position;
	game_packets::Packet apply {game_packets::Type::ApplyToObject, target, at, state.gesture.gesture};
	apply.data[0] = state.gesture.size;
	game_packets::Push(apply);
	return hand_seed::Step::Sent;
}

void HandSystem::ApplySeedToObject(const game_packets::Packet& packet) noexcept
{
	// the ApplyToObject packet: the target interactable, the hand holding, ValidToApplyThisToObject again
	auto& registry = Locator::entitiesRegistry::value();
	const auto seed = *_held;
	const auto target = packet.object;
	// the target and the held seed both interactable
	if (!Interactable(target) || !Interactable(seed) || !ValidToApplyThisToObject(seed, target))
	{
		return;
	}
	// the help event after the apply: 7 for a sacrifice altar, else 6, for the local interface
	const auto helpEvent = ecs::held_apply::ApplyHelpEvent(ecs::held_apply::IsSacrificeAltar(target));
	// the apply's first branches, before the gesture and the cast: a spell dispenser, a WorshipTotem or a spell icon
	// takes the seed back (Worship/Worship.cpp). The original deletes the seed later, so its InterfaceSetOutMagicHand
	// still sees it: the hand lets go first here.
	if (worship::IsSeedReturnPoint(target, SeedOf(seed).creator.player))
	{
		if (_held && *_held == seed)
		{
			SeedLeftHand(seed);
			_held.reset();
			// the taken seed is no longer in the hand, so the drawn hand lets go of it too and opens in the same turn,
			// before an icon of another miracle puts its own charged seed in the hand
			RenderHandRelease();
		}
		_seedAction = SeedAction::None;
		EndApplyOnRelease();
		worship::ApplySeedToObject(seed, target);
		help_profile::Trigger(helpEvent);
		return;
	}
	// a magic fireball a FIRE seed takes in: the seed's power grows by the ball's catch factor, the ball goes, and the
	// seed stays in the hand (1)
	if (const bool fireBall = registry.AllOf<MagicFireBall>(target);
	    fireBall && magic::seed::ValidToApplyToMagicFireBall(SeedOf(seed).seedType, fireBall))
	{
		magic::fireball::DeleteAndPutIntoSpellSeed(target, seed);
		help_profile::Trigger(help_profile::Event::Supply); // after the apply
		HandleSeedApplyResult(1, seed);
		return;
	}
	// else the object cast (the keep / remove rule of the map point apply)
	int result = 0;
	const float size = packet.data[0];
	glm::vec3 interfacePos;
	glm::vec3 turnHand;
	glm::vec3 cameraForward;
	glm::vec3 turnVelocity;
	GetSpellInfo(interfacePos, turnHand, cameraForward, turnVelocity);
	const glm::vec3 handPos = _statusThrowHandPosition;
	const glm::vec3 velocity = _statusThrowVelocity;
	psys::ProcessInfo handInfo {
	    .interfacePos = interfacePos,
	    .handPos = handPos,
	    .cameraForward = cameraForward,
	    .direction = velocity,
	};
	// the object cast: on the object itself, as the original's apply to an object casts
	entt::entity spell = entt::null;
	if (magic::seed::CastOnObject(seed, target, &spell, size, handInfo) != 0)
	{
		if (spell != entt::null && IsSpellKeptInHand(seed))
		{
			result = 1;
		}
		else
		{
			// the success spot visual at the object's position, as on the land
			const auto at = registry.Get<const Transform>(target).position;
			Locator::particleSystem::value().StartSpotVisual(k_SpotVisualSucceedCast, at, std::nullopt, spell);
			result = k_ResultRemoved;
		}
	}
	help_profile::Trigger(helpEvent); // after the apply
	HandleSeedApplyResult(result, seed);
}

void HandSystem::HandleSeedApplyResult(int result, entt::entity seed) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	if (result == k_ResultNothing || result == 0 || result == 1)
	{
		return;
	}
	if (result == k_ResultConsumed || !registry.Valid(seed))
	{
		if (_held && *_held == seed)
		{
			SeedLeftHand(seed);
			_held.reset();
			// a seed used up or deleted by its cast leaves the hand in the same turn: the drawn hand lets go and opens
			RenderHandRelease();
		}
		_seedAction = SeedAction::None;
		return;
	}
	if (hand_seed::HandLetsGoAfterApply(result))
	{
		// the seed removed from the hand (InterfaceSetOutMagicHand), the hand lets go
		if (_held && *_held == seed)
		{
			SeedLeftHand(seed);
			_held.reset();
			// with nothing left in it, the drawn hand lets go of the seed too and opens, as after any throw. A seed
			// that follows its spell (a forest's) stays alive, so it would otherwise stay held, the hand closed on it
			RenderHandRelease();
		}
		_seedAction = SeedAction::None;
		EndApplyOnRelease();
	}
}

void HandSystem::UpdateSeedAction(bool actionHeld) noexcept
{
	if (!SeedActionGoesOn())
	{
		return;
	}
	if (!actionHeld)
	{
		SeedActionReleased();
		return;
	}
	switch (_seedAction)
	{
	case SeedAction::ApplyOnReleaseMap:
	case SeedAction::ApplyOnReleaseObject:
		// armed: the apply waits for the release
		break;
	case SeedAction::LockedApplyMap:
	{
		// the apply again every tick (one packet per turn); on the land only when the point is InBounds and
		// ValidToApplyThisToMapCoord, with no FailApply otherwise
		// (inferred) (0, 0, 0) with no point under the hand, as in SeedActionPressed
		const auto position = magic::ToMap(_interactionPoint.value_or(glm::vec3(0.0f)));
		if (magic::TheWorld().InBounds(position) && ValidToApplyThisToMapCoord(*_held, position))
		{
			SendSeedApplyToMapCoord();
		}
		break;
	}
	case SeedAction::LockedApplyObject:
		SendSeedApplyToObject();
		break;
	case SeedAction::None:
		break;
	}
}

bool HandSystem::SeedActionGoesOn() noexcept
{
	if (_seedAction == SeedAction::None)
	{
		return false;
	}
	if (!IsHoldingSeed())
	{
		EndApplyOnRelease();
		_seedAction = SeedAction::None;
		return false;
	}
	return true;
}

ecs::hand_seed::Step HandSystem::SeedActionReleased() noexcept
{
	const auto seed = *_held;
	switch (_seedAction)
	{
	case SeedAction::ApplyOnReleaseMap:
	case SeedAction::ApplyOnReleaseObject:
	{
		// on the release the loop stops and the apply goes
		const auto state = _seedAction;
		_seedAction = SeedAction::None;
		EndApplyOnRelease();
		return state == SeedAction::ApplyOnReleaseMap ? SendSeedApplyToMapCoord() : SendSeedApplyToObject();
	}
	case SeedAction::LockedApplyMap:
	case SeedAction::LockedApplyObject:
		// the unlock goes as a packet after the applies already sent, so that the last of them (a click's only
		// one) is cast before the unlock closes its spell (ApplyUnlock's handler)
		_seedAction = SeedAction::None;
		SendSeedUnlock(seed);
		return hand_seed::Step::Unlocked;
	case SeedAction::None:
		break;
	}
	return hand_seed::Step::Nothing;
}

MagicSystemInterface::HandResult HandSystem::PressSeedAction() noexcept
{
	// the press the hand's Update gives a held seed (its press chain's seed branch, which a creature held or being let
	// go keeps from the hand), unless the action is down already. The press is taken here, so Update does not press
	// again; it counts the action as held until ReleaseSeedAction
	if (!IsHoldingSeed() || _creatureLock.has_value() || _actionWasHeld)
	{
		return MagicSystemInterface::HandResult::None;
	}
	const bool ready = SeedOf(*_held).ready;
	_drivenActionHeld = true;
	_actionWasHeld = true;
	const auto step = SeedActionPressed();
	const bool locked = _seedAction == SeedAction::LockedApplyMap || _seedAction == SeedAction::LockedApplyObject;
	return hand_seed::ResultOf(step, ready, locked);
}

MagicSystemInterface::HandResult HandSystem::ReleaseSeedAction() noexcept
{
	// the release step the hand's Update takes when the action comes up; the next Update sees the release itself
	_drivenActionHeld = false;
	if (!SeedActionGoesOn())
	{
		return MagicSystemInterface::HandResult::None;
	}
	const bool ready = SeedOf(*_held).ready;
	// the release leaves no action in progress, so a sent apply is a cast, not a held one
	return hand_seed::ResultOf(SeedActionReleased(), ready, false);
}

void HandSystem::SendSeedUnlock(entt::entity seed) noexcept
{
	game_packets::Push(hand_seed::UnlockPacket(seed, _seedLockTurn, game_clock::Turn()));
}

void HandSystem::SeedLeftHand(entt::entity seed) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(seed) && registry.AllOf<SpellSeed>(seed))
	{
		// the status's last seed type and the icon's cancelled charge (Worship/Worship.cpp keeps the player's own
		// copy of the last type, for the R gesture)
		gestures::State().lastSeedType = static_cast<int>(SeedOf(seed).seedType);
		worship::OnSeedOutOfHand(seed, SeedOf(seed).creator.player);
		// out of the hand the seed is drawn only while it follows its spell (a forest seed cast from an icon):
		// seed::DrawSpells (Magic/Core/SpellSeed.cpp) puts the mesh back over the spell in the same frame
		ShowSeedMesh(seed, false);
	}
	// the local hand: the in-hand effect released, the power-up level 0, the tribal power ring stopped
	if (Locator::miracleFxSystem::has_value())
	{
		Locator::miracleFxSystem::value().SeedLeftHand();
		Locator::miracleFxSystem::value().StopTribalPowerRing();
	}
	gestures::State().showHeldGestureTrail = false;
	EndApplyOnRelease();
	_seedAction = SeedAction::None;
	if (_seedInHand == seed)
	{
		_seedInHand = entt::null;
	}
}

void HandSystem::UpdateSeedInHand(bool actionHeld) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto held = IsHoldingSeed() ? *_held : entt::null;
	if (held != _seedInHand)
	{
		if (_seedInHand != entt::null)
		{
			SeedLeftHand(_seedInHand);
		}
		// (the in-hand effect comes with the seed's power-up, from InterfaceSetInMagicHand)
		_seedInHand = held;
	}
	if (held != entt::null)
	{
		ComputeHoldParameters(held);
		ShowSeedMesh(held, IsG3DObjectDrawnInHand(held));
	}
	// the action point, the map's corner when it is cleared
	const auto actionPoint = _interactionPoint.value_or(glm::vec3(0.0f));
	gestures::HandStatus status {
	    .heldSeed = held,
	    .holdingSomething = _held.has_value(),
	    .validToShake = _held.has_value(), // any object can be shaken from the hand (a seed too)
	    .handReady = IsHandReadyForObject(),
	    // in influence: the hand in the player's influence at the action position, measured across the land (its x
	    // and z as they are)
	    .inInfluence =
	        magic::TheWorld().HandInInfluence(PlayerNames::PLAYER_ONE, glm::vec3(actionPoint.x, 0.0f, actionPoint.z)),
	    .actionLatched = actionHeld && held != entt::null,
	    .paused = Locator::time::has_value() && game_clock::IsPaused(),
	    .mouse = input::GameCursor(),
	    .cursorObject = _cursorObject && ecs::IsAvailable(*_cursorObject) ? *_cursorObject : entt::null,
	    .forceDropHeld = [this]() { ForceDropHeld(); },
	    .removeFromHandFx =
	        []() {
		        if (Locator::miracleFxSystem::has_value())
		        {
			        Locator::miracleFxSystem::value().SeedShakenOff();
		        }
	        },
	    // the hand's own apply to an object, as the holding press sends it. Sent, or with an apply still waiting, it leaves
	    // the action state that takes no press, in place of the seed's action (whose end does not run)
	    .applyHeldToCursorObject =
	        [this]() {
		        if (const auto step = SendSeedApplyToObject();
		            step == hand_seed::Step::Sent || step == hand_seed::Step::Waiting)
		        {
			        _seedAction = SeedAction::None;
			        _applyObjectState = true;
		        }
	        },
	};
	gestures::SetHandStatus(std::move(status));
	(void)registry;
}

bool HandSystem::TestCastActionHeld(float seconds, bool actionHeld) noexcept
{
	auto& events = TestCastEvents();
	if (events.empty() || !Locator::terrainSystem::has_value())
	{
		return actionHeld;
	}
	if (_testCastTime < 0.0f)
	{
		_testCastTime = 0.0f;
	}
	else
	{
		_testCastTime += seconds;
	}
	bool held = false;
	for (auto& event : events)
	{
		if (event.time > _testCastTime)
		{
			continue;
		}
		if (event.kind == TestCastEvent::Kind::Shot)
		{
			const char* path = std::getenv("OPENBLACK_TEST_SHOT_PATH");
			if (!event.done && path != nullptr && Locator::screenshotRequest::has_value())
			{
				// several shots in one run: a "{}" in the path becomes the shot's time in tenths of a second
				std::string file(path);
				if (const auto mark = file.find("{}"); mark != std::string::npos)
				{
					file.replace(mark, 2, std::to_string(static_cast<int>(event.time * 10.0f + 0.5f)));
				}
				Locator::screenshotRequest::value().Request(file);
			}
			event.done = true;
			continue;
		}
		held = event.kind == TestCastEvent::Kind::Press;
	}
	return actionHeld || held;
}

std::optional<glm::vec3> HandSystem::TestCastPathPoint() const noexcept
{
	// OPENBLACK_TEST_CAST_PATH="x0,z0,x1,z1": during the first press of OPENBLACK_TEST_CAST the hand goes along that line
	static const auto path = []() -> std::optional<glm::vec4> {
		const char* value = std::getenv("OPENBLACK_TEST_CAST_PATH");
		glm::vec4 line;
		if (value == nullptr || std::sscanf(value, "%f,%f,%f,%f", &line.x, &line.y, &line.z, &line.w) != 4)
		{
			return std::nullopt;
		}
		return line;
	}();
	const auto& events = TestCastEvents();
	if (!path || events.size() < 2 || _testCastTime < 0.0f || !Locator::terrainSystem::has_value())
	{
		return std::nullopt;
	}
	const float t0 = events[0].time;
	const float t1 = events[1].time;
	if (_testCastTime < t0)
	{
		return std::nullopt;
	}
	const float t = t1 > t0 ? std::clamp((_testCastTime - t0) / (t1 - t0), 0.0f, 1.0f) : 1.0f;
	const glm::vec2 xz = glm::mix(glm::vec2(path->x, path->y), glm::vec2(path->z, path->w), t);
	return glm::vec3(xz.x, magic::TheWorld().LandHeight(xz), xz.y);
}
