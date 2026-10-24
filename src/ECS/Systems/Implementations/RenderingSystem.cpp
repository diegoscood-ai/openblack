/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "RenderingSystem.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <optional>
#include <span>

#include <entt/core/hashed_string.hpp>
#include <glm/gtx/transform.hpp>
#include <spdlog/spdlog.h>

#include "3D/DayNightClock.h"
#include "3D/FrameAnim.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLight.h"
#include "3D/MapCoords.h"
#include "3D/NightLights.h"
#include "3D/ObjectMatrix.h"
#include "Camera/Camera.h"
#include "Common/ProcessLogOnce.h"
#include "ECS/Abodes.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/ComputerHand.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/DetailMesh.h"
#include "ECS/Components/DrawMesh.h"
#include "ECS/Components/DrawnWith.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/HandFxPart.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/NeedsSorting.h"
#include "ECS/Components/NotDrawn.h"
#include "ECS/Components/ObjectColour.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SeeThrough.h"
#include "ECS/Components/SpecularColour.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/TreeRoots.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Unlit.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/ComputerHand.h"
#include "ECS/DrawList/DrawGate.h"
#include "ECS/DrawnMesh.h"
#include "ECS/Fields.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Fire/FireGraphic.h"
#include "ECS/Life.h"
#include "ECS/MissionaryBoat.h"
#include "ECS/MobileDrawing.h"
#include "ECS/ObjectGhosts.h"
#include "ECS/Registry.h"
#include "ECS/ShadowReceiver.h"
#include "ECS/SuperVillager.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HeartbeatSystemInterface.h"
#include "ECS/Systems/VegetationInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/Graveyard.h"
#include "ECS/Trees.h"
#include "EngineConfig.h"
#include "Game.h"
#include "GameClock.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/DebugLines.h"
#include "Graphics/DetailLevel.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/Haze.h"
#include "Graphics/ShaderManager.h"
#include "Locator.h"
#include "Particles/Creators/Mesh.h"
#include "Particles/ParticleManager.h"
#include "Particles/Rules/SurfRevol.h"
#include "Resources/ResourcesInterface.h"
#include "Temple/HeartFade.h"

using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

RenderingSystem::~RenderingSystem() = default;

namespace openblack::ecs::systems
{
/// The components a row asks about more than once, each looked up once per row and passed down to the helpers below
struct RowParts
{
	const components::Alpha* alpha;
	const components::Tree* tree;
	const components::Field* field;
	const components::Abode* abode;
	const components::Feature* feature;
	const components::Animal* animal;
	const components::Pot* pot;
	const components::OneOffSpellSeed* orb;
	const components::MagicShield* shield;
	bool spellIcon;
	bool villager;
	bool creature;
	bool hand;
	bool deadTree;
	bool bigForest;
	bool morphWithTerrain;
	bool needsSorting;
	const components::DrawMesh* drawMesh;
	/// a villager's, an animal's or a bird's high mesh (drawn_mesh::Of)
	const components::DetailMesh* detailMesh;
	bool unlit;
	const components::Temple* temple;
};
} // namespace openblack::ecs::systems

