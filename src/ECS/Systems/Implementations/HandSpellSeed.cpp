/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A spell seed in the player's hand: the interface's seed branches (ActionPressedHolding 0x5D1560, the apply states
// 8..11, SendApplyToMapCoord 0x5D3340, SendApplyToObject 0x5D30D0, FailApply 0x5D18F0, ForceDropHeld 0x5D4350) and the
// seed's interface virtuals (SpellSeed 0x728580..0x72ACD0). Wiki: docs/bw1-notes/magic.md, "Lanzar desde la mano".

#define LOCATOR_IMPLEMENTATIONS

#include "HandSystem.h"
#include "HandSystemDetail.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <string>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Influence/Influence.h"
#include "ECS/MapCoords.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "Game.h"
#include "GameClock.h"
#include "Input/GamePackets.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/CastRules.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellSeed.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Magic/Hand/HandMagicFX.h"
#include "Magic/MagicTables.h"
#include "PSys/PSysManager.h"
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

/// ApplyThisTo* results (HandleApplyResult fn_005DA100)
constexpr int k_ResultConsumed = 3;
constexpr int k_ResultNothing = 5;
constexpr int k_ResultRemoved = 0x16;
constexpr int k_ResultPlaced = 0x17;

/// SPOT_VISUAL_SUCEED_CAST / SPOT_VISUAL_FAIL_CAST (GSpotVisualInfo 0xD44470)
constexpr int k_SpotVisualSucceedCast = 3;
constexpr int k_SpotVisualFailCast = 4;

bool SeedTrace()
{
	static const bool trace = std::getenv("OPENBLACK_SPELL_TRACE") != nullptr || std::getenv("OPENBLACK_GESTURE_TRACE") != nullptr;
	return trace;
}

SpellSeed& SeedOf(entt::entity seed)
{
	return Locator::entitiesRegistry::value().Get<SpellSeed>(seed);
}

const GSpellSeedInfo& InfoOf(entt::entity seed)
{
	return magic::seed::InfoOf(SeedOf(seed));
}

/// SpellSeed::ApplyOnlyAfterRecSystem 0x7286B0: castType HAND_GESTURE
bool ApplyOnlyAfterRecSystem(entt::entity seed)
{
	return InfoOf(seed).castType == SpellCastType::SpellCastHandGesture;
}

/// SpellSeed::ValidForLockedApplyProcess 0x728750 = IsSpellCastInHand 0x729820: castType IN_HAND
bool ValidForLockedApplyProcess(entt::entity seed)
{
	return InfoOf(seed).castType == SpellCastType::SpellCastInHand;
}

/// SpellSeed::IsSpellKeptInHand 0x729840
bool IsSpellKeptInHand(entt::entity seed)
{
	return InfoOf(seed).isKeptInHand != 0;
}

/// fn_00729AC0: the sizing gesture (CIRCLE for storm and the shields)
uint32_t SizingGesture(entt::entity seed)
{
	return static_cast<uint32_t>(InfoOf(seed).sizingGesture);
}

/// fn_00729AF0: a seed with a sizing gesture needs that gesture in the packet
bool GestureAllowsCast(entt::entity seed, gestures::Gesture gesture)
{
	const auto sizing = SizingGesture(seed);
	return sizing == 0 || (gesture & 0xFFu) == sizing;
}

/// SpellSeed::ValidToApplyThisToMapCoord 0x728720: ready, and CanCast there (the cast rule and the class check)
bool ValidToApplyThisToMapCoord(entt::entity seed, const glm::vec3& mapPosition)
{
	return SeedOf(seed).ready && magic::seed::CanCast(seed, mapPosition);
}

/// SpellSeed::CanCast(Object, GPlayer) 0x729190: a return point of the seed's player (a spell dispenser fn_00728C50 or
/// a worship site fn_00728A50), a MagicFireBall (ValidToApplyToMagicFireBall 0x728A20), else a seed with
/// castOnObject, the cast rule at the object and the class's object check (vt 0x2C).
/// TODO(magic): the MagicFireBall target (ValidToApplyToMagicFireBall 0x728A20) is not ported
bool CanCastOnObject(entt::entity seed, entt::entity object)
{
	const auto& component = SeedOf(seed);
	if (worship::IsSeedReturnPoint(object, component.creator.player))
	{
		return true; // Worship/Worship.cpp: a dispenser, a WorshipTotem or a spell icon always takes the seed back
	}
	const auto& info = magic::seed::InfoOf(component);
	if (info.castOnObject == 0)
	{
		return false;
	}
	auto& registry = Locator::entitiesRegistry::value();
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

/// SpellSeed::ValidToApplyThisToObject 0x7286D0: ready, the target not highlighted by the script, CanCast(object).
/// (aproximado) the highlight test (vt 0x48C IsScriptHighlight, 0 for GameThingWithPos 0x4023A0) is not ported
bool ValidToApplyThisToObject(entt::entity seed, entt::entity target)
{
	return SeedOf(seed).ready && CanCastOnObject(seed, target);
}

/// SpellSeed::IsG3DObjectDrawnInHand 0x728600: GMagicInfo.isSpellSeedDrawnInHand == 1 (the fire, lightning, heal and
/// storm seeds are only the in-hand effect)
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
		registry.Assign<Mesh>(seed, resources::HashIdentifier(InfoOf(seed).mesh), static_cast<int8_t>(0), static_cast<int8_t>(0));
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
std::vector<TestCastEvent>& TestCastEvents()
{
	static auto events = [] {
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
			char kind[16] = {};
			float time = 0.0f;
			if (std::sscanf(item.c_str(), "%15[^@]@%f", kind, &time) == 2)
			{
				const auto type = std::strcmp(kind, "press") == 0  ? TestCastEvent::Kind::Press
				                  : std::strcmp(kind, "shot") == 0 ? TestCastEvent::Kind::Shot
				                                                   : TestCastEvent::Kind::Release;
				list.push_back({type, time});
			}
			start = end + 1;
		}
		return list;
	}();
	return events;
}
} // namespace

