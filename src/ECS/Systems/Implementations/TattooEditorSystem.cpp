/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "TattooEditorSystem.h"

#include <algorithm>

#include "Creature/CreatureFace.h"
#include "Creature/CreatureLayers.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/TattooMenuCreature.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// The skins of the creatures in the registry, painted again by the skin system as their tattoos change
class RegistrySkin final: public TattooEditorSystem::Skin
{
public:
	[[nodiscard]] std::optional<creature_tattoo::Slots> Read(entt::entity creature) const override
	{
		if (!Locator::entitiesRegistry::has_value())
		{
			return std::nullopt;
		}
		// A creature gone from the registry, or an id now given to something else, has no skin
		const auto& registry = Locator::entitiesRegistry::value();
		if (!registry.Valid(creature))
		{
			return std::nullopt;
		}
		const auto* tattoos = registry.TryGet<const ecs::components::CreatureTattoos>(creature);
		return tattoos != nullptr ? std::optional(tattoos->slots) : std::nullopt;
	}

	void Write(entt::entity creature, size_t slot, const creature_tattoo::Slot& tattoo) override
	{
		if (Locator::creatureSkinSystem::has_value() && Locator::entitiesRegistry::has_value() &&
		    Locator::entitiesRegistry::value().Valid(creature))
		{
			Locator::creatureSkinSystem::value().SetTattoo(creature, slot, tattoo);
		}
	}

	void PutOn(entt::entity creature, const creature_tattoo::Slots& slots) override
	{
		if (Locator::creatureSkinSystem::has_value() && Locator::entitiesRegistry::has_value() &&
		    Locator::entitiesRegistry::value().Valid(creature))
		{
			Locator::creatureSkinSystem::value().PutOnTattoos(creature, slots);
		}
	}

	void RemakeMasks(entt::entity creature) override
	{
		if (Locator::creatureSkinSystem::has_value() && Locator::entitiesRegistry::has_value() &&
		    Locator::entitiesRegistry::value().Valid(creature))
		{
			Locator::creatureSkinSystem::value().RemakeTattooMasks(creature, creature_tattoo::Sheet::OwnPictures);
		}
	}

	void CopyPullsFace(creature_face::Face face) override
	{
		if (!Locator::entitiesRegistry::has_value())
		{
			return;
		}
		auto& registry = Locator::entitiesRegistry::value();
		const auto pull = [face](ecs::components::CreatureAnimation& animation) {
			animation.face = creature_layers::PullFace(animation.face, creature_face::AnimationOf(face),
			                                           TattooEditorSystem::k_TattooFaceMs, creature_face::Cue::Tattooed);
		};
		registry.Each<const ecs::components::CreatureCopy, ecs::components::CreatureAnimation>(
		    [&pull](entt::entity, const ecs::components::CreatureCopy&, ecs::components::CreatureAnimation& animation) {
			    pull(animation);
		    });
		// the body the Players page's editor shows, as the editor's own
		registry.Each<const ecs::components::TattooMenuCreature, ecs::components::CreatureAnimation>(
		    [&pull](entt::entity, const ecs::components::TattooMenuCreature&, ecs::components::CreatureAnimation& animation) {
			    pull(animation);
		    });
	}

	void DestroyShown(entt::entity shown) override
	{
		if (Locator::entitiesRegistry::has_value() && Locator::entitiesRegistry::value().Valid(shown))
		{
			Locator::entitiesRegistry::value().Destroy(shown);
		}
	}
};
} // namespace

TattooEditorSystem::TattooEditorSystem()
    : TattooEditorSystem(std::make_unique<RegistrySkin>())
{
}

TattooEditorSystem::TattooEditorSystem(std::unique_ptr<Skin> skin)
    : _skin(std::move(skin))
{
}

void TattooEditorSystem::Open(entt::entity creature, const creature_tattoo_editor::Orbit& view)
{
	Close();
	_open = true;
	_creature = creature;
	_session = creature_tattoo_editor::Open(_skin->Read(creature).value_or(creature_tattoo::Slots {}));
	_view = view;
	_hovered.reset();
	_hoveredPoint.reset();
	// The masks are made again from the sheet with the player's own pictures as the editor opens, and the creature is
	// painted
	if (HasSkin(creature))
	{
		_skin->RemakeMasks(creature);
	}
}

