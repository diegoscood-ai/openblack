/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Console.h"

#include <algorithm>
#include <iterator>
#include <optional>

#include <SDL.h>
#include <glm/gtc/constants.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLine.h"
#include "Camera/Camera.h"
#include "ConsoleLogSink.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "LHScriptX/FeatureScriptCommands.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::debug::gui;

namespace
{
/// The index, in the island's list, of the land block whose square holds a point
std::optional<int> BlockIndexAt(const LandIslandInterface& island, glm::vec3 point)
{
	constexpr float k_BlockSize = static_cast<float>(LandBlock::k_Resolution.x) * land_line::k_CellSize;
	const auto& blocks = island.GetBlocks();
	const auto found = std::ranges::find_if(blocks, [point](const LandBlock& block) {
		const auto corner = block.GetMapPosition();
		return point.x >= corner.x && point.x < corner.x + k_BlockSize && point.z >= corner.y &&
		       point.z < corner.y + k_BlockSize;
	});
	if (found == blocks.end())
	{
		return std::nullopt;
	}
	return static_cast<int>(std::distance(blocks.begin(), found));
}

/// The most lines the Console keeps, its own and the log's; the oldest go first
constexpr size_t k_KeptItems = 2000;
} // namespace

Console::Console() noexcept
    : Window("Console", ImVec2(520, 600))
    // , _items {"Welcome message goes here"}
    , _commands {
          {"help", "    - Display list of possible commands."},
          {"history", " - Display previously executed commands."},
          {"clear", "   - Clear the output log."},
      }
    , _logSink(std::make_shared<ConsoleLogSink>())
{
	// every logger the game made also writes to the Console
	spdlog::apply_all([this](const std::shared_ptr<spdlog::logger>& logger) { logger->sinks().push_back(_logSink); });
	for (const auto& signature : lhscriptx::FeatureScriptCommands::k_Signatures)
	{
		std::string details;
		for (bool skip = true; const auto param : signature.parameters)
		{
			if (!skip)
			{
				details += ", ";
				skip = false;
			}
			switch (param)
			{
			case lhscriptx::ParameterType::None:
				details += "none";
				break;
			case lhscriptx::ParameterType::String:
				details += "string";
				break;
			case lhscriptx::ParameterType::Float:
				details += "float";
				break;
			case lhscriptx::ParameterType::Number:
				details += "number";
				break;
			case lhscriptx::ParameterType::Vector:
				details += "vector";
				break;
			}
		}
		details += ")";
		_commands.emplace_back(std::string(signature.name.data(), signature.name.size()) + "(", details);
	}
}

Console::~Console() noexcept
{
	spdlog::apply_all([this](const std::shared_ptr<spdlog::logger>& logger) {
		auto& sinks = logger->sinks();
		sinks.erase(std::remove(sinks.begin(), sinks.end(), _logSink), sinks.end());
	});
}

void Console::Open() noexcept
{
	Window::Open();
	_reclaimFocus = true;
}

