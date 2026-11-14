/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string>
#include <string_view>

#include "ECS/Systems/CameraZoneSystemInterface.h"
#include "Particles/LightSheet.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::player_camera
{
struct Zone;
} // namespace openblack::player_camera

namespace openblack::ecs::systems
{

class CameraZoneSystem final: public CameraZoneSystemInterface
{
public:
	CameraZoneSystem();

	bool SetZones(std::string_view fileName) override;
	[[nodiscard]] const camera_zones::Zones& GetZones() const override { return _zones; }
	void ClearMap() override;
	void ClearForScriptsRestart() override;
	[[nodiscard]] const std::string& GetFileName() const override { return _fileName; }
	void PulseForceField(const glm::vec3& at) override;
	void UpdateForceField(float seconds) override;
	[[nodiscard]] particles::LightSheet* GetForceField() override;

	/// The zones a file holds, switched on as a script's are
	[[nodiscard]] static camera_zones::Zones FromFile(const player_camera::Zone& file);

private:
	camera_zones::Zones _zones;
	std::string _fileName;
	particles::LightSheet _forceField;
};

} // namespace openblack::ecs::systems
