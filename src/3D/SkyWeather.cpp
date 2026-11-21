/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SkyWeather.h"

#include "Camera/Camera.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "Locator.h"

namespace openblack::sky_weather
{

uint8_t LightningFlash()
{
	// the storm flash at the drawn camera's position (the falling spell's path camera while it is drawn)
	if (!Locator::camera::has_value())
	{
		return 0;
	}
	return Locator::weatherSystem::value().GetLightningFlash(Locator::camera::value().GetDrawnOrigin());
}

} // namespace openblack::sky_weather
