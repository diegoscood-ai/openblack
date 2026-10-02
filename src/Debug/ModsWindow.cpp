/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ModsWindow.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>

#include <fmt/format.h>
#include <stb_image.h>

#include "ImGuiUtils.h"
#include "Locator.h"
#include "Mods/Manifest.h"
#include "Mods/ModLog.h"
#include "Mods/ModRegistry.h"
#include "Mods/Restart.h"

using namespace openblack;
using namespace openblack::debug::gui;
using openblack::mods::Mod;
using openblack::mods::ModRegistry;

namespace
{
constexpr float k_ListWidth = 300.0f;
constexpr float k_SmallIcon = 32.0f;
constexpr float k_BigIcon = 128.0f;
const ImVec4 k_Red {1.0f, 0.45f, 0.4f, 1.0f};
const ImVec4 k_Yellow {1.0f, 0.85f, 0.4f, 1.0f};
const ImVec4 k_Green {0.5f, 0.9f, 0.5f, 1.0f};
const ImVec4 k_Grey {0.6f, 0.6f, 0.6f, 1.0f};

std::string Lower(std::string_view text)
{
	std::string lower(text);
	std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return lower;
}

bool Matches(const Mod& mod, const std::string& search)
{
	if (search.empty())
	{
		return true;
	}
	const auto needle = Lower(search);
	const auto& info = mod.GetInfo();
	return Lower(info.name).find(needle) != std::string::npos || Lower(info.id).find(needle) != std::string::npos ||
	       Lower(info.category).find(needle) != std::string::npos;
}

/// The state line of a mod: active, off, blocked (why) or waiting for its parent
void DrawState(const ModRegistry& registry, const Mod& mod)
{
	if (!mod.IsEnabled())
	{
		ImGui::TextColored(k_Grey, "Off");
	}
	else if (!mod.GetBlockedReason().empty())
	{
		ImGui::TextColored(k_Red, "Blocked: %s", mod.GetBlockedReason().c_str());
	}
	else if (!registry.IsActive(mod))
	{
		ImGui::TextColored(k_Yellow, "Waiting for %s to be on", mod.GetInfo().parent.c_str());
	}
	else
	{
		ImGui::TextColored(k_Green, "Active");
	}
}

std::string_view KindName(Mod::Kind kind)
{
	switch (kind)
	{
	case Mod::Kind::Package:
		return "mod.json";
	case Mod::Kind::Data:
		return "replacement files (old mod.cfg)";
	case Mod::Kind::Module:
		return "module (old mod.cfg)";
	}
	return "?";
}
} // namespace

ModsWindow::ModsWindow() noexcept
    : Window(k_Name, ImVec2(900.0f, 560.0f))
{
	// Test hook: OPENBLACK_TEST_MODS_WINDOW = "modpacks" | "mods" | "mods:<mod id>" | "pack:<pack id>" | "order" | "log"
	// opens the window on that tab (screenshots without the mouse)
	if (const char* test = std::getenv("OPENBLACK_TEST_MODS_WINDOW"); test != nullptr)
	{
		const std::string_view value = test;
		Open();
		_switchTab = true;
		if (value == "modpacks")
		{
			_switchTo = Tab::Modpacks;
		}
		else if (value == "order")
		{
			_switchTo = Tab::LoadOrder;
		}
		else if (value == "log")
		{
			_switchTo = Tab::Log;
		}
		else if (value.starts_with("pack:"))
		{
			_switchTo = Tab::Mods;
			_packFilter = std::string(value.substr(5));
		}
		else
		{
			_switchTo = Tab::Mods;
			if (value.starts_with("mods:"))
			{
				_selectedMod = std::string(value.substr(5));
			}
		}
	}
}

ModsWindow::~ModsWindow() noexcept
{
	for (const auto& [path, handle] : _icons)
	{
		if (bgfx::isValid(handle))
		{
			bgfx::destroy(handle);
		}
	}
}

bgfx::TextureHandle ModsWindow::Icon(const std::filesystem::path& path) noexcept
{
	if (path.empty())
	{
		return BGFX_INVALID_HANDLE;
	}
	if (const auto it = _icons.find(path); it != _icons.end())
	{
		return it->second;
	}
	bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
	int width = 0;
	int height = 0;
	int channels = 0;
	// read with the wide path (any folder name works), decoded from memory
	std::ifstream file(path, std::ios::binary);
	const std::vector<char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	if (auto* pixels = bytes.empty() ? nullptr
	                                 : stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(bytes.data()),
	                                                         static_cast<int>(bytes.size()), &width, &height, &channels, 4);
	    pixels != nullptr)
	{
		const auto size = static_cast<uint32_t>(width * height * 4);
		handle = bgfx::createTexture2D(static_cast<uint16_t>(width), static_cast<uint16_t>(height), false, 1,
		                               bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_NONE, bgfx::copy(pixels, size));
		stbi_image_free(pixels);
	}
	else
	{
		mods::log::Warning("", fmt::format("could not read the image {}", mods::log::Utf8(path)));
	}
	_icons[path] = handle;
	return handle;
}

