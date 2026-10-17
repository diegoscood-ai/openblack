/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "MiracleFxSystem.h"

#include "Audio/Audio.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "Magic/Core/OneOffSpellSeed.h"
#include "Magic/Hand/HandMagicFX.h"

using namespace openblack;
using namespace openblack::ecs::systems;

void MiracleFxSystem::UpdateGlobes(float gameSeconds)
{
	// the globes' animation frame, with the game time step in ms
	magic::one_off::UpdateFrames(gameSeconds * 1000.0f);
}

void MiracleFxSystem::UpdateHand(float gameSeconds)
{
	// the hand's draw: the hand effects and the spell in the hand (game time in ms)
	magic::hand_fx::Update(gameSeconds);
	magic::hand_fx::UpdateInHandEffect(gameSeconds * 1000.0f);
}

void MiracleFxSystem::UpdatePiles(float seconds)
{
	ecs::archetypes::PotArchetype::UpdateSizes(seconds);
}

void MiracleFxSystem::SeedInHand(entt::entity seed, int powerUp, int previousPowerUp)
{
	// the seed's in-hand effect, the power-up level (delayed) and, unless the level went down, the spell's hand visuals
	magic::hand_fx::CreateInHandEffect(seed);
	magic::hand_fx::SetPowerUpLevel(powerUp + 1, true);
	if (previousPowerUp <= powerUp)
	{
		magic::hand_fx::AddSpellToHandVisuals(false);
	}
	// power-up 0 / 1 / 2 -> SpellDialogue samples 10 / 11 / 12; none for -1. No owner, mode 2, no loop, not 3D
	if (powerUp >= 0 && powerUp <= 2)
	{
		audio::PlaySoundEffect(audio::Owner::None(), 10 + powerUp, 2, 0, false, false, audio::SfxBank::SpellDialogue);
	}
}

void MiracleFxSystem::SeedLeftHand()
{
	magic::hand_fx::ReleaseInHandEffect();
	magic::hand_fx::SetPowerUpLevel(0, false);
}

void MiracleFxSystem::SeedShakenOff()
{
	magic::hand_fx::RemoveHandSpellVisuals();
}

void MiracleFxSystem::Reset()
{
	magic::hand_fx::Reset();
}

void MiracleFxSystem::SetInterface(const gui::GameInterface* interface)
{
	magic::hand_fx::SetInterface(interface);
}

void MiracleFxSystem::StartTribalPowerRing(Tribe tribe)
{
	magic::hand_fx::StartTribalPowerRing(static_cast<int>(tribe));
}

void MiracleFxSystem::StopTribalPowerRing()
{
	magic::hand_fx::StopTribalPowerRing();
}

void MiracleFxSystem::ReleaseTribalPowerRing(Tribe tribe, glm::vec3 handPosition)
{
	magic::hand_fx::ReleaseOrCreateTribalPowerRing(static_cast<int>(tribe), handPosition);
}

void MiracleFxSystem::TribalPowerColumn(Tribe tribe, glm::vec3 position, PlayerNames player)
{
	magic::hand_fx::CreateTribalPowerColumn(static_cast<int>(tribe), position, player);
}

std::vector<const magic::tribal_spin::Runner*> MiracleFxSystem::GetTribalPowerRunners() const
{
	return magic::hand_fx::GetTribalPowerRunners();
}

std::vector<OrientedTextVertex> MiracleFxSystem::GetTribalPowerText(const magic::tribal_spin::Runner& runner) const
{
	return magic::hand_fx::GetTribalPowerText(runner);
}

const graphics::Texture2D* MiracleFxSystem::GetTextTexture() const
{
	return magic::hand_fx::GetTribalPowerTexture();
}
