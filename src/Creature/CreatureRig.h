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
#include <span>
#include <string>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/SkeletalAnimation.h"
#include "Creature/CreatureAudio.h"
#include "Creature/CreatureEyes.h"
#include "Creature/CreatureHair.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureTattoo.h"
#include "Creature/TattooEditor.h"

namespace openblack::creature
{
/// What moves a species' body, from its .cbn file in Data/CTR: the animations of its base mesh and of its evil, good,
/// thin and fat meshes, and where its eyes sit on it
struct CreatureRig
{
	/// The meshes a body is blended from, in the order the file names them
	enum class Mesh : uint8_t
	{
		Base,
		Evil,
		Good,
		Thin,
		Fat,
		Weak,
		Strong,
	};
	static constexpr size_t k_MeshCount = 7;
	/// The meshes that may have animations of their own: the base, evil, good, thin and fat
	static constexpr size_t k_AnimatedMeshCount = 5;

	/// The base mesh's name, which says the species
	std::string baseMeshName;
	/// Each mesh's file name in Data/CreatureMesh, without its extension, empty for the meshes the species lacks
	std::array<std::string, k_MeshCount> meshNames;
	/// Whether the species has each mesh other than the base
	std::array<bool, k_MeshCount> hasMesh {};
	/// Each animated mesh's animations, in the order of the creature spec file, absent where it has none of its own
	std::array<std::vector<std::optional<skeletal_animation::Animation>>, k_AnimatedMeshCount> animations;

	/// A point on the body the eyes are placed by
	struct EyePoint
	{
		bool enabled;
		/// How far into the body the point is sunk
		float depth;
		/// The triangle's vertices in each mesh, in the space of the bone that moves each, and where between them
		std::array<std::array<glm::vec3, 3>, k_MeshCount> vertices;
		std::array<uint32_t, 3> bones;
		float u;
		float v;
		/// The colour of the base mesh's skin there, opaque 0xAARRGGBB: each of its 4-bit channels shifted up by 4
		uint32_t skinColour;
	};
	struct Eyes
	{
		/// The species' eye size
		float scale;
		/// The left and right eyes, then the points the left and right eyelids are turned towards
		std::array<EyePoint, 4> points;
		creature_eyes::LidAngles lidAngles;
	};
	std::optional<Eyes> eyes;

	/// A strand of hair, rooted on a triangle of the body
	struct HairStrand
	{
		/// Whether the strand grows out turned from the surface by its angles, rather than straight out of it
		bool turned;
		/// The x, y and z angles it is turned by when neutral, evil and good
		std::array<glm::vec3, 3> angles;
		/// The triangle's vertices in each mesh, in the space of the bone that moves each, and where between them
		std::array<std::array<glm::vec3, 3>, k_MeshCount> vertices;
		std::array<uint32_t, 3> bones;
		float u;
		float v;
	};
	/// A tuft of strands that look and move alike
	struct HairGroup
	{
		/// The points each strand is made of
		uint32_t segmentCount;
		/// Drawn with the hair texture rather than in the strands' colour alone
		bool textured;
		creature_hair::Variants looks;
		std::vector<HairStrand> strands;
		/// Its place among the file's groups, the number an animation's moment shows or hides it by (a group the file
		/// gives no strands is not kept, but still counts)
		uint32_t fileIndex {0};
	};
	/// The species' hair, none for most
	std::vector<HairGroup> hairGroups;

	/// Where the species' tattoos go, for species that have places for them
	std::optional<creature_tattoo::Sites> tattooSites;
	/// The triangle of the body a place for tattoos is on, and where on it, found once as the species is loaded: which
	/// triangle holds a place does not depend on the pose
	struct TattooPoint
	{
		/// The triangle's vertices in each mesh, in the space of the bone that moves each, and where between them
		std::array<std::array<glm::vec3, 3>, k_MeshCount> vertices;
		std::array<uint32_t, 3> bones;
		float u;
		float v;
	};
	/// Each place's triangle, none where the place is not used or no triangle of the base mesh holds it
	std::array<std::optional<TattooPoint>, creature_tattoo::k_SlotCount> tattooPoints {};

	/// The bones the species acts with, counted in its meshes, and the moments its object animations take hold or let
	/// go, in milliseconds from their starts. Each left side bone is the mirror of the right.
	struct ActionPoints
	{
		uint32_t rightHand;
		uint32_t rightFoot;
		uint32_t rightArmpit;
		uint32_t belly;
		uint32_t head;
		uint32_t groin;
		float pickUpMs;
		float destroyMs;
		float discardMs;
		float eatMs;
		float throwMs;
		float putDownMs;
		/// When the catching hand closes, in the four catching animations
		float catchMs {0.0f};
		/// When the reaching hand takes what the player's hand gives it
		float takeFromHandMs {0.0f};
		/// When the foot meets what it kicks
		float kickMs {0.0f};
	};
	std::optional<ActionPoints> actionPoints;

