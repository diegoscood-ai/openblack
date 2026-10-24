/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Weather/WeatherState.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// Holds the weather's state, as the game starts it, and answers through the weather's functions
class WeatherSystem final: public WeatherSystemInterface
{
public:
	[[nodiscard]] weather::State& GetState() noexcept override { return _state; }

	void Reset() override;

	void CreateClimate(int32_t index, uint32_t info, glm::vec2 position, float radius1, float radius2) override;
	void SetClimateRain(int32_t index, float desire, int32_t dryDays, int32_t rainingDays, int32_t raining) override;
	void SetClimateTemperature(int32_t index, float temperature, float targetTemperature) override;
	void SetClimateWind(int32_t index, float windX, float windZ, float angle) override;

	void SetClimateSystemEnabled(bool enabled) override;
	void SetStormCreationEnabled(bool enabled) override;
	[[nodiscard]] bool IsClimateSystemEnabled() const override;
	[[nodiscard]] bool IsStormCreationEnabled() const override;

	void KillStormsInArea(glm::vec3 position, float radius) override;

	[[nodiscard]] components::WeatherInfo GetWeather(const glm::vec3& position) override;
	[[nodiscard]] components::WeatherInfo GetWeatherSmooth(const glm::vec3& position) override;
	[[nodiscard]] float GetOvercast(const glm::vec3& position) override;
	[[nodiscard]] uint8_t GetLightningFlash(const glm::vec3& camera) const override;

	[[nodiscard]] double GetDaysFromStart(uint32_t turn) const override;
	[[nodiscard]] uint32_t GetSeason(uint32_t turn) const override;

private:
	weather::State _state;
};
} // namespace openblack::ecs::systems
