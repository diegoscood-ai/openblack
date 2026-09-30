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
#include "ECS/Influence/Influence.h"
#include "ECS/Registry.h"
#include "Game.h"
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
/// a worship site fn_00728A50: M7), a MagicFireBall (ValidToApplyToMagicFireBall 0x728A20: M5), else a seed with
/// castOnObject, the cast rule at the object and the class's object check (vt 0x2C)
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

/// SpellSeed::ValidToApplyThisToObject 0x7286D0: ready, the target not highlighted by the script, CanCast(object)
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

uint32_t CurrentTurn()
{
	return Game::Instance() != nullptr ? Game::Instance()->GetTurn() : 0;
}

/// OPENBLACK_TEST_CAST="press@t0,release@t1[,press@t2,release@t3...][,shot@t]" (seconds after the land exists); shot
/// takes a screenshot then into OPENBLACK_TEST_SHOT_PATH (for screenshots at a game time rather than a frame)
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
	return _held && registry.Valid(*_held) && registry.AllOf<SpellSeed>(*_held);
}

bool HandSystem::IsHandReadyForObject() const noexcept
{
	// IsSpaceInHands and no locked hand (status +0x24 & 0x20: a tug or a locked select here)
	return !_held && !_tug && !_pickSource;
}

void HandSystem::GetSpellInfo(glm::vec3& interfacePos, glm::vec3& handPos, glm::vec3& cameraForward,
                              glm::vec3& velocity) const noexcept
{
	const auto& hand = Locator::entitiesRegistry::value().Get<const Transform>(_hands[static_cast<size_t>(Side::Left)]);
	// +0x00: the status's map position (inf: the point under the hand) as a world point; +0x0C: status +0xC8, the hand
	interfacePos = _interactionPoint.value_or(hand.position);
	handPos = hand.position;
	cameraForward = Locator::camera::has_value() ? glm::normalize(Locator::camera::value().GetForward()) : glm::vec3(0.0f);
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
	if (!_held)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = *_held;
	if (!IsHoldingSeed())
	{
		// Object::ThrowObjectFromHand(status, 1) with the 0x4D packet's zero velocity: it falls
		Throw(glm::vec3(0.0f));
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
	// packet 0x2B (point, 4): the fail spot visual; LH_SAMPLE_G_SPELLCASTFAILURE (0x25)
	psys::manager::CreateSpotVisual(k_SpotVisualFailCast, point, 0.0f, entt::null);
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
	if (Locator::audio::has_value() && !_seedLoopSound)
	{
		auto& audio = Locator::audio::value();
		const auto id = static_cast<entt::id_type>(audio::SoundId::G_HandGesture_02);
		if (Locator::resources::value().GetSounds().Contains(id))
		{
			const auto& sound = audio.GetSound(id);
			_seedLoopSound = audio.CreateEmitter(id, audio::PlayType::Repeat, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec2(0.0f),
			                                     sound.volume, audio::AudioStatus::Playing, true);
			audio.PlayEmitter(*_seedLoopSound);
		}
	}
	if (SeedTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand seed: armed (state {})", static_cast<int>(state));
	}
}

void HandSystem::EndApplyOnRelease() noexcept
{
	// 0x5D27B0: SoundTag::Remove(this, 3, IN_GAME)
	if (_seedLoopSound && Locator::audio::has_value())
	{
		auto& audio = Locator::audio::value();
		audio.StopEmitter(*_seedLoopSound);
		audio.DestroyEmitter(*_seedLoopSound);
	}
	_seedLoopSound.reset();
}

void HandSystem::SeedActionPressed() noexcept
{
	const auto seed = *_held;
	const auto target = _cursorObject && Locator::entitiesRegistry::value().Valid(*_cursorObject) ? *_cursorObject : entt::null;
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
	const auto turn = CurrentTurn();
	if (_applySentTurn && *_applySentTurn == turn)
	{
		return 1; // one apply packet per turn (m_ApplySentTurn)
	}
	const auto seed = *_held;
	auto& state = gestures::State();
	glm::vec3 point = _interactionPoint.value_or(glm::vec3(0.0f));
	if (state.circlePending)
	{
		state.gesture.position = state.circlePosition;
		state.gesture.size = state.circleSize;
		state.gesture.gesture = state.circleGesture;
		if (SizingGesture(seed) != 0)
		{
			// MapCoords(circlePos.x * 6553.6, circlePos.z * 6553.6, altitude 0)
			point = glm::vec3(state.circlePosition.x, 0.0f, state.circlePosition.z);
			point.y = magic::ToWorld(glm::vec3(point.x, 0.0f, point.z)).y;
		}
	}
	else
	{
		state.gesture.gesture = gestures::k_None;
	}
	const auto position = magic::ToMap(point);
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
	// packet 0x12 -> GInterface 0x5DA400: held->IsAvailable, ValidToApplyThisToMapCoord, ApplyThisToMapCoord 0x728E20
	int result = 0;
	if (ValidToApplyThisToMapCoord(seed, position) && magic::seed::CanCast(seed, position))
	{
		glm::vec3 interfacePos;
		glm::vec3 handPos;
		glm::vec3 cameraForward;
		glm::vec3 velocity;
		GetSpellInfo(interfacePos, handPos, cameraForward, velocity);
		if (const char* test = std::getenv("OPENBLACK_TEST_THROW_VEL"); test != nullptr)
		{
			std::sscanf(test, "%f,%f,%f", &velocity.x, &velocity.y, &velocity.z);
		}
		psys::ProcessInfo handInfo;
		handInfo.interfacePos = interfacePos;
		handInfo.handPos = handPos;
		handInfo.cameraForward = cameraForward;
		handInfo.direction = velocity; // status +0x44 ThrowVelocity
		entt::entity spell = entt::null;
		if (magic::seed::Cast(seed, position, &spell, state.gesture.size, handInfo) != 0)
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
				psys::manager::CreateSpotVisual(k_SpotVisualSucceedCast, at, 0.0f, spell);
				result = k_ResultRemoved;
			}
			if (SeedTrace())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"),
				                   "Hand seed: cast {} at ({:.1f}, {:.1f}) gesture {} size {:.1f} velocity ({:.1f}, {:.1f}, {:.1f}) -> spell {}",
				                   InfoOf(seed).debugString.data(), position.x, position.z, state.gesture.gesture,
				                   state.gesture.size, velocity.x, velocity.y, velocity.z,
				                   spell == entt::null ? -1 : static_cast<int>(spell));
			}
		}
	}
	HandleSeedApplyResult(result, seed);
	return 1;
}