	/// The sounds placed on moments of each animation of the base mesh, in the order of the creature spec file
	std::vector<std::vector<creature_audio::SoundEvent>> soundEvents;
	/// What the species is to its voice bank, the object key its voice is looked up by
	int32_t soundObject {0};
	/// The voice bank the file names, without its extension, empty when it names none
	std::string soundBankName;
	/// The last numbers of the species' file and its creature block's version, which give each bone's mirror bone
	std::vector<int32_t> fileTail;
	uint32_t creatureVersion {0};
	/// The bone of the right eye, when the file names one; the left is its mirror
	std::optional<uint32_t> rightEye;
	/// The bone at the creature's collar that a leash is tied to, when the file names one
	std::optional<uint32_t> leashBone;

	/// Each bone's mirror bone, the bone on the other side of the body, for the species' mesh of so many bones: the
	/// table that ends its file. None when the file has none that fits.
	[[nodiscard]] std::optional<std::vector<uint32_t>> MirrorBones(size_t boneCount) const;
	/// An animated mesh's animation, falling back on the base's when the mesh has none of its own
	[[nodiscard]] const skeletal_animation::Animation* GetAnimation(Mesh mesh, size_t index) const;
};

/// Each primitive's vertex positions in one mesh, in the space of the bone that moves each, every submesh's primitives in
/// turn
using PrimitivePositions = std::vector<std::vector<glm::vec3>>;

/// The triangles the places for a species' tattoos are on, searched for on its base mesh's primitives (every submesh's
/// in turn). The vertices in each mesh are taken at the triangle's own indices; a mesh the species lacks, or one
/// without the vertex, takes the base's. positions holds the base's first.
[[nodiscard]] std::array<std::optional<CreatureRig::TattooPoint>, creature_tattoo::k_SlotCount>
FindTattooPoints(const creature_tattoo::Sites& sites, std::span<const creature_tattoo_editor::TexturedPrimitive> base,
                 const std::array<PrimitivePositions, CreatureRig::k_MeshCount>& positions);

/// The places for tattoos as the body is posed this frame, in the world: each place's triangle posed as the body is
/// drawn, and the point on it and the way into the body there. The places without a triangle are left out.
[[nodiscard]] std::vector<creature_tattoo_editor::SitePoint>
PosedTattooPoints(std::span<const std::optional<CreatureRig::TattooPoint>> points, const creature_morph::Morph& morph,
                  std::span<const glm::mat4> boneMatrices, const glm::mat4& placement);

/// The matrix the renderer places a creature's mesh with: scaled, then turned and moved
[[nodiscard]] glm::mat4 PlacementMatrix(const glm::vec3& position, const glm::mat3& rotation, const glm::vec3& scale);

/// A triangle of the body as it is drawn this frame, in the world: each vertex blended between the meshes as the body
/// is, then placed by its bone's posed matrix and the creature's placement. vertices are the triangle's in each mesh,
/// in the space of the bone that moves each.
[[nodiscard]] std::array<glm::vec3, 3>
PosedTriangle(const std::array<std::array<glm::vec3, 3>, CreatureRig::k_MeshCount>& vertices,
              const std::array<uint32_t, 3>& bones, const creature_morph::Morph& morph, std::span<const glm::mat4> boneMatrices,
              const glm::mat4& placement);

/// The world matrix of a bone as posed this frame, the creature's placement included; the placement alone for a bone
/// the pose lacks
[[nodiscard]] glm::mat4 PosedBone(uint32_t bone, std::span<const glm::mat4> boneMatrices, const glm::mat4& placement);

/// A bone a creature's drawn size makes bigger, and by how much (creature_morph::PartScalesAt)
struct GrownJoint
{
	uint32_t joint;
	float scale;
};
/// The bones of a body of boneCount bones that its drawn size makes bigger, in the order they are scaled: the collar
/// bone the leash is tied to by the head's scale, so the whole head grows; the right hand and its mirror, then the
/// right foot and its mirror, by the limbs'; the right eye's bone and its mirror by the eyes'. A bone the species does
/// not name, or without a mirror, is left out; one named twice is scaled twice
[[nodiscard]] std::vector<GrownJoint> GrownJoints(const CreatureRig& rig, size_t boneCount, float drawnSize);

/// Poses with the bones a drawn size makes bigger scaled (GrownJoints), as each pose of the body is sampled, before the
/// head's turn, the sway, the face and the gestures are added
void GrowParts(std::span<skeletal_animation::Pose> poses, const CreatureRig& rig, float drawnSize);
} // namespace openblack::creature
