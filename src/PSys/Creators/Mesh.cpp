/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "Mesh.h"

#include <cmath>

#include <algorithm>
#include <map>
#include <mutex>
#include <string>

#include <entt/core/hashed_string.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/Billboard.h"
#include "3D/FrameAnim.h"
#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "3D/SkeletalPose.h"
#include "Camera/Camera.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "PSys/PSysFile.h"
#include "PSys/PSysManager.h"
#include "PSys/PSysRegistry.h"
#include "PSys/Rules/ExplodeObject.h"
#include "PSys/SoundAction.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
/// MeshEnumProperty: the MSH_* name in Data\AllMeshes.h (LHParseFile::FindEnumVal, as the sound actions), -1 if absent
int32_t MeshEnumValue(std::string_view name)
{
	static std::map<std::string, int32_t, std::less<>> names;
	static std::once_flag once;
	std::call_once(once, [] {
		if (!Locator::filesystem::has_value())
		{
			return;
		}
		auto& fileSystem = Locator::filesystem::value();
		try
		{
			const auto bytes = fileSystem.ReadAll(fileSystem.GetPath<filesystem::Path::Data>() / "AllMeshes.h");
			for (auto& [key, value] : ParseEnumHeader(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size())))
			{
				names.insert_or_assign(std::move(key), value);
			}
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "PSys: cannot read Data\\AllMeshes.h: {}", e.what());
		}
	});
	const auto it = names.find(name);
	return it != names.end() ? it->second : -1;
}

/// A spell file's path (".\Data\Spells\Meshes\X.l3d") under the data folder ("Spells/Meshes/X.l3d")
std::string DataRelativePath(std::string path)
{
	std::replace(path.begin(), path.end(), '\\', '/');
	if (path.starts_with("./"))
	{
		path = path.substr(2);
	}
	if (path.size() > 5 && (path.starts_with("Data/") || path.starts_with("data/")))
	{
		path = path.substr(5);
	}
	return path;
}

/// GJUtils::GetSharedMesh 0x57DFB0: a mesh file (".\Data\Spells\Meshes\X.l3d"), loaded once by its name
entt::id_type SharedMesh(std::string path)
{
	path = DataRelativePath(std::move(path));
	const auto id = entt::hashed_string(("psys/" + path).c_str()).value();
	if (!Locator::resources::has_value() || !Locator::filesystem::has_value())
	{
		return id;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (meshes.Contains(id))
	{
		return id;
	}
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		meshes.Load(id, resources::L3DLoader::FromDiskTag {},
		            fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / path));
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "PSys: mesh {}: {}", path, e.what());
	}
	return id;
}

/// RenderParticleGJMesh::DrawAt 0x67C150 of an exploded piece (PSys/Rules/ExplodeObject.h): the GJ mesh through the
/// drawn PSR matrix (0x67C279..0x67C30A, the same matrix as the mesh atoms'), every vertex of the colour of the DrawData
/// times the land light (+0x21, 0x67C175..0x67C1F6) since the GJ mesh has no colours of its own (+0x24 != the vertex
/// count: 0x67C47F..0x67C4C4), then lit by the model light ([0xC029C0] = 1: 0x67C4CE..0x67C6B2, I = fistp(255 n.l) with
/// the light [0xEA9E90] through the inverse of the drawn matrix, f = I < 0 ? amb : amb + ((255 - amb) I >> 8), RGB x f
/// >> 8: vs_object's PSys mesh atom branch, the colour in the third column). [0xD4EC08] (the second light) is 0. The
/// DrawData alpha != 255 draws through the alpha render modes (0x67C9BA..0x67C9C0, 0xC387C8): the translucent pass.
/// Draw3DWorldTriangle 0x81C090 with the primitive's material: no haze (fn_007FEB30), no specular.
mesh_atoms::Instance PieceInstance(const Effect::DrawAtom& atom, entt::id_type meshId)
{
	glm::mat3 axes = atom.rotation * atom.scale;
	axes[1] *= atom.stretch;
	glm::mat4 model(axes);
	model[3] = glm::vec4(atom.position, 1.0f);
	// the DrawData colour 0xAARRGGBB: the atom's colour and its alpha byte
	const auto alphaByte = static_cast<uint32_t>(std::clamp(atom.alpha, 0.0f, 255.0f));
	const uint32_t argb = (alphaByte << 24) | (static_cast<uint32_t>(atom.colour[0]) << 16) |
	                      (static_cast<uint32_t>(atom.colour[1]) << 8) | atom.colour[2];
	const uint32_t lit = explode_object::LitColour(argb, atom.position);
	const std::array<uint8_t, 3> colour {static_cast<uint8_t>(lit >> 16), static_cast<uint8_t>(lit >> 8),
	                                     static_cast<uint8_t>(lit)};
	const float alpha = static_cast<float>(lit >> 24) / 255.0f;
	return {meshId, model, alpha, glm::vec2(0.0f), alphaByte != 255u, false, colour, false};
}

