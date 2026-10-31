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

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <L3DFile.h>
#include <LNDFile.h>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "Camera/CameraHelp.h"
#include "Camera/CameraModel.h"
#include "Camera/ScriptCamera.h"
#include "Common/HelpText.h"
#include "Debug/DebugEnv.h"
#include "ECS/Abodes.h"
#include "ECS/AnimalAI.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/HandArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandClicked.h"
#include "ECS/Components/LandscapeVortex.h"
#include "ECS/Components/LeashPost.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/NotDrawn.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/HandActionPoint.h"
#include "ECS/HandLeash.h"
#include "ECS/HandPressChain.h"
#include "ECS/Influence/Influence.h"
#include "ECS/ObjectFlags.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/ObjectResources.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/Physics/ParticleCarriedObjects.h"
#include "ECS/PuzzleTotemHand.h"
#include "ECS/Registry.h"
#include "ECS/RewardOpening.h"
#include "ECS/Scaffolds.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/AbodeKnockSystemInterface.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/HandTap.h"
#include "ECS/Systems/HandToolTipRules.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/VillageTotem.h"
#include "ECS/Villager/VillagerHome.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "GameClock.h"
#include "Graphics/Texture2D.h"
#include "Hand/HandAnimationRules.h"
#include "Hand/HandGrabRules.h"
#include "Hand/HandPickUpRules.h"
#include "Hand/HandPlaceRules.h"
#include "Hand/HandStateRules.h"
#include "HandSystemDetail.h"
#include "Help/HelpProfile.h"
#include "Help/ToolTips.h"
#include "InfoConstants.h"
#include "Input/GameActionMapInterface.h"
#include "Input/GamePackets.h"
#include "Input/HandDemo.h"
#include "Input/InterfaceActive.h"
#include "Locator.h"
#include "Magic/HandHoldPose.h"
#include "Magic/Objects/MagicFireBall.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Windowing/WindowingInterface.h"
#include "Worship/Citadel.h"
#include "Worship/TempleLeash.h"
#include "Worship/Worship.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace openblack::ecs::systems::hand_detail
{
void PlaySample(audio::SoundId id)
{
	// a 2D sample with no owner, not tracked, mode 3, no loops
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

/// The landscape cell under the point does not have the water bit (off the map or without a block: not land)
bool IsLand(glm::vec3 point)
{
	return sea_cells::IsLand(point);
}
} // namespace openblack::ecs::systems::hand_detail

using namespace openblack::ecs::systems::hand_detail;

namespace
{
/// What the hand keeps for its test hooks between frames, in the debug hooks' store (Locator::debugHooks)
struct HandSystemDebugHooksState
{
	bool debugHooksRan {false}; // the environment test hooks have run (once, on the first land)
};

HandSystemDebugHooksState& HandSystemDebugHooksData()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("ecs::systems::HandSystem: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<HandSystemDebugHooksState>();
}

/// The hand's spec file, by its name in the data folder, read once through the file cache; nothing when it is not there
/// or cannot be read
std::optional<std::vector<uint8_t>> ReadHandSpec(const std::string& specName)
{
	auto& fileSystem = Locator::filesystem::value();
	const auto path = fileSystem.GetPath<filesystem::Path::Data>() / specName;
	if (!fileSystem.Exists(path))
	{
		return std::nullopt;
	}
	try
	{
		return resources::LoadBlob(Locator::resources::value().GetBlobs(), path);
	}
	catch (const std::exception&)
	{
		return std::nullopt;
	}
}
} // namespace

bool HandSystem::OfferScreenObject(uint32_t id, float depth) noexcept
{
	// while gripping a screen object every offer is accepted, the pending one unchanged; otherwise the nearer offer
	// wins, the pending depth starting above every offer when it is empty
	if (_screenGrip)
	{
		return true;
	}
	if (_screenOffer && !(_screenOfferDepth > depth))
	{
		return false;
	}
	_screenOffer = id;
	_screenOfferDepth = depth;
	return true;
}

bool HandSystem::Initialize() noexcept
{
	// one hand per player; the original's is mirrored by its left/right toggle at load. Here the right-handed option
	// mirrors it through the model's scale, off until that is checked against the original. Side::Right stays entt::null
	_hands[static_cast<size_t>(Side::Left)] =
	    HandArchetype::Create(glm::vec3(0.0f), glm::half_pi<float>(), 0.0f, glm::half_pi<float>(), 0.01f, false);
	// what the player clicks is kept on the hand, nothing to begin with
	Locator::entitiesRegistry::value().AssignState<HandClicked>(_hands[static_cast<size_t>(Side::Left)]);

	LoadAnimations();
	LoadMorphMeshes();
	RegisterPhysicsHandlers();
	RegisterTapHandlers();
	RegisterPacketHandlers();
	// the normal state's entry: the up Zoomer at (0, 1, 0)
	_up.SetPosition(glm::vec3(0.0f, 1.0f, 0.0f));
	// the tooltip builder asks the hand's state for its tooltip every turn
	help::tooltips::SetStateSubmitter([this]() { SubmitToolTips(); });
	help::tooltips::SetObjectNumbers([this](uint32_t text) { return ToolTipNumbers(text); });
	return false;
}

void HandSystem::RegisterTapHandlers() noexcept
{
	// rocks: tappable when taller than 0.7; a tap splits it in two
	hand_tap::Register(
	    &object_physics::IsRockObject,
	    [](entt::entity rock, const pot_resource::Dropper&) { return object_physics::CanTapRock(rock); },
	    [](entt::entity rock, const pot_resource::Dropper&, glm::vec3 handPos) -> uint32_t {
		    object_physics::TapRock(rock, handPos);
		    return 1;
	    });
	// abodes: all but the village centre are tappable; a tap knocks on the roof (the abode knock system), at the
	// status's point for the sound
	hand_tap::Register<Abode>(
	    [](entt::entity abode, const pot_resource::Dropper&) { return abodes::InterfaceValidToTap(abode); },
	    [](entt::entity abode, const pot_resource::Dropper& is, glm::vec3 handPos) -> uint32_t {
		    if (!Locator::abodeKnockSystem::has_value())
		    {
			    throw std::logic_error("hand: no abode knock service (Locator::abodeKnockSystem is empty)");
		    }
		    return Locator::abodeKnockSystem::value().Tap(abode, handPos, is.isMyInterface) ? 1 : 0;
	    });
	// spell icons and one-shot spell seeds
	const auto worshipValid = [](entt::entity object, const pot_resource::Dropper& is) {
		return worship::InterfaceValidToTap(object, is.player);
	};
	const auto worshipTap = [](entt::entity object, const pot_resource::Dropper& is, glm::vec3) -> uint32_t {
		return static_cast<uint32_t>(worship::InterfaceTap(object, is.player));
	};
	hand_tap::Register<SpellIcon>(worshipValid, worshipTap);
	hand_tap::Register<OneOffSpellSeed>(worshipValid, worshipTap);
	// a "Did you know?" scroll; the tapping status is the local one when it is the local interface. It is the one class
	// tapped outside the player's influence too
	hand_tap::Register<ScriptHighlight>(
	    [](entt::entity thing, const pot_resource::Dropper&) { return ecs::script_highlight::InterfaceValidToTap(thing); },
	    [](entt::entity thing, const pot_resource::Dropper& is, glm::vec3) -> uint32_t {
		    return static_cast<uint32_t>(ecs::script_highlight::InterfaceTap(thing, is.isMyInterface));
	    },
	    false);
	// a reward chest: only its own player's interface may tap it, and only until it is open; the tap opens one on the
	// land
	hand_tap::Register<Reward>(
	    [](entt::entity chest, const pot_resource::Dropper& is) {
		    const auto* given = Locator::entitiesRegistry::value().TryGet<const Reward>(chest);
		    return given != nullptr && ecs::reward::ValidToTap(*given, is.player);
	    },
	    [](entt::entity chest, const pot_resource::Dropper& is, glm::vec3) -> uint32_t {
		    if (!Locator::rewardSystem::has_value())
		    {
			    throw std::logic_error("hand: no reward service (Locator::rewardSystem is empty)");
		    }
		    return Locator::rewardSystem::value().Tap(chest, is.player) ? 1 : 0;
	    });
	// a temple's leash post: only the local player's own posts can be tapped, in the influence. The tap picks its leash,
	// or unpicks it on the picked post
	hand_tap::Register<LeashPost>(
	    [](entt::entity post, const pot_resource::Dropper&) { return worship::temple_leash::InterfaceValidToTap(post); },
	    [](entt::entity post, const pot_resource::Dropper& is, glm::vec3) -> uint32_t {
		    return worship::temple_leash::InterfaceTap(post, is.isMyInterface);
	    });
	// a magic fireball: always valid to tap, in the influence; the tap catches it (the player's own is refused), and
	// the ball goes when it is caught
	hand_tap::Register<MagicFireBall>([](entt::entity, const pot_resource::Dropper&) { return true; },
	                                  [](entt::entity ball, const pot_resource::Dropper& is, glm::vec3) -> uint32_t {
		                                  return magic::fireball::Catch(ball, is.player) != entt::null ? 1 : 0;
	                                  });
}

bool HandSystem::InInfluence() const noexcept
{
	// the interface's in-influence flag: the player's influence at the action point (interface type, allies) > 0, at
	// the map's corner when the point is cleared
	return influence::CalculatePlayerInfluence(PlayerNames::PLAYER_ONE, _interactionPoint.value_or(glm::vec3(0.0f)),
	                                           influence::CalcType::Interface) > 0.0f;
}

bool HandSystem::ValidForPlaceInHand(entt::entity object) const noexcept
{
	// An allowlist, as the original: an object never goes into the hand unless its class says it may. Buildings, totems,
	// features, animated statics, creatures, temple parts, icons, leash posts and highlights keep that default
	auto& registry = Locator::entitiesRegistry::value();
	const auto* potInfo = ecs::object::PotInfoOf(object);
	const auto rule = hand_grab::PlaceInHandRuleOf({
	    .creature = registry.AllOf<Creature>(object),
	    .villager = registry.AllOf<Villager>(object),
	    .animal = registry.AllOf<Animal>(object),
	    .scaffold = registry.AllOf<Scaffold>(object),
	    .seedOrTeleport = registry.AnyOf<SpellSeed, TeleportStone>(object),
	    .fireBall = registry.AllOf<MagicFireBall>(object),
	    .field = registry.AllOf<Field>(object),
	    .fishFarm = registry.AllOf<FishFarm>(object),
	    .pot = registry.AllOf<Pot>(object),
	    .pile = potInfo != nullptr && potInfo->potType != PotType::Pot,
	    .deadTree = registry.AllOf<DeadTree>(object),
	    .mobileStatic = registry.AllOf<MobileStatic>(object),
	    .rock = object_physics::IsRockObject(object),
	    .bonfire = object_physics::IsBonfire(object),
	    .fragment = registry.AllOf<Fragment>(object),
	    .tree = registry.AllOf<Tree>(object),
	    .bigForest = registry.AllOf<BigForest>(object),
	    .oneOffSpellSeed = registry.AllOf<OneOffSpellSeed>(object),
	    .mobile = registry.AllOf<Mobile>(object),
	});
	switch (rule)
	{
	case hand_grab::PlaceInHandRule::Always:
		return true;
	case hand_grab::PlaceInHandRule::Rock:
		// no wider than 3.6 across the ground
		return object_physics::RockValidForPlaceInHand(object);
	case hand_grab::PlaceInHandRule::Villager:
		return ecs::villager::IsReachable(object);
	case hand_grab::PlaceInHandRule::Animal:
		return ecs::animal_ai::ValidForPlaceInHand(object);
	case hand_grab::PlaceInHandRule::Field:
	{
		const auto& field = registry.Get<const Field>(object);
		return hand_grab::FieldValidForPlaceInHand(field.growth, field.food);
	}
	case hand_grab::PlaceInHandRule::FishFarm:
		// the farm's food value as food, which is its info's full stock rather than what is left in it
		return FishFarm::k_FoodValue > 0.0f;
	case hand_grab::PlaceInHandRule::Pile:
		// only a pile of the hand's own row with something in it: a stored pile, or a magic one, never
		return hand_grab::PileValidForPlaceInHand(
		    potInfo->potType, registry.Get<const Pot>(object).type,
		    ecs::object_resources::GetResource(object, ecs::object_resources::GetResourceType(object)));
	case hand_grab::PlaceInHandRule::Scaffold:
		return ecs::scaffolds::ValidForPlaceInHand(object);
	case hand_grab::PlaceInHandRule::Seed:
		// a spell seed and a magic teleport by their seed
		return SeedToPlaceInHand(object) != entt::null;
	case hand_grab::PlaceInHandRule::FireBall:
		// never the hand's own player's ball
		return magic::fireball::ValidForPlaceInHand(object, PlayerNames::PLAYER_ONE);
	case hand_grab::PlaceInHandRule::Never:
		break;
	}
	return false;
}

void HandSystem::StartGrab(entt::entity entity) noexcept
{
	// The press on a thing the hand may hold: the grab starts, or the thing is tapped instead
	if (!ValidForPlaceInHand(entity) || object_flags::IsCannotBePickedUp(entity) || !InInfluence())
	{
		// an object that cannot go into the hand (a rock too big to lift, a spell icon, the SET_ID_PICKUPABLE flag)
		// or out of the influence is tapped at once: SendTap (refused out of the influence) -> the tap packet -> the
		// object's tap (a rock splits, a spell icon's own)
		Tap(entity);
		_hovered.reset();
	}
	else if (Locator::entitiesRegistry::value().AllOf<Tree>(entity) &&
	         !(Locator::entitiesRegistry::value().AllOf<InPhysics>(entity) &&
	           !ecs::physics::particle_carried_objects::IsCarried(entity)))
	{
		// a standing tree: the grab starts as for anything else (the frame's engine sample, the turn, the interface's
		// object, grab action state), and the hand takes hold of it with a tug at once, not after the 225 ms grab
		// threshold. Pulled free, the tug puts it in the hand; let go before, the grab state ends the pull and taps it
		// when it was let go within the grab's wait
		_pendingPick = entity;
		_collideObject = Interactable(entity) ? std::optional(entity) : std::nullopt;
		_pendingPressMs = game_clock::EngineFrameSampleMs();
		_pendingPressTurn = game_clock::Turn();
		_pendingTimer = false;
		_pendingTugHold = false;
		_pendingTugStarted = false;
		_pendingTree = true;
		RenderHandPickUp(entity); // with a tug
		BeginTug(entity);
	}
	else
	{
		// the grab start (in the press's own frame): the frame's engine sample and the turn are kept, grab action
		// state. The object is free when it is in physics and not carried (InPhysics without CarriedByTornado): the
		// 225 ms timer, as a forest. A magic fireball, a field and a pile (a hand pile on the ground among them) are not
		// tuggable and have their 3D object, so they take the forest's branch too. Otherwise RenderHandPickUp holds it
		// with a tug, none for a seed. (not ported) a tuggable object without a 3D object
		auto& registry = Locator::entitiesRegistry::value();
		_pendingPick = entity;
		// the grab keeps the grabbed object as the interface's, or none when it is not interactable
		_collideObject = Interactable(entity) ? std::optional(entity) : std::nullopt;
		_pendingPressMs = game_clock::EngineFrameSampleMs();
		_pendingPressTurn = game_clock::Turn();
		const bool tuggable =
		    !registry.AnyOf<BigForest, MagicFireBall, Field>(entity) && !ecs::object_resources::IsPileResource(entity);
		const auto start = hand_grab::GrabStartOf(registry.AllOf<InPhysics>(entity) &&
		                                              !ecs::physics::particle_carried_objects::IsCarried(entity),
		                                          tuggable, registry.AllOf<SpellSeed>(entity));
		_pendingTimer = start.timer;
		_pendingTugHold = start.tugHold;
		_pendingTugStarted = false;
		_pendingTree = false;
		if (start.drawnHold)
		{
			RenderHandPickUp(entity);
		}
	}
}

bool HandSystem::SendTap(entt::entity object) noexcept
{
	// sending a tap: (in the influence || the object does not need it) && valid to tap && not flagged cannot be picked
	// up -> a tap packet, whose handler checks validity again and taps. Every ported class needs the influence but the
	// script highlight. The cannot-be-picked-up flag is SET_ID_PICKUPABLE 169's. (Tap remembers the object first)
	// a tap ends what the press started: the demo mouse's buttons go out
	_litEnded = true;
	const auto is = InterfaceStatus();
	if (!hand_tap::SendsTap(InInfluence(), hand_tap::NeedsInfluence(object), hand_tap::ValidToTap(object, is),
	                        object_flags::IsCannotBePickedUp(object)))
	{
		return false;
	}
	// the tap packet: applied at the next turn's start (HandTurn.cpp ApplyTap)
	game_packets::Push({game_packets::Type::Tap, object});
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
	std::array<std::optional<glm::vec3>, static_cast<size_t>(Side::_Count)> result {};
	for (size_t i = 0; i < hands.size(); ++i)
	{
		if (registry.Valid(hands[i]) && registry.AllOf<Transform>(hands[i]))
		{
			result[i] = registry.Get<Transform>(hands[i]).position;
		}
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
		auto animator = std::make_unique<HandAnimator>();
		if (animator->Load(resources::LoadBlob(Locator::resources::value().GetBlobs(), hbnPath), ReadHandSpec,
		                   mesh->GetBoneLocals(), mesh->GetBoneParents()))
		{
			_animator = std::move(animator);
			// a new animator has no state blend running
			_stateBlend = {};
			_stateBlendT.reset();
			LoadGeometry();
		}
		static const debug_env::Variable k_HandInfo("OPENBLACK_HAND_INFO");
		if (k_HandInfo.Get() != nullptr)
		{
			const auto& pots = Locator::infoConstants::value().pot;
			for (size_t i = 0; i < pots.size(); ++i)
			{
				const auto& p = pots[i];
				SPDLOG_LOGGER_INFO(logger,
				                   "PotInfo {}: potType={} resource={} maxInPot={} next={} initial={} perTurn={} perTurnEnd={} "
				                   "maxPick={} ramp={} mesh={}",
				                   i, static_cast<int>(p.potType), static_cast<int>(p.resourceType), p.maxAmountInPot,
				                   static_cast<int>(p.nextPotForResource), p.amountPickedUpInitially, p.amountPickedUpPerTurn,
				                   p.amountPickedUpPerTurnEnd, p.maxAmountCanBePickedUp, p.multiPickUpRampTime,
				                   static_cast<int>(p.meshId));
			}
			const auto& trees = Locator::infoConstants::value().tree;
			for (size_t i = 0; i < trees.size(); ++i)
			{
				const auto& t = trees[i];
				SPDLOG_LOGGER_INFO(
				    logger,
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
		static const debug_env::Variable k_HandAnim("OPENBLACK_HAND_ANIM");
		if (const char* forced = k_HandAnim.Get(); forced != nullptr && _animator)
		{
			_override = forced;
		}
		// Debug: OPENBLACK_HAND_DUMP=<file> writes the evaluated bone matrices of every clip for validation.
		static const debug_env::Variable k_HandDump("OPENBLACK_HAND_DUMP");
		if (const char* dump = k_HandDump.Get(); dump != nullptr && _animator)
		{
			std::ofstream out(dump);
			out << "{";
			bool firstClip = true;
			for (const auto& clip : _animator->ListClips())
			{
				// at 0 and 137 ms with no lean, and at 0 ms leaning 48 pixels sideways and -32 back and forth
				for (const auto& [t, lr, fb] : {std::tuple {0, 0.0f, 0.0f}, {137, 0.0f, 0.0f}, {0, 48.0f, -32.0f}})
				{
					const auto key = fmt::format("{}@{}@{}@{}", clip.name, t, lr, fb);
					out << (firstClip ? "" : ",") << "\n\"" << key << "\":[";
					firstClip = false;
					const bool leans = lr != 0.0f || fb != 0.0f;
					const auto mats = _animator->Evaluate(clip.name, t, leans ? std::optional(lr) : std::nullopt,
					                                      leans ? std::optional(fb) : std::nullopt);
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

void HandSystem::PrepareState() noexcept
{
	if (!_animator)
	{
		return;
	}
	// The hand's draw preparation changes its state before anything else of the frame's hand: before the hand finds
	// its point under the cursor, and before the interface's frame pass (the press, the box rule), which comes after
	// the draw. So both see the state the change left, and the change reads what the last frame's pass and this
	// frame's turns left
	// the hand's required state first, which sends the release impulse: every frame, hidden or not, before the state
	// change and its packets
	UpdateReleaseImpulse();
	const auto handMs =
	    game_clock::IsInsideCitadel() ? game_clock::FrameRealMs() : game_clock::CameraFrameMs(hand_demo::IsPlaying());
	// the CREATURE state begins only once the creature is locked for the hand, (pending) and one more field of the
	// creature it also tests; leaving it sends the feedback (HandTurn.cpp)
	if (const auto state = UpdateCreatureFrame(handMs); state != _renderHandState)
	{
		_renderHandState = state;
		// a tree pulled free is the pulling state's only until the state changes
		_tugFreed.reset();
		if (state == 9)
		{
			_playAnimMs = 0; // the play-anim state's Enter
		}
		if (state == 7)
		{
			_creatureClock.Enter(); // the creature state's Enter
			EnterCreatureDistance();
		}
		if (state == 0xA)
		{
			// the temple state's Enter: its distance zoomer at rest at the hand's last view distance, its clock at 0
			_citadelDistance.SetPosition(_viewDistance);
			_citadelClock.Enter();
		}
		// the TOTEM state's Enter pins the cursor's image where the mouse is, and its Exit lets the image go
		_cursorPin.reset();
		if (state == 5 && Locator::gameActionSystem::has_value())
		{
			_cursorPin = glm::ivec2(Locator::gameActionSystem::value().GetMousePosition());
		}
		if (state == 5)
		{
			EnterTotemViewDistance();
		}
		// the Exit, the Enter, then the state blend start. INVISIBLE's Enter copies the drawn pose into the buffer blends
		// fade from; the blend start swaps the two buffers (the pose drawn last is faded from, the next one is written
		// over the other) and fades from the hand's place as last drawn
		if (state == 0)
		{
			_handWorld.KeepDrawn();
		}
		if (state == 1)
		{
			_normalPose.Enter();
			_normalClock.Enter();
			// the normal state's entry: the hand's distance zoomer at rest at the hand's last view distance
			_handDistance =
			    hand_detail::RestartHandDistance({.zoomer = _handDistance, .started = _handDistanceStarted}, _viewDistance)
			        .zoomer;
		}
		_handWorld.Swap();
		_stateBlend.Start(
		    Locator::entitiesRegistry::value().Get<const Transform>(_hands[static_cast<size_t>(Side::Left)]).position);
	}
	// INVISIBLE (the required state for the interface's modes 1 / 2 / 12 / 25) -> its Enter hides the hand: not drawn,
	// and its shadow off; its Exit shows it again (the bones kept for the blend back). The draw preparation does nothing
	// more while hidden, while a box has the mouse or with the interface inactive: no mouse smoothing, no state Update,
	// no blend, no pose (Update and Place return). The box is the one the last frame's interface pass found, as the draw
	// comes before this frame's pass. (not ported) the system cursor: openblack has none
	// Only the INVISIBLE state hides (Enter -> Hide); a box or an inactive interface alone only stops the update, and the
	// hand stays drawn as last placed (inside the citadel the required state is CITADEL first, and the hand stays drawn)
	// The same frame as the original: the required state reads the turn's interface hand state there too, and the
	// Enter's hide comes before that frame's draw, as NotDrawn here before RenderingSystem::PrepareDraw (Game.cpp's
	// order)
	_preparationStopped =
	    hand_detail::DrawPreparationStops(_renderHandState, interface_active::IsActive(), interface_active::BoxHasMouse());
	auto& registry = Locator::entitiesRegistry::value();
	const auto hand = _hands[static_cast<size_t>(Side::Left)];
	const bool invisible = _renderHandState == 0;
	if (invisible && !registry.AllOf<NotDrawn>(hand))
	{
		registry.Assign<NotDrawn>(hand);
		registry.SetDirty();
	}
	else if (!invisible && registry.AllOf<NotDrawn>(hand))
	{
		registry.Remove<NotDrawn>(hand);
		registry.SetDirty();
	}
	// The held object is drawn by the hand's state, with the interface active and no box holding the mouse, and only by
	// the tug, holding, multiple pick-up and grain states: otherwise it is not drawn, wherever it is. The box is the one
	// the last frame's interface pass found, as the draw comes before this frame's pass
	const auto held = GetHeldObject();
	const bool heldDrawn =
	    hand_detail::DrawsHeldObject(_renderHandState, interface_active::IsActive(), interface_active::BoxHasMouse());
	const auto hide = held.has_value() && !heldDrawn ? held : std::nullopt;
	if (_heldHiddenByState != hide)
	{
		if (_heldHiddenByState.has_value() && registry.Valid(*_heldHiddenByState) &&
		    registry.AllOf<NotDrawn>(*_heldHiddenByState))
		{
			registry.Remove<NotDrawn>(*_heldHiddenByState);
		}
		_heldHiddenByState.reset();
		if (hide.has_value() && !registry.AllOf<NotDrawn>(*hide))
		{
			registry.Assign<NotDrawn>(*hide);
			_heldHiddenByState = hide;
		}
		registry.SetDirty();
	}
}

std::optional<GrabReadout> HandSystem::GetGrabReadout() const noexcept
{
	if (!_pendingPick)
	{
		return std::nullopt;
	}
	const auto now = game_clock::EngineFrameSampleMs();
	const auto elapsed = hand_grab::ElapsedMs(now, _pendingPressMs, game_clock::Turn(), _pendingPressTurn);
	return GrabReadout {.object = *_pendingPick, .elapsedMs = elapsed, .tugHold = _pendingTugHold, .tree = _pendingTree};
}

std::optional<entt::entity> HandSystem::GetCursorObject() const noexcept
{
	// a destroyed object is no longer the interface's: the game lets go of an object the hand can no longer interact
	// with before it is freed
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	return ecs::hand_action_point::ExistingObject(_renderCollideObject, Locator::entitiesRegistry::value());
}

void HandSystem::Update(std::chrono::microseconds dt, glm::vec2 mouseDelta, bool gripping, bool actionHeld) noexcept
{
	// the action button held, as the interface keeps it for its hand state
	_actionHeld = actionHeld;
	// the interface's kept object of the last frame becomes the one the frame is drawn with, before it is looked for again
	_renderCollideObject = _collideObject;
	if (!_animator)
	{
		return;
	}
	// what the demo mouse's buttons read before this frame's presses: was the hand busy with an earlier press, gripping
	// the land, waiting to pick something up
	const bool litBusy =
	    _pendingPick.has_value() || _screenGrip || _seedAction != SeedAction::None || _giving.givenTo != entt::null;
	const bool litLandGrip = _gripPoint.has_value();
	const bool litWaiting = _pendingPick.has_value();
	const bool litActionWasHeld = _actionWasHeld;
	_litEnded = false;
	const float seconds = static_cast<float>(dt.count()) / 1e6f;
	// the hand's draw preparation runs before its state machine: the good / evil morph
	UpdateAlignmentMorph(std::nullopt, false);

	if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f)
	{
		// the hand-move help event, from a mouse move with a pixel delta
		help_profile::Trigger(help_profile::Event::HandMove);
	}
	// mouse smoothing: smooth += dt * vel, clamped to +-80 px of the mouse, vel = (vel + (mouse - smooth) * 20 dt)
	// * 0.03^dt. The mouse is in the game's pixels (ints): the window's pixels here, so the lean depends on the
	// resolution as in the original
	_mouse += mouseDelta; // (approximate) the sum of the deltas stands for the mouse's absolute position
	if (_preparationStopped)
	{
		// the draw preparation stops after the state switch while hidden, under a box or with the interface inactive: the
		// smoothing does not run
	}
	else if (_smoothMouseValid && seconds > 0.0f)
	{
		const hand_place::SmoothMouse previous {.position = _smoothMouse, .velocity = _smoothMouseVelocity};
		const auto smooth = hand_place::StepSmoothMouse(previous, _mouse, seconds);
		_smoothMouse = smooth.position;
		_smoothMouseVelocity = smooth.velocity;
	}
	else
	{
		_smoothMouse = _mouse;
		_smoothMouseVelocity = glm::vec2(0.0f);
		_smoothMouseValid = true;
	}
	// The L lean poses: the _lr pose at (lag + 80) / 160 x its duration (truncated) with lag = clamp(smoothed x - mouse x,
	// -80, 80), only when |lag| > 0.0001; the _fb pose the same with clamp(mouse y - smoothed y), only when |smoothed y|
	// > 0.0001; each the difference from its middle frame ((count - 1) >> 1). In the camera state the sideways lag is the
	// other way (mouse x - smoothed x); (inferred) openblack's land grip stands for that state (the CAMERA state is
	// (pending)). The lean is handed to the animator with the clip, below
	const auto lean = hand_place::LeanMotion(_mouse, _smoothMouse, gripping);
	_leanSideways = lean.sideways;
	_leanForwards = lean.forwards;

	// Test hooks driven by environment variables, once the landscape exists (HandDebugHooks.cpp).
	if (auto& ran = HandSystemDebugHooksData().debugHooksRan; !ran && Locator::terrainSystem::has_value())
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
	// a press through PressSeedAction holds the action until ReleaseSeedAction
	actionHeld = actionHeld || _drivenActionHeld;
	// OPENBLACK_TEST_CAST: the synthetic presses of the action button (HandSpellSeed.cpp)
	actionHeld = TestCastActionHeld(seconds, actionHeld);

	// Pick up / drop with the action button (right). Only while not gripping the land.
	// a destroyed held object (openblack destroys at once while the deferred deletion is off) leaves the hand here; a
	// deleted one (Unavailable) at the turn's hand validation. (pending) the hand's game turn update also drops it the
	// same turn (not available -> thrown), for a deletion after step I
	if (_held && !Locator::entitiesRegistry::value().Valid(*_held))
	{
		// a seed among them still leaves its type as the last seed type
		RecordGoneSeedType(*_held);
		_held.reset();
	}
	// a hand demo's action click lands on the recorded kind of thing near the recorded place, before the screen object
	// and the press see the object under the cursor (the debug bar's switch leaves the click where the cursor is)
	if (const auto click = hand_demo::TakeClick();
	    click && !(Locator::config::has_value() && Locator::config::value().handDemoNoRetarget))
	{
		RetargetDemoClick(*click);
	}
	// the pending screen object becomes this frame's; then, unless gripping, the pending one is cleared for this frame's
	// offers. Before that, the offer is arbitrated (skipped while gripping): the object under the cursor against the
	// bubble, the nearer wins, then a land hit nearer than the bubble (the land's distance + 2.3) drops it. The bubble's
	// depth is its camera z, compared as it is with the ray distances, as the original does. (not ported) the second
	// land test
	bool screenWins = false;
	if (_screenOffer && !_screenGrip)
	{
		const bool objectNearer = _cursorObjectDistance && !(_screenOfferDepth < *_cursorObjectDistance);
		const bool landNearer = _cursorLandDistance && *_cursorLandDistance + 2.3f < _screenOfferDepth;
		if (objectNearer || landNearer)
		{
			_screenOffer.reset();
		}
		else
		{
			screenWins = true;
		}
	}
	_screenObject = _screenOffer;
	_screenObjectDepth = _screenOfferDepth;
	if (!_screenGrip)
	{
		_screenOffer.reset();
	}
	if (screenWins || _screenGrip)
	{
		_cursorObject.reset(); // the bubble is the collide, not the object
	}
	// the tied leash's rope under the hand (the interface's second land test): picked like the object and the screen
	// object, only with no action in progress, it takes the pick from them or from the land when it is nearer
	if (!_leashPull && !_held && !_tug && !ActionInProgress(gripping))
	{
		static_cast<void>(RopeTakesThePick());
	}
	else
	{
		_ropeUnderHand = false;
	}
	_hovered = (_held || _tug || gripping || _screenObject) ? std::nullopt : FindObjectUnderHand();
	// while a hand demo plays, a press that took nothing at the last pass is pressed again, with what is under the hand
	// now
	const bool demoPlaying = hand_demo::IsPlaying();
	bool actionPressed =
	    actionHeld && (!_actionWasHeld || hand_demo::PressTakenAgain(demoPlaying, _demoPressPending, actionHeld));
	// The interface's fight state takes the presses of both buttons, the grab's first, paused or not, and leaves nothing
	// of them to the rest of the hand; a press held on a body waits only for its own button to be let go
	const bool grabPressed = gripping && !_gripWasHeld;
	_gripWasHeld = gripping;
	std::optional<hand_fight::Button> fightButton;
	if (_fightPress.has_value() || InFightState())
	{
		if (!_fightPress.has_value() && grabPressed)
		{
			fightButton = hand_fight::Button::Grab;
		}
		else if (!_fightPress.has_value() && actionPressed)
		{
			fightButton = hand_fight::Button::Action;
		}
		actionPressed = false;
		_actionDoubleClick = false;
	}
	// while the game is paused the idle hand takes only the land grip: the action press and its double click are lost
	const bool takesPress = hand_press::IdleTakesActionPress(Locator::time::has_value() && game_clock::IsPaused());
	if (!takesPress)
	{
		actionPressed = false;
	}
	// the action state the feed of a fireball left: the press is not taken (nor a double click), and the release ends
	// it, so a click made in it is lost
	if (_applyObjectState)
	{
		if (!actionHeld && _actionWasHeld)
		{
			_applyObjectState = false;
		}
		actionPressed = false;
		_actionDoubleClick = false;
	}
	// the Action button's double click, taken once a frame by the idle hand before anything else of the press: the
	// leash's tie in place of the action press, which it ends. A busy hand lets it go
	if (std::exchange(_actionDoubleClick, false) && takesPress &&
	    ecs::hand_leash::TakesDoubleClick(
	        {.gripping = gripping || _gripPoint.has_value(),
	         .bubbleGrip = _screenGrip,
	         .tugging = _tug.has_value() || (_pendingPick.has_value() && _pendingTugHold),
	         .grabbing = _pendingPick.has_value() || _leashPull.has_value(),
	         .lockedSelect = _pickPressHeld || _pickSource.has_value() || _creatureLock.has_value(),
	         .releaseArmed = _releaseArmed,
	         .seedApply = _seedAction != SeedAction::None}))
	{
		LeashDoubleClick();
		actionPressed = false;
	}
	// the land grip as this frame has it, for a demo's press taken again at the turn
	_pressGripping = gripping;
	// the press's search near the action's point, and its trace
	SearchNearForPress(actionPressed, gripping);
	// the creature under the hand this frame, as the hover and the press see it (Creature Mode reads it)
	if (Locator::creatureHandSystem::has_value())
	{
		const auto& registry = std::as_const(Locator::entitiesRegistry::value());
		const bool creature = _cursorObject && ecs::IsAvailable(*_cursorObject) && registry.AllOf<Creature>(*_cursorObject);
		Locator::creatureHandSystem::value().SetCreatureUnderHand(creature ? _cursorObject : std::nullopt);
	}
	// the interface keeps the object it had while an action is in progress (the land grip among them); otherwise it takes
	// the frame's
	_collideObject = ecs::hand_action_point::CollideObject(_collideObject, _cursorObject, ActionInProgress(gripping));
	// an object the hand can no longer interact with (gone, deleted, an abode not yet started) ends the interface's
	// action, which clears both of its objects. The interface checks it with or without an action in progress; with
	// none, the game's pick only takes objects the hand can interact with, so only a kept object or the press's object
	// near the action's point can fail it. (not ported) the action itself is not ended here, and the unavailable for a
	// state change flag is not tested
	if (_collideObject.has_value() && !Interactable(*_collideObject))
	{
		_collideObject.reset();
		_renderCollideObject.reset();
	}
	// the fight's press, on the object the interface keeps or on the ground; then the release of a press held on a body,
	// by its own button or by the interface no longer acting
	if (fightButton.has_value())
	{
		FightPress(*fightButton);
	}
	if (_fightPress.has_value() &&
	    ((_fightPress->button == hand_fight::Button::Grab ? !gripping : !actionHeld) || !interface_active::IsActive()))
	{
		FightRelease();
	}
	const bool actionReleased = !actionHeld && _actionWasHeld;
	_actionWasHeld = actionHeld;
	// an empty hand pressed on a screen object -> the bubble grip action state; the release clears it and resets the
	// action state. (approximate) the original clears the whole flag word; only its grip bit is kept here
	if (_screenGrip && actionReleased)
	{
		_screenGrip = false;
	}
	const bool screenPress = StartScreenPress(actionPressed, gripping);
	// before the action states: the last thing tapped or clicked
	UpdateTapMemory(actionReleased);
	// the press takes its branch; a demo's press that took nothing stays pending while the button is held, through a
	// pause too, and anything else ends it
	const bool pressTookNothing = TakeActionPress(actionPressed, gripping, screenPress);
	_demoPressPending = hand_demo::PressPendingAfterPass(demoPlaying, _demoPressPending, takesPress, actionHeld, actionPressed,
	                                                     pressTookNothing);
	// the interface's in-influence flag, for the grab state below
	const auto TapInInfluence = [this]() { return InInfluence(); };
	// the seed's apply states 8..11, and what the gesture system is told about the hand (HandSpellSeed.cpp)
	UpdateSeedAction(actionHeld);
	UpdateSeedInHand(actionHeld);
	if (actionReleased && _pickPressHeld)
	{
		// Releasing the press that picked it up ends the grab / scooping (the end locked select packet); the
		// object stays in the hand.
		_pickPressHeld = false;
		SendEndLockedSelect();
	}
	else if (actionReleased && _held && _releaseArmed)
	{
		_releaseArmed = false;
		// The release sends the holding spring's velocity (units per second, capped at 124) and every release takes the
		// same path: the physics from the hand decides thrown (vel.x^2 + vel.z^2 > 4) or put down, a hand pot |v|^2 <= 5
		// (HandHolding.cpp). Sent as two packets, applied at the next turn's start (HandTurn.cpp)
		SendRelease(_handVelocity);
	}
	// giving what the hand holds to a creature: the offer is kept up or runs down
	UpdateGiving();
	// the creature's locked select: held once the turn locks it, let go at the release
	UpdateCreatureLock(actionReleased);
	// the tied leash's rope pulled: let go, its length and the creature's order
	UpdateLeashPull(actionReleased);
	if (_pendingPick)
	{
		// The grab state runs before the hand's draw, so it reads the tug hold as the frame before left it: a hold that
		// ends this frame takes the thing into the hand only at the next
		const bool holdOverBefore = !_pendingTugHold;
		// The tug state's update for the tug hold, in the draw: the TUG state change of the first draw after the press
		// zeroes the state blend with firstFrame set; each Update, firstFrame is cleared once the blend >= 0.13, and the
		// Update after clears the tug hold; then the blend += dt while < 0.13, dt = the camera time step x 0.001 (the
		// real frame time inside the citadel). (not ported) the blend does not advance while the hand is hidden (the
		// draw preparation's early return)
		if (_pendingTugHold)
		{
			if (!_pendingTugStarted)
			{
				_pendingTugStarted = true;
				_pendingTugFirst = true;
				_pendingTugBlend = 0.0f;
			}
			else if (_pendingTugFirst)
			{
				_pendingTugFirst = !(_pendingTugBlend >= 0.13f);
			}
			else
			{
				_pendingTugHold = false;
			}
			if (_pendingTugBlend < 0.13f)
			{
				const auto ms = game_clock::IsInsideCitadel() ? game_clock::FrameRealMs()
				                                              : game_clock::CameraFrameMs(hand_demo::IsPlaying());
				_pendingTugBlend += static_cast<float>(ms) * 0.001f;
			}
		}
		// The grab state, every frame (and every turn): elapsed = min(sample - press sample, (turn + 1) * 100 - press turn *
		// 100); first the pick-up (the timer path at elapsed >= 225 ms; else once the tug hold was over the frame before; a
		// standing tree never, its pull takes it), then the release (every object): stop the immersion, throw it when the
		// hand holds it, and a tap only if elapsed <= 225 ms. A tap does nothing for most objects, splits rocks taller than
		// 0.7, puts a one-shot orb's charged seed in the hand and, with the leash in the hand, sends the creature to it. A
		// tree let go ends its pull (UpdateTug)
		const uint32_t elapsed =
		    hand_grab::ElapsedMs(game_clock::EngineFrameSampleMs(), _pendingPressMs, game_clock::Turn(), _pendingPressTurn);
		const bool pickNow = hand_grab::GrabPicksUp(_pendingTimer, elapsed, holdOverBefore, _pendingTree);
		if (!ecs::IsAvailable(*_pendingPick))
		{
			_pendingPick.reset();
			_pendingTree = false;
		}
		else if (!pickNow && !actionHeld)
		{
			RenderHandRelease();
			const bool taps = _pendingTree ? hand_grab::TreeRelease(elapsed, false) == hand_grab::TreeLetGo::Tap
			                               : elapsed <= hand_grab::k_GrabWaitMs;
			if (taps)
			{
				Tap(*_pendingPick);
			}
			_pendingPick.reset();
			_pendingTree = false;
		}
		else if (pickNow)
		{
			const auto entity = *_pendingPick;
			_pendingPick.reset();
			if (!hand_pick_up::GrabTakes(TapInInfluence(), object_flags::IsCannotBePickedUp(entity)))
			{
				// the generic pick-up of an object that cannot be picked up or out of the influence fails: the grab state
				// resets the action, nothing is picked up, and the hand lets go of the thing it drew from the press, as a
				// release does
				RenderHandRelease();
			}
			else if (Locator::entitiesRegistry::value().AllOf<BigForest>(entity))
			{
				// not tuggable: the grab start sets the hold and the timer; after 225 ms the generic pick-up sends the place
				// in hand packet with the forest; its tree is made in the handler
				SendPlaceInHand(entity);
			}
			else if (Locator::entitiesRegistry::value().AllOf<Tree>(entity) &&
			         !(Locator::entitiesRegistry::value().AllOf<InPhysics>(entity) &&
			           !ecs::physics::particle_carried_objects::IsCarried(entity)))
			{
				// a standing tree is tugged; one in physics (thrown, not landed yet) is caught like any flying object, unless
				// a particle system carries it (not free)
				BeginTug(entity);
			}
			else if (SendSeedOrStonePickup(entity, true))
			{
				// a spell seed over its spell or a teleport stone: the generic pick-up -> the place in hand packet with the
				// object (ApplyPlaceInHand: the seed in the hand and its spell closed)
			}
			else
			{
				// anything else held for 225 ms (a one-shot orb among them) is picked up itself: the generic pick-up, the
				// place in hand packet. (not verified) a release during the wait (the wait-pickup state only watches the
				// object's flag): here it clears _pickPressHeld
				SendPlaceInHand(entity);
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
	// the locked select's particles and looping sound: from the start locked select packet's handler to its end (at
	// once)
	const bool multiPickup = _pickSource.has_value() && _held.has_value() && !_lockedSelectStopped;
	UpdatePickupSound(multiPickup);
	// the end of the interface's action processing (all its exits, after the power-up system): the interface hand state.
	// The power-up system itself runs on both paths, the turn's (the last frame's ms) and the frame's (this frame's):
	// hand_casting::ProcessTurn / Update. (not ported) the extra call per queued action and the playback path
	UpdateInterfaceHandState();
	// Once per game turn: the amount in the hand (the "amount" tooltip, forced), forced every turn of the scooping
	// (piles, fields, fish farms) and once when the locked select ends; its lifetime keeps it about 13 turns after that.
	// (The help system's turn is a step of the game logic loop)
	if (_toolTipTurn != game_clock::Turn())
	{
		_toolTipTurn = game_clock::Turn();
		const bool scooping = _pickSource.has_value() && _held.has_value();
		const bool handful = _held && (PotInfoOf(*_held) == PotInfo::HandFood || PotInfoOf(*_held) == PotInfo::HandWood);
		if (hand_tooltips::ShowsAmountInHand(scooping, _toolTipScooping, handful))
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
	static const debug_env::Variable k_NoPickupParticles("OPENBLACK_NO_PICKUP_PSYS");
	UpdatePickupParticles(seconds, multiPickup && k_NoPickupParticles.Get() == nullptr);
	UpdateTestSplash(seconds);
	UpdateTestAbode(seconds);
	UpdateRootsAndPiles(seconds);
	// the piles rising or sinking to their amount
	if (Locator::miracleFxSystem::has_value())
	{
		Locator::miracleFxSystem::value().UpdatePiles(seconds);
	}

	// Gameplay state machine (original hand states): holding > gripping > can pick up > idle.
	// The normal state's clip (hndspec5.txt's order): Cwiggle at the entry, Chold_fingers on the tied leash's rope or
	// pulling it; the object under the cursor (none if a LandscapeVortex), not the held one and with a 3D
	// object ((approximate) here a Mesh component, which fields and fish farms, drawn their own way, lack, or a temple's
	// entrance, whose 3D object is only collided), Ccan_pickup when the interface hand state is 9 (Can Pick up), else
	// Cstroke; later and on its own, a SpellIcon Ccan_pickup.
	// With no object under it (none the hand strokes, nor a spell icon, nor a screen object, nor the tied leash's
	// rope), the hand at the edge of the screen shows turning or tilting the camera from the camera's hints
	// (hand_navigation_pose::WhileHovering), the turning standing its up towards the camera's focus. While a hand demo
	// plays, its recorded hints are shown. (undetermined) not while the player's camera controls are not read (a script
	// camera, a miracle's camera path): the camera's hints are not worked out then. (not ported) the turning pose the
	// camera's edge turn asks for with its ring cursor
	// The camera state (the empty hand dragging the land) takes its pose from the camera's hints: the grip once the
	// drag pans the land, else turning, zooming or tilting the camera, else the wiggle (hand_navigation_pose)
	const bool cameraState = gripping && !_held;
	if (!cameraState)
	{
		_cameraHand.reset();
	}
	auto& clipRegistry = Locator::entitiesRegistry::value();
	// (openblack guard) Valid: the original's pointer is never a deleted object's
	const bool cursorValid = _cursorObject && clipRegistry.Valid(*_cursorObject);
	const bool overSpellIcon = cursorValid && clipRegistry.AllOf<SpellIcon>(*_cursorObject);
	const bool onLeashRope = _ropeUnderHand || _leashPull.has_value();
	std::string clip(hand_animation::EntryClip(onLeashRope));
	_hoverPose = HoverPoseNow(cameraState);
	if (cursorValid && !clipRegistry.AllOf<LandscapeVortex>(*_cursorObject) && !(_held && *_held == *_cursorObject) &&
	    clipRegistry.AnyOf<Mesh, CitadelEntrance>(*_cursorObject))
	{
		clip = hand_animation::OverObject(_interfaceHandState);
	}
	else
	{
		if (_hoverPose)
		{
			const std::string poseClip(hand_navigation_pose::ClipName(*_hoverPose));
			if (_animator->Has(poseClip))
			{
				clip = poseClip;
			}
		}
	}
	if (overSpellIcon)
	{
		clip = hand_animation::k_CanPickUp;
	}
	std::optional<std::string> cameraClip;
	if (cameraState)
	{
		auto& cameraHand = EnterCameraHand();
		cameraHand.pose = NavigationPose();
		const std::string poseClip(hand_navigation_pose::ClipName(cameraHand.pose));
		cameraClip = _animator->Has(poseClip) ? poseClip : "Cgrip";
		clip = *cameraClip;
	}
	else if (gripping)
	{
		clip = "Cgrip"; // the camera state's own clip
	}
	// The holding state's update, and the tug's: the pose and its frame (in whole ms, cut towards zero) per hold type.
	//   ABOVE: Chold_above at dur * 0.5 * (1 - grip), grip = min(1, R / (3.2 s * 1.2))
	//   TREE / SIDE / VILLAGER: Chold_side at (dur >> 1) * grip, grip = min(1, R / (3.2 s))
	// The hand's state (the required state: held from the press, before the place in hand packet)
	const bool holding = (RenderHeld().has_value() || _tug.has_value()) && _override.empty() && _holdType != HoldType::None;
	if (const auto held = RenderHeld();
	    held && Locator::entitiesRegistry::value().Valid(*held) && Locator::entitiesRegistry::value().AllOf<Transform>(*held))
	{
		// every drawn frame the holding state asks the thing in the hand again for its hold radius, its lowering and
		// its height, so a thing that changes changes the grip and the hang (a food pile's radius is its Get2DRadius x
		// GetProportionRaised: the hand opens as the food in it grows). The hold type and whether it is rooted are
		// asked once a turn (GameTurnUpdate) and at the pick-up. The aim of the next frame reads the hang worked out
		// here, as the original's aim reads the one its last frame left
		RefreshHoldGeometry(*held);
	}
	const bool hasWiggle = _animator->Has("Cwiggle");
	const bool hasAbove = _animator->Has("Chold_above");
	const bool hasSide = _animator->Has("Chold_side");
	const auto holdClip = hand_place::HoldClipOf(holding, _holdType, {.wiggle = hasWiggle, .above = hasAbove, .side = hasSide});
	if (holdClip == hand_place::HoldClip::Wiggle)
	{
		// MAGIC (a spell seed until it is ready): Cwiggle held at half its length
		clip = "Cwiggle";
		_animator->SetTime(static_cast<float>(
		    magic::hand_hold::HoldTimeMs(HoldType::Magic, _animator->GetDurationMs(clip), _holdRadius, _handScale)));
	}
	else if (holdClip == hand_place::HoldClip::Above)
	{
		clip = "Chold_above";
		// pulling it free (the tug state), the hand opens by the reach against its own height alone
		const auto durationMs = _animator->GetDurationMs(clip);
		_animator->SetTime(
		    static_cast<float>(_tug ? magic::hand_hold::TugTimeMs(HoldType::Above, durationMs, _holdRadius, _handScale)
		                            : magic::hand_hold::HoldTimeMs(HoldType::Above, durationMs, _holdRadius, _handScale)));
	}
	else if (holdClip == hand_place::HoldClip::Side)
	{
		clip = "Chold_side";
		// every hold but ABOVE that reaches here takes the side pose's time (a MAGIC one only without Cwiggle)
		_animator->SetTime(static_cast<float>(
		    magic::hand_hold::HoldTimeMs(HoldType::Side, _animator->GetDurationMs(clip), _holdRadius, _handScale)));
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
	// The original blends at a change of the hand's state (Exit, Enter, the state blend start), 0.13 s, linear, and
	// the normal state at a change of its pose (below); the animator itself never blends
	_animator->SetSpecialHold(std::nullopt, std::chrono::milliseconds(0));
	// the hand's dt: the camera time step, the real frame time inside the citadel. Outside a hand demo's playback the
	// camera time step is the real frame time too, as the original's: the creature's click is timed in it
	const auto handMs =
	    game_clock::IsInsideCitadel() ? game_clock::FrameRealMs() : game_clock::CameraFrameMs(hand_demo::IsPlaying());
	// the state change ran first this frame (PrepareState). The draw preparation does nothing more while hidden, while a
	// box has the mouse or with the interface inactive: no state Update, no blend, no pose
	if (_preparationStopped)
	{
		return;
	}
	// the TOTEM state's update, every drawn frame: the mouse's move up the screen since the last frame slides the village
	// totem the hand holds. The mouse is in the window's pixels, down the screen, against the window's height. Then the
	// hand takes its side hold on the icon, closed by the icon's hold radius; its place and tilt are the placement's
	if (_renderHandState == 5 && _lockedSelectAction && mouseDelta.y != 0.0f && Locator::windowing::has_value() &&
	    ecs::puzzle_totem_hand::IsPuzzleTotem(*_lockedSelectAction))
	{
		// a puzzle totem: the same move drags it in whole steps
		ecs::puzzle_totem_hand::Slide(*_lockedSelectAction, -mouseDelta.y,
		                              static_cast<float>(Locator::windowing::value().GetSize().y));
	}
	else if (_renderHandState == 5 && _lockedSelectAction && mouseDelta.y != 0.0f && Locator::villageTotemSystem::has_value() &&
	         Locator::windowing::has_value())
	{
		Locator::villageTotemSystem::value().Slide(*_lockedSelectAction, -mouseDelta.y,
		                                           static_cast<float>(Locator::windowing::value().GetSize().y));
	}
	if (_renderHandState == 5 && _lockedSelectAction && _override.empty() && _animator->Has("Chold_side") &&
	    (Locator::villageTotemSystem::has_value() || ecs::puzzle_totem_hand::IsPuzzleTotem(*_lockedSelectAction)))
	{
		const auto hold = ecs::puzzle_totem_hand::IsPuzzleTotem(*_lockedSelectAction)
		                      ? ecs::puzzle_totem_hand::GetHandHold(*_lockedSelectAction)
		                      : Locator::villageTotemSystem::value().GetHandHold(*_lockedSelectAction);
		if (hold)
		{
			// the cursor's image is pinned on the hand's point while that point is in front of the eye
			if (Locator::camera::has_value() && Locator::windowing::has_value())
			{
				const auto clipPoint = Locator::camera::value().GetViewProjectionMatrix() * glm::vec4(hold->position, 1.0f);
				if (clipPoint.w > 0.0f)
				{
					const auto size = Locator::windowing::value().GetSize();
					const auto onScreen = glm::vec2((clipPoint.x / clipPoint.w + 1.0f) * 0.5f * static_cast<float>(size.x),
					                                (1.0f - clipPoint.y / clipPoint.w) * 0.5f * static_cast<float>(size.y));
					_cursorPin = ecs::village_totem::CursorPin(onScreen, size);
				}
			}
			clip = "Chold_side";
			_animator->SetTime(static_cast<float>(ecs::village_totem::GripTimeMs(
			    ecs::village_totem::GripClosure(hold->holdRadius), _animator->GetDurationMs(clip))));
		}
	}
	// held to a creature: the hand strokes and slaps it, and rests on its body
	_creaturePose.reset();
	if (_renderHandState == 7 && Locator::creatureHandSystem::has_value() &&
	    Locator::creatureHandSystem::value().GetCreature().has_value())
	{
		// the slap's gate watches the cursor cross the screen's middle column
		const auto screenWidth =
		    Locator::windowing::has_value() ? static_cast<float>(Locator::windowing::value().GetSize().x) : 0.0f;
		_creaturePose = Locator::creatureHandSystem::value().Update(_mouseRayOrigin, _mouseRayDirection, _mouse, screenWidth,
		                                                            static_cast<float>(handMs) * 0.001f);
		// the state's own distance zoomer gives the hand its view distance and scale, and the place it is drawn at
		StepCreatureDistance(static_cast<float>(handMs) * 0.001f);
		// each change of the state's cycle to the slap or the caress swaps the pose buffers and starts the state blend
		// afresh from the hand's last place, before the pose is written, as a change of state does; turning back to the
		// wiggle beside the body does not
		for (uint8_t start = 0; _creaturePose && start < _creaturePose->blendStarts; ++start)
		{
			_handWorld.Swap();
			_stateBlend.Start(clipRegistry.Get<const Transform>(_hands[static_cast<size_t>(Side::Left)]).position);
		}
		// the CREATURE state's own cycle: wiggling beside the body, caressing it, or slapping, shown at the state's own
		// clock, which goes on by the frame's real ms from its Enter and wraps at the length of the cycle shown, so a
		// change of cycle does not restart it
		if (_creaturePose)
		{
			const char* cycleClip = "Cwiggle";
			switch (_creaturePose->cycle)
			{
			case creature_feedback::TouchCycle::Wiggle:
				cycleClip = "Cwiggle";
				break;
			case creature_feedback::TouchCycle::Tickle:
				cycleClip = "Ctickle";
				break;
			case creature_feedback::TouchCycle::Slap:
				cycleClip = "Cslap";
				break;
			}
			if (_animator->Has(cycleClip))
			{
				const auto clockMs = _creatureClock.Advance(game_clock::FrameRealMs(), _animator->GetDurationMs(cycleClip));
				if (_override.empty())
				{
					clip = cycleClip;
					_animator->SetTime(static_cast<float>(clockMs));
				}
			}
		}
	}
	// the temple state's update: its own distance zoomer places and sizes the hand
	if (_renderHandState == 0xA)
	{
		StepCitadelDistance(static_cast<float>(handMs) * 0.001f);
	}
	// The play-anim state's update: the clip at the time since Enter, which then goes on by the real frame ms; at the clip's
	// length the flag goes and the hand is back to NORMAL on the next frame
	if (_renderHandState == 9 && _playAnim && _override.empty() && _animator->Has(_playAnimClip))
	{
		clip = _playAnimClip;
		_animator->SetTime(static_cast<float>(_playAnimMs));
		_playAnimMs += game_clock::FrameRealMs();
		if (_playAnimMs >= _animator->GetDurationMs(clip))
		{
			_playAnim = false;
		}
	}
	// the camera state's animation goes on by the frame's real ms, wrapped at the length of the pose it shows, from 0
	// at the state's start and through its changes of pose
	if (cameraClip && clip == *cameraClip)
	{
		const auto durationMs = _animator->GetDurationMs(clip);
		_cameraHand->animationMs = durationMs > 0 ? (_cameraHand->animationMs + game_clock::FrameRealMs()) % durationMs : 0;
		_animator->SetTime(static_cast<float>(_cameraHand->animationMs));
	}
	// The temple state's update: the wiggle, whatever is under the hand, at the state's own clock, on by the frame's
	// real ms and wrapped at the wiggle's length
	if (_renderHandState == hand_state::k_Citadel && _override.empty() && _animator->Has(hand_animation::k_Wiggle))
	{
		clip = std::string(hand_animation::k_Wiggle);
		const auto ms = _citadelClock.Advance(game_clock::FrameRealMs(), _animator->GetDurationMs(clip));
		_animator->SetTime(static_cast<float>(ms));
	}
	// The normal state's update: its clock goes on by the frame's real ms, wrapped at the length of the pose it shows,
	// and runs on through its changes of pose (the fingers over the leash's rope at their own time); another pose than
	// the one it showed last (not its first after its Enter) swaps the pose buffers and starts the state blend afresh
	// from the hand's last place, before the pose is written. The camera state is not one of openblack's states yet
	// (the empty hand dragging the land stands for it), so the normal state is entered afresh after it
	if (_renderHandState == 1)
	{
		if (cameraClip)
		{
			_normalPose.Enter();
			_normalClock.Enter();
		}
		else
		{
			const auto durationMs = _animator->GetDurationMs(clip);
			const auto ms = hand_animation::NormalPoseTime(_normalClock.Advance(game_clock::FrameRealMs(), durationMs), clip,
			                                               durationMs, _leashPull.has_value());
			_normalClock.Set(ms);
			_animator->SetTime(static_cast<float>(ms));
			if (_normalPose.Change(clip))
			{
				_handWorld.Swap();
				_stateBlend.Start(clipRegistry.Get<const Transform>(_hands[static_cast<size_t>(Side::Left)]).position);
			}
		}
	}
	if (clip != _animator->GetCurrentClip())
	{
		_animator->Play(clip, std::chrono::milliseconds(0));
	}
	// Only the normal, camera and citadel states lean their poses by their lean ranges (rotations and places); the
	// holding, totem, tug, grain, creature and play-anim states fill their clip alone
	const bool leans =
	    _renderHandState == hand_state::k_Normal || _renderHandState == 2 || _renderHandState == hand_state::k_Citadel;
	_animator->SetLean(leans ? _leanSideways : std::nullopt, leans ? _leanForwards : std::nullopt);
	// Every clip applies its root bone under the hand's matrix, as the original
	_animator->SetRootLocked(false);
	_animator->Update(dt);
	// the blend after the state's Update, with the hand's dt (the camera time step x 0.001, the real frame time inside
	// the citadel): while it runs, t = its time / 0.13; the frame it reaches 0.13, no t (as the original).
	// (approximate) the original lerps its world-space transformed matrices (the first one the hand's matrix); here the
	// model-space bone matrices, the hand's own matrix being the Transform
	const auto ms =
	    game_clock::IsInsideCitadel() ? game_clock::FrameRealMs() : game_clock::CameraFrameMs(hand_demo::IsPlaying());
	_stateBlendT.reset();
	if (_stateBlend.IsActive())
	{
		_stateBlend.Update(static_cast<float>(ms) * 0.001f);
		_stateBlendT = _stateBlend.Fraction();
	}
	// the demo mouse's buttons: the frame's presses and releases, and the end of the grab that was waiting
	_litButtons = hand_tricons::StepLitButtons(_litButtons, {
	                                                            .movePressed = gripping && !_litMoveWasHeld,
	                                                            .actionPressed = actionHeld && !litActionWasHeld,
	                                                            .moveReleased = !gripping && _litMoveWasHeld,
	                                                            .actionReleased = !actionHeld && litActionWasHeld,
	                                                            .busy = litBusy,
	                                                            .landGrip = litLandGrip,
	                                                            .ended = _litEnded || (litWaiting && !_pendingPick),
	                                                        });
	_litMoveWasHeld = gripping;
}

void HandSystem::SearchNearForPress(bool actionPressed, bool gripping) noexcept
{
	// the action press: with nothing collided, the object near the action's point becomes the collided object, and the
	// branches below test it. (approximate) the hover's class filter (FindObjectUnderHand) stands for the pick-up / tap
	// tests and the locked select's is the field / pile branches'; (inferred) every ported class is interactable. A fish
	// farm goes to the fish branch through the action's point. A cleared point searches at the map's corner
	std::optional<entt::entity> nearObject;
	const bool searchNear = actionPressed && !_held && !_tug && !gripping && !_hovered && !_cursorObject && !_ropeUnderHand;
	if (searchNear)
	{
		nearObject = FindObjectNearMapCoord(_interactionPoint.value_or(glm::vec3(0.0f)));
		if (nearObject)
		{
			_cursorObject = nearObject;
			_hovered = FindObjectUnderHand();
		}
	}
	// (debug, OPENBLACK_HAND_TRACE) the press and its search, logged; nothing else changes
	if (actionPressed)
	{
		TracePress(searchNear, nearObject);
	}
	else
	{
		_traceClearedByDemoClick.reset();
	}
}

bool HandSystem::StartScreenPress(bool actionPressed, bool gripping) noexcept
{
	// an empty hand pressed on a screen object -> the bubble grip action state
	const bool screenPress = actionPressed && !_held && !_tug && !gripping && _screenObject.has_value();
	if (screenPress)
	{
		_screenGrip = true;
		_screenOffer = _screenObject; // the frozen collide keeps it
		_screenOfferDepth = _screenObjectDepth;
	}
	return screenPress;
}

bool HandSystem::TakeActionPress(bool actionPressed, bool gripping, bool screenPress) noexcept
{
	// whether the press took nothing: a click on the land that did not send the creature, or a press the lesson refused
	bool pressTookNothing = false;
	// the interface's in-influence flag: every ported class needs it for taps, locked selects and pick-ups
	const auto TapInInfluence = [this]() { return InInfluence(); };
	// the press takes the first branch that holds, in the hand's order (ECS/HandPressChain.h). Every fact is a read that
	// changes nothing; the ones that matter only with an empty hand over nothing are read only then, in that order
	auto branch = hand_press::Branch::None;
	// an empty hand pressed with nothing under it or near it: the press is a click on the land
	bool pressOnLand = false;
	// the village totem whose plinth or icon is under the cursor
	std::optional<entt::entity> pressTotem;
	// the fish farm of a shown fish at the hand's point, with nothing else under the empty hand
	std::optional<entt::entity> pressFarm;
	// a lesson's interface level can keep the empty hand from the rope and the locked selects, and let its pick-up path
	// act only on a script's highlight: a press it refuses there does nothing
	const auto interfaceFlags = interface_active::GetFlags();
	const bool lessonSkipsSelects = hand_press::LessonSkipsSelects(interfaceFlags);
	// the thing a press on nothing else finds at the hand's point: the object under the cursor, while it is available
	const auto AvailableCursorObject = [this]() -> std::optional<entt::entity> {
		return _cursorObject && ecs::IsAvailable(*_cursorObject) ? _cursorObject : std::nullopt;
	};
	const auto lessonRefuses = [interfaceFlags](std::optional<entt::entity> thing) {
		return hand_press::LessonRefusesPress(interfaceFlags, thing.has_value() && ecs::script_highlight::IsHighlight(*thing));
	};
	if (actionPressed)
	{
		hand_press::Facts facts {
		    .screenPress = screenPress,
		    .ropePress = _ropeUnderHand && !_held,
		    .held = _held.has_value(),
		    .hovered = _hovered.has_value(),
		    .hoveredIsField = _hovered && Locator::entitiesRegistry::value().AllOf<Field>(*_hovered),
		    .gripping = gripping,
		    .holdingSeed = IsHoldingSeed(),
		    .pickPressHeld = _pickPressHeld,
		    .creatureLockBusy = _creatureLock.has_value(),
		    .lessonSkipsSelects = lessonSkipsSelects,
		};
		if (hand_press::EmptyHandOverNothing(facts))
		{
			const bool cursorAvailable = _cursorObject && ecs::IsAvailable(*_cursorObject);
			if (cursorAvailable && Locator::villageTotemSystem::has_value())
			{
				pressTotem = Locator::villageTotemSystem::value().TotemOf(*_cursorObject);
			}
			// a puzzle totem is pressed as a village totem is, by its own test
			if (cursorAvailable && !pressTotem && ecs::puzzle_totem_hand::IsPuzzleTotem(*_cursorObject))
			{
				pressTotem = *_cursorObject;
			}
			facts.cursorIsTotem = pressTotem.has_value();
			facts.tapOnlyCursorObject =
			    cursorAvailable &&
			    (abodes::InterfaceValidToTap(*_cursorObject) || worship::citadel::IsEntranceValidToTap(*_cursorObject) ||
			     ecs::script_highlight::InterfaceValidToTap(*_cursorObject) ||
			     worship::temple_leash::InterfaceValidToTap(*_cursorObject) || RewardValidToTap(*_cursorObject));
			facts.cursorIsCreature = !facts.tapOnlyCursorObject && cursorAvailable &&
			                         std::as_const(Locator::entitiesRegistry::value()).AllOf<Creature>(*_cursorObject);
			facts.creatureTakesPress = facts.cursorIsCreature && CreatureTakesPress(*_cursorObject);
			// the fish farm is found in or out of the influence; its scooping starts only in it, when no script made it
			// unpickable and it is not in the locked select of an earlier press until its end is applied
			if (!facts.cursorIsTotem && !facts.tapOnlyCursorObject && !facts.cursorIsCreature)
			{
				pressFarm = FishFarmUnderHand(_interactionPoint.value_or(glm::vec3(0.0f)));
				facts.fishFarmInInfluence = pressFarm.has_value() && TapInInfluence() &&
				                            hand_press::PressMayLockSelect(true, object_flags::IsCannotBePickedUp(*pressFarm),
				                                                           object_flags::IsInLockedSelect(*pressFarm));
			}
		}
		branch = hand_press::Choose(facts);
		pressOnLand = branch == hand_press::Branch::None && hand_press::EmptyHandOverNothing(facts);
	}
	switch (branch)
	{
	case hand_press::Branch::None:
		// with nothing the hand can hold, tap or select at the point, the land is clicked; a thing found there is
		// tapped instead while the untied leash is on, which sends the creature to it
		if (pressOnLand && lessonRefuses(pressFarm.has_value() ? pressFarm : AvailableCursorObject()))
		{
			// the lesson refuses the press: nothing, and the press is not ended
			pressTookNothing = true;
		}
		else if (pressOnLand && pressFarm && Locator::entitiesRegistry::value().AllOf<Transform>(*pressFarm))
		{
			// the fish farm whose scooping can't start is the thing at the point: the grab start (out of the influence a
			// tap, which a farm doesn't take but the leash does; in it, a farm still in an earlier press's select is
			// pulled at, and its place-in-hand gives a handful straight from it), else the land is clicked. Only a farm
			// of the map: the fish puzzle's shoals, which openblack keeps as farms with no place, are not among them
			if (hand_press::RefusedSelectPress(ValidForPlaceInHand(*pressFarm), object_flags::IsCannotBePickedUp(*pressFarm),
			                                   LeashIsOnUntied()) == hand_press::RefusedSelect::Grab)
			{
				StartGrab(*pressFarm);
			}
			else
			{
				pressTookNothing = TapLand(_interactionPoint.value_or(glm::vec3(0.0f)));
			}
		}
		else if (pressOnLand)
		{
			const auto point = _interactionPoint.value_or(glm::vec3(0.0f));
			const bool thingAtPoint = _cursorObject && ecs::IsAvailable(*_cursorObject);
			if (hand_press::PressFallsThrough(true, thingAtPoint, LeashIsOnUntied()) == hand_press::FallThrough::TapThing)
			{
				Tap(*_cursorObject);
			}
			else
			{
				pressTookNothing = TapLand(point);
			}
		}
		break;
	case hand_press::Branch::ScreenObject:
		// the bubble grip action state has no other work at the press
		break;
	case hand_press::Branch::LeashPull:
		// the tied leash's rope: the hand takes hold of it, action state 21, until it lets go
		StartLeashPull();
		break;
	case hand_press::Branch::Field:
	{
		// fields are a locked select: the press starts the scooping at once, in the influence. Otherwise (out of it, a
		// field the scooping refuses, one with a unit of food or less, or one still in the locked select of an earlier
		// press, until its end is applied) the press goes on as for any other thing: a field the hand may take, or any
		// field with the untied leash on, goes to the grab start (in the influence the grab waits its time, as a field
		// can't be pulled, and its place-in-hand gives a handful straight from it; out of it, a tap, which a field
		// doesn't take but the leash does); else the land is clicked at the hand's point
		_pickPressHeld =
		    !lessonSkipsSelects && TapInInfluence() &&
		    hand_press::PressMayLockSelect(FieldValidForLockedSelect(*_hovered), object_flags::IsCannotBePickedUp(*_hovered),
		                                   object_flags::IsInLockedSelect(*_hovered));
		if (_pickPressHeld)
		{
			SendStartLockedSelect(*_hovered);
		}
		else if (lessonRefuses(_hovered))
		{
			// the lesson refuses the press: nothing, and the press is not ended
			pressTookNothing = true;
		}
		else if (hand_press::RefusedSelectPress(ValidForPlaceInHand(*_hovered), object_flags::IsCannotBePickedUp(*_hovered),
		                                        LeashIsOnUntied()) == hand_press::RefusedSelect::Grab)
		{
			StartGrab(*_hovered);
		}
		else
		{
			pressTookNothing = TapLand(_interactionPoint.value_or(glm::vec3(0.0f)));
		}
		break;
	}
	case hand_press::Branch::Hovered:
	{
		// Piles cannot be tapped, so the locked select (scooping) starts at once, in the influence as above; out of it
		// nothing happens (the grab start taps, and a pile is not tappable); as for a pile still in the locked select of an
		// earlier press, until its end is applied.
		const auto source = PotInfoOf(*_hovered);
		const auto* sourceInfo = source != PotInfo::_COUNT ? ecs::object::PotInfoOf(*_hovered) : nullptr;
		if (sourceInfo != nullptr && hand_grab::PileValidForLockedSelect(sourceInfo->potType, source) && !lessonSkipsSelects)
		{
			_pickPressHeld =
			    TapInInfluence() && hand_press::PressMayLockSelect(true, object_flags::IsCannotBePickedUp(*_hovered),
			                                                       object_flags::IsInLockedSelect(*_hovered));
			if (_pickPressHeld)
			{
				SendStartLockedSelect(*_hovered);
			}
		}
		else if (lessonRefuses(_hovered))
		{
			// the lesson refuses the press: nothing, and the press is not ended
			pressTookNothing = true;
		}
		else if (sourceInfo != nullptr && hand_grab::PileValidForLockedSelect(sourceInfo->potType, source))
		{
			// a pile whose scooping the lesson keeps the hand from goes on to the pick-up path: the grab start when the
			// hand may take it and no script made it unpickable, or while the untied leash is on; else the land is clicked
			if (hand_press::RefusedSelectPress(ValidForPlaceInHand(*_hovered), object_flags::IsCannotBePickedUp(*_hovered),
			                                   LeashIsOnUntied()) == hand_press::RefusedSelect::Grab)
			{
				StartGrab(*_hovered);
			}
			else
			{
				pressTookNothing = TapLand(_interactionPoint.value_or(glm::vec3(0.0f)));
			}
		}
		else
		{
			StartGrab(*_hovered);
		}
		break;
	}
	case hand_press::Branch::Totem:
	{
		// a village totem: the hand takes hold of it through a locked select, started at once as a totem can't be tapped,
		// when its player may take hold of it, it can be picked up, nobody holds it already and the hand is in the
		// influence. Otherwise nothing: it is neither placeable nor tappable. (not ported) with the leash in the hand, the
		// wait for a tap first. A puzzle totem the same, when it works, whoever's hand it is. A totem still in the locked
		// select of an earlier press is neither, until its end is applied. A puzzle totem not taken hold of (one the script
		// made unpickupable) goes on as a thing no branch takes: the land is clicked at the hand's point, or the totem is
		// tapped when it can be or the untied leash is on; the release remembers it as the thing clicked
		if (ecs::puzzle_totem_hand::IsPuzzleTotem(*pressTotem))
		{
			_pickPressHeld = hand_press::PressMayLockSelect(ecs::puzzle_totem_hand::ValidForLockedSelect(*pressTotem),
			                                                object_flags::IsCannotBePickedUp(*pressTotem),
			                                                object_flags::IsInLockedSelect(*pressTotem)) &&
			                 !ecs::puzzle_totem_hand::IsHeld(*pressTotem) && TapInInfluence();
			if (_pickPressHeld)
			{
				SendStartLockedSelect(*pressTotem);
			}
			const bool validToTap = abodes::InterfaceValidToTap(*pressTotem);
			if (!_pickPressHeld && lessonRefuses(pressTotem))
			{
				pressTookNothing = true;
				break; // the lesson refuses the press: nothing, and the press is not ended
			}
			switch (hand_press::TotemPressFallsThrough(_pickPressHeld, validToTap, LeashIsOnUntied()))
			{
			case hand_press::FallThrough::TapThing:
				Tap(*pressTotem);
				break;
			case hand_press::FallThrough::LandClick:
				pressTookNothing = TapLand(_interactionPoint.value_or(glm::vec3(0.0f)));
				break;
			case hand_press::FallThrough::None:
				break;
			}
			break;
		}
		auto& totems = Locator::villageTotemSystem::value();
		_pickPressHeld = hand_press::PressMayLockSelect(totems.ValidForLockedSelect(*pressTotem, PlayerNames::PLAYER_ONE),
		                                                object_flags::IsCannotBePickedUp(*pressTotem),
		                                                object_flags::IsInLockedSelect(*pressTotem)) &&
		                 !totems.IsGripped(*pressTotem) && TapInInfluence();
		if (_pickPressHeld)
		{
			SendStartLockedSelect(*pressTotem);
		}
		break;
	}
	case hand_press::Branch::TapOnly:
	{
		// the press sends the object under the cursor to the grab start when it can go into the hand or it is only
		// tappable (abodes are always tappable, so FindObjectUnderHand leaves them out: they are never hovered for a
		// pick-up). An abode cannot go into the hand, so it is tapped at once -> SendTap, which needs the hand inside the
		// influence -> the tap packet -> the abode's tap: knocking on the roof. A temple's entrance is the same, valid
		// to tap unless the script has locked it: its tap takes the local player inside its own temple. A "Did you
		// know?" sign is the same, valid to tap once it has a text, in the influence or not: its tap opens its bubble,
		// or closes it when it is that sign's. A temple's leash post is the same, valid to tap for its own player. A
		// lesson that lets the hand act only on a highlight refuses the press on anything else: nothing
		if (!lessonRefuses(_cursorObject))
		{
			Tap(*_cursorObject);
		}
		else
		{
			pressTookNothing = true;
		}
		break;
	}
	case hand_press::Branch::Creature:
	{
		// the player's own creature: the hand takes hold of it through a locked select, started at once as a creature
		// can't be tapped. (not ported) with the leash in the hand, the wait for a tap first
		SendStartLockedSelect(*_cursorObject);
		_creatureLock = *_cursorObject;
		_creatureLockState = {};
		_creatureLocked = false;
		_creatureFinishing = false;
		break;
	}
	case hand_press::Branch::FishFarm:
	{
		// fish: the locked select starts at the press, like piles, in the influence
		SendStartLockedSelect(*FishFarmUnderHand(_interactionPoint.value_or(glm::vec3(0.0f))));
		_pickPressHeld = true;
		break;
	}
	case hand_press::Branch::Seed:
	{
		// a press while holding a spell seed: armed until the release (HAND_GESTURE), cast at once
		// (HAND_POSITION) or kept casting while held (IN_HAND); HandSpellSeed.cpp
		SeedActionPressed();
		break;
	}
	case hand_press::Branch::Held:
	{
		// a press while holding: first, wherever the hand is, the press gives the held object to the creature it is
		// offered to or one under the hand that takes it, and does nothing more (HandGiving.cpp). Otherwise the object
		// under the hand takes the held one (a villager into a teleport stone: HandApplyToObject.cpp), or press the action
		// button again, move and release to put it down or hurl it (the release state). Out of the influence nothing
		// happens
		if (Give(GiveUnderHand()))
		{
			break;
		}
		if (TapInInfluence() && !HeldActionPressedOnObject(true))
		{
			_releaseArmed = true;
		}
		break;
	}
	}
	return pressTookNothing;
}

void HandSystem::RetryDemoPressAtTurn() noexcept
{
	// the turn's interface pass, after the hand's sync and its status pass: a demo's press that took nothing is taken
	// again here too, with the hand as the last frame left it, so that it lands on the same pass as the original's.
	// While the game is paused, or in the action state the feed of a fireball left, it is not taken and stays pending
	if (!hand_demo::PressTakenAgain(hand_demo::IsPlaying(), _demoPressPending, _actionHeld) ||
	    !hand_press::IdleTakesActionPress(Locator::time::has_value() && game_clock::IsPaused()) || _applyObjectState)
	{
		return;
	}
	const bool gripping = _pressGripping;
	SearchNearForPress(true, gripping);
	_collideObject = ecs::hand_action_point::CollideObject(_collideObject, _cursorObject, ActionInProgress(gripping));
	const bool screenPress = StartScreenPress(true, gripping);
	_demoPressPending = hand_demo::PressStaysPending(true, true, TakeActionPress(true, gripping, screenPress));
}

HandSystem::CameraHand& HandSystem::EnterCameraHand() noexcept
{
	if (!_cameraHand)
	{
		_cameraHand.emplace();
		_cameraHand->distance.SetPosition(_viewDistance);
	}
	return *_cameraHand;
}

hand_navigation_pose::Pose HandSystem::NavigationPose() noexcept
{
	const auto cues =
	    Locator::camera::has_value() ? Locator::camera::value().GetModel().GetHandCues() : CameraModel::HandCues {};
	// The grip while the drag pans the land or the clear view is shown. (undetermined) a grip the camera is not
	// dragging (its controls given up, a hand demo's, or another camera's) keeps the grip it always had here
	const bool gripsLand = !cues.dragging || cues.dragMode == camera_drag::DragMode::Pan || cues.clearViewGrip;
	return hand_navigation_pose::WhileDragging(gripsLand, cues.tricons,
	                                           static_cast<uint32_t>(camera_help::GetEnabledFeatures()));
}

std::optional<hand_navigation_pose::Pose> HandSystem::HoverPoseNow(bool cameraState) const noexcept
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	// (openblack guard) Valid: the original's pointer is never a deleted object's
	const bool cursorValid = _cursorObject && registry.Valid(*_cursorObject);
	if (cursorValid && !registry.AllOf<LandscapeVortex>(*_cursorObject) && !(_held && *_held == *_cursorObject) &&
	    registry.AnyOf<Mesh, CitadelEntrance>(*_cursorObject))
	{
		return std::nullopt;
	}
	const bool overSpellIcon = cursorValid && registry.AllOf<SpellIcon>(*_cursorObject);
	const bool onLeashRope = _ropeUnderHand || _leashPull.has_value();
	if (!hand_animation::ShowsNavigationPose(cameraState, _renderHandState, _screenObject.has_value(), overSpellIcon,
	                                         onLeashRope))
	{
		return std::nullopt;
	}
	return HoverPose();
}

std::optional<hand_navigation_pose::Pose> HandSystem::HoverPose() noexcept
{
	// While a demo plays, its recorded hints are what the hand shows (the camera does not work its own out then)
	if (const auto demoHints = hand_demo::PlayingHints(); demoHints)
	{
		return hand_navigation_pose::WhileHovering(hand_demo::HintsAsTricons(*demoHints),
		                                           static_cast<uint32_t>(camera_help::GetEnabledFeatures()));
	}
	const bool cameraReadsPlayer =
	    Locator::camera::has_value() && !script_camera::Drives() &&
	    !(Locator::cameraPathSystem::has_value() && Locator::cameraPathSystem::value().HoldsCamera());
	if (!cameraReadsPlayer)
	{
		return std::nullopt;
	}
	return hand_navigation_pose::WhileHovering(Locator::camera::value().GetModel().GetHandCues().tricons,
	                                           static_cast<uint32_t>(camera_help::GetEnabledFeatures()));
}

int32_t HandSystem::RequiredHandState() const noexcept
{
	// the hand's required state (HAND_STATES), from what it holds and what the interface is doing
	const auto held = RenderHeld();
	auto& registry = Locator::entitiesRegistry::value();
	return hand_state::Required({
	    .insideCitadel = game_clock::IsInsideCitadel(),
	    .interfaceState = _interfaceHandState,
	    .holding = held.has_value(),
	    // the status's locked select (_pickSource (inferred)) on a pile
	    .lockedSelectOnPile = held.has_value() && _pickSource && ecs::object_resources::IsPileResource(*_pickSource),
	    .holdsSeed = held.has_value() && registry.Valid(*held) && registry.AllOf<SpellSeed>(*held),
	    // a tree's tug, or a thing taken at the press while its tug hold lasts: the hand tugs until the hold is over
	    .tugging = _tug.has_value() || (_pendingPick.has_value() && _pendingTugHold),
	    // StartFixedPosAnimation's flag, or openblack's forced animation
	    .playingAnimation = _playAnim || !_override.empty(),
	    .creatureInHand = _creatureInHand.has_value(),
	});
}

HandSystemInterface::CameraIconFacts HandSystem::GetCameraIconFacts() const noexcept
{
	// The hand states that show the camera icons. (inferred) the hand's own source is the object locked in its
	// interaction, which holds them back while it scoops
	const bool shows =
	    hand_tooltips::ShowsCameraIcons(RequiredHandState(), _pickSource.has_value() && ecs::IsAvailable(*_pickSource));
	const bool over = (_hovered && ecs::IsAvailable(*_hovered)) || (_cursorObject && ecs::IsAvailable(*_cursorObject));
	return {.showsCameraIcons = shows,
	        .inInfluence = InInfluence(),
	        .overObject = over,
	        .crossHoldBacks = _crossHoldBacks,
	        .moveLit = _litButtons.move,
	        .actionLit = _litButtons.action};
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

HandSystemInterface::ClipTimes HandSystem::GetClipTimes() const noexcept
{
	if (!_animator)
	{
		return {};
	}
	const auto& readout = _animator->GetReadout();
	return {.clipMs = readout.timeMs, .sidewaysMs = readout.sidewaysMs, .forwardsMs = readout.forwardsMs};
}

void HandSystem::SetAnimationOverride(const std::string& name) noexcept
{
	_override = name;
}

void HandSystem::StartFixedPosAnimation(const std::string& clip, glm::vec3 point) noexcept
{
	// the fixed-position animation: the flag, the point and the clip; the time is Enter's
	_playAnim = true;
	_playAnimClip = clip;
	_playAnimPoint = point;
}
