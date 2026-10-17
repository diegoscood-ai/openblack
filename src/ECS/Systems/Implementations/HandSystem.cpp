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
#include "ECS/Registry.h"
#include "ECS/Scaffolds.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/AbodeKnockSystemInterface.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/HandTap.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/VillageTotem.h"
#include "ECS/Villager/VillagerHome.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "GameClock.h"
#include "Graphics/Texture2D.h"
#include "Hand/HandGrabRules.h"
#include "HandSystemDetail.h"
#include "Help/HelpProfile.h"
#include "Help/ToolTips.h"
#include "InfoConstants.h"
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
	return false;
}

void HandSystem::RegisterTapHandlers() noexcept
{
	// rocks: tappable when taller than 0.7; a tap splits it in two
	hand_tap::Register(
	    &object_physics::IsRock,
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
	    .rock = object_physics::IsRock(object),
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

bool HandSystem::SendTap(entt::entity object) noexcept
{
	// sending a tap: (in the influence || the object does not need it) && valid to tap && not flagged cannot be picked
	// up -> a tap packet, whose handler checks validity again and taps. Every ported class needs the influence but the
	// script highlight. The cannot-be-picked-up flag is SET_ID_PICKUPABLE 169's. (Tap remembers the object first)
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
		const auto specsDirectory = fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>());
		auto animator = std::make_unique<HandAnimator>();
		if (animator->Load(resources::LoadBlob(Locator::resources::value().GetBlobs(), hbnPath), specsDirectory,
		                   mesh->GetBoneMatrices(), mesh->GetBoneParents()))
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
		if (state == 9)
		{
			_playAnimMs = 0; // the play-anim state's Enter
		}
		// the Exit, the Enter, then the state blend start. INVISIBLE's Enter copies the drawn pose into the buffer blends
		// fade from; the blend start swaps the two buffers (the pose drawn last is faded from, the next one is written
		// over the other) and fades from the hand's place as last drawn. (not ported) the interaction camera of the
		// CREATURE state
		if (state == 0)
		{
			_handWorld.KeepDrawn();
		}
		if (state == 1)
		{
			_normalPose.Enter();
		}
		_handWorld.Swap();
		_stateBlend.Start(
		    Locator::entitiesRegistry::value().Get<const Transform>(_hands[static_cast<size_t>(Side::Left)]).position);
	}
	// INVISIBLE (the required state for the interface's modes 1 / 2 / 12 / 25) -> its Enter hides the hand: not drawn,
	// and its shadow off; its Exit shows it again (the bones kept for the blend back). The draw preparation does nothing
	// more while hidden or with the interface inactive: no state Update, no pose (Update returns). (not ported) the
	// system cursor: openblack has none
	// Only the INVISIBLE state hides (Enter -> Hide); an inactive interface alone only stops the update (inside the
	// citadel the required state is CITADEL first, and the hand stays drawn)
	// The same frame as the original: the required state reads the turn's interface hand state there too, and the
	// Enter's hide comes before that frame's draw, as NotDrawn here before RenderingSystem::PrepareDraw (Game.cpp's
	// order)
	_handHidden = _renderHandState == 0 || !interface_active::IsActive();
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
	// The held object is drawn by the hand's state, with the interface active, and only by the tug, holding, multiple
	// pick-up and grain states (3, 4, 6 and 8): otherwise it is not drawn, wherever it is. Nor, once ported, while a box
	// has the mouse
	const auto held = GetHeldObject();
	const bool drawingState = _renderHandState == 3 || _renderHandState == 4 || _renderHandState == 6 || _renderHandState == 8;
	const bool heldDrawn = drawingState && interface_active::IsActive();
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
	// the interface's kept object of the last frame becomes the one the frame is drawn with, before it is looked for again
	_renderCollideObject = _collideObject;
	if (!_animator)
	{
		return;
	}
	const float seconds = static_cast<float>(dt.count()) / 1e6f;
	// the hand's draw preparation runs before its state machine: the good / evil morph
	UpdateAlignmentMorph(std::nullopt, false);

	if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f)
	{
		// the hand-move help event, from a mouse move with a pixel delta
		help_profile::Trigger(help_profile::Event::HandMove);
	}
	UpdateGripDust(seconds);
	// mouse smoothing: smooth += dt * vel, clamped to +-80 px of the mouse, vel = (vel + (mouse - smooth) * 20 dt)
	// * 0.03^dt. The mouse is in the game's pixels (ints): the window's pixels here, so the lean depends on the
	// resolution as in the original
	_mouse += mouseDelta; // (approximate) the sum of the deltas stands for the mouse's absolute position
	if (_handHidden)
	{
		// the draw preparation stops after the state switch while hidden: the smoothing does not run
	}
	else if (_smoothMouseValid && seconds > 0.0f)
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
	// The L lean poses: the _lr pose at (lag + 80) / 160 x its duration (truncated) with lag = clamp(smoothed x - mouse x,
	// -80, 80), only when |lag| > 0.0001; the _fb pose the same with clamp(mouse y - smoothed y), only when |smoothed y|
	// > 0.0001; each the difference from its middle frame ((count - 1) >> 1). In the camera state the sideways lag is the
	// other way (mouse x - smoothed x); (inferred) openblack's land grip stands for that state (the CAMERA state is
	// (pending)). (approximate) HandAnimator samples the frame at (v + 1) / 2 of the clip
	// instead of the ms time; skipping a pose is the same as its middle frame (no difference)
	const auto clampLag = [](float v) { return v > -80.0f ? (v < 80.0f ? v : 80.0f) : -80.0f; };
	const float lagX = clampLag(gripping ? _mouse.x - _smoothMouse.x : _smoothMouse.x - _mouse.x);
	const float lagY = clampLag(_mouse.y - _smoothMouse.y);
	_motion.x = std::abs(lagX) > 0.0001f ? lagX / 80.0f : 0.0f;
	_motion.y = std::abs(_smoothMouse.y) > 0.0001f ? lagY / 80.0f : 0.0f;
	_animator->SetMotion(_motion.x, _motion.y);

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
		_held.reset();
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
	_hovered = (_held || _tug || gripping || _screenObject) ? std::nullopt : FindObjectUnderHand();
	bool actionPressed = actionHeld && !_actionWasHeld;
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
	if (std::exchange(_actionDoubleClick, false) &&
	    ecs::hand_leash::TakesDoubleClick(
	        {.gripping = gripping || _gripPoint.has_value(),
	         .bubbleGrip = _screenGrip,
	         .tugging = _tug.has_value(),
	         .grabbing = _pendingPick.has_value(),
	         .lockedSelect = _pickPressHeld || _pickSource.has_value() || _creatureLock.has_value(),
	         .releaseArmed = _releaseArmed,
	         .seedApply = _seedAction != SeedAction::None}))
	{
		LeashDoubleClick();
		actionPressed = false;
	}
	// the action press: with nothing collided, the object near the action's point becomes the collided object, and the
	// branches below test it. (approximate) the hover's class filter (FindObjectUnderHand) stands for the pick-up / tap
	// tests and the locked select's is the field / pile branches'; (inferred) every ported class is interactable. A fish
	// farm goes to the fish branch through the action's point. A cleared point searches at the map's corner
	std::optional<entt::entity> nearObject;
	if (actionPressed && !_held && !_tug && !gripping && !_hovered && !_cursorObject)
	{
		nearObject = FindObjectNearMapCoord(_interactionPoint.value_or(glm::vec3(0.0f)));
		if (nearObject)
		{
			_cursorObject = nearObject;
			_hovered = FindObjectUnderHand();
		}
	}
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
	const bool actionReleased = !actionHeld && _actionWasHeld;
	_actionWasHeld = actionHeld;
	// an empty hand pressed on a screen object -> the bubble grip action state; the release clears it and resets the
	// action state. (approximate) the original clears the whole flag word; only its grip bit is kept here
	if (_screenGrip && actionReleased)
	{
		_screenGrip = false;
	}
	const bool screenPress = actionPressed && !_held && !_tug && !gripping && _screenObject.has_value();
	if (screenPress)
	{
		_screenGrip = true;
		_screenOffer = _screenObject; // the frozen collide keeps it
		_screenOfferDepth = _screenObjectDepth;
	}
	// before the action states: the last thing tapped or clicked
	UpdateTapMemory(actionReleased);
	// the interface's in-influence flag: every ported class needs it for taps, locked selects and pick-ups
	const auto TapInInfluence = [this]() { return InInfluence(); };
	// the press takes the first branch that holds, in the hand's order (ECS/HandPressChain.h). Every fact is a read that
	// changes nothing; the ones that matter only with an empty hand over nothing are read only then, in that order
	auto branch = hand_press::Branch::None;
	// an empty hand pressed with nothing under it or near it: the press is a click on the land
	bool pressOnLand = false;
	// the village totem whose plinth or icon is under the cursor
	std::optional<entt::entity> pressTotem;
	if (actionPressed)
	{
		hand_press::Facts facts {
		    .screenPress = screenPress,
		    .held = _held.has_value(),
		    .hovered = _hovered.has_value(),
		    .hoveredIsField = _hovered && Locator::entitiesRegistry::value().AllOf<Field>(*_hovered),
		    .gripping = gripping,
		    .holdingSeed = IsHoldingSeed(),
		    .pickPressHeld = _pickPressHeld,
		    .creatureLockBusy = _creatureLock.has_value(),
		};
		if (hand_press::EmptyHandOverNothing(facts))
		{
			const bool cursorAvailable = _cursorObject && ecs::IsAvailable(*_cursorObject);
			if (cursorAvailable && Locator::villageTotemSystem::has_value())
			{
				pressTotem = Locator::villageTotemSystem::value().TotemOf(*_cursorObject);
			}
			facts.cursorIsTotem = pressTotem.has_value();
			facts.tapOnlyCursorObject = cursorAvailable && (abodes::InterfaceValidToTap(*_cursorObject) ||
			                                                worship::citadel::IsEntranceValidToTap(*_cursorObject) ||
			                                                ecs::script_highlight::InterfaceValidToTap(*_cursorObject) ||
			                                                worship::temple_leash::InterfaceValidToTap(*_cursorObject));
			facts.cursorIsCreature = !facts.tapOnlyCursorObject && cursorAvailable &&
			                         std::as_const(Locator::entitiesRegistry::value()).AllOf<Creature>(*_cursorObject);
			facts.creatureTakesPress = facts.cursorIsCreature && CreatureTakesPress(*_cursorObject);
			facts.fishFarmInInfluence = !facts.cursorIsTotem && !facts.tapOnlyCursorObject && !facts.cursorIsCreature &&
			                            TapInInfluence() &&
			                            FishFarmUnderHand(_interactionPoint.value_or(glm::vec3(0.0f))).has_value();
		}
		branch = hand_press::Choose(facts);
		pressOnLand = branch == hand_press::Branch::None && hand_press::EmptyHandOverNothing(facts) && !_cursorObject;
	}
	switch (branch)
	{
	case hand_press::Branch::None:
		if (pressOnLand)
		{
			TapLand(_interactionPoint.value_or(glm::vec3(0.0f)));
		}
		break;
	case hand_press::Branch::ScreenObject:
		// the bubble grip action state has no other work at the press
		break;
	case hand_press::Branch::Field:
	{
		// fields are a locked select: the press starts the scooping at once, in the influence. Out of it the field goes
		// on to the pick-up / tap path, where it is neither placeable nor tappable: nothing.
		_pickPressHeld =
		    TapInInfluence() && !object_flags::IsCannotBePickedUp(*_hovered) && FieldValidForLockedSelect(*_hovered);
		if (_pickPressHeld)
		{
			SendStartLockedSelect(*_hovered);
		}
		break;
	}
	case hand_press::Branch::Hovered:
	{
		// Piles cannot be tapped, so the locked select (scooping) starts at once, in the influence as above; out of it
		// nothing happens (the grab start taps, and a pile is not tappable).
		const auto source = PotInfoOf(*_hovered);
		const auto* sourceInfo = source != PotInfo::_COUNT ? ecs::object::PotInfoOf(*_hovered) : nullptr;
		if (sourceInfo != nullptr && hand_grab::PileValidForLockedSelect(sourceInfo->potType, source))
		{
			_pickPressHeld = TapInInfluence() && !object_flags::IsCannotBePickedUp(*_hovered);
			if (_pickPressHeld)
			{
				SendStartLockedSelect(*_hovered);
			}
		}
		else if (!ValidForPlaceInHand(*_hovered) || object_flags::IsCannotBePickedUp(*_hovered) || !TapInInfluence())
		{
			// an object that cannot go into the hand (a rock too big to lift, a spell icon, the SET_ID_PICKUPABLE flag)
			// or out of the influence is tapped at once: SendTap (refused out of the influence) -> the tap packet -> the
			// object's tap (a rock splits, a spell icon's own)
			Tap(*_hovered);
			_hovered.reset();
		}
		else if (Locator::entitiesRegistry::value().AllOf<Tree>(*_hovered) &&
		         !(Locator::entitiesRegistry::value().AllOf<InPhysics>(*_hovered) &&
		           !ecs::physics::particle_carried_objects::IsCarried(*_hovered)))
		{
			// a tuggable object is picked up with a tug at the press, so the tug starts at once, not after the 225 ms
			// grab threshold (the grab state picks it up once the tug lets it go)
			RenderHandPickUp(*_hovered); // with a tug
			BeginTug(*_hovered);
		}
		else
		{
			// the grab start (in the press's own frame): the frame's engine sample and the turn are kept, grab action
			// state. The object is free when it is in physics and not carried (InPhysics without CarriedByTornado): the
			// 225 ms timer, as a forest. A magic fireball is not tuggable and has its 3D object, so it takes the forest's
			// branch too. Otherwise RenderHandPickUp holds it with a tug, none for a seed. (not ported) a tuggable object
			// without a 3D object
			auto& registry = Locator::entitiesRegistry::value();
			const auto entity = *_hovered;
			_pendingPick = entity;
			// the grab keeps the grabbed object as the interface's, or none when it is not interactable
			_collideObject = Interactable(entity) ? std::optional(entity) : std::nullopt;
			_pendingPressMs = game_clock::EngineFrameSampleMs();
			_pendingPressTurn = game_clock::Turn();
			_pendingTimer = (registry.AllOf<InPhysics>(entity) && !ecs::physics::particle_carried_objects::IsCarried(entity)) ||
			                registry.AnyOf<BigForest, MagicFireBall>(entity);
			_pendingTugHold = !_pendingTimer && !registry.AllOf<SpellSeed>(entity);
			_pendingTugStarted = false;
			if (!registry.AnyOf<BigForest, MagicFireBall>(entity))
			{
				RenderHandPickUp(entity);
			}
		}
		break;
	}
	case hand_press::Branch::Totem:
	{
		// a village totem: the hand takes hold of it through a locked select, started at once as a totem can't be tapped,
		// when its player may take hold of it, it can be picked up, nobody holds it already and the hand is in the
		// influence. Otherwise nothing: it is neither placeable nor tappable. (not ported) with the leash in the hand, the
		// wait for a tap first
		auto& totems = Locator::villageTotemSystem::value();
		_pickPressHeld = totems.ValidForLockedSelect(*pressTotem, PlayerNames::PLAYER_ONE) &&
		                 !object_flags::IsCannotBePickedUp(*pressTotem) && !totems.IsGripped(*pressTotem) && TapInInfluence();
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
		// or closes it when it is that sign's. A temple's leash post is the same, valid to tap for its own player.
		Tap(*_cursorObject);
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
		// a press while holding: the object under the hand takes the held one (a villager into a teleport stone:
		// HandApplyToObject.cpp); otherwise press the action button again, move and release to put it down or hurl it
		// (the release state).
		// out of the influence nothing happens. Held down long enough, wherever the hand is, the press gives the held
		// object to a creature (HandGiving.cpp)
		_giving.givePress = true;
		_giving.pressMs = game_clock::EngineFrameSampleMs();
		_giving.pressTurn = game_clock::Turn();
		if (TapInInfluence() && !HeldActionPressedOnObject(true))
		{
			_releaseArmed = true;
		}
		break;
	}
	}
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
	// giving what the hand holds to a creature: the press held long enough gives it, and the offer is kept up or runs down
	UpdateGiving(actionHeld);
	// the creature's locked select: held once the turn locks it, let go at the release
	UpdateCreatureLock(actionReleased);
	if (_pendingPick)
	{
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
		// 100); first the pick-up (the timer path at elapsed >= 225 ms; else once the tug hold is over), then the release
		// (every object): stop the immersion, throw it when the hand holds it, and a tap only if elapsed <= 225 ms. A tap
		// does nothing for most objects, splits rocks taller than 0.7 and puts a one-shot orb's charged seed in the hand
		const uint32_t elapsed =
		    hand_grab::ElapsedMs(game_clock::EngineFrameSampleMs(), _pendingPressMs, game_clock::Turn(), _pendingPressTurn);
		const bool pickNow = _pendingTimer ? elapsed >= hand_grab::k_GrabWaitMs : !_pendingTugHold;
		if (!ecs::IsAvailable(*_pendingPick))
		{
			_pendingPick.reset();
		}
		else if (!pickNow && !actionHeld)
		{
			RenderHandRelease();
			if (elapsed <= hand_grab::k_GrabWaitMs)
			{
				Tap(*_pendingPick);
			}
			_pendingPick.reset();
		}
		else if (pickNow)
		{
			const auto entity = *_pendingPick;
			_pendingPick.reset();
			if (!TapInInfluence() || object_flags::IsCannotBePickedUp(entity))
			{
				// the generic pick-up of an object that cannot be picked up or out of the influence fails: the grab state
				// resets the action, nothing is picked up
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
	static const debug_env::Variable k_NoPickupPsys("OPENBLACK_NO_PICKUP_PSYS");
	UpdatePickupParticles(seconds, multiPickup && k_NoPickupPsys.Get() == nullptr);
	UpdateTestSplash(seconds);
	UpdateTestAbode(seconds);
	UpdateRootsAndPiles(seconds);
	// the piles rising or sinking to their amount
	if (Locator::miracleFxSystem::has_value())
	{
		Locator::miracleFxSystem::value().UpdatePiles(seconds);
	}

	// Gameplay state machine (original hand states): holding > gripping > can pick up > idle.
	// The normal state's clip (hndspec5.txt's order): Cwiggle at the entry ((not ported) Chold_fingers with the leash or
	// the leash action state); the object under the cursor (none if a LandscapeVortex), not the held one and with a 3D
	// object ((approximate) here a Mesh component, which fields and fish farms, drawn their own way, lack, or a temple's
	// entrance, whose 3D object is only collided), Ccan_pickup when the interface hand state is 9 (Can Pick up), else
	// Cstroke; later and on its own, a SpellIcon Ccan_pickup.
	// With no object under it (none the hand strokes, nor a spell icon, nor a screen object; (not ported) nor the
	// leash), the hand at the edge of the screen shows turning or tilting the camera from the camera's hints
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
	std::string clip = "Cwiggle";
	_hoverPose.reset();
	if (cursorValid && !clipRegistry.AllOf<LandscapeVortex>(*_cursorObject) && !(_held && *_held == *_cursorObject) &&
	    clipRegistry.AnyOf<Mesh, CitadelEntrance>(*_cursorObject))
	{
		clip = _interfaceHandState == 9 ? "Ccan_pickup" : "Cstroke";
	}
	else if (!cameraState && _renderHandState == 1 && !_screenObject &&
	         !(cursorValid && clipRegistry.AllOf<SpellIcon>(*_cursorObject)))
	{
		_hoverPose = HoverPose();
		if (_hoverPose)
		{
			const std::string poseClip(hand_navigation_pose::ClipName(*_hoverPose));
			if (_animator->Has(poseClip))
			{
				clip = poseClip;
			}
		}
	}
	if (cursorValid && clipRegistry.AllOf<SpellIcon>(*_cursorObject))
	{
		clip = "Ccan_pickup";
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
	if (_held && PotInfoOf(*_held) == PotInfo::HandFood)
	{
		// the hold radius of a food pile is its Get2DRadius x GetProportionRaised (ecs::object::Get2DRadius, inside
		// ComputeHoldParameters): the hand opens as the food in it grows.
		// Wood keeps a constant radius (wood piles have no Get2DRadius of their own).
		ComputeHoldParameters(*_held);
	}
	_animator->SetFrame(std::nullopt);
	if (holding && _holdType == HoldType::Magic && _animator->Has("Cwiggle"))
	{
		// MAGIC (a spell seed until it is ready): Cwiggle held at half its length
		clip = "Cwiggle";
		_animator->SetTime(static_cast<float>(
		    magic::hand_hold::HoldTimeMs(HoldType::Magic, _animator->GetDurationMs(clip), _holdRadius, _handScale)));
	}
	else if (holding && _holdType == HoldType::Above && _animator->Has("Chold_above"))
	{
		clip = "Chold_above";
		// pulling it free (the tug state), the hand opens by the reach against its own height alone
		const auto durationMs = _animator->GetDurationMs(clip);
		_animator->SetTime(
		    static_cast<float>(_tug ? magic::hand_hold::TugTimeMs(HoldType::Above, durationMs, _holdRadius, _handScale)
		                            : magic::hand_hold::HoldTimeMs(HoldType::Above, durationMs, _holdRadius, _handScale)));
	}
	else if (holding && _holdType != HoldType::Above && _animator->Has("Chold_side"))
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
	// the state change ran first this frame (PrepareState). The draw preparation does nothing more while hidden or with
	// the interface inactive: no state Update, no pose
	if (_handHidden)
	{
		return;
	}
	// the TOTEM state's update, every drawn frame: the mouse's move up the screen since the last frame slides the village
	// totem the hand holds. The mouse is in the window's pixels, down the screen, against the window's height. Then the
	// hand takes its side hold on the icon, closed by the icon's hold radius; its place and tilt are the placement's
	if (_renderHandState == 5 && _lockedSelectAction && mouseDelta.y != 0.0f && Locator::villageTotemSystem::has_value() &&
	    Locator::windowing::has_value())
	{
		Locator::villageTotemSystem::value().Slide(*_lockedSelectAction, -mouseDelta.y,
		                                           static_cast<float>(Locator::windowing::value().GetSize().y));
	}
	if (_renderHandState == 5 && _lockedSelectAction && _override.empty() && _animator->Has("Chold_side") &&
	    Locator::villageTotemSystem::has_value())
	{
		if (const auto hold = Locator::villageTotemSystem::value().GetHandHold(*_lockedSelectAction))
		{
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
		_creaturePose = Locator::creatureHandSystem::value().Update(_mouseRayOrigin, _mouseRayDirection, _mouse,
		                                                            static_cast<float>(handMs) * 0.001f);
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
	// The normal state's update: another pose than the one it showed last (not its first after its Enter) swaps the
	// pose buffers and starts the state blend afresh from the hand's last place, before the pose is written. The camera
	// state is not one of openblack's states yet (the empty hand dragging the land stands for it), so the normal state
	// is entered afresh after it
	if (_renderHandState == 1)
	{
		if (cameraClip)
		{
			_normalPose.Enter();
		}
		else if (_normalPose.Change(clip))
		{
			_handWorld.Swap();
			_stateBlend.Start(clipRegistry.Get<const Transform>(_hands[static_cast<size_t>(Side::Left)]).position);
		}
	}
	if (clip != _animator->GetCurrentClip())
	{
		_animator->Play(clip, std::chrono::milliseconds(0));
	}
	// Grip drag lives mostly in the root translation of Lgrip_lr/Lgrip_fb; the camera state leans every pose of its own
	// the same way
	_animator->SetLayerTranslationScale(clip == "Cgrip" || (cameraClip && clip == *cameraClip) ? 1.0f : 0.0f);
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
	// the hand's required state (HAND_STATES)
	if (game_clock::IsInsideCitadel())
	{
		return 0xA; // CITADEL
	}
	const auto interfaceState = _interfaceHandState;
	const auto held = RenderHeld();
	if (held && interfaceState == 0x19)
	{
		return 0; // INVISIBLE
	}
	if (held)
	{
		// the status's locked select (_pickSource (inferred)) on a pile with an object held: HOLDING, before GRAIN / TUG
		// and without the 180 ms release impulse; INVISIBLE (mode 0x19) is above
		if (_pickSource && ecs::object_resources::IsPileResource(*_pickSource))
		{
			return 4; // HOLDING
		}
		auto& registry = Locator::entitiesRegistry::value();
		if (registry.Valid(*held) && registry.AllOf<SpellSeed>(*held))
		{
			return 8; // GRAIN
		}
		return _tug ? 3 : 4; // TUG while tugging, else HOLDING ((pending) the 180 ms release packet)
	}
	if (_playAnim || !_override.empty())
	{
		return 9; // PLAY_ANIM: StartFixedPosAnimation's flag, or openblack's forced animation
	}
	// the interface's hand state
	switch (interfaceState)
	{
	case 1:
	case 2:
	case 0xC:
	case 0x19:
		return 0; // INVISIBLE
	case 0xE:
		return 5; // TOTEM
	case 0x13:
		return _creatureInHand ? 7 : 1; // CREATURE with the creature, else NORMAL
	default:
		// (pending) CAMERA 2 in the camera mode with the tricons
		return 1; // NORMAL
	}
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

void HandSystem::StartFixedPosAnimation(const std::string& clip, glm::vec3 point) noexcept
{
	// the fixed-position animation: the flag, the point and the clip; the time is Enter's
	_playAnim = true;
	_playAnimClip = clip;
	_playAnimPoint = point;
}