/// fn_006A9570 with AnimEnum -1: the AnimFileName (".\Data\SPELLS\Anims\X.anm") loaded by fn_00839900 (the whole
/// file, LHLoadData 0x83993C, then the fix-ups fn_0083A610), when LHFileLength finds it. (openblack) once per path,
/// kept by the animation manager; 0 when there is no file
entt::id_type SharedAnim(std::string path)
{
	path = DataRelativePath(std::move(path));
	if (path.empty() || path == "NULL_STRING")
	{
		return 0;
	}
	const auto id = entt::hashed_string(("psys/" + path).c_str()).value();
	if (!Locator::resources::has_value() || !Locator::filesystem::has_value())
	{
		return id;
	}
	auto& animations = Locator::resources::value().GetAnimations();
	if (animations.Contains(id))
	{
		return id;
	}
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		animations.Load(id, resources::L3DAnimLoader::FromDiskTag {},
		                fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / path));
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "PSys: animation {}: {}", path, e.what());
		return 0;
	}
	return id;
}

/// The clip of an animated mesh creator, if it is loaded. (openblack guard) without a clip (missing file) or with a
/// mesh without bones the atom is drawn in its rest pose; the original would read the null clip (fn_006A9570 leaves
/// +0x40 at 0, GetCycleTimeFromFrame 0x6C85F0 reads [0 + 0x20])
const L3DAnim* CreatorClip(const MeshCreator& creator)
{
	if (!creator.animated || creator.animId == 0 || !Locator::resources::has_value())
	{
		return nullptr;
	}
	const auto& animations = Locator::resources::value().GetAnimations();
	return animations.Contains(creator.animId) ? &*animations.Handle(creator.animId) : nullptr;
}