namespace
{
/// Whether a mesh atom is drawn with DrawCutByPlane: a flag of the particle (copied from its creator), tested on both
/// the immediate and the sorted path, which then draw it cut by the plane instead of the plain draw:
/// particles::mesh_atoms::Instance::cutByPlane
[[nodiscard]] bool AtomCutByPlane(const openblack::particles::mesh_atoms::Instance& atom)
{
	return atom.cutByPlane;
}

/// A cut atom that also draws with the land colour would need the land-coloured tint under the cut's own light
/// (inferred, no effect known to set both): not ported, drawn as today
[[nodiscard]] bool AtomDrawnCut(const openblack::particles::mesh_atoms::Instance& atom)
{
	if (!AtomCutByPlane(atom))
	{
		return false;
	}
	if (atom.landscapeColour)
	{
		if (openblack::log_once::First("graphics/psys-atom-cut-with-land-colour"))
		{
			SPDLOG_LOGGER_WARN(
			    spdlog::get("graphics"),
			    "PSys mesh atom with DrawCutByPlane and DrawWithLandscapeColor: the cut is not ported, drawn uncut");
		}
		return false;
	}
	return true;
}

/// The graphics detail level the drawn models follow (the default one before the game's settings exist)
[[nodiscard]] uint8_t DetailLevelNow()
{
	return openblack::Locator::config::has_value() ? openblack::Locator::config::value().detailLevel
	                                               : openblack::graphics::detail_level::k_Default;
}

/// The model an entity's model passes draw (drawn_mesh::Of): the layout counts its ranges with it and the rows are
/// written with it, so that both find the same range. The Mesh stays the object's own (sizes, map cells, static
/// shadow, footprint)
Mesh DrawnMeshOf(const openblack::ecs::Registry& registry, entt::entity entity, const Mesh& mesh)
{
	return openblack::ecs::drawn_mesh::Of(registry.TryGet<const DrawMesh>(entity), mesh,
	                                      registry.TryGet<const DetailMesh>(entity), DetailLevelNow());
}

/// Which of the draw layout's components an entity has (RowClasses)
[[nodiscard]] RowClasses RowClassesOf(const openblack::ecs::Registry& registry, entt::entity entity)
{
	RowClasses classes {
	    .alpha = registry.AllOf<Alpha>(entity),
	    .tree = registry.AllOf<Tree>(entity),
	    .field = registry.AllOf<Field>(entity),
	    .abode = registry.AllOf<Abode>(entity),
	    .feature = registry.AllOf<Feature>(entity),
	    .pot = registry.AllOf<Pot>(entity),
	    .villager = registry.AllOf<Villager>(entity),
	    .creature = registry.AllOf<Creature>(entity),
	    .hand = registry.AllOf<Hand>(entity),
	    .deadTree = registry.AllOf<DeadTree>(entity),
	    .bigForest = registry.AllOf<BigForest>(entity),
	    .morphWithTerrain = registry.AllOf<MorphWithTerrain>(entity),
	    .needsSorting = registry.AllOf<NeedsSorting>(entity),
	    .drawMesh = registry.AllOf<DrawMesh>(entity),
	    .unlit = registry.AllOf<Unlit>(entity),
	};
	return classes;
}

/// The row's components: those of its classes looked up only when it has them
[[nodiscard]] RowParts RowPartsOf(const openblack::ecs::Registry& registry, entt::entity entity, const RowClasses& classes)
{
	return {
	    .alpha = classes.alpha ? registry.TryGet<const Alpha>(entity) : nullptr,
	    .tree = classes.tree ? registry.TryGet<const Tree>(entity) : nullptr,
	    .field = classes.field ? registry.TryGet<const Field>(entity) : nullptr,
	    .abode = classes.abode ? registry.TryGet<const Abode>(entity) : nullptr,
	    .feature = classes.feature ? registry.TryGet<const Feature>(entity) : nullptr,
	    .animal = registry.TryGet<const Animal>(entity),
	    .pot = classes.pot ? registry.TryGet<const Pot>(entity) : nullptr,
	    .orb = registry.TryGet<const OneOffSpellSeed>(entity),
	    .shield = registry.TryGet<const MagicShield>(entity),
	    .spellIcon = registry.AllOf<SpellIcon>(entity),
	    .villager = classes.villager,
	    .creature = classes.creature,
	    .hand = classes.hand,
	    .deadTree = classes.deadTree,
	    .bigForest = classes.bigForest,
	    .morphWithTerrain = classes.morphWithTerrain,
	    .needsSorting = classes.needsSorting,
	    .drawMesh = classes.drawMesh ? registry.TryGet<const DrawMesh>(entity) : nullptr,
	    .detailMesh = registry.TryGet<const DetailMesh>(entity),
	    .unlit = classes.unlit,
	    .temple = registry.TryGet<const Temple>(entity),
	};
}

[[nodiscard]] RowParts RowPartsOf(const openblack::ecs::Registry& registry, entt::entity entity)
{
	return RowPartsOf(registry, entity, RowClassesOf(registry, entity));
}


/// How a model's draw takes the land light (land_light::ObjectMode): the bilinear land light and the haze for most; a
/// tree the cell-shifted light, then the haze; a worship site and a spell icon the light of their cell without haze,
/// except a burning worship site (the fire's draw: bilinear and haze); a dove the full light without haze
/// ((inferred) every species of the Dove class draws with it).
/// The partly built draw of a building is the bilinear light without the haze (with a fire only the tint): it is taken
/// while the building is drawn as a building site, which for a Feature is the ArkDryDock while not built
/// (ecs/FeatureBuild.h), and a worship site or a totem not built yet. MissionaryBoat's hull gets the bilinear light
/// before its draw and no haze. Pending: a spell icon not built yet (openblack's spell icons are always built); an
/// abode drawn as a building site without a damage mesh takes the partly built mode (below); the repair part of a
/// damaged Abode
/// (partly built draw, no haze) is merged into its damage mesh, whose pieces take the haze, so the whole keeps it
/// (approximate); the boat's sailors and deck take their colour as a set colour (DrawColoursOf); the scaffold's phantom
/// building (cell-shifted light, no haze) is not drawn by openblack; a spell icon's specular colour branch (not read)
/// `fire`: the entity's fire (fire::Find), or null
openblack::land_light::ObjectLight LandLightOf(const openblack::ecs::Registry& registry, entt::entity entity,
                                               const RowParts& parts, const openblack::ecs::fire::FireEffect* fire)
{
	const auto* feature = parts.feature;
	const auto* super = registry.TryGet<const openblack::ecs::components::SuperVillager>(entity);
	const auto* seed = registry.TryGet<const SpellSeedGraphic>(entity);
	const auto* animal = parts.animal;
	return openblack::land_light::ObjectLightOf({
	    .tree = parts.tree != nullptr,
	    // the partly built draw: an ArkDryDock not finished, or an abode drawn as a building site (IsDrawBuilding) and
	    // no damage mesh (with a fire only the tint); the hull's light, set before its draw; a SuperVillager's HD body
	    // (ecs/SuperVillager.h): its draw takes the bilinear light again, which rewrites the colour and the specular on
	    // every path, and no haze: the haze the villager's draw put there is gone. So does one drawn with its own high
	    // mesh (no HD file): the same draw. A worship site not built yet is drawn as a building site
	    .bilinearWithoutHaze =
	        (feature != nullptr && feature->type == openblack::FeatureInfo::ArkDryDock && feature->percentBuilt < 1.0f) ||
	        (parts.abode != nullptr && openblack::ecs::abodes::IsDrawBuilding(entity) &&
	         !openblack::ecs::abodes::HasDestructionMesh(entity)) ||
	        entity == openblack::ecs::missionary_boat::GetHull() || super != nullptr ||
	        (registry.AllOf<WorshipSite>(entity) && !openblack::ecs::abodes::IsBuilt(entity)),
	    .worshipSite = registry.AllOf<WorshipSite>(entity),
	    // a built totem building, a town's or a script's puzzle totem, takes its cell's light like a worship site
	    .totem = parts.abode != nullptr && openblack::ecs::abodes::TypeOf(entity) == openblack::AbodeType::Totem,
	    .spellIcon = parts.spellIcon,
	    // a player seed's mesh in an icon or a one-shot orb (Worship/SpellSeedGraphic.cpp)
	    .cellSeed = seed != nullptr && seed->landCellLight,
	    .burning = fire != nullptr,
	    .flyingAnimal = animal != nullptr && openblack::ecs::animal_ai::IsFlyingSpecies(animal->type),
	});
}
/// A village totem's model: its plinth (the entity with the VillageTotem) or its icon is itself; a see-through copy of
/// either is the model it copies (the copy's DrawnWith step: 0 the plinth, 1 the icon). Nothing for anything else
std::optional<entt::entity> TotemModelOf(const openblack::ecs::Registry& registry, entt::entity entity)
{
	if (registry.AllOf<VillageTotem>(entity))
	{
		return registry.AllOf<SeeThrough>(entity) ? std::nullopt : std::optional(entity);
	}
	const auto* with = registry.TryGet<const DrawnWith>(entity);
	const auto* totem = with != nullptr ? registry.TryGet<const VillageTotem>(with->owner) : nullptr;
	if (totem == nullptr)
	{
		return std::nullopt;
	}
	if (!registry.AllOf<SeeThrough>(entity))
	{
		return totem->icon == entity ? std::optional(entity) : std::nullopt;
	}
	return with->step == 0 ? std::optional(with->owner) : std::optional(totem->icon);
}
/// The colour fields an object's draw leaves in its 3D object this frame (argb_colour::PackInstance*): the tint t
/// (x the land light), or a set colour (instead of it), and the specular
struct DrawColours
{
	std::optional<uint32_t> tint;
	std::optional<uint32_t> colour;
	uint32_t specular {0};
	/// The tree draw's own product after the haze, argb_colour::PackInstanceTreeTint
	bool tintAfterHaze {false};
};
/// The draw of an object with a fire, shared by DeadTree, FelledTree, Rock, MultiMapFixed, SingleMapFixed,
/// MobileObject, WorshipSite, Totem, Living and Animal, and the partly built draw (the same pair as a tint): the tint =
/// the charring grey (alpha 0xFF), the specular = the fire's glow (alpha 0xFF)
DrawColours Burning(const openblack::ecs::fire::FireEffect& fire)
{
	namespace argb_colour = openblack::argb_colour;
	const uint32_t grey = openblack::ecs::fire::graphic::CharringGrey(fire);
	const auto glow = openblack::ecs::fire::graphic::CharringGlow(fire, static_cast<float>(openblack::game_clock::Turn()) +
	                                                                        openblack::game_clock::TurnFraction());
	return {argb_colour::Argb(grey, grey, grey, 0xFF), std::nullopt, argb_colour::Argb(glow.r, glow.g, glow.b, 0xFF)};
}
/// What each class's draw passes as its tint, set colour and specular, for the classes openblack draws
/// through the instances (the fields' and trees' own tints stay at their call sites below)
/// `fire`: the entity's fire (fire::Find), or null
DrawColours DrawColoursOf(const openblack::ecs::Registry& registry, entt::entity entity, const RowParts& parts,
                          const openblack::ecs::fire::FireEffect* fire)
{
	namespace argb_colour = openblack::argb_colour;
	using openblack::AnimalInfo;
	constexpr uint32_t k_White = 0xFFFFFFFFu; // the white tint: every channel x 255 / 256
	const auto specularOf = [&registry, entity]() -> std::optional<uint32_t> {
		// the living's specular colour, tested as a whole with its alpha. (approximate) openblack has the component
		// only while its rgb is not 0: the heal chakra writes alpha 0xFF, so its fade frames with rgb 0 still take the
		// white tint in the original and the land light alone here (Particles/Rules/Heal.cpp drops the component at rgb 0)
		if (const auto* specular = registry.TryGet<const SpecularColour>(entity); specular != nullptr)
		{
			return argb_colour::Argb(specular->colour.r, specular->colour.g, specular->colour.b);
		}
		return std::nullopt;
	};
	// The power-up bands: the spell graphic sets its colour and specular; the hand FX's band writes the same fields
	// directly. (inferred) no land light runs on the hand band between those writes and its draw, so both end as a set
	// colour
	if (const auto* colour = registry.TryGet<const ObjectColour>(entity); colour != nullptr)
	{
		return {std::nullopt, argb_colour::Argb(colour->rgb[0], colour->rgb[1], colour->rgb[2]), colour->specular};
	}
	// the missionaries' boat's people and things: the colour set on them before they are drawn
	if (const auto place = openblack::ecs::missionary_boat::PeopleLightOf(entity); place.has_value())
	{
		const auto people = openblack::ecs::missionary_boat::PeopleColoursAt(openblack::land_light::GetCells(),
		                                                                     openblack::land_light::CurrentTable(), *place);
		return {std::nullopt, people.colour, people.specular};
	}
	// the town centre's draw (the icons in its slots, while the centre's life > 0): always the white tint and the
	// icon's specular colour (not ported: 0). (inferred) this is the write the frame keeps: the slot icon's own draw is
	// the spell icon's, and whether it also runs for the slot icons in the same frame (and after the town centre's) was
	// not read
	if (parts.spellIcon && registry.AllOf<TownCentreSpellIcon>(entity))
	{
		return {k_White, std::nullopt, 0};
	}
	// a spell icon's draw (the worship site's): the white tint only with a specular colour; without one the cell's
	// light alone, and as a building site the partly built draw. That colour is not ported (always 0), so never the
	// tint
	if (parts.spellIcon)
	{
		return {};
	}
	// the physical shield: the white tint, specular 0
	if (const auto* shield = parts.shield; shield != nullptr && shield->kind == MagicShield::Kind::Physical)
	{
		return {k_White, std::nullopt, 0};
	}
	// the one-shot orb (in the map or out of it): (alpha & 0xFF) << 24 | 0xFFFFFF, specular 0.
	// Its alpha is the caller's components::Alpha (see argb_colour::PackInstanceTint)
	if (parts.orb != nullptr)
	{
		return {k_White, std::nullopt, 0};
	}
	if (const auto* animal = parts.animal; animal != nullptr)
	{
		// the spell wolf: the colour alpha << 24 | 0xFFFFFF (the alpha is components::Alpha here), with a fire that
		// times the charring grey in the four channels (argb_colour::MultiplyArgbShift8) and the glow, else that colour and the
		// living's specular
		if (animal->type == AnimalInfo::SpellWolf)
		{
			if (fire != nullptr)
			{
				auto burning = Burning(*fire);
				burning.tint = argb_colour::MultiplyArgbShift8(k_White, *burning.tint);
				return burning;
			}
			return {k_White, std::nullopt, specularOf().value_or(0)};
		}
		// an animal: the fire first, then a specular colour with the white tint, else the land light alone
		if (fire != nullptr)
		{
			return Burning(*fire);
		}
		if (const auto specular = specularOf(); specular.has_value())
		{
			return {k_White, std::nullopt, *specular};
		}
		return {};
	}
	if (parts.villager)
	{
		// a living's or villager's draw: the fire, then a specular colour with the white tint, then a poisoned one with
		// the poison's pair, else the land light alone
		if (fire != nullptr)
		{
			return Burning(*fire);
		}
		if (const auto specular = specularOf(); specular.has_value())
		{
			return {k_White, std::nullopt, *specular};
		}
		if (registry.AllOf<Poisoned>(entity))
		{
			return {openblack::ecs::life::k_PoisonDiffuse, std::nullopt, openblack::ecs::life::k_PoisonSpecular};
		}
		return {};
	}
	// a pot (in the map or out of it) or a food pile: a poisoned pot without a fire takes the poison's pair, otherwise
	// the MobileObject draw (the fire's, below)
	if (const auto* pot = parts.pot; pot != nullptr && pot->poisoned && fire == nullptr)
	{
		return {openblack::ecs::life::k_PoisonDiffuse, std::nullopt, openblack::ecs::life::k_PoisonSpecular};
	}
	// the other classes with a fire (Burning). (inferred) every class drawn here that can burn goes through one of
	// those draws (the fire's, the partly built one) or carries the same pair inline (the spell wolf, above); the
	// creature and the hand do not; the damaged Abode's damage mesh takes its pair in BuildingDamageSystem::AppendDamageMeshes.
	// Not ported: the out-of-map draw's own pair and the physics prediction object; the citadel heart's draw has its own
	// pair (WriteEntityRow)
	if (fire != nullptr && parts.tree == nullptr && parts.field == nullptr && !parts.creature && !parts.hand)
	{
		return Burning(*fire);
	}
	return {};
}
/// Whether a row receives the projected shadows that fall on objects (ECS/ShadowReceiver.h)
bool ReceivesDynamicShadow(const openblack::ecs::Registry& registry, entt::entity entity, entt::id_type meshId,
                           const RowParts& parts)
{
	static constexpr auto k_PowerUpBand = entt::hashed_string("Power_Up_Band");
	return openblack::ecs::shadow_receiver::Receives({
	    .powerUpBand = meshId == k_PowerUpBand.value(),
	    .tree = parts.tree != nullptr || parts.deadTree || parts.bigForest,
	    .forest = registry.AllOf<Forest>(entity),
	    .hand = parts.hand,
	    .creature = parts.creature,
	    .orb = parts.orb != nullptr,
	    .shield = parts.shield != nullptr,
	    .flowers = registry.AllOf<Flowers>(entity),
	    .templePart = registry.AllOf<TempleInteriorPart>(entity),
	    .handFxPart = registry.AllOf<HandFxPart>(entity),
	    .desireFlag = registry.AllOf<TownDesireFlag>(entity),
	    .pot = parts.pot != nullptr ? std::optional(parts.pot->type) : std::nullopt,
	});
}
/// The land's height for the draw of what a computer hand holds, none without a land
class DrawGround final: public openblack::computer_hand::Ground
{
public:
	[[nodiscard]] float AltitudeAt(int32_t mapX, int32_t mapZ) const override
	{
		if (!openblack::Locator::terrainSystem::has_value())
		{
			return 0.0f;
		}
		return openblack::Locator::terrainSystem::value().GetHeightAt(
		    {openblack::map_coords::ToMetres(mapX), openblack::map_coords::ToMetres(mapZ)});
	}
};

} // namespace