bool HandSystem::IsHoldingSeed() const noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	return _held && ecs::IsAvailable(*_held) && registry.AllOf<SpellSeed>(*_held);
}

bool HandSystem::IsHandReadyForObject() const noexcept
{
	// IsSpaceInHands and no locked hand (status +0x24 & 0x20: a tug or a locked select here)
	return !_held && !_tug && !_pickSource;
}

void HandSystem::GetSpellInfo(glm::vec3& interfacePos, glm::vec3& handPos, glm::vec3& cameraForward,
                              glm::vec3& velocity) const noexcept
{
	// GInterfaceStatus::UpdateSpellInfo 0x5DC8F0, from the synced status (packets 0x15 / 0x16 / 0x17):
	// +0x00 MapCoords(+0x14) as a world point (0x5DC915..0x5DC945), +0x0C the hand +0xC8 (0x5DC8FC), +0x18
	// normalize(+0xBC - +0xB0) (0x5DC948), +0x24 the velocity +0x10C (0x5DC9F5)
	interfacePos = SyncMapPoint();
	handPos = _turnHand;
	const glm::vec3 forward = _syncCameraFocus - _syncCameraPosition;
	cameraForward = glm::length(forward) > 0.0f ? glm::normalize(forward) : glm::vec3(0.0f);
	velocity = _turnVelocity;
}

void HandSystem::LiveHandThrowData(glm::vec3& handPos, glm::vec3& velocity) const noexcept
{
	// CHand's throw block +0x48C8 (velocity) and +0x48E0 (HandPos): in the holding state ObtainRequiredHandPosition
	// writes +0x48E0 as an LH3DObject's translation + 0.2 state +0x108 (0x5B553F..0x5B55A1), (inferred) the held
	// object's (ebx = held +0x40 at 0x5B3CBA). (approximate) the held object's Transform position, the hand's without one
	auto& registry = Locator::entitiesRegistry::value();
	const auto& source = _held && registry.Valid(*_held) ? *_held : _hands[static_cast<size_t>(Side::Left)];
	handPos = registry.Get<const Transform>(source).position;
	velocity = _handVelocity;
}

