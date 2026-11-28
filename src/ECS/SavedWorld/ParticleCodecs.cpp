/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ParticleCodecs.h"

#include <cstring>

#include <algorithm>
#include <bit>

#include "Particles/AtomSavedDraw.h"
#include "Particles/Creators/Chain.h"
#include "Particles/Creators/Mesh.h"
#include "Particles/Creators/Mist.h"
#include "Particles/ParticleEffect.h"
#include "Particles/Rules/KeyPoints.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;

namespace
{
/// The atom's flag bits the original writes: drawn on its first update, stepped once, stepped twice, deflected, in use
constexpr uint16_t k_DrawOnFirstUpdate = 0x01;
constexpr uint16_t k_SteppedOnce = 0x02;
constexpr uint16_t k_SteppedTwice = 0x04;
constexpr uint16_t k_Deflected = 0x08;
constexpr uint16_t k_InUse = 0x10;
constexpr uint16_t k_AtomFlagsWritten = 0x1F;
/// The collection's flag bits the original writes: in the effect's own coordinates, drawn interpolated
constexpr uint8_t k_OwnCoordinates = 0x01;
constexpr uint8_t k_Interpolated = 0x02;
constexpr uint8_t k_CollectionFlagsWritten = 0x03;
/// The places of the mana path rule's data's values in its class's row
constexpr size_t k_ManaPathEnd = 0;
constexpr size_t k_ManaPathStart = 1;
constexpr size_t k_ManaPathSide = 2;
constexpr size_t k_ManaPathCubic = 3;
constexpr size_t k_ManaPathSquare = 4;
constexpr size_t k_ManaPathLinear = 5;
constexpr size_t k_ManaPathConstant = 6;
constexpr size_t k_ManaPathLength = 7;
constexpr size_t k_ManaPathAmplitude = 8;
constexpr size_t k_ManaPathFrequency = 9;
constexpr size_t k_ManaPathPhase = 10;
constexpr size_t k_ManaPathProgress = 11;
/// The places of the fireball's rule's data's values in its class's row
constexpr size_t k_FireBallFirst = 0;
constexpr size_t k_FireBallTurn = 1;
/// The places of the glowing shell rule's data's values in its class's row
constexpr size_t k_VolumeGlowTurn = 0;
/// The places of the plasma rule's data's values in its class's row
constexpr size_t k_PlasmaStart = 0;
constexpr size_t k_PlasmaEnd = 1;
constexpr size_t k_PlasmaStartTangent = 2;
constexpr size_t k_PlasmaEndTangent = 3;
constexpr size_t k_PlasmaLife = 4;
constexpr size_t k_PlasmaSpeed = 5;
constexpr size_t k_PlasmaAlpha = 6;
/// The places of the belief sprite rule's data's values in its class's row
constexpr size_t k_BeliefStart = 0;
constexpr size_t k_BeliefAlpha = 1;
/// The places of the creature spell wisps rule's data's values in its classes' rows: on its collection, then on a wisp
constexpr size_t k_WispsFirst = 0;
constexpr size_t k_WispsCreature = 1;
constexpr size_t k_WispsHand = 2;
constexpr size_t k_WispsFirstAgain = 3;
constexpr size_t k_WispsDue = 4;
constexpr size_t k_WispsMade = 5;
constexpr size_t k_WispTheta = 0;
constexpr size_t k_WispPhi = 1;
constexpr size_t k_WispThetaRate = 2;
constexpr size_t k_WispPhiRate = 3;
constexpr size_t k_WispScale = 4;
/// The places of the orient-with-velocity rule's data's values in its class's row
constexpr size_t k_OrientFirst = 0;
constexpr size_t k_OrientVelocity = 1;
constexpr size_t k_OrientRate = 2;
constexpr size_t k_OrientStarted = 3;
/// The places of the creature spells' collection data's values in their classes' rows
constexpr size_t k_HeldFirst = 0;
constexpr size_t k_HeldTurn = 1;
constexpr size_t k_CountFirst = 0;
constexpr size_t k_ItchFirst = 0;
/// The places of the sphere angles' values in their classes' rows
constexpr size_t k_AnglesFirst = 0;
constexpr size_t k_AnglesTheta = 1;
constexpr size_t k_AnglesPhi = 2;
constexpr size_t k_AnglesPsi = 3;
/// The places of the flocking rule's data's values in its class's row
constexpr size_t k_FlockFirst = 0;
constexpr size_t k_FlockVelocity = 1;

gamesave::Point SavedPoint(const glm::vec3& value)
{
	return {value.x, value.y, value.z};
}

glm::vec3 VectorOfSaved(const std::optional<gamesave::Point>& value)
{
	const auto point = value.value_or(gamesave::Point {});
	return {point[0], point[1], point[2]};
}

void Put(uint8_t* at, const auto& value)
{
	std::memcpy(at, &value, sizeof(value));
}
} // namespace

std::array<uint8_t, 56> particle_codecs::DrawInfoBytes(const particles::saved_draw::SavedDraw& draw)
{
	std::array<uint8_t, 56> bytes {};
	std::memcpy(bytes.data(), draw.matrix.data(), sizeof(draw.matrix));
	Put(bytes.data() + 48, draw.scale);
	bytes[52] = draw.alpha;
	bytes[53] = k_Unset;
	bytes[54] = k_Unset;
	bytes[55] = k_Unset;
	return bytes;
}

std::array<uint8_t, 12> particle_codecs::AnimInfoBytes(const particles::Atom& atom)
{
	// the sprites', the animated textured meshes' and the light maps' creators give it their rate, frame count and
	// looping; a mist's its looping alone; the others keep the atom's first values (rate 1, one frame, not looping).
	// Whether it plays is the atom's (AR_SetAnimPlay turns it on)
	const auto* creator = atom.creator;
	const bool animated = creator != nullptr && (creator->className == "ParticleSpriteCreator" ||
	                                             creator->className == "ParticleMeshCreatorAnimTextured" ||
	                                             creator->className == "ParticleLightMapCreator");
	const bool mist = creator != nullptr && creator->className == "ParticleMistCreator";
	const float rate = animated ? atom.frameRate : 1.0f;
	const int32_t frames = animated ? creator->FramesPerAtom() : 1;
	std::array<uint8_t, 12> bytes {};
	Put(bytes.data(), rate);
	Put(bytes.data() + 4, frames);
	bytes[8] = atom.playAnim ? 1 : 0;
	bytes[9] = (animated || mist) && creator->loopAnim ? 1 : 0;
	bytes[10] = atom.savedAnimTail[0];
	bytes[11] = atom.savedAnimTail[1];
	return bytes;
}

uint16_t particle_codecs::AtomFlags(const particles::Atom& atom)
{
	uint32_t flags = ((static_cast<uint32_t>(k_Unset) << 8U) | k_Unset) & ~static_cast<uint32_t>(k_AtomFlagsWritten);
	flags |= atom.drawOnFirstUpdate ? k_DrawOnFirstUpdate : 0U;
	flags |= atom.steps == 1 ? k_SteppedOnce : (atom.steps >= 2 ? k_SteppedTwice : 0U);
	flags |= (atom.flags & k_Deflected) != 0 ? k_Deflected : 0U;
	flags |= atom.visible ? k_InUse : 0U;
	return static_cast<uint16_t>(flags);
}

gamesave::AtomCoreRecord particle_codecs::AtomCoreValues(const particles::Atom& atom)
{
	gamesave::AtomCoreRecord record;
	record.point = {atom.velocity.x, atom.velocity.y, atom.velocity.z};
	record.value = atom.birth;
	// the turn's rows, then a translation the atom never has
	std::array<float, 12> rotation {};
	for (glm::length_t row = 0; row < 3; ++row)
	{
		for (glm::length_t column = 0; column < 3; ++column)
		{
			rotation.at(static_cast<size_t>(row * 3 + column)) = atom.rotation[row][column];
		}
	}
	std::memcpy(record.zoomer.data(), rotation.data(), sizeof(rotation));
	record.floats = {atom.baseScale, atom.ruleScale, atom.stretch};
	record.secondPoint = {atom.position.x, atom.position.y, atom.position.z};
	// ARGB
	record.word = (static_cast<uint32_t>(atom.colour[3]) << 24U) | (static_cast<uint32_t>(atom.colour[0]) << 16U) |
	              (static_cast<uint32_t>(atom.colour[1]) << 8U) | static_cast<uint32_t>(atom.colour[2]);
	record.secondWord = atom.specular;
	record.half = AtomFlags(atom);
	record.drawPosition = DrawInfoBytes(atom.savedDraw[0]);
	record.secondDrawPosition = DrawInfoBytes(atom.savedDraw[1]);
	record.lastFloat = atom.previous.frame;
	record.thirdWord = std::bit_cast<uint32_t>(atom.frame);
	record.animation = AnimInfoBytes(atom);
	record.fourthWord = std::bit_cast<uint32_t>(atom.gravity);
	record.byte = static_cast<uint8_t>(atom.random);
	return record;
}