std::unique_ptr<Creator> MakeMeshCreator(const Object& object)
{
	auto creator = std::make_unique<MeshCreator>();
	ReadCreatorProperties(object, *creator);
	creator->kind = Creator::Kind::Mesh;
	creator->animTextured = object.className == "ParticleMeshCreatorAnimTextured";
	// fn_006A8CC0: MeshEnum != -1 -> LH3DMesh::MeshPack[MeshEnum] (0 outside the pack), else the shared mesh file
	const int32_t meshEnum = MeshEnumValue(object.String("MeshEnum"));
	if (meshEnum >= 0)
	{
		creator->meshId = resources::HashIdentifier(static_cast<MeshId>(meshEnum));
	}
	else if (const auto file = object.String("MeshFileName"); !file.empty())
	{
		creator->meshId = SharedMesh(file);
	}
	creator->faceCamera = object.Bool("FaceCamera", false);
	creator->faceCameraSprite = object.Bool("FaceCameraSprite", false);
	// ParticleBaseMeshCreator ctor 0x6A87C0: MeshEnum -1, HeightStretch 1, FaceCamera / FaceCameraSprite / the pulse 0
	creator->heightStretch = object.Float("HeightStretch", 1.0f);
	creator->scriptHighlightPulse = object.Bool("UseScriptHightlightPulse", false);
	// ParticleMeshCreator ctor 0x6A8960: MeshChangeMaterialProps 1, double-sided 1, the rest 0 (AnimTextured's ctor
	// 0x6A8BB0 calls the base 0x6A87C0 and sets the same, 0x6A8BBD..0x6A8BF6). With MeshChangeMaterialProps,
	// fn_0057E1D0 gives every material of the mesh GJUtils::SetMaterialProperties 0x57E120 (+0x55 additive, +0x56 Z
	// write, +0x57 double-sided); without it the L3D materials are drawn as they are (opaque).
	creator->changeMaterialProps = object.Bool("MeshChangeMaterialProps", true);
	if (object.className == "ParticleAnimCreator")
	{
		// ParticleAnimCreator (DefineProperties 0x6B3D70, ctor 0x6A9200: +0x90 UseAdditiveAlpha 0, +0x91 Z 0, +0x92
		// double-sided 1, +0x93 / +0x94 1, no MeshChangeMaterialProps property): fn_006A95E0 gives the pack mesh
		// SetMaterialProperties when +0x93 (0x6A9602..0x6A9617), the file goes through fn_0057D420 with them
		creator->changeMaterialProps = true;
		// The clip (fn_006A9570, set on each particle's object by vt 0x180 fn_008185C0 at 0x6A97A2) and the ctor's
		// defaults (0x6A93A7..0x6A93B5): SpeedUpFactor 1, PlayAnim 0, RandomiseInitFrame 0. Every spell file (SF_Forest,
		// SF_Butterflies, SF_ButterfliesOnObject) names an AnimFileName and no AnimEnum. (pendiente) AnimEnum (+0x7C,
		// LH3DAnim::AnimPack [0xEDD508], pack[0] out of range, 0x6A957C..0x6A959A); the blend of DrawAt
		// 0x67A946..0x67A9B1 to MeshFileName1/2 (+0x38 / +0x3C, both needed) between FrameToStartBlend and
		// FrameToEndBlend (vt 0xDC fn_007F9A80): NULL_STRING in every file; UseSuperSortedPolys (vt 0xD4, 0 everywhere),
		// UseDynamicLighting (vt 0x58 fn_008168C0), UseGlobalAlpha (vt 0x48 fn_007F9D60, the object's +4 bit 0x80; 1
		// everywhere, the alpha is drawn as for every mesh atom) and NeverClip (vt 0x98 fn_007F98E0 with !NeverClip)
		creator->animated = true;
		creator->animId = SharedAnim(object.String("AnimFileName"));
		creator->speedUpFactor = object.Float("SpeedUpFactor", 1.0f);
		creator->animPlay = object.Bool("PlayAnim", false);
		creator->animRandomInitFrame = object.Bool("RandomiseInitFrame", false);
		if (creator->animId == 0)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "PSys: {} {}: no animation ({})", object.className, object.name,
			                   object.String("AnimFileName"));
		}
	}
	creator->additive = creator->changeMaterialProps && object.Bool("UseAdditiveAlpha", false);
	creator->writeDepth = object.Bool("MaterialUpdateZBuffer", false);
	creator->doubleSided = object.Bool("MaterialSetDoubleSided", true);
	creator->neverClip = object.Bool("NeverClip", false);
	// DrawWithLandscapeColor: ParticleMeshCreator's DefineProperties 0x6B38B0 reads it into +0x5E (0x6B390E; CreateParticle
	// 0x6A8B82 puts it in the particle's +0x24 bit 1, which Particle3DObj::DrawAt 0x67A00C tests for fn_0080BEC0), and
	// ParticleMeshCreatorAnimTextured's DefineProperties 0x6B3970 reads it too, into its own +0x84 (its last property,
	// 0x6B3AEF..0x6B3AFD; ctor default 0 at 0x6A8BF6), which its CreateParticle 0x6A8F0D..0x6A8F20 puts in the same
	// bit 1. So the tornado funnel's DrawWithLandscapeColor=1 is honoured
	creator->drawWithLandscapeColour = object.Bool("DrawWithLandscapeColor", false);
	creator->drawCutByPlane = object.Bool("DrawCutByPlane", false);
	if (creator->animTextured)
	{
		// ctor 0x6A8BB0: 64 x 64, 1 frame at 1 fps, FrameRateMax 10, StretchY 1
		creator->textureWidth = std::max(1, object.Int("TextureWidth", 64));
		creator->textureHeight = std::max(1, object.Int("TextureHeight", 64));
		creator->slideU = object.Bool("SlideU", false);
		creator->slideV = object.Bool("SlideV", false);
		creator->randomiseInitFrame = object.Bool("RandomiseInitFrame", false);
		creator->randomiseFrameRate = object.Bool("RandomiseFrameRate", false);
		creator->frameRate = object.Float("FrameRate", 1.0f);
		creator->frameRateMax = object.Float("FrameRateMax", 10.0f);
		creator->numFrames = std::max(1, object.Int("NumFrames", 1));
		creator->playAnimation = object.Bool("PlayAnim", false);
		creator->initialOffsetFrac = object.Float("InitialOffsetFrac", 0.0f);
		creator->stretchY = object.Float("StretchY", 1.0f);
	}
	if (creator->meshId == 0 || (Locator::resources::has_value() && !Locator::resources::value().GetMeshes().Contains(creator->meshId)))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "PSys: {} {}: no mesh ({} / {})", object.className, object.name,
		                   object.String("MeshEnum"), object.String("MeshFileName"));
	}
	return creator;
}
} // namespace

