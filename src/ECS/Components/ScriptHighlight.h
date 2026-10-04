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

#include "Enums.h"

namespace openblack::ecs::components
{

/// ScriptHighlight (runblack.exe W120, 0x8C bytes, vtable 0x94228C; : SingleMapFixed): the scroll or did-you-know sign
/// a script puts on the map (CREATE_HIGHLIGHT 272) or an abode puts by its door (Abode::CreateAbodeSurroundingObjects).
/// The entity also has a Transform (what LH3DObject::SetPosition gets each frame) and a Mesh (the info's normal or
/// active mesh). Logic and research: ECS/ScriptHighlight.h, dev\documentacion\intro\spec_highlight.md.
struct ScriptHighlight
{
	/// Object +0x28: the GScriptHighlightInfo row ((info - 0xD96390) / 272), 0..3 (ecs::script_highlight::Info)
	uint32_t infoIndex {0};
	/// +0x14 / +0x1C: where it stands (x, z) and its altitude over the ground (Pos.altitude), which Process sets every
	/// turn (fn_006022C0 or the draw height) and SET_PROPERTY YPOS writes
	float x {0.0f};
	float z {0.0f};
	float altitude {0.0f};
	/// FixedObject's scale (Create's last argument, 1.0 from CREATE_HIGHLIGHT 0x6F1C92)
	float scale {1.0f};
	/// +0x5C: Draw 0x709C8B draws nothing while it is set; Save / Load keep it. (pending) no writer found besides
	/// fn_00709910 (= 0) and Load
	int32_t hidden {0};
	/// +0x60: SetActivated 0x70A643 (IsActive 0x709750)
	bool active {false};
	/// +0x6C: the glints, GJPSysInterface of info +0x108 (CallVirtualFunctionsForCreation 0x709C0E); psys::manager id
	uint32_t glintsEffect {0};
	/// +0x70: the active effect, info +0x10C (SetActivated 0x70A707); psys::manager id
	uint32_t activeEffect {0};
	/// +0x78: the script id. Create's third argument (the ctor 0x7098FB: CREATE_HIGHLIGHT's challenge id), then
	/// HIGHLIGHT_PROPERTIES' text (SetScriptId 0x709A28): the HelpText of a did-you-know, the challenge of a scroll
	uint32_t scriptId {0};
	/// +0x7C / +0x80: SetDrawHeight 0x709C40 (SET_PROPERTY YPOS 0x70EFB3): Process keeps the altitude at drawHeight
	bool hasDrawHeight {false};
	float drawHeight {0.0f};
	/// +0x84: SetScriptId's DYK_CATEGORY
	DykCategory category {DykCategory::Navigation};
	/// The LH3DObject's Y angle (its +0x48), which Draw turns by g_game_time_inc x pi / 1000 every frame (0x709CE8..)
	float drawAngle {0.0f};
};

} // namespace openblack::ecs::components