glm::mat4 HandSystem::GetHandMatrix() const noexcept
{
	const auto& hand = Locator::entitiesRegistry::value().Get<const Transform>(_hands[static_cast<size_t>(Side::Left)]);
	return glm::translate(glm::mat4(1.0f), hand.position) * glm::mat4(hand.rotation) * glm::scale(glm::mat4(1.0f), hand.scale);
}

void HandSystem::EndAction() noexcept
{
	// fn_005D1260: the state's exit, ResetActionState, the message buffer emptied
	EndApplyOnRelease();
	_seedAction = SeedAction::None;
	_releaseArmed = false;
}

void HandSystem::ForceDropHeld() noexcept
{
	// GInterface::ForceDropHeld 0x5D4350: packet 0x4D with zero velocity (0x5D4361..0x5D4389) and the held object's
	// matrix position and YXZ angles (0x5D4393..0x5D43C3), then 0x1D with no fields (0x5D442C); the next turn applies
	// them (ApplyForceDropHeld)
	if (!_held)
	{
		// 0x5D4391 -> 0x5D43CA: no held object, the status's +0x44.. copied as they are
		game_packets::Packet data {game_packets::Type::ThrowData};
		data.data = {_statusThrowVelocity.x, _statusThrowVelocity.y, _statusThrowVelocity.z, 0.0f, 0.0f, 0.0f,
		             _statusThrowHandPosition.x, _statusThrowHandPosition.y, _statusThrowHandPosition.z};
		data.rotation = _statusThrowRotation;
		game_packets::Push(data);
	}
	else
	{
		PushThrowData(glm::vec3(0.0f));
	}
	game_packets::Push({game_packets::Type::ThrowHeld});
}

void HandSystem::ApplyForceDropHeld() noexcept
{
	// 0x5DA8F0 (packet 0x1D): the hand holds (+0x90), its first object IsAvailable, then its ThrowObjectFromHand(status,
	// 1) (vt 0x758); (pending) fn_005DA100 with the result when it is not 3
	auto& registry = Locator::entitiesRegistry::value();
	if (!_held || !ecs::IsAvailable(*_held)) // 0x5DA90F IsAvailable (vt 0x2C)
	{
		return;
	}
	const auto entity = *_held;
	if (!IsHoldingSeed())
	{
		// Object::ThrowObjectFromHand(status, 1) with the 0x4D's zero velocity and pose: it falls (from_hand::Throw with
		// dont_replant 1, as from_hand::ForceDrop)
		ThrowObjectFromHand(_statusThrowVelocity, true, true);
		return;
	}
	// SpellSeed::ThrowObjectFromHand 0x72ACD0 (forced) -> ApplyToWorshipSite 0x729A80: the charge goes back to the icon's
	// worship site (Worship/Worship.cpp), and the seed is deleted either way (3)
	if (SeedTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand seed: {} shaken out of the hand", static_cast<uint32_t>(entity));
	}
	SeedLeftHand(entity);
	_held.reset();
	_seedAction = SeedAction::None;
	worship::ReturnSeedToItsSite(entity); // its ToBeDeleted, with the refund
	registry.SetDirty();
}

