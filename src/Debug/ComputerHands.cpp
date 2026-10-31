/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComputerHands.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <imgui.h>

#include "ECS/Components/ComputerHand.h"
#include "ECS/Components/ComputerMind.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/Player.h"
#include "ECS/ComputerPlayerMindTree.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::debug::gui;
using openblack::ecs::components::ComputerHand;
using openblack::ecs::components::ComputerMind;
using openblack::ecs::components::ComputerPlayer;
using openblack::ecs::components::HandSubAction;
using openblack::ecs::components::Player;

namespace
{

/// The names of the hand's steps, by their numbers
constexpr std::array<const char*, 37> k_StepNames {"PickUpObject",
                                                   "MoveHandToPos",
                                                   "MoveHandToObject",
                                                   "MoveHandToCreatureHead",
                                                   "MoveHandNearPos",
                                                   "MoveHandNearObject",
                                                   "MoveHandToPosStopWhenOutOfInfluence",
                                                   "ThrowObjectInHandAtPos",
                                                   "DropObjectInHandAtPos",
                                                   "SayText",
                                                   "GiveObjectInHandToCreature",
                                                   "SetLeash",
                                                   "AttachLeashToObject",
                                                   "SlapCreature",
                                                   "StrokeCreature",
                                                   "CastSpellAtObject",
                                                   "CastSpellAtPos",
                                                   "CastSpellTowardPos",
                                                   "CastShieldAtPos",
                                                   "CloseDownCurrentSpell",
                                                   "RaiseTotem",
                                                   "LowerTotem",
                                                   "MakeGesture",
                                                   "Wait",
                                                   "DropObjectInHandOnObject",
                                                   "ForceCreatureToPerformAction",
                                                   "ForceCreatureDominantDesire",
                                                   "WaitForObjectToBeNearPos",
                                                   "WaitForSpellToWearOff",
                                                   "CreateFoodFromContainer",
                                                   "CreateWoodFromContainer",
                                                   "SetCreatureSpeedForJourney",
                                                   "MakeWorshippersChantForSpell",
                                                   "MakeWorshippersChantForSpellNoWait",
                                                   "SetHandToPos",
                                                   "MoveHandToPosDirectYInterpolation",
                                                   "SetHandToPosDirectY"};

const char* NameOf(HandSubAction type)
{
	const auto index = static_cast<size_t>(type);
	return index < k_StepNames.size() ? k_StepNames.at(index) : "none";
}

std::string ThingText(entt::entity thing)
{
	return thing == entt::null ? std::string("none") : fmt::format("#{}", entt::to_integral(thing));
}

/// A node of the mind and the nodes under it, folded under the node's name
void DrawMindNode(const ComputerMind& mind, uint16_t node)
{
	const auto& state = mind.nodes.at(node);
	const auto chosen = state.chosen.has_value() ? fmt::format("{}", *state.chosen) : std::string("none");
	const auto text = fmt::format("{} {}: weight {:.3f}, held back {:.3f}, worth {:.4f}, chose {}, weighed {}, ran {}", node,
	                              computer_player::NamePrefix(computer_player::RowOf(node).name), state.personality,
	                              state.suppression, state.score, chosen, state.lastWeighedTurn, state.lastRunTurn);
	const auto children = computer_player::ChildrenOf(node);
	if (children.empty())
	{
		ImGui::BulletText("%s", text.c_str());
		return;
	}
	ImGui::PushID(static_cast<int>(node));
	if (ImGui::TreeNode("node", "%s", text.c_str()))
	{
		for (const auto child : children)
		{
			DrawMindNode(mind, child);
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
}

/// Each computer player's mind: its plan, its rests, its attitudes, the switch that holds it and its tree
void DrawMinds(ecs::Registry& registry)
{
	std::vector<std::pair<PlayerNames, entt::entity>> minds;
	std::as_const(registry).Each<const Player, const ComputerMind>(
	    [&minds](entt::entity entity, const Player& player, const ComputerMind& /*mind*/) {
		    minds.emplace_back(player.name, entity);
	    });
	for (const auto& [name, entity] : minds)
	{
		auto& mind = registry.Get<ComputerMind>(entity);
		if (mind.nodes.size() != computer_player::k_MindNodeCount)
		{
			continue;
		}
		ImGui::SeparatorText(fmt::format("Mind of player {}", static_cast<int>(name)).c_str());
		ImGui::PushID(static_cast<int>(name));
		// off by default: ticked, the mind weighs nothing, as a script's pause
		ImGui::Checkbox("Hold this mind", &mind.heldByDebug);
		ImGui::SameLine();
		ImGui::Checkbox("Trace this mind", &mind.traced);
		ImGui::Text("plan worth %.4f, found nothing at turn %u, rests after steps: %s", mind.planScore, mind.emptyWeighingTurn,
		            mind.restAfterSteps ? "yes" : "no");
		std::string attitudes;
		for (size_t player = 0; player < mind.attitude.size(); ++player)
		{
			attitudes += fmt::format("{}{}: {:.3f}", player == 0 ? "" : ", ", player, mind.attitude.at(player) * 0.5f);
		}
		ImGui::Text("attitudes (as a script reads them): %s", attitudes.c_str());
		if (ImGui::TreeNode("tree", "The tree"))
		{
			DrawMindNode(mind, computer_player::k_RootNode);
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
}

} // namespace

ComputerHands::ComputerHands() noexcept
    : Window("Computer hands", ImVec2(460.0f, 360.0f))
{
}

void ComputerHands::Draw() noexcept
{
	if (!Locator::entitiesRegistry::has_value())
	{
		ImGui::TextUnformatted("No land loaded");
		return;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	int count = 0;
	registry.Each<const Player, const ComputerHand>(
	    [&registry, &count](entt::entity entity, const Player& player, const ComputerHand& hand) {
		    ++count;
		    ImGui::SeparatorText(fmt::format("Player {}", static_cast<int>(player.name)).c_str());
		    if (const auto* mind = registry.TryGet<const ComputerPlayer>(entity); mind != nullptr)
		    {
			    ImGui::Text("mind: %s%s%s", mind->switchedOn ? "on" : "off", mind->paused ? ", paused" : "",
			                mind->started ? ", started" : "");
		    }
		    ImGui::Text("held by a script: %s, busy: %s, ready: %s", hand.held ? "yes" : "no", hand.busy ? "yes" : "no",
		                hand.steps.empty() ? "yes" : "no");
		    ImGui::Text("at %.2f %.2f %.2f, going to %.2f %.2f %.2f", hand.point.x, hand.point.y, hand.point.z, hand.target.x,
		                hand.target.y, hand.target.z);
		    ImGui::Text("speed %.1f (the player's %.1f), holds %s", hand.speed, hand.playerSpeed,
		                ThingText(hand.heldObject).c_str());
		    if (hand.heldObject != entt::null)
		    {
			    // where the held thing is drawn: where the hand was as its last turn of steps began
			    ImGui::Text("  drawn at %.2f %.2f %.2f (turn %u of steps)", hand.turnStartPoint.x, hand.turnStartPoint.y,
			                hand.turnStartPoint.z, hand.stepTurns);
		    }
		    for (size_t index = 0; index < hand.steps.size(); ++index)
		    {
			    const auto& step = hand.steps.at(index);
			    if (index == 0)
			    {
				    ImGui::Text("  %s, stage %d", NameOf(step.type), static_cast<int>(hand.stage));
			    }
			    else
			    {
				    ImGui::Text("  %s", NameOf(step.type));
			    }
		    }
		    for (const auto& queued : hand.queue)
		    {
			    ImGui::Text("  queued: %s on %s and %s", queued.name.c_str(), ThingText(queued.first).c_str(),
			                ThingText(queued.second).c_str());
		    }
	    });
	if (count == 0)
	{
		ImGui::TextUnformatted("No computer player's hand was given anything yet");
	}
	DrawMinds(Locator::entitiesRegistry::value());
}
