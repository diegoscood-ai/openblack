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

#include "Particles/ParticleDrawFrame.h"

namespace openblack::graphics
{

/// One entry of the frame's transparency queue (zsort::Queue): what DrawPass needs to draw it when the queue is
/// drained, in place of the original's object and callback. One of the indices is set; with none, an instance of the
/// models (a mesh flagged to be sorted; a fading one; the hand).
/// (approximate) the arrival order, which breaks ties between equal keys, is not the original's: here clouds, then the
/// models mesh by mesh (a map), fading ones, the shield domes' hook's (none today), the PSys sprites, meshes, chains and Queued
/// effects, sprites, mists, smoke, the temples' leash posts (each post's smoke, then its collar), the columns of light, rain,
/// boat, ripples, the intro light, the gesture icons, the villagers' names, the did-you-know bubble, the tribes' names and the
/// globes' hook's (none today); there the order in which the frame queues them
/// (docs/wiki/rendering/frame.md#draw-stages-in-order). DrawPass fills the queue; the hooks of their own files push into it too
/// (Renderer::DrawShieldDomes, Renderer::DrawGlobes)
struct ZObject
{
	entt::id_type meshId {0};
	uint32_t index {0};
	bool morphWithTerrain {false};
	bool fading {false};
	entt::entity sprite {entt::null};
	int dust {-1}; ///< a puff of the pass's dust list (DynamicsSystemInterface::SnapshotDust), drawn as the sprites
	/// a thing of a Sorted effect: a sprite (manager::SortedFrame::sprites), a mesh atom, opaque or not
	/// (RenderContext::particleAtoms) or a chain (manager::SortedFrame::chains)
	std::optional<particles::draw::Item> particleItem;
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
	/// the did-you-know bubble, DrawSceneDesc::overlay.didYouKnow
	int didYouKnow {-1};
	/// a villager's name over its head, an index of DrawSceneDesc::overlay.villagerNames
	int villagerName {-1};
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
	int lightColumn {-1}; ///< a column of light (a town's or a temple's), an index of Renderer::_frameLightColumns
	int creedGlow {-1};   ///< a creature's creed glow, an index of Renderer::_frameCreedGlows
	int gestureIcon {-1}; ///< a gesture icon, an index of DrawSceneDesc::overlay.gestureIcons
	/// the leash target marker of the local player's creature (Renderer::LeashMarkerSprites), its ring and footprint
	int leashMarker {-1};

	/// a model instance (meshId / index): none of the other kinds is set. The drain draws it with drawInstance, its
	/// shadows inside it (DrawShadowsOnObject); a new kind must be added here too
	[[nodiscard]] bool IsModel() const
	{
		return sprite == entt::null && dust < 0 && !particleItem.has_value() && queuedEffect < 0 && mist < 0 && smoke < 0 &&
		       cloud < 0 && rain < 0 && snow < 0 && boat < 0 && ripple < 0 && introLight < 0 && didYouKnow < 0 &&
		       villagerName < 0 && tribalPower < 0 && shieldDome < 0 && globe < 0 && leashSmoke < 0 && leashCollar < 0 &&
		       lightColumn < 0 && creedGlow < 0 && gestureIcon < 0 && leashMarker < 0;
	}
};

} // namespace openblack::graphics
