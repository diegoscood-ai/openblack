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

#include "3D/SnowCover.h"
#include "3D/TempleChallengePick.h"
#include "3D/TempleChallengeRoom.h"
#include "3D/TempleDoors.h"
#include "3D/TempleInteriorInterface.h"
#include "3D/TemplePool.h"
#include "Common/MachineClock.h"
#include "Creature/LocalPlayer.h"
#include "ECS/ChallengeRecords.h"
#include "ECS/Components/ChallengePicture.h"
#include "ECS/Components/ChallengeRecord.h"
#include "ECS/Components/LeashPost.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MistDome.h"
#include "ECS/Components/NotDrawn.h"
#include "ECS/Components/ShowNeeds.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/MapCells.h"
#include "ECS/PlayerColours.h"
#include "ECS/Registry.h"
#include "ECS/ShowNeeds.h"
#include "ECS/Statistics.h"
#include "ECS/StatisticsRules.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/GameStatsSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HeartbeatSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "ECS/Systems/TempleExteriorSystemInterface.h"
#include "ECS/Town/BuildingSites.h"
#include "EngineConfig.h"
#include "Game/GameStats.h"
#include "Graphics/LightColumn.h"
#include "Help/HelpProfile.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Temple/HeartFade.h"
#include "Temple/TempleHelp.h"
#include "Temple/TempleHelpScripts.h"
#include "TempleAlignmentModel.h"
#include "Worship/Citadel.h"
#include "Worship/LeashPosts.h"
#include "Worship/TempleLeash.h"
#include "Worship/WorshipSite.h"

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

		// The rooms other than the main one open once the main room has been drawn in this game
		ImGui::Text("Rooms opened: %s", temple.RoomsOpened() ? "yes" : "no (only the main room)");
		if (!temple.RoomsOpened())
		{
			ImGui::SameLine();
			if (ImGui::Button("Open the rooms"))
			{
				temple.OpenRooms();
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

		// The help script the room the player is in starts as the player comes into it, and whether it would start
		const auto roomHelp = temple_help::RoomHelp(temple.GetCurrentRoom());
		ImGui::Text("Room help: %s (help system %s)", roomHelp.has_value() ? std::string(*roomHelp).c_str() : "none",
		            temple_help::HelpSystemOn() ? "on" : "off");

		// The main room's doorway, which is kept from one visit to the next: its leaf (the way out's turns none) and its
		// swing, from 0 to 2
		const auto& doors = temple.GetDoors();
		const auto leaf = doors.GetLeaf();
		std::string doorway = "none";
		if (leaf.has_value())
		{
			doorway = *leaf == TempleDoors::k_ExitLeaf ? std::string("the way out") : fmt::format("leaf {}", *leaf);
		}
		ImGui::Text("Doorway: %s, swing %.3f", doorway.c_str(), static_cast<double>(doors.GetSwing()));

		// The world's dialogue the temple keeps while the player is inside
		if (const auto owner = temple.GetKeptDialogueOwner())
		{
			ImGui::Text("World dialogue kept: task %u", *owner);
		}
		else
		{
			ImGui::TextUnformatted("World dialogue kept: none");
		}

		// The options room: whether it has opened the options and waits for them to close (or for the game to start
		// again) before going back to the main room
		const auto dialogs = temple.GetRoomDialogs();
		ImGui::Text("Options room: %s", dialogs.optionsOpened ? "the options opened" : "not opened");
		// The future room's words, which once shown in a visit turn the room by themselves
		ImGui::Text("Future room: words %s, alpha %s", dialogs.futureWordsShown ? "shown" : "not shown",
		            dialogs.futureAlpha.has_value() ? fmt::format("{:.0f}", *dialogs.futureAlpha).c_str() : "none");

		// The lines of the scroll looked at close up that picture a control, drawn over it on the screen
		const auto pictures = temple.GetScrollPictures();
		ImGui::Text("Scroll pictures: %zu", pictures.size());
		for (const auto& picture : pictures)
		{
			ImGui::Text("    control %d after the text at %.1f, line at %.1f", picture.control,
			            static_cast<double>(picture.lineX), static_cast<double>(picture.lineY));
		}

		// The save game room's figures: the profile's time played and its saves and loads
		if (Locator::playerProfileSystem::has_value())
		{
			const auto& profiles = Locator::playerProfileSystem::value();
			const auto profile = profiles.GetCurrentProfile();
			ImGui::Text("Save game room: %u s played, %u saves, %u loads", profiles.TimePlayed(machine_clock::WallTime()),
			            profile.has_value() ? profile->statistics.saves : 0U,
			            profile.has_value() ? profile->statistics.loads : 0U);
		}

		// The players' influence circles over the main room's map, while its influence button shows them
		if (const auto influence = temple.GetMapInfluence())
		{
			ImGui::Text("Influence on the map: at (%.2f, %.2f, %.2f), scale %.5f", static_cast<double>(influence->offset.x),
			            static_cast<double>(influence->offset.y), static_cast<double>(influence->offset.z),
			            static_cast<double>(influence->scale));
		}
		else
		{
			ImGui::TextUnformatted("Influence on the map: not shown");
		}
	}
	else
	{
		ImGui::Text("No temple");
	}

	DrawChallenges();
	DrawStatistics();
	DrawAlignment();
	DrawLeashPosts();
	DrawAltarNeeds();
	DrawLightColumns();
	DrawCreatureRoom();
	DrawTempleFog();
}

