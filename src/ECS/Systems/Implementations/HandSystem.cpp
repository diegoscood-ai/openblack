/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "HandSystem.h"
#include "HandSystemDetail.h"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <tuple>

#include <fmt/format.h>
#include <glm/gtc/type_ptr.hpp>

#include <spdlog/spdlog.h>

#include <L3DFile.h>
#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/HandArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "Camera/Camera.h"
#include "Windowing/WindowingInterface.h"
#include "Camera/CameraModel.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Influence/Influence.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Sprite.h"
#include "Graphics/Texture2D.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Abodes.h"
#include "ECS/Rocks.h"
#include "ECS/Systems/HandTap.h"
#include "ECS/ThingFlags.h"
#include "Common/HelpText.h"
#include "Help/ToolTips.h"
#include "FileSystem/FileSystemInterface.h"
#include "InfoConstants.h"
#include "Game.h"
#include "GameClock.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Worship/Worship.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace openblack::ecs::systems::hand_detail
{
void PlaySample(audio::SoundId id)
{
	// 0x429D60 -> 0x42A040: GAudio's options with owner 0, the sample, +0x10 0, is3D = track = 0, mode 3, loops 0
	audio::PlayOptions options;
	options.sound = static_cast<entt::id_type>(id);
	options.track = false;
	options.mode = 3;
	options.loops = 0;
	audio::PlaySoundEffect(options);
}

entt::entity PlaySample3D(audio::SoundId id, glm::vec3 point)
{
	audio::PlayOptions options;
	options.sound = static_cast<entt::id_type>(id);
	options.is3D = true;
	options.track = false;
	options.position = point;
	return audio::sample_play::AsEntity(audio::PlaySoundEffect(options));
}

/// MapCoords::IsLand (0x603720): the landscape cell under the point does not have the water bit (off the map or
/// without a block: not land)
bool IsLand(glm::vec3 point)
{
	return sea_cells::IsLand(point);
}
} // namespace openblack::ecs::systems::hand_detail

using namespace openblack::ecs::systems::hand_detail;

bool HandSystem::Initialize() noexcept
{
	_hands[static_cast<size_t>(Side::Left)] =
	    HandArchetype::Create(glm::vec3(0.0f), glm::half_pi<float>(), 0.0f, glm::half_pi<float>(), 0.01f, false);
	_hands[static_cast<size_t>(Side::Right)] =
	    HandArchetype::Create(glm::vec3(0.0f), glm::half_pi<float>(), 0.0f, glm::half_pi<float>(), 0.01f, true);

	LoadAnimations();
	LoadMorphMeshes();
	RegisterPhysicsHandlers();
	RegisterTapHandlers();
	// HandStateNormal::Enter 0x5B5D00: the up Zoomers at (0, 1, 0)
	_up.SetPosition(glm::vec3(0.0f, 1.0f, 0.0f));
	// the builder 0x5C9FC0 asks the hand's state for its tooltip every turn (fn_005D78D0)
	help::tooltips::SetStateSubmitter([this]() { SubmitToolTips(); });
	return false;
}

void HandSystem::RegisterTapHandlers() noexcept
{
	// Rock::InterfaceValidToTap 0x6E7450 (taller than 0.7) / Rock::InterfaceTap 0x6E7480 (SplitInTwo, G_RockTap)
	hand_tap::Register(
	    &Rocks::IsRock, [](entt::entity rock, const pot_resource::Dropper&) { return Rocks::ValidToTap(rock); },
	    [](entt::entity rock, const pot_resource::Dropper&, glm::vec3 handPos) -> uint32_t {
		    Rocks::Tap(rock, handPos);
		    return 1;
	    });
	// Abode::InterfaceValidToTap 0x406820 (always 1) / Abode::InterfaceTap 0x406830 (knocking on the roof)
	hand_tap::Register<Abode>(
	    [](entt::entity abode, const pot_resource::Dropper&) { return abodes::InterfaceValidToTap(abode); },
	    [](entt::entity abode, const pot_resource::Dropper&, glm::vec3 handPos) -> uint32_t {
		    abodes::InterfaceTap(abode, handPos);
		    return 1;
	    });
	// SpellIcon::InterfaceValidToTap 0x7263C0 / InterfaceTap 0x726430 and OneOffSpellSeed 0x72A630 / 0x72A640
	const auto worshipValid = [](entt::entity object, const pot_resource::Dropper& is) {
		return worship::InterfaceValidToTap(object, is.player);
	};
	const auto worshipTap = [](entt::entity object, const pot_resource::Dropper& is, glm::vec3) -> uint32_t {
		return static_cast<uint32_t>(worship::InterfaceTap(object, is.player));
	};
	hand_tap::Register<SpellIcon>(worshipValid, worshipTap);
	hand_tap::Register<OneOffSpellSeed>(worshipValid, worshipTap);
}