int HandSystem::FailApply(glm::vec3 point) noexcept
{
	// packet 0x2B (point, 4) (0x5D1914..0x5D1928): the fail spot visual at the next turn's start; the sample
	// LH_SAMPLE_G_SPELLCASTFAILURE (0x25) at once
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
	// fn_005D2730: fn_005D2770 (the buffer cleared and the mouse sample again), m_Buttons |= 0x10, the state, and
	// SoundTag::Create(this, LH_SAMPLE_G_HANDGESTURE_02, 0, 2, -1, 0, 1, IN_GAME, 0): a loop while the button is down
	gestures::ReseedBuffer();
	_seedAction = state;
	_seedTarget = _cursorObject.value_or(entt::null);
	// 0x5D274D..0x5D275E: SoundTag::Create 0x71E840(GInterface, 3, track 0, mode 2, loops -1, +0x10 0, is3D 1, IN_GAME, delay
	// 0), a tag of the interface replayed every turn (mode 2: nothing while the loop plays). (aproximado) the hand's
	// entity stands for the GInterface: its Transform for GInterface::Get3DSoundPos 0x5CEC50 (the newest point of the
	// mouse sample buffer g_game+0x25006C, +0xC88 / +0xC90, the entry's +0x1C)
	audio::tags::Create(_hands[static_cast<size_t>(Side::Left)], 3, false, 2, -1, false, true, audio::SfxBank::InGame, 0);
	if (SeedTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand seed: armed (state {})", static_cast<int>(state));
	}
}

void HandSystem::EndApplyOnRelease() noexcept
{
	// 0x5D27C3..0x5D27C8: SoundTag::Remove 0x71EBE0(this, 3, IN_GAME): every such tag deleted, its playing loop released
	// (it ends with its pass, SoundTag::ToBeDeleted 0x71ECB0)
	audio::tags::Remove(_hands[static_cast<size_t>(Side::Left)], 3, audio::SfxBank::InGame);
}

void HandSystem::SeedActionPressed() noexcept
{
	const auto seed = *_held;
	const auto target = _cursorObject && ecs::IsAvailable(*_cursorObject) ? *_cursorObject : entt::null;
	const bool inInfluence = gestures::GetHandStatus().inInfluence;
	// GetCreatureToGiveTo, then m_ActionCollide.object; ValidAsInterfaceTarget (Object 0x402840 = 1). Giving to a
	// creature (InterfaceValidToGiveObject) needs a creature. InterfaceMustBeInInfluenceForInteraction is 1 (0x4028A0).
	if (target != entt::null && inInfluence && ValidToApplyThisToObject(seed, target))
	{
		// IsSpellSeedReturnPoint (Object 0x402730 = 0; worship sites and dispensers: M7)
		if (ApplyOnlyAfterRecSystem(seed))
		{
			BeginApplyOnRelease(SeedAction::ApplyOnReleaseObject);
			return;
		}
		_seedTarget = target;
		if (ValidForLockedApplyProcess(seed))
		{
			_seedAction = SeedAction::LockedApplyObject;
		}
		SendSeedApplyToObject();
		return;
	}
	// no target: the land under the hand, which must be in the player's influence
	if (!inInfluence)
	{
		return;
	}
	// (inferido) the original always has the hand's map point (GInterface +0x3F0); with no point under the hand
	// openblack tries (0, 0, 0)
	const auto point = _interactionPoint.value_or(glm::vec3(0.0f));
	const auto position = magic::ToMap(point);
	if (ApplyOnlyAfterRecSystem(seed))
	{
		if (magic::cast_rules::InBounds(position) && ValidToApplyThisToMapCoord(seed, position))
		{
			BeginApplyOnRelease(SeedAction::ApplyOnReleaseMap);
			return;
		}
		FailApply(point);
		return;
	}
	// DropOnMapCoord fn_005D1850 (ApplyOnlyAfterReleased is 0 for a seed)
	if (!ValidToApplyThisToMapCoord(seed, position))
	{
		FailApply(point);
		return;
	}
	if (ValidForLockedApplyProcess(seed))
	{
		_seedAction = SeedAction::LockedApplyMap; // m_LockTurn
	}
	SendSeedApplyToMapCoord();
}