void RenderingSystem::BeginPrepareDraw()
{
	// which blocks the land has, for where the rows' land light is the full light
	_landBlocks.reset();
	if (Locator::terrainSystem::has_value())
	{
		_landBlocks = land_light::BlocksOf(Locator::terrainSystem::value());
	}
	// What each computer player's hand holds is drawn where its hand was as its last turn of steps began, through the
	// map grid (computer_hand::HeldDrawPoint); the frame's game time goes by only while it holds something
	_computerHeld.clear();
	if (Locator::entitiesRegistry::has_value())
	{
		const auto& registry = std::as_const(Locator::entitiesRegistry::value());
		const DrawGround ground;
		const auto frameMs = openblack::game_clock::FrameGameMs();
		registry.Each<const ComputerHand>([this, &registry, &ground, frameMs](entt::entity player, const ComputerHand& hand) {
			auto& draw = _computerHeldDraws[player];
			if (hand.heldObject == entt::null || !registry.Valid(hand.heldObject))
			{
				return;
			}
			openblack::computer_hand::NewTurnsOfTheHand(draw, hand.turnStartPoint, hand.stepTurns - draw.turnsSeen);
			draw.turnsSeen = hand.stepTurns;
			const auto point = openblack::computer_hand::HeldDrawPoint(draw, frameMs);
			_computerHeld.emplace_back(hand.heldObject, openblack::computer_hand::OnTheMapGrid(point, ground));
		});
		std::erase_if(_computerHeldDraws, [&registry](const auto& entry) {
			return !registry.Valid(entry.first) || !registry.AllOf<ComputerHand>(entry.first);
		});
	}
	// the totems' lights the last frame left, for this frame's see-through copies
	for (const auto& [entity, light] : _totemLightsNow)
	{
		_totemLights.insert_or_assign(entity, light);
	}
	_totemLightsNow.clear();
	if (Locator::entitiesRegistry::has_value())
	{
		const auto& registry = std::as_const(Locator::entitiesRegistry::value());
		std::erase_if(_totemLights, [&registry](const auto& entry) { return !registry.Valid(entry.first); });
	}
	_treeBrightness.reset();
	// the citadel hearts' own specular colours: this frame's rows take what the last frame's draw left (none once a heart
	// is no longer being destroyed, as its end clears it), and from three and a half seconds into its destruction each
	// heart's draw leaves its god's colour by the heartbeat for the next
	if (Locator::entitiesRegistry::has_value())
	{
		const auto& registry = std::as_const(Locator::entitiesRegistry::value());
		std::erase_if(_heartSpeculars, [&registry](const auto& entry) {
			const auto* temple = registry.Valid(entry.first) ? registry.TryGet<const Temple>(entry.first) : nullptr;
			return temple == nullptr || !temple->destroying;
		});
		_heartSpecularsDrawn = _heartSpeculars;
		const float turnFraction = openblack::game_clock::TurnFraction();
		registry.Each<const Temple>([this, turnFraction](entt::entity heart, const Temple& temple) {
			if (!temple.destroying)
			{
				return;
			}
			const float pulse = Locator::heartbeatSystem::has_value()
			                        ? Locator::heartbeatSystem::value().Pulse(temple.owner, turnFraction)
			                        : 0.0f;
			if (const auto specular = openblack::heart_fade::Specular(
			        temple.destructionClock, pulse,
			        openblack::particles::surf_revol::PlayerColour(static_cast<int>(temple.owner)));
			    specular.has_value())
			{
				_heartSpeculars.insert_or_assign(heart, *specular);
			}
		});
	}
	_dayNightClock = Locator::dayNightClock::has_value() ? &Locator::dayNightClock::value().Clock() : nullptr;
}

