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
#include <utility>

#include <LNDFile.h>

#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "Debug/ImGuiUtils.h"
#include "ECS/DrawList/ObjectList.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DrawListSystemInterface.h"
#include "Editor/EditorEntities.h"
#include "Editor/EditorOutline.h"
#include "EngineConfig.h"
#include "Game.h"
#include "GameClock.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/ObjectListFrame.h"
#include "Locator.h"

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

	if (ImGui::TreeNodeEx("Footprints", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const auto& frameBuffer = landIsland.GetFootprintFramebuffer();
		uint16_t width;
		uint16_t height;
		frameBuffer.GetSize(width, height);
		ImGui::Text("Resolution: %ux%u", width, height);
		const float scaling = 512.0f / static_cast<float>(width);
		ImGui::Image(toBgfx(frameBuffer.GetColorAttachment().GetNativeHandle()), ImVec2(width * scaling, height * scaling));
		ImGui::TreePop();
	}

	DrawObjectList();

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
	if (!Locator::drawListSystem::has_value())
	{
		ImGui::TextUnformatted("No object draw list");
		ImGui::TreePop();
		return;
	}
	const auto& list = Locator::drawListSystem::value();
	ImGui::Text("Entries: %zu of %zu%s", list.Count(), ecs::draw_list::k_MaxEntries,
	            list.Count() >= ecs::draw_list::k_MaxEntries ? " (full)" : "");
	ImGui::Text("Last pass: %s", list.LastPassFull() ? "full" : "still");
	ImGui::Text("Last rebuild at turn %u, now turn %u; rebuilds asked for: %u", list.RebuildTurn(), game_clock::Turn(),
	            static_cast<unsigned>(list.RebuildCount()));
	ImGui::Text("Drawn by the last pass: %zu", list.DrawnThisFrame().size());

	const auto* game = Game::Instance();
	const auto* frame = game != nullptr ? game->GetObjectListFrame() : nullptr;
	if (frame == nullptr || !frame->ran)
	{
		ImGui::TextUnformatted("The list did not run on the last frame: nothing is left out");
		ImGui::TreePop();
		return;
	}
	const auto notListed = static_cast<size_t>(std::ranges::count(frame->reasons, graphics::ObjectListHidden::NotListed));
	ImGui::Text("Left out of the last frame: %zu (%zu not in the list, %zu listed and not drawn)", frame->hidden.size(),
	            notListed, frame->hidden.size() - notListed);
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
				const bool listed =
				    index < frame->reasons.size() && frame->reasons[index] == graphics::ObjectListHidden::NotDrawn;
				ImGui::TextUnformatted(listed ? "listed, not drawn" : "not in the list");
			}
		}
		ImGui::EndTable();
	}
	ImGui::TreePop();
}

void LandIsland::Update() noexcept {}

void LandIsland::ProcessEventOpen([[maybe_unused]] const SDL_Event& event) noexcept {}

void LandIsland::ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept {}