void TempleInterior::DrawStatistics() noexcept
{
	// The local player's game statistics, as the world scroll and the statistics page read them
	if (!ImGui::CollapsingHeader("Statistics") || !Locator::entitiesRegistry::has_value() ||
	    !Locator::gameStatsSystem::has_value() || !Locator::playerSystem::has_value())
	{
		return;
	}
	const auto player = Locator::playerSystem::value().GetLocalPlayer();
	const auto& stats = game_stats::Of(static_cast<size_t>(player));
	ImGui::Text("Births %u, deaths %u, killed %u, sacrifices %u", stats.births, stats.deaths, stats.killedVillagers,
	            stats.sacrifices);
	ImGui::Text("Food eaten %u, wood used %u, buildings built %u, stopped %u", stats.foodEaten, stats.woodUsed,
	            stats.buildingsBuilt, stats.buildingStopped);
	ImGui::Text("Population most %u, fewest %u; men %u..%u, women %u..%u", stats.maxPopulation, stats.minPopulation,
	            stats.minMales, stats.maxMales, stats.minFemales, stats.maxFemales);
	ImGui::Text("Histories: population %u buckets, influence %u buckets", stats.population.count, stats.influence.count);
	const auto& magic = magic::players::ReadMagicOf(player);
	ImGui::Text("Miracles cast: aggressive %u, nice %u, creature %u",
	            statistics::MiraclesCast(magic.castCount, statistics::MiracleKind::Aggressive),
	            statistics::MiraclesCast(magic.castCount, statistics::MiracleKind::Nice),
	            statistics::MiraclesCast(magic.castCount, statistics::MiracleKind::Creature));
	const auto world = ecs::player_statistics::World(player);
	ImGui::Text("World scroll: %d people, %d%% believers, %d%% male, %d buildings, %d wonders, %d disciples, %d missionaries",
	            world.population, world.believersPercent, world.malePercent, world.buildings, world.wonders, world.disciples,
	            world.discipleKinds.at(5));
	// The creature's tallies and the figures kept once for everyone, as the Game Statistics box shows them
	const auto tallies = ecs::player_statistics::CreatureTalliesOf(player);
	ImGui::Text("Creature killed: %u people, %u animals, %u creatures; fights %u, won %u; poos %u, mushrooms %u",
	            tallies.peopleKilled, tallies.animalsKilled, tallies.creaturesKilled, tallies.battlesFought, tallies.battlesWon,
	            tallies.poos, tallies.mushroomsEaten);
	const auto& statics = game_stats::GetStatics();
	ImGui::Text("Lines of code %u (towns at the start %u), frame rate most %.0f fewest %.0f, entities made %u",
	            statics.linesOfCode, statics.townCount, static_cast<double>(statics.maxFrameRate),
	            static_cast<double>(statics.minFrameRate), Locator::entitiesRegistry::value().GetEntitiesMade());
	if (Locator::helpProfileSystem::has_value())
	{
		const auto& profile = std::as_const(Locator::helpProfileSystem::value()).Get();
		ImGui::Text("Fight help counts: block %u, attack %u, cast %u, step %u",
		            profile.Count(help::profile::k_FightBlock).Total(), profile.Count(help::profile::k_FightAttack).Total(),
		            profile.Count(help::profile::k_FightCast).Total(), profile.Count(help::profile::k_FightStep).Total());
	}
}