gamesave::EmitterCollectionRecord particle_codecs::EmitterDataValues(const particles::Collection::Slot& slot, uint32_t fileType)
{
	gamesave::EmitterCollectionRecord record;
	record.persistent = {.first = fileType, .second = slot.modifier != nullptr ? slot.modifier->fileIndex : 0};
	record.words = {std::bit_cast<uint32_t>(slot.state.x), static_cast<uint32_t>(static_cast<int32_t>(slot.state.y))};
	record.byte = slot.first ? 1 : 0;
	return record;
}

gamesave::TrailCollectionRecord
particle_codecs::TrailDataValues(const particles::Collection::Slot& slot, uint32_t fileType,
                                 const std::vector<particles::fireball_rules::TrailPlace>& places)
{
	gamesave::TrailCollectionRecord record;
	record.persistent = {.first = fileType, .second = slot.modifier != nullptr ? slot.modifier->fileIndex : 0};
	record.byte = slot.first ? 1 : 0;
	// the count kept, written twice by the original (as the count and as the size of the ring)
	record.word = static_cast<uint32_t>(places.size());
	for (const auto& place : places)
	{
		std::array<float, 16> cells {};
		cells[0] = place.position.x;
		cells[1] = place.position.y;
		cells[2] = place.position.z;
		cells[3] = place.scale;
		for (glm::length_t row = 0; row < 3; ++row)
		{
			for (glm::length_t column = 0; column < 3; ++column)
			{
				cells.at(static_cast<size_t>(4 + row * 3 + column)) = place.rotation[row][column];
			}
		}
		std::array<uint8_t, 64> bytes {};
		std::memcpy(bytes.data(), cells.data(), bytes.size());
		record.places.values.push_back(bytes);
	}
	return record;
}

gamesave::HandSprinkleCollectionRecord particle_codecs::HandSprinkleDataValues(const particles::Collection::Slot& slot,
                                                                               uint32_t fileType)
{
	gamesave::HandSprinkleCollectionRecord record;
	record.persistent = {.first = fileType, .second = slot.modifier != nullptr ? slot.modifier->fileIndex : 0};
	record.byte = slot.first ? 1 : 0;
	record.word = std::bit_cast<uint32_t>(slot.extra.x);
	return record;
}

gamesave::WillowWispCollectionRecord particle_codecs::WillowWispDataValues(const particles::Collection::Slot& slot,
                                                                           uint32_t fileType)
{
	gamesave::WillowWispCollectionRecord record;
	record.persistent = {.first = fileType, .second = slot.modifier != nullptr ? slot.modifier->fileIndex : 0};
	record.point = {slot.state.x, slot.state.y, slot.state.z};
	record.words = {std::bit_cast<uint32_t>(slot.extra.x), std::bit_cast<uint32_t>(slot.extra.y)};
	record.byte = slot.first ? 1 : 0;
	return record;
}

gamesave::TargetAtomsCollectionRecord particle_codecs::TargetAtomsDataValues(const particles::Collection::Slot& slot,
                                                                             uint32_t fileType)
{
	gamesave::TargetAtomsCollectionRecord record;
	record.persistent = {.first = fileType, .second = slot.modifier != nullptr ? slot.modifier->fileIndex : 0};
	record.word = std::bit_cast<uint32_t>(slot.extra.x);
	return record;
}

gamesave::SoundAtomDataRecord particle_codecs::SoundAtomDataValues(const particles::Atom& atom, const particles::Modifier& rule,
                                                                   uint32_t fileType)
{
	gamesave::SoundAtomDataRecord record;
	record.persistent = {.first = fileType, .second = rule.fileIndex};
	const auto found = atom.data.find(&rule);
	record.byte = found != atom.data.end() && found->second.x != 0.0f ? 1 : 0;
	return record;
}

gamesave::ParticleSpriteRecord particle_codecs::SpriteValues(const particles::Atom& atom, uint32_t fileType)
{
	constexpr uint8_t k_CentredAtBase = 0x01;
	constexpr uint8_t k_Stretched = 0x02;
	constexpr uint8_t k_IgnoresTurn = 0x04;
	constexpr uint8_t k_LandColour = 0x08;
	constexpr uint8_t k_SpriteFlagsWritten = 0x0F;
	gamesave::ParticleSpriteRecord record;
	record.renderByte = 1;
	const auto* creator = atom.creator;
	if (creator == nullptr)
	{
		return record;
	}
	record.byte = static_cast<uint8_t>(creator->scaleAlpha);
	record.persistent = {.first = fileType, .second = creator->fileIndex};
	const bool stretched = creator->stretch != 1.0f;
	uint32_t flags = k_Unset & ~static_cast<uint32_t>(k_SpriteFlagsWritten);
	flags |= creator->centreAtBase ? k_CentredAtBase : 0U;
	flags |= stretched ? k_Stretched : 0U;
	flags |= creator->ignoreRotation ? k_IgnoresTurn : 0U;
	flags |= creator->useLandscapeColour ? k_LandColour : 0U;
	record.bytes = {static_cast<uint8_t>(creator->fileOffset), static_cast<uint8_t>(flags)};
	// its step copies the atom's stretch while it is stretched: the last one, then this one, 1 before any step
	float previous = 1.0f;
	float current = 1.0f;
	if (stretched && atom.steps >= 1)
	{
		current = atom.current.stretch;
		previous = atom.steps >= 2 ? atom.previous.stretch : 1.0f;
	}
	record.words = {std::bit_cast<uint32_t>(previous), std::bit_cast<uint32_t>(current)};
	return record;
}

gamesave::ParticleMistRecord particle_codecs::MistValues(const particles::Atom& atom, uint32_t fileType)
{
	gamesave::ParticleMistRecord record;
	record.renderByte = 1;
	const auto* creator = dynamic_cast<const particles::MistCreator*>(atom.creator);
	if (creator == nullptr)
	{
		return record;
	}
	record.persistent = {.first = fileType, .second = creator->fileIndex};
	record.word = std::bit_cast<uint32_t>(creator->initialScale);
	record.byte = creator->takeRatioFromMatrix ? 1 : 0;
	record.half = 0;
	if (atom.mistStampPlace.has_value())
	{
		record.point = {atom.mistStampPlace->x, atom.mistStampPlace->y, atom.mistStampPlace->z};
	}
	else
	{
		const auto unset = std::bit_cast<float>(0x01010101U * k_Unset);
		record.point = {unset, unset, unset};
	}
	record.lastWord = std::bit_cast<uint32_t>(atom.mistStampAlpha);
	return record;
}

gamesave::ParticleTexturedObjectRecord particle_codecs::TexturedObjectValues(const particles::Atom& atom, uint32_t fileType)
{
	constexpr uint8_t k_NeverClipped = 0x01;
	constexpr uint8_t k_LandColour = 0x02;
	constexpr uint8_t k_ObjectFlagsWritten = 0x07;
	gamesave::ParticleTexturedObjectRecord record;
	record.renderByte = 1;
	const auto* creator = dynamic_cast<const particles::MeshCreator*>(atom.creator);
	if (creator == nullptr)
	{
		return record;
	}
	record.persistent = {.first = fileType, .second = creator->fileIndex};
	uint32_t flags = k_Unset & ~static_cast<uint32_t>(k_ObjectFlagsWritten);
	// the animated textured creator reads NeverClip where the plain one reads CastHumanShadow
	flags |= creator->neverClip ? k_NeverClipped : 0U;
	flags |= creator->drawWithLandscapeColour ? k_LandColour : 0U;
	record.byte = static_cast<uint8_t>(flags);
	record.words = {static_cast<uint32_t>(creator->textureHeight), static_cast<uint32_t>(creator->textureWidth)};
	record.bytes = {static_cast<uint8_t>(creator->slideU ? 1 : 0), static_cast<uint8_t>(creator->slideV ? 1 : 0), 0};
	record.lastWord = static_cast<uint32_t>(creator->FramesPerAtom());
	return record;
}

std::array<uint8_t, 24> particle_codecs::SoundActionBytes(const particles::SoundAction& action)
{
	std::array<uint8_t, 24> bytes {};
	Put(bytes.data(), action.action);
	Put(bytes.data() + 4, action.surface);
	Put(bytes.data() + 8, action.size);
	Put(bytes.data() + 12, action.alignment);
	Put(bytes.data() + 16, action.fadeStep);
	bytes[20] = action.flags;
	return bytes;
}

gamesave::ParticleSoundRecord particle_codecs::ParticleSoundValues(const audio::ParticleSound& sound, uint32_t liveSounds)
{
	gamesave::ParticleSoundRecord record;
	record.word = liveSounds;
	record.action = SoundActionBytes(sound.action);
	record.secondWord = std::bit_cast<uint32_t>(sound.delay);
	record.byte = sound.released ? 1 : 0;
	return record;
}

namespace
{
template <typename T>
T Get(const uint8_t* at)
{
	T value {};
	std::memcpy(&value, at, sizeof(value));
	return value;
}
} // namespace

gamesave::DrawOffsetHandRecord particle_codecs::HandDrawOffsetValues(const particles::Atom::HandDrawOffset& offset)
{
	return {.references = 1, .amount = offset.weight, .point = {offset.reference.x, offset.reference.y, offset.reference.z}};
}

