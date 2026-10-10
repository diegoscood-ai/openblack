/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Temple.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>

#include <fmt/format.h>

#include "3D/TempleInteriorInterface.h"
#include "Creature/LocalPlayer.h"
#include "ECS/Components/LeashPost.h"
#include "ECS/Components/NotDrawn.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TempleExteriorSystemInterface.h"
#include "EngineConfig.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "TempleAlignmentModel.h"
#include "Worship/Citadel.h"
#include "Worship/LeashPosts.h"
#include "Worship/TempleLeash.h"

using namespace openblack;
using namespace debug::gui;

TempleInterior::TempleInterior() noexcept
    : Window("Citadel", ImVec2(360.0f, 420.0f))
{
}

const std::array<std::tuple<TempleRoom, std::string_view>, 7> k_Lookup {
    std::make_tuple(TempleRoom::Challenge, "Challenge Room"),   //
    std::make_tuple(TempleRoom::CreatureCave, "Creature Cave"), //
    std::make_tuple(TempleRoom::Credits, "Credits Room"),       //
    std::make_tuple(TempleRoom::Main, "Hall"),                  //
    std::make_tuple(TempleRoom::Multi, "Multiplayer Room"),     //
    std::make_tuple(TempleRoom::Options, "Options Room"),       //
    std::make_tuple(TempleRoom::SaveGame, "Save Game Room"),    //
};

void TempleInterior::Draw() noexcept
{
	if (Locator::temple::has_value())
	{
		auto& temple = Locator::temple::value();
		const auto inactive = !temple.Active();

		if (ImGui::Button(inactive ? "Enter temple" : "Exit temple"))
		{
			if (inactive)
			{
				temple.Activate();
			}
			else
			{
				temple.Deactivate();
			}
		}

		// Outside, each room's button takes the player into the temple at that room; inside, it goes to the room
		for (auto [room, name] : k_Lookup)
		{
			if (ImGui::Button(fmt::format("{}", name).c_str()))
			{
				if (inactive)
				{
					temple.Activate(static_cast<TempleRoom>(room));
				}
				else
				{
					temple.EnterRoom(room);
				}
			}
		}
	}
	else
	{
		ImGui::Text("No temple");
	}

	DrawAlignment();
	DrawLeashPosts();
}

void TempleInterior::DrawAlignment() noexcept
{
	ImGui::Separator();
	ImGui::TextUnformatted("Alignment");
	if (!Locator::playerSystem::has_value() || !Locator::alignmentSystem::has_value())
	{
		ImGui::TextUnformatted("No players");
		return;
	}
	const auto players = temple_alignment::PlayersInGame(&magic::players::EntityOf);
	if (players.empty())
	{
		ImGui::TextUnformatted("No players");
		return;
	}
	if (std::ranges::find(players, _player) == players.end())
	{
		_player = players.front();
	}
	if (ImGui::BeginCombo("Player", k_PlayerNamesStrs.at(static_cast<size_t>(_player)).data()))
	{
		for (const auto player : players)
		{
			if (ImGui::Selectable(k_PlayerNamesStrs.at(static_cast<size_t>(player)).data(), player == _player))
			{
				_player = player;
			}
		}
		ImGui::EndCombo();
	}

	// Only a move of the slider or a press of a button writes the alignment
	auto& alignment = Locator::alignmentSystem::value();
	float value = alignment.GetPlayerAlignment(_player);
	if (ImGui::SliderFloat("Evil to good", &value, -1.0f, 1.0f, "%.3f"))
	{
		temple_alignment::SetAlignment(alignment, _player, value);
	}
	for (const auto& [label, preset] : {std::pair {"Evil", -1.0f}, std::pair {"Neutral", 0.0f}, std::pair {"Good", 1.0f}})
	{
		if (preset != -1.0f)
		{
			ImGui::SameLine();
		}
		if (ImGui::Button(label))
		{
			temple_alignment::SetAlignment(alignment, _player, preset);
		}
	}
	ImGui::Text("Alignment %.3f", alignment.GetPlayerAlignment(_player));

	// The player's temple outside, which takes the alignment a step a turn
	const auto heart = Locator::entitiesRegistry::has_value() ? worship::citadel::HeartOf(worship::citadel::Of(_player))
	                                                          : entt::entity {entt::null};
	const auto look = heart != entt::null && Locator::templeExteriorSystem::has_value()
	                      ? Locator::templeExteriorSystem::value().GetLook(heart)
	                      : std::nullopt;
	if (!look)
	{
		ImGui::TextUnformatted("No temple outside for this player");
		return;
	}
	const float targetNow = temple_alignment::AlignmentTargetNow(alignment, _player);
	ImGui::TextUnformatted(temple_alignment::LookStatus(*look, targetNow).c_str());
	ImGui::ProgressBar(look->alignment, ImVec2(-1.0f, 0.0f), "evil to good");
	if (ImGui::Button("Snap the outside now"))
	{
		temple_alignment::SnapOutside(alignment, Locator::templeExteriorSystem::value(), heart, _player);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Without it the outside follows a step a turn, as the original's");
	}
}