int Console::InputTextCallback(ImGuiInputTextCallbackData* data) noexcept
{
	switch (data->EventFlag)
	{
	default:
		break;
	case ImGuiInputTextFlags_CallbackCompletion:
	{
		// Example of TEXT COMPLETION

		// Locate beginning of current word
		const char* wordEnd = data->Buf + data->CursorPos;
		const char* wordStart = wordEnd;
		while (wordStart > data->Buf)
		{
			const char c = wordStart[-1];
			if (c == ' ' || c == '\t' || c == ',' || c == ';')
			{
				break;
			}
			wordStart--;
		}

		// Build a list of candidates
		std::vector<std::string> candidates;
		for (auto& command : _commands)
		{
			if (wordEnd == wordStart || strncmp(command.first.c_str(), wordStart, static_cast<int>(wordEnd - wordStart)) == 0)
			{
				candidates.push_back(command.first);
			}
		}

		if (candidates.empty())
		{
			// No match
			AddLog("No match for \"%.*s\"!\n", static_cast<int>(wordEnd - wordStart), wordStart);
		}
		else if (candidates.size() == 1)
		{
			// Single match. Delete the beginning of the word and replace it entirely so we've got nice casing
			data->DeleteChars(static_cast<int>(wordStart - data->Buf), static_cast<int>(wordEnd - wordStart));
			data->InsertChars(data->CursorPos, candidates[0].c_str());
		}
		else
		{
			// Multiple matches. Complete as much as we can, so inputing "C" will complete to "CL" and display
			// "CLEAR" and "CLASSIFY"
			int matchLen = static_cast<int>(wordEnd - wordStart);
			for (;;)
			{
				char c = 0;
				bool allCandidatesMatches = true;
				for (size_t i = 0; i < candidates.size() && allCandidatesMatches; i++)
				{
					if (i == 0)
					{
						c = candidates[i][matchLen];
					}
					else if (c == 0 || c != candidates[i][matchLen])
					{
						allCandidatesMatches = false;
					}
				}
				if (!allCandidatesMatches)
				{
					break;
				}
				matchLen++;
			}

			if (matchLen > 0)
			{
				data->DeleteChars(static_cast<int>(wordStart - data->Buf), static_cast<int>(wordEnd - wordStart));
				data->InsertChars(data->CursorPos, candidates[0].c_str(), candidates[0].c_str() + matchLen);
			}

			// List matches
			AddLog("Possible matches:\n");
			for (auto& candidate : candidates)
			{
				AddLog("- %s\n", candidate.c_str());
			}
		}

		break;
	}
	case ImGuiInputTextFlags_CallbackHistory:
	{
		// Example of HISTORY
		const auto prevHistoryPos = _historyPos;
		if (data->EventKey == ImGuiKey_UpArrow)
		{
			if (!_historyPos.has_value())
			{
				if (!_history.empty())
				{
					_historyPos = _history.size() - 1;
				}
				_partial = _inputBuffer.data();
			}
			else if (*_historyPos > 0)
			{
				(*_historyPos)--;
			}
		}
		else if (data->EventKey == ImGuiKey_DownArrow)
		{
			if (_historyPos.has_value())
			{
				if (++*_historyPos >= _history.size())
				{
					_historyPos.reset();
				}
			}
			else
			{
				_partial = _inputBuffer.data();
			}
		}

		// A better implementation would preserve the data on the current input line along with cursor position.
		if (prevHistoryPos != _historyPos)
		{
			auto historyStr = _historyPos.has_value() ? _history[*_historyPos] : _partial;
			data->DeleteChars(0, data->BufTextLen);
			data->InsertChars(0, historyStr.c_str());
		}
		break;
	}
	case ImGuiInputTextFlags_CallbackAlways:
	{
		_inputCursorPosition = data->CursorPos;
		break;
	}
	}
	return 0;
}