gamesave::DrawOffsetDecayRecord particle_codecs::DecayingDrawOffsetValues(const particles::Atom::DecayingDrawOffset& offset,
                                                                          uint16_t references)
{
	// the step the original has made already after the last frame's draw: that frame's ms on the age
	auto stepped = offset;
	stepped.Step(0);
	return {.references = references,
	        .start = {offset.initial.x, offset.initial.y, offset.initial.z},
	        .current = {stepped.current.x, stepped.current.y, stepped.current.z},
	        .age = stepped.age,
	        .rate = particles::Atom::DecayingDrawOffset::k_DecayPerSecond};
}

std::array<uint8_t, 28> particle_codecs::ChainJointBytes(const particles::Collection::ChainJoint& joint)
{
	std::array<uint8_t, 28> bytes {};
	Put(bytes.data(), joint.position.x);
	Put(bytes.data() + 4, joint.position.y);
	Put(bytes.data() + 8, joint.position.z);
	Put(bytes.data() + 12, joint.scale);
	Put(bytes.data() + 16, joint.colour);
	Put(bytes.data() + 20, joint.specular);
	bytes[24] = joint.drawn ? 1 : 0;
	return bytes;
}

gamesave::ChainRecord particle_codecs::ChainValues(const particles::Collection& collection, uint32_t fileType)
{
	gamesave::ChainRecord record;
	const auto unset = 0x01010101U * k_Unset;
	const auto joints = static_cast<int>(collection.chainJoints.size());
	const auto* creator = dynamic_cast<const particles::ChainCreator*>(collection.chainCreator);
	if (creator != nullptr)
	{
		record.persistent = {.first = fileType, .second = creator->fileIndex};
		// the creator's repeats, or one a segment for -1, unless a rule has set the chain's own
		int repeats = creator->numTexturesForWholeChain == -1 ? joints - 1 : creator->numTexturesForWholeChain;
		repeats = collection.chainTextures != -1 ? collection.chainTextures : repeats;
		record.words = {unset,
		                static_cast<uint32_t>(creator->frameHeight),
		                static_cast<uint32_t>(creator->frameWidth),
		                static_cast<uint32_t>(creator->frameOfHead),
		                static_cast<uint32_t>(creator->frameOfTail),
		                k_ChainMadeWord,
		                static_cast<uint32_t>(repeats),
		                static_cast<uint32_t>(creator->fileOffset)};
		record.byte = creator->dynamicLighting ? 1 : 0;
	}
	record.word = std::bit_cast<uint32_t>(collection.chainScroll);
	for (const auto& joint : collection.chainJoints)
	{
		record.joints.values.push_back(ChainJointBytes(joint));
	}
	record.lastWord = std::bit_cast<uint32_t>(collection.chainScrollRate);
	record.lastByte = collection.chainDrawn ? 1 : 0;
	return record;
}

gamesave::ParticleChainJointRecord particle_codecs::ChainJointValues(const particles::Atom& atom)
{
	gamesave::ParticleChainJointRecord record;
	record.renderByte = 1;
	record.words = {static_cast<uint32_t>(atom.chainJoint), std::bit_cast<uint32_t>(k_ChainJointJitter), 0};
	return record;
}

particles::Collection::ChainJoint particle_codecs::ChainJointOf(const std::array<uint8_t, 28>& bytes)
{
	particles::Collection::ChainJoint joint;
	std::memcpy(&joint.position.x, bytes.data(), 4);
	std::memcpy(&joint.position.y, bytes.data() + 4, 4);
	std::memcpy(&joint.position.z, bytes.data() + 8, 4);
	std::memcpy(&joint.scale, bytes.data() + 12, 4);
	std::memcpy(&joint.colour, bytes.data() + 16, 4);
	std::memcpy(&joint.specular, bytes.data() + 20, 4);
	joint.drawn = bytes[24] != 0;
	return joint;
}

void particle_codecs::ApplyChain(const gamesave::ChainRecord& record, particles::Collection& collection)
{
	collection.chainJoints.clear();
	for (const auto& bytes : record.joints.values)
	{
		collection.chainJoints.push_back(ChainJointOf(bytes));
	}
	collection.chainDrawn = record.lastByte != 0;
	collection.chainScroll = std::bit_cast<float>(record.word);
	collection.chainScrollRate = std::bit_cast<float>(record.lastWord);
	// the repeats are the chain's own when they are not what its creator gives a chain of that many joints
	const auto* creator = dynamic_cast<const particles::ChainCreator*>(collection.chainCreator);
	const auto joints = static_cast<int>(collection.chainJoints.size());
	const auto repeats = static_cast<int>(record.words[6]);
	const int made = creator == nullptr                        ? repeats
	                 : creator->numTexturesForWholeChain == -1 ? joints - 1
	                                                           : creator->numTexturesForWholeChain;
	collection.chainTextures = repeats != made ? repeats : -1;
}

gamesave::BeamAtomDataRecord particle_codecs::BeamAtomDataValues(const particles::beam::Target& target,
                                                                 const particles::Modifier& rule, uint32_t fileType)
{
	gamesave::BeamAtomDataRecord record;
	record.persistent = {.first = fileType, .second = rule.fileIndex};
	record.word = target.validatedTurn;
	record.points = {gamesave::Point {target.target.x, target.target.y, target.target.z},
	                 gamesave::Point {target.origin.x, target.origin.y, target.origin.z}};
	return record;
}

gamesave::BeamSubCollectionRecord particle_codecs::BeamRibbonValues(const particles::beam::Ribbon& ribbon,
                                                                    const particles::Modifier& rule, uint32_t fileType)
{
	gamesave::BeamSubCollectionRecord record;
	record.persistent = {.first = fileType, .second = rule.fileIndex};
	record.word = k_SplineZeroEndSlopes;
	const auto& keys = ribbon.keys;
	if (keys.size() >= 2)
	{
		// the key points at their shares of the way, as the beam lays them, one spline an axis
		const float keyStep = 1.0f / (static_cast<float>(keys.size()) - 1.0f);
		std::array<std::vector<float>, 3> pairs;
		for (size_t i = 0; i < keys.size(); ++i)
		{
			const float t = static_cast<float>(i) * keyStep;
			for (size_t axis = 0; axis < pairs.size(); ++axis)
			{
				pairs.at(axis).push_back(t);
				pairs.at(axis).push_back(keys[i][static_cast<glm::length_t>(axis)]);
			}
		}
		const std::array<particles::key_points::Spline, 3> curves {particles::key_points::Make(pairs[0]),
		                                                           particles::key_points::Make(pairs[1]),
		                                                           particles::key_points::Make(pairs[2])};
		for (size_t i = 0; i < keys.size(); ++i)
		{
			record.points.elements.push_back({.word = std::bit_cast<uint32_t>(curves[0].keys[i].x),
			                                  .first = {keys[i].x, keys[i].y, keys[i].z},
			                                  .second = {curves[0].keys[i].z, curves[1].keys[i].z, curves[2].keys[i].z}});
		}
	}
	record.lastWords = {static_cast<uint32_t>(ribbon.number), static_cast<uint32_t>(ribbon.beams)};
	return record;
}

gamesave::ExplosionCollectionRecord particle_codecs::ExplosionDataValues(const particles::explosion::BlastData& data,
                                                                         const particles::Modifier& rule, uint32_t fileType)
{
	gamesave::ExplosionCollectionRecord record;
	record.persistent = {.first = fileType, .second = rule.fileIndex};
	record.bytes = {static_cast<uint8_t>(data.notStarted ? 1 : 0), static_cast<uint8_t>(data.anyTarget ? 1 : 0)};
	record.centre = {data.centre.x, data.centre.y, data.centre.z};
	record.spread = std::bit_cast<uint32_t>(data.spread);
	for (const auto& target : data.targets)
	{
		record.targets.elements.push_back({.word = target.turn, .object = {0}});
	}
	record.counts = {static_cast<uint32_t>(data.exploded), static_cast<uint32_t>(data.deleted)};
	record.made = {static_cast<uint8_t>(data.smokeDone ? 1 : 0), static_cast<uint8_t>(data.beamDone ? 1 : 0)};
	record.stopped = static_cast<uint8_t>(data.finished ? 1 : 0);
	return record;
}

particles::lightning::SavedBolt particle_codecs::LightningOf(const gamesave::LightningCollectionRecord& record)
{
	const auto vector = [](const gamesave::Point& p) { return glm::vec3(p[0], p[1], p[2]); };
	particles::lightning::SavedBolt bolt;
	bolt.origin = vector(record.points[0]);
	bolt.searchOrigin = vector(record.points[1]);
	bolt.centroid = vector(record.points[2]);
	bolt.heading = std::bit_cast<float>(record.word);
	bolt.first = record.bytes[0] != 0;
	bolt.fromHand = record.bytes[1] != 0;
	bolt.life = std::bit_cast<float>(record.words[0]);
	bolt.centroidDistance = std::bit_cast<float>(record.words[1]);
	bolt.searchLimit = static_cast<int>(record.words[2]);
	bolt.atOnce = static_cast<int>(record.words[3]);
	for (const auto& target : record.targets.elements)
	{
		bolt.targets.push_back({.object = entt::null,
		                        .turn = target.turn,
		                        .point = vector(target.point),
		                        .isObject = target.bytes[0] != 0,
		                        .active = target.bytes[1] != 0,
		                        .lightMapDone = target.bytes[2] != 0,
		                        .cooldown = static_cast<int>(target.word)});
	}
	for (const auto& fork : record.forks.elements)
	{
		auto& list = bolt.lists.emplace_back();
		for (const auto index : fork.words.values)
		{
			list.push_back(static_cast<int>(index));
		}
	}
	bolt.nextList = static_cast<int>(record.forkWord);
	bolt.nextFork = static_cast<int>(record.heldWord);
	bolt.forkScale = std::bit_cast<float>(record.lastWords[0]);
	bolt.createdTurn = record.lastWords[1];
	bolt.clash = vector(record.lastPoint);
	return bolt;
}

