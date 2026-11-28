/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CameraStack.h"

#include <algorithm>
#include <utility>

#include "Camera/CameraModel.h"

namespace openblack::ecs::systems
{

CameraStack::CameraStack() = default;

CameraStack::~CameraStack() = default;

void CameraStack::Keep(uint32_t id, std::unique_ptr<CameraModel> model)
{
	_kept.push_back({.id = id, .model = std::move(model)});
}

std::unique_ptr<CameraModel> CameraStack::TakeKept(uint32_t id)
{
	const auto kept = std::ranges::find_if(_kept, [id](const KeptModel& candidate) { return candidate.id == id; });
	if (kept == _kept.end())
	{
		return nullptr;
	}
	auto model = std::move(kept->model);
	_kept.erase(kept);
	return model;
}

void CameraStack::DropKept(uint32_t id)
{
	std::erase_if(_kept, [id](const KeptModel& kept) { return kept.id == id; });
}

void CameraStack::DropLost()
{
	std::erase_if(_kept, [this](const KeptModel& kept) { return !_modes.KeepsModel(kept.id); });
}

void CameraStack::Clear()
{
	_modes.Clear();
	_kept.clear();
}

} // namespace openblack::ecs::systems