void ModsWindow::IconOrBlank(const std::filesystem::path& path, float size) noexcept
{
	const auto handle = Icon(path);
	if (bgfx::isValid(handle))
	{
		ImGui::Image(handle, ImVec2(size, size));
	}
	else
	{
		ImGui::Dummy(ImVec2(size, size));
	}
}

void ModsWindow::Draw() noexcept
{
	if (!Locator::mods::has_value())
	{
		ImGui::TextUnformatted("The mod library is not running");
		return;
	}
	DrawRestartNotice();
	if (ImGui::BeginTabBar("ModsTabs"))
	{
		const auto tab = [this](const char* label, Tab which) {
			const auto flags = _switchTab && _switchTo == which ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
			return ImGui::BeginTabItem(label, nullptr, flags);
		};
		if (tab("Modpacks", Tab::Modpacks))
		{
			DrawModpacks();
			ImGui::EndTabItem();
		}
		const auto modsLabel =
		    _packFilter.empty() ? std::string("Mods###ModsTab")
		                        : fmt::format("Mods ({})###ModsTab", Locator::mods::value().FindModpack(_packFilter) != nullptr
		                                                                ? Locator::mods::value().FindModpack(_packFilter)->name
		                                                                : _packFilter);
		if (tab(modsLabel.c_str(), Tab::Mods))
		{
			DrawMods();
			ImGui::EndTabItem();
		}
		if (tab("Load order", Tab::LoadOrder))
		{
			DrawLoadOrder();
			ImGui::EndTabItem();
		}
		if (tab("Log", Tab::Log))
		{
			DrawLog();
			ImGui::EndTabItem();
		}
		_switchTab = false;
		ImGui::EndTabBar();
	}
}

