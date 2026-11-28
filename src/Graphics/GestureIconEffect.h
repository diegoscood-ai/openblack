/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <optional>
#include <vector>

#include "Graphics/Mists.h"
#include "Graphics/OverlayFrame.h"
#include "Particles/Creators/Mesh.h"
#include "Particles/ParticleEffect.h"

namespace openblack::graphics
{
/// The holder effect of a seed's graphic inside a gesture icon (Magic/Gestures/GestureIconSeeds.h): its sprites, its
/// mists and its meshes where the step of this frame's draw left them, in its draw order, around the models' camera's
/// focus. They are drawn at once with the models' camera and moved on the screen to the box above the icon
/// (Graphics/RendererGestureIcons.cpp)
struct GestureIconEffect
{
	/// One draw of the walk: a run of sprites, a mist or a mesh
	struct Draw
	{
		std::vector<particles::Effect::DrawAtom> sprites;
		std::optional<mists::MistDesc> mist;
		std::optional<GestureIconModelOverlay> mesh;
	};
	std::vector<Draw> draws;
};

/// A holder effect's draws from its atoms in their draw order: each run of sprites (but not an atom laying a glowing
/// shell), each mist `mistOf` describes and each mesh atom `meshOf` makes a model of, in the walk's order; the other
/// atoms (chains, the rest) draw nothing
[[nodiscard]] GestureIconEffect
IconEffectOf(std::vector<particles::Effect::OrderedItem> items,
             const std::function<std::optional<mists::MistDesc>(const particles::Effect::DrawAtom&)>& mistOf,
             const std::function<std::optional<GestureIconModelOverlay>(const particles::Effect::DrawAtom&)>& meshOf);

/// A holder effect's mesh atom as the icon draws it, a model of the models' camera as the seed's own mesh: its mesh,
/// matrix, texture offset, alpha, colour and specular, lit as its creator says, its colour a tint of the land's light
/// where it stands with DrawWithLandscapeColor (as the world's draw), and drawn with its own alpha when the atom takes
/// the global alpha (always a model today)
[[nodiscard]] std::optional<GestureIconModelOverlay> IconMeshOf(const particles::mesh_atoms::Instance& instance);
} // namespace openblack::graphics
