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

#include <optional>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>

#include "Graphics/ParticleDrawFrame.h"

namespace openblack::graphics
{

/// One entry of the frame's transparency queue (zsort::Queue): what DrawPass needs to draw it when the queue is
/// drained, in place of the original's object and callback. One of the indices is set; with none, an instance of the
/// models (a mesh flagged to be sorted; a fading one; the hand).
/// (approximate) the arrival order, which breaks ties between equal keys, is not the original's: here clouds, then the
/// models mesh by mesh (a map), fading ones, the shield domes' hook's (none today), the PSys sprites, meshes, chains and Queued
/// effects, sprites, mists, smoke, the temples' leash posts (each post's smoke, then its collar), rain, boat, ripples, the
/// intro light, the tribes' names and the globes' hook's (none today); there the order in which the frame queues them
/// (docs/bw1-notes/original-frame.md). DrawPass fills the queue; the hooks of their own files push into it too
/// (Renderer::DrawShieldDomes, Renderer::DrawGlobes)
struct ZObject
{
	entt::id_type meshId {0};
	uint32_t index {0};
	bool morphWithTerrain {false};
	bool fading {false};
	entt::entity sprite {entt::null};
	int dust {-1}; ///< a puff of the pass's dust list (ecs::physics::Dust::Snapshot), drawn as the sprites
	/// a thing of a Sorted effect: a sprite (manager::SortedFrame::sprites), a mesh atom, opaque or not
	/// (RenderContext::psysAtoms) or a chain (manager::SortedFrame::chains)
	std::optional<particles::draw::Item> psysItem;
	/// a Queued effect (manager::CollectQueued), all of it, as one Z object
	int queuedEffect {-1};
	int mist {-1};   ///< an index of _frameMists
	int smoke {-1};  ///< an index of _frameSmoke (one Z object per chimney)
	int cloud {-1};  ///< a cloud of Locator::cloudSystem
	int rain {-1};   ///< an index of _frameRain (one per raining tile)
	int snow {-1};   ///< an index of _frameSnow (one per snowing tile)
	int boat {-1};   ///< an index of _frameBoatSprites (one per sprite)
	int ripple {-1}; ///< an index of influence::Ripples()
	/// the intro light, DrawSceneDesc::overlay.introLight
	int introLight {-1};
	/// a tribe's name round the hand or rising (an index of the frame's MiracleFxSystemInterface::GetTribalPowerRunners())
	int tribalPower {-1};
	/// a shield dome handed out by MagicShieldSystemInterface::GetDomes (an index of Renderer::_frameShieldDomes). None
	/// is queued today: the domes are map objects, queued as models. The drain passes over it
	int shieldDome {-1};
	/// a one-shot globe's draw handed to Renderer::DrawGlobes (an index of Renderer::_frameGlobes). None is queued today:
	/// the globes are entities, queued as models. The drain passes over it
	int globe {-1};
	int leashSmoke {-1};  ///< a temple leash post's smoke, an index of Renderer::_frameLeashPosts
	int leashCollar {-1}; ///< a temple leash post's collar, an index of Renderer::_frameLeashPosts

	/// a model instance (meshId / index): none of the other kinds is set. The drain draws it with drawInstance, its
	/// shadows inside it (DrawShadowsOnObject); a new kind must be added here too
	[[nodiscard]] bool IsModel() const
	{
		return sprite == entt::null && dust < 0 && !psysItem.has_value() && queuedEffect < 0 && mist < 0 && smoke < 0 &&
		       cloud < 0 && rain < 0 && snow < 0 && boat < 0 && ripple < 0 && introLight < 0 && tribalPower < 0 &&
		       shieldDome < 0 && globe < 0 && leashSmoke < 0 && leashCollar < 0;
	}
};

} // namespace openblack::graphics
