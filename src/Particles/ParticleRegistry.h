/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

#include "ParticleEffect.h"

// Class name -> factory for the spell files' modifier (rule, emitter) and creator classes. The original resolves the
// names with its persistence registry when a file is loaded; here every topic file registers its own
// classes from one explicit list, RegisterAll() in ParticleRegistry.cpp (no static initialisers). A class nobody registered
// is "not ported yet": it is attached with no effect, so the group logic stays the same.

namespace openblack::particles
{
using ModifierFactory = std::unique_ptr<Modifier> (*)(const Object& object);
using CreatorFactory = std::unique_ptr<Creator> (*)(const Object& object);

/// A later registration of the same name replaces the earlier one (and logs it)
void RegisterModifier(std::string_view className, ModifierFactory factory);
/// For Particle*Creator classes that need more than the common properties; the others use the plain Creator
void RegisterCreator(std::string_view className, CreatorFactory factory);

/// An EventCondition class (true for an atom or for a collection): the raw answer, before
/// the object's InvertResponse. `atom` is null when it is asked for a collection.
using ConditionTest = bool (*)(const Effect& effect, const Object& object, const Atom* atom, const Collection& collection);
void RegisterCondition(std::string_view className, ConditionTest test);
[[nodiscard]] ConditionTest FindCondition(std::string_view className);

/// Every registered rule, creator and condition class (in the psys state)
struct FactoryRegistry
{
	std::map<std::string, ModifierFactory, std::less<>> modifiers;
	std::map<std::string, CreatorFactory, std::less<>> creators;
	std::map<std::string, ConditionTest, std::less<>> conditions;
};

/// nullptr when the class is not registered
[[nodiscard]] ModifierFactory FindModifierFactory(std::string_view className);
[[nodiscard]] CreatorFactory FindCreatorFactory(std::string_view className);

/// The common ParticleCreator properties and the sprite ones (ParticleSpriteCreator). A
/// registered creator calls it first on its own Creator subclass.
void ReadCreatorProperties(const Object& object, Creator& creator);

/// The base name of a TextureFileName property (".\Data\Textures\S_Lightning.raw" -> "S_lightning") with the case the
/// file really has in Data\Textures: "raw/<that>" is the id Game.cpp loaded it under, and the spell files do not always
/// spell it the same way (S_Lightning.raw for the file S_lightning.raw).
[[nodiscard]] std::string TextureBaseName(std::string path);
/// The lower-case stem of every .raw in Data\Textures -> its real stem
using TextureStemMap = std::map<std::string, std::string, std::less<>>;
/// The texture stems, read once into the resource caches; empty without a file system
[[nodiscard]] const TextureStemMap& TextureStems();
/// The texture stems listed afresh (TextureStemsLoader's read)
[[nodiscard]] std::shared_ptr<TextureStemMap> ReadTextureStems();

/// The usual factory: T(object), or T() for the classes without properties
template <class T>
std::unique_ptr<Modifier> MakeModifierOf(const Object& object)
{
	if constexpr (std::is_constructible_v<T, const Object&>)
	{
		return std::make_unique<T>(object);
	}
	else
	{
		return std::make_unique<T>();
	}
}

// The registration lists, one per topic file (called once, in this order, by ParticleRegistry.cpp)
void RegisterCoreModifiers();   ///< ParticleEffect.cpp: the create rules, emitters and rules ported before the miracles
void RegisterSoundRules();      ///< Rules/Sound.cpp: StartStopSoundOnCondition, AddSoundToAtom, RemoveSoundFromAtom
void RegisterFireballRules();   ///< Rules/Fireball.cpp: the throw, bounce, MagicFireBall, deflection, spin, trail
void RegisterSprinkleRules();   ///< Rules/Sprinkle.cpp: HandSprinkle, AppearanceRuleTumble
void RegisterMeshCreators();    ///< Creators/Mesh.cpp: ParticleMeshCreator, ParticleMeshCreatorAnimTextured
void RegisterHandFollowRules(); ///< Rules/HandFollow.cpp: FollowLocalHand, FollowCastPosn
void RegisterGestureRules();    ///< Rules/Gesture.cpp: GesturingRecognised, ChainGesture, CreateRuleMakeChain
void RegisterLightningRules();  ///< Rules/Lightning.cpp: Lightning, LightningStrike
void RegisterChainCreator();    ///< Creators/Chain.cpp: ParticleChainCreator
void RegisterLightMapCreator(); ///< Creators/LightMap.cpp: ParticleLightMapCreator
void RegisterHealRules();       ///< Rules/Heal.cpp: HealSpellChakra, the fused explode, HealInHand
void RegisterShieldRules();     ///< Rules/Shield.cpp: the defensive spheres, shield sparks, spin, vapour, EP atoms
void RegisterSurfRevolRules();  ///< Rules/SurfRevol.cpp: SurfRevol, the teleport pool and dispenser discs
void RegisterExplosionRules();  ///< Rules/Explosion.cpp: Explosion, SetPSysCloseDown, MoveAtom, ChangeScaleXYZ
void RegisterKeyPointRules();   ///< Rules/KeyPoints.cpp: KPStretchHeight, KPMoveAtoms (heal PU mushroom)
void RegisterOrientRules();     ///< Rules/Orient.cpp: OrientSpriteWithVelocity (fireball in hand)
void RegisterForestRules();     ///< Rules/Forest.cpp: ForestPath, ParticleGoodEvilCreator (forest butterflies)
void RegisterMistCreator();     ///< Creators/Mist.cpp: ParticleMistCreator (the water; the storm uses it too)
void RegisterFlockRules();      ///< Rules/Flock.cpp: FollowTargets, EventConditionAtomNearVillagers
void RegisterStormRules();      ///< Rules/Storm.cpp: CloudMoverNew, CloudGather, Tornado, StormCast
void RegisterBeamRules();       ///< ParticleBeamRules.cpp: SimpleBeam
void RegisterArcRules();        ///< ParticleArcRules.cpp: ObjectArcer
void RegisterGlintRules();      ///< ParticleGlintRules.cpp: GlintsOnTarget
void RegisterObjectRules();     ///< ParticleObjectRules.cpp: CreateRule_GameObjectRef, EmitFromParentAtom
/// ParticleCreatureSpellRules.cpp: CreatureSpell, CreatureSpellGeneric, CreatureSpellItch
void RegisterCreatureSpellRules();
void RegisterBeliefRules();   ///< Rules/Belief.cpp: BeliefSprite
void RegisterManaPathRules(); ///< Rules/ManaPath.cpp: ManaPathNew
/// TownBelief.cpp: TownCentreBelief, CollectionShouldBeEmitting
void RegisterTownBeliefRules();
void RegisterPlayerSymbolCreator(); ///< PlayerSymbol.cpp: ParticleSymbolSpriteCreator
/// Rules/CameraScale.cpp: ScaleByCameraDist (no file the game loads uses it)
void RegisterCameraScaleRules();
/// Rules/StormFlash.cpp: EmitterRuleLightningSprite (the storm in the hand's lightning sprites and flashes)
void RegisterStormFlashRules();
/// Rules/VortexAttract.cpp: VortexAttract (what a way out of a land draws in)
void RegisterVortexAttractRules();
/// ParticleVolumeGlowRules.cpp: VolumeGlowOnObject (the glowing shell over a worthy artefact)
void RegisterVolumeGlowRules();
/// Rules/LightSheetOnObject.cpp: LightSheetOnObject (the ring of light round the singing stones' healing song)
void RegisterLightSheetRules();
} // namespace openblack::particles