int HandSystem::SendSeedApplyToMapCoord() noexcept
{
	if (!IsHoldingSeed())
	{
		return 0;
	}
	const auto turn = game_clock::Turn(); // g_game +0x205A40
	if (_applySentTurn && *_applySentTurn == turn)
	{
		return 1; // one apply packet per turn (m_ApplySentTurn)
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
			// MapCoords(ftol(circlePos.x * 6553.6), ftol(circlePos.z * 6553.6), altitude 0) (0x5D33DD..0x5D3400)
			const auto coords = ecs::map_coords::FromMetres(glm::vec2(state.circlePosition.x, state.circlePosition.z));
			circleCoords = glm::vec3(ecs::map_coords::ToMetres(coords.x), 0.0f, ecs::map_coords::ToMetres(coords.z));
			point = glm::vec3(state.circlePosition.x, 0.0f, state.circlePosition.z);
			point.y = magic::ToWorld(glm::vec3(point.x, 0.0f, point.z)).y;
		}
	}
	else
	{
		state.gesture.gesture = gestures::k_None;
	}
	const auto position = circleCoords.value_or(magic::ToMap(point));
	if (!magic::cast_rules::InBounds(position) || !ValidToApplyThisToMapCoord(seed, position) ||
	    !gestures::GetHandStatus().inInfluence)
	{
		return FailApply(point);
	}
	if (!GestureAllowsCast(seed, state.gesture.gesture))
	{
		return FailApply(point); // a storm or a shield without its circle
	}
	gestures::ClearBuffer();
	state.circlePending = false;
	// IsInterfacePowerUpWhenInHand (1): a level being charged goes with the cast (help 0x17)
	if (const auto level = gestures::PowerUpLevelGesture(); level != gestures::k_None)
	{
		gestures::SetupPowerUpGesturesInner();
		state.gesture.gesture = level;
	}
	// packet 0x4D: the hand's throw data (CHand +0x48C8) goes into the status (ThrowVelocity +0x44, HandPos +0x5C)
	state.gesture.position = point; // m_Gesture.SetPos
	_applySentTurn = turn;
	// SendApplyToMapCoord 0x5D3340: the 0x4D (0x5D35C8: CHand's throw block as it is), a 0x16 (0x5D360D: the apply
	// MapCoords and CHand+0x48E0 into +0x14 / +0xA4; fn_005D2250's last-sent values untouched) and the 0x12 (0x5D362D)
	glm::vec3 handPos;
	glm::vec3 velocity;
	LiveHandThrowData(handPos, velocity);
	if (const char* test = std::getenv("OPENBLACK_TEST_THROW_VEL"); test != nullptr)
	{
		std::sscanf(test, "%f,%f,%f", &velocity.x, &velocity.y, &velocity.z);
	}
	game_packets::Packet data {game_packets::Type::ThrowData};
	data.data = {velocity.x, velocity.y, velocity.z, 0.0f, 0.0f, 0.0f, handPos.x, handPos.y, handPos.z};
	game_packets::Push(data);
	game_packets::Packet hand {game_packets::Type::Hand};
	hand.coords = ecs::map_coords::FromWorld(point);
	hand.data[0] = handPos.x;
	hand.data[1] = handPos.y;
	hand.data[2] = handPos.z;
	game_packets::Push(hand);
	// packet 0x12 with the gesture (GestureSystemPacketData: type, size, position)
	game_packets::Packet apply {game_packets::Type::ApplyToMapCoord, entt::null, position, state.gesture.gesture};
	apply.data = {state.gesture.size, point.x, point.y, point.z};
	game_packets::Push(apply);
	return 1;
}