bool HandSystem::InInfluence() const noexcept
{
	// GInterface +0x48 m_InInfluence (InterfaceActionProcess fn_005D1120: CalculatePlayerInfluence(action position, type
	// 1, allies) > 0)
	return _interactionPoint.has_value() &&
	       influence::CalculatePlayerInfluence(PlayerNames::PLAYER_ONE, *_interactionPoint, influence::CalcType::Interface) > 0.0f;
}

bool HandSystem::ValidForPlaceInHand(entt::entity object) const noexcept
{
	// Rock::ValidForPlaceInHand 0x6E7030 (2D radius <= 3.6); a spell icon keeps Object::ValidForPlaceInHand 0x402870 = 0
	// (a one-shot orb is Mobile::ValidForPlaceInHand 0x425B00 = 1)
	if (Rocks::IsRock(object))
	{
		return Rocks::ValidForPlaceInHand(object);
	}
	return !Locator::entitiesRegistry::value().AllOf<SpellIcon>(object);
}

bool HandSystem::SendTap(entt::entity object) noexcept
{
	// GInterface::SendTap 0x5D38A0: (m_InInfluence || !InterfaceMustBeInInfluenceForInteraction) && InterfaceValidToTap(IS)
	// == 1 && !IsCannotBePickedUp -> packet 0x20 -> 0x5DA650, which checks InterfaceValidToTap again and calls InterfaceTap.
	// InterfaceMustBeInInfluenceForInteraction is Object's 0x4028A0 = 1 for every ported class (only ScriptHighlight
	// 0x709840 overrides it, not ported). IsCannotBePickedUp 0x401A10: the flag 0x2000 of SET_ID_PICKUPABLE 169.
	// Tap 0x5D3930 first remembers the object (RememberTapped fn_005D36D0 at 0x5D3967; (not ported) not a Reward under
	// the leash), whatever SendTap then does
	RememberTapped(object);
	const pot_resource::Dropper is {true, PlayerNames::PLAYER_ONE, true};
	if (!InInfluence() || !hand_tap::ValidToTap(object, is) || thing_flags::IsCannotBePickedUp(object))
	{
		return false;
	}
	hand_tap::Tap(object, is, _interactionPoint.value_or(glm::vec3(0.0f)));
	return true;
}

std::array<entt::entity, static_cast<size_t>(HandSystemInterface::Side::_Count)> HandSystem::GetPlayerHands() const noexcept
{
	return _hands;
}

std::array<std::optional<glm::vec3>, static_cast<size_t>(HandSystemInterface::Side::_Count)>
HandSystem::GetPlayerHandPositions() const noexcept
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto hands = GetPlayerHands();
	std::array<std::optional<glm::vec3>, static_cast<size_t>(Side::_Count)> result = {
	    registry.Get<Transform>(hands[static_cast<size_t>(Side::Left)]).position,
	    registry.Get<Transform>(hands[static_cast<size_t>(Side::Right)]).position,
	};
	// TODO(#693): Hand Getter should return an optional if the hand doesn't have a valid position
	// When the position is zero, it probably means it's not on the map (e.g. mouse is in the sky)
	if (result[static_cast<size_t>(Side::Left)] == glm::zero<glm::vec3>())
	{
		result[static_cast<size_t>(Side::Left)] = std::nullopt;
	}
	if (result[static_cast<size_t>(Side::Right)] == glm::zero<glm::vec3>())
	{
		result[static_cast<size_t>(Side::Right)] = std::nullopt;
	}
	return result;
}