void ModsWindow::DrawRestartNotice() noexcept
{
	const auto pending = Locator::mods::value().PendingRestart();
	if (pending.empty())
	{
		_pendingRestart = 0;
		return;
	}
	std::string names;
	for (const auto* mod : pending)
	{
		names += (names.empty() ? "" : ", ") + mod->GetInfo().name;
	}
	// a mod that needs a restart has just been switched or changed: ask
	if (pending.size() > _pendingRestart)
	{
		ImGui::OpenPopup("Restart needed");
	}
	_pendingRestart = pending.size();

	ImGui::TextColored(k_Yellow, "Takes effect after a restart: %s", names.c_str());
	ImGui::SameLine();
	if (ImGui::SmallButton("Restart openblack now"))
	{
		mods::restart::Request();
	}
	ImGui::Separator();

	if (ImGui::BeginPopupModal("Restart needed", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("These changes take effect when openblack starts again:");
		for (const auto* mod : pending)
		{
			ImGui::BulletText("%s", mod->GetInfo().name.c_str());
		}
		ImGui::Spacing();
		if (ImGui::Button("Restart openblack now"))
		{
			mods::restart::Request();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Later"))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void ModsWindow::DrawModpacks() noexcept
{
	auto& registry = Locator::mods::value();
	const auto& packs = registry.GetModpacks();
	if (packs.empty())
	{
		ImGui::TextWrapped("No modpacks. A modpack is a folder of %s with a modpack.json and its mods inside, each in its "
		                   "own folder with its mod.json.",
		                   mods::log::Utf8(registry.GetModsDirectory()).c_str());
		return;
	}
	for (const auto& pack : packs)
	{
		ImGui::PushID(pack.id.c_str());
		IconOrBlank(pack.icon, 48.0f);
		ImGui::SameLine();
		ImGui::BeginGroup();
		bool enabled = registry.IsModpackEnabled(pack);
		if (ImGui::Checkbox("##on", &enabled))
		{
			registry.SetModpackEnabled(pack, enabled);
		}
		ImGui::SameLine();
		if (ImGui::Selectable(fmt::format("{}  {}", pack.name, pack.version.ToString()).c_str(), false))
		{
			_packFilter = pack.id;
			_switchTo = Tab::Mods;
			_switchTab = true;
			_selectedMod.clear();
		}
		ImGui::TextColored(k_Grey, "%zu mods%s%s", pack.mods.size(), pack.authors.empty() ? "" : "  -  ",
		                   pack.authors.empty() ? "" : pack.authors.front().c_str());
		if (!pack.description.empty())
		{
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextUnformatted(pack.description.c_str());
			ImGui::PopTextWrapPos();
		}
		ImGui::EndGroup();
		ImGui::Separator();
		ImGui::PopID();
	}
}

void ModsWindow::DrawModEntry(Mod& mod, int indent) noexcept
{
	auto& registry = Locator::mods::value();
	const auto& info = mod.GetInfo();
	ImGui::PushID(info.id.c_str());
	if (indent > 0)
	{
		ImGui::Indent(16.0f * static_cast<float>(indent));
	}
	IconOrBlank(info.icon, k_SmallIcon);
	ImGui::SameLine();
	bool enabled = mod.IsEnabled();
	ImGui::BeginDisabled(!info.parent.empty() && registry.Find(info.parent) != nullptr &&
	                     !registry.IsActive(*registry.Find(info.parent)));
	if (ImGui::Checkbox("##on", &enabled))
	{
		registry.SetEnabled(mod, enabled);
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	const bool blocked = mod.IsEnabled() && !mod.GetBlockedReason().empty();
	if (blocked)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, k_Red);
	}
	const auto label = info.restartRequired ? info.name + " *" : info.name;
	if (ImGui::Selectable(label.c_str(), _selectedMod == info.id))
	{
		_selectedMod = info.id;
	}
	if (blocked)
	{
		ImGui::PopStyleColor();
	}
	if (indent > 0)
	{
		ImGui::Unindent(16.0f * static_cast<float>(indent));
	}
	ImGui::PopID();
}

void ModsWindow::DrawMods() noexcept
{
	auto& registry = Locator::mods::value();

	ImGui::BeginChild("ModList", ImVec2(k_ListWidth, 0.0f), true);
	if (!_packFilter.empty())
	{
		if (ImGui::SmallButton("< All loose mods"))
		{
			_packFilter.clear();
			_selectedMod.clear();
		}
		if (const auto* pack = registry.FindModpack(_packFilter); pack != nullptr)
		{
			ImGui::SameLine();
			bool enabled = registry.IsModpackEnabled(*pack);
			if (ImGui::Checkbox("Whole pack", &enabled))
			{
				registry.SetModpackEnabled(*pack, enabled);
			}
		}
	}
	ImGui::SetNextItemWidth(-1.0f);
	char buffer[128] = {};
	std::snprintf(buffer, sizeof(buffer), "%s", _search.c_str());
	if (ImGui::InputTextWithHint("##search", "Search", buffer, sizeof(buffer)))
	{
		_search = buffer;
	}

	// by category, modules under their parent
	std::map<std::string, std::vector<Mod*>> byCategory;
	for (const auto& mod : registry.GetMods())
	{
		const auto& info = mod->GetInfo();
		if (info.pack != _packFilter || !info.parent.empty() || !Matches(*mod, _search))
		{
			continue;
		}
		byCategory[info.category].push_back(mod.get());
	}
	for (auto& [category, list] : byCategory)
	{
		std::ranges::sort(list, [](const Mod* a, const Mod* b) { return a->GetInfo().name < b->GetInfo().name; });
		ImGui::SeparatorText(category.c_str());
		for (auto* mod : list)
		{
			DrawModEntry(*mod, 0);
			for (const auto& module : registry.GetMods())
			{
				if (module->GetInfo().parent == mod->GetInfo().id)
				{
					DrawModEntry(*module, 1);
				}
			}
		}
	}
	if (_packFilter.empty() && !registry.GetBroken().empty())
	{
		ImGui::SeparatorText("Could not be read");
		for (const auto& broken : registry.GetBroken())
		{
			ImGui::TextColored(k_Red, "%s", mods::log::Utf8(broken.folder.filename()).c_str());
			if (ImGui::IsItemHovered() && !broken.errors.empty())
			{
				std::string text;
				for (const auto& error : broken.errors)
				{
					text += error + "\n";
				}
				ImGui::SetTooltip("%s", text.c_str());
			}
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();
	ImGui::BeginChild("ModDetails", ImVec2(0.0f, 0.0f), true);
	if (auto* mod = registry.Find(_selectedMod); mod != nullptr)
	{
		DrawModDetails(*mod);
	}
	else
	{
		ImGui::TextWrapped("Pick a mod on the left. Mods are folders of %s (each with a mod.json); see the wiki page "
		                   "mod-library.md to make one.",
		                   mods::log::Utf8(registry.GetModsDirectory()).c_str());
		ImGui::TextColored(k_Grey, "* takes effect after a restart");
	}
	ImGui::EndChild();
}

void ModsWindow::DrawModDetails(Mod& mod) noexcept
{
	auto& registry = Locator::mods::value();
	const auto& info = mod.GetInfo();

	IconOrBlank(info.icon, k_BigIcon);
	ImGui::SameLine();
	ImGui::BeginGroup();
	ImGui::TextUnformatted(info.name.c_str());
	ImGui::TextColored(k_Grey, "%s  -  version %s", info.id.c_str(), info.version.ToString().c_str());
	if (!info.authors.empty())
	{
		std::string authors;
		for (const auto& author : info.authors)
		{
			authors += (authors.empty() ? "" : ", ") + author;
		}
		ImGui::TextColored(k_Grey, "by %s", authors.c_str());
	}
	ImGui::TextColored(k_Grey, "%s%s%s", info.category.c_str(), info.pack.empty() ? "" : "  -  modpack ",
	                   info.pack.c_str());
	bool enabled = mod.IsEnabled();
	if (ImGui::Checkbox("Enabled", &enabled))
	{
		registry.SetEnabled(mod, enabled);
	}
	if (info.restartRequired)
	{
		ImGui::SameLine();
		ImGui::TextColored(k_Yellow, "(takes effect after a restart)");
	}
	DrawState(registry, mod);
	ImGui::EndGroup();

	if (!info.description.empty())
	{
		ImGui::Separator();
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextUnformatted(info.description.c_str());
		ImGui::PopTextWrapPos();
	}

	const auto& options = mod.GetOptions();
	if (!options.empty())
	{
		ImGui::SeparatorText("Settings");
		ImGui::BeginDisabled(!mod.IsEnabled());
		for (size_t i = 0; i < options.size(); ++i)
		{
			const auto& option = options[i];
			ImGui::PushID(static_cast<int>(i));
			ImGui::SetNextItemWidth(200.0f);
			if (option.slider)
			{
				auto choice = static_cast<int>(option.value);
				if (ImGui::SliderInt(option.label.c_str(), &choice, 0, static_cast<int>(option.choices.size()) - 1,
				                     option.choices.at(option.value).c_str(), ImGuiSliderFlags_NoInput))
				{
					registry.SetOption(mod, i, static_cast<size_t>(choice));
				}
			}
			else if (ImGui::BeginCombo(option.label.c_str(), option.choices.at(option.value).c_str()))
			{
				for (size_t choice = 0; choice < option.choices.size(); ++choice)
				{
					if (ImGui::Selectable(option.choices[choice].c_str(), choice == option.value))
					{
						registry.SetOption(mod, i, choice);
					}
				}
				ImGui::EndCombo();
			}
			if (!option.description.empty() && ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", option.description.c_str());
			}
			ImGui::PopID();
		}
		ImGui::EndDisabled();
	}

	if (!info.dependencies.empty() || !info.parent.empty())
	{
		ImGui::SeparatorText("Needs");
		if (!info.parent.empty())
		{
			const auto* parent = registry.Find(info.parent);
			ImGui::TextColored(parent != nullptr && registry.IsActive(*parent) ? k_Green : k_Yellow, "module of %s",
			                   parent != nullptr ? parent->GetInfo().name.c_str() : info.parent.c_str());
		}
		for (const auto& dependency : info.dependencies)
		{
			const auto* other = registry.Find(dependency.id);
			const char* kind = dependency.kind == mods::Dependency::Kind::Required   ? "needs"
			                   : dependency.kind == mods::Dependency::Kind::Optional ? "works with"
			                                                                          : "does not work with";
			ImVec4 colour = k_Grey;
			std::string state = "not installed";
			if (other != nullptr)
			{
				state = fmt::format("{} {}", other->GetInfo().version.ToString(), registry.IsActive(*other) ? "on" : "off");
				const bool fine = dependency.kind == mods::Dependency::Kind::Incompatible ? !registry.IsActive(*other)
				                                                                          : registry.IsActive(*other);
				colour = fine ? k_Green : k_Red;
			}
			else if (dependency.kind == mods::Dependency::Kind::Required)
			{
				colour = k_Red;
			}
			ImGui::TextColored(colour, "%s %s %s (%s)", kind, dependency.id.c_str(), dependency.range.GetText().c_str(),
			                   state.c_str());
		}
	}
	if (!info.provides.empty())
	{
		ImGui::SeparatorText("Offers to other mods");
		for (const auto& name : info.provides)
		{
			ImGui::BulletText("%s", name.c_str());
		}
	}

	ImGui::SeparatorText("Files");
	ImGui::TextColored(k_Grey, "%s", mods::log::Utf8(registry.GetModDirectory(mod)).c_str());
	ImGui::TextColored(k_Grey, "%s  -  --mod %s", KindName(info.kind).data(), info.id.c_str());
	if (!info.url.empty())
	{
		ImGui::TextColored(k_Grey, "%s", info.url.c_str());
	}
}

void ModsWindow::DrawLoadOrder() noexcept
{
	auto& registry = Locator::mods::value();
	ImGui::TextWrapped("Mods load from top to bottom; with two changing the same thing, the lower one wins. A mod's "
	                   "dependencies always load before it. Saved in Mods/load_order.cfg.");
	if (!ImGui::BeginTable("LoadOrder", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
	                                           ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit))
	{
		return;
	}
	ImGui::TableSetupColumn("#");
	ImGui::TableSetupColumn("Mod", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("State");
	ImGui::TableSetupColumn("Loads after", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("Move");
	ImGui::TableHeadersRow();
	const auto order = registry.GetLoadOrder(); // copied: moving re-sorts it
	for (size_t i = 0; i < order.size(); ++i)
	{
		const auto& mod = *order[i];
		const auto& info = mod.GetInfo();
		ImGui::PushID(info.id.c_str());
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("%zu", i + 1);
		ImGui::TableNextColumn();
		IconOrBlank(info.icon, 18.0f);
		ImGui::SameLine();
		ImGui::Text("%s (%s)", info.name.c_str(), info.id.c_str());
		ImGui::TableNextColumn();
		DrawState(registry, mod);
		ImGui::TableNextColumn();
		std::string after;
		for (const auto& dependency : info.dependencies)
		{
			if (dependency.kind != mods::Dependency::Kind::Incompatible)
			{
				after += (after.empty() ? "" : ", ") + dependency.id;
			}
		}
		for (const auto& id : info.loadAfter)
		{
			after += (after.empty() ? "" : ", ") + id;
		}
		if (!info.parent.empty() && after.find(info.parent) == std::string::npos)
		{
			after += (after.empty() ? "" : ", ") + info.parent;
		}
		ImGui::TextColored(k_Grey, "%s", after.c_str());
		ImGui::TableNextColumn();
		if (ImGui::ArrowButton("up", ImGuiDir_Up))
		{
			registry.MoveInLoadOrder(mod, -1);
		}
		ImGui::SameLine();
		if (ImGui::ArrowButton("down", ImGuiDir_Down))
		{
			registry.MoveInLoadOrder(mod, 1);
		}
		ImGui::PopID();
	}
	ImGui::EndTable();
}

void ModsWindow::DrawLog() noexcept
{
	ImGui::Checkbox("Info", &_logInfo);
	ImGui::SameLine();
	ImGui::Checkbox("Warnings", &_logWarnings);
	ImGui::SameLine();
	ImGui::Checkbox("Errors", &_logErrors);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(200.0f);
	if (ImGui::BeginCombo("Mod", _logMod.empty() ? "(all)" : _logMod.c_str()))
	{
		if (ImGui::Selectable("(all)", _logMod.empty()))
		{
			_logMod.clear();
		}
		for (const auto& mod : Locator::mods::value().GetMods())
		{
			if (ImGui::Selectable(mod->GetInfo().id.c_str(), _logMod == mod->GetInfo().id))
			{
				_logMod = mod->GetInfo().id;
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Clear"))
	{
		mods::log::Clear();
	}
	ImGui::BeginChild("LogLines", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
	for (const auto& entry : mods::log::Entries())
	{
		using mods::log::Level;
		if ((entry.level == Level::Info && !_logInfo) || (entry.level == Level::Warning && !_logWarnings) ||
		    (entry.level == Level::Error && !_logErrors) || (!_logMod.empty() && entry.mod != _logMod))
		{
			continue;
		}
		const auto colour = entry.level == Level::Error ? k_Red : entry.level == Level::Warning ? k_Yellow : k_Grey;
		if (entry.mod.empty())
		{
			ImGui::TextColored(colour, "%s", entry.text.c_str());
		}
		else
		{
			ImGui::TextColored(colour, "[%s] %s", entry.mod.c_str(), entry.text.c_str());
		}
	}
	if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
	{
		ImGui::SetScrollHereY(1.0f);
	}
	ImGui::EndChild();
}