void RenderingSystem::SetLayoutDirty()
{
	RenderingSystemCommon::SetLayoutDirty();
	_rowClassesStale = true;
}

void RenderingSystem::PrepareDrawDescs(bool drawBoundingBox)
{
	auto& registry = Locator::entitiesRegistry::value();
	// a rebuild finds every row's classes again; so does OPENBLACK_INSTANCE_VERIFY's build, which a refill's rows are
	// compared with
	_rowClassesStale = true;

	// Count number of instances
	uint32_t instanceCount = 0;
	std::unordered_map<entt::id_type, std::pair<uint32_t, bool>> meshIds;
	std::unordered_map<entt::id_type, uint32_t> translucentIds;
	// fading meshes that follow the land (fields, piles) keep doing it while they fade (per mesh: any of its entities)
	std::unordered_map<entt::id_type, bool> translucentMorph;

	// The count starts at 0: seeding it with the sub-mesh index miscounted every mesh drawn from another sub-mesh than
	// the first, and a whole-mesh draw (-1) wrapped to 0 instances, so its row was read from stale instance data
	auto prep = [&meshIds, &instanceCount](const Mesh& mesh, bool morphWithTerrain) {
		auto count = meshIds.insert(std::make_pair(mesh.id, std::make_pair(0u, morphWithTerrain)));
		count.first->second.first++;
		instanceCount++;
	};

	// (the model passes count the drawn model, DrawnMeshOf; components::NotDrawn is out of them)
	registry.Each<const Mesh, const Transform>(
	    [&prep, &registry](entt::entity entity, const Mesh& mesh, const Transform& /*unused*/) {
		    prep(DrawnMeshOf(registry, entity, mesh), false);
	    },
	    entt::exclude<MorphWithTerrain, TempleInteriorPart, Alpha, NotDrawn, NeedsSorting, Unavailable>);
	registry.Each<const Mesh, const Transform, const MorphWithTerrain>(
	    [&prep, &registry](entt::entity entity, const Mesh& mesh, const Transform& /*unused*/,
	                       const MorphWithTerrain& /*unused*/) { prep(DrawnMeshOf(registry, entity, mesh), true); },
	    // TempleInteriorPart as the write walk below: a row counted and never written keeps an earlier frame's matrix
	    entt::exclude<TempleInteriorPart, Alpha, NotDrawn, NeedsSorting, Unavailable>);
	// the opaque ones with components::NeedsSorting (the held object): their own range, queued like a mesh that asks
	// to be sorted; following the land per mesh, as the others
	std::unordered_map<entt::id_type, std::pair<uint32_t, bool>> sortedOpaqueIds;
	registry.Each<const Mesh, const Transform, const NeedsSorting>(
	    [&registry, &sortedOpaqueIds, &instanceCount](entt::entity entity, const Mesh& mesh, const Transform& /*unused*/) {
		    auto count =
		        sortedOpaqueIds.insert(std::make_pair(DrawnMeshOf(registry, entity, mesh).id, std::make_pair(0u, false)));
		    count.first->second.first++;
		    count.first->second.second = count.first->second.second || registry.AllOf<MorphWithTerrain>(entity);
		    ++instanceCount;
	    },
	    entt::exclude<TempleInteriorPart, Alpha, NotDrawn, Unavailable>);
	registry.Each<const Mesh, const Transform, const Alpha>(
	    [&registry, &translucentIds, &translucentMorph, &instanceCount](entt::entity entity, const Mesh& mesh,
	                                                                    const Transform& /*unused*/, const Alpha& /*unused*/) {
		    const auto drawn = DrawnMeshOf(registry, entity, mesh).id;
		    ++translucentIds[drawn];
		    translucentMorph[drawn] = translucentMorph[drawn] || registry.AllOf<MorphWithTerrain>(entity);
		    ++instanceCount;
	    },
	    entt::exclude<TempleInteriorPart, NotDrawn, Unavailable>);

	// the particle effects' mesh atoms: opaque ones with the meshes, translucent ones with the fading meshes
	CollectMeshAtoms();
	// the opaque ones drawn with DrawCutByPlane get their own ranges (cutAtomDrawDescs), the translucent ones stay sorted.
	// Once the renderer draws by path (particles::manager::k_DrawByPath) all of them go to particleAtomDrawDescs instead: a
	// Sorted one is its own Z object at its translation, opaque or cut alike, a Queued / Immediate one an item of its effect
	// (RenderContext::particleAtoms)
	std::unordered_map<entt::id_type, uint32_t> cutAtomIds;
	std::map<entt::id_type, uint32_t> particleAtomIds;
	for (const auto& atom : _particleMeshes)
	{
		if constexpr (openblack::particles::manager::k_DrawByPath)
		{
			++particleAtomIds[atom.meshId];
		}
		else if (atom.translucent)
		{
			++translucentIds[atom.meshId];
			translucentMorph.try_emplace(atom.meshId, false);
		}
		else if (AtomDrawnCut(atom))
		{
			++cutAtomIds[atom.meshId];
		}
		else
		{
			auto count = meshIds.insert(std::make_pair(atom.meshId, std::make_pair(0u, false)));
			count.first->second.first++;
		}
		++instanceCount;
	}

	if (drawBoundingBox)
	{
		instanceCount *= 2;
	}

	// Recreate instancing uniform buffer if it is too small
	if (_renderContext.instanceUniforms.size() < instanceCount)
	{
		// Grow with headroom: particles change the instance count every frame, and each resize reallocates.
		ResizeInstances(instanceCount + instanceCount / 2 + 256);
	}

	// Determine uniform buffer offsets and instance count for draw
	uint32_t offset = 0;
	_renderContext.instancedDrawDescs.clear();
	for (const auto& [meshId, desc] : meshIds)
	{
		_renderContext.instancedDrawDescs.emplace(std::piecewise_construct, std::forward_as_tuple(meshId),
		                                          std::forward_as_tuple(offset, desc.first, desc.second));
		offset += desc.first;
	}
	_renderContext.translucentDrawDescs.clear();
	_renderContext.additiveInstances.clear();
	for (const auto& [meshId, count] : translucentIds)
	{
		_renderContext.translucentDrawDescs.emplace(std::piecewise_construct, std::forward_as_tuple(meshId),
		                                            std::forward_as_tuple(offset, count, translucentMorph[meshId]));
		offset += count;
	}
	_renderContext.sortedOpaqueDrawDescs.clear();
	for (const auto& [meshId, desc] : sortedOpaqueIds)
	{
		_renderContext.sortedOpaqueDrawDescs.emplace(std::piecewise_construct, std::forward_as_tuple(meshId),
		                                             std::forward_as_tuple(offset, desc.first, desc.second));
		offset += desc.first;
	}
	_renderContext.cutAtomDrawDescs.clear();
	_renderContext.cutAtomInstances.clear();
	for (const auto& [meshId, count] : cutAtomIds)
	{
		_renderContext.cutAtomDrawDescs.emplace(std::piecewise_construct, std::forward_as_tuple(meshId),
		                                        std::forward_as_tuple(offset, count, false));
		offset += count;
	}
	// the PSys mesh atoms last, in a range of their own: their count changes every frame, and the entities' ranges
	// before them do not move with it (RefillKeepingDescs lays them out again)
	_entityRowsEnd = offset;
	LayOutAtoms(particleAtomIds);
}

