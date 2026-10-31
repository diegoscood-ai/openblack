/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <bit>
#include <numbers>
#include <string>
#include <string_view>
#include <utility>

#include <fmt/format.h>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/CreatureBody.h"
#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "Camera/CreatureInteractCameraModel.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureThrow.h"
#include "CreatureSpawner.h"
#include "ECS/Archetypes/MobileObjectArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/HandOnCreature.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureHoldPose.h"
#include "ECS/HoldLowering.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Town/TownCreatureAttitude.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::debug::gui;
using openblack::ecs::components::Creature;
using openblack::ecs::components::CreatureMindState;
using openblack::ecs::components::CreatureObjectAction;
using openblack::ecs::components::MobileObject;
using openblack::ecs::components::Transform;

namespace
{
/// The things that can be put by a creature to pick up
constexpr std::array<std::pair<MobileObjectInfo, std::string_view>, 6> k_Objects {{
    {MobileObjectInfo::Ball, "Ball"},
    {MobileObjectInfo::EgyptBarrel, "Barrel"},
    {MobileObjectInfo::EgyptPotA, "Pot"},
    {MobileObjectInfo::MagicFood, "Food"},
    {MobileObjectInfo::WaterJug, "Water jug"},
    {MobileObjectInfo::MagicWood, "Wood"},
}};
/// What it can do to what it holds: stroke, shake, smell, examine
constexpr std::array<std::string_view, creature_object_actions::k_KeepAnimationCount> k_KeepNames {"Stroke", "Shake", "Smell",
                                                                                                   "Examine"};
/// Things are put this far from the creature for its size
constexpr float k_PutAtDistance = 10.0f;

std::string_view StateName(ecs::systems::CreatureObjectActionSystemInterface::State state)
{
	constexpr std::array<std::string_view, 4> k_Names {"idle", "busy", "done", "failed"};
	return k_Names.at(static_cast<size_t>(state));
}

std::string ObjectName(const ecs::Registry& registry, entt::entity entity)
{
	if (const auto* object = std::as_const(registry).TryGet<const MobileObject>(entity))
	{
		for (const auto& [type, name] : k_Objects)
		{
			if (type == object->type)
			{
				return fmt::format("{} {}", name, entt::to_integral(entity));
			}
		}
		return fmt::format("object {} (type {})", entt::to_integral(entity), static_cast<int>(object->type));
	}
	return fmt::format("entity {}", entt::to_integral(entity));
}
} // namespace

