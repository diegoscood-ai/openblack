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

#include <glm/vec3.hpp>

#include "Camera/CameraZones.h"

namespace openblack::particles
{
class LightSheet;
} // namespace openblack::particles

namespace openblack::ecs::systems
{

/// The camera zones the land's scripts last set: the fence the world camera is kept inside, its height limits and the
/// places it is kept out of. The map's clear takes the fence and the height limits away with the land.
class CameraZoneSystemInterface
{
public:
	virtual ~CameraZoneSystemInterface() = default;

	/// A script sets the zones from one of the files under Data/Zones: those it set before go, and the file's are kept
	/// to, its fence and exclusions switched on. Whether the file could be read; when it can't, there are none.
	virtual bool SetZones(std::string_view fileName) = 0;
	[[nodiscard]] virtual const camera_zones::Zones& GetZones() const = 0;
	/// The map is cleared with the land: the fence is off and gone, the height limits are the game's own again, and the
	/// places marked to go with the map are no longer kept out of. The other places stay
	virtual void ClearMap() = 0;
	/// The scripts restart: the fence, the height limits and every place the camera is kept out of go
	virtual void ClearForScriptsRestart() = 0;
	/// The file the zones came from, empty for none
	[[nodiscard]] virtual const std::string& GetFileName() const = 0;

	/// The force field, a sheet of light along the fence, lit where the camera is put back inside it: a pulse of light
	/// within LightSheet::k_ForceFieldPulseRadius of where the camera was put
	virtual void PulseForceField(const glm::vec3& /*at*/) {}
	/// Once a drawn frame, while the fence is on: the force field stands on the fence again and its light fades on by
	/// the frame's game time
	virtual void UpdateForceField(float /*seconds*/) {}
	/// The force field while the fence is on, as it stands; none without a fence
	[[nodiscard]] virtual particles::LightSheet* GetForceField() { return nullptr; }
};

} // namespace openblack::ecs::systems