void RenderingSystem::CollectMeshAtoms()
{
	// the particle effects' mesh atoms, then the objects' ghosts drawn the same way (ecs::object_ghosts), each a Z
	// object of its own; the ones whose mesh did not load are left out
	_particleMeshes = openblack::particles::mesh_atoms::Collect();
	auto ghosts = openblack::ecs::object_ghosts::Instances();
	_particleMeshes.insert(_particleMeshes.end(), std::make_move_iterator(ghosts.begin()),
	                       std::make_move_iterator(ghosts.end()));
	std::erase_if(_particleMeshes,
	              [](const auto& atom) { return !openblack::Locator::resources::value().GetMeshes().Contains(atom.meshId); });
}

void RenderingSystem::LayOutAtoms(const std::map<entt::id_type, uint32_t>& particleAtomIds)
{
	uint32_t offset = _entityRowsEnd;
	_renderContext.particleAtomDrawDescs.clear();
	for (const auto& [meshId, count] : particleAtomIds)
	{
		_renderContext.particleAtomDrawDescs.emplace(std::piecewise_construct, std::forward_as_tuple(meshId),
		                                             std::forward_as_tuple(offset, count, false));
		offset += count;
	}
}

/// One entity's instance row: the write walk's body, moved unchanged
void RenderingSystem::WriteEntityRow(const ecs::Registry& registry, entt::entity entity, const RowParts& parts,
                                     const Mesh& mesh, const Transform& transform, uint32_t idx, bool drawBoundingBox)
{
	const auto* alpha = parts.alpha;
	// the fire is asked once for the row's light and colours
	const auto* fire = ecs::fire::Find(entity);

	// villagers and animals are drawn where ECS/MobileDrawing puts them this frame (between turns, turning, on the slope)
	// a moving physics object at its pose between the last two turns (ECS/Physics), before all that;
	// with the slope shear, and a SuperVillager's own turn of its copy (ecs::DrawnBodyModel: ecs::DrawnModel, shared
	// with CarriedProps and the SuperVillagers)
	// T(p) R S with the position straight into the translation, as the original sets its matrices. It was
	// R T(p R) S, whose translation is R R^T p: a few ulp off p for a rotation, but far from it for the matrices that
	// are not one (the hand's bands while they fly, HandMagicFX SetTransform;
	// the props of villagers on a slope, CarriedProps; the map shield between two turns, DrawPhysical)
	// the drawing's own pose: smooth between turns (openblack's chosen difference, ECS/MobileDrawing.h)
	auto modelMatrix = openblack::ecs::DrawnBodyModel(registry, entity, openblack::ecs::PoseOf::Drawing);
	// a tree's roots (drawn by the tree's draw): the tree's drawn matrix copied, its nine rotation elements times
	// TreeRoots::factor = 0.15 x the mesh extent, the translation as it is (glm::scale on the right touches columns 0-2
	// only). They are only drawn from a live tree (also out of the map and in the physics): with the tree gone they
	// are not drawn until HandTrees' erase_if destroys them. A zero matrix: the row is still counted and written
	// (count == write), and its triangles collapse, so nothing is rasterised
	if (const auto* roots = registry.TryGet<const TreeRoots>(entity); roots != nullptr)
	{
		modelMatrix = openblack::ecs::IsAvailable(roots->tree) && registry.AllOf<Transform>(roots->tree)
		                  ? openblack::ecs::DrawnModel(registry, roots->tree, false) *
		                        glm::scale(glm::mat4(1.0f), glm::vec3(roots->factor))
		                  : glm::mat4(0.0f);
	}
	// the one-shot orb is drawn turned to the camera (Magic/Core/OneOffSpellSeed.cpp)
	if (const auto* orb = parts.orb; orb != nullptr)
	{
		// at its drawn place: in the hand, the hand's
		modelMatrix = glm::scale(glm::translate(openblack::ecs::DrawnPosition(registry, entity) + orb->facingOffset) *
		                             glm::mat4(orb->facing),
		                         transform.scale);
		if (alpha != nullptr)
		{
			_renderContext.sortPoints.insert_or_assign(idx, orb->sortPoint);
		}
	}

	// a thing a computer player's hand holds is drawn at its own point for it, turned and scaled as it is
	if (const auto held = std::ranges::find(_computerHeld, entity, &std::pair<entt::entity, glm::vec3>::first);
	    held != _computerHeld.end())
	{
		modelMatrix[3] = glm::vec4(held->second, 1.0f);
	}

	_renderContext.instanceUniforms[idx] = modelMatrix;
	// the row's land light goes in its own instance (below), so that two kinds drawn with one mesh (a dead or felled
	// tree, drawn with the bilinear light, on a living tree's mesh; a burning worship site beside another) each take
	// theirs. The mesh keeps one mode for the rows with none of their own and the readout: a special mode over the plain
	const auto light = LandLightOf(registry, entity, parts, fire);
	if (const auto [it, inserted] = _renderContext.meshLandLight.try_emplace(mesh.id, light);
	    !inserted && it->second.mode == openblack::land_light::ObjectMode::Bilinear && it->second.haze)
	{
		it->second = light;
	}
	// the entity's own model: the Mesh, which the pose, the pick, the shadow, the reflection and the culling read
	const auto* own = registry.TryGet<const Mesh>(entity);
	const RenderContext::EntityInstance instance {mesh.id, idx, parts.morphWithTerrain,
	                                              ReceivesDynamicShadow(registry, entity, mesh.id, parts),
	                                              own != nullptr ? own->id : mesh.id};
	if (_renderContext.rowOwnMesh.size() <= idx)
	{
		_renderContext.rowOwnMesh.resize(idx + 1, 0);
	}
	_renderContext.rowOwnMesh[idx] = instance.ownMeshId;
	if (_instanceNodes.empty())
	{
		_renderContext.entityInstances.insert_or_assign(entity, instance);
	}
	else
	{
		// an entry taken out at the start of the walk, given this row: the map places it as it places a new one
		auto node = std::move(_instanceNodes.back());
		_instanceNodes.pop_back();
		node.key() = entity;
		node.mapped() = instance;
		if (auto result = _renderContext.entityInstances.insert(std::move(node)); !result.inserted)
		{
			result.position->second = instance;
			_instanceNodes.push_back(std::move(result.node));
		}
	}
	if (alpha != nullptr)
	{
		_renderContext.instanceUniforms[idx][0][3] = 1.0f - glm::clamp(alpha->value, 0.0f, 1.0f);
	}
	// The w of the second column carries the texture offset (components::UvScroll): v + 4 x u in 1/256 steps
	if (const auto* scroll = registry.TryGet<const UvScroll>(entity); scroll != nullptr)
	{
		_renderContext.instanceUniforms[idx][1][3] = openblack::graphics::frame_anim::PackUvOffset(scroll->u, scroll->v);
	}
	// The object's colour fields (colour, specular, window) in the fifth column (argb_colour::PackInstance*):
	// the class's own pair (DrawColoursOf), then the fields' and trees' tints below
	auto colours = DrawColoursOf(registry, entity, parts, fire);
	// a citadel heart's draw: its land light times its tint, white with its alpha, and its own specular colour as the
	// last frame's draw left it, whatever burns on it (heart_fade)
	if (const auto* temple = parts.temple; temple != nullptr)
	{
		const auto specular = _heartSpecularsDrawn.find(entity);
		colours = {openblack::heart_fade::Tint(temple->fadeAlpha), std::nullopt,
		           specular != _heartSpecularsDrawn.end() ? specular->second : 0};
	}
	// a village totem's plinth and icon leave the land's light at their origin, hazed at its depth, for their
	// see-through copy, which is drawn with what they were left with the last frame they were drawn
	if (const auto source = TotemModelOf(registry, entity); source.has_value())
	{
		if (!registry.AllOf<SeeThrough>(entity))
		{
			const glm::vec3 origin(modelMatrix[3]);
			const auto sample = openblack::land_light::At(openblack::land_light::CurrentTable(), {origin.x, origin.z});
			const float depth =
			    Locator::camera::has_value()
			        ? graphics::haze::Depth(Locator::camera::value().GetViewMatrix(Camera::Interpolation::Current), origin)
			        : 0.0f;
			_totemLightsNow.insert_or_assign(
			    entity, graphics::haze::LitAndHazed(graphics::haze::Frame(), depth, sample.diffuse, sample.specular));
		}
		else if (const auto light = _totemLights.find(*source);
		         light != _totemLights.end() &&
		         (!Locator::config::has_value() || Locator::config::value().totemCopyTakesSolidLight))
		{
			colours = {std::nullopt, light->second.colour, light->second.specular};
		}
	}
	// a Norse gate's lamp: the gate's draw lights it at the land under the gate and hazes it at the gate's depth, then
	// hands both its lamps that colour and specular as they are, so the lamps take no light or haze of their own
	if (const auto gate = openblack::ecs::draw_gate::LampLightOrigin(registry, entity); gate.has_value())
	{
		const auto sample = openblack::land_light::At(openblack::land_light::CurrentTable(), {gate->x, gate->z});
		const float depth =
		    Locator::camera::has_value()
		        ? graphics::haze::Depth(Locator::camera::value().GetViewMatrix(Camera::Interpolation::Current), *gate)
		        : 0.0f;
		const auto lit = graphics::haze::LitAndHazed(graphics::haze::Frame(), depth, sample.diffuse, sample.specular);
		colours = {std::nullopt, lit.colour, lit.specular};
	}
	// a field's draw: the tint by growth (specular 0), and with a fire that colour x
	// the charring grey in the four channels (MultiplyArgbShift8) and the glow; and the ripe field's sway, a shear of its up
	// axis along world z (only the drawn matrix: the original restores it after queuing the draw)
	if (const auto* field = parts.field; field != nullptr)
	{
		const auto colour = ecs::FieldDrawColour(*field);
		// the blended colour's alpha 0xFF
		const uint32_t tint = argb_colour::Argb(colour.r, colour.g, colour.b, 0xFF);
		if (fire != nullptr)
		{
			colours = Burning(*fire);
			colours.tint = argb_colour::MultiplyArgbShift8(tint, *colours.tint);
		}
		else
		{
			colours = {tint, std::nullopt, 0};
		}
		if (field->growth >= Field::k_AgeRecolt)
		{
			// slot: bits 16-19 of the field's address in the original, any stable per-field number here
			const auto slot = (static_cast<uint32_t>(entt::to_integral(entity)) * 2654435761u) >> 28u;
			auto& instance = _renderContext.instanceUniforms[idx];
			instance = Locator::vegetation::value().GetFieldMatrix(instance, transform.scale.y, static_cast<uint8_t>(slot));
		}
	}
	// a tree's draw (with the sway tables made before it): the wind sway, the up axis's x = scale x 0 and z = scale x
	// the lean of the tree's slot, only the drawn matrix. A tree bent away from a passing object (worked out in
	// ecs::UpdateTreeBends) draws that bend instead of the sway: the drawn matrix turned about its base, the crown leaning
	// along the bend direction. Only the MATRIX: not for a tree the hand poses (the tug's matrix overwrites it after
	// the tree's draw; a held one is drawn out of the map, which never sets it) nor for one in the physics (out of the
	// map too). The colour below is every path's, in the map or out of it. A tree out of the physics is upright with
	// its yaw only, so its sway lands on a vertical up
	const auto* coloured = parts.tree;
	// asked only for a tree, the one reader below
	const bool drawnByHandOrPhysics = coloured != nullptr && registry.AnyOf<HandDrawPose, PhysicsDrawPose>(entity);
	if (coloured != nullptr)
	{
		// every RGB channel of the tree's colour times the frame's brightness / 256
		// (ecs::TreeBrightness, ScaleRgbShift8KeepAlpha), carried as a grey tint like the fields'. Asked at the first
		// tree, then kept for the PrepareDraw
		if (!_treeBrightness.has_value())
		{
			_treeBrightness = ecs::TreeBrightness();
		}
		const auto grey = static_cast<uint32_t>(*_treeBrightness);
		colours.tint = argb_colour::Argb(grey, grey, grey, 0xFF);
		colours.tintAfterHaze = true;
	}
	if (const auto* tree = coloured; tree != nullptr && !drawnByHandOrPhysics && tree->bendAngle != 0.0f)
	{
		const glm::vec3 away(tree->bendDirection.x, 0.0f, tree->bendDirection.y);
		// (inferred) the tree draw's turn is not checked against this +angle about up x away
		const auto bend =
		    glm::mat3(glm::rotate(glm::mat4(1.0f), tree->bendAngle, glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), away)));
		auto& instance = _renderContext.instanceUniforms[idx];
		for (int column = 0; column < 3; ++column)
		{
			const auto turned = bend * glm::vec3(instance[column]);
			instance[column] = glm::vec4(turned, instance[column][3]);
		}
	}
	else if (const auto* swayTree = coloured; swayTree != nullptr && !drawnByHandOrPhysics)
	{
		// the tree's own slot, round(yAngle x 16 / 2pi) & 15: trees facing the same way sway together
		const auto slot = swayTree->windSlot;
		_renderContext.instanceUniforms[idx][1][0] = 0.0f;
		_renderContext.instanceUniforms[idx][1][2] = transform.scale.y * Locator::vegetation::value().GetLean(slot);
	}
	// the fire part of a tree's draw (a tree with a FireEffect, ECS/Fire/FireGraphic): its colour x the burnt grey (the
	// tint, as the field's), and below 0.2 life it shrinks to 5 x life across (the matrix rows 0 and 2, not its
	// height). Only the tree's draws (in the map and out of it) do it: a burning DeadTree takes the shared fire draw
	// (DrawColoursOf)
	// (TreeDrawColour has a colour only for a tree with a fire)
	if (parts.tree != nullptr && fire != nullptr)
	{
		if (const auto colour = ecs::fire::graphic::TreeDrawColour(entity); colour.has_value())
		{
			colours.tint = argb_colour::Argb(colour->r, colour->g, colour->b, 0xFF);
			colours.tintAfterHaze = true;
			// and while it is drawn, the ALPHAREF of its alpha tested primitives is forced: the foliage thins out
			if (const auto ref = ecs::fire::graphic::TreeAlphaRef(entity); ref.has_value())
			{
				_renderContext.forcedAlphaRefs.insert_or_assign(idx, *ref);
			}
			if (const auto shrink = ecs::fire::graphic::BurntTreeWidth(ecs::life::LifeOf(entity)); shrink.has_value())
			{
				for (const int axis : {0, 2})
				{
					auto& column = _renderContext.instanceUniforms[idx][axis];
					column = glm::vec4(glm::vec3(column) * *shrink, column.w);
				}
			}
		}
	}
	auto& lh3d = _renderContext.instanceColours[idx];
	// off the map or in a missing block the light the row is drawn at is the full light, as the land light sampler
	// gives it (land_light::At)
	const glm::vec4 drawnAt = _renderContext.instanceUniforms[idx][3];
	const bool fullLight = _landBlocks.has_value() && land_light::FullLightAt(*_landBlocks, {drawnAt.x, drawnAt.z});
	argb_colour::PackInstanceLandLight(lh3d, land_light::RowLightCode(land_light::AtItsPlace(light, fullLight)));
	if (colours.tint.has_value() && colours.tintAfterHaze)
	{
		argb_colour::PackInstanceTreeTint(lh3d, *colours.tint);
	}
	else if (colours.tint.has_value())
	{
		argb_colour::PackInstanceTint(lh3d, *colours.tint);
	}
	else if (colours.colour.has_value())
	{
		argb_colour::PackInstanceColour(lh3d, *colours.colour);
	}
	argb_colour::PackInstanceSpecular(lh3d, colours.specular);
	// the trees, dead trees and big forests: their colour as it is, without the model light
	if (parts.unlit)
	{
		argb_colour::PackInstanceUnlit(lh3d);
	}
	// an abode's draw: the window colour of a house's lit windows at night, 0 otherwise; lit only with someone inside:
	// presentAtHome && it is night to the eye
	if (const auto* abode = parts.abode; abode != nullptr && _dayNightClock != nullptr)
	{
		argb_colour::PackInstanceWindow(
		    lh3d, night_lights::WindowColour(*_dayNightClock, transform.position, abode->presentAtHome != 0));
	}
	// a graveyard's graves: the parts of its model up to its graves stage, drawn over the rest (graphics::mesh_status)
	if (const auto* graveyard = registry.TryGet<const openblack::ecs::components::Graveyard>(entity);
	    graveyard != nullptr && graveyard->gravesStage != 0)
	{
		_renderContext.objectStatuses.insert_or_assign(idx, graveyard->gravesStage);
	}
	if (drawBoundingBox)
	{
		auto l3dMesh = Locator::resources::value().GetMeshes().Handle(mesh.id);
		auto box = l3dMesh->GetBoundingBox();
		auto boxMatrix = modelMatrix * glm::translate(box.Center()) * glm::scale(box.Size());
		_renderContext.instanceUniforms[idx + _renderContext.instanceUniforms.size() / 2] = boxMatrix;
	}
}

