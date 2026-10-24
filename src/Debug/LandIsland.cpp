/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandIsland.h"

#include <cstddef>

#include <algorithm>
#include <array>
#include <span>
#include <utility>

#include <LNDFile.h>

#include "3D/BlockBake.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "Audio/Services/Confirmation.h"
#include "Audio/Services/Guidance.h"
#include "Camera/ScriptCamera.h"
#include "Debug/GameHost.h"
#include "Debug/ImGuiUtils.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/DrawList/DrawGate.h"
#include "ECS/DrawList/ObjectList.h"
#include "ECS/HeldApply.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/ScriptTimer.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "Editor/EditorEntities.h"
#include "Editor/EditorOutline.h"
#include "EngineConfig.h"
#include "GameClock.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/ObjectListFrame.h"
#include "Help/HelpSystem.h"
#include "Help/InterfaceInteraction.h"
#include "Help/ScriptControl.h"
#include "Help/ToolTips.h"
#include "Input/HandDemo.h"
#include "Locator.h"
#include "Magic/Hand/HandMagicFX.h"
#include "Worship/Citadel.h"
#include "Worship/PlayerSpellIcons.h"

using namespace openblack::debug::gui;
using namespace openblack::graphics;

LandIsland::LandIsland() noexcept
    : Window("Land Island", ImVec2(600.0f, 600.0f))
{
}