void TempleInterior::DrawLeashPosts() noexcept
{
	if (!ImGui::TreeNode("Leash posts"))
	{
		return;
	}
	// the drawing alone: the posts' spin moves on either way
	if (Locator::config::has_value())
	{
		ImGui::Checkbox("Draw the posts", &Locator::config::value().drawLeashPosts);
	}
	const auto heart = Locator::entitiesRegistry::has_value() ? worship::citadel::HeartOf(worship::citadel::Of(_player))
	                                                          : entt::entity {entt::null};
	const auto* leashes =
	    heart != entt::null
	        ? std::as_const(Locator::entitiesRegistry::value()).TryGet<const ecs::components::TempleLeash>(heart)
	        : nullptr;
	if (leashes == nullptr)
	{
		ImGui::TextUnformatted("No temple posts for this player");
		ImGui::TreePop();
		return;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	constexpr std::array<std::string_view, worship::leash_posts::k_Count> k_Leashes {"Aggression", "Learning", "Compassion"};
	const auto pick = leashes->pick;
	ImGui::Text("Picked: %s (%d)",
	            pick == worship::leash_posts::k_NoPick
	                ? "none"
	                : k_Leashes.at(static_cast<size_t>(worship::leash_posts::PickedType(pick)) - 1).data(),
	            pick);
	// Only a press writes the pick (the temple's alone, as the leash's packet does at the turn)
	for (size_t post = 0; post < k_Leashes.size(); ++post)
	{
		if (post != 0)
		{
			ImGui::SameLine();
		}
		if (ImGui::Button(fmt::format("Pick {}", k_Leashes.at(post)).c_str()))
		{
			worship::temple_leash::SetPick(heart, worship::leash_posts::TypeOf(post));
		}
	}
	// The hand's tap on a post, as the local interface: the click, the pick at once and the leash sent to the next
	// turn, or the picked post unpicked. Only the local player's own posts take it
	for (size_t post = 0; post < k_Leashes.size(); ++post)
	{
		if (post != 0)
		{
			ImGui::SameLine();
		}
		if (ImGui::Button(fmt::format("Tap {}", k_Leashes.at(post)).c_str()))
		{
			worship::temple_leash::InterfaceTap(leashes->posts.at(post), true);
		}
	}
	if (ImGui::BeginTable("LeashPosts", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
	{
		ImGui::TableSetupColumn("Post");
		ImGui::TableSetupColumn("Point");
		ImGui::TableSetupColumn("Scroll, X turn, Z turn");
		ImGui::TableSetupColumn("Smoke clock");
		ImGui::TableHeadersRow();
		for (size_t post = 0; post < leashes->posts.size(); ++post)
		{
			const auto entity = leashes->posts.at(post);
			const auto* p = registry.Valid(entity) ? registry.TryGet<const ecs::components::LeashPost>(entity) : nullptr;
			const auto* t = registry.Valid(entity) ? registry.TryGet<const ecs::components::Transform>(entity) : nullptr;
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%zu %s", post, k_Leashes.at(post).data());
			if (p == nullptr || t == nullptr)
			{
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("gone");
				continue;
			}
			ImGui::TableNextColumn();
			ImGui::Text("%.3f, %.3f, %.3f", t->position.x, t->position.y, t->position.z);
			ImGui::TableNextColumn();
			ImGui::Text("%.6f, %.6f, %.6f", p->spin.scroll, p->spin.xAngle, p->spin.zAngle);
			ImGui::TableNextColumn();
			ImGui::Text("%.6f (cell %u)", p->spin.frame, static_cast<unsigned>(worship::leash_posts::SpriteCell(p->spin)));
		}
		ImGui::EndTable();
	}
	// which posts show this frame: a fully built temple, its player's creature knowing the leash
	if (Locator::leashSystem::has_value())
	{
		std::string shown;
		for (const auto entity : worship::temple_leash::ShownPosts(heart, Locator::leashSystem::value()))
		{
			const auto& p = registry.Get<const ecs::components::LeashPost>(entity);
			shown += fmt::format("{}{}", shown.empty() ? "" : ", ", k_Leashes.at(p.index));
		}
		ImGui::Text("Shown now: %s", shown.empty() ? "none" : shown.c_str());
		// what the hand can pick of this temple's posts this frame: a sphere of radius 1 at each post's point now
		bool handHidden = false;
		if (Locator::handSystem::has_value())
		{
			const auto hand = Locator::handSystem::value().GetPlayerHands()[0];
			handHidden = registry.Valid(hand) && registry.AllOf<ecs::components::NotDrawn>(hand);
		}
		std::string felt;
		for (const auto& post :
		     worship::temple_leash::HandPosts(Locator::leashSystem::value(), creature::LocalPlayer(), handHidden))
		{
			const auto& p = registry.Get<const ecs::components::LeashPost>(post.post);
			if (p.heart == heart)
			{
				felt += fmt::format("{}{} at ({:.2f}, {:.2f}, {:.2f})", felt.empty() ? "" : "; ", k_Leashes.at(p.index),
				                    post.point.x, post.point.y, post.point.z);
			}
		}
		ImGui::TextWrapped("Felt by the hand now (radius %.1f): %s",
		                   static_cast<double>(worship::leash_posts::k_HandCollisionRadius),
		                   felt.empty() ? "none" : felt.c_str());
	}
	ImGui::TextUnformatted("The spin holds the four draws each post took when the temple was made, moved on each frame "
	                       "it is shown");
	ImGui::TreePop();
}

void TempleInterior::Update() noexcept {}

void TempleInterior::ProcessEventOpen([[maybe_unused]] const SDL_Event& event) noexcept {}

void TempleInterior::ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept {}