void RenderingSystem::PrepareDrawUploadUniforms(bool drawBoundingBox)
{
	WriteEntityRows(drawBoundingBox, false);
	WriteAtomRows();
	// Copied, not referenced: bgfx reads the memory a frame later, after a resize may have freed it.
	UploadInstances();
}

bool RenderingSystem::RefillKeepingDescs()
{
	if (!WriteEntityRows(false, true))
	{
		return false;
	}
	// the atoms of this frame, in their range after the entities' if the instances have room for them
	CollectMeshAtoms();
	if (_entityRowsEnd + _particleMeshes.size() > _renderContext.instanceUniforms.size())
	{
		return false;
	}
	std::map<entt::id_type, uint32_t> particleAtomIds;
	for (const auto& atom : _particleMeshes)
	{
		++particleAtomIds[atom.meshId];
	}
	LayOutAtoms(particleAtomIds);
	WriteAtomRows();
	UploadInstances();
	return true;
}

bool RenderingSystem::WriteEntityRows(bool drawBoundingBox, bool check)
{
	auto& registry = Locator::entitiesRegistry::value();
	// a refill (`check`) that finds an entity the draw lists have no room for
	bool fits = true;

	// the ranges of the draw lists, each with the rows this walk has written into it so far
	LayOutSlots();
	// the rows' classes the last walks found, unless an entity may have gained or lost one since
	if (_rowClassesStale)
	{
		_rowClasses.clear();
		_rowClassesStale = false;
	}
	// emptied as clear() does (the buckets stay), its entries kept to be filled again by this walk's rows
	while (!_renderContext.entityInstances.empty())
	{
		_instanceNodes.push_back(_renderContext.entityInstances.extract(_renderContext.entityInstances.begin()));
	}
	_renderContext.instancePoses.clear();
	_renderContext.sortPoints.clear();
	_renderContext.forcedAlphaRefs.clear();
	_renderContext.objectStatuses.clear();
	_renderContext.meshLandLight.clear();

	// Set transforms for instanced draw at offsets: each entity's row and its static shadow's, kept in entityRows
	_renderContext.entityRows.clear();
	_renderContext.sortedInstances.clear();
	const uint8_t level = DetailLevelNow();
	registry.Each<const Mesh, const Transform>(
	    [this, &registry, &fits, check, drawBoundingBox, level](entt::entity entity, const Mesh& intactMesh,
	                                                            const Transform& transform) {
		    if (check && !fits)
		    {
			    return;
		    }
		    // the row's place in the walk, by which the last walk's ranges and classes are kept
		    const auto row = static_cast<uint32_t>(_renderContext.entityRows.size());
		    const auto parts = RowPartsOf(registry, entity, ClassesOfRow(registry, row, entity));
		    // the drawn model, as the layout counted it (drawn_mesh::Of); the static shadow below keeps the whole one
		    const Mesh mesh = openblack::ecs::drawn_mesh::Of(parts.drawMesh, intactMesh, parts.detailMesh, level);
		    const auto* alpha = parts.alpha;
		    const bool needsSorting = parts.needsSorting;
		    const bool sortedOpaque = alpha == nullptr && needsSorting;
		    const auto range = alpha != nullptr ? RowRange::Translucent
		                       : sortedOpaque   ? RowRange::SortedOpaque
		                                        : RowRange::Instanced;
		    // the last walk's ranges for the row at this place in the walk: the same entity's while the walk is the same
		    const auto hint = row < _rowSlots.size() ? _rowSlots[row] : k_NoSlot;
		    const uint32_t slot = FindSlot(range, mesh.id, hint);
		    if (check && (slot == k_NoSlot || _instanceSlots[slot].filled >= _instanceSlots[slot].count))
		    {
			    fits = false;
			    return;
		    }
		    // (a rebuild's ranges were just made by the same walk, so it always finds one)
		    if (slot == k_NoSlot)
		    {
			    return;
		    }
		    auto& rows = _instanceSlots[slot];
		    const uint32_t idx = rows.offset + rows.filled;
		    if (needsSorting)
		    {
			    _renderContext.sortedInstances.insert(idx);
		    }
		    WriteEntityRow(registry, entity, parts, mesh, transform, idx, drawBoundingBox);
		    const auto* drawnWith = registry.TryGet<const DrawnWith>(entity);
		    _renderContext.entityRows.push_back({.entity = entity,
		                                         .index = idx,
		                                         .drawnBy = drawnWith != nullptr ? drawnWith->owner : entity,
		                                         .step = drawnWith != nullptr ? drawnWith->step : uint8_t {2}});
		    if (row < _rowSlots.size())
		    {
			    _rowSlots[row] = slot;
		    }
		    else
		    {
			    _rowSlots.push_back(slot);
		    }
		    rows.filled++;
	    },
	    entt::exclude<TempleInteriorPart, NotDrawn, Unavailable>);

	if (!check)
	{
		return true;
	}
	// every range filled to its count: the draw lists a rebuild would make now are these ones. (Every range has at least
	// one row, and a row found no range only with fits false, so this is "each mesh written as many times as its range
	// counts, and no other mesh")
	return fits && std::ranges::all_of(_instanceSlots, [](const InstanceSlots& rows) { return rows.filled == rows.count; }) &&
	       _renderContext.cutAtomDrawDescs.empty();
}