void Console::Draw() noexcept
{
	const auto& io = ImGui::GetIO();

	const auto screenSize = Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::ivec2 {};
	glm::ivec2 mousePosition {};
	SDL_GetMouseState(&mousePosition.x, &mousePosition.y);
	if (!io.WantCaptureMouse && screenSize.x > 0 && screenSize.y > 0)
	{
		// The object the interface picked under the cursor, else the land block under it
		const auto& pickingSystem = Locator::pickingSystem::value();
		if (const auto& object = pickingSystem.GetPick().object; object.has_value())
		{
			ImGui::SetTooltip("Entity %d", static_cast<int>(entt::to_integral(*object)));
		}
		else if (Locator::terrainSystem::has_value())
		{
			const auto& camera = Locator::camera::value();
			const auto eye = camera.GetEye();
			const auto nearPoint = camera.PixelNearPoint(eye, screenSize, mousePosition, camera.GetPixelNearClip());
			if (const auto land = pickingSystem.LandUnderPixel(eye, nearPoint, false); land.has_value())
			{
				if (const auto blockIndex = BlockIndexAt(Locator::terrainSystem::value(), *land); blockIndex.has_value())
				{
					ImGui::SetTooltip("Block Index: %d", *blockIndex);
				}
			}
		}
	}

	// As a specific feature guaranteed by the library, after calling Begin() the last Item represent the title bar. So e.g.
	// IsItemHovered() will return true when hovering the title bar. Here we create a context menu only available from the
	// title bar.
	if (ImGui::BeginPopupContextItem())
	{
		if (ImGui::MenuItem("Close Console"))
		{
			Close();
		}
		ImGui::EndPopup();
	}

	ImGui::TextWrapped("Enter 'help' for help, press TAB to use text completion.");

	// the log's new lines after what is there, the newest last
	auto logged = _logSink->Take();
	_items.insert(_items.end(), std::make_move_iterator(logged.begin()), std::make_move_iterator(logged.end()));
	if (_items.size() > k_KeptItems)
	{
		_items.erase(_items.begin(), _items.begin() + static_cast<std::ptrdiff_t>(_items.size() - k_KeptItems));
	}

	if (ImGui::SmallButton("Clear"))
	{
		_items.clear();
	}
	ImGui::SameLine();
	const auto copyToClipboard = ImGui::SmallButton("Copy");

	ImGui::Separator();

	// 1 separator, 1 input text
	const auto footerHeightToReserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
	// Leave room for 1 separator + 1 InputText
	ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footerHeightToReserve), 0, ImGuiWindowFlags_HorizontalScrollbar);
	if (ImGui::BeginPopupContextWindow())
	{
		if (ImGui::Selectable("Clear"))
		{
			_items.clear();
		}
		ImGui::EndPopup();
	}

	// Display every line as a separate entry so we can change their color or add custom widgets. If you only want raw text
	// you can use ImGui::TextUnformatted(log.begin(), log.end()); NB- if you have thousands of entries this approach may be
	// too inefficient and may require user-side clipping to only process visible items. You can seek and display only the
	// lines that are visible using the ImGuiListClipper helper, if your elements are evenly spaced and you have cheap
	// random access to the elements. To use the clipper we could replace the 'for (int i = 0; i < Items.Size; i++)' loop
	// with:
	//     ImGuiListClipper clipper(Items.Size);
	//     while (clipper.Step())
	//         for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
	// However, note that you can not use this code as is if a filter is active because it breaks the 'cheap random-access'
	// property. We would need random-access on the post-filtered list. A typical application wanting coarse clipping and
	// filtering may want to pre-compute an array of indices that passed the filtering test, recomputing this array when
	// user changes the filter, and appending newly elements as they are inserted. This is left as a task to the user until
	// we can manage to improve this example code! If your items are of variable size you may want to implement code similar
	// to what ImGuiListClipper does. Or split your data into fixed height items to allow random-seeking into your list.
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
	if (copyToClipboard)
	{
		ImGui::LogToClipboard();
	}
	for (const auto& item : _items)
	{
		// Normally you would store more information in your item (e.g. make Items[] an array of structure, store color/type
		// etc.)
		bool popColor = false;
		if (strstr(item.c_str(), "[error]") != nullptr)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
			popColor = true;
		}
		else if (strncmp(item.c_str(), "# ", 2) == 0)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.6f, 1.0f));
			popColor = true;
		}
		ImGui::TextUnformatted(item.c_str());
		if (popColor)
		{
			ImGui::PopStyleColor();
		}
	}
	if (copyToClipboard)
	{
		ImGui::LogFinish();
	}

	if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
	{
		ImGui::SetScrollHereY(1.0f);
	}

	ImGui::PopStyleVar();
	ImGui::EndChild();
	ImGui::Separator();

	// Command-line
	ImGui::PushItemWidth(-1);
	if (ImGui::InputText(
	        "##Input", _inputBuffer.data(), _inputBuffer.size(),
	        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion |
	            ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackAlways,
	        [](ImGuiInputTextCallbackData* data) -> int {
		        return reinterpret_cast<Console*>(data->UserData)->InputTextCallback(data);
	        },
	        this))
	{
		std::string s = _inputBuffer.data();

		// trim string
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return std::isspace(ch) == 0; }));
		s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) { return std::isspace(ch) == 0; }).base(), s.end());

		if (s[0] != 0)
		{
			ExecCommand(s);
		}

		_inputBuffer[0] = '\0';

		_reclaimFocus = true;
	}
	ImGui::PopItemWidth();

	if (_insertHandPosition)
	{
		if (_inputCursorPosition >= 0)
		{
			const auto pre = std::string(_inputBuffer.data(), _inputBuffer.data() + _inputCursorPosition);
			const auto post = std::string(_inputBuffer.data() + _inputCursorPosition);

			const auto positions = Locator::handSystem::value().GetPlayerHandPositions();
			if (positions[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)] ||
			    positions[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Right)])
			{
				const auto position = positions[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)].value_or(
				    positions[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Right)].value_or(
				        glm::zero<glm::vec3>()));
				snprintf(_inputBuffer.data(), _inputBuffer.size(), "%s%.2f,%.2f%s", pre.c_str(), position.x, position.z,
				         post.c_str());
			}
		}
	}
	_insertHandPosition = false;

	// Autofocus on window apparition
	ImGui::SetItemDefaultFocus();
	if (_reclaimFocus)
	{
		ImGui::SetKeyboardFocusHere(-1); // Autofocus previous widget
		_reclaimFocus = false;
	}
}