particles::explosion::BlastData particle_codecs::ExplosionDataOf(const gamesave::ExplosionCollectionRecord& record)
{
	particles::explosion::BlastData data;
	data.notStarted = record.bytes[0] != 0;
	data.anyTarget = record.bytes[1] != 0;
	data.centre = glm::vec3(record.centre[0], record.centre[1], record.centre[2]);
	data.spread = std::bit_cast<float>(record.spread);
	for (const auto& target : record.targets.elements)
	{
		data.targets.push_back({.turn = target.word, .object = entt::null});
	}
	data.exploded = static_cast<int>(record.counts[0]);
	data.deleted = static_cast<int>(record.counts[1]);
	data.smokeDone = record.made[0] != 0;
	data.beamDone = record.made[1] != 0;
	data.finished = record.stopped != 0;
	data.loadedFx = record.beamFx.number;
	return data;
}

gamesave::PlasmaSubCollectionRecord particle_codecs::PlasmaRibbonValues(const particles::beam::Ribbon& ribbon,
                                                                        const particles::Modifier& rule, uint32_t fileType)
{
	const auto beam = BeamRibbonValues(ribbon, rule, fileType);
	return gamesave::PlasmaSubCollectionRecord {.thing = beam.thing,
	                                            .persistent = beam.persistent,
	                                            .word = beam.word,
	                                            .points = beam.points,
	                                            .lastWords = beam.lastWords};
}

gamesave::VortexAttractAtomDataRecord particle_codecs::VortexAttractDataValues(const particles::vortex_attract::AtomData& data,
                                                                               const particles::Modifier& rule,
                                                                               uint32_t fileType)
{
	gamesave::VortexAttractAtomDataRecord record;
	record.persistent = {.first = fileType, .second = rule.fileIndex};
	record.word = std::bit_cast<uint32_t>(data.startHeight);
	record.point = {data.takenVelocity.x, data.takenVelocity.y, data.takenVelocity.z};
	const auto look = data.look.value_or(particles::vortex_attract::AtomData::TakenLook {});
	record.bytes = {static_cast<uint8_t>(data.indestructible ? 1 : 0), static_cast<uint8_t>(look.wasPaper ? 1 : 0),
	                static_cast<uint8_t>(look.wasDisappearing ? 1 : 0)};
	record.secondWord = std::bit_cast<uint32_t>(look.importance);
	return record;
}

gamesave::RuleDataRecord particle_codecs::ManaPathDataValues(const particles::mana_path::AtomData& data,
                                                             const particles::Modifier& rule, uint32_t fileType)
{
	auto record = gamesave::RuleDataRecord::Of(k_ManaPathAtomDataType).value_or(gamesave::RuleDataRecord {});
	record.persistent = {.first = fileType, .second = rule.fileIndex};
	const auto& path = data.path;
	record.SetPoint(k_ManaPathEnd, SavedPoint(path.to));
	record.SetPoint(k_ManaPathStart, SavedPoint(path.from));
	record.SetPoint(k_ManaPathSide, SavedPoint(path.side));
	record.SetPoint(k_ManaPathCubic, SavedPoint(path.cubic));
	record.SetPoint(k_ManaPathSquare, SavedPoint(path.square));
	record.SetPoint(k_ManaPathLinear, SavedPoint(path.linear));
	record.SetPoint(k_ManaPathConstant, SavedPoint(path.constant));
	record.SetFloat(k_ManaPathLength, path.length);
	record.SetFloat(k_ManaPathAmplitude, path.amplitude);
	record.SetFloat(k_ManaPathFrequency, path.frequency);
	record.SetFloat(k_ManaPathPhase, path.phase);
	record.SetFloat(k_ManaPathProgress, data.progress);
	return record;
}

gamesave::RuleDataRecord particle_codecs::VolumeGlowDataValues(const particles::volume_glow::AtomData& data,
                                                               const particles::Modifier& rule, uint32_t fileType)
{
	auto record = gamesave::RuleDataRecord::Of(k_VolumeGlowAtomDataType).value_or(gamesave::RuleDataRecord {});
	record.persistent = {.first = fileType, .second = rule.fileIndex};
	record.SetWord(k_VolumeGlowTurn, data.validatedTurn);
	return record;
}

gamesave::RuleDataRecord particle_codecs::PlasmaDataValues(const particles::PlasmaCommand& arc, const particles::Modifier& rule,
                                                           uint32_t fileType)
{
	auto record = gamesave::RuleDataRecord::Of(k_PlasmaAtomDataType).value_or(gamesave::RuleDataRecord {});
	record.persistent = {.first = fileType, .second = rule.fileIndex};
	record.SetPoint(k_PlasmaStart, SavedPoint(arc.start));
	record.SetPoint(k_PlasmaEnd, SavedPoint(arc.end));
	record.SetPoint(k_PlasmaStartTangent, SavedPoint(arc.startTangent));
	record.SetPoint(k_PlasmaEndTangent, SavedPoint(arc.endTangent));
	record.SetFloat(k_PlasmaLife, arc.life);
	record.SetFloat(k_PlasmaSpeed, arc.speed);
	record.SetByte(k_PlasmaAlpha, arc.alpha);
	return record;
}

gamesave::RuleDataRecord particle_codecs::BeliefSpriteDataValues(const particles::belief_sprite::AtomData& data,
                                                                 const particles::Modifier& rule, uint32_t fileType)
{
	auto record = gamesave::RuleDataRecord::Of(k_BeliefSpriteAtomDataType).value_or(gamesave::RuleDataRecord {});
	record.persistent = {.first = fileType, .second = rule.fileIndex};
	record.SetPoint(k_BeliefStart, SavedPoint(data.start));
	record.SetByte(k_BeliefAlpha, data.alpha);
	return record;
}

gamesave::RuleDataRecord particle_codecs::CreatureSpellWispsValues(const particles::creature_spell::WispsData& data,
                                                                   const particles::Modifier& rule, uint32_t fileType)
{
	auto record = gamesave::RuleDataRecord::Of(k_CreatureSpellWispsDataType).value_or(gamesave::RuleDataRecord {});
	record.persistent = {.first = fileType, .second = rule.fileIndex};
	const auto first = static_cast<uint8_t>(data.first ? 1 : 0);
	record.SetByte(k_WispsFirst, first);
	record.SetPoint(k_WispsCreature, SavedPoint(data.creature));
	record.SetPoint(k_WispsHand, SavedPoint(data.hand));
	record.SetByte(k_WispsFirstAgain, first);
	record.SetFloat(k_WispsDue, data.due);
	record.SetWord(k_WispsMade, static_cast<uint32_t>(data.made));
	return record;
}

gamesave::RuleDataRecord particle_codecs::CreatureSpellWispValues(const particles::creature_spell::WispData& data,
                                                                  const particles::Modifier& rule, uint32_t fileType)
{
	auto record = gamesave::RuleDataRecord::Of(k_CreatureSpellWispDataType).value_or(gamesave::RuleDataRecord {});
	record.persistent = {.first = fileType, .second = rule.fileIndex};
	record.SetFloat(k_WispTheta, data.theta);
	record.SetFloat(k_WispPhi, data.phi);
	record.SetFloat(k_WispThetaRate, data.thetaRate);
	record.SetFloat(k_WispPhiRate, data.phiRate);
	record.SetFloat(k_WispScale, data.scale);
	return record;
}

gamesave::RuleDataRecord particle_codecs::OrientWithVelocityValues(const particles::orient_velocity::AtomData& data,
                                                                   const particles::Modifier& rule, uint32_t fileType)
{
	auto record = gamesave::RuleDataRecord::Of(k_OrientWithVelocityAtomDataType).value_or(gamesave::RuleDataRecord {});
	record.persistent = {.first = fileType, .second = rule.fileIndex};
	record.SetByte(k_OrientFirst, static_cast<uint8_t>(data.first ? 1 : 0));
	record.SetPoint(k_OrientVelocity, SavedPoint(data.velocity));
	record.SetFloat(k_OrientRate, data.rate);
	record.SetByte(k_OrientStarted, static_cast<uint8_t>(data.first ? 0 : 1));
	return record;
}