void LandIsland::Draw() noexcept
{
	auto& config = Locator::config::value();

	const auto& landIsland = Locator::terrainSystem::value();

	ImGui::SliderFloat("Bump", &config.bumpMapStrength, 0.0f, 1.0f, "%.3f");
	ImGui::SliderFloat("Small Bump", &config.smallBumpMapStrength, 0.0f, 1.0f, "%.3f");

	ImGui::Separator();

	ImGui::Text("Block Count: %zu", landIsland.GetBlocks().size());
	ImGui::Text("Country Count: %zu", landIsland.GetCountries().size());
	// the creatures' regions are made on the game's first land and kept by every later one
	if (Locator::entitiesRegistry::has_value())
	{
		const auto& regions = std::as_const(Locator::entitiesRegistry::value()).Context().explorationRegions;
		if (regions.Map() == nullptr)
		{
			ImGui::TextUnformatted("Creature regions: not made yet");
		}
		else
		{
			ImGui::Text("Creature regions made from: %s",
			            regions.MadeFrom().empty() ? "the land, when first asked" : regions.MadeFrom().c_str());
		}
	}

	ImGui::Separator();

	if (ImGui::TreeNodeEx("Height Map", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const auto indexExtent = landIsland.GetIndexExtent();
		const auto extentSize = indexExtent.maximum - indexExtent.minimum;
		const auto dim = static_cast<uint16_t>(LandIslandInterface::k_CellCount) * extentSize;
		const auto& texture = landIsland.GetHeightMap();
		ImGui::Text("Resolution: %ux%u", dim.x, dim.y);
		const float scaling = 512.0f / static_cast<float>(dim.x);
		ImGui::Image(toBgfx(texture.GetNativeHandle()), ImVec2(dim.x * scaling, dim.y * scaling));
		ImGui::TreePop();
	}

	DrawBlockTextures();
	DrawObjectList();
	DrawDesireFlags();
	DrawKeptByALandChange();

	ImGui::Separator();

	if (ImGui::Button("Dump Textures"))
	{
		landIsland.DumpTextures();
	}

	if (ImGui::Button("Dump Heightmap"))
	{
		landIsland.DumpMaps();
	}
}

void LandIsland::DrawBlockTextures() noexcept
{
	if (!ImGui::TreeNode("Block textures"))
	{
		return;
	}
	auto& island = Locator::terrainSystem::value();
	ImGui::Text("Waiting to be painted again: %zu", island.BlockTextureRequests());
	// as the original's request to update all the land's textures: each block is painted again once it is drawn,
	// the nearest first
	if (ImGui::Button("Paint every block again"))
	{
		island.RepaintAllBlockTextures();
	}
	// which layers the prints step bakes into the blocks' textures, all of them unless one is unticked: shown as each
	// block is painted again (the button above)
	if (ImGui::TreeNode("Show bake layers"))
	{
		auto layers = island.BakeLayers();
		const auto layer = [&layers](const char* name, uint8_t bit) {
			bool on = (layers & bit) != 0;
			if (ImGui::Checkbox(name, &on))
			{
				layers = static_cast<uint8_t>(on ? layers | bit : layers & ~bit);
			}
		};
		layer("Static shadows", block_texture::k_BakeShadows);
		layer("Footprints", block_texture::k_BakeFootprints);
		layer("River beds", block_texture::k_BakeLandPrints);
		if (layers != island.BakeLayers())
		{
			island.SetBakeLayers(layers);
		}
		ImGui::TreePop();
	}
	// the texture detail line: beyond it a block shows its low-resolution tile, in its band both
	bool levels = island.ShowTextureLevels();
	if (ImGui::Checkbox("Texture detail line", &levels))
	{
		island.SetShowTextureLevels(levels);
	}
	ImGui::SetItemTooltip("Each block tinted by what it shows: red its low-resolution tile only, blue its texture with "
	                      "the tile blended over it across the line's band, untinted its own texture");
	bool tilesOnly = island.TilesOnly();
	if (ImGui::Checkbox("Low-resolution tiles only", &tilesOnly))
	{
		island.SetTilesOnly(tilesOnly);
	}
	ImGui::SetItemTooltip("Every block shows its low-resolution tile, as a block beyond the texture detail line does");
	ImGui::TreePop();
}

void LandIsland::DrawObjectList() noexcept
{
	if (!ImGui::TreeNode("Object draw list"))
	{
		return;
	}
	if (Locator::config::has_value())
	{
		ImGui::Checkbox("Draw only what the list draws", &Locator::config::value().drawFollowsList);
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("The map's objects the list does not draw this frame are left out, as the original's");
		}
	}
	if (!Locator::objectDrawListSystem::has_value())
	{
		ImGui::TextUnformatted("No object draw list");
		ImGui::TreePop();
		return;
	}
	const auto& list = Locator::objectDrawListSystem::value();
	ImGui::Text("Entries: %zu of %zu%s", list.Count(), ecs::draw_list::k_MaxEntries,
	            list.Count() >= ecs::draw_list::k_MaxEntries ? " (full)" : "");
	ImGui::Text("Last pass: %s", list.LastPassFull() ? "full" : "still");
	ImGui::Text("Last rebuild at turn %u, now turn %u; rebuilds asked for: %u", list.RebuildTurn(), game_clock::Turn(),
	            static_cast<unsigned>(list.RebuildCount()));
	ImGui::Text("Drawn by the last pass: %zu", list.DrawnThisFrame().size());
	// the Norse gates' lamps take the light and haze of their gate's place (draw_gate::LampLightOrigin)
	if (Locator::entitiesRegistry::has_value() && ImGui::TreeNode("Gate lamps' light"))
	{
		const auto& registry = std::as_const(Locator::entitiesRegistry::value());
		registry.Each<const ecs::components::GateLamp>([&registry](entt::entity lamp, const ecs::components::GateLamp& gate) {
			if (const auto origin = ecs::draw_gate::LampLightOrigin(registry, lamp); origin.has_value())
			{
				ImGui::Text("Lamp %u: lit at gate %u's place (%.1f, %.1f, %.1f)",
				            static_cast<unsigned>(entt::to_integral(lamp)), static_cast<unsigned>(entt::to_integral(gate.gate)),
				            origin->x, origin->y, origin->z);
			}
			else
			{
				ImGui::Text("Lamp %u: its gate is gone, lit at its own place", static_cast<unsigned>(entt::to_integral(lamp)));
			}
		});
		ImGui::TreePop();
	}
	// how each mesh drawn on the last frame takes the land's light (land_light::ObjectLightOf), and how many things take
	// each light on their own row (the instance's fifth column), whatever mesh they share
	if (Locator::rendereringSystem::has_value() && ImGui::TreeNode("Land light by mesh"))
	{
		static constexpr std::array<const char*, 4> k_Modes = {"bilinear", "cell shifted", "one cell", "full light"};
		// the blocks this frame's cull drew, which alone take the light and shadow stamps
		ImGui::Text("Blocks stamped this frame: %zu", static_cast<size_t>(std::ranges::count(list.DrawnBlocks(), uint8_t {1})));
		const auto& context = Locator::rendereringSystem::value().GetContext();
		for (const auto& [meshId, light] : context.meshLandLight)
		{
			if (light.mode == land_light::ObjectMode::Bilinear && light.haze)
			{
				continue;
			}
			ImGui::Text("Mesh %u: %s, %s", static_cast<unsigned>(meshId), k_Modes.at(static_cast<size_t>(light.mode)),
			            light.haze ? "with the haze" : "no haze");
		}
		std::array<std::array<size_t, 2>, 4> things {};
		for (const auto& [entity, instance] : context.entityInstances)
		{
			if (instance.index >= context.instanceColours.size())
			{
				continue;
			}
			const auto code = argb_colour::InstanceLandLight(context.instanceColours[instance.index]);
			if (code == 0)
			{
				continue;
			}
			const auto own = land_light::ResolveObjectLight(0.0f, code);
			++things.at(static_cast<size_t>(own.mode)).at(own.haze ? 0 : 1);
		}
		for (size_t mode = 0; mode < things.size(); ++mode)
		{
			for (size_t haze = 0; haze < 2; ++haze)
			{
				if (things.at(mode).at(haze) != 0)
				{
					ImGui::Text("Things %s, %s: %zu", k_Modes.at(mode), haze == 0 ? "with the haze" : "no haze",
					            things.at(mode).at(haze));
				}
			}
		}
		ImGui::TreePop();
	}

	const auto* frame = _host != nullptr ? _host->GetObjectListFrame() : nullptr;
	if (frame == nullptr || !frame->ran)
	{
		ImGui::TextUnformatted("The list did not run on the last frame: nothing is left out");
		ImGui::TreePop();
		return;
	}
	const auto notListed = static_cast<size_t>(std::ranges::count(frame->reasons, graphics::ObjectListHidden::NotListed));
	const auto gateLamps = static_cast<size_t>(std::ranges::count(frame->reasons, graphics::ObjectListHidden::GateNotOnScreen));
	const auto buried = static_cast<size_t>(std::ranges::count(frame->reasons, graphics::ObjectListHidden::Buried));
	ImGui::Text("Left out of the last frame: %zu (%zu not in the list, %zu listed and not drawn, %zu gate lamps with their "
	            "gate off the screen, %zu piles buried in the land)",
	            frame->hidden.size(), notListed, frame->hidden.size() - notListed - gateLamps - buried, gateLamps, buried);
	if (!frame->hidden.empty() && Locator::entitiesRegistry::has_value() &&
	    ImGui::BeginTable("ObjectListHidden", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
	                      ImVec2(0.0f, 200.0f)))
	{
		const auto& registry = std::as_const(Locator::entitiesRegistry::value());
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Entity");
		ImGui::TableSetupColumn("Kind");
		ImGui::TableSetupColumn("Why");
		ImGui::TableHeadersRow();
		ImGuiListClipper clipper;
		clipper.Begin(static_cast<int>(frame->hidden.size()));
		while (clipper.Step())
		{
			for (auto row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
			{
				const auto index = static_cast<size_t>(row);
				const auto entity = frame->hidden[index];
				const bool exists = registry.Valid(entity);
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("%u", static_cast<unsigned>(entt::to_integral(entity)));
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(exists ? editor::Name(editor::KindOf(registry, entity)).data() : "(gone)");
				ImGui::TableNextColumn();
				const auto reason =
				    index < frame->reasons.size() ? frame->reasons[index] : graphics::ObjectListHidden::NotListed;
				ImGui::TextUnformatted(reason == graphics::ObjectListHidden::NotDrawn          ? "listed, not drawn"
				                       : reason == graphics::ObjectListHidden::GateNotOnScreen ? "its gate off the screen"
				                       : reason == graphics::ObjectListHidden::Buried          ? "a pile buried in the land"
				                                                                               : "not in the list");
			}
		}
		ImGui::EndTable();
	}
	ImGui::TreePop();
}

void LandIsland::DrawKeptByALandChange() noexcept
{
	if (!ImGui::TreeNode("Kept by a land change"))
	{
		return;
	}
	if (auto* helpSystem = openblack::help::Get(); helpSystem != nullptr)
	{
		ImGui::Text("Cinema bars: %s (task %u)", helpSystem->GetWideScreen() != 0 ? "on" : "off",
		            static_cast<unsigned>(helpSystem->GetWideScreenOwner()));
		// The bars as the game's own (task 0) turn them: coming in, they put every dialog away
		ImGui::SameLine();
		if (ImGui::Button(helpSystem->GetWideScreen() != 0 ? "Bars off" : "Bars on"))
		{
			helpSystem->SetWideScreen(helpSystem->GetWideScreen() != 0 ? 0 : 1, 0);
		}
	}
	else
	{
		ImGui::TextUnformatted("No help system");
	}
	if (Locator::cinematicDirectorSystem::has_value())
	{
		const auto& director = Locator::cinematicDirectorSystem::value();
		ImGui::Text("Close clipping: %s", director.IsCloseClipping() ? "on" : "off");
		ImGui::Text("Cinema bars drawn: %.3f", static_cast<double>(director.GetWideScreenFraction()));
		ImGui::Text("Script fade: 0x%08X (%s)", static_cast<unsigned>(director.GetFadeColour()),
		            director.IsFadeFinished() ? "still" : "fading");
	}
	ImGui::Text("Interface interaction: level %d", openblack::help::interface_interaction::GetLevel());
	const auto& cameraControl = openblack::help::script_control::GetCameraControl();
	ImGui::Text("Leashes drawn: %d, highlights drawn: %d", cameraControl.drawLeash, cameraControl.drawHighlight);
	ImGui::Text("Citadel interface: %u", static_cast<unsigned>(openblack::worship::citadel::InterfaceCitadel()));
	ImGui::Text("Hand demo: %s", openblack::hand_demo::IsPlaying() ? "playing" : "none");
	ImGui::Text("Debug camera mode: %d", openblack::script_camera::DebugCameraMode());
	ImGui::Text("Sound confirmation: %s", openblack::audio::confirmation::Get().active ? "on" : "off");
	ImGui::Text("Tooltip icon: %s", openblack::help::tooltips::HasIcon() ? "drawn" : "none");
	ImGui::Text("Sequence mode: %d", openblack::game_clock::SequenceMode());
	if (Locator::scriptState::has_value())
	{
		const auto& countdown = openblack::ecs::script_countdown::Get();
		ImGui::Text("Countdown timer: %s, %s, %d turns left", countdown.on ? "on" : "off", countdown.shown ? "shown" : "hidden",
		            countdown.turnsLeft);
	}
	if (Locator::worshipState::has_value())
	{
		ImGui::Text("Last seed type: %d",
		            static_cast<int>(openblack::worship::player::LastSeedType(openblack::PlayerNames::PLAYER_ONE)));
	}
	if (Locator::handSystem::has_value())
	{
		const auto velocity = Locator::handSystem::value().GetTurnHandVelocity();
		ImGui::Text("Synced hand's velocity: %.2f, %.2f, %.2f", static_cast<double>(velocity.x),
		            static_cast<double>(velocity.y), static_cast<double>(velocity.z));
		ImGui::Text("Hand's action state: %d", Locator::handSystem::value().GetActionStateNumber());
		ImGui::Text("Hand's still turns: %u", Locator::handSystem::value().GetStillTurns());
		// the building site a held object let go now would be over (a handful of wood over one going up goes into it)
		const auto held = Locator::handSystem::value().GetHeldObject();
		const auto* transform =
		    held && Locator::entitiesRegistry::has_value() && Locator::entitiesRegistry::value().Valid(*held)
		        ? Locator::entitiesRegistry::value().TryGet<const openblack::ecs::components::Transform>(*held)
		        : nullptr;
		if (transform != nullptr)
		{
			const auto site = openblack::ecs::held_apply::BuildingSiteAt(transform->position);
			ImGui::Text("Building site under the held object: %s", site != entt::null ? "yes" : "none");
			// whether that site takes it when it is let go (a pot of its resource, a tree, a dead tree or a fence)
			const bool takes = openblack::ecs::held_apply::BuildingSiteTakingReleased(*held) != entt::null;
			ImGui::Text("Let go now, it goes into the site: %s", takes ? "yes" : "no");
		}
	}
	if (Locator::influenceSystem::has_value() && Locator::entitiesRegistry::has_value())
	{
		if (const auto* kept = Locator::influenceSystem::value().GetVirtualInfluence(openblack::PlayerNames::PLAYER_ONE);
		    kept != nullptr)
		{
			ImGui::Text("Hand's virtual influence: strength %.2f, last in at turn %u%s", static_cast<double>(kept->fraction),
			            kept->lastInsideTurn, kept->disabled ? ", off" : "");
		}
	}
	if (Locator::audioState::has_value())
	{
		const auto& guidance = openblack::audio::guidance::GetState();
		ImGui::Text("Heart beat: value %.2f, pitch %.2f, phase %.2f", static_cast<double>(guidance.heartBeatValue),
		            static_cast<double>(guidance.heartBeatPitch), static_cast<double>(guidance.heartBeatPhase));
		const auto remembered = openblack::audio::guidance::RememberedThings();
		ImGui::Text("Guidance: remembers %zu things (%zu from older lands), %s", remembered.count, remembered.olderLand,
		            guidance.enabled ? "enabled" : "disabled");
	}
	if (Locator::magicSystem::has_value())
	{
		ImGui::Text("Hand's bracelets: %d, tribes' names: %zu", openblack::magic::hand_fx::PowerUpLevel(),
		            openblack::magic::hand_fx::GetTribalPowerRunners().size());
	}
	ImGui::Text("Camera lens: %.2f degrees",
	            openblack::script_camera::Get().fov.value / openblack::script_camera::k_DegreesToRadians);
	ImGui::TreePop();
}

void LandIsland::DrawDesireFlags() noexcept
{
	if (!ImGui::TreeNode("Town desire flags"))
	{
		return;
	}
	if (!Locator::entitiesRegistry::has_value())
	{
		ImGui::TextUnformatted("No entities");
		ImGui::TreePop();
		return;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	// the flags the object draw list called the Draw of on its last pass: only those step
	const auto drawn = Locator::objectDrawListSystem::has_value() ? Locator::objectDrawListSystem::value().DrawnThisFrame()
	                                                              : std::span<const entt::entity> {};
	if (ImGui::BeginTable("DesireFlags", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
	                      ImVec2(0.0f, 200.0f)))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Flag");
		ImGui::TableSetupColumn("Town");
		ImGui::TableSetupColumn("Desire");
		ImGui::TableSetupColumn("Store");
		ImGui::TableSetupColumn("Flies for");
		ImGui::TableSetupColumn("Up");
		ImGui::TableSetupColumn("Shows");
		ImGui::TableSetupColumn("Drawn");
		ImGui::TableHeadersRow();
		registry.Each<const ecs::components::TownDesireFlag>([&registry, drawn](entt::entity entity,
		                                                                        const ecs::components::TownDesireFlag& flag) {
			const auto* town = registry.Valid(flag.town) ? registry.TryGet<const ecs::components::Town>(flag.town) : nullptr;
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%u", static_cast<unsigned>(entt::to_integral(entity)));
			ImGui::TableNextColumn();
			if (town != nullptr)
			{
				ImGui::Text("%u", static_cast<unsigned>(town->id));
			}
			else
			{
				ImGui::TextUnformatted("(gone)");
			}
			ImGui::TableNextColumn();
			ImGui::Text("%d", static_cast<int>(flag.desire));
			ImGui::TableNextColumn();
			if (flag.store != entt::null)
			{
				ImGui::Text("%u", static_cast<unsigned>(entt::to_integral(flag.store)));
			}
			else
			{
				ImGui::TextUnformatted("(none yet)");
			}
			ImGui::TableNextColumn();
			ImGui::Text("%.3f", static_cast<double>(flag.storedDesire));
			ImGui::TableNextColumn();
			ImGui::Text("%.3f", static_cast<double>(flag.height.value));
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(flag.shown ? "yes" : "no");
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(std::ranges::find(drawn, entity) != drawn.end() ? "yes" : "no");
		});
		ImGui::EndTable();
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Flies for: the desire its town's last turn gave it, held to 1 while it shows. Drawn: the "
		                  "object draw list's last pass ran its Draw, the only frames it moves on");
	}
	ImGui::TreePop();
}

void LandIsland::Update() noexcept {}

void LandIsland::ProcessEventOpen([[maybe_unused]] const SDL_Event& event) noexcept {}

void LandIsland::ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept {}
