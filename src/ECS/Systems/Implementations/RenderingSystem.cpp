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

#include <optional>
#include <unordered_set>

#include <entt/core/hashed_string.hpp>
#include <glm/gtx/transform.hpp>
#include <spdlog/spdlog.h>

#include "3D/DayNightClock.h"
#include "3D/FrameAnim.h"
#include "3D/L3DMesh.h"
#include "3D/NightLights.h"
#include "3D/ObjectMatrix.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Fields.h"
#include "ECS/Trees.h"
#include "ECS/Components/MeshTint.h"
#include "ECS/Components/ObjectColour.h"
#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandFxPart.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/MapShield.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpecularColour.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Tree.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Fire/FireGraphic.h"
#include "ECS/Life.h"
#include "ECS/PetitNavire.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "Graphics/DebugLines.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/Lh3dColour.h"
#include "Graphics/ShaderManager.h"
#include "Game.h"
#include "GameClock.h"
#include "Locator.h"
#include "PSys/Creators/Mesh.h"
#include "PSys/PSysManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

RenderingSystem::~RenderingSystem() = default;

namespace
{
/// The mesh atoms of the particle effects this frame (PSys/Creators/Mesh.h), drawn as instances of their mesh
std::vector<openblack::psys::mesh_atoms::Instance> g_PSysMeshes;

/// Whether a mesh atom is drawn with DrawCutByPlane: the particle's +0x24 & 4 (from the creator's +0x5F,
/// CreateParticle 0x6A8B94..0x6A8B9A), tested by fn_00679F20 (`test al, 4` 0x679F29) on both the immediate and the
/// sorted path (fn_00679F60's callback is fn_00679F20, `push 0x679F20` 0x679FBC), which then calls vt+0x11C (0x679F4A)
/// instead of the Draw vt+0x104 (0x679F52): psys::mesh_atoms::Instance::cutByPlane
[[nodiscard]] bool AtomCutByPlane(const openblack::psys::mesh_atoms::Instance& atom)
{
	return atom.cutByPlane;
}

/// A cut atom that also draws with the land colour (fn_0080BEC0 0x67A01C before vt+0x11C) would need the land light of
/// fn_0080BEC0 under the light of fn_00858BA0 (inferido, no effect known to set both): not ported, drawn as today
[[nodiscard]] bool AtomDrawnCut(const openblack::psys::mesh_atoms::Instance& atom)
{
	if (!AtomCutByPlane(atom))
	{
		return false;
	}
	if (atom.landscapeColour)
	{
		static bool warned = false;
		if (!warned)
		{
			warned = true;
			SPDLOG_LOGGER_WARN(spdlog::get("graphics"),
			                   "PSys mesh atom with DrawCutByPlane and DrawWithLandscapeColor: the cut is not ported, drawn uncut");
		}
		return false;
	}
	return true;
}

/// The original bakes a shadow for every Fixed and MobileObject (SetShadowOnTexture in Create3DObject 0x52DE30 /
/// 0x607210), trees and forests included, except the classes that turn it off (AnimatedStatic, DeadTree, Pot, fields,
/// ...); villagers and the creature have blob / dynamic shadows instead.
bool CastsStaticShadow(const openblack::ecs::Registry& registry, entt::entity entity)
{
	if (!registry.AnyOf<Fixed, MobileStatic, MobileObject, Tree, Abode, Feature, BigForest>(entity) ||
	    registry.AnyOf<Pot, AnimatedStatic, DeadTree, Field, Villager, Creature, Hand, Alpha, TempleInteriorPart>(entity))
	{
		return false;
	}
	// The baker (fn_008721A0) takes its casters from the map cells: an object in the hand (fn_005DC330) or in physics
	// (Object::InitialisePhysics*) has left them until it lands (EndPhysics), so it casts none meanwhile. (The original
	// re-bakes the blocks only for Fixed types; the old shadow of a tree or MobileObject lingers until something else
	// re-bakes that block. Not reproduced: openblack redraws the static shadows every frame.)
	if (openblack::Locator::handSystem::has_value())
	{
		const auto held = openblack::Locator::handSystem::value().GetHeldObject();
		if (held.has_value() && *held == entity)
		{
			return false;
		}
	}
	return !openblack::ecs::physics::PhysicsObjects::IsFlying(entity);
}
/// How a model's Draw takes the land light (land_light::ObjectMode): fn_00801C90 + fn_007FEB30 for most; Tree::Draw
/// fn_00802120 0x74AB1B then the haze 0x74AB60; WorshipSite::Draw 0x519460 and SpellIcon::Draw 0x5196CC
/// LH3DIsland::GetAltitudeAndSetColorSpecular 0x803340 without haze, except a burning WorshipSite (Object +0x44, the
/// FireEffect, 0x5193FF..0x51940A -> fn_00518050 -> fn_0080BEC0: fn_00801C90 + fn_007FEB30); Dove::Draw 0x41F75B
/// table[255] ([0xEDDD08]) without haze ((inferido) every species of the Dove class draws with it).
/// MultiMapFixed::DrawBuilding 0x517F90 (the partly built draw) is fn_00801C90 (0x517FB2) without fn_007FEB30 (to
/// 0x518046; with a fire only the tint fn_0080BF10, 0x517FD4): MultiMapFixed::Draw 0x518090 takes it while
/// IsDrawBuilding (vt +0x8A4, 0x5180A6), which for a Feature is Feature::IsDrawBuilding 0x527790 (the ArkDryDock while
/// not built, ecs/FeatureBuild.h). PetitNavire's hull (+0x28) gets fn_00801C90 in PreDraw (0x5E018D, 0x5E03DF) and
/// neither PreDraw 0x5DFF20 nor PostDraw 0x5E03F0 calls fn_007FEB30. Pending: the vt +0x890 == 0 branch of
/// WorshipSite / SpellIcon / Totem (0x5193E9, 0x519668, 0x51ABC3 -> DrawBuilding) and the building site (+0x74) of
/// MultiMapFixed::IsDrawBuilding 0x52F0C0 (openblack builds nothing on a site); the repair part of a damaged Abode
/// (Abode::Draw 0x516129 -> DrawBuilding, no haze) is merged into its FragMesh, whose pieces take the haze (fn_007F7ED0
/// 0x7F7F5F, 0x7F807D), so the whole keeps it (aproximado); the boat's sailors (0x5E073B) and deck objects (they copy
/// the hull's +0x4C, 0x5E099C..0x5E09A2) share their meshes with villagers and cows (one mode per mesh), so they keep
/// the haze (aproximado); Scaffold::Draw's phantom building (+0x74, fn_00802120 0x6EA6CA, no haze) is not drawn by
/// openblack; SpellIcon's +0x10C branch (0x519672, not read)
openblack::land_light::ObjectLight LandLightOf(const openblack::ecs::Registry& registry, entt::entity entity)
{
	using openblack::land_light::ObjectMode;
	if (registry.AllOf<Tree>(entity))
	{
		return {ObjectMode::CellShift, true};
	}
	if (const auto* feature = registry.TryGet<const Feature>(entity);
	    feature != nullptr && feature->type == openblack::FeatureInfo::ArkDryDock && feature->percentBuilt < 1.0f)
	{
		return {ObjectMode::Bilinear, false}; // DrawBuilding 0x517F90
	}
	if (entity == openblack::ecs::petit_navire::GetHull())
	{
		return {ObjectMode::Bilinear, false}; // PetitNavire::PreDraw 0x5E03DF
	}
	if (registry.AllOf<WorshipSite>(entity) && openblack::ecs::fire::Find(entity) == nullptr)
	{
		return {ObjectMode::Cell, false};
	}
	if (registry.AllOf<SpellIcon>(entity))
	{
		return {ObjectMode::Cell, false};
	}
	if (const auto* animal = registry.TryGet<const Animal>(entity);
	    animal != nullptr && openblack::ecs::animal_ai::IsFlyingSpecies(animal->type))
	{
		return {ObjectMode::Full, false};
	}
	return {};
}
/// The LH3DColor fields an object's Draw leaves in its LH3DObject this frame (lh3d_colour::PackInstance*): the tint t
/// of fn_0080BF10 (x the land light), or the colour of SetColorSpecular 0x7F9770 (instead of it), and the specular
struct DrawColours
{
	std::optional<uint32_t> tint;
	std::optional<uint32_t> colour;
	uint32_t specular {0};
	/// Tree::Draw's own product after the haze (0x74AB60 -> 0x74B077), lh3d_colour::PackInstanceTreeTint
	bool tintAfterHaze {false};
};
/// fn_00518050 (0x518050..0x518070), the Draw of an object with a fire (Object +0x44) of DeadTree 0x510833, FelledTree
/// 0x51199D, Rock 0x517F1C, MultiMapFixed 0x5180B6, SingleMapFixed 0x51810C, MobileObject 0x51815C, WorshipSite
/// 0x51940A, Totem 0x51AC79 / 0x51AD0E, Living fn_0051B3D0 0x51B40B and Animal 0x51C4CF (callers.py 0x518050), and
/// DrawBuilding 0x517FC1..0x517FD4 (the same pair to fn_0080BF10): the tint = the charring grey fn_00730570 (alpha 0xFF,
/// 0x7305DE), the specular = the glow GetFireEffectCharingColor 0x730480 (alpha 0xFF, 0x73055A)
DrawColours Burning(const openblack::ecs::fire::FireEffect& fire)
{
	namespace lh3d_colour = openblack::lh3d_colour;
	const uint32_t grey = openblack::ecs::fire::graphic::CharringGrey(fire);
	const auto glow = openblack::ecs::fire::graphic::CharringGlow(
	    fire, static_cast<float>(openblack::game_clock::Turn()) + openblack::game_clock::TurnFraction());
	return {lh3d_colour::Argb(grey, grey, grey, 0xFF), std::nullopt, lh3d_colour::Argb(glow.r, glow.g, glow.b, 0xFF)};
}
/// What each class's Draw passes to fn_0080BEC0 / fn_0080BF10 / SetColorSpecular, for the classes openblack draws
/// through the instances (the fields' and trees' own tints stay at their call sites below)
DrawColours DrawColoursOf(const openblack::ecs::Registry& registry, entt::entity entity)
{
	namespace lh3d_colour = openblack::lh3d_colour;
	using openblack::AnimalInfo;
	constexpr uint32_t k_White = 0xFFFFFFFFu; // `or edx, 0xffffffff`: the white tint, every channel - 1 (c 0xFF >> 8)
	const auto* fire = openblack::ecs::fire::Find(entity);
	const auto specularOf = [&registry, entity]() -> std::optional<uint32_t> {
		// Living +0xD0 (Living::GetSpecularColor 0x417490), tested as a whole dword with its alpha (0x51B416 / 0x51C4D6
		// `test eax,eax`). (aproximado) openblack has the component only while its rgb is not 0: the heal chakra writes
		// alpha 0xFF (fn_006A0E30 0x6A0EF5 before vt 0x5A0), so its fade frames with rgb 0 still take the white tint
		// in the original and the land light alone here (PSys/Rules/Heal.cpp drops the component at rgb 0)
		if (const auto* specular = registry.TryGet<const SpecularColour>(entity); specular != nullptr)
		{
			return lh3d_colour::Argb(specular->colour.r, specular->colour.g, specular->colour.b);
		}
		return std::nullopt;
	};
	// The power-up bands: DrawSpellGraphic through SetColorSpecular 0x7F9770 (vt 0x2C, 0x51A3BE); PHandFX Band::Draw
	// writes the same fields directly (+0x4C 0x68D87D / 0x68D8AB, +0x50 0x68D8B1). (inferido) no fn_00801C90 runs on
	// the hand band between those writes and its AddForDrawing, so both end as a set colour
	if (const auto* colour = registry.TryGet<const ObjectColour>(entity); colour != nullptr)
	{
		return {std::nullopt, lh3d_colour::Argb(colour->rgb[0], colour->rgb[1], colour->rgb[2]), colour->specular};
	}
	// TownCentre::Draw 0x5164A6..0x5164B2 (the icons in its slots, while the centre's life > 0): always the white tint
	// and the icon's +0x10C (SetSpecularColor 0x55D380; not ported: 0). (inferido) this is the write the frame keeps:
	// TownCentreSpellIcon::Draw 0x519640 is a jmp to SpellIcon::Draw, and whether it also runs for the slot icons in the
	// same frame (and after TownCentre::Draw) was not read
	if (registry.AllOf<SpellIcon, TownCentreSpellIcon>(entity))
	{
		return {k_White, std::nullopt, 0};
	}
	// SpellIcon::Draw 0x519650 (the WorshipSpellIcon's): the white tint 0x5198A8 only when +0x10C != 0
	// (0x519672..0x51967C); with +0x10C == 0 the light of 0x5196CC alone, and with vt 0x890 == 0 DrawBuilding
	// 0x519668. +0x10C is not ported (always 0), so never the tint
	if (registry.AllOf<SpellIcon>(entity))
	{
		return {};
	}
	// PhysicalShield::DrawShield 0x72D0D4: the white tint, specular 0 (0x72D0D2)
	if (const auto* shield = registry.TryGet<const MapShield>(entity);
	    shield != nullptr && shield->kind == MapShield::Kind::Physical)
	{
		return {k_White, std::nullopt, 0};
	}
	// OneOffSpellSeed::Draw 0x519002..0x51901E (DrawOutOfMap 0x518DDA..0x518DF6): ([0xBE8E8C] & 0xFF) << 24 | 0xFFFFFF,
	// specular 0. Its alpha is the caller's components::Alpha (see lh3d_colour::PackInstanceTint)
	if (registry.AllOf<OneOffSpellSeed>(entity))
	{
		return {k_White, std::nullopt, 0};
	}
	if (const auto* animal = registry.TryGet<const Animal>(entity); animal != nullptr)
	{
		// SpellWolf::Draw 0x51C6EC..0x51C7E1: +0x4C = (fistp alpha) << 24 | 0xFFFFFF (0x51C709..0x51C714; the alpha is
		// components::Alpha here), with a fire that times the charring grey in the four channels (0x51C751..0x51C7B7,
		// lh3d_colour::MulShr8_4) and the glow 0x51C74C, else that colour and Living +0xD0 (0x51C7D2..0x51C7E1)
		if (animal->type == AnimalInfo::SpellWolf)
		{
			if (fire != nullptr)
			{
				auto burning = Burning(*fire);
				burning.tint = lh3d_colour::MulShr8_4(k_White, *burning.tint);
				return burning;
			}
			return {k_White, std::nullopt, specularOf().value_or(0)};
		}
		// Animal::Draw 0x51C4C6..0x51C51C: the fire first, then +0xD0 != 0 with the white tint, else the land light alone
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
	if (registry.AllOf<Villager>(entity))
	{
		// fn_0051B3D0 (Living::Draw 0x51AEEC, Villager::Draw 0x51BA74 / 0x51BAD5): the fire (0x51B402), then +0xD0 != 0
		// with the white tint (0x51B416..0x51B429), then a poisoned one with 0x51BB50 / 0x51BB60 (0x51B43D..0x51B45D),
		// else the land light alone (0x51B45F..0x51B488)
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
	// Pot::Draw 0x51BB70 / DrawOutOfMap 0x51BBC0 / PileFood::Draw 0x51C191..0x51C1B8: a poisoned pot without a fire takes the
	// poison's pair (0x51BB8F..0x51BBA3), otherwise MobileObject::Draw 0x518150 (the fire's, below)
	if (const auto* pot = registry.TryGet<const Pot>(entity); pot != nullptr && pot->poisoned && fire == nullptr)
	{
		return {openblack::ecs::life::k_PoisonDiffuse, std::nullopt, openblack::ecs::life::k_PoisonSpecular};
	}
	// the other classes with a fire (Burning). (inferido) every class drawn here that can burn goes through one of those
	// Draws (fn_00518050, DrawBuilding) or carries the same pair inline (SpellWolf 0x51C751, above); the creature
	// (LH3DCreature) and the hand do not. Not ported: the damaged Abode's FragMesh (0x5160AF; 0xFFFFFFFF / 0 when not
	// burning, 0x5160CB..0x5160D8), Object::DrawOutOfMap's own pair (0x51C839), the PhysicsObject prediction object
	// (0x646F8C) and CitadelHeart::DrawNow (0x4670DD..0x4670EE: tint +0xA4, specular vt 0x5A4)
	if (fire != nullptr && !registry.AnyOf<Tree, Field, Creature, Hand>(entity))
	{
		return Burning(*fire);
	}
	return {};
}
/// A broken building keeps the static shadow of its intact mesh (the FragMesh casts none); fragments cast none either
/// (Fragment: SetShadowOnTexture(0)), which CastsStaticShadow already leaves out.
entt::id_type ShadowMeshOf(const openblack::ecs::Registry& registry, entt::entity entity, entt::id_type drawn)
{
	const auto* damage = registry.TryGet<const openblack::ecs::components::BuildingDamage>(entity);
	return damage != nullptr && damage->intactMesh != 0 ? damage->intactMesh : drawn;
}
/// The receivers of the projected shadows: the LH3DObject's +4 bit 0x40, read by vt+0x7C (fn_007F9870, +4 >> 6 & 1)
/// and set only by vt+0x78(1) (fn_008168A0: 0x40 when [0xC38220] != 0 and the argument != 0). A new LH3DObject has it
/// clear (the LH3DMeshedObject ctor puts +4 = 0x10009, 0x816537), so only what turns it on receives:
/// - Object::Create3DObject (0x6365F0, vt+0x78(1) at 0x63661E) for every game object; trees (0x749FA3), forests
///   (0x439098), flowers, magic food (0x5FAAC8), the food in the hand (pot info 12, 0x66D180) and a few others turn it
///   off again. So do the one-shot orb, the dispensers' bubble (OneOffSpellSeed::CallVirtualFunctionsForCreation
///   0x72A4B4), and the two shields (MagicShield 0x72C2B4, PhysicalShield 0x72CCF4): vt+0x78(0) on their LH3DObject
///   (obj+0x40), the same call as the trees' 0x749FA3;
/// - the spell seed graphic's mesh (fn_00727190, vt+0x78(1) at 0x727245);
/// and not the objects made with LH3DObject::Create alone, which never call it: the hand FX's power-up bands (Band,
/// fn_0068CA30 0x68CA98; components::HandFxPart) and the seed graphic's power-up band (CreatePUBand 0x727080 ->
/// Game3DObject::Create 0x63ABB0 = jmp LH3DObject::Create; the same Power_Up_Band mesh, which no game object uses), nor
/// the PSys mesh atoms (vt+0x78 with the creator's +0x54, 0x6A8ACE / 0x6A8D65, 0 from the ctors 0x6A8986 / 0x6A8BDE
/// and no property; not entities here anyway)
bool ReceivesDynamicShadow(const openblack::ecs::Registry& registry, entt::entity entity, entt::id_type meshId)
{
	static constexpr auto k_PowerUpBand = entt::hashed_string("Power_Up_Band");
	if (meshId == k_PowerUpBand.value() ||
	    registry.AnyOf<Tree, DeadTree, BigForest, Forest, Hand, TempleInteriorPart, OneOffSpellSeed, MapShield, HandFxPart>(
	        entity))
	{
		return false;
	}
	const auto* pot = registry.TryGet<const Pot>(entity);
	return pot == nullptr || (pot->type != openblack::PotInfo::HandFood && pot->type != openblack::PotInfo::MagicFood);
}
} // namespace


void RenderingSystem::PrepareDrawDescs(bool drawBoundingBox)
{
	auto& registry = Locator::entitiesRegistry::value();

	// Count number of instances
	uint32_t instanceCount = 0;
	std::unordered_map<entt::id_type, std::pair<uint32_t, bool>> meshIds;
	std::unordered_map<entt::id_type, uint32_t> translucentIds;
	// fading meshes that follow the land (fields, piles) keep doing it while they fade (per mesh: any of its entities)
	std::unordered_map<entt::id_type, bool> translucentMorph;

	auto prep = [&meshIds, &instanceCount](const Mesh& mesh, bool morphWithTerrain) {
		auto count = meshIds.insert(std::make_pair(mesh.id, std::make_pair(mesh.submeshId, morphWithTerrain)));
		count.first->second.first++;
		instanceCount++;
	};

	registry.Each<const Mesh, const Transform>([&prep](const Mesh& mesh, const Transform& /*unused*/) { prep(mesh, false); },
	                                           entt::exclude<MorphWithTerrain, TempleInteriorPart, Alpha>);
	registry.Each<const Mesh, const Transform, const MorphWithTerrain>(
	    [&prep](const Mesh& mesh, const Transform& /*unused*/, const MorphWithTerrain& /*unused*/) { prep(mesh, true); },
	    entt::exclude<Alpha>);
	registry.Each<const Mesh, const Transform, const Alpha>(
	    [&registry, &translucentIds, &translucentMorph, &instanceCount](entt::entity entity, const Mesh& mesh,
	                                                                    const Transform& /*unused*/, const Alpha& /*unused*/) {
		    ++translucentIds[mesh.id];
		    translucentMorph[mesh.id] = translucentMorph[mesh.id] || registry.AllOf<MorphWithTerrain>(entity);
		    ++instanceCount;
	    },
	    entt::exclude<TempleInteriorPart>);

	// ParticleMeshCreator atoms (Particle3DObj::DrawAt 0x679FD0): opaque ones with the meshes, translucent ones with the
	// fading meshes
	g_PSysMeshes = openblack::psys::mesh_atoms::Collect();
	std::erase_if(g_PSysMeshes, [](const auto& atom) {
		return !openblack::Locator::resources::value().GetMeshes().Contains(atom.meshId);
	});
	// the opaque ones drawn with DrawCutByPlane get their own ranges (cutAtomDrawDescs), the translucent ones stay sorted.
	// Once the renderer draws by path (psys::manager::k_DrawByPath) all of them go to psysAtomDrawDescs instead: a Sorted
	// one is its own Z object at its translation, opaque or cut alike (fn_00679F60 from 0x67A246), a Queued / Immediate
	// one an item of its effect (RenderContext::psysAtoms)
	std::unordered_map<entt::id_type, uint32_t> cutAtomIds;
	std::map<entt::id_type, uint32_t> psysAtomIds;
	for (const auto& atom : g_PSysMeshes)
	{
		if constexpr (openblack::psys::manager::k_DrawByPath)
		{
			++psysAtomIds[atom.meshId];
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

	std::unordered_map<entt::id_type, uint32_t> shadowCasterIds;
	registry.Each<const Mesh, const Transform>([&registry, &shadowCasterIds, &instanceCount](entt::entity entity, const Mesh& mesh,
	                                                                                        const Transform& /*unused*/) {
		if (CastsStaticShadow(registry, entity))
		{
			++shadowCasterIds[ShadowMeshOf(registry, entity, mesh.id)];
			++instanceCount;
		}
	});

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
	_renderContext.cutAtomDrawDescs.clear();
	_renderContext.cutAtomInstances.clear();
	for (const auto& [meshId, count] : cutAtomIds)
	{
		_renderContext.cutAtomDrawDescs.emplace(std::piecewise_construct, std::forward_as_tuple(meshId),
		                                        std::forward_as_tuple(offset, count, false));
		offset += count;
	}
	_renderContext.psysAtomDrawDescs.clear();
	for (const auto& [meshId, count] : psysAtomIds)
	{
		_renderContext.psysAtomDrawDescs.emplace(std::piecewise_construct, std::forward_as_tuple(meshId),
		                                         std::forward_as_tuple(offset, count, false));
		offset += count;
	}
	_renderContext.shadowCasterDrawDescs.clear();
	for (const auto& [meshId, count] : shadowCasterIds)
	{
		_renderContext.shadowCasterDrawDescs.emplace(std::piecewise_construct, std::forward_as_tuple(meshId),
		                                             std::forward_as_tuple(offset, count, false));
		offset += count;
	}
}

void RenderingSystem::PrepareDrawUploadUniforms(bool drawBoundingBox)
{
	auto& registry = Locator::entitiesRegistry::value();

	// Store offsets of uniforms for descs
	std::map<entt::id_type, uint32_t> uniformOffsets;
	std::map<entt::id_type, uint32_t> translucentOffsets;
	std::map<entt::id_type, uint32_t> shadowCasterOffsets;
	_renderContext.entityInstances.clear();
	_renderContext.instancePoses.clear();
	_renderContext.sortPoints.clear();
	_renderContext.meshLandLight.clear();

	// Set transforms for instanced draw at offsets
	registry.Each<const Mesh, const Transform>(
	    [this, &registry, &uniformOffsets, &translucentOffsets, &shadowCasterOffsets,
	     drawBoundingBox](entt::entity entity, const Mesh& mesh, const Transform& transform) {
		    const auto* alpha = registry.TryGet<const Alpha>(entity);
		    auto offset = (alpha != nullptr ? translucentOffsets : uniformOffsets).insert(std::make_pair(mesh.id, 0));
		    auto desc = (alpha != nullptr ? _renderContext.translucentDrawDescs : _renderContext.instancedDrawDescs).find(mesh.id);

		    // villagers and animals are drawn where ECS/MobileDrawing puts them this frame (between turns, turning, on the slope)
		    const auto* draw = registry.TryGet<const DrawPosition>(entity);
		    const auto& drawRotation = draw != nullptr ? draw->rotation : transform.rotation;
		    const auto& drawPosition = draw != nullptr ? draw->position : transform.position;
		    // T(p) R S with the position straight into the translation, as every Set* of the original (0x423195,
		    // 0x6382B7, 0x607606). It was R T(p R) S, whose translation is R R^T p: a few ulp off p for a rotation, but
		    // far from it for the matrices that are not one (the hand's bands while they fly, HandMagicFX SetTransform;
		    // the props of villagers on a slope, CarriedProps; the map shield between two turns, DrawPhysical)
		    auto modelMatrix = openblack::lh_matrix::Model(drawPosition, drawRotation, transform.scale);
		    // the one-shot orb is drawn turned to the camera (fn_00518720, Magic/Core/OneOffSpellSeed.cpp)
		    if (const auto* orb = registry.TryGet<const OneOffSpellSeed>(entity); orb != nullptr)
		    {
			    modelMatrix = glm::scale(glm::translate(transform.position + orb->facingOffset) * glm::mat4(orb->facing),
			                             transform.scale);
			    if (alpha != nullptr)
			    {
				    _renderContext.sortPoints.insert_or_assign(desc->second.offset + offset.first->second, orb->sortPoint);
			    }
		    }
		    else if (draw != nullptr)
		    {
			    modelMatrix[0] += draw->shearX * modelMatrix[1];
			    modelMatrix[2] += draw->shearZ * modelMatrix[1];
		    }

		    const uint32_t idx = desc->second.offset + offset.first->second;
		    _renderContext.instanceUniforms[idx] = modelMatrix;
		    // (aproximado) one land light mode per mesh (the uniform of its draw), not per instance: a mesh drawn by
		    // two kinds (a dead or felled tree, DeadTree::Draw 0x51084C / FelledTree::Draw 0x5119B3: fn_00801C90, on
		    // a living tree's mesh; a burning worship site beside another) takes the other mode than the plain one,
		    // logged once
		    const auto light = LandLightOf(registry, entity);
		    if (const auto [it, inserted] = _renderContext.meshLandLight.try_emplace(mesh.id, light);
		        !inserted && (it->second.mode != light.mode || it->second.haze != light.haze))
		    {
			    if (it->second.mode == openblack::land_light::ObjectMode::Bilinear && it->second.haze)
			    {
				    it->second = light;
			    }
			    static std::unordered_set<entt::id_type> s_Logged;
			    if (s_Logged.insert(mesh.id).second)
			    {
				    SPDLOG_LOGGER_DEBUG(spdlog::get("graphics"), "Mesh {}: instances with two land light modes",
				                        mesh.id);
			    }
		    }
		    _renderContext.entityInstances.insert_or_assign(
		        entity, RenderContext::EntityInstance {mesh.id, idx, registry.AllOf<MorphWithTerrain>(entity),
		                                               ReceivesDynamicShadow(registry, entity, mesh.id)});
		    if (CastsStaticShadow(registry, entity))
		    {
			    const auto casterMesh = ShadowMeshOf(registry, entity, mesh.id);
			    auto casterOffset = shadowCasterOffsets.insert(std::make_pair(casterMesh, 0));
			    const auto casterDesc = _renderContext.shadowCasterDrawDescs.find(casterMesh);
			    if (casterDesc != _renderContext.shadowCasterDrawDescs.end())
			    {
				    _renderContext.instanceUniforms[casterDesc->second.offset + casterOffset.first->second] = modelMatrix;
				    casterOffset.first->second++;
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
		    // The object's LH3DColor fields (obj+0x4C / +0x50 / +0x54) in the fifth column (lh3d_colour::PackInstance*):
		    // the class's own pair (DrawColoursOf), then the fields' and trees' tints below
		    auto colours = DrawColoursOf(registry, entity);
		    // The w of the third column: components::MeshTint, 1e6 (2e6 dissolving) + 5 bits each of the ground colour
		    // (r, g, b from the bottom) and of `own` (bits 15-19)
		    if (const auto* tint = registry.TryGet<const MeshTint>(entity); tint != nullptr)
		    {
			    const auto bits = [](float value) {
				    return static_cast<uint32_t>(std::clamp(value * 31.0f + 0.5f, 0.0f, 31.0f));
			    };
			    const auto packed = bits(tint->own) * 32768u + bits(tint->ground.r) * 1024u + bits(tint->ground.g) * 32u +
			                        bits(tint->ground.b);
			    _renderContext.instanceUniforms[idx][2][3] = (tint->dissolve ? 2e6f : 1e6f) + static_cast<float>(packed);
		    }
		    // Field::Draw 0x528570 (without the world.foliage tint): the tint by growth (0x528881..0x52888A, specular 0),
		    // and with a fire that colour x the charring grey in the four channels (0x528809..0x528862, MulShr8_4) and the
		    // glow (0x5287FB); and the ripe field's sway, a shear of its up axis along world z (only the drawn matrix:
		    // the original restores it after AddForDrawing)
		    if (const auto* field = registry.TryGet<const Field>(entity);
		        field != nullptr && !registry.AllOf<MeshTint>(entity))
		    {
			    const auto colour = ecs::FieldDrawColour(*field);
			    // BlendColor's alpha 0xFF (0x528510)
			    const uint32_t tint = lh3d_colour::Argb(colour.r, colour.g, colour.b, 0xFF);
			    if (const auto* fire = ecs::fire::Find(entity); fire != nullptr)
			    {
				    colours = Burning(*fire);
				    colours.tint = lh3d_colour::MulShr8_4(tint, *colours.tint);
			    }
			    else
			    {
				    colours = {tint, std::nullopt, 0};
			    }
			    if (field->growth >= Field::k_AgeRecolt)
			    {
				    // slot: bits 16-19 of the field's address in the original, any stable per-field number here
				    const auto slot = (static_cast<uint32_t>(entt::to_integral(entity)) * 2654435761u) >> 28u;
				    _renderContext.instanceUniforms[idx][1][0] = 0.0f;
				    _renderContext.instanceUniforms[idx][1][2] = transform.scale.y * 1.75f * ecs::WindSway(slot);
			    }
		    }
		    // Tree::Draw 0x74B016 (the tables of Tree::PreDraw 0x74A7C0): the wind sway, the up axis's x = scale x 0 and
		    // z = scale x the lean of the tree's slot (bits 2-5 of +0x5C), only the drawn matrix. Not while the tree is
		    // tilted (pulled or held by the hand). A tree bent away from a passing object (bits 6-9 of +0x5C, table
		    // 0xD19A48, worked out in ecs::UpdateTrees) draws that bend instead of the sway: the drawn matrix turned about
		    // its base, the crown leaning along the bend direction.
		    if (const auto* tree = registry.TryGet<const Tree>(entity);
		        tree != nullptr && tree->bendAngle != 0.0f && transform.rotation[1].x == 0.0f &&
		        transform.rotation[1].z == 0.0f)
		    {
			    const glm::vec3 away(tree->bendDirection.x, 0.0f, tree->bendDirection.y);
			    // (inferido) the turn of Tree::Draw 0x74B016 is not checked against this +angle about up x away
			    const auto bend =
			        glm::mat3(glm::rotate(glm::mat4(1.0f), tree->bendAngle, glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), away)));
			    auto& instance = _renderContext.instanceUniforms[idx];
			    for (int column = 0; column < 3; ++column)
			    {
				    const auto turned = bend * glm::vec3(instance[column]);
				    instance[column] = glm::vec4(turned, instance[column][3]);
			    }
			    if (!registry.AllOf<MeshTint>(entity))
			    {
				    const auto grey = static_cast<uint32_t>(ecs::TreeBrightness());
				    colours.tint = lh3d_colour::Argb(grey, grey, grey, 0xFF);
				    colours.tintAfterHaze = true;
			    }
		    }
		    else if (const auto* swayTree = registry.TryGet<const Tree>(entity);
		        swayTree != nullptr && transform.rotation[1].x == 0.0f && transform.rotation[1].z == 0.0f)
		    {
			    // the tree's own slot, round(yAngle x 16 / 2pi) & 15 (0x74A0E7): trees facing the same way sway together
			    const auto slot = static_cast<uint32_t>(swayTree->windSlot);
			    // Tree::Draw 0x74B077: every RGB channel of the tree's colour times the frame's brightness / 256
			    // (ecs::TreeBrightness, ScaleShr8_3KeepA), carried as a grey tint like the fields'
			    if (!registry.AllOf<MeshTint>(entity))
			    {
				    const auto grey = static_cast<uint32_t>(ecs::TreeBrightness());
				    colours.tint = lh3d_colour::Argb(grey, grey, grey, 0xFF);
				    colours.tintAfterHaze = true;
			    }
			    _renderContext.instanceUniforms[idx][1][0] = 0.0f;
			    _renderContext.instanceUniforms[idx][1][2] = transform.scale.y * ecs::WindSway(slot);
		    }
		    // Tree::Draw's fire part fn_0074B3A0 (a tree with a FireEffect, ECS/Fire/FireGraphic): its colour x the burnt
		    // grey (the tint, as the field's), and below 0.2 life it shrinks to 5 x life across (the matrix rows 0 and 2,
		    // not its height). Only Tree::Draw 0x74B070 and Tree::DrawOutOfMap 0x74B320 call it (callers.py 0x74B3A0): a
		    // burning DeadTree takes fn_00518050 (0x510833, DrawColoursOf)
		    if (registry.AllOf<Tree>(entity))
		    {
			    if (const auto colour = ecs::fire::graphic::TreeDrawColour(entity); colour.has_value())
			    {
				    colours.tint = lh3d_colour::Argb(colour->r, colour->g, colour->b, 0xFF);
				    colours.tintAfterHaze = true;
				    const float life = ecs::life::LifeOf(entity);
				    if (life < 0.2f)
				    {
					    const float shrink = 1.0f - (0.2f - life) * 5.0f;
					    for (const int axis : {0, 2})
					    {
						    auto& column = _renderContext.instanceUniforms[idx][axis];
						    column = glm::vec4(glm::vec3(column) * shrink, column.w);
					    }
				    }
			    }
		    }
		    auto& lh3d = _renderContext.instanceColours[idx];
		    if (colours.tint.has_value() && colours.tintAfterHaze)
		    {
			    lh3d_colour::PackInstanceTreeTint(lh3d, *colours.tint);
		    }
		    else if (colours.tint.has_value())
		    {
			    lh3d_colour::PackInstanceTint(lh3d, *colours.tint);
		    }
		    else if (colours.colour.has_value())
		    {
			    lh3d_colour::PackInstanceColour(lh3d, *colours.colour);
		    }
		    lh3d_colour::PackInstanceSpecular(lh3d, colours.specular);
		    // Abode::Draw 0x515F70: the window colour (+0x54, vt 0x30) of a house's lit windows at night, 0 otherwise
		    if (const auto* abode = registry.TryGet<const Abode>(entity); abode != nullptr && Game::Instance() != nullptr)
		    {
			    lh3d_colour::PackInstanceWindow(lh3d, night_lights::WindowColour(Game::Instance()->GetDayNightClock(),
			                                                                     transform.position, !abode->inhabitants.empty()));
		    }
		    if (drawBoundingBox)
		    {
			    auto l3dMesh = entt::locator<resources::ResourcesInterface>::value().GetMeshes().Handle(mesh.id);
			    auto box = l3dMesh->GetBoundingBox();
			    auto boxMatrix = modelMatrix * glm::translate(box.Center()) * glm::scale(box.Size());
			    _renderContext.instanceUniforms[idx + _renderContext.instanceUniforms.size() / 2] = boxMatrix;
		    }
		    offset.first->second++;
	    },
	    entt::exclude<TempleInteriorPart>);

	// the particle effects' mesh atoms, after the entities of the same mesh
	std::map<entt::id_type, uint32_t> cutAtomOffsets;
	std::map<entt::id_type, uint32_t> psysAtomOffsets;
	_renderContext.psysAtoms.clear();
	_renderContext.psysAtomIndex.clear();
	constexpr bool byPath = openblack::psys::manager::k_DrawByPath;
	for (const auto& atom : g_PSysMeshes)
	{
		const bool cut = AtomDrawnCut(atom);
		auto& offsets = byPath ? psysAtomOffsets
		                       : (atom.translucent ? translucentOffsets : (cut ? cutAtomOffsets : uniformOffsets));
		const auto& descs = byPath ? _renderContext.psysAtomDrawDescs
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
		// still the diffuse alpha the blending primitives of the mesh take (milagros2 rayo3)
		if (atom.translucent || atom.alpha < 1.0f)
		{
			_renderContext.instanceUniforms[idx][0][3] = 1.0f - atom.alpha;
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
			_renderContext.psysAtomIndex.insert_or_assign(atom.atom, static_cast<uint32_t>(_renderContext.psysAtoms.size()));
		}
		// fn_00679F60 0x679F7E..0x679F9F: the key of a Sorted atom's own Z object, the object's +0x38, its translation
		_renderContext.psysAtoms.push_back({idx, atom.meshId, atom.path, atom.effect, atom.atom, glm::vec3(atom.model[3]),
		                                    atom.translucent, atom.additive, cut});
		// the DrawData colour +8 (the creator's colour, x the player's for UsePlayerColor; Particle3DObj::DrawAt 0x67A00C..
		// 0x67A01C): with DrawWithLandscapeColor the tint of fn_0080BEC0, else the colour of SetColorSpecular (vt 0x2C).
		// the specular DrawData +0xC (read at 0x67A012 for fn_0080BEC0 and at 0x67A023 for vt 0x2C) is
		// psys::mesh_atoms::Instance::specular, packed below
		const uint32_t atomColour = lh3d_colour::Argb(atom.colour[0], atom.colour[1], atom.colour[2], 0xFF);
		auto& lh3d = _renderContext.instanceColours[idx];
		if (atom.landscapeColour)
		{
			lh3d_colour::PackInstanceTint(lh3d, atomColour);
		}
		else
		{
			lh3d_colour::PackInstanceColour(lh3d, atomColour);
		}
		// the DrawData specular +0xC (SetColorSpecular vt 0x2C, 0x67A023; fn_0080BEC0, 0x67A012): obj +0x50
		if ((atom.specular & 0x00FFFFFFu) != 0)
		{
			lh3d_colour::PackInstanceSpecular(lh3d, atom.specular);
		}
		if (atom.uv != glm::vec2(0.0f))
		{
			_renderContext.instanceUniforms[idx][1][3] = openblack::graphics::frame_anim::PackUvOffset(atom.uv.x, atom.uv.y - std::floor(atom.uv.y));
		}
		// a ParticleAnimCreator atom's bones (Particle3DAnim::DrawAt 0x67A8E0), drawn like a posed entity's
		if (!atom.pose.empty())
		{
			_renderContext.instancePoses.insert_or_assign(idx, atom.pose);
		}
		offset.first->second++;
	}

	// Copied, not referenced: bgfx reads the memory a frame later, after a resize may have freed it.
	UploadInstances();
}