gamesave::RuleDataRecord particle_codecs::CreatureSpellValues(const particles::creature_spell::CreatureHeld& held,
                                                              const particles::Modifier& rule, uint32_t fileType)
{
	auto record = gamesave::RuleDataRecord::Of(k_CreatureSpellDataType).value_or(gamesave::RuleDataRecord {});
	record.persistent = {.first = fileType, .second = rule.fileIndex};
	record.SetByte(k_HeldFirst, static_cast<uint8_t>(held.first ? 1 : 0));
	record.SetWord(k_HeldTurn, held.validatedTurn);
	return record;
}

gamesave::RuleDataRecord particle_codecs::CreatureSpellAtomsMadeValues(uint32_t type,
                                                                       const particles::creature_spell::AtomsMade& made,
                                                                       const particles::Modifier& rule, uint32_t fileType)
{
	auto record = gamesave::RuleDataRecord::Of(type).value_or(gamesave::RuleDataRecord {});
	record.persistent = {.first = fileType, .second = rule.fileIndex};
	// the hearts' row is the byte, then the two counts; the steam's the byte twice, then the two counts
	const size_t counts = type == k_CreatureSpellFreezeDataType ? 2 : 1;
	record.SetByte(k_CountFirst, 0);
	if (counts == 2)
	{
		record.SetByte(1, 0);
	}
	record.SetFloat(counts, made.due);
	record.SetWord(counts + 1, static_cast<uint32_t>(made.made));
	return record;
}

gamesave::RuleDataRecord particle_codecs::CreatureSpellItchValues(bool first, const particles::Modifier& rule,
                                                                  uint32_t fileType)
{
	auto record = gamesave::RuleDataRecord::Of(k_CreatureSpellItchDataType).value_or(gamesave::RuleDataRecord {});
	record.persistent = {.first = fileType, .second = rule.fileIndex};
	record.SetByte(k_ItchFirst, static_cast<uint8_t>(first ? 1 : 0));
	return record;
}

gamesave::RuleDataRecord particle_codecs::SphereAnglesValues(uint32_t type, const particles::sphere_angles::AtomData& data,
                                                             const particles::Modifier& rule, uint32_t fileType)
{
	auto record = gamesave::RuleDataRecord::Of(type).value_or(gamesave::RuleDataRecord {});
	record.persistent = {.first = fileType, .second = rule.fileIndex};
	record.SetByte(k_AnglesFirst, static_cast<uint8_t>(data.first ? 1 : 0));
	record.SetFloat(k_AnglesTheta, data.theta);
	record.SetFloat(k_AnglesPhi, data.phi);
	record.SetFloat(k_AnglesPsi, data.psi);
	return record;
}

gamesave::RuleDataRecord particle_codecs::FlockingValues(const glm::vec3& velocity, const particles::Modifier& rule,
                                                         uint32_t fileType)
{
	auto record = gamesave::RuleDataRecord::Of(k_FlockingDataType).value_or(gamesave::RuleDataRecord {});
	record.persistent = {.first = fileType, .second = rule.fileIndex};
	record.SetByte(k_FlockFirst, 1);
	record.SetPoint(k_FlockVelocity, SavedPoint(velocity));
	return record;
}

gamesave::RuleDataRecord particle_codecs::CloudGatherValues(const particles::storm::CloudGatherValues& values,
                                                            const particles::Modifier& rule, uint32_t fileType)
{
	auto record = gamesave::RuleDataRecord::Of(k_CloudGatherDataType).value_or(gamesave::RuleDataRecord {});
	record.persistent = {.first = fileType, .second = rule.fileIndex};
	const auto byte = [](bool value) { return static_cast<uint8_t>(value ? 1 : 0); };
	record.SetPoint(0, SavedPoint(values.heading));
	record.SetFloat(1, values.emitted);
	record.SetWord(2, static_cast<uint32_t>(values.count));
	record.SetByte(3, byte(values.first));
	record.SetFloat(4, values.rate);
	record.SetFloat(5, values.nextStrike);
	record.SetFloat(6, values.strikeEnd);
	record.SetFloat(7, values.spin);
	record.SetByte(10, byte(values.detached));
	record.SetByte(11, byte(values.lightningOn));
	record.SetByte(12, byte(values.rainOn));
	record.SetByte(13, byte(values.registered));
	return record;
}

std::optional<particles::storm::CloudGatherValues> particle_codecs::CloudGatherOf(const gamesave::RuleDataRecord& record)
{
	if (record.type != k_CloudGatherDataType)
	{
		return std::nullopt;
	}
	const auto flag = [&record](size_t field) { return record.Byte(field).value_or(0) != 0; };
	return particles::storm::CloudGatherValues {.heading = VectorOfSaved(record.PointAt(0)),
	                                            .emitted = record.Float(1).value_or(0.0f),
	                                            .count = static_cast<int>(record.Word(2).value_or(0)),
	                                            .first = flag(3),
	                                            .rate = record.Float(4).value_or(0.0f),
	                                            .nextStrike = record.Float(5).value_or(0.0f),
	                                            .strikeEnd = record.Float(6).value_or(0.0f),
	                                            .spin = record.Float(7).value_or(0.0f),
	                                            .detached = flag(10),
	                                            .lightningOn = flag(11),
	                                            .rainOn = flag(12),
	                                            .registered = flag(13)};
}

gamesave::RuleDataRecord particle_codecs::CloudValues(const particles::storm::CloudData& data, const particles::Modifier& rule,
                                                      uint32_t fileType)
{
	auto record = gamesave::RuleDataRecord::Of(k_CloudAtomDataType).value_or(gamesave::RuleDataRecord {});
	record.persistent = {.first = fileType, .second = rule.fileIndex};
	record.SetFloat(0, data.radius);
	record.SetFloat(1, data.theta);
	record.SetWord(2, 0);
	record.SetByte(3, static_cast<uint8_t>(data.flashing ? 1 : 0));
	record.SetFloat(4, data.flashStart);
	record.SetFloat(5, data.height);
	return record;
}

std::optional<particles::storm::CloudData> particle_codecs::CloudOf(const gamesave::RuleDataRecord& record)
{
	if (record.type != k_CloudAtomDataType)
	{
		return std::nullopt;
	}
	return particles::storm::CloudData {.radius = record.Float(0).value_or(0.0f),
	                                    .theta = record.Float(1).value_or(0.0f),
	                                    .flashing = record.Byte(3).value_or(0) != 0,
	                                    .flashStart = record.Float(4).value_or(0.0f),
	                                    .height = record.Float(5).value_or(0.0f)};
}

gamesave::RuleDataRecord particle_codecs::CloudMoverValues(bool first, const particles::Modifier& rule, uint32_t fileType)
{
	auto record = gamesave::RuleDataRecord::Of(k_CloudMoverDataType).value_or(gamesave::RuleDataRecord {});
	record.persistent = {.first = fileType, .second = rule.fileIndex};
	record.SetByte(0, static_cast<uint8_t>(first ? 1 : 0));
	return record;
}

gamesave::RuleDataRecord particle_codecs::TornadoFlyingValues(const particles::storm::TornadoFlyingData& data,
                                                              const particles::Modifier& rule, uint32_t fileType)
{
	auto record = gamesave::RuleDataRecord::Of(k_TornadoFlyingAtomDataType).value_or(gamesave::RuleDataRecord {});
	record.persistent = {.first = fileType, .second = rule.fileIndex};
	record.SetWord(0, static_cast<uint32_t>(data.state));
	record.SetFloat(1, data.blend);
	record.SetFloat(2, data.speed);
	return record;
}

gamesave::RuleDataRecord particle_codecs::TornadoEmitterValues(const particles::storm::TornadoEmitterData& data,
                                                               const particles::Modifier& rule, uint32_t fileType)
{
	auto record = gamesave::RuleDataRecord::Of(k_TornadoEmitterDataType).value_or(gamesave::RuleDataRecord {});
	record.persistent = {.first = fileType, .second = rule.fileIndex};
	record.SetFloat(0, data.made);
	record.SetFloat(1, data.owed);
	return record;
}

gamesave::RuleDataRecord particle_codecs::TornadoValues(const particles::storm::TornadoData& data,
                                                        const particles::Modifier& rule, uint32_t fileType)
{
	auto record = gamesave::RuleDataRecord::Of(k_TornadoDataType).value_or(gamesave::RuleDataRecord {});
	record.persistent = {.first = fileType, .second = rule.fileIndex};
	const auto byte = [](bool value) { return static_cast<uint8_t>(value ? 1 : 0); };
	record.SetByte(0, byte(data.first));
	record.SetPoint(1, SavedPoint(data.base));
	record.SetPoint(2, SavedPoint(data.top));
	record.SetPoint(3, SavedPoint(data.baseVelocity));
	record.SetPoint(4, SavedPoint(data.topVelocity));
	record.SetPoint(5, gamesave::Point {});
	record.SetFloat(6, data.tornadoScale);
	record.SetFloat(7, data.strength);
	record.SetByte(8, data.alpha);
	record.SetFloat(9, data.fade);
	record.SetByte(10, byte(data.closing));
	return record;
}