void HandSystem::ApplySeedToMapCoord(const game_packets::Packet& packet) noexcept
{
	// GInterface 0x5DA400 (packet 0x12): held->IsAvailable, ValidToApplyThisToMapCoord, ApplyThisToMapCoord 0x728E20
	const auto seed = *_held;
	const auto position = packet.position;
	const float size = packet.data[0];
	const glm::vec3 point(packet.data[1], packet.data[2], packet.data[3]);
	// UpdateSpellInfo's status fields (the synced +0x14 and camera), the 0x4D's hand and velocity
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
		psys::ProcessInfo handInfo;
		handInfo.interfacePos = interfacePos;
		handInfo.handPos = handPos;
		handInfo.cameraForward = cameraForward;
		handInfo.direction = velocity; // status +0x44 ThrowVelocity
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
				// CreateSpotVisual(pos, 3, 1.0, spell) 0x728E8B: the 1.0 is the effect's strength (PSys Start's
				// 1.0), the duration is the entry's own (+0x44, 0x63E55F), which 0 seconds selects here
				psys::manager::CreateSpotVisual(k_SpotVisualSucceedCast, at, 0.0f, spell);
				result = k_ResultRemoved;
			}
			if (SeedTrace())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"),
				                   "Hand seed: cast {} at ({:.1f}, {:.1f}) gesture {} size {:.1f} velocity ({:.1f}, {:.1f}, {:.1f}) -> spell {}",
				                   InfoOf(seed).debugString.data(), position.x, position.z, packet.value, size,
				                   velocity.x, velocity.y, velocity.z,
				                   spell == entt::null ? -1 : static_cast<int>(spell));
			}
		}
	}
	HandleSeedApplyResult(result, seed);
}

int HandSystem::SendSeedApplyToObject() noexcept
{
	if (!IsHoldingSeed())
	{
		return 0;
	}
	const auto seed = *_held;
	auto& registry = Locator::entitiesRegistry::value();
	const auto target = _cursorObject && ecs::IsAvailable(*_cursorObject) ? *_cursorObject : entt::null;
	if (target == entt::null || !ValidToApplyThisToObject(seed, target))
	{
		return 0;
	}
	// SendApplyToObject 0x5D30D0 for every seed target, a return point too (its branch is ApplyThisToObject's, in the
	// handler): +0x444 (m_ApplySentTurn) and fn_00729AF0 (GestureAllowsCast, 0x5D3162) at the send
	const auto turn = game_clock::Turn(); // g_game +0x205A40
	auto& state = gestures::State();
	if (!_applySentTurn || *_applySentTurn != turn)
	{
		if (!GestureAllowsCast(seed, state.gesture.gesture))
		{
			return FailApply(registry.Get<const Transform>(target).position);
		}
		gestures::ClearBuffer();
		if (const auto level = gestures::PowerUpLevelGesture(); level != gestures::k_None)
		{
			gestures::SetupPowerUpGesturesInner();
			state.gesture.gesture = level;
		}
		_applySentTurn = turn;
		// packet 0x4D (0x5D328D: the hand's velocity and position into the status; (approximate) the live hand), then
		// 0x11 with the gesture
		glm::vec3 handPos;
		glm::vec3 velocity;
		LiveHandThrowData(handPos, velocity);
		game_packets::Packet data {game_packets::Type::ThrowData};
		data.data = {velocity.x, velocity.y, velocity.z, 0.0f, 0.0f, 0.0f, handPos.x, handPos.y, handPos.z};
		game_packets::Push(data);
		const auto at = registry.Get<const Transform>(target).position;
		game_packets::Packet apply {game_packets::Type::ApplyToObject, target, at, state.gesture.gesture};
		apply.data[0] = state.gesture.size;
		game_packets::Push(apply);
	}
	return 1;
}

