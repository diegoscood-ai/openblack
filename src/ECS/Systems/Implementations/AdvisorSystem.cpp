/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "AdvisorSystem.h"

#include <cstdio>
#include <cstdlib>

#include "Help/SpiritsRuntime.h"

using namespace openblack::ecs::systems;

namespace
{
/// The advisors once loaded; stops with a message before Load, which every caller checks with IsLoaded first
openblack::help::spirits::Runtime& Advisors()
{
	auto* runtime = openblack::help::spirits::Get();
	if (runtime == nullptr)
	{
		std::fputs("advisors: not loaded (AdvisorSystem::Load)\n", stderr);
		std::abort();
	}
	return *runtime;
}
} // namespace

void AdvisorSystem::Load()
{
	openblack::help::spirits::Start();
}

bool AdvisorSystem::IsLoaded() const
{
	return openblack::help::spirits::Get() != nullptr;
}

void AdvisorSystem::Update([[maybe_unused]] const Frame& frame)
{
	// The frame is not read: our advisors take the clock, the cursor, the window, the camera and the help system's
	// wide screen themselves
	Advisors().Update();
}

void AdvisorSystem::ProcessTurn()
{
	Advisors().ProcessTurn();
}

openblack::help::spirits::AdvisorSpiritController& AdvisorSystem::GetController()
{
	return Advisors().Control();
}

const openblack::help::spirits::AdvisorSpiritController& AdvisorSystem::GetController() const
{
	return Advisors().Control();
}

void AdvisorSystem::FillOverlay(openblack::graphics::OverlayFrame& frame)
{
	Advisors().FillOverlay(frame);
}

const openblack::help::HelpDudeFile* AdvisorSystem::GetFile(int advisor) const
{
	const auto* runtime = openblack::help::spirits::Get();
	return runtime != nullptr ? runtime->File(advisor) : nullptr;
}