void Console::Update() noexcept {}

void Console::AddLog(const char* fmt, ...) noexcept
{
	// FIXME-OPT
	std::array<char, 1024> buf;
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf.data(), buf.size(), fmt, args);
	buf[buf.size() - 1] = 0;
	va_end(args);
	_items.emplace_back(buf.data());
}

void Console::ExecCommand(const std::string& commandLine) noexcept
{
	AddLog("# %s\n", commandLine.c_str());

	// Insert into history. First find match and delete it so it can be pushed to the back. This isn't trying to be smart or
	// optimal.
	_historyPos.reset();
	for (int i = static_cast<int>(_history.size()) - 1; i >= 0; i--)
	{
		if (_history[i] == commandLine)
		{
			_history.erase(_history.begin() + i);
			break;
		}
	}
	_history.emplace_back(commandLine);

	// Process command
	if (commandLine == "clear")
	{
		_items.clear();
	}
	else if (commandLine == "help")
	{
		AddLog("Commands:");
		for (auto& command : _commands)
		{
			AddLog("- %s%s", command.first.c_str(), command.second.c_str());
		}
	}
	else if (commandLine == "history")
	{
		const size_t first = _history.size() - 10;
		for (size_t i = first > 0 ? first : 0; i < _history.size(); i++)
		{
			AddLog("%3zu: %s\n", i, _history[i].c_str());
		}
	}
	else
	{
		try
		{
			lhscriptx::Script script;
			script.Load(commandLine);
		}
		catch (std::runtime_error& error)
		{
			AddLog("[error]: %s", error.what());
		}
	}
}

void Console::ProcessEventOpen(const SDL_Event& event) noexcept
{
	const ImGuiIO& io = ImGui::GetIO();
	switch (event.type)
	{
	default:
		break;
	case SDL_MOUSEBUTTONDOWN:
		if (event.button.clicks == 2 && !io.WantCaptureMouse)
		{
			_insertHandPosition = true;
		}
		break;
	}
}

void Console::ProcessEventAlways(const SDL_Event& event) noexcept
{
	switch (event.type)
	{
	default:
		break;
	case SDL_KEYDOWN:
		if (event.key.keysym.sym == SDLK_BACKQUOTE)
		{
			Toggle();
		}
		break;
	}
}