void HandSystem::ApplySeedToObject(const game_packets::Packet& packet) noexcept
{
	// 0x5DA1A0 (packet 0x11): the target interactable, the hand holding, ValidToApplyThisToObject again (0x5DA21E)
	auto& registry = Locator::entitiesRegistry::value();
	const auto seed = *_held;
	const auto target = packet.object;
	// IsInteractable of the target (0x5DA1C8) and of the held seed (0x5DA208)
	if (!Interactable(target) || !Interactable(seed) || !ValidToApplyThisToObject(seed, target))
	{
		return;
	}
	// SpellSeed::ApplyThisToObject 0x728D10's first branches, before the gesture and the cast: a spell dispenser, a
	// WorshipTotem or a spell icon takes the seed back (Worship/Worship.cpp). The original deletes the seed from the
	// ToBeDeleted list, so its InterfaceSetOutMagicHand still sees it: the hand lets go first here.
	if (worship::IsSeedReturnPoint(target, SeedOf(seed).creator.player))
	{
		if (_held && *_held == seed)
		{
			SeedLeftHand(seed);
			_held.reset();
		}
		_seedAction = SeedAction::None;
		EndApplyOnRelease();
		worship::ApplySeedToObject(seed, target);
		return;
	}
	// SpellSeed::ApplyThisToObject 0x728D10: a spell dispenser, a worship site (above), a MagicFireBall, else the
	// object cast fn_00729690 (the keep / remove rule of ApplyThisToMapCoord).
	// TODO(magic): ApplyToMagicFireBall 0x728AE0 (0x728D9A) is not ported
	int result = 0;
	const float size = packet.data[0];
	glm::vec3 interfacePos;
	glm::vec3 turnHand;
	glm::vec3 cameraForward;
	glm::vec3 turnVelocity;
	GetSpellInfo(interfacePos, turnHand, cameraForward, turnVelocity);
	const glm::vec3 handPos = _statusThrowHandPosition; // status +0x5C
	const glm::vec3 velocity = _statusThrowVelocity;    // status +0x44
	psys::ProcessInfo handInfo;
	handInfo.interfacePos = interfacePos;
	handInfo.handPos = handPos;
	handInfo.cameraForward = cameraForward;
	handInfo.direction = velocity;
	const auto position = magic::ToMap(registry.Get<const Transform>(target).position);
	entt::entity spell = entt::null;
	if (magic::seed::Cast(seed, position, &spell, size, handInfo) != 0)
	{
		if (spell != entt::null && IsSpellKeptInHand(seed))
		{
			result = 1;
		}
		else
		{
			// 0x728DEF: CreateSpotVisual(the object's pos, 3, 1.0, spell), as on the land
			const auto at = registry.Get<const Transform>(target).position;
			psys::manager::CreateSpotVisual(k_SpotVisualSucceedCast, at, 0.0f, spell);
			result = k_ResultRemoved;
		}
	}
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
		}
		_seedAction = SeedAction::None;
		return;
	}
	if (result == k_ResultRemoved || result == k_ResultPlaced)
	{
		// RemoveFirstFromHand fn_005CED60 (status fn_005DC1E0 -> InterfaceSetOutMagicHand), CHand::ThrowObject
		if (_held && *_held == seed)
		{
			SeedLeftHand(seed);
			_held.reset();
		}
		_seedAction = SeedAction::None;
		EndApplyOnRelease();
	}
}

void HandSystem::UpdateSeedAction(bool actionHeld) noexcept
{
	if (_seedAction == SeedAction::None)
	{
		return;
	}
	if (!IsHoldingSeed())
	{
		EndApplyOnRelease();
		_seedAction = SeedAction::None;
		return;
	}
	const auto seed = *_held;
	switch (_seedAction)
	{
	case SeedAction::ApplyOnReleaseMap:
	case SeedAction::ApplyOnReleaseObject:
		// 0x5D48D0: on the release (m_Buttons & 0x6000) the loop stops and the apply goes
		if (!actionHeld)
		{
			const auto state = _seedAction;
			_seedAction = SeedAction::None;
			EndApplyOnRelease();
			if (state == SeedAction::ApplyOnReleaseMap)
			{
				SendSeedApplyToMapCoord();
			}
			else
			{
				SendSeedApplyToObject();
			}
		}
		break;
	case SeedAction::LockedApplyMap:
	case SeedAction::LockedApplyObject:
		if (!actionHeld)
		{
			// packet 0x1A (held, turn - m_LockTurn) -> SpellSeed::ApplyUnlockProcess 0x728EB0
			_seedAction = SeedAction::None;
			magic::seed::ApplyUnlockProcess(seed);
			if (!Locator::entitiesRegistry::value().Valid(seed))
			{
				SeedLeftHand(seed);
				_held.reset();
			}
			break;
		}
		// 0x5D4C10 / 0x5D4D00: the apply again every tick (one packet per turn); on the land only when the point is
		// InBounds (0x5D4C82) and ValidToApplyThisToMapCoord (vt 0x724, 0x5D4C97), with no FailApply otherwise
		if (_seedAction == SeedAction::LockedApplyMap)
		{
			// (inferido) (0, 0, 0) with no point under the hand, as in SeedActionPressed
			const auto position = magic::ToMap(_interactionPoint.value_or(glm::vec3(0.0f)));
			if (magic::cast_rules::InBounds(position) && ValidToApplyThisToMapCoord(seed, position))
			{
				SendSeedApplyToMapCoord();
			}
		}
		else
		{
			SendSeedApplyToObject();
		}
		break;
	case SeedAction::None:
		break;
	}
}