gamesave::RuleDataRecord particle_codecs::SoundRemovalValues(bool removed, const particles::Modifier& rule, uint32_t fileType)
{
	auto record = gamesave::RuleDataRecord::Of(k_SoundRemovalAtomDataType).value_or(gamesave::RuleDataRecord {});
	record.persistent = {.first = fileType, .second = rule.fileIndex};
	record.SetByte(0, static_cast<uint8_t>(removed ? 1 : 0));
	return record;
}

gamesave::RuleDataRecord particle_codecs::SubCollectionsValues(bool added, const particles::Modifier& rule, uint32_t fileType)
{
	auto record = gamesave::RuleDataRecord::Of(k_SubCollectionsAtomDataType).value_or(gamesave::RuleDataRecord {});
	record.persistent = {.first = fileType, .second = rule.fileIndex};
	record.SetByte(0, static_cast<uint8_t>(added ? 1 : 0));
	return record;
}

gamesave::RuleDataRecord particle_codecs::FireBallDataValues(const particles::fireball_rules::BallData& data,
                                                             const particles::Modifier& rule, uint32_t fileType)
{
	auto record = gamesave::RuleDataRecord::Of(k_FireBallAtomDataType).value_or(gamesave::RuleDataRecord {});
	record.persistent = {.first = fileType, .second = rule.fileIndex};
	record.SetByte(k_FireBallFirst, static_cast<uint8_t>(data.firstUpdate ? 1 : 0));
	record.SetWord(k_FireBallTurn, data.validatedTurn);
	return record;
}

gamesave::RuleDataRecord particle_codecs::LightningSpriteValues(const particles::storm_flash::State& state,
                                                                const particles::Modifier& rule, uint32_t fileType)
{
	auto record = gamesave::RuleDataRecord::Of(k_LightningSpriteDataType).value_or(gamesave::RuleDataRecord {});
	record.persistent = {.first = fileType, .second = rule.fileIndex};
	record.SetFloat(0, state.strikeAge);
	record.SetByte(1, static_cast<uint8_t>(state.striking ? 1 : 0));
	record.SetFloat(2, state.nextTime);
	record.SetWord(3, static_cast<uint32_t>(state.emitted));
	record.SetByte(4, static_cast<uint8_t>(state.first ? 1 : 0));
	return record;
}

gamesave::ParticleGameObjectRecord particle_codecs::GameObjectValues(const particles::vortex_attract::CarriedObject& carried,
                                                                     const particles::saved_draw::SavedDraw& draw)
{
	gamesave::ParticleGameObjectRecord record;
	record.renderByte = 1;
	record.word = carried.availableTurn;
	std::memcpy(record.zoomer.data(), draw.matrix.data(), sizeof(draw.matrix));
	record.byte = carried.byte;
	return record;
}

gamesave::ParticleCreatureRefRecord particle_codecs::CreatureRefValues(const particles::creature_spell::CreatureRefData& data)
{
	gamesave::ParticleCreatureRefRecord record;
	record.renderByte = data.enabled ? 1 : 0;
	record.turn = data.validatedTurn;
	return record;
}

particles::beam::Target particle_codecs::BeamTargetOf(const gamesave::BeamAtomDataRecord& record)
{
	return {.validatedTurn = record.word,
	        .target = glm::vec3(record.points[0][0], record.points[0][1], record.points[0][2]),
	        .origin = glm::vec3(record.points[1][0], record.points[1][1], record.points[1][2])};
}

particles::beam::Ribbon particle_codecs::BeamRibbonOf(const gamesave::BeamSubCollectionRecord& record)
{
	particles::beam::Ribbon ribbon;
	ribbon.number = static_cast<int>(record.lastWords[0]);
	ribbon.beams = static_cast<int>(record.lastWords[1]);
	for (const auto& point : record.points.elements)
	{
		ribbon.keys.emplace_back(point.first[0], point.first[1], point.first[2]);
	}
	return ribbon;
}

particles::beam::Ribbon particle_codecs::BeamRibbonOf(const gamesave::PlasmaSubCollectionRecord& record)
{
	return BeamRibbonOf(gamesave::BeamSubCollectionRecord {.thing = record.thing,
	                                                       .persistent = record.persistent,
	                                                       .word = record.word,
	                                                       .points = record.points,
	                                                       .lastWords = record.lastWords});
}

particles::Atom::HandDrawOffset particle_codecs::HandDrawOffsetOf(const gamesave::DrawOffsetHandRecord& record)
{
	return {.reference = glm::vec3(record.point[0], record.point[1], record.point[2]), .weight = record.amount};
}

particles::Atom::DecayingDrawOffset particle_codecs::DecayingDrawOffsetOf(const gamesave::DrawOffsetDecayRecord& record)
{
	return {.initial = glm::vec3(record.start[0], record.start[1], record.start[2]),
	        .current = glm::vec3(record.current[0], record.current[1], record.current[2]),
	        .age = record.age};
}

particles::ProcessInfo particle_codecs::ProcessInfoOf(const std::array<uint8_t, 60>& bytes)
{
	const auto point = [&bytes](size_t at) {
		return glm::vec3(Get<float>(bytes.data() + at), Get<float>(bytes.data() + at + 4), Get<float>(bytes.data() + at + 8));
	};
	particles::ProcessInfo info;
	info.interfacePos = point(0);
	info.handPos = point(12);
	info.cameraForward = point(24);
	info.direction = point(36);
	info.power = Get<float>(bytes.data() + 48);
	info.curl = Get<float>(bytes.data() + 52);
	info.enabled = bytes[56] != 0;
	return info;
}

particles::saved_draw::SavedDraw particle_codecs::SavedDrawOf(const std::array<uint8_t, 56>& bytes)
{
	particles::saved_draw::SavedDraw draw;
	std::memcpy(draw.matrix.data(), bytes.data(), sizeof(draw.matrix));
	draw.scale = Get<float>(bytes.data() + 48);
	draw.alpha = bytes[52];
	return draw;
}

void particle_codecs::ApplyAtomCore(const gamesave::AtomCoreRecord& record, particles::Atom& atom)
{
	atom.velocity = glm::vec3(record.point[0], record.point[1], record.point[2]);
	atom.birth = record.value;
	std::array<float, 12> rotation {};
	std::memcpy(rotation.data(), record.zoomer.data(), sizeof(rotation));
	for (glm::length_t row = 0; row < 3; ++row)
	{
		for (glm::length_t column = 0; column < 3; ++column)
		{
			atom.rotation[row][column] = rotation.at(static_cast<size_t>(row * 3 + column));
		}
	}
	atom.baseScale = record.floats[0];
	atom.ruleScale = record.floats[1];
	atom.stretch = record.floats[2];
	atom.position = glm::vec3(record.secondPoint[0], record.secondPoint[1], record.secondPoint[2]);
	atom.colour = {static_cast<uint8_t>(record.word >> 16U), static_cast<uint8_t>(record.word >> 8U),
	               static_cast<uint8_t>(record.word), static_cast<uint8_t>(record.word >> 24U)};
	atom.specular = record.secondWord;
	atom.drawOnFirstUpdate = (record.half & k_DrawOnFirstUpdate) != 0;
	atom.steps = (record.half & k_SteppedTwice) != 0 ? 2 : ((record.half & k_SteppedOnce) != 0 ? 1 : 0);
	atom.flags = (record.half & k_Deflected) != 0 ? (atom.flags | k_Deflected) : (atom.flags & ~uint32_t {k_Deflected});
	atom.visible = (record.half & k_InUse) != 0;
	atom.savedDraw[0] = SavedDrawOf(record.drawPosition);
	atom.savedDraw[1] = SavedDrawOf(record.secondDrawPosition);
	atom.frameRate = Get<float>(record.animation.data());
	atom.playAnim = record.animation[8] != 0;
	atom.savedAnimTail = {record.animation[10], record.animation[11]};
	atom.gravity = std::bit_cast<float>(record.fourthWord);
	atom.random = record.byte;
	// the drawn state the logic reads between the steps (the places of the last two steps, the turn and scale of this
	// one) as the atom's step leaves it: the places the saved matrices' translations, the turn the atom's own, or its
	// parent's drawn turn times it in a hierarchy, the scale this step's. The draw itself takes the saved matrices
	// (Effect::DrawnOf), not this state; the last step's turn is not saved and nothing reads it: it is this step's
	glm::mat3 turn = atom.rotation;
	if (atom.collection != nullptr && atom.collection->hierarchy && atom.collection->parent != nullptr)
	{
		turn = atom.collection->parent->current.rotation * atom.rotation;
	}
	const auto stateOf = [&atom, &turn](const particles::saved_draw::SavedDraw& draw, float frame) {
		particles::DrawState state;
		state.position = glm::vec3(draw.matrix[9], draw.matrix[10], draw.matrix[11]);
		state.rotation = turn;
		state.scale = draw.scale;
		state.stretch = atom.stretch;
		state.alpha = static_cast<float>(draw.alpha);
		state.frame = frame;
		return state;
	};
	atom.frame = std::bit_cast<float>(record.thirdWord);
	atom.previous = stateOf(atom.savedDraw[0], record.lastFloat);
	atom.current = stateOf(atom.savedDraw[1], atom.frame);
	atom.drawn = atom.steps > 0;
}