void HandSystem::LoadAnimations() noexcept
{
	auto logger = spdlog::get("game");
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		const auto& mesh = Locator::resources::value().GetMeshes().Handle(Hand::k_MeshId);
		if (!mesh || !mesh->IsBoned())
		{
			SPDLOG_LOGGER_WARN(logger, "Hand mesh is not loaded or has no bones: hand stays in bind pose");
			return;
		}
		const auto hbnPath = fileSystem.GetPath<filesystem::Path::Data>() / "CTR" / "hh.HBN";
		const auto specsDirectory = fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>());
		auto animator = std::make_unique<HandAnimator>();
		if (animator->Load(fileSystem.ReadAll(hbnPath), specsDirectory, mesh->GetBoneMatrices(), mesh->GetBoneParents()))
		{
			_animator = std::move(animator);
			LoadGeometry();
		}
		if (std::getenv("OPENBLACK_HAND_INFO") != nullptr)
		{
			const auto& pots = Locator::infoConstants::value().pot;
			for (size_t i = 0; i < pots.size(); ++i)
			{
				const auto& p = pots[i];
				SPDLOG_LOGGER_INFO(logger,
				                   "PotInfo {}: potType={} resource={} maxInPot={} next={} initial={} perTurn={} perTurnEnd={} maxPick={} ramp={} mesh={}",
				                   i, static_cast<int>(p.potType), static_cast<int>(p.resourceType), p.maxAmountInPot,
				                   static_cast<int>(p.nextPotForResource), p.amountPickedUpInitially, p.amountPickedUpPerTurn,
				                   p.amountPickedUpPerTurnEnd, p.maxAmountCanBePickedUp, p.multiPickUpRampTime, static_cast<int>(p.meshId));
			}
			const auto& trees = Locator::infoConstants::value().tree;
			for (size_t i = 0; i < trees.size(); ++i)
			{
				const auto& t = trees[i];
				SPDLOG_LOGGER_INFO(logger,
				                   "TreeInfo {} '{}': mesh={} growing={} burning={} strength={} defence={} startLife={} wood={} food={} "
				                   "weight={} carried={} minSize={} maxSize={} grows={} growth={} immersion={} collide={} "
				                   "helpInHand={} maxTrees={}",
				                   i, t.debugString.data(), static_cast<int>(t.normal), static_cast<int>(t.growing),
				                   static_cast<int>(t.burning), t.strength, t.defence, t.startLife, t.woodValue, t.foodValue, t.weight,
				                   static_cast<int>(t.carriedType), t.minSize, t.maxSize, t.growsAfterNumGameTurns, t.growthAmount,
				                   static_cast<int>(t.immersion), static_cast<int>(t.collideSound), static_cast<int>(t.helpInHand),
				                   t.maxNumTreesCanProduce);
			}
		}
		// Debug: OPENBLACK_HAND_ANIM=<C node> forces an animation (e.g. Cgrip) instead of the gameplay state.
		if (const char* forced = std::getenv("OPENBLACK_HAND_ANIM"); forced != nullptr && _animator)
		{
			_override = forced;
		}
		// Debug: OPENBLACK_HAND_DUMP=<file> writes the evaluated bone matrices of every clip for validation.
		if (const char* dump = std::getenv("OPENBLACK_HAND_DUMP"); dump != nullptr && _animator)
		{
			std::ofstream out(dump);
			out << "{";
			bool firstClip = true;
			for (const auto& clip : _animator->ListClips())
			{
				for (const auto& [t, lr, fb] : {std::tuple {0.0f, 0.0f, 0.0f}, {137.0f, 0.0f, 0.0f}, {0.0f, 0.6f, -0.4f}})
				{
					const auto key = fmt::format("{}@{}@{}@{}", clip.name, t, lr, fb);
					out << (firstClip ? "" : ",") << "\n\"" << key << "\":[";
					firstClip = false;
					const auto mats = _animator->Evaluate(clip.name, t, lr, fb, clip.name == "Cgrip" ? 1.0f : 0.0f);
					for (size_t b = 0; b < mats.size(); ++b)
					{
						const float* v = glm::value_ptr(mats[b]);
						out << (b ? "," : "") << "[";
						for (int k = 0; k < 16; ++k)
						{
							out << (k ? "," : "") << v[k];
						}
						out << "]";
					}
					out << "]";
				}
			}
			out << "\n}\n";
			SPDLOG_LOGGER_INFO(logger, "Hand animation dump written to {}", dump);
		}
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(logger, "Failed to load hand animations: {}", e.what());
	}
}