float openblack::psys::AnimFrameRate(int32_t clipMs, float speedUpFactor) noexcept
{
	// (openblack guard) a clip of 0 ms: no rate, where the original divides by it
	if (clipMs <= 0)
	{
		return 0.0f;
	}
	return 1000.0f / static_cast<float>(clipMs) * speedUpFactor * 1000.0f;
}

int32_t openblack::psys::AnimCycleTime(int32_t clipMs, int frame) noexcept
{
	return clipMs * frame / k_AnimFrames;
}

void MeshCreator::InitAtom(Effect& effect, Atom& atom) const
{
	if (animated)
	{
		// ParticleAnimCreator::CreateParticle (vt 0x10 0x6A98C0 -> fn_006A97F0): the particle's own object of type 2
		// (vt 0x1C, CreateLH3DObject 0x6A9760), then on the atom (0x6A9843..0x6A98AD) the rate +0x110 from the clip's
		// length (AnimFrameRate), +0x114 = 1000 frames, +0x118 PlayAnim (+0xA1) and +0x119 LoopAnim (+0xC), and with
		// RandomiseInitFrame (+0xA2) SetFrame 0x674100 (+0x108 and +0x10C) of PSysRand(1000) 0x6729E0. (aproximado) the
		// PSysRand of the function pointer [0xD4E0BC] by the effect's generator, as every random of this PSys port
		const auto* clip = CreatorClip(*this);
		atom.frameRate = AnimFrameRate(clip != nullptr ? clip->GetDurationMs() : 0, speedUpFactor);
		atom.playAnim = animPlay;
		if (animRandomInitFrame)
		{
			atom.frame = std::floor(effect.Random(static_cast<float>(k_AnimFrames)));
		}
		return;
	}
	if (!animTextured)
	{
		return; // ParticleMeshCreator::CreateParticle 0x6A8B00: the Particle3DObj only
	}
	// ParticleMeshCreatorAnimTextured::CreateParticle 0x6A8DA0
	float rate = frameRate;
	if (randomiseFrameRate && frameRateMax != frameRate)
	{
		rate = frameRate + effect.Random(frameRateMax - frameRate);
	}
	atom.frame = 0.0f;
	if (slideU || slideV)
	{
		// the slide runs over 1000 frames: the rate x 1000, and InitialOffsetFrac is the first frame (0..999)
		rate *= 1000.0f;
		if (initialOffsetFrac != 0.0f)
		{
			atom.frame = static_cast<float>(static_cast<int>(std::clamp(initialOffsetFrac * 1000.0f, 0.0f, 999.0f)));
		}
	}
	if (randomiseInitFrame)
	{
		atom.frame = std::floor(effect.Random(static_cast<float>(FramesPerAtom()))); // PSysRand(N)
	}
	atom.frameRate = rate;         // +0x110
	atom.playAnim = playAnimation; // +0x118 PlayAnim
	atom.stretch = stretchY;       // +0x7C
}

glm::vec2 MeshCreator::UvOffset(int frame) const
{
	// Particle3DObjAnimTextured::DrawAt 0x67A530 (frame_anim::AnimTexturedCell): cells of W x H pixels in rows of
	// 256 / W, or the slide over the 1000 frames
	return graphics::frame_anim::AnimTexturedCell(frame, {textureWidth, textureHeight, slideU, slideV, FramesPerAtom()});
}

bool mesh_atoms::Any()
{
	for (const auto& drawable : manager::Collect(Creator::Kind::Mesh))
	{
		for (const auto& atom : drawable.atoms)
		{
			const auto* creator = dynamic_cast<const MeshCreator*>(atom.creator);
			if (creator != nullptr && creator->meshId != 0)
			{
				return true;
			}
		}
	}
	return false;
}