void particle_codecs::ApplyCollection(const gamesave::AtomCollectionRecord& record, particles::Collection& collection)
{
	collection.birth = record.value;
	collection.flags = static_cast<uint8_t>((collection.flags & ~k_Interpolated) | (record.byte & k_Interpolated));
	collection.alpha = static_cast<float>(record.lastByte);
	for (auto& slot : collection.modifiers)
	{
		slot.attached = slot.modifier != nullptr &&
		                std::ranges::any_of(record.persistents.elements, [&slot](const gamesave::PersistentId& id) {
			                return id.second == slot.modifier->fileIndex;
		                });
	}
}

void particle_codecs::ApplyEmitterData(const gamesave::EmitterCollectionRecord& record, particles::Collection::Slot& slot)
{
	slot.state.x = std::bit_cast<float>(record.words[0]);
	slot.state.y = static_cast<float>(static_cast<int32_t>(record.words[1]));
	slot.first = record.byte != 0;
}

void particle_codecs::ApplyWillowWispData(const gamesave::WillowWispCollectionRecord& record, particles::Collection::Slot& slot)
{
	slot.state = glm::vec4(record.point[0], record.point[1], record.point[2], 0.0f);
	slot.extra.x = std::bit_cast<float>(record.words[0]);
	slot.extra.y = std::bit_cast<float>(record.words[1]);
	slot.first = record.byte != 0;
}

void particle_codecs::ApplyHandSprinkleData(const gamesave::HandSprinkleCollectionRecord& record,
                                            particles::Collection::Slot& slot)
{
	slot.first = record.byte != 0;
	slot.extra.x = std::bit_cast<float>(record.word);
}

void particle_codecs::ApplyTargetAtomsData(const gamesave::TargetAtomsCollectionRecord& record,
                                           particles::Collection::Slot& slot)
{
	slot.extra.x = std::bit_cast<float>(record.word);
}

std::vector<particles::fireball_rules::TrailPlace> particle_codecs::TrailPlacesOf(const gamesave::TrailCollectionRecord& record)
{
	std::vector<particles::fireball_rules::TrailPlace> places;
	for (const auto& bytes : record.places.values)
	{
		std::array<float, 16> cells {};
		std::memcpy(cells.data(), bytes.data(), bytes.size());
		particles::fireball_rules::TrailPlace place;
		place.position = glm::vec3(cells[0], cells[1], cells[2]);
		place.scale = cells[3];
		for (glm::length_t row = 0; row < 3; ++row)
		{
			for (glm::length_t column = 0; column < 3; ++column)
			{
				place.rotation[row][column] = cells.at(static_cast<size_t>(4 + row * 3 + column));
			}
		}
		places.push_back(place);
	}
	return places;
}

void particle_codecs::ApplySoundAtomData(const gamesave::SoundAtomDataRecord& record, const particles::Modifier& rule,
                                         particles::Atom& atom)
{
	atom.data[&rule].x = record.byte != 0 ? 1.0f : 0.0f;
}

particles::SoundAction particle_codecs::SoundActionOf(const std::array<uint8_t, 24>& bytes)
{
	particles::SoundAction action;
	action.action = Get<int32_t>(bytes.data());
	action.surface = Get<int32_t>(bytes.data() + 4);
	action.size = Get<int32_t>(bytes.data() + 8);
	action.alignment = Get<int32_t>(bytes.data() + 12);
	action.fadeStep = Get<int32_t>(bytes.data() + 16);
	action.flags = bytes[20];
	return action;
}

void particle_codecs::ApplyMist(const gamesave::ParticleMistRecord& record, particles::Atom& atom)
{
	const auto unset = 0x01010101U * k_Unset;
	if (std::bit_cast<uint32_t>(record.point[0]) == unset && std::bit_cast<uint32_t>(record.point[1]) == unset &&
	    std::bit_cast<uint32_t>(record.point[2]) == unset)
	{
		atom.mistStampPlace.reset();
	}
	else
	{
		atom.mistStampPlace = glm::vec3(record.point[0], record.point[1], record.point[2]);
	}
	atom.mistStampAlpha = std::bit_cast<float>(record.lastWord);
}

uint8_t particle_codecs::CollectionFlags(const particles::Collection& collection)
{
	uint32_t flags = k_Unset & ~static_cast<uint32_t>(k_CollectionFlagsWritten);
	flags |= collection.hierarchy ? 0U : k_OwnCoordinates;
	flags |= (collection.flags & k_Interpolated) != 0 ? k_Interpolated : 0U;
	return static_cast<uint8_t>(flags);
}

gamesave::AtomCollectionRecord particle_codecs::CollectionValues(const particles::Collection& collection, uint32_t fileType,
                                                                 bool passesFrame)
{
	gamesave::AtomCollectionRecord record;
	for (const auto& slot : collection.modifiers)
	{
		if (slot.attached && slot.modifier != nullptr)
		{
			record.persistents.elements.push_back({.first = fileType, .second = slot.modifier->fileIndex});
		}
	}
	record.value = collection.birth;
	record.word = static_cast<uint32_t>(collection.group);
	record.byte = CollectionFlags(collection);
	record.lastWord = passesFrame ? k_PassesFrame : 0;
	record.lastByte = static_cast<uint8_t>(std::clamp(collection.alpha, 0.0f, 255.0f));
	return record;
}

gamesave::FadeOutAtomDataRecord particle_codecs::FadeOutDataValues(const particles::fade_out::AtomData& data,
                                                                   const particles::Modifier& rule, uint32_t fileType)
{
	gamesave::FadeOutAtomDataRecord record;
	record.persistent = {.first = fileType, .second = rule.fileIndex};
	record.byte = data.conditionMet ? 1 : 0;
	record.words = {std::bit_cast<uint32_t>(data.startTime), std::bit_cast<uint32_t>(data.startAlpha),
	                std::bit_cast<uint32_t>(data.startScale)};
	return record;
}

particles::fade_out::AtomData particle_codecs::FadeOutDataOf(const gamesave::FadeOutAtomDataRecord& record)
{
	return {.conditionMet = record.byte != 0,
	        .startTime = std::bit_cast<float>(record.words[0]),
	        .startAlpha = std::bit_cast<float>(record.words[1]),
	        .startScale = std::bit_cast<float>(record.words[2])};
}

particles::vortex_attract::AtomData particle_codecs::VortexAttractDataOf(const gamesave::VortexAttractAtomDataRecord& record)
{
	particles::vortex_attract::AtomData data;
	data.startHeight = std::bit_cast<float>(record.word);
	data.takenVelocity = glm::vec3(record.point[0], record.point[1], record.point[2]);
	data.indestructible = record.bytes[0] != 0;
	data.started = true;
	data.look = particles::vortex_attract::AtomData::TakenLook {.wasPaper = record.bytes[1] != 0,
	                                                            .wasDisappearing = record.bytes[2] != 0,
	                                                            .importance = std::bit_cast<float>(record.secondWord)};
	return data;
}

std::optional<particles::mana_path::AtomData> particle_codecs::ManaPathDataOf(const gamesave::RuleDataRecord& record)
{
	if (record.type != k_ManaPathAtomDataType)
	{
		return std::nullopt;
	}
	particles::mana_path::AtomData data;
	auto& path = data.path;
	path.to = VectorOfSaved(record.PointAt(k_ManaPathEnd));
	path.from = VectorOfSaved(record.PointAt(k_ManaPathStart));
	path.side = VectorOfSaved(record.PointAt(k_ManaPathSide));
	path.cubic = VectorOfSaved(record.PointAt(k_ManaPathCubic));
	path.square = VectorOfSaved(record.PointAt(k_ManaPathSquare));
	path.linear = VectorOfSaved(record.PointAt(k_ManaPathLinear));
	path.constant = VectorOfSaved(record.PointAt(k_ManaPathConstant));
	path.length = record.Float(k_ManaPathLength).value_or(0.0f);
	path.amplitude = record.Float(k_ManaPathAmplitude).value_or(0.0f);
	path.frequency = record.Float(k_ManaPathFrequency).value_or(0.0f);
	path.phase = record.Float(k_ManaPathPhase).value_or(0.0f);
	data.progress = record.Float(k_ManaPathProgress).value_or(0.0f);
	return data;
}

std::optional<particles::volume_glow::AtomData> particle_codecs::VolumeGlowDataOf(const gamesave::RuleDataRecord& record)
{
	if (record.type != k_VolumeGlowAtomDataType)
	{
		return std::nullopt;
	}
	return particles::volume_glow::AtomData {.object = entt::null, .validatedTurn = record.Word(k_VolumeGlowTurn).value_or(0)};
}

std::optional<particles::PlasmaCommand> particle_codecs::PlasmaDataOf(const gamesave::RuleDataRecord& record)
{
	if (record.type != k_PlasmaAtomDataType)
	{
		return std::nullopt;
	}
	return particles::PlasmaCommand {
	    .start = VectorOfSaved(record.PointAt(k_PlasmaStart)),
	    .end = VectorOfSaved(record.PointAt(k_PlasmaEnd)),
	    .startTangent = VectorOfSaved(record.PointAt(k_PlasmaStartTangent)),
	    .endTangent = VectorOfSaved(record.PointAt(k_PlasmaEndTangent)),
	    .life = record.Float(k_PlasmaLife).value_or(0.0f),
	    .speed = record.Float(k_PlasmaSpeed).value_or(0.0f),
	    .alpha = record.Byte(k_PlasmaAlpha).value_or(0),
	};
}