void TempleInterior::DrawChallenges() noexcept
{
	// The challenges found, in the challenge room's slots: the first 21, a quest's in a gold frame
	if (!ImGui::CollapsingHeader("Challenges") || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto records = ecs::challenge_records::InOrder(registry);
	ImGui::Text("%zu found, the room shows %zu", records.size(),
	            std::min<size_t>(records.size(), temple_challenge_room::k_Slots));
	for (size_t slot = 0; slot < records.size(); ++slot)
	{
		const auto& record = registry.Get<const ecs::components::ChallengeRecord>(records[slot]);
		ImGui::Text("slot %zu: challenge %u, title %u, success %.3f%s, %s frame%s%s", slot, record.challenge, record.title,
		            static_cast<double>(record.success), ecs::challenge_records::IsDone(record) ? " (done)" : "",
		            record.quest ? "gold" : "silver", temple_challenge_pick::CanChoose(record.success) ? ", replay button" : "",
		            slot < temple_challenge_room::k_Slots ? "" : " (not shown)");
		// and its picture: how many were taken, and whether one is waiting for the frame's draw
		const auto* picture = registry.TryGet<const ecs::components::ChallengePicture>(records[slot]);
		// (pictures asked in the same frame wait under one name: they are of one frame)
		ImGui::Text("    picture: %u taken%s", picture != nullptr ? picture->taken : 0u,
		            picture != nullptr && picture->waiting.has_value()
		                ? fmt::format(", one waiting as {}", *picture->waiting).c_str()
		                : "");
	}
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
	// The snow lying on the heart, and, once it is drawn see-through as it is destroyed, how much of it still shows
	const auto& lookup = std::as_const(Locator::entitiesRegistry::value());
	const auto* temple = lookup.TryGet<const ecs::components::Temple>(heart);
	const auto* transform = lookup.TryGet<const ecs::components::Transform>(heart);
	if (temple != nullptr && transform != nullptr && Locator::snowSystem::has_value())
	{
		const auto level =
		    snow_cover::ObjectLevel(Locator::snowSystem::value().GetDepth({transform->position.x, transform->position.z}),
		                            snow_cover::k_ObjectRate, snow_cover::k_ObjectRate);
		if (heart_fade::TurnsSeeThrough(temple->fadeAlpha, temple->destroying))
		{
			const auto alpha = heart_fade::DrawnAlpha(temple->fadeAlpha);
			ImGui::Text("Snow on the heart: %d, drawn %d at its alpha %u", level, heart_fade::SnowLevel(level, alpha),
			            static_cast<unsigned>(alpha));
		}
		else
		{
			ImGui::Text("Snow on the heart: %d", level);
		}
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

void TempleInterior::DrawAltarNeeds() noexcept
{
	if (!ImGui::TreeNode("Altar need signs"))
	{
		return;
	}
	const auto citadel = Locator::entitiesRegistry::has_value() ? worship::citadel::Of(_player) : entt::entity {entt::null};
	const auto* sites =
	    citadel != entt::null
	        ? std::as_const(Locator::entitiesRegistry::value()).TryGet<const ecs::components::CitadelWorship>(citadel)
	        : nullptr;
	if (sites == nullptr)
	{
		ImGui::TextUnformatted("No citadel for this player");
		ImGui::TreePop();
		return;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	constexpr std::array<std::string_view, ecs::show_needs::k_AltarSigns> k_Signs {"Hunger", "Rest", "People hiding"};
	bool any = false;
	for (const auto site : sites->sites)
	{
		const auto* w =
		    site != entt::null && registry.Valid(site) ? registry.TryGet<const ecs::components::WorshipSite>(site) : nullptr;
		if (w == nullptr)
		{
			continue;
		}
		any = true;
		const auto tribe = static_cast<size_t>(w->tribe);
		ImGui::Text("%s site (slot %u)", tribe < k_TribeStrs.size() ? k_TribeStrs.at(tribe).data() : "NONE",
		            static_cast<unsigned>(w->slot));
		// how far the site and its altar are built, and the site's building site with its builders
		if (const auto* part = registry.TryGet<const ecs::components::CitadelPartBuild>(site); part != nullptr)
		{
			const auto buildingSite = part->buildingSite;
			const bool hasSite = buildingSite != entt::null && registry.Valid(buildingSite);
			ImGui::Text(
			    "  Built %.0f %%%s; building site: %s, %d builders", static_cast<double>(part->percentBuilt * 100.0f),
			    (part->buildFlags & ecs::components::CitadelPartBuild::k_UnderConstruction) != 0 ? " (under construction)" : "",
			    hasSite ? "yes" : "none", hasSite ? ecs::building_sites::GetBuilderCount(buildingSite) : 0);
		}
		if (const auto* altar = w->totem != entt::null && registry.Valid(w->totem)
		                            ? registry.TryGet<const ecs::components::CitadelPartBuild>(w->totem)
		                            : nullptr)
		{
			ImGui::Text("  Altar built %.0f %%, %s", static_cast<double>(altar->percentBuilt * 100.0f),
			            registry.AllOf<ecs::components::NotDrawn>(w->totem) ? "not drawn" : "drawn");
		}
		// the altar in the map cells, and the site the cell lookup finds where it stands
		if (const auto* at = w->totem != entt::null && registry.Valid(w->totem)
		                         ? registry.TryGet<const ecs::components::Transform>(w->totem)
		                         : nullptr)
		{
			const auto found = worship::site::FindAt(at->position);
			ImGui::Text("  Altar: %s the map cells; the site found there: %s",
			            ecs::map_cells::IsObjectInMap(w->totem) ? "in" : "not in",
			            found == site ? "this one" : (found == entt::null ? "none" : "another"));
		}
		for (size_t row = 0; row < k_Signs.size(); ++row)
		{
			const auto sign = w->needsSigns.at(row);
			const auto* v = sign != entt::null && registry.Valid(sign)
			                    ? registry.TryGet<const ecs::components::ShowNeedsVisuals>(sign)
			                    : nullptr;
			if (v == nullptr)
			{
				ImGui::Text("  %s: no sign", k_Signs.at(row).data());
				continue;
			}
			ImGui::Text("  %s: desire %.3f, shows %.3f, %s", k_Signs.at(row).data(), static_cast<double>(v->desire),
			            static_cast<double>(v->height.value),
			            registry.AllOf<ecs::components::Mesh>(sign) ? "drawn" : "not drawn");
		}
	}
	if (!any)
	{
		ImGui::TextUnformatted("No worship site for this player");
	}
	ImGui::TextUnformatted("Each sign takes its site's desire every turn and rises to it in a second, drawn only while "
	                       "the site is built and the desire is above the sign's least");
	ImGui::TreePop();
}

void TempleInterior::Update() noexcept {}

void TempleInterior::ProcessEventOpen([[maybe_unused]] const SDL_Event& event) noexcept {}

void TempleInterior::ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept {}

void TempleInterior::DrawTempleFog() noexcept
{
	if (!ImGui::TreeNode("Temple fog"))
	{
		return;
	}
	// the drawing alone: nothing of the game reads the fog
	if (Locator::config::has_value())
	{
		ImGui::Checkbox("Draw the rooms in their fog", &Locator::config::value().drawTempleFog);
	}
	if (!Locator::temple::has_value() || !Locator::temple::value().Active())
	{
		ImGui::TextUnformatted("Not inside the temple");
		ImGui::TreePop();
		return;
	}
	const auto& light = Locator::temple::value().GetLight();
	const auto end = temple_fade::EndColour(light.multiply);
	ImGui::Text("From %.0f to %.0f in front of the camera, in %d steps", light.fade.from, light.fade.to, temple_fade::k_Steps);
	ImGui::Text("Colour added at its end: %.1f, %.1f, %.1f of 255", end.r, end.g, end.b);
	ImGui::Text("Share left at its end: %u of 256", temple_fade::ShareOfStep(temple_fade::k_Steps - 1));
	// the pool, which is drawn unfogged
	const float poolTime = Locator::temple::value().GetPoolTime();
	const auto pool = temple_pool::Layers(poolTime);
	ImGui::Text("Pool: %.3f s on its clock, its layers' alpha %d and %d", static_cast<double>(poolTime), pool[0].alpha,
	            pool[1].alpha);
	ImGui::TreePop();
}

void TempleInterior::DrawLightColumns() noexcept
{
	if (!ImGui::TreeNode("Light columns"))
	{
		return;
	}
	// the drawing alone: nothing of the game reads the columns
	if (Locator::config::has_value())
	{
		ImGui::Checkbox("Draw the light columns", &Locator::config::value().drawLightColumns);
	}
	if (Locator::influenceSystem::has_value())
	{
		// the temple's border is the one of its player's colour index on the land
		const auto colourIndex = ecs::player_colours::OnThisLand(_player);
		ImGui::Text("Colour index on this land: %d", static_cast<int>(colourIndex));
		ImGui::Text("Border shown: %s", Locator::influenceSystem::value().IsBorderShown(colourIndex) ? "yes" : "no");
	}
	const auto state = Locator::heartbeatSystem::has_value() ? Locator::heartbeatSystem::value().StateOf(_player)
	                                                         : std::optional<ecs::heartbeat::State> {};
	if (!state.has_value())
	{
		ImGui::TextUnformatted("No heartbeat for this player (only the local player's beats)");
		ImGui::TreePop();
		return;
	}
	ImGui::Text("Danger %.3f, rate %.3f (%.3f beats a second)", state->stress, state->rate,
	            state->rate * ecs::heartbeat::k_BeatsPerRate);
	ImGui::Text("Phase %.3f, pulse %.3f (last turn %.3f)", state->phase, state->pulse, state->previousPulse);
	ImGui::Text("Held by the destruction: %.3f", state->destructionBeats);
	ImGui::Text("Temple column strength now: %u of 255",
	            static_cast<unsigned>(graphics::light_column::TempleAlpha(state->pulse)));
	ImGui::TreePop();
}

void TempleInterior::DrawCreatureRoom() noexcept
{
	ImGui::Separator();
	if (!ImGui::CollapsingHeader("Creature room"))
	{
		return;
	}
	if (!Locator::creatureCaveSystem::has_value())
	{
		ImGui::TextUnformatted("No creature cave");
		return;
	}
	auto& cave = Locator::creatureCaveSystem::value();
	// As a script sets it: it counts the next time the room starts in a visit to the temple
	bool shown = cave.IsCreatureShown();
	if (ImGui::Checkbox("Creature shown", &shown))
	{
		cave.SetCreatureShown(shown);
	}
	// What the room's camera last found under the cursor: the creature's body as it is drawn
	ImGui::Text("Cursor over the creature: %s", cave.WasCursorOverCaveCreature() ? "yes" : "no");
	// The waterfall's texture, which slides on only while the room is drawn
	if (Locator::temple::has_value())
	{
		ImGui::Text("Waterfall slide: %.6f down", static_cast<double>(Locator::temple::value().GetWaterfallSlide().y));
	}
	// The mists at the waterfall's foot, coloured in the temple's light as the room started. Read through the const
	// registry, which makes no storage of mists while there are none.
	if (Locator::entitiesRegistry::has_value())
	{
		std::as_const(Locator::entitiesRegistry::value())
		    .Each<const ecs::components::MistDome>([](entt::entity, const ecs::components::MistDome& mist) {
			    ImGui::Text("Mist: colour %08X, frame counter %d", mist.colour, mist.clock.counter);
		    });
	}
}
