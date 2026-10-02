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
#include "3D/L3DMesh.h"
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

/// GJUtils::GetSharedMesh 0x57DFB0: a mesh file (".\Data\Spells\Meshes\X.l3d"), loaded once by its name
entt::id_type SharedMesh(std::string path)
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
		// double-sided 1, +0x93 / +0x94 1, no MeshChangeMaterialProps): (aproximado) drawn as a still mesh in its bind
		// pose. Not ported: the .anm (AnimFileName / AnimEnum, Particle3DAnim::DrawAt 0x67A8E0: GetCycleTimeFromFrame
		// 0x6C85F0 -> vt 0x188, SpeedUpFactor, RandomiseInitFrame, PlayAnim), the blend to MeshFileName1/2 between
		// FrameToStartBlend and FrameToEndBlend (vt 0xDC), UseSuperSortedPolys
		creator->changeMaterialProps = true;
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

void MeshCreator::InitAtom(Effect& effect, Atom& atom) const
{
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
		}
	}
	return result;
}

void openblack::psys::RegisterMeshCreators()
{
	RegisterCreator("ParticleMeshCreator", MakeMeshCreator);
	RegisterCreator("ParticleMeshCreatorAnimTextured", MakeMeshCreator);
	RegisterCreator("ParticleAnimCreator", MakeMeshCreator); // (aproximado) the forest's butterflies and bats, still
}