RowClasses RenderingSystem::ClassesOfRow(const ecs::Registry& registry, uint32_t row, entt::entity entity)
{
	if (row >= _rowClasses.size())
	{
		_rowClasses.resize(row + 1, {entt::null, {}});
	}
	auto& [rowEntity, classes] = _rowClasses[row];
	// (and found again after a layout mark during this walk)
	if (rowEntity != entity || _rowClassesStale)
	{
		rowEntity = entity;
		classes = RowClassesOf(registry, entity);
	}
	return classes;
}

void RenderingSystem::LayOutSlots()
{
	_instanceSlots.clear();
	const std::array<const std::map<entt::id_type, const RenderContext::InstancedDrawDesc>*, k_RowRanges> descs {
	    &_renderContext.instancedDrawDescs, &_renderContext.translucentDrawDescs, &_renderContext.sortedOpaqueDrawDescs};
	for (size_t range = 0; range < descs.size(); ++range)
	{
		_rangeBegin[range] = static_cast<uint32_t>(_instanceSlots.size());
		// in the map's order: by mesh id within each range
		for (const auto& [meshId, desc] : *descs[range])
		{
			_instanceSlots.push_back({.meshId = meshId, .offset = desc.offset, .count = desc.count, .filled = 0});
		}
	}
	_rangeBegin[k_RowRanges] = static_cast<uint32_t>(_instanceSlots.size());
}

