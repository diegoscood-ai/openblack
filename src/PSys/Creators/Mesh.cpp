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
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "PSys/PSysFile.h"
#include "PSys/PSysManager.h"
#include "PSys/PSysRegistry.h"
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
	creator->heightStretch = object.Float("HeightStretch", 1.0f);
	creator->changeMaterialProps = object.Bool("MeshChangeMaterialProps", creator->animTextured);
	creator->additive = creator->changeMaterialProps && object.Bool("UseAdditiveAlpha", false);
	creator->neverClip = object.Bool("NeverClip", false);
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
	atom.frameRate = playAnimation ? rate : 0.0f; // +0x110, PlayAnim +0x118
	atom.stretch = stretchY;                      // +0x7C
}

glm::vec2 MeshCreator::UvOffset(int frame) const
{
	if (!slideU && !slideV)
	{
		// the frames tile a 256 x 256 texture in rows of 256 / W
		const int perRow = std::max(1, 256 / textureWidth);
		const auto f = static_cast<unsigned>(frame);
		return {static_cast<float>(textureWidth) / 256.0f * static_cast<float>(f % static_cast<unsigned>(perRow)),
		        static_cast<float>(textureHeight) / 256.0f * static_cast<float>(f / static_cast<unsigned>(perRow))};
	}
	const float n = static_cast<float>(FramesPerAtom()) * 256.0f;
	return {slideU ? static_cast<float>(textureWidth * frame) / n : 0.0f, slideV ? static_cast<float>(textureHeight * frame) / n : 0.0f};
}

std::vector<mesh_atoms::Instance> mesh_atoms::Collect()
{
	std::vector<Instance> result;
	for (const auto& drawable : manager::Collect(Creator::Kind::Mesh))
	{
		for (const auto& atom : drawable.atoms)
		{
			const auto* creator = dynamic_cast<const MeshCreator*>(atom.creator);
			if (creator == nullptr || creator->meshId == 0)
			{
				continue;
			}
			// fn_00679920: the PSR matrix (rotation x scale, the Y axis x the stretch); TODO: FaceCamera /
			// FaceCameraSprite / HeightStretch (0x679FD0) and the colour (SetColour) are not drawn
			glm::mat3 axes = atom.rotation * atom.scale;
			axes[1] *= atom.stretch;
			glm::mat4 model(axes);
			model[3] = glm::vec4(atom.position, 1.0f);
			glm::vec2 uv(0.0f);
			if (creator->animTextured)
			{
				// the frame drawn: looped fmod(f, N), else clamped to 0..N-1
				const float frames = static_cast<float>(creator->FramesPerAtom());
				float frame = atom.frame;
				frame = creator->loopAnim ? std::fmod(std::fmod(frame, frames) + frames, frames) : std::clamp(frame, 0.0f, frames - 1.0f);
				uv = creator->UvOffset(static_cast<int>(frame));
			}
			const float alpha = std::clamp(atom.alpha / 255.0f, 0.0f, 1.0f);
			result.push_back({creator->meshId, model, alpha, uv, creator->additive || alpha < 1.0f});
		}
	}
	return result;
}

void openblack::psys::RegisterMeshCreators()
{
	RegisterCreator("ParticleMeshCreator", MakeMeshCreator);
	RegisterCreator("ParticleMeshCreatorAnimTextured", MakeMeshCreator);
}