void HandSystem::Update(std::chrono::microseconds dt, glm::vec2 mouseDelta, bool gripping, bool actionHeld) noexcept
{
	if (!_animator)
	{
		return;
	}
	const float seconds = static_cast<float>(dt.count()) / 1e6f;
	// CHand::PrepareForDrawing 0x46C550 runs before the hand's state machine: the good / evil morph
	UpdateMorphing();

	// Cursor speed (px/s) normalised to -1..+1 like the original L layers expect. Quick response while the
	// mouse moves, slower return to neutral once it stops.
	constexpr float k_ReferenceSpeed = 900.0f;
	if (seconds > 0.0f && (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f))
	{
		const auto velocity = mouseDelta / std::max(seconds, 0.004f);
		_motionTarget = glm::clamp(glm::vec2(velocity.x, -velocity.y) / k_ReferenceSpeed, -1.0f, 1.0f);
		_motionAge = 0.0f;
	}
	UpdateGripDust(seconds);
	// CHand mouse smoothing: smooth += dt * vel, clamped to +-80 px of the mouse, vel = (vel + (mouse - smooth) * 20 dt)
	// * 0.03^dt. Pixels are taken on a 1024-wide screen (the original ran at 640-1024 wide; the reference is a guess).
	if (Locator::windowing::has_value())
	{
		const float width = static_cast<float>(std::max(1, Locator::windowing::value().GetSize().x));
		_mouse += mouseDelta * (1024.0f / width);
	}
	if (_smoothMouseValid && seconds > 0.0f)
	{
		_smoothMouse = glm::clamp(_smoothMouse + seconds * _smoothMouseVelocity, _mouse - 80.0f, _mouse + 80.0f);
		_smoothMouseVelocity = (_smoothMouseVelocity + (_mouse - _smoothMouse) * 20.0f * seconds) * std::pow(0.03f, seconds);
	}
	else
	{
		_smoothMouse = _mouse;
		_smoothMouseVelocity = glm::vec2(0.0f);
		_smoothMouseValid = true;
	}
	_motionAge += seconds;
	if (_motionAge > 0.045f)
	{
		_motionTarget *= std::exp(-seconds * 8.0f);
	}
	_motion += (_motionTarget - _motion) * (1.0f - std::exp(-seconds * 15.0f));
	_animator->SetMotion(_motion.x, _motion.y);

	// Test hooks driven by environment variables, once the landscape exists (HandDebugHooks.cpp).
	if (static bool ran = false; !ran && Locator::terrainSystem::has_value())
	{
		ran = true;
		RunDebugHooks();
	}
	hand_detail::DumpEntityCounts();
	if (_testActionSeconds > 0.0f)
	{
		actionHeld = true;
		_testActionSeconds -= seconds;
	}
	// OPENBLACK_TEST_CAST: the synthetic presses of the action button (HandSpellSeed.cpp)
	actionHeld = TestCastActionHeld(seconds, actionHeld);

	// Pick up / drop with the action button (right). Only while not gripping the land.
	if (_held && !Locator::entitiesRegistry::value().Valid(*_held))
	{
		_held.reset();
	}
	_hovered = (_held || _tug || gripping) ? std::nullopt : FindObjectUnderHand();
	const bool actionPressed = actionHeld && !_actionWasHeld;
	// ActionPressed fn_005D1330 (0x5D13A1 / 0x5D14A6): with nothing collided (+0x400), the object near the action's point
	// (FindObjectNearMapCoord fn_005D39E0) becomes the collided object, and the branches below test it. (approximate)
	// the hover's class filter (FindObjectUnderHand) stands for the pick-up / tap tests (vt 0x6FC, 0x740) and the locked
	// select's (vt 0x6CC) is the field / pile branches'; SetObject's IsInteractable (vt 0x190, fn_005D5E40): (inferred)
	// every ported class is interactable. A fish farm (0x5D3AAC) goes to the fish branch through the action's point
	std::optional<entt::entity> nearObject;
	if (actionPressed && !_held && !_tug && !gripping && !_hovered && !_cursorObject && _interactionPoint)
	{
		nearObject = FindObjectNearMapCoord(*_interactionPoint);
		if (nearObject)
		{
			_cursorObject = nearObject;
			_hovered = FindObjectUnderHand();
		}
	}
	const bool actionReleased = !actionHeld && _actionWasHeld;
	_actionWasHeld = actionHeld;
	// fn_005D3700 (InterfaceActionProcess, before the action states): the last thing tapped or clicked
	UpdateTapMemory(actionReleased);
	// GInterface +0x48 m_InInfluence: every ported class needs it for taps, locked selects and pick-ups
	// (Object::InterfaceMustBeInInfluenceForInteraction 0x4028A0 = 1, vt 0x714)
	const auto TapInInfluence = [this]() { return InInfluence(); };
	if (actionPressed && _hovered && !_held && Locator::entitiesRegistry::value().AllOf<Field>(*_hovered))
	{
		// fields are a locked select (ValidForLockedSelectProcess 0x5299E0): ActionPressed fn_005D1330 starts the scooping
		// at once (StartTapOrLockedSelect 0x5D1A00) in the influence (m_InInfluence || !vt 0x714). Out of it the field
		// goes on to the pick-up / tap path, where it is neither placeable (Object 0x402870) nor tappable (Object
		// 0x4196B0): nothing.
		_pickPressHeld = TapInInfluence() && !thing_flags::IsCannotBePickedUp(*_hovered) && TryPickUpField(*_hovered);
	}
	else if (actionPressed && _hovered && !_held)
	{
		// Piles cannot be tapped, so the locked select (scooping) starts at once (StartGrab -> packet 0x1B), in the
		// influence as above; out of it nothing happens (StartGrab 0x5D1740 taps, and a pile is not tappable).
		const auto source = PotInfoOf(*_hovered);
		if (source != PotInfo::_COUNT && source != PotInfo::HandWood && source != PotInfo::HandFood)
		{
			if (TapInInfluence() && !thing_flags::IsCannotBePickedUp(*_hovered))
			{
				PickUp(*_hovered);
			}
			_pickPressHeld = _held.has_value();
		}
		else if (!ValidForPlaceInHand(*_hovered) || thing_flags::IsCannotBePickedUp(*_hovered) || !TapInInfluence())
		{
			// StartGrab 0x5D1740: an object that cannot go into the hand (a rock too big to lift, a spell icon, the flag
			// 0x2000 of SET_ID_PICKUPABLE) or out of the influence is tapped at once: Tap 0x5D3930 -> SendTap 0x5D38A0
			// (refused out of the influence) -> packet 0x20 -> InterfaceTap (Rock::InterfaceTap splits it,
			// SpellIcon::InterfaceTap 0x726430)
			SendTap(*_hovered);
			_hovered.reset();
		}
		else if (Locator::entitiesRegistry::value().AllOf<Tree>(*_hovered) && _interactionPoint &&
		         !physics::PhysicsObjects::Find(*_hovered))
		{
			// StartGrab 0x5D1740: a tuggable object goes to CHand::PickUp(obj, needsTug) at the press, so the tug starts
			// at once, not after the 225 ms grab threshold (State_Grab picks it up once the tug lets it go)
			BeginTug(*_hovered);
		}
		else
		{
			_pendingPick = _hovered;
			_pendingPickTime = 0.0f;
		}
	}
	else if (actionPressed && !_held && !_hovered && !gripping && _cursorObject &&
	         Locator::entitiesRegistry::value().Valid(*_cursorObject) && abodes::InterfaceValidToTap(*_cursorObject))
	{
		// GInterface::ActionPressed fn_005D1330 sends the object under the cursor to StartGrab 0x5D1740 when it can go into
		// the hand or it is only tappable (Abode::InterfaceValidToTap 0x406820 = 1, so FindObjectUnderHand leaves abodes
		// out: they are never hovered for a pick-up). An abode cannot go into the hand (Object::ValidForPlaceInHand
		// 0x402870 = 0), so StartGrab taps it at once -> Tap 0x5D3930 -> SendTap 0x5D38A0, which needs the hand inside the
		// influence (Object::InterfaceMustBeInInfluenceForInteraction 0x4028A0 = 1) -> packet 0x20 ->
		// Abode::InterfaceTap 0x406830: knocking on the roof.
		SendTap(*_cursorObject);
	}
	else if (actionPressed && !_held && !_hovered && !gripping && _interactionPoint && TapInInfluence() &&
	         TryPickUpFish(*_interactionPoint))
	{
		// fish: the locked select starts at once, like piles, in the influence (ActionPressed fn_005D1330)
		_pickPressHeld = true;
	}
	else if (actionPressed && IsHoldingSeed())
	{
		// ActionPressedHolding 0x5D1560 with a spell seed: armed until the release (HAND_GESTURE), cast at once
		// (HAND_POSITION) or kept casting while held (IN_HAND); HandSpellSeed.cpp
		SeedActionPressed();
	}
	else if (actionPressed && _held && !_pickPressHeld)
	{
		// ActionPressedHolding 0x5D1560: the object under the hand takes the held one (a villager into a teleport stone:
		// HandApplyToObject.cpp); otherwise press the action button again, move and release to put it down or hurl it
		// (state 12, 0x5D4DB0).
		// 0x5D16BE: out of the influence ([this+0x48] == 0) nothing happens (0x5D172A returns 1 with no state set)
		if (TapInInfluence() && !HeldActionPressedOnObject(true))
		{
			_releaseArmed = true;
		}
	}
	// the seed's apply states 8..11, and what the gesture system is told about the hand (HandSpellSeed.cpp)
	UpdateSeedAction(actionHeld);
	UpdateSeedInHand(actionHeld);
	if (actionReleased && _pickPressHeld)
	{
		// Releasing the press that picked it up ends the grab / scooping (packet 0x1C); the object stays in the hand.
		_pickPressHeld = false;
		_pickSource.reset();
	}
	else if (actionReleased && _held && _releaseArmed)
	{
		_releaseArmed = false;
		// State 12 (0x5D4DB0) sends the holding spring's velocity (CHand+0x48C8, units per second, capped at 124) and
		// every release takes the same path: Object::InitialisePhysicsFromHand decides thrown (vel.x^2 + vel.z^2 > 4)
		// or put down, a hand pot |v|^2 <= 5 (HandHolding.cpp).
		Release(_handVelocity);
	}
	if (_pendingPick)
	{
		// The grab completes after 225 ms of holding the action button (State_Grab 0x5D5250). Released earlier it is a
		// tap (Tap 0x5D3930 -> SendTap), which does nothing for most objects (Object::InterfaceValidToTap 0x4196B0 = 0),
		// splits rocks taller than 0.7 (Rock::InterfaceTap) and puts a one-shot orb's charged seed in the hand
		// (OneOffSpellSeed::InterfaceTap 0x72A640).
		constexpr float k_PickUpHoldSeconds = 0.225f;
		_pendingPickTime += seconds;
		if (!Locator::entitiesRegistry::value().Valid(*_pendingPick))
		{
			_pendingPick.reset();
		}
		else if (!actionHeld)
		{
			SendTap(*_pendingPick);
			_pendingPick.reset();
		}
		else if (_pendingPickTime >= k_PickUpHoldSeconds)
		{
			const auto entity = *_pendingPick;
			_pendingPick.reset();
			if (!TapInInfluence() || thing_flags::IsCannotBePickedUp(entity))
			{
				// GenericPickup 0x5D2800 with IsCannotBePickedUp (vt 0x180) or out of the influence (m_InInfluence 0,
				// vt 0x714 = 1) returns 0: State_Grab resets the action, nothing is picked up
			}
			else if (Locator::entitiesRegistry::value().AllOf<BigForest>(entity))
			{
				// not tuggable: the grab takes a tree out of the forest straight into the hand
				_pickPressHeld = TakeTreeFromForest(entity);
			}
			else if (Locator::entitiesRegistry::value().AllOf<Tree>(entity) && _interactionPoint &&
			         !physics::PhysicsObjects::Find(entity))
			{
				// a standing tree is tugged; one in physics (thrown, not landed yet) is caught like any flying object
				BeginTug(entity);
			}
			else if (PickUpSeedOrStone(entity, true))
			{
				// a spell seed over its spell or a teleport stone: GenericPickup 0x5D2800 -> PlaceObjectInMagicHand
				// 0x5DA6F0, the seed in the hand and its spell closed (HandApplyToObject.cpp)
			}
			else
			{
				// a one-shot orb held for 225 ms is picked up itself (GenericPickup 0x5D2800, packet 0x13 ->
				// PlaceObjectInMagicHand -> OneOffSpellSeed::InterfaceSetInMagicHand 0x72A530; Worship.cpp)
				PickUp(entity);
				_pickPressHeld = _held.has_value();
			}
		}
	}
	UpdateTug(seconds, actionHeld);
	if (!_held && !_tug)
	{
		_releaseArmed = false;
		if (!actionHeld)
		{
			_pickPressHeld = false;
		}
	}
	UpdateMultiPickUp(seconds, actionHeld);
	UpdatePickupSound(_pickSource.has_value() && _held.has_value());
	// Once per game turn: the amount in the hand (0xEEA "Cantidad: %3.0f", ForceToolTips), forced every turn of the
	// scooping (ProcessInInteract: Pile 0x66E6E4, Field 0x52989F, FishFarm 0x52DAD6) and once when the locked select
	// ends (Pile 0x66E8DA, Field 0x529AD9, FishFarm 0x52D92A); its lifetime keeps it about 13 turns after that. (The
	// help system's turn, HelpSystem::Process 0x5C8FE0, is Game::GameLogicLoop's step 0x54E69E)
	if (_toolTipTurn != game_clock::Turn())
	{
		_toolTipTurn = game_clock::Turn();
		const bool scooping = _pickSource.has_value() && _held.has_value();
		if ((scooping || _toolTipScooping) && _held &&
		    (PotInfoOf(*_held) == PotInfo::HandFood || PotInfoOf(*_held) == PotInfo::HandWood))
		{
			const auto& pot = Locator::entitiesRegistry::value().Get<const Pot>(*_held);
			help::tooltips::Force(helptext::k_ToolTipAmountInHand, static_cast<float>(pot.amount));
		}
		_toolTipScooping = scooping;
		// test hook OPENBLACK_TEST_TOOLTIP=<amount>: the amount forced every turn
		if (static const char* test = std::getenv("OPENBLACK_TEST_TOOLTIP"); test != nullptr)
		{
			help::tooltips::Force(helptext::k_ToolTipAmountInHand, static_cast<float>(std::atof(test)));
		}
	}
	// the KMIcon's fade, in real time (fn_00447850 -> fn_00448AC0(g_delta_time * 0.001))
	help::tooltips::Frame(seconds);
	UpdatePickupParticles(seconds, _pickSource.has_value() && _held.has_value() && std::getenv("OPENBLACK_NO_PICKUP_PSYS") == nullptr);
	UpdateTestSplash(seconds);
	UpdateTestAbode(seconds);
	UpdateRootsAndPiles(seconds);
	archetypes::PotArchetype::UpdateSizes(seconds);

	// Gameplay state machine (original hand states): holding > gripping > can pick up > idle.
	std::string clip = gripping ? "Cgrip" : (_hovered ? "Ccan_pickup" : "Cwiggle");
	// HandStateHolding::Update (jump table 0x5B56A8): the pose and its frame (in ms) per hold type.
	//   ABOVE: Chold_above at dur * 0.5 * (1 - grip), grip = min(1, R / (3.2 s * 1.2))
	//   TREE / SIDE / VILLAGER: Chold_side at (dur >> 1) * grip, grip = min(1, R / (3.2 s))
	const bool holding = (_held.has_value() || _tug.has_value()) && _override.empty() && _holdType != HoldType::None;
	if (_held && PotInfoOf(*_held) == PotInfo::HandFood)
	{
		// Object::GetHoldRadius 0x638C00 -> PileFood::Get2DRadius 0x66F180 = Object::Get2DRadius x GetProportionRaised
		// 0x66EB60 (ecs::object::Get2DRadius, inside ComputeHoldParameters): the hand opens as the food in it grows.
		// Wood keeps a constant radius (PileWood has no Get2DRadius of its own).
		ComputeHoldParameters(*_held);
	}
	_animator->SetFrame(std::nullopt);
	if (holding && _holdType == HoldType::Magic && _animator->Has("Cwiggle"))
	{
		// MAGIC (0x5B50B0, a spell seed until it is ready): GetAnim(0, 0) = Cwiggle held at half its length
		clip = "Cwiggle";
		_animator->SetTime(static_cast<float>(_animator->GetDurationMs(clip) >> 1));
	}
	else if (holding && _holdType == HoldType::Above && _animator->Has("Chold_above"))
	{
		clip = "Chold_above";
		const float grip = std::min(1.0f, _holdRadius / (3.2f * _handScale * 1.2f));
		_animator->SetTime(static_cast<float>(_animator->GetDurationMs(clip)) * 0.5f * (1.0f - grip));
	}
	else if (holding && _holdType != HoldType::Above && _animator->Has("Chold_side"))
	{
		clip = "Chold_side";
		const float grip = std::min(1.0f, _holdRadius / (3.2f * _handScale));
		_animator->SetTime(static_cast<float>(_animator->GetDurationMs(clip) >> 1) * grip);
	}
	else
	{
		_animator->SetTime(std::nullopt);
	}
	if (!_override.empty())
	{
		clip = _override;
	}
	// special_hold (Cphile/Chorn) is not used for held objects: hand piles are SIDE, Chorn is the spell seed grain.
	_animator->SetSpecialHold(std::nullopt, std::chrono::milliseconds(140));
	if (clip != _animator->GetCurrentClip())
	{
		_animator->Play(clip, std::chrono::milliseconds(clip == "Cgrip" ? 90 : 150));
	}
	// Grip drag lives mostly in the root translation of Lgrip_lr/Lgrip_fb.
	_animator->SetLayerTranslationScale(clip == "Cgrip" ? 1.0f : 0.0f);
	// Every clip applies its root bone (CAnim A_0 * R_0) under fn_0046E160's matrix, as the original
	_animator->SetRootLocked(false);
	_animator->Update(dt);
}

const std::vector<glm::mat4>* HandSystem::GetBoneMatrices() const noexcept
{
	return _animator ? &_animator->GetBoneMatrices() : nullptr;
}

std::vector<std::string> HandSystem::GetAnimationNames() const noexcept
{
	std::vector<std::string> names;
	if (_animator)
	{
		for (const auto& clip : _animator->ListClips())
		{
			if (!clip.name.empty() && clip.name.front() == 'C')
			{
				names.push_back(clip.name);
			}
		}
	}
	return names;
}

const std::string& HandSystem::GetCurrentAnimation() const noexcept
{
	static const std::string k_None;
	return _animator ? _animator->GetCurrentClip() : k_None;
}

void HandSystem::SetAnimationOverride(const std::string& name) noexcept
{
	_override = name;
}