std::optional<particles::belief_sprite::AtomData> particle_codecs::BeliefSpriteDataOf(const gamesave::RuleDataRecord& record)
{
	if (record.type != k_BeliefSpriteAtomDataType)
	{
		return std::nullopt;
	}
	return particles::belief_sprite::AtomData {.start = VectorOfSaved(record.PointAt(k_BeliefStart)),
	                                           .alpha = record.Byte(k_BeliefAlpha).value_or(0)};
}

std::optional<particles::creature_spell::WispsData>
particle_codecs::CreatureSpellWispsOf(const gamesave::RuleDataRecord& record)
{
	if (record.type != k_CreatureSpellWispsDataType)
	{
		return std::nullopt;
	}
	return particles::creature_spell::WispsData {
	    .creature = VectorOfSaved(record.PointAt(k_WispsCreature)),
	    .hand = VectorOfSaved(record.PointAt(k_WispsHand)),
	    .first = record.Byte(k_WispsFirstAgain).value_or(0) != 0,
	    .due = record.Float(k_WispsDue).value_or(0.0f),
	    .made = static_cast<int>(record.Word(k_WispsMade).value_or(0)),
	};
}

std::optional<particles::creature_spell::WispData> particle_codecs::CreatureSpellWispOf(const gamesave::RuleDataRecord& record)
{
	if (record.type != k_CreatureSpellWispDataType)
	{
		return std::nullopt;
	}
	return particles::creature_spell::WispData {
	    .theta = record.Float(k_WispTheta).value_or(0.0f),
	    .phi = record.Float(k_WispPhi).value_or(0.0f),
	    .thetaRate = record.Float(k_WispThetaRate).value_or(0.0f),
	    .phiRate = record.Float(k_WispPhiRate).value_or(0.0f),
	    .scale = record.Float(k_WispScale).value_or(0.0f),
	};
}

std::optional<particles::orient_velocity::AtomData>
particle_codecs::OrientWithVelocityOf(const gamesave::RuleDataRecord& record)
{
	if (record.type != k_OrientWithVelocityAtomDataType)
	{
		return std::nullopt;
	}
	return particles::orient_velocity::AtomData {.first = record.Byte(k_OrientFirst).value_or(1) != 0,
	                                             .velocity = VectorOfSaved(record.PointAt(k_OrientVelocity)),
	                                             .rate = record.Float(k_OrientRate).value_or(0.0f)};
}

std::optional<particles::creature_spell::CreatureHeld> particle_codecs::CreatureSpellOf(const gamesave::RuleDataRecord& record)
{
	if (record.type != k_CreatureSpellDataType)
	{
		return std::nullopt;
	}
	return particles::creature_spell::CreatureHeld {.first = record.Byte(k_HeldFirst).value_or(1) != 0,
	                                                .creature = entt::null,
	                                                .validatedTurn = record.Word(k_HeldTurn).value_or(0)};
}

std::optional<particles::creature_spell::AtomsMade>
particle_codecs::CreatureSpellAtomsMadeOf(const gamesave::RuleDataRecord& record)
{
	if (record.type != k_CreatureSpellCompassionDataType && record.type != k_CreatureSpellFreezeDataType)
	{
		return std::nullopt;
	}
	const size_t counts = record.type == k_CreatureSpellFreezeDataType ? 2 : 1;
	return particles::creature_spell::AtomsMade {.due = record.Float(counts).value_or(0.0f),
	                                             .made = static_cast<int32_t>(record.Word(counts + 1).value_or(0))};
}

std::optional<bool> particle_codecs::CreatureSpellItchOf(const gamesave::RuleDataRecord& record)
{
	if (record.type != k_CreatureSpellItchDataType)
	{
		return std::nullopt;
	}
	return record.Byte(k_ItchFirst).value_or(1) != 0;
}

std::optional<particles::sphere_angles::AtomData> particle_codecs::SphereAnglesOf(const gamesave::RuleDataRecord& record)
{
	if (record.type != k_SphereTracerAtomDataType && record.type != k_ForestPathAtomDataType)
	{
		return std::nullopt;
	}
	return particles::sphere_angles::AtomData {.first = record.Byte(k_AnglesFirst).value_or(1) != 0,
	                                           .theta = record.Float(k_AnglesTheta).value_or(0.0f),
	                                           .phi = record.Float(k_AnglesPhi).value_or(0.0f),
	                                           .psi = record.Float(k_AnglesPsi).value_or(0.0f)};
}

std::optional<glm::vec3> particle_codecs::FlockingOf(const gamesave::RuleDataRecord& record)
{
	if (record.type != k_FlockingDataType)
	{
		return std::nullopt;
	}
	return VectorOfSaved(record.PointAt(k_FlockVelocity));
}

std::optional<bool> particle_codecs::CloudMoverOf(const gamesave::RuleDataRecord& record)
{
	if (record.type != k_CloudMoverDataType)
	{
		return std::nullopt;
	}
	return record.Byte(0).value_or(1) != 0;
}

std::optional<particles::storm::TornadoFlyingData> particle_codecs::TornadoFlyingOf(const gamesave::RuleDataRecord& record)
{
	if (record.type != k_TornadoFlyingAtomDataType)
	{
		return std::nullopt;
	}
	return particles::storm::TornadoFlyingData {.state = static_cast<int>(record.Word(0).value_or(2)),
	                                            .blend = record.Float(1).value_or(0.0f),
	                                            .speed = record.Float(2).value_or(0.0f)};
}

std::optional<particles::storm::TornadoEmitterData> particle_codecs::TornadoEmitterOf(const gamesave::RuleDataRecord& record)
{
	if (record.type != k_TornadoEmitterDataType)
	{
		return std::nullopt;
	}
	return particles::storm::TornadoEmitterData {.made = record.Float(0).value_or(0.0f),
	                                             .owed = record.Float(1).value_or(0.0f)};
}

std::optional<particles::storm::TornadoData> particle_codecs::TornadoOf(const gamesave::RuleDataRecord& record)
{
	if (record.type != k_TornadoDataType)
	{
		return std::nullopt;
	}
	const auto flag = [&record](size_t field) { return record.Byte(field).value_or(0) != 0; };
	return particles::storm::TornadoData {.first = flag(0),
	                                      .base = VectorOfSaved(record.PointAt(1)),
	                                      .top = VectorOfSaved(record.PointAt(2)),
	                                      .baseVelocity = VectorOfSaved(record.PointAt(3)),
	                                      .topVelocity = VectorOfSaved(record.PointAt(4)),
	                                      .tornadoScale = record.Float(6).value_or(1.0f),
	                                      .strength = record.Float(7).value_or(1.0f),
	                                      .alpha = record.Byte(8).value_or(0),
	                                      .fade = record.Float(9).value_or(1.0f),
	                                      .closing = flag(10)};
}

std::optional<bool> particle_codecs::SoundRemovalOf(const gamesave::RuleDataRecord& record)
{
	if (record.type != k_SoundRemovalAtomDataType)
	{
		return std::nullopt;
	}
	return record.Byte(0).value_or(0) != 0;
}

std::optional<bool> particle_codecs::SubCollectionsOf(const gamesave::RuleDataRecord& record)
{
	if (record.type != k_SubCollectionsAtomDataType)
	{
		return std::nullopt;
	}
	return record.Byte(0).value_or(0) != 0;
}

std::optional<particles::fireball_rules::BallData> particle_codecs::FireBallDataOf(const gamesave::RuleDataRecord& record)
{
	if (record.type != k_FireBallAtomDataType)
	{
		return std::nullopt;
	}
	return particles::fireball_rules::BallData {.firstUpdate = record.Byte(k_FireBallFirst).value_or(1) != 0,
	                                            .validatedTurn = record.Word(k_FireBallTurn).value_or(0),
	                                            .ball = entt::null};
}

std::optional<particles::storm_flash::State> particle_codecs::LightningSpriteOf(const gamesave::RuleDataRecord& record)
{
	if (record.type != k_LightningSpriteDataType)
	{
		return std::nullopt;
	}
	return particles::storm_flash::State {.first = record.Byte(4).value_or(1) != 0,
	                                      .nextTime = record.Float(2).value_or(0.0f),
	                                      .emitted = static_cast<int>(record.Word(3).value_or(0)),
	                                      .striking = record.Byte(1).value_or(0) != 0,
	                                      .strikeAge = record.Float(0).value_or(0.0f)};
}

particles::vortex_attract::CarriedObject particle_codecs::CarriedObjectOf(const gamesave::ParticleGameObjectRecord& record)
{
	return {.object = entt::null, .player = -1, .availableTurn = record.word, .letGo = false, .byte = record.byte};
}

particles::creature_spell::CreatureRefData particle_codecs::CreatureRefOf(const gamesave::ParticleCreatureRefRecord& record)
{
	return {.enabled = record.renderByte != 0, .creature = entt::null, .validatedTurn = record.turn};
}
