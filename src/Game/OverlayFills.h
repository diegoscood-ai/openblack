/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <span>
#include <vector>

#include <entt/entity/fwd.hpp>

#include "3D/Billboard.h"
#include "Graphics/OverlayFrame.h"
#include "Gui/VillagerLabels.h"

namespace openblack::ecs::systems
{
class CinematicDirectorSystemInterface;
} // namespace openblack::ecs::systems

// What the frame's logic lays out for the overlays the draw reads (Graphics/OverlayFrame.h), once a frame before the
// draw
namespace openblack::game_overlay
{

void FillKnockReadout(std::span<const entt::entity> drawn, const graphics::billboard::CameraFrame& frame,
                      std::vector<graphics::SpiritQuadVertex>& out, std::vector<graphics::KnockRow>& rows);
void FillGestureIcons(graphics::OverlayFrame& out);
void FillFloatingNumbers(int width, int height, std::vector<graphics::FloatingNumberOverlay>& out);
void FillVillagerNames(std::span<const gui::villager_labels::Label> labels, std::vector<graphics::VillagerNameOverlay>& out);
void FillOverlayFrame(const ecs::systems::CinematicDirectorSystemInterface& director, bool boxActive,
                      graphics::OverlayFrame& out);

} // namespace openblack::game_overlay