void HandSystem::SeedLeftHand(entt::entity seed) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(seed) && registry.AllOf<SpellSeed>(seed))
	{
		// InterfaceSetOutMagicHand 0x728940: the status's last seed type (fn_005DCA20) and the icon's CancelCharge
		// 0x77F9A0 (Worship/Worship.cpp keeps the player's own copy of the last type, for the R gesture)
		gestures::State().lastSeedType = static_cast<int>(SeedOf(seed).seedType);
		worship::OnSeedOutOfHand(seed, SeedOf(seed).creator.player);
		// out of the hand only Spell::DrawSpellSeed 0x721360 -> 0x729020 draws it, while it follows its spell (a forest
		// seed cast from an icon): seed::DrawSpells (Magic/Core/SpellSeed.cpp) puts the mesh back over the spell in the
		// same frame
		ShowSeedMesh(seed, false);
	}
	// the local hand: fn_0046E890 (the in-hand effect), PHandFX SetPULevel(0, 0), StopTribalPowerRing
	magic::hand_fx::ReleaseInHandEffect();
	magic::hand_fx::SetPULevel(0, false);
	magic::hand_fx::StopTribalPowerRing();
	gestures::State().heldFlag8 = false;
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
		// (the in-hand effect comes with SpellSeed::SetPowerUp, from InterfaceSetInMagicHand)
		_seedInHand = held;
	}
	if (held != entt::null)
	{
		ComputeHoldParameters(held);
		ShowSeedMesh(held, IsG3DObjectDrawnInHand(held));
	}
	gestures::HandStatus status;
	status.heldSeed = held;
	status.holdingSomething = _held.has_value();
	status.validToShake = _held.has_value(); // Object::ValidToShakeFromHand 0x636AA0 = 1 (SpellSeed does not override)
	status.handReady = IsHandReadyForObject();
	// GInterface::InterfaceActionProcess fn_005D1120: m_InInfluence = CalculatePlayerInfluence(action position, type 1,
	// allies) > 0
	status.inInfluence = _interactionPoint.has_value() &&
	                     influence::CalculatePlayerInfluence(PlayerNames::PLAYER_ONE, *_interactionPoint) > 0.0f;
	status.actionLatched = actionHeld && held != entt::null;
	status.paused = Game::Instance() != nullptr && Game::Instance()->IsPaused();
	status.mouse = Game::Instance() != nullptr ? Game::Instance()->GetMousePosition() : glm::ivec2(0);
	status.forceDropHeld = [this]() { ForceDropHeld(); };
	status.removeFromHandFx = []() { magic::hand_fx::DoRemoveFromHandVisual(); };
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
			if (!event.done && path != nullptr && Game::Instance() != nullptr)
			{
				// several shots in one run: a "{}" in the path becomes the shot's time in tenths of a second
				std::string file(path);
				if (const auto mark = file.find("{}"); mark != std::string::npos)
				{
					file.replace(mark, 2, std::to_string(static_cast<int>(event.time * 10.0f + 0.5f)));
				}
				Game::Instance()->RequestScreenshot(file);
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
	return glm::vec3(xz.x, Locator::terrainSystem::value().GetHeightAt(xz), xz.y);
}
