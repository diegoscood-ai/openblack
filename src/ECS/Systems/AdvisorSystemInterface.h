/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack
{
class Camera;
}

namespace openblack::graphics
{
struct OverlayFrame;
}

namespace openblack::help
{
struct HelpDudeFile;
}

namespace openblack::help::spirits
{
class AdvisorSpiritController;
} // namespace openblack::help::spirits

namespace openblack::ecs::systems
{

/// The player's two advisors, the good one and the evil one: where they are and what they do each frame, and what the
/// renderer draws of them
class AdvisorSystemInterface
{
public:
	/// What a frame gives the advisors
	struct Frame
	{
		const Camera* camera {nullptr};
		glm::u16vec2 screen {640, 480};
		glm::ivec2 mouse {0};
		/// The frame's time, at least 1 ms
		uint32_t frameMs {1};
		/// The frame's time as the fades and the puff take it: the frame's time inside the citadel, else the game's
		/// step, at most 500 ms
		int32_t stepMs {0};
		/// The time since the computer started, for the flicker
		uint32_t tickMs {0};
		/// A script holds the cinema bars
		bool wideScreen {false};
	};

	virtual ~AdvisorSystemInterface() = default;

	/// Both advisors from their .hd files, both at home
	virtual void Load() = 0;
	/// Load has run (an advisor whose file is missing has no clips and no mesh, and still runs its states)
	[[nodiscard]] virtual bool IsLoaded() const = 0;

	/// Once a frame: the advisors move, act and are posed
	virtual void Update(const Frame& frame) = 0;
	/// Once a game turn: what they point at and look at is followed
	virtual void ProcessTurn() = 0;

	/// The advisors' logic, which the scripts drive
	[[nodiscard]] virtual help::spirits::AdvisorSpiritController& GetController() = 0;
	[[nodiscard]] virtual const help::spirits::AdvisorSpiritController& GetController() const = 0;

	// Ours, until his advisor models and renderer come
	/// The frame's advisors and their trails, written into the frame's overlay before the draw
	virtual void FillOverlay(graphics::OverlayFrame& frame) = 0;
	/// An advisor's .hd file as loaded, nothing without it
	[[nodiscard]] virtual const help::HelpDudeFile* GetFile(int advisor) const = 0;
};

} // namespace openblack::ecs::systems