void CreatureSpawner::DrawHands(entt::entity entity) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* creature = std::as_const(registry).TryGet<const Creature>(entity);
	const auto* transform = std::as_const(registry).TryGet<const Transform>(entity);
	if (creature == nullptr || transform == nullptr || !Locator::creatureObjectActionSystem::has_value())
	{
		return;
	}
	auto& hands = Locator::creatureObjectActionSystem::value();

	ImGui::SeparatorText("Hands");
	const auto held = hands.GetHeld(entity);
	ImGui::Text("Holding %s (what GET_OBJECT_HELD gives a script)",
	            held.has_value() ? ObjectName(registry, *held).c_str() : "nothing");
	// What it holds is out of the map until it lets go of it
	if (held.has_value())
	{
		ImGui::Text("What it holds is %s", ecs::map_cells::IsObjectInMap(*held) ? "in the map" : "out of the map");
		// It hangs below the hand's grasp point by its height times its class's hold lowering
		const float height = ecs::object::GetHeight(*held);
		ImGui::Text("Hung %.2f below the grasp point (height %.2f x lowering %.2f)",
		            static_cast<double>(ecs::hold_lowering::LoweringOf(registry, *held)), static_cast<double>(height),
		            static_cast<double>(ecs::hold_lowering::MultiplierOf(registry, *held)));
		// Turned each turn by the hand's bone, the angles it takes as it is put down, tossed away or thrown
		const auto angles = ecs::placed_turn::AnglesOf(std::as_const(registry), *held);
		ImGui::Text("Turned by the hand: %.3f, %.3f, %.3f about x, y and z", static_cast<double>(angles.x),
		            static_cast<double>(angles.y), static_cast<double>(angles.z));
		// The arm that holds it shows the hold pose, unless the body plays with what it holds
		const auto holdingHand = ecs::creature_hold_pose::HoldingHandOf(std::as_const(registry), entity);
		ImGui::Text("The arm %s the hold pose", holdingHand.has_value() ? "shows" : "does not show");
	}
	// What it eats is in its hand for a script's IN_CREATURE_HAND, as what it holds is
	if (const auto eating = hands.GetEating(entity))
	{
		ImGui::Text("Eating %s (in its hand for IN_CREATURE_HAND)", ObjectName(registry, *eating).c_str());
	}
	if (const auto* action = std::as_const(registry).TryGet<const CreatureObjectAction>(entity))
	{
		const auto state = hands.GetState(entity);
		ImGui::Text("%s: %s%s%s", creature_object_actions::Name(action->kind).data(), StateName(state).data(),
		            action->failure.empty() ? "" : ", ", action->failure.c_str());
		// A take whose giving was called off plays on, to close on nothing at its moment
		if (action->kind == creature_object_actions::Kind::TakeFromHand &&
		    action->phase == CreatureObjectAction::Phase::Playing &&
		    action->status == creature_object_actions::Status::Running && !action->target.has_value())
		{
			ImGui::TextUnformatted("The giving was called off: its hand closes on nothing at the moment of taking");
		}
		if (action->phase == CreatureObjectAction::Phase::Playing &&
		    state == ecs::systems::CreatureObjectActionSystemInterface::State::Busy)
		{
			std::string slots;
			for (size_t i = 0; i < action->animationCount; ++i)
			{
				slots +=
				    fmt::format("{}{} {:+.2f}", slots.empty() ? "" : ", ", action->animations.at(i), action->weights.at(i));
			}
			ImGui::TextWrapped("%.0f of %.0f ms, %s at %.0f ms%s: %s", static_cast<double>(action->timeMs),
			                   static_cast<double>(action->durationMs), action->eventDone ? "acted" : "acts",
			                   static_cast<double>(action->eventMs), action->mirrored ? ", mirrored" : "", slots.c_str());
		}
		else if (action->phase == CreatureObjectAction::Phase::Approach)
		{
			// Whether it turns to face the thing, or the hands wait for a walk (they start the turn after) or the body's
			// own action to end
			const bool walking =
			    Locator::creatureLocomotionSystem::has_value() && Locator::creatureLocomotionSystem::value().IsMoving(entity);
			const auto* animation = std::as_const(registry).TryGet<const ecs::components::CreatureAnimation>(entity);
			const bool playing = animation != nullptr && creature_layers::IsPlaying(animation->body);
			const char* moving = action->turningToIt ? ", turning to it"
			                     : walking           ? ", waits for its walk to end"
			                     : playing           ? ", waits for its body's action to end"
			                                         : "";
			if (action->reach.has_value())
			{
				ImGui::Text("Getting in reach, try %u, reaches %.1f%s", action->attempts, static_cast<double>(action->maxReach),
				            moving);
			}
			else
			{
				ImGui::Text("Setting about it, try %u%s", action->attempts, moving);
			}
		}
		// Its throw, the last until its next action: where to and how long it is to fly, closed by default
		if (action->kind == creature_object_actions::Kind::Throw && ImGui::TreeNode("Throw"))
		{
			const auto& point = action->point;
			ImGui::Text("At (%.2f, %.2f, %.2f), %.2f along the ground from where it stands now", static_cast<double>(point.x),
			            static_cast<double>(point.y), static_cast<double>(point.z),
			            static_cast<double>(creature_throw::GroundDistance(transform->position, point)));
			ImGui::Text("%.6f s in flight (bits %08X), %s", static_cast<double>(action->flightSeconds),
			            std::bit_cast<uint32_t>(action->flightSeconds),
			            action->givenFlightSeconds.has_value() ? "as it was given" : "timed by the distance along the ground");
			ImGui::TreePop();
		}
	}
	// What it was asked while its hands still played: it sets about it once they are free
	if (const auto* next = std::as_const(registry).TryGet<const ecs::components::CreatureObjectActionWaiting>(entity))
	{
		ImGui::Text("Then %s, once its hands are free", creature_object_actions::Name(next->action.kind).data());
	}
	// What each town that met it makes of it (the town's records of creatures)
	std::as_const(registry).Each<const ecs::components::Town>([entity](entt::entity town, const ecs::components::Town& t) {
		for (const auto& record : t.creatureRecords)
		{
			if (record.creature == entity)
			{
				ImGui::Text("Town %u sees it with %s, set at turn %u", entt::to_integral(town),
				            ecs::town_creature_attitude::Name(record.attitude).data(), record.setTurn);
			}
		}
	});
	if (const auto* mind = std::as_const(registry).TryGet<const CreatureMindState>(entity))
	{
		if (mind->lastFeedback.has_value())
		{
			ImGui::Text("Last feedback %+.2f while %s", static_cast<double>(mind->lastFeedback->value),
			            creature_mind::Name(mind->lastFeedback->activity).data());
		}
		else
		{
			ImGui::TextUnformatted("No feedback from the player yet");
		}
		ImGui::Text("Attitude to the player %+.3f, average feedback %+.2f", static_cast<double>(mind->attitudeToPlayer),
		            static_cast<double>(mind->averageFeedback));
		// Given something, what it will decide about once it is free to choose
		if (mind->useWhatItHolds)
		{
			ImGui::TextUnformatted("Given something: once free to choose, it decides what to do with what it holds");
		}
	}
	if (Locator::creatureHandSystem::has_value() && Locator::creatureHandSystem::value().GetCreature() == entity)
	{
		ImGui::Text("The hand is on it: %+.2f so far",
		            static_cast<double>(Locator::creatureHandSystem::value().GetFeedbackSum()));
		// The touch's clocks, cycle and hard slaps, closed by default
		if (ImGui::TreeNode("Creature hand"))
		{
			auto& creatureHand = Locator::creatureHandSystem::value();
			bool interactionCamera = creatureHand.InteractionCamera();
			if (ImGui::Checkbox("The camera turns to the creature while the hand holds it", &interactionCamera))
			{
				creatureHand.SetInteractionCamera(interactionCamera);
			}
			// Which camera gave the view to the interaction, or which has it
			if (const auto takenFrom = creatureHand.CameraTakenFrom(); takenFrom.has_value())
			{
				ImGui::Text("The view was taken from %s", creature_interact::Name(*takenFrom).data());
			}
			else if (Locator::camera::has_value())
			{
				ImGui::Text("The view stays with %s",
				            creature_interact::Name(creature_interact::CurrentHolder(Locator::camera::value())).data());
			}
			// The end of a hold that did not get the view takes off the camera that kept it
			if (const auto takenOff = creatureHand.LastTakenOff(); takenOff.has_value())
			{
				ImGui::Text("The end of the last hold that kept the view took off %s",
				            creature_interact::Name(*takenOff).data());
			}
			std::as_const(registry).Each<const ecs::components::HandOnCreature>(
			    [&](entt::entity, const ecs::components::HandOnCreature& contact) {
				    if (contact.creature != entity)
				    {
					    return;
				    }
				    constexpr std::array<const char*, 3> k_Cycles {"wiggle", "tickle", "slap"};
				    ImGui::Text("Since a touch %.0f ms, ready %.0f ms, on the body %.0f ms, slap ready %.0f ms",
				                static_cast<double>(contact.clocks.sinceTouch), static_cast<double>(contact.clocks.ready),
				                static_cast<double>(contact.clocks.contact), static_cast<double>(contact.slapReadyMs));
				    ImGui::Text("Cycle %s, speed %.1f, hard slaps %u in a row at band %u",
				                k_Cycles.at(static_cast<size_t>(contact.cycle)), static_cast<double>(contact.speed),
				                contact.hardSlaps.count, static_cast<unsigned>(contact.hardSlaps.band));
				    // each change to the slap or the caress starts the hand's blend
				    ImGui::Text("Changes of cycle that started the hand's blend: %u", contact.cycleBlends);
			    });
			ImGui::TreePop();
		}
	}
	// The player's hand offering it what the hand holds, or the creature reaching for it once given
	if (Locator::handGrabSystem::has_value() && Locator::handGrabSystem::value().GetOfferedCreature() == entity)
	{
		const auto& grab = Locator::handGrabSystem::value();
		const auto offered = grab.GetHeld();
		ImGui::Text("The player's hand offers it %s, turned %.2f",
		            offered.has_value() ? ObjectName(registry, *offered).c_str() : "nothing",
		            static_cast<double>(grab.GetOfferTurn()));
	}
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto rigId = creature::GetRigId(creature->species);
	if (rigs.Contains(rigId) && rigs.Handle(rigId)->actionPoints.has_value())
	{
		const auto& points = *rigs.Handle(rigId)->actionPoints;
		ImGui::TextDisabled("Hand %u, foot %u, head %u; pick up %.0f, eat %.0f, throw %.0f, put down %.0f ms", points.rightHand,
		                    points.rightFoot, points.head, static_cast<double>(points.pickUpMs),
		                    static_cast<double>(points.eatMs), static_cast<double>(points.throwMs),
		                    static_cast<double>(points.putDownMs));
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Armpit %u, belly %u, groin %u; destroy %.0f, discard %.0f ms", points.rightArmpit, points.belly,
			                  points.groin, static_cast<double>(points.destroyMs), static_cast<double>(points.discardMs));
		}
	}

	// Things to put by it
	if (ImGui::BeginCombo("Thing", k_Objects.at(static_cast<size_t>(_objectType)).second.data()))
	{
		for (size_t i = 0; i < k_Objects.size(); ++i)
		{
			if (ImGui::Selectable(k_Objects.at(i).second.data(), static_cast<int>(i) == _objectType))
			{
				_objectType = static_cast<int>(i);
			}
		}
		ImGui::EndCombo();
	}
	const auto putAt = [&](float angle) {
		if (!Locator::terrainSystem::has_value() || !Locator::infoConstants::has_value())
		{
			return;
		}
		auto at =
		    creature_spawner::PointAround(transform->position, transform->rotation, angle, k_PutAtDistance * creature->size);
		at = creature_spawner::OnGround(at, Locator::terrainSystem::value().GetHeightAt(glm::xz(at)));
		ecs::archetypes::MobileObjectArchetype::Create(at, k_Objects.at(static_cast<size_t>(_objectType)).first, 0.0f, 1.0f);
	};
	constexpr float k_Quarter = std::numbers::pi_v<float> / 2.0f;
	ImGui::TextUnformatted("Put it");
	ImGui::SameLine();
	if (ImGui::SmallButton("in front"))
	{
		putAt(0.0f);
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("front left"))
	{
		putAt(-k_Quarter / 2.0f);
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("front right"))
	{
		putAt(k_Quarter / 2.0f);
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("left"))
	{
		putAt(-k_Quarter);
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("right"))
	{
		putAt(k_Quarter);
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("behind"))
	{
		putAt(2.0f * k_Quarter);
	}

	// What to do with what it holds
	const auto told = [this](std::string_view what, bool started) {
		_lastHands = fmt::format("{}: {}", what, started ? "started" : "can't");
	};
	ImGui::BeginDisabled(!held.has_value());
	if (ImGui::Button("Put down"))
	{
		told("Put down", hands.PutDown(entity));
	}
	ImGui::SameLine();
	if (ImGui::Button("Toss away"))
	{
		told("Toss away", hands.Discard(entity));
	}
	ImGui::SameLine();
	if (ImGui::Button("Lob"))
	{
		told("Lob", hands.Lob(entity));
	}
	ImGui::SameLine();
	if (ImGui::Button("Eat it"))
	{
		told("Eat", hands.EatHeld(entity));
	}
	ImGui::SameLine();
	if (ImGui::Button("Drop"))
	{
		hands.Drop(entity);
	}
	ImGui::SetNextItemWidth(100.0f);
	if (ImGui::BeginCombo("##keep", k_KeepNames.at(static_cast<size_t>(_keepAction)).data()))
	{
		for (size_t i = 0; i < k_KeepNames.size(); ++i)
		{
			if (ImGui::Selectable(k_KeepNames.at(i).data(), static_cast<int>(i) == _keepAction))
			{
				_keepAction = static_cast<int>(i);
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	if (ImGui::Button("Look it over"))
	{
		told(k_KeepNames.at(static_cast<size_t>(_keepAction)),
		     hands.Keep(entity, creature_object_actions::k_FirstKeepAnimation + static_cast<size_t>(_keepAction)));
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Stop it"))
	{
		hands.Cancel(entity);
	}
	if (!_lastHands.empty())
	{
		ImGui::TextUnformatted(_lastHands.c_str());
	}
	ImGui::TextDisabled("Command it to pick up, throw, knock down or point with a left click");
}
