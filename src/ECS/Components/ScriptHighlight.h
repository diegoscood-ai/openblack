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

#include <array>
#include <optional>

#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

/// One of the sparkles that rise over a gold scroll: its clock in game ms (drawn from 0, wrapping at 8000) and the
/// angle its sprite is turned by, drawn again at each wrap
struct HighlightSparkle
{
	int32_t ms {0};
	float angle {0.0f};
};

/// The sparkles every highlight makes at its creation, which only a gold scroll's Draw draws: twenty sprites of
/// S_SpriteSheet1 (mode 13) rising from a point over the scroll. Logic: ECS/ScriptHighlight.h
struct HighlightSparkles
{
	std::array<HighlightSparkle, 20> sparkles {};
	/// Added to each sparkle's cell (LocalRand(0x20) at creation, kept as a byte)
	uint8_t cellOffset {0};
	/// The sparkles' greatest alpha
	uint8_t maxAlpha {0xFF};
	/// Where they rise from, which the highlight's Draw sets
	glm::vec3 position {0.0f};
};

/// A script highlight (a single-map fixed object): the scroll or did-you-know sign a script puts on the map
/// (CREATE_HIGHLIGHT 272) or an abode puts by its door (when it creates its surrounding objects). The entity also has
/// a Transform (the position the model gets each frame) and a Mesh (the info's normal or active mesh). Logic:
/// ECS/ScriptHighlight.h.
struct ScriptHighlight
{
	/// The script highlight info row, 0..3 (ecs::script_highlight::Info)
	uint32_t infoIndex {0};
	/// Where it stands (x, z) and its altitude over the ground, which Process sets every turn (a ground query or the
	/// draw height) and SET_PROPERTY YPOS writes
	float x {0.0f};
	float z {0.0f};
	float altitude {0.0f};
	/// The fixed object's scale (Create's last argument, 1.0 from CREATE_HIGHLIGHT)
	float scale {1.0f};
	/// Nothing is drawn while it is set; Save / Load keep it. (pending) no writer found besides a reset to 0 and Load
	int32_t hidden {0};
	/// Set by SetActivated, read by IsActive
	bool active {false};
	/// The glints, the info's glint effect made at creation; psys::manager id. The highlight's Draw steps them
	uint32_t glintsEffect {0};
	/// The glints have not been stepped yet: their first step is a double one
	bool glintsFirstStepPending {false};
	/// The highlight's Draw stepped the glints this frame, so they are drawn
	bool glintsDrawn {false};
	/// The active effect, the info's active effect (SetActivated); psys::manager id. The highlight's Draw moves it to
	/// the mesh's centre and steps it
	uint32_t activeEffect {0};
	/// The active effect has not been stepped yet: its first step is a double one
	bool activeFirstStepPending {false};
	/// The highlight's Draw stepped the active effect this frame, so it is drawn
	bool activeDrawn {false};
	/// The sparkles made at its creation (every row has them; only a gold scroll's Draw draws them)
	std::optional<HighlightSparkles> sparkles;
	/// The script id. Create's third argument (CREATE_HIGHLIGHT's challenge id), then HIGHLIGHT_PROPERTIES' text
	/// (SetScriptId): the HelpText of a did-you-know, the challenge of a scroll
	uint32_t scriptId {0};
	/// SetDrawHeight (SET_PROPERTY YPOS): Process keeps the altitude at drawHeight; none until it is set
	std::optional<float> drawHeight;
	/// SetScriptId's DYK_CATEGORY
	DykCategory category {DykCategory::Navigation};
	/// The model's Y angle, which Draw turns by the frame's game time x pi / 1000 every frame
	float drawAngle {0.0f};
};

} // namespace openblack::ecs::components
