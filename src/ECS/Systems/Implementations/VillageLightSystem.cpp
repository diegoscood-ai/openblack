/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillageLightSystem.h"

#include <cstdint>

#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "3D/DayNightClock.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "3D/NightLights.h"
#include "Common/GameRandom.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;

VillageLightSystem::VillageLightSystem()
    : VillageLightSystem([](float a, float b) { return game_random::crt::Random(a, b); })
{
}

VillageLightSystem::VillageLightSystem(night_lights::Draw random)
    : _random(std::move(random))
{
}

void VillageLightSystem::AddLight(entt::entity lantern, const glm::vec3& position, uint8_t type)
{
	// at the head of the list (its end here): the flicker walks it newest first
	_state.lights.push_back(night_lights::MakeLight(lantern, position, type, _state.flameStarts, _random));
}

void VillageLightSystem::Update(std::chrono::duration<float, std::milli> gameTime)
{
	if (!Locator::terrainSystem::has_value() || !Locator::dayNightClock::has_value())
	{
		return;
	}
	// The lights go into this frame's land cells after the stamps: they read each cell's luminosity and write it back
	// directly, with no minimum against the loaded one. The land light of the frame is the table the renderer built
	// last (land_light::CurrentTable)
	const auto& table = land_light::CurrentTable();
	auto luminosity = land_light::Luminosity();
	night_lights::LightCells cells {
	    .firstCell = land_light::GetCells().firstCell,
	    .size = glm::ivec2(Locator::terrainSystem::value().GetCellMap().GetResolution()),
	    .cap = &luminosity,
	    .fullLightGreen = static_cast<uint8_t>((land_light::FullLight(table) >> 8) & 0xFFu),
	};
	_dark = night_lights::Update(gameTime.count(), Locator::dayNightClock::value().Clock().GetScriptTime(),
	                             LandLightTable::ToColour(table.GetLandColour()), cells, _random);
	land_light::SetLuminosity(luminosity);
}