std::vector<mesh_atoms::Instance> mesh_atoms::Collect()
{
	std::vector<Instance> result;
	const glm::vec3 cameraOrigin = Locator::camera::has_value() ? Locator::camera::value().GetOrigin() : glm::vec3(0.0f);
	const glm::vec3* camera = Locator::camera::has_value() ? &cameraOrigin : nullptr;
	for (const auto& drawable : manager::Collect(Creator::Kind::Mesh))
	{
		for (const auto& atom : drawable.atoms)
		{
			if (const auto* piece = atom.atom != nullptr ? explode_object::PieceOf(*atom.atom) : nullptr; piece != nullptr)
			{
				if (piece->meshId != 0)
				{
					result.push_back(PieceInstance(atom, piece->meshId));
				}
				continue;
			}
			const auto* creator = dynamic_cast<const MeshCreator*>(atom.creator);
			if (creator == nullptr || creator->meshId == 0)
			{
				continue;
			}
			// Particle3DAnim::DrawAt 0x67A8E0: the whole frame of DrawData +0x10 (fn_00679920, 0..999) gives the clip's
			// time (GetCycleTimeFromFrame 0x6C85F0, kept in the particle's +0x28 and set on its object by vt 0x188
			// fn_0080B880: +0x84), which the object's draw poses the bones at (fn_008175B0: LH3DAnim::GetPose 0x839980 of
			// +0x80 at +0x84, 0x8177B8..0x8177CE). Frame 0 is not drawn: 0x67A9B7..0x67A9BE leaves before vt 0xF8 / 0x104
			const L3DAnim* clip = CreatorClip(*creator);
			int animFrame = 0;
			if (creator->animated)
			{
				animFrame = graphics::frame_anim::PSysFrameIndex(atom.frame, creator->FramesPerAtom(), creator->loopAnim);
				if (animFrame == 0)
				{
					continue;
				}
			}
			// fn_00679920: the PSR matrix (rotation x scale, the Y axis x the stretch): the LHMatrix rows r0, r1, r2 are
			// the columns here
			glm::mat3 axes = atom.rotation * atom.scale;
			axes[1] *= atom.stretch;
			// Particle3DObj::DrawAt 0x679FD0: FaceCamera (+0x4D) first (0x67A040), else FaceCameraSprite (+0x4C, 0x67A250;
			// no spell file sets it), which starts again from the identity x the scale
			if (creator->faceCamera && camera != nullptr)
			{
				graphics::billboard::ParticleYaw(axes, atom.position, *camera, creator->heightStretch);
			}
			else if (creator->faceCameraSprite && camera != nullptr)
			{
				axes = graphics::billboard::FullSprite(atom.position, *camera, atom.scale);
			}
			glm::mat4 model(axes);
			model[3] = glm::vec4(atom.position, 1.0f);
			glm::vec2 uv(0.0f);
			if (creator->animTextured)
			{
				// the whole frame drawn (fn_00679920, DrawData +0x10): looped within N or clamped to its last
				uv = creator->UvOffset(graphics::frame_anim::PSysFrameIndex(atom.frame, creator->FramesPerAtom(), creator->loopAnim));
			}
			// UseScriptHightlightPulse (A x fn_0070A510, the script highlight's pulse): not ported
			const float alpha = std::clamp(atom.alpha / 255.0f, 0.0f, 1.0f);
			// (inferido: port routing) translucent when additive or not fully opaque. DrawCutByPlane (+0x24 bit 4) only
			// changes the call: fn_00679F20 draws through vt 0x11C instead of vt 0x104, and for the static LH3DObject a
			// particle mesh is (LH3DObject::Create(0) 0x80B4F8 -> LH3DStaticObject, vtable 0x9A2974) vt 0x11C is
			// fn_0080C050, a plain draw of its primitives with the world-to-clip matrix: no plane cuts a static mesh (the
			// cut at y = 0 is the animated objects' fn_00811C70, rendering.md). Nothing to port for the dome.
			result.push_back({creator->meshId, model, alpha, uv, creator->additive || alpha < 1.0f, creator->additive, atom.colour,
			                  creator->drawWithLandscapeColour});
			// (openblack) no pose for an atom of alpha 0: the renderer does not draw it
			if (clip != nullptr && alpha > 0.0f && Locator::resources::has_value())
			{
				const auto& meshes = Locator::resources::value().GetMeshes();
				if (meshes.Contains(creator->meshId))
				{
					const auto mesh = meshes.Handle(creator->meshId);
					if (mesh->IsBoned())
					{
						graphics::ComputePose(*mesh, *clip,
						                      static_cast<float>(AnimCycleTime(clip->GetDurationMs(), animFrame)),
						                      result.back().pose);
					}
				}
			}
		}
	}
	return result;
}

void openblack::psys::RegisterMeshCreators()
{
	RegisterCreator("ParticleMeshCreator", MakeMeshCreator);
	RegisterCreator("ParticleMeshCreatorAnimTextured", MakeMeshCreator);
	RegisterCreator("ParticleAnimCreator", MakeMeshCreator); // the forest's butterflies and bats (Particle3DAnim)
}
