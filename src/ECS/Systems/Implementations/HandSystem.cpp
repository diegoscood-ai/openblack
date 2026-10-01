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
#include "Audio/AudioManagerInterface.h"
#include "Camera/Camera.h"
#include "Windowing/WindowingInterface.h"
#include "Camera/CameraModel.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Mesh.h"
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
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Rocks.h"
#include "FileSystem/FileSystemInterface.h"
#include "InfoConstants.h"
#include "Game.h"
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
	const auto soundId = static_cast<entt::id_type>(id);
	if (Locator::audio::has_value() && Locator::resources::value().GetSounds().Contains(soundId))
	{
		Locator::audio::value().PlaySound(soundId, audio::PlayType::Once);
	}
}

entt::entity PlaySample3D(audio::SoundId id, glm::vec3 point)
{
	if (!Locator::audio::has_value() || !Locator::camera::has_value())
	{
		return entt::null;
	}
	return Locator::audio::value().PlayAt(static_cast<entt::id_type>(id), point);
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
	RegisterPhysicsHandlers();
	return false;
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
	// The player's hand interacts through the index fingertip (or the grip point), not the mesh origin.
	if (_interactionPoint)
	{
		result[static_cast<size_t>(Side::Left)] = _interactionPoint;
	}
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
	const bool actionReleased = !actionHeld && _actionWasHeld;
	_actionWasHeld = actionHeld;
	if (actionPressed && _hovered && !_held && Locator::entitiesRegistry::value().AllOf<Field>(*_hovered))
	{
		// fields are a locked select (ValidForLockedSelectProcess 0x5299E0): the scooping starts at once
		_pickPressHeld = TryPickUpField(*_hovered);
	}
	else if (actionPressed && _hovered && !_held)
	{
		// Piles cannot be tapped, so the locked select (scooping) starts at once (StartGrab -> packet 0x1B).
		const auto source = PotInfoOf(*_hovered);
		if (source != PotInfo::_COUNT && source != PotInfo::HandWood && source != PotInfo::HandFood)
		{
			PickUp(*_hovered);
			_pickPressHeld = _held.has_value();
		}
		else if (Rocks::IsRock(*_hovered) && !Rocks::ValidForPlaceInHand(*_hovered))
		{
			// StartGrab 0x5D1740: an object that cannot be placed in the hand is tapped at once (Rock::InterfaceTap splits it)
			if (Rocks::ValidToTap(*_hovered))
			{
				Rocks::Tap(*_hovered, _interactionPoint.value_or(glm::vec3(0.0f)));
			}
			_hovered.reset();
		}
		else if (worship::InterfaceValidToTap(*_hovered, PlayerNames::PLAYER_ONE))
		{
			// the same StartGrab branch: spell icons and one-shot orbs cannot go in the hand, so they are tapped at
			// once (Worship/Worship.cpp -> SpellIcon::InterfaceTap 0x726430, OneOffSpellSeed::InterfaceTap 0x72A640)
			worship::InterfaceTap(*_hovered, PlayerNames::PLAYER_ONE);
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
	else if (actionPressed && !_held && !_hovered && !gripping && _interactionPoint && TryPickUpFish(*_interactionPoint))
	{
		// fish: the locked select starts at once, like piles
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
		// GInterface: with an object in the hand, press the action button again, move and release to put it down or
		// hurl it (state 12, 0x5D4DB0).
		_releaseArmed = true;
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
		// tap, which does nothing for most objects (Object::InterfaceValidToTap returns false) and splits rocks taller
		// than 0.7 (Rock::InterfaceTap).
		constexpr float k_PickUpHoldSeconds = 0.225f;
		_pendingPickTime += seconds;
		if (!Locator::entitiesRegistry::value().Valid(*_pendingPick))
		{
			_pendingPick.reset();
		}
		else if (!actionHeld)
		{
			if (Rocks::IsRock(*_pendingPick) && Rocks::ValidToTap(*_pendingPick))
			{
				Rocks::Tap(*_pendingPick, _interactionPoint.value_or(glm::vec3(0.0f)));
			}
			_pendingPick.reset();
		}
		else if (_pendingPickTime >= k_PickUpHoldSeconds)
		{
			const auto entity = *_pendingPick;
			_pendingPick.reset();
			if (Locator::entitiesRegistry::value().AllOf<BigForest>(entity))
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
			else
			{
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
	// The amount in the hand (0xEEA "Cantidad: %3.0f"): forced every turn while scooping from piles, fields and fish
	// farms, and shown for as long as the hand holds the food or wood (as the original looks in play; the scooping
	// code alone would drop it 12 turns after the last turn)
	if (_held && (PotInfoOf(*_held) == PotInfo::HandFood || PotInfoOf(*_held) == PotInfo::HandWood))
	{
		const auto& pot = Locator::entitiesRegistry::value().Get<const Pot>(*_held);
		_amountToolTip = static_cast<float>(pot.amount);
		_amountToolTipTime = 1.2f;
	}
	else
	{
		_amountToolTipTime = 0.0f;
	}
	UpdatePickupParticles(seconds, _pickSource.has_value() && _held.has_value() && std::getenv("OPENBLACK_NO_PICKUP_PSYS") == nullptr);
	UpdateThrown(seconds);
	// PhysicsObject::GameTurnUpdate runs with the game turns: stopped while paused, faster or slower with the game speed
	if (Game::Instance() == nullptr || !Game::Instance()->IsPaused())
	{
		const float speed = Game::Instance() != nullptr ? Game::Instance()->GetGameSpeed() : 1.0f;
		physics::PhysicsObjects::Update(speed > 0.0f ? seconds / speed : seconds);
	}
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
		// PileFood::GetHoldRadius: Get2DRadius * q, q = 1 - (1 - p)^2, p = 0.05 + 0.95 * amount / 1600 (clamped):
		// the hand opens as the food in it grows. Wood keeps a constant radius.
		ComputeHoldParameters(*_held);
		const auto& pot = Locator::entitiesRegistry::value().Get<Pot>(*_held);
		const float p = std::clamp(0.05f + 0.95f * static_cast<float>(pot.amount) / 1600.0f, 0.0f, 1.0f);
		_holdRadius *= 1.0f - (1.0f - p) * (1.0f - p);
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
	// The palm always faces the ground, except in the camera states where the root motion is the drag itself.
	// Cgrip and the hold poses animate the root bone (CAnim applies A_0 * R_0 to the root); the other poses are
	// placed by our own palm-down frame.
	const bool rootAnimated = clip == "Cgrip" || clip == "Chold_side" || clip == "Chold_above";
	_animator->SetRootLocked(!rootAnimated || _animator->IsSpecialHold());
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

