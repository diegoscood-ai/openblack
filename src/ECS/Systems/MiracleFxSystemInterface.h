/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "3D/OrientedText.h"
#include "Enums.h"

namespace openblack::gui
{
class GameInterface;
} // namespace openblack::gui

namespace openblack::graphics
{
class Texture2D;
} // namespace openblack::graphics

namespace openblack::magic::tribal_spin
{
class Runner;
} // namespace openblack::magic::tribal_spin

namespace openblack::ecs::systems
{

/// How the miracles look outside their own effects, frame by frame: the one-shot globes, the hand holding a miracle
/// (its bands, its glow, the miracle's effect in it and the tribe's name round it) and the piles of food and wood
/// rising and sinking for what they hold. The miracles' code tells it when a miracle comes to the hand, powers up, or
/// leaves it.
class MiracleFxSystemInterface
{
public:
	virtual ~MiracleFxSystemInterface() = default;

	/// The globes' animation some seconds of game time on
	virtual void UpdateGlobes(float gameSeconds) = 0;
	/// The hand's bands, glow, tribe's names and in-hand effect some seconds of game time on (0 while paused)
	virtual void UpdateHand(float gameSeconds) = 0;
	/// The piles rising or sinking to their amount, some seconds on
	virtual void UpdatePiles(float seconds) = 0;

	/// A miracle came to the hand at a power-up level, or powered up from one: the seed's effect plays in the hand, the
	/// hand wears a bracelet for it and one for each power-up, bands fly onto it with their sound unless the level fell,
	/// and the announcer names the power-up
	virtual void SeedInHand(entt::entity seed, int powerUp, int previousPowerUp) = 0;
	/// The miracle left the hand: its effect and the bracelets go
	virtual void SeedLeftHand() = 0;
	/// The miracle was shaken off the hand: a band flies off it to the camera
	virtual void SeedShakenOff() = 0;
	/// Everything shown goes, for a new land
	virtual void Reset() = 0;

	/// The interface whose font a tribe's power is written in, none while there is none
	virtual void SetInterface(const gui::GameInterface* /*interface*/) {}
	/// A tribe's power is behind the miracle that came to this computer's hand: the tribe's name rings the hand
	virtual void StartTribalPowerRing(Tribe /*tribe*/) {}
	/// The miracle left the hand: a ring still round it goes
	virtual void StopTribalPowerRing() {}
	/// The miracle in this computer's hand was cast with a tribe's power behind it: the ring is let go where the hand is
	/// and rises as a column of the name; without a ring a column rises there anyway
	virtual void ReleaseTribalPowerRing(Tribe /*tribe*/, glm::vec3 /*handPosition*/) {}
	/// Another player cast a miracle with a tribe's power behind it: a column of the name rises there in their colour
	virtual void TribalPowerColumn(Tribe /*tribe*/, glm::vec3 /*position*/, PlayerNames /*player*/) {}
	/// The tribes' names now: the ring round the hand, if there is one, then the rising columns. Each is drawn as a
	/// see-through object of its own, sorted by its Position(); none by default
	[[nodiscard]] virtual std::vector<const magic::tribal_spin::Runner*> GetTribalPowerRunners() const { return {}; }
	/// A ring's or column's letters as triangles in the world (none without a font, and none by default)
	[[nodiscard]] virtual std::vector<OrientedTextVertex> GetTribalPowerText(const magic::tribal_spin::Runner& /*runner*/) const
	{
		return {};
	}
	/// The font's texture the letters are cut from (none without a font, and none by default)
	[[nodiscard]] virtual const graphics::Texture2D* GetTextTexture() const { return nullptr; }
};

} // namespace openblack::ecs::systems