void TattooEditorSystem::OpenFromMenu(std::optional<entt::entity> playerCreature, entt::entity shown)
{
	Close();
	_open = true;
	_creature = playerCreature;
	_shown = shown;
	const auto slots = playerCreature.has_value() ? _skin->Read(*playerCreature) : std::nullopt;
	_session = creature_tattoo_editor::Open(slots.value_or(creature_tattoo::Slots {}));
	_hovered.reset();
	_hoveredPoint.reset();
	// The body shown wears the tattoos being edited from the start; the masks are made again from the player's
	// creature's own block of pictures when there is one
	if (HasSkin(shown))
	{
		_skin->PutOn(shown, _session.slots);
	}
	if (_creature.has_value() && HasSkin(*_creature))
	{
		_skin->RemakeMasks(*_creature);
	}
}

void TattooEditorSystem::Close()
{
	// The body shown from the Players page is the editor's, and goes with it
	if (_shown.has_value())
	{
		_skin->DestroyShown(*_shown);
	}
	_open = false;
	_creature.reset();
	_shown.reset();
	_hovered.reset();
	_hoveredPoint.reset();
}

void TattooEditorSystem::Update(float milliseconds)
{
	if (IsOpen())
	{
		creature_tattoo_editor::Turn(_view, milliseconds);
	}
}

void TattooEditorSystem::Steer(glm::ivec2 grab, glm::ivec2 pointer, float milliseconds)
{
	creature_tattoo_editor::Steer(_view, grab, pointer, milliseconds);
}

void TattooEditorSystem::Hover(std::span<const creature_tattoo_editor::SiteOnScreen> sites, glm::ivec2 pointer)
{
	_hovered = IsOpen() ? creature_tattoo_editor::SiteUnderPointer(sites, pointer) : std::nullopt;
	_hoveredPoint.reset();
	if (_hovered.has_value())
	{
		const auto site = std::ranges::find(sites, *_hovered, &creature_tattoo_editor::SiteOnScreen::site);
		_hoveredPoint = site->screen;
	}
}

bool TattooEditorSystem::Drop(uint8_t design, const glm::u8vec3& colour)
{
	if (!IsOpen())
	{
		return false;
	}
	const auto putOn = creature_tattoo_editor::Drop(_session, design, colour, _hovered);
	// The end of a drag, over a place or not, puts the tattoos on with a new block of the player's own pictures and makes
	// the masks again
	for (const auto creature : Wearers())
	{
		_skin->PutOn(creature, _session.slots);
	}
	// Put on, the shown copy smiles
	if (putOn)
	{
		_skin->CopyPullsFace(creature_face::Face::Smile);
	}
	return putOn;
}

std::optional<creature_tattoo::Slot> TattooEditorSystem::Lift()
{
	if (!IsOpen() || !_hovered.has_value())
	{
		return std::nullopt;
	}
	auto lifted = creature_tattoo_editor::Lift(_session, *_hovered);
	// Any press on a place, with a tattoo there or not, writes the tattoos again, makes the masks again from the block
	// the creature has and makes the shown copy look sad; over a place with no tattoo it does so again each frame the
	// button stays down
	Write(_session.slots);
	for (const auto creature : Wearers())
	{
		_skin->RemakeMasks(creature);
	}
	_skin->CopyPullsFace(creature_face::Face::Sad);
	return lifted;
}

creature_tattoo_editor::Accept TattooEditorSystem::Ok(bool networkGame)
{
	const auto accept = creature_tattoo_editor::Ok(_session, networkGame);
	if (accept == creature_tattoo_editor::Accept::Close)
	{
		Close();
	}
	return accept;
}

void TattooEditorSystem::Cancel()
{
	if (IsOpen())
	{
		Write(_session.opened);
	}
	Close();
}

bool TattooEditorSystem::HasSkin(entt::entity creature) const
{
	return _skin->Read(creature).has_value();
}

std::vector<entt::entity> TattooEditorSystem::Wearers() const
{
	// Only a creature that still has its skin takes the tattoos
	std::vector<entt::entity> wearers;
	for (const auto& creature : {_creature, _shown})
	{
		if (creature.has_value() && HasSkin(*creature))
		{
			wearers.push_back(*creature);
		}
	}
	return wearers;
}

void TattooEditorSystem::Write(const creature_tattoo::Slots& slots)
{
	for (const auto creature : Wearers())
	{
		for (size_t i = 0; i < slots.size(); ++i)
		{
			_skin->Write(creature, i, slots.at(i));
		}
	}
}