int HandSystem::SendSeedApplyToObject() noexcept
{
	if (!IsHoldingSeed())
	{
		return 0;
	}
	const auto seed = *_held;
	auto& registry = Locator::entitiesRegistry::value();
	const auto target = _cursorObject && registry.Valid(*_cursorObject) ? *_cursorObject : entt::null;
	if (target == entt::null || !ValidToApplyThisToObject(seed, target))
	{
		return 0;
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
		return 1;
	}
	const auto turn = CurrentTurn();
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
		// packet 0x11 -> 0x5DA1A0 -> SpellSeed::ApplyThisToObject 0x728D10: a spell dispenser, a worship site (M7), a
		// MagicFireBall (M5), else the object cast fn_00729690 (the keep / remove rule of ApplyThisToMapCoord)
		int result = 0;
		glm::vec3 interfacePos;
		glm::vec3 handPos;
		glm::vec3 cameraForward;
		glm::vec3 velocity;
		GetSpellInfo(interfacePos, handPos, cameraForward, velocity);
		psys::ProcessInfo handInfo;
		handInfo.interfacePos = interfacePos;
		handInfo.handPos = handPos;
		handInfo.cameraForward = cameraForward;
		handInfo.direction = velocity;
		const auto position = magic::ToMap(registry.Get<const Transform>(target).position);
		entt::entity spell = entt::null;
		if (magic::seed::Cast(seed, position, &spell, state.gesture.size, handInfo) != 0)
		{
			result = spell != entt::null && IsSpellKeptInHand(seed) ? 1 : k_ResultRemoved;
		}
		HandleSeedApplyResult(result, seed);
	}
	return 1;
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
		// 0x5D4C10 / 0x5D4D00: the apply again every tick (one packet per turn)
		if (_seedAction == SeedAction::LockedApplyMap)
		{
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
		// Spell::DrawSpellSeed 0x721360 draws nothing: out of the hand the seed is not seen
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
				Game::Instance()->RequestScreenshot(path);
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
