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

#include <array>
#include <optional>
#include <vector>

#include <GameTailRecords.h>
#include <ParticleRecords.h>
#include <RuleDataRecords.h>
#include <WorldListRecords.h>

#include "Audio/Services/SpellSounds.h"
#include "Particles/BeamData.h"
#include "Particles/CreatureSpellData.h"
#include "Particles/CreatureSpellWispData.h"
#include "Particles/ParticleEffect.h"
#include "Particles/PlasmaData.h"
#include "Particles/Rules/Belief.h"
#include "Particles/Rules/Explosion.h"
#include "Particles/Rules/Fireball.h"
#include "Particles/Rules/Flock.h"
#include "Particles/Rules/Lightning.h"
#include "Particles/Rules/ManaPath.h"
#include "Particles/Rules/Orient.h"
#include "Particles/Rules/Sound.h"
#include "Particles/Rules/Storm.h"
#include "Particles/Rules/StormFlash.h"
#include "Particles/Rules/VortexAttract.h"
#include "Particles/SoundAction.h"
#include "Particles/SphereAnglesData.h"
#include "Particles/VolumeGlowData.h"

// The values of a particle effect's atoms and collections as a saved game's world file holds them, from the particle
// engine's (docs/wiki/engine/saved-games.md#openblack, the particles). Pure: the references between the records are
// the writer's (WriteParticles.cpp), these are every other value.
namespace openblack::ecs::saved_world_write::particle_codecs
{

/// The bytes the original leaves unset in an atom and a collection: its flags' bits it never writes, the draw infos'
/// last three bytes and the animation info's last two, which its heap and stack hold as this (all 19 world files read)
constexpr uint8_t k_Unset = 0xCD;
/// A collection's word for a group whose atoms pass their frame down (the file's Hierarchies flag): the saves hold
/// this for every such collection and 0 for the others (516 collections of the 19 world files read)
constexpr uint32_t k_PassesFrame = 0xFFFFFFFE;

/// A drawn matrix as the record lays it out: the three rows and the translation, the scale, the alpha byte, three
/// unset bytes
[[nodiscard]] std::array<uint8_t, 56> DrawInfoBytes(const particles::saved_draw::SavedDraw& draw);

/// An atom's animation: its frame rate, its creator's frame count, whether it plays and whether it loops (the
/// creator's), two unset bytes; as the original's creators give it (a sprite's, an animated textured mesh's and a
/// light map's all of it, a mist's its looping alone, the others none: rate 1, one frame, not looping)
[[nodiscard]] std::array<uint8_t, 12> AnimInfoBytes(const particles::Atom& atom);

/// An atom's flags word: drawn on its first update (0x01), stepped once (0x02) or twice (0x04), deflected (0x08), in
/// use (0x10); the other bits unset
[[nodiscard]] uint16_t AtomFlags(const particles::Atom& atom);

/// An atom's values: its velocity, birth, turn (with no translation), base scale, scale, stretch, place, colour,
/// specular, flags, its two drawn matrices (the last step's, then this step's), its last and current frames, its
/// animation, its gravity and its random byte. The references (its sub-collections, its rules' data, its sounds, its
/// collection, its draw offset and its particle) are left as none
[[nodiscard]] gamesave::AtomCoreRecord AtomCoreValues(const particles::Atom& atom);

/// A collection's flags byte: in the effect's own coordinates (0x01, cleared under an ancestor that passes its frame
/// down), drawn interpolated (0x02); the other bits unset
[[nodiscard]] uint8_t CollectionFlags(const particles::Collection& collection);

/// A collection's values: its rules by their places in the effect's file (of that particle type), those still on it,
/// its birth, group, flags, whether its atoms pass their frame down and its alpha. The references (its rules' data,
/// its manager, its parent atom, its atoms and its chain) are left as none
[[nodiscard]] gamesave::AtomCollectionRecord CollectionValues(const particles::Collection& collection, uint32_t fileType,
                                                              bool passesFrame);

/// An emitter's data on a collection (its rule's slot): the rule by its place in the effect's file of that particle
/// type, the next emission time, the count emitted and whether its first update is still to come
[[nodiscard]] gamesave::EmitterCollectionRecord EmitterDataValues(const particles::Collection::Slot& slot, uint32_t fileType);

/// A trail's data on a collection (its rule's slot and the places it keeps, the newest first): the rule by its place in
/// the effect's file of that particle type, whether its first update is still to come, the count of places, then each
/// place as its position, scale and 3x4 matrix of its turn with no translation
[[nodiscard]] gamesave::TrailCollectionRecord TrailDataValues(const particles::Collection::Slot& slot, uint32_t fileType,
                                                              const std::vector<particles::fireball_rules::TrailPlace>& places);

/// The willow wisp's data on a collection: the rule by its place in the file, its parent's last place, the atoms due and
/// made (floats) and whether its first update is still to come
[[nodiscard]] gamesave::WillowWispCollectionRecord WillowWispDataValues(const particles::Collection::Slot& slot,
                                                                        uint32_t fileType);

/// The hand's sprinkle's data on a collection: the rule by its place in the file, whether its first update is still to
/// come and its age
[[nodiscard]] gamesave::HandSprinkleCollectionRecord HandSprinkleDataValues(const particles::Collection::Slot& slot,
                                                                            uint32_t fileType);

/// The atoms-at-a-target rule's data on a collection: the rule by its place in the file and the turn it took its
/// target at (its target, a reference, is the writer's)
[[nodiscard]] gamesave::TargetAtomsCollectionRecord TargetAtomsDataValues(const particles::Collection::Slot& slot,
                                                                          uint32_t fileType);

/// An atom's sound rule's data: the rule by its place in the file and whether it has started its sound
[[nodiscard]] gamesave::SoundAtomDataRecord SoundAtomDataValues(const particles::Atom& atom, const particles::Modifier& rule,
                                                                uint32_t fileType);

/// An atom's sprite (a ParticleSpriteCreator's): drawn, its creator's alpha, its creator by its place in the file, its
/// creator's file offset, its flags (centred at the base 0x01, stretched 0x02, ignoring the turn 0x04, of the land's
/// colour 0x08; the high four bits unset) and the stretch of its last two steps while it is stretched (1 until then)
[[nodiscard]] gamesave::ParticleSpriteRecord SpriteValues(const particles::Atom& atom, uint32_t fileType);

/// An atom's mist (a ParticleMistCreator's): drawn, its creator by its place in the file, its creator's initial scale,
/// whether it takes its ratio from the drawn matrix, a half the saves hold as 0, then the place and alpha of its last
/// land light map stamp (unset and 1 for a mist that has drawn none)
[[nodiscard]] gamesave::ParticleMistRecord MistValues(const particles::Atom& atom, uint32_t fileType);

/// An atom's animated textured mesh (a ParticleMeshCreatorAnimTextured's): drawn, its creator by its place in the file,
/// its flags (never clipped 0x01, of the land's colour 0x02; the high five bits unset), its creator's texture height and
/// width, whether it slides along U and along V, a byte the saves hold as 0, and its frame count
[[nodiscard]] gamesave::ParticleTexturedObjectRecord TexturedObjectValues(const particles::Atom& atom, uint32_t fileType);

/// A sound action as the original keeps it: the action, its surface, size and alignment slots, its fade step, its flags
/// (looping 0x01, delayed 0x02, soft release 0x04, of the surface 0x08, snapped to the ground 0x20) and three zero bytes
[[nodiscard]] std::array<uint8_t, 24> SoundActionBytes(const particles::SoundAction& action);

/// An atom's sound: the count of the particles' sounds alive (the original writes its global count with every sound),
/// its action, its delay and whether it has been released (its atom, a reference, is the writer's)
[[nodiscard]] gamesave::ParticleSoundRecord ParticleSoundValues(const audio::ParticleSound& sound, uint32_t liveSounds);

/// An atom's draw offset that follows the hand: taken by its one atom, its weight and its point
[[nodiscard]] gamesave::DrawOffsetHandRecord HandDrawOffsetValues(const particles::Atom::HandDrawOffset& offset);

/// An atom's decaying draw offset, taken by `references` atoms, as the original keeps it between two frames: its first
/// offset, then its age and its offset now after the last frame drawn with it (the original steps it after each frame's
/// draw, openblack before the next one), and its decay rate
[[nodiscard]] gamesave::DrawOffsetDecayRecord DecayingDrawOffsetValues(const particles::Atom::DecayingDrawOffset& offset,
                                                                       uint16_t references);

/// A chain's word the original's chain is made with and never changes (4 in all six saved chains)
constexpr uint32_t k_ChainMadeWord = 4;
/// The jitter a chain joint is made with (5); it never draws it, its jitter switch being always off
constexpr float k_ChainJointJitter = 5.0f;

/// A chain joint as its chain's record lays it out: its drawn place, scale, colour and specular, whether it has been
/// drawn, then three bytes all 120 saved joints hold as 0
[[nodiscard]] std::array<uint8_t, 28> ChainJointBytes(const particles::Collection::ChainJoint& joint);

/// A collection's chain (a ParticleChainCreator's, Collection::chainJoints): its creator by its place in the file of
/// that particle type, a word the original leaves unset, its creator's frame height and width, head and tail frames, a
/// word the chain is made with, its texture repeats (a rule's, else its creator's, else its joints - 1), its creator's
/// file offset and dynamic lighting, its v-scroll, its joints, its v-scroll rate and whether any joint has been drawn
[[nodiscard]] gamesave::ChainRecord ChainValues(const particles::Collection& collection, uint32_t fileType);

/// A chain joint's particle: drawn, its number in its chain, its jitter, and its jitter switch, off (its chain, a
/// reference, is the writer's)
[[nodiscard]] gamesave::ParticleChainJointRecord ChainJointValues(const particles::Atom& atom);

/// The way the simple beam's key-point spline ends, as its beams' data saves it: with zero slopes (the clamped kind, 1)
constexpr uint32_t k_SplineZeroEndSlopes = 1;

/// The simple beam's data on its atom: the rule by its place in the file, the game turn its object was last found
/// available, then the beams' end and their start (its object, a reference, is the writer's)
[[nodiscard]] gamesave::BeamAtomDataRecord BeamAtomDataValues(const particles::beam::Target& target,
                                                              const particles::Modifier& rule, uint32_t fileType);

/// The simple beam's data on one of its beams' collections: the rule by its place in the file, its spline's zero end
/// slopes, the spline through the key points it was last laid through (each key's share of the way, the key point and
/// the spline's second derivatives there, one curve an axis), its number and its atom's count of beams
[[nodiscard]] gamesave::BeamSubCollectionRecord BeamRibbonValues(const particles::beam::Ribbon& ribbon,
                                                                 const particles::Modifier& rule, uint32_t fileType);
/// The explosion's data on its collection: the rule by its place in the file, then its blast's state and its targets'
/// turns (their objects and the beam's spot visual, references, are the writer's)
[[nodiscard]] gamesave::ExplosionCollectionRecord ExplosionDataValues(const particles::explosion::BlastData& data,
                                                                      const particles::Modifier& rule, uint32_t fileType);
/// The lightning's data from its record: its places, flags, caps, targets (their objects the caller's), lists of
/// targets and counters, forks' scale, the turn it was made and its clash point; its links, forks, picked fork and own
/// collection, references, are the caller's
[[nodiscard]] particles::lightning::SavedBolt LightningOf(const gamesave::LightningCollectionRecord& record);
/// The explosion's data from its record (its targets' objects and the beam's spot visual are the caller's)
[[nodiscard]] particles::explosion::BlastData ExplosionDataOf(const gamesave::ExplosionCollectionRecord& record);
/// The plasma's data on one of its beams' collections, written as the simple beam's under its own class
[[nodiscard]] gamesave::PlasmaSubCollectionRecord PlasmaRibbonValues(const particles::beam::Ribbon& ribbon,
                                                                     const particles::Modifier& rule, uint32_t fileType);

/// The vortex attract rule's data on a particle it carries: the rule by its place in the file, the thing's start height,
/// the velocity it was taken with, whether it can't be destroyed, what the take kept of its 3D object (`data.look`, which
/// the caller has checked is there) and its out vortex (a reference, the writer's)
[[nodiscard]] gamesave::VortexAttractAtomDataRecord VortexAttractDataValues(const particles::vortex_attract::AtomData& data,
                                                                            const particles::Modifier& rule, uint32_t fileType);

/// The fade-out rule's data on an atom: the rule by its place in the file, whether its condition has held, and the age,
/// alpha and scale the atom started fading from
[[nodiscard]] gamesave::FadeOutAtomDataRecord FadeOutDataValues(const particles::fade_out::AtomData& data,
                                                                const particles::Modifier& rule, uint32_t fileType);

/// A particle drawn as a game object (the thing a vortex carries): drawn, the game turn the thing was last found
/// available on, its matrix (the atom's drawn matrix of this step) and the byte; the thing and the player, references,
/// are the writer's
[[nodiscard]] gamesave::ParticleGameObjectRecord GameObjectValues(const particles::vortex_attract::CarriedObject& carried,
                                                                  const particles::saved_draw::SavedDraw& draw);

/// A creature spell's first particle, standing for its creature: enabled and the game turn its creature was last found
/// there; the creature, a reference, is the writer's
[[nodiscard]] gamesave::ParticleCreatureRefRecord CreatureRefValues(const particles::creature_spell::CreatureRefData& data);

/// The class the original writes the mana path rule's data on a spark as
constexpr uint32_t k_ManaPathAtomDataType = 0xC0;

/// The mana path rule's data on a spark: the rule by its place in the file, the path's end, its start and its way across
/// the ground, the curve's terms from the cube's to the constant's, the path's length across the ground, the weave's
/// size and frequency, where along the noise it starts, and how far along its path the spark's last step left it
[[nodiscard]] gamesave::RuleDataRecord ManaPathDataValues(const particles::mana_path::AtomData& data,
                                                          const particles::Modifier& rule, uint32_t fileType);

/// The class the original writes the glowing shell rule's data on an atom as
constexpr uint32_t k_VolumeGlowAtomDataType = 0xBB;
/// The place of its object, a reference, in its class's row
constexpr size_t k_VolumeGlowObjectField = 1;

/// The glowing shell rule's data on an atom: the rule by its place in the file and the game turn its object was last
/// found there (the object, a reference, is the writer's)
[[nodiscard]] gamesave::RuleDataRecord VolumeGlowDataValues(const particles::volume_glow::AtomData& data,
                                                            const particles::Modifier& rule, uint32_t fileType);

/// The class the original writes the plasma rule's data on a carrier atom as
constexpr uint32_t k_PlasmaAtomDataType = 0xBD;

/// The plasma rule's data on a carrier atom: the rule by its place in the file, then the arc as it was asked for: its
/// start, end, start and end tangents, its life, its speed and its alpha
[[nodiscard]] gamesave::RuleDataRecord PlasmaDataValues(const particles::PlasmaCommand& arc, const particles::Modifier& rule,
                                                        uint32_t fileType);

/// The class the original writes the belief sprite rule's data on a sprite as
constexpr uint32_t k_BeliefSpriteAtomDataType = 0xBE;

/// The belief sprite rule's data on a sprite: the rule by its place in the file, where the sprite started and the alpha
/// it started with
[[nodiscard]] gamesave::RuleDataRecord BeliefSpriteDataValues(const particles::belief_sprite::AtomData& data,
                                                              const particles::Modifier& rule, uint32_t fileType);

/// The classes the original writes the creature spell wisps rule's data as, on its collection and on each wisp
constexpr uint32_t k_CreatureSpellWispsDataType = 0xC1;
constexpr uint32_t k_CreatureSpellWispDataType = 0xC2;

/// The creature spell wisps rule's data on its collection: the rule by its place in the file, whether its first update
/// is still to come, where the creature stood and the caster's hand was as the wisps began, whether its first update is
/// still to come again (the original writes it twice), the wisps due and the wisps made
[[nodiscard]] gamesave::RuleDataRecord CreatureSpellWispsValues(const particles::creature_spell::WispsData& data,
                                                                const particles::Modifier& rule, uint32_t fileType);

/// The creature spell wisps rule's data on a wisp: the rule by its place in the file, its two angles, their rates and
/// its size
[[nodiscard]] gamesave::RuleDataRecord CreatureSpellWispValues(const particles::creature_spell::WispData& data,
                                                               const particles::Modifier& rule, uint32_t fileType);

/// The class the original writes the orient-with-velocity rule's data on a sprite as
constexpr uint32_t k_OrientWithVelocityAtomDataType = 0xC3;

/// The orient-with-velocity rule's data on a sprite: the rule by its place in the file, whether its first step is still
/// to come, then its smoothing filter: the smoothed velocity, the rate and whether the filter has been started (once
/// that first step is done)
[[nodiscard]] gamesave::RuleDataRecord OrientWithVelocityValues(const particles::orient_velocity::AtomData& data,
                                                                const particles::Modifier& rule, uint32_t fileType);

/// The classes the original writes the creature spells' other rules' data on their collections as: the hearts', the
/// first rule's, the itch's and the steam's
constexpr uint32_t k_CreatureSpellCompassionDataType = 0xC5;
constexpr uint32_t k_CreatureSpellDataType = 0xC6;
constexpr uint32_t k_CreatureSpellItchDataType = 0xC7;
constexpr uint32_t k_CreatureSpellFreezeDataType = 0xC8;
/// The place of the first rule's creature, a reference, in its class's row
constexpr size_t k_CreatureSpellCreatureField = 2;

/// The first rule's data on its collection: the rule by its place in the file, whether its first update is still to
/// come and the game turn its creature was last found available (the creature, a reference, is the writer's)
[[nodiscard]] gamesave::RuleDataRecord CreatureSpellValues(const particles::creature_spell::CreatureHeld& held,
                                                           const particles::Modifier& rule, uint32_t fileType);

/// The hearts' (0xC5) or the steam's (0xC8) data on its collection: the rule by its place in the file, its first
/// update's byte (cleared on the step the data is made, so always 0 once it is there; the steam's writes it twice),
/// the atoms due and the atoms made
[[nodiscard]] gamesave::RuleDataRecord CreatureSpellAtomsMadeValues(uint32_t type,
                                                                    const particles::creature_spell::AtomsMade& made,
                                                                    const particles::Modifier& rule, uint32_t fileType);

/// The itch's data on its collection: the rule by its place in the file and whether its first update is still to come
[[nodiscard]] gamesave::RuleDataRecord CreatureSpellItchValues(bool first, const particles::Modifier& rule, uint32_t fileType);

/// The classes the original writes the sphere surface tracer's and the forest path's data on an atom as
constexpr uint32_t k_SphereTracerAtomDataType = 0xCB;
constexpr uint32_t k_ForestPathAtomDataType = 0xCD;

/// The sphere surface tracer's (0xCB) or the forest path's (0xCD) data on an atom: the rule by its place in the file,
/// whether its first step is still to come and its three angles
[[nodiscard]] gamesave::RuleDataRecord SphereAnglesValues(uint32_t type, const particles::sphere_angles::AtomData& data,
                                                          const particles::Modifier& rule, uint32_t fileType);

/// The class the original writes the flocking rule's data on its collection as
constexpr uint32_t k_FlockingDataType = 0xD0;

/// The flocking rule's data on its collection: the rule by its place in the file, its first update's byte (1: nothing
/// clears it) and the flock's velocity
[[nodiscard]] gamesave::RuleDataRecord FlockingValues(const glm::vec3& velocity, const particles::Modifier& rule,
                                                      uint32_t fileType);

/// The class the original writes the cloud mover's data on its collection as
constexpr uint32_t k_CloudMoverDataType = 0xD1;

/// The class the original writes the cloud gather's data on its collection as, and the places of its lightning
/// collection and the cloud it hangs from, references, in its row
constexpr uint32_t k_CloudGatherDataType = 0xD2;
constexpr size_t k_CloudGatherLightningField = 8;
constexpr size_t k_CloudGatherCloudField = 9;

/// The cloud gather's data on its collection: the rule by its place in the file, the heading, the clouds due and made,
/// its first update's byte, the clouds' rate, the next strike's and the strike's end's ages, the spin, then (the
/// writer's references) its lightning collection and the cloud it hangs from, then whether that collection hangs from
/// no cloud and whether it has lightning, rain and the weather storm
[[nodiscard]] gamesave::RuleDataRecord CloudGatherValues(const particles::storm::CloudGatherValues& values,
                                                         const particles::Modifier& rule, uint32_t fileType);
/// The cloud gather's data on its collection from its record (its references are the caller's); none for a record of
/// another class
[[nodiscard]] std::optional<particles::storm::CloudGatherValues> CloudGatherOf(const gamesave::RuleDataRecord& record);

/// The class the original writes the cloud gather's data on a cloud as
constexpr uint32_t k_CloudAtomDataType = 0xD3;

/// The cloud gather's data on a cloud: the rule by its place in the file, its radius and angle round the gather, a word
/// the original never writes after making it (0), whether its lightning lights it, its age then, and its height
[[nodiscard]] gamesave::RuleDataRecord CloudValues(const particles::storm::CloudData& data, const particles::Modifier& rule,
                                                   uint32_t fileType);

/// The cloud mover's data on its collection: the rule by its place in the file and whether its first call is to come
[[nodiscard]] gamesave::RuleDataRecord CloudMoverValues(bool first, const particles::Modifier& rule, uint32_t fileType);

/// The classes the original writes the tornado's data as: on each flying atom, on each debris and flying collection
/// (both as its debris collections' class), and on its collection
constexpr uint32_t k_TornadoFlyingAtomDataType = 0xD4;
constexpr uint32_t k_TornadoEmitterDataType = 0xD5;
constexpr uint32_t k_TornadoDataType = 0xD7;

/// The tornado's data on a flying atom: the rule by its place in the file, its state, the height fraction it rises to
/// and its spin's scale
[[nodiscard]] gamesave::RuleDataRecord TornadoFlyingValues(const particles::storm::TornadoFlyingData& data,
                                                           const particles::Modifier& rule, uint32_t fileType);
/// The tornado's data on a debris or flying collection: the rule by its place in the file, the atoms made and due
[[nodiscard]] gamesave::RuleDataRecord TornadoEmitterValues(const particles::storm::TornadoEmitterData& data,
                                                            const particles::Modifier& rule, uint32_t fileType);
/// The tornado's data on its collection: the rule by its place in the file, its first update, its base, top and their
/// velocities, a point the original never sets after making it (0, 0, 0), the scale, the strength, the alpha, the
/// fade and whether it is closing down
[[nodiscard]] gamesave::RuleDataRecord TornadoValues(const particles::storm::TornadoData& data, const particles::Modifier& rule,
                                                     uint32_t fileType);

/// The class the original writes the sound removal's data on an atom as
constexpr uint32_t k_SoundRemovalAtomDataType = 0xDD;

/// The sound removal's data on an atom: the rule by its place in the file and whether it has stopped the atom's sound
[[nodiscard]] gamesave::RuleDataRecord SoundRemovalValues(bool removed, const particles::Modifier& rule, uint32_t fileType);

/// The class the original writes the sub-collections rule's data on an atom as
constexpr uint32_t k_SubCollectionsAtomDataType = 0xE2;

/// The sub-collections rule's data on an atom: the rule by its place in the file and whether it has put its groups
/// under the atom
[[nodiscard]] gamesave::RuleDataRecord SubCollectionsValues(bool added, const particles::Modifier& rule, uint32_t fileType);

/// The class the original writes the fireball's rule's data on an atom as, and the place of its ball, a reference, in
/// its row
constexpr uint32_t k_FireBallAtomDataType = 0xDB;
constexpr size_t k_FireBallBallField = 2;

/// The fireball's rule's data on an atom: the rule by its place in the file, whether its first update is still to come
/// and the game turn its ball was last found there (the ball, a reference, is the writer's)
[[nodiscard]] gamesave::RuleDataRecord FireBallDataValues(const particles::fireball_rules::BallData& data,
                                                          const particles::Modifier& rule, uint32_t fileType);

/// The class the original writes the lightning sprite emitter's data on its collection as
constexpr uint32_t k_LightningSpriteDataType = 0xD9;

/// The lightning sprite emitter's data on its collection: the rule by its place in the file, the collection age its
/// flash began at, whether the flash lights the cloud, the next sprite's due time, the sprites made and whether its
/// first update is to come
[[nodiscard]] gamesave::RuleDataRecord LightningSpriteValues(const particles::storm_flash::State& state,
                                                             const particles::Modifier& rule, uint32_t fileType);

// The load's side: the values of a record put back on the engine's atom or collection, the inverse of the writes above.
// The bytes the original leaves unset are not read

/// A process info from its 60 bytes (the inverse of saved_world_write::ProcessInfoBytes)
[[nodiscard]] particles::ProcessInfo ProcessInfoOf(const std::array<uint8_t, 60>& bytes);

/// A drawn matrix of the record's layout
[[nodiscard]] particles::saved_draw::SavedDraw SavedDrawOf(const std::array<uint8_t, 56>& bytes);

/// An atom's values put back from its record: everything AtomCoreValues writes but its creator's animation frame count
/// and looping (the creator's); its drawn state for the draw from its two saved matrices
void ApplyAtomCore(const gamesave::AtomCoreRecord& record, particles::Atom& atom);

/// A collection's values put back from its record: its birth, its flags' drawn-interpolated bit, its alpha, and which of
/// its rules are still on it (those the record lists, by their places in the file)
void ApplyCollection(const gamesave::AtomCollectionRecord& record, particles::Collection& collection);

/// A rule's data on a collection put back on its slot (the emitters', the hand's sprinkle's and the atoms-at-a-target
/// rule's; the target, an entity, is the caller's)
void ApplyEmitterData(const gamesave::EmitterCollectionRecord& record, particles::Collection::Slot& slot);
void ApplyHandSprinkleData(const gamesave::HandSprinkleCollectionRecord& record, particles::Collection::Slot& slot);
/// The willow wisp's data put back on its slot
void ApplyWillowWispData(const gamesave::WillowWispCollectionRecord& record, particles::Collection::Slot& slot);
void ApplyTargetAtomsData(const gamesave::TargetAtomsCollectionRecord& record, particles::Collection::Slot& slot);
/// A trail's kept places, the newest first, from its record
[[nodiscard]] std::vector<particles::fireball_rules::TrailPlace> TrailPlacesOf(const gamesave::TrailCollectionRecord& record);
/// An atom's sound rule's data put back on the atom
void ApplySoundAtomData(const gamesave::SoundAtomDataRecord& record, const particles::Modifier& rule, particles::Atom& atom);
/// A sound action from the record's layout
[[nodiscard]] particles::SoundAction SoundActionOf(const std::array<uint8_t, 24>& bytes);
/// A draw offset that follows the hand, from its record
[[nodiscard]] particles::Atom::HandDrawOffset HandDrawOffsetOf(const gamesave::DrawOffsetHandRecord& record);
/// A decaying draw offset from its record, with no frame's ms still to take: the next frame draws its offset now; its id
/// is the caller's
[[nodiscard]] particles::Atom::DecayingDrawOffset DecayingDrawOffsetOf(const gamesave::DrawOffsetDecayRecord& record);
/// A chain joint from its chain's record layout
[[nodiscard]] particles::Collection::ChainJoint ChainJointOf(const std::array<uint8_t, 28>& bytes);
/// A collection's chain put back from its record: its joints, whether any has been drawn, its v-scroll and its rate,
/// and its texture repeats when they are not its creator's (its creator, by the joints, is the caller's)
void ApplyChain(const gamesave::ChainRecord& record, particles::Collection& collection);
/// The simple beam's data on its atom from its record: the turn and the two ends (its object is the caller's)
[[nodiscard]] particles::beam::Target BeamTargetOf(const gamesave::BeamAtomDataRecord& record);
/// The simple beam's data on a beam's collection from its record: its number, its atom's count of beams and the key
/// points of its spline
[[nodiscard]] particles::beam::Ribbon BeamRibbonOf(const gamesave::BeamSubCollectionRecord& record);
/// The plasma's data on a beam's collection from its record, read as the simple beam's
[[nodiscard]] particles::beam::Ribbon BeamRibbonOf(const gamesave::PlasmaSubCollectionRecord& record);
/// A mist's last light map stamp put back on its atom, when it had drawn one
void ApplyMist(const gamesave::ParticleMistRecord& record, particles::Atom& atom);
/// The fade-out rule's data from its record
[[nodiscard]] particles::fade_out::AtomData FadeOutDataOf(const gamesave::FadeOutAtomDataRecord& record);
/// The vortex attract rule's data from its record (its out vortex, a reference, is the caller's)
[[nodiscard]] particles::vortex_attract::AtomData VortexAttractDataOf(const gamesave::VortexAttractAtomDataRecord& record);
/// The mana path rule's data on a spark from its record; none for a record of another class
[[nodiscard]] std::optional<particles::mana_path::AtomData> ManaPathDataOf(const gamesave::RuleDataRecord& record);
/// The glowing shell rule's data on an atom from its record (its object, a reference, is the caller's); none for a
/// record of another class
[[nodiscard]] std::optional<particles::volume_glow::AtomData> VolumeGlowDataOf(const gamesave::RuleDataRecord& record);
/// The plasma rule's arc from its record; none for a record of another class
[[nodiscard]] std::optional<particles::PlasmaCommand> PlasmaDataOf(const gamesave::RuleDataRecord& record);
/// The belief sprite rule's data from its record; none for a record of another class
[[nodiscard]] std::optional<particles::belief_sprite::AtomData> BeliefSpriteDataOf(const gamesave::RuleDataRecord& record);
/// The creature spell wisps rule's data on its collection and on a wisp from their records; none for a record of
/// another class
[[nodiscard]] std::optional<particles::creature_spell::WispsData> CreatureSpellWispsOf(const gamesave::RuleDataRecord& record);
[[nodiscard]] std::optional<particles::creature_spell::WispData> CreatureSpellWispOf(const gamesave::RuleDataRecord& record);
/// The orient-with-velocity rule's data from its record; none for a record of another class
[[nodiscard]] std::optional<particles::orient_velocity::AtomData> OrientWithVelocityOf(const gamesave::RuleDataRecord& record);
/// The creature spells' collection data from their records (the first rule's creature, a reference, is the caller's);
/// none for a record of another class
[[nodiscard]] std::optional<particles::creature_spell::CreatureHeld> CreatureSpellOf(const gamesave::RuleDataRecord& record);
[[nodiscard]] std::optional<particles::creature_spell::AtomsMade>
CreatureSpellAtomsMadeOf(const gamesave::RuleDataRecord& record);
[[nodiscard]] std::optional<bool> CreatureSpellItchOf(const gamesave::RuleDataRecord& record);
/// The sphere surface tracer's or the forest path's data from its record; none for a record of another class
[[nodiscard]] std::optional<particles::sphere_angles::AtomData> SphereAnglesOf(const gamesave::RuleDataRecord& record);
/// The flock's velocity from its record; none for a record of another class
[[nodiscard]] std::optional<glm::vec3> FlockingOf(const gamesave::RuleDataRecord& record);
/// The cloud gather's data on a cloud from its record; none for a record of another class
[[nodiscard]] std::optional<particles::storm::CloudData> CloudOf(const gamesave::RuleDataRecord& record);
/// The cloud mover's first call from its record; none for a record of another class
[[nodiscard]] std::optional<bool> CloudMoverOf(const gamesave::RuleDataRecord& record);
/// The tornado's data on a flying atom, on a debris or flying collection and on its collection from their records; none
/// for a record of another class
[[nodiscard]] std::optional<particles::storm::TornadoFlyingData> TornadoFlyingOf(const gamesave::RuleDataRecord& record);
[[nodiscard]] std::optional<particles::storm::TornadoEmitterData> TornadoEmitterOf(const gamesave::RuleDataRecord& record);
[[nodiscard]] std::optional<particles::storm::TornadoData> TornadoOf(const gamesave::RuleDataRecord& record);
/// Whether the sound removal has stopped the atom's sound, from its record; none for a record of another class
[[nodiscard]] std::optional<bool> SoundRemovalOf(const gamesave::RuleDataRecord& record);
/// Whether the sub-collections rule has put its groups under the atom, from its record; none for a record of another
/// class
[[nodiscard]] std::optional<bool> SubCollectionsOf(const gamesave::RuleDataRecord& record);
/// The fireball's rule's data from its record (the ball, a reference, is the caller's); none for a record of another
/// class
[[nodiscard]] std::optional<particles::fireball_rules::BallData> FireBallDataOf(const gamesave::RuleDataRecord& record);
/// The lightning sprite emitter's data from its record; none for a record of another class
[[nodiscard]] std::optional<particles::storm_flash::State> LightningSpriteOf(const gamesave::RuleDataRecord& record);
/// A carried game object's values from its record (the thing and the player, references, are the caller's); its matrix
/// is the atom's drawn matrix of the step it was saved on, which the atom's own record holds too
[[nodiscard]] particles::vortex_attract::CarriedObject CarriedObjectOf(const gamesave::ParticleGameObjectRecord& record);
/// A creature spell's first particle's values from its record (the creature, a reference, is the caller's)
[[nodiscard]] particles::creature_spell::CreatureRefData CreatureRefOf(const gamesave::ParticleCreatureRefRecord& record);

} // namespace openblack::ecs::saved_world_write::particle_codecs
