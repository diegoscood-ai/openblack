/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/mat4x4.hpp>

#include "Creature/TattooEditor.h"
#include "Creature/TattooMenu.h"

namespace openblack::ecs::components
{

/// The body that the tattoo editor of the options' Players page shows over the menu: a creature's body of its own, made
/// as the editor opens from the profile's physique file and gone as it closes. It has no place in the world (no
/// Transform and no Mesh), so nothing that draws, moves or picks the world's things finds it; it stands at the world's
/// origin in the editor's own view.
struct TattooMenuCreature
{
	/// The body as the physique file gave it
	tattoo_menu::Shown shown;
	/// Its idle actions and its look at the pointer, as the cave's copy has them while the editor is open on it
	creature_tattoo_editor::CopyLife life;
	/// Where it is drawn: at the world's origin, facing as its mesh does, at its species' drawn scale for the size of 1
	glm::mat4 model {1.0f};
};

} // namespace openblack::ecs::components