uint32_t RenderingSystem::FindSlot(RowRange range, entt::id_type meshId, uint32_t hint) const
{
	const auto begin = _rangeBegin[static_cast<size_t>(range)];
	const auto end = _rangeBegin[static_cast<size_t>(range) + 1];
	if (hint >= begin && hint < end && _instanceSlots[hint].meshId == meshId)
	{
		return hint;
	}
	const auto first = _instanceSlots.begin() + begin;
	const auto last = _instanceSlots.begin() + end;
	const auto found = std::ranges::lower_bound(first, last, meshId, {}, &InstanceSlots::meshId);
	return found != last && found->meshId == meshId ? static_cast<uint32_t>(found - _instanceSlots.begin()) : k_NoSlot;
}

void RenderingSystem::WriteAtomRows()
{
	// the particle effects' mesh atoms, each in its range of particleAtomDrawDescs (the draw by path; the other ranges
	// below are the old draw's, which the renderer no longer has)
	static_assert(openblack::particles::manager::k_DrawByPath, "the PSys mesh atoms have a range of their own");
	_renderContext.additiveInstances.clear();
	_renderContext.cutAtomInstances.clear();
	std::map<entt::id_type, uint32_t> translucentOffsets;
	std::map<entt::id_type, uint32_t> uniformOffsets;
	std::map<entt::id_type, uint32_t> cutAtomOffsets;
	std::map<entt::id_type, uint32_t> particleAtomOffsets;
	_renderContext.particleAtoms.clear();
	_renderContext.particleAtomIndex.clear();
	constexpr bool byPath = openblack::particles::manager::k_DrawByPath;
	for (const auto& atom : _particleMeshes)
	{
		const bool cut = AtomDrawnCut(atom);
		auto& offsets =
		    byPath ? particleAtomOffsets : (atom.translucent ? translucentOffsets : (cut ? cutAtomOffsets : uniformOffsets));
		const auto& descs =
		    byPath ? _renderContext.particleAtomDrawDescs
		           : (atom.translucent ? _renderContext.translucentDrawDescs
		                               : (cut ? _renderContext.cutAtomDrawDescs : _renderContext.instancedDrawDescs));
		const auto desc = descs.find(atom.meshId);
		if (desc == descs.end())
		{
			continue;
		}
		auto offset = offsets.insert(std::make_pair(atom.meshId, 0));
		const uint32_t idx = desc->second.offset + offset.first->second;
		_renderContext.instanceUniforms[idx] = atom.model;
		// the colour's alpha: with the global alpha table for the translucent ones, else (no SetGlobalAlpha, Mesh.h) it is
		// still the diffuse alpha the blending primitives of the mesh take
		if (const auto fade = openblack::particles::mesh_atoms::Fade(atom); fade.has_value())
		{
			_renderContext.instanceUniforms[idx][0][3] = *fade;
		}
		if (atom.additive)
		{
			_renderContext.additiveInstances.insert(idx);
		}
		if (cut && (atom.translucent || byPath))
		{
			_renderContext.cutAtomInstances.insert(idx);
		}
		if (atom.atom != nullptr)
		{
			_renderContext.particleAtomIndex.insert_or_assign(atom.atom,
			                                                  static_cast<uint32_t>(_renderContext.particleAtoms.size()));
		}
		// the key of a Sorted atom's own Z object: its translation
		_renderContext.particleAtoms.push_back({idx, atom.meshId, atom.path, atom.effect, atom.atom, glm::vec3(atom.model[3]),
		                                        atom.translucent, atom.additive, cut, atom.mode, atom.forcedAlphaRef,
		                                        atom.depthEqual, atom.texture});
		// the atom's draw colour (the creator's colour, x the player's for UsePlayerColor): with DrawWithLandscapeColor
		// a tint by the land light, else a set colour. Its specular, read by both, is
		// particles::mesh_atoms::Instance::specular, packed below
		const uint32_t atomColour = argb_colour::Argb(atom.colour[0], atom.colour[1], atom.colour[2], 0xFF);
		auto& lh3d = _renderContext.instanceColours[idx];
		if (atom.landscapeColour)
		{
			argb_colour::PackInstanceTint(lh3d, atomColour);
		}
		else
		{
			argb_colour::PackInstanceColour(lh3d, atomColour);
		}
		// the atom's specular, into the object's specular field
		if ((atom.specular & 0x00FFFFFFu) != 0)
		{
			argb_colour::PackInstanceSpecular(lh3d, atom.specular);
		}
		if (atom.uv != glm::vec2(0.0f))
		{
			_renderContext.instanceUniforms[idx][1][3] =
			    openblack::graphics::frame_anim::PackUvOffset(atom.uv.x, atom.uv.y - std::floor(atom.uv.y));
		}
		// an animated atom's bones, drawn like a posed entity's
		if (!atom.pose.empty())
		{
			_renderContext.instancePoses.insert_or_assign(idx, atom.pose);
		}
		offset.first->second++;
	}
}
