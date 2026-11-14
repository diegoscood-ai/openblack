/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleSaveGameControls.h"

#include <algorithm>
#include <utility>

using namespace openblack;
namespace room = openblack::temple::save_game_room;

void TempleSaveGameControls::Find(std::span<const std::string> subMeshNames)
{
	_controls.clear();
	_held.reset();
	for (uint32_t i = 0; i < subMeshNames.size(); ++i)
	{
		if (const auto control = room::ParseControl(subMeshNames[i]); control.has_value())
		{
			_controls.push_back({.subMesh = i, .control = *control});
		}
	}
}

bool TempleSaveGameControls::HasSave(uint32_t slot) const
{
	return room::HasSave(_saves, slot);
}

std::vector<TempleSaveGameControls::Picture> TempleSaveGameControls::GetPictures() const
{
	std::vector<Picture> pictures;
	for (const auto& found : _controls)
	{
		if (found.control.part == room::Part::Picture)
		{
			pictures.push_back({.subMesh = found.subMesh, .slot = found.control.slot});
		}
	}
	return pictures;
}

bool TempleSaveGameControls::Shown(const Found& found, const room::View& view) const
{
	return room::IsShown(found.control, view, HasSave(found.control.slot));
}

const TempleSaveGameControls::Found* TempleSaveGameControls::FindShown(uint32_t subMesh, const room::View& view) const
{
	const auto found = std::ranges::find(_controls, subMesh, &Found::subMesh);
	if (found == _controls.end() || !Shown(*found, view))
	{
		return nullptr;
	}
	return &*found;
}

bool TempleSaveGameControls::Hold(bool pressed, std::optional<uint32_t> hoveredSubMesh, const room::View& view,
                                  std::optional<Click>& click)
{
	const bool pressing = pressed && !_wasPressed;
	_wasPressed = pressed;
	if (pressing && hoveredSubMesh.has_value())
	{
		if (const auto* found = FindShown(*hoveredSubMesh, view); found != nullptr)
		{
			_held = *found;
		}
	}
	if (pressed || !_held.has_value())
	{
		return _held.has_value();
	}
	// let go: a click if over the same control, still shown
	const auto held = *std::exchange(_held, std::nullopt);
	if (hoveredSubMesh == held.subMesh && Shown(held, view))
	{
		click = Click {.control = held.control, .subMesh = held.subMesh};
	}
	return true;
}

std::vector<uint32_t> TempleSaveGameControls::GetHidden(const room::View& view) const
{
	std::vector<uint32_t> hidden;
	for (const auto& found : _controls)
	{
		if (!Shown(found, view))
		{
			hidden.push_back(found.subMesh);
		}
	}
	return hidden;
}

bool TempleSaveGameControls::IsControl(uint32_t subMesh, const room::View& view) const
{
	return FindShown(subMesh, view) != nullptr;
}

std::vector<room::Control> TempleSaveGameControls::GetShown(const room::View& view) const
{
	std::vector<room::Control> shown;
	for (const auto& found : _controls)
	{
		if (Shown(found, view))
		{
			shown.push_back(found.control);
		}
	}
	return shown;
}
