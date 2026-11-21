/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TattooMenu.h"

#include <PhysiqueFile.h>
#include <fmt/format.h>

#include "Creature/CreatureMindFileBody.h"

using namespace openblack;

tattoo_menu::Shown tattoo_menu::StandIn()
{
	return {.species = CreatureType::GiantApe,
	        .morph = {.evilGood = 0.0f, .thinFat = 0.5f, .weakStrong = 0.5f},
	        .firstMarks = {},
	        .secondMarks = {}};
}

tattoo_menu::Shown tattoo_menu::FromPhysique(const std::optional<creaturemind::PhysiqueFileData>& physique,
                                             const SpeciesStrength& speciesStrength)
{
	if (!physique.has_value())
	{
		return StandIn();
	}
	const auto species = creature_mind_body::SpeciesFromRow(physique->speciesRow);
	if (!species.has_value())
	{
		return StandIn();
	}
	return {.species = *species,
	        .morph = creature_morph::FromAttributes(physique->alignment, physique->fatness, physique->strength,
	                                                speciesStrength(*species)),
	        .firstMarks = physique->listA,
	        .secondMarks = physique->listB};
}

bool tattoo_menu::ButtonShown(bool physiqueKept, bool creatureShown, bool restartPending)
{
	return physiqueKept && creatureShown && !restartPending;
}

void tattoo_menu::Step(creature_tattoo_editor::CopyLife& life, creature_layers::BodyAction& body,
                       creature_layers::GestureLayer& gesture, std::optional<glm::vec3>& lookAt, const Input& input,
                       const creature_tattoo_editor::Orbit& view, const std::function<bool(size_t)>& hasAnimation,
                       const std::function<uint32_t(int32_t)>& localRand)
{
	const creature_tattoo_editor::CopyFrame frame {
	    .editorOpen = true,
	    .eye = view.eye,
	    .focus = view.focus,
	    .buttonHeld = input.buttonHeld,
	    .pointer = input.pointer,
	    .leftHeld = input.leftHeld,
	    .ticks = input.ticks,
	    .frameMs = input.frameMs,
	};
	// the turn it would want to face the eye is never made: it stays as it is
	static_cast<void>(creature_tattoo_editor::CopyStep(life, body, gesture, lookAt, frame, hasAnimation, localRand));
	life.turnWanted = false;
}

std::vector<tattoo_menu::Layer> tattoo_menu::DrawOrder(bool hasHair, bool hairShown, bool hasEyes)
{
	std::vector<Layer> layers {Layer::Body};
	if (hasHair && hairShown)
	{
		layers.push_back(Layer::Hair);
	}
	if (hasEyes)
	{
		layers.push_back(Layer::Eyes);
	}
	return layers;
}

std::string tattoo_menu::Readout(const Shown& shown, const creature_tattoo_editor::CopyLife& life)
{
	return fmt::format("species {}, evil-good {:.2f}, thin-fat {:.2f}, weak-strong {:.2f}, marks {} and {}, next idle in "
	                   "{} ms, {} idles",
	                   static_cast<int>(shown.species), shown.morph.evilGood, shown.morph.thinFat, shown.morph.weakStrong,
	                   shown.firstMarks.size(), shown.secondMarks.size(), life.idleMs, life.idles);
}
