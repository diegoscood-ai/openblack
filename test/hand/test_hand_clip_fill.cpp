/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// How the 3D engine fills a boned mesh's bones from a clip and adds a lean range (anim_fill), against the original's
// own routines run on the same synthetic skeleton and clips (tools/oracle): every float bit for bit.
// docs/wiki/hand/animation-and-placement.md#the-hands-clip, docs/wiki/project/progress/hand/hand_animations.md

#include <cstdint>

#include <array>
#include <bit>
#include <vector>

#include <gtest/gtest.h>

#include "3D/AnimationFill.h"
#include "3D/ObjectMatrix.h"

using namespace openblack;

namespace
{
// The original's outputs: each bone's nine rows, then (for the poses) its place
constexpr std::array<uint32_t, 36> k_RestBase {{
    0x3F79E459u, 0xBDE01397u, 0x3E400F9Du, 0x3B4BFC20u, 0x3F5EE883u, 0x3EFBC59Cu, 0xBE5E5406u, 0xBEF57744u, 0x3F59AD4Du,
    0x3EFA245Eu, 0x3E82D53Eu, 0x3F5592FCu, 0x3C9959B0u, 0x3F73EEE8u, 0xBE9B0B14u, 0xBF5F5109u, 0x3E277CCDu, 0x3EEBE6FAu,
    0x3F53088Eu, 0xBD85D910u, 0x3F0FF2A7u, 0x3EDF411Eu, 0x3F34ED5Bu, 0xBF0E9EF2u, 0xBEB8D3C9u, 0x3F345645u, 0x3F1C714Eu,
    0x3F7477EAu, 0xBC1C1978u, 0xBE97DBC3u, 0x3E9378AAu, 0xBE57D586u, 0x3F6F239Fu, 0xBD92426Bu, 0xBF7A3CA8u, 0xBE4B4D40u,
}};
constexpr std::array<uint32_t, 36> k_RestInverse {{
    0x3F79E458u, 0x3B4BFC00u, 0xBE5E5406u, 0xBDE01396u, 0x3F5EE883u, 0xBEF57743u, 0x3E400F9Cu, 0x3EFBC59Eu, 0x3F59AD4Eu,
    0x3EFA245Cu, 0x3C9959ADu, 0xBF5F5108u, 0x3E82D53Du, 0x3F73EEECu, 0x3E277CCEu, 0x3F5592FEu, 0xBE9B0B17u, 0x3EEBE6FEu,
    0x3F53088Au, 0x3EDF4120u, 0xBEB8D3C8u, 0xBD85D90Cu, 0x3F34ED5Du, 0x3F345644u, 0x3F0FF2A6u, 0xBF0E9EF6u, 0x3F1C7150u,
    0x3F7477EAu, 0x3E9378A9u, 0xBD92426Au, 0xBC1C1968u, 0xBE57D58Bu, 0xBF7A3CA9u, 0xBE97DBC3u, 0x3F6F239Fu, 0xBE4B4D3Bu,
}};
constexpr std::array<uint32_t, 48> k_Fill0 {{
    0x3F57B60Au, 0xBF09458Eu, 0x3D4AADF0u, 0x3F011A1Cu, 0x3F51689Eu, 0x3E8DA583u, 0xBE415B1Du, 0xBE552830u,
    0x3F75AE2Du, 0x3F800000u, 0x40200000u, 0xC0400000u, 0x3E9A6555u, 0x3F6ED7CEu, 0xBE492FDFu, 0xBEF62099u,
    0xBCEF05C0u, 0xBF605AA8u, 0xBF52C969u, 0x3EB7AB03u, 0x3EE120E1u, 0xC1120000u, 0x4306C000u, 0x437C8000u,
    0x3F4684EBu, 0xBF074AF0u, 0xBEB0E270u, 0x3EBB9F16u, 0x3F528243u, 0xBEDEE77Fu, 0x3F03A089u, 0x3E581242u,
    0x3F54D1F1u, 0x3F000000u, 0xBFA00000u, 0x40800000u, 0x3F74778Eu, 0xBC1C1882u, 0xBE97DB8Au, 0x3E937872u,
    0xBE57D440u, 0x3F6F2344u, 0xBD924231u, 0xBF7A3B32u, 0xBE4B4CF2u, 0x41FE0000u, 0x40DC0000u, 0x420D0000u,
}};
constexpr std::array<uint32_t, 48> k_Fill177 {{
    0x3F72EC8Fu, 0xBE82F2C1u, 0x3E3CDA70u, 0x3E7200E1u, 0x3F76C948u, 0x3DF7E50Du, 0xBE55FAD4u, 0xBD921C6Cu,
    0x3F79B45Au, 0x3FBFC286u, 0x40001EBDu, 0xC0201EBDu, 0x3E3E5CC0u, 0x3F7AFE2Eu, 0x3D831DCCu, 0xBEECA63Bu,
    0x3E14285Au, 0xBF5FF72Eu, 0xBF5DFB56u, 0x3E084755u, 0x3EF5C0D8u, 0xC1120000u, 0x4306C000u, 0x437C8000u,
    0x3F4684EBu, 0xBF074AF0u, 0xBEB0E270u, 0x3EBB9F16u, 0x3F528243u, 0xBEDEE77Fu, 0x3F03A089u, 0x3E581242u,
    0x3F54D1F1u, 0x3E807AF5u, 0xBF801EBDu, 0x40601EBDu, 0x3F7A9E1Bu, 0x3DC8A669u, 0xBE3751DBu, 0x3E318A0Du,
    0x3D7DB240u, 0x3F7B9C2Au, 0x3DDBB888u, 0xBF7E44A2u, 0x3D331B54u, 0x41FE0000u, 0x40DC0000u, 0x420D0000u,
}};
constexpr std::array<uint32_t, 48> k_Fill400 {{
    0x3F23C386u, 0xBF3A36DCu, 0x3E80CB9Cu, 0x3E9C0758u, 0x3F1B0A2Du, 0x3F3E5A34u, 0xBF2D34D5u, 0xBEC41DCEu,
    0x3F1E16DFu, 0x3F202E1Cu, 0x4037F479u, 0xC010170Eu, 0x3EF69DA4u, 0x3F587B55u, 0xBE816A1Eu, 0xBE39B687u,
    0xBE4FC1D0u, 0xBF7739AEu, 0xBF5C2FB5u, 0x3EFAC0F6u, 0x3D54AC84u, 0xC1120000u, 0x4306C000u, 0x437C8000u,
    0x3F4684EBu, 0xBF074AF0u, 0xBEB0E270u, 0x3EBB9F16u, 0x3F528243u, 0xBEDEE77Fu, 0x3F03A089u, 0x3E581242u,
    0x3F54D1F1u, 0x3F5FD1E4u, 0xBF30452Au, 0x4050170Eu, 0x3F683D5Bu, 0x3DA7070Au, 0xBEDE3ABAu, 0x3EC2F402u,
    0x3E619E03u, 0x3F632B24u, 0x3E3FD8A9u, 0xBF7843A9u, 0x3E2324EAu, 0x41FE0000u, 0x40DC0000u, 0x420D0000u,
}};
constexpr std::array<uint32_t, 48> k_Fill532 {{
    0x3F576B90u, 0xBF09B15Eu, 0x3D51BD44u, 0x3F00E4CDu, 0x3F5142A6u, 0x3E8F9242u, 0xBE44E558u, 0xBE56B178u,
    0x3F755EA0u, 0x3F7F4790u, 0x40202E1Cu, 0xC03FA3C8u, 0x3E9B2463u, 0x3F6EBA5Eu, 0xBE49D5C2u, 0xBEF5256Du,
    0xBCF895E0u, 0xBF60A33Eu, 0xBF52F622u, 0x3EB82D45u, 0x3EDFBE5Cu, 0xC1120000u, 0x4306C000u, 0x437C8000u,
    0x3F4684EBu, 0xBF074AF0u, 0xBEB0E270u, 0x3EBB9F16u, 0x3F528243u, 0xBEDEE77Fu, 0x3F03A089u, 0x3E581242u,
    0x3F54D1F1u, 0x3F00B870u, 0xBF9F75ACu, 0x407FA3C8u, 0x3F7470CBu, 0xBC10F740u, 0xBE98763Fu, 0x3E93DAFAu,
    0xBE54BDFAu, 0x3F6F2CA0u, 0xBD8E70C0u, 0xBF7A668Fu, 0xBE48BD0Du, 0x41FE0000u, 0x40DC0000u, 0x420D0000u,
}};
// a lag of -80 pixels: 0 ms
constexpr std::array<uint32_t, 48> k_LeanM80_0 {{
    0x3F7930B3u, 0xBDAF57D3u, 0x3E594C39u, 0x3DEA15C8u, 0x3F7C38CEu, 0xBE01B014u, 0xBE4B3370u, 0x3E1707B1u,
    0x3F7812DFu, 0x4137F851u, 0xBFFFC286u, 0xBF007AF4u, 0x3C80991Cu, 0x3F7D6648u, 0xBE104C72u, 0xBEB68611u,
    0xBE013996u, 0xBF6CFAB9u, 0xBF6F2769u, 0x3D84A307u, 0x3EB39C3Fu, 0xC1120000u, 0x4306C000u, 0x437C8000u,
    0x3F4684EBu, 0xBF074AF0u, 0xBEB0E270u, 0x3EBB9F16u, 0x3F528243u, 0xBEDEE77Fu, 0x3F03A089u, 0x3E581242u,
    0x3F54D1F1u, 0x3E807AF5u, 0xBF801EBDu, 0x40601EBDu, 0x3F7CF12Du, 0xBCD3D260u, 0x3E1BA8DEu, 0xBE1DF76Eu,
    0xBE47E5F0u, 0x3F77EF16u, 0x3B93F3B0u, 0xBF7AFAD0u, 0xBE498DD8u, 0x41FE0000u, 0x40DC0000u, 0x420D0000u,
}};
// -23.5 pixels: 188 ms
constexpr std::array<uint32_t, 48> k_LeanM23_5 {{
    0x3F70BA7Fu, 0xBE774B80u, 0x3E732B94u, 0x3E71D62Eu, 0x3F786568u, 0x3D6FFAD6u, 0xBE7AAF8Au, 0x3A1C7B00u,
    0x3F783B80u, 0x408E1D38u, 0x3F531F78u, 0xBFF4E69Bu, 0x3E32822Bu, 0x3F7C52DFu, 0x3ACE2500u, 0xBED2A218u,
    0x3D96B610u, 0xBF686003u, 0xBF6518DCu, 0x3E1E6748u, 0x3ED5EA72u, 0xC1120000u, 0x4306C000u, 0x437C8000u,
    0x3F4684EBu, 0xBF074AF0u, 0xBEB0E270u, 0x3EBB9F16u, 0x3F528243u, 0xBEDEE77Fu, 0x3F03A089u, 0x3E581242u,
    0x3F54D1F1u, 0x3E807AF5u, 0xBF801EBDu, 0x40601EBDu, 0x3F7E7CCEu, 0x3D895626u, 0xBD966A6Au, 0x3D94BD34u,
    0xBC589CE0u, 0x3F7EE5D4u, 0x3D9055B2u, 0xBF7FF009u, 0xBCA2EC90u, 0x41FE0000u, 0x40DC0000u, 0x420D0000u,
}};
// no lag: 266 ms, half the length
constexpr std::array<uint32_t, 48> k_LeanP0_0 {{
    0x3F72E87Fu, 0xBE82E8F2u, 0x3E3D210Du, 0x3E71FF77u, 0x3F76CAD3u, 0x3DF73DFDu, 0xBE562999u, 0xBD915C1Cu,
    0x3F79B213u, 0x3FC1AE8Au, 0x3FFF78ACu, 0xC01FED8Au, 0x3E3E4CD8u, 0x3F7AFF96u, 0x3D8276CCu, 0xBEEC83C7u,
    0x3E13C96Eu, 0xBF6001E1u, 0xBF5E03E3u, 0x3E0863CEu, 0x3EF596C9u, 0xC1120000u, 0x4306C000u, 0x437C8000u,
    0x3F4684EBu, 0xBF074AF0u, 0xBEB0E270u, 0x3EBB9F16u, 0x3F528243u, 0xBEDEE77Fu, 0x3F03A089u, 0x3E581242u,
    0x3F54D1F1u, 0x3E807AF5u, 0xBF801EBDu, 0x40601EBDu, 0x3F7AA33Eu, 0x3DC854E6u, 0xBE36C570u, 0x3E310413u,
    0x3D7C1F00u, 0x3F7BA0EDu, 0x3DDB5696u, 0xBF7E476Au, 0x3D31CC70u, 0x41FE0000u, 0x40DC0000u, 0x420D0000u,
}};
// 80 pixels: 533 ms, the whole length: the last frame, blending towards the first
constexpr std::array<uint32_t, 48> k_LeanP80_0 {{
    0x3F69D812u, 0xBECFB973u, 0xBCDDBDD8u, 0x3ECA1BB8u, 0x3F5E5DC0u, 0x3E99276Fu, 0xBDC92E35u, 0xBE91718Du,
    0x3F7430FBu, 0xC1078CADu, 0x40BFACF6u, 0xC08FDE2Au, 0x3E99BF0Eu, 0x3F6B6934u, 0x3E81BF67u, 0xBF1C8EE4u,
    0x3EC7579Eu, 0xBF304933u, 0xBF3B670Bu, 0x3D5365A4u, 0x3F2DE4D6u, 0xC1120000u, 0x4306C000u, 0x437C8000u,
    0x3F4684EBu, 0xBF074AF0u, 0xBEB0E270u, 0x3EBB9F16u, 0x3F528243u, 0xBEDEE77Fu, 0x3F03A089u, 0x3E581242u,
    0x3F54D1F1u, 0x3E807AF5u, 0xBF801EBDu, 0x40601EBDu, 0x3F4F481Eu, 0x3E122AB9u, 0xBF11AE9Bu, 0x3F044570u,
    0x3E91DA9Au, 0x3F4E9EC0u, 0x3E8E2AEFu, 0xBF72B108u, 0x3E207F12u, 0x41FE0000u, 0x40DC0000u, 0x420D0000u,
}};

// The original's world matrices of the pose at 177 ms under the object's matrix of WorldBits below
constexpr std::array<uint32_t, 48> k_World177 {{
    0x3B12B0ACu, 0xBBF40402u, 0x3A44C2C0u, 0x3BBAC1FDu, 0x3B1066BFu, 0x3B9F79DBu, 0xBB9F03A0u, 0xBA5DCAE5u,
    0x3BC6BACDu, 0x449610D8u, 0x420D0390u, 0xC459B07Cu, 0x3BBA8F3Cu, 0x3A3D3500u, 0x3BADA6EDu, 0x3B843C8Eu,
    0x3B937C94u, 0xBBA227EBu, 0xBB66233Eu, 0x3BCFE43Eu, 0x3B3E91BDu, 0x4496018Eu, 0x420D9E6Bu, 0xC45924F2u,
    0x3B6513E8u, 0xBB836D1Eu, 0x3BBB7099u, 0x3BE332E9u, 0x3A9DBFF0u, 0xBB5E6104u, 0x3A6153A8u, 0x3BD81B81u,
    0x3B86487Du, 0x44960114u, 0x420DB0BEu, 0xC45923E4u, 0x3B6D2295u, 0xBBE2D924u, 0x390401A6u, 0xBB8400EAu,
    0xBB022ADCu, 0x3BD176AEu, 0xBBB89425u, 0xBB46352Fu, 0xBB93118Au, 0x44960EE6u, 0x420C026Du, 0xC4599F23u,
}};

/// Four bones: the root, a child of the root, a grandchild, and a second child of the root
const std::vector<uint32_t> k_Parents {anim_fill::k_NoParent, 0, 1, 0};

/// The bones' rest matrices relative to their parents: a turn by the engine's Y-X-Z rotation and a place
std::vector<glm::mat4> Locals()
{
	// (y, x, z) as the engine takes them
	const std::array<glm::vec3, 4> turns {glm::vec3(0.25f, -0.5f, 0.125f), glm::vec3(1.0f, 0.375f, -0.75f),
	                                      glm::vec3(-0.625f, 0.5f, 0.25f), glm::vec3(0.0f, -1.25f, 0.5f)};
	const std::array<glm::vec3, 4> places {glm::vec3(1.5f, -2.0f, 0.25f), glm::vec3(0.0f, 3.0f, 1.0f),
	                                       glm::vec3(0.5f, 0.0f, 2.5f), glm::vec3(-1.0f, 0.75f, -0.5f)};
	std::vector<glm::mat4> locals;
	for (size_t i = 0; i < turns.size(); ++i)
	{
		const auto m = affine::RotationYXZ(turns[i].x, turns[i].y, turns[i].z);
		const auto& p = places[i];
		locals.emplace_back(m[0][0], m[0][1], m[0][2], 0.0f, m[1][0], m[1][1], m[1][2], 0.0f, m[2][0], m[2][1], m[2][2], 0.0f,
		                    p.x, p.y, p.z, 1.0f);
	}
	return locals;
}

/// A cycle of three frames that turns the root, its first child and its second, and moves the root and the grandchild
anim_fill::Clip Cycle()
{
	return {
	    .durationMs = 533,
	    .cyclic = true,
	    .boneCount = 4,
	    .angleBones = {0, 1, 3},
	    .positionBones = {0, 2},
	    .frames =
	        {
	            {.angles = {{0.125f, -0.25f, 0.5f}, {0.75f, 0.0625f, -0.375f}, {-0.5f, 0.25f, 0.125f}},
	             .positions = {{1.0f, 2.5f, -3.0f}, {0.5f, -1.25f, 4.0f}}},
	            {.angles = {{0.375f, -0.125f, 0.25f}, {0.5f, 0.25f, -0.5f}, {-0.25f, 0.375f, 0.0f}},
	             .positions = {{1.5f, 2.0f, -2.5f}, {0.25f, -1.0f, 3.5f}}},
	            {.angles = {{-0.25f, 0.5f, 0.75f}, {1.0f, -0.125f, 0.25f}, {0.0f, 0.125f, -0.25f}},
	             .positions = {{0.5f, 3.0f, -2.0f}, {1.0f, -0.5f, 3.0f}}},
	        },
	};
}

/// The stand frame, read by bone: what the bones a clip neither turns nor moves take
anim_fill::Frame Stand()
{
	return {.angles = {{0.0625f, 0.125f, -0.25f}, {0.25f, -0.5f, 0.0f}, {-0.375f, 0.25f, 0.125f}, {0.5f, 0.0f, -0.125f}},
	        .positions = {
	            {-229.25f, -555.25f, -4.875f}, {-9.125f, 134.75f, 252.5f}, {0.0f, 0.0f, 79.5f}, {31.75f, 6.875f, 35.25f}}};
}

/// A lean range of five frames that turns the root and both children and moves the root
anim_fill::Clip Range()
{
	return {
	    .durationMs = 533,
	    .cyclic = false,
	    .boneCount = 4,
	    .angleBones = {0, 1, 3},
	    .positionBones = {0},
	    .frames =
	        {
	            {.angles = {{0.25f, 0.0f, -0.125f}, {0.5f, 0.125f, 0.0f}, {-0.125f, 0.25f, 0.375f}},
	             .positions = {{10.0f, -4.0f, 2.0f}}},
	            {.angles = {{0.125f, 0.0625f, 0.0f}, {0.375f, 0.0f, 0.125f}, {0.0f, 0.125f, 0.25f}},
	             .positions = {{5.0f, -2.0f, 1.0f}}},
	            {.angles = {{0.0f, 0.0f, 0.0f}, {0.25f, -0.0625f, 0.0625f}, {0.125f, 0.0f, 0.125f}},
	             .positions = {{0.0f, 0.0f, 0.0f}}},
	            {.angles = {{-0.125f, -0.0625f, 0.0f}, {0.125f, -0.125f, 0.0f}, {0.25f, -0.125f, 0.0f}},
	             .positions = {{-5.0f, 2.0f, -1.0f}}},
	            {.angles = {{-0.25f, -0.125f, 0.125f}, {0.0f, -0.25f, -0.0625f}, {0.375f, -0.25f, -0.125f}},
	             .positions = {{-10.0f, 4.0f, -2.0f}}},
	        },
	};
}

std::vector<uint32_t> RowBits(const std::vector<anim_fill::Rows>& rows)
{
	std::vector<uint32_t> bits;
	for (const auto& r : rows)
	{
		for (const float v : r)
		{
			bits.push_back(std::bit_cast<uint32_t>(v));
		}
	}
	return bits;
}

std::vector<uint32_t> PoseBits(const std::vector<anim_fill::BoneMatrix>& pose)
{
	std::vector<uint32_t> bits;
	for (const auto& bone : pose)
	{
		for (const float v : bone.rows)
		{
			bits.push_back(std::bit_cast<uint32_t>(v));
		}
		for (const float v : {bone.position.x, bone.position.y, bone.position.z})
		{
			bits.push_back(std::bit_cast<uint32_t>(v));
		}
	}
	return bits;
}

template <size_t N>
std::vector<uint32_t> Expected(const std::array<uint32_t, N>& bits)
{
	return {bits.begin(), bits.end()};
}

std::vector<anim_fill::BoneMatrix> Filled(int32_t timeMs)
{
	const auto rest = anim_fill::RestFromLocals(Locals(), k_Parents);
	std::vector<anim_fill::BoneMatrix> pose(4, anim_fill::BoneMatrix {.rows = {}, .position = glm::vec3(0.0f)});
	anim_fill::Fill(pose, Cycle(), rest, timeMs, Stand());
	return pose;
}

std::vector<anim_fill::BoneMatrix> Leaning(float lag)
{
	const auto rest = anim_fill::RestFromLocals(Locals(), k_Parents);
	auto pose = Filled(177);
	const auto range = Range();
	// the difference from the middle frame, (count - 1) / 2
	anim_fill::AddDifferenceFromFrame(pose, range, range.frames[2], rest, anim_fill::RangeTimeMs(lag, 80.0f, range.durationMs));
	return pose;
}
} // namespace

TEST(AnimationFill, TheFrameIsTheCountTimesTheTimeOverTheLengthAndARangeEndsBlendingToItsFirst)
{
	// a cycle: 3 x 177 / 533 is frame 0, blending 3 / 533 x 177 of the way to frame 1
	const auto cycle = anim_fill::FindFrame(3, 533, true, 177);
	EXPECT_EQ(cycle.frame, 0);
	EXPECT_EQ(cycle.next, 1);
	EXPECT_EQ(cycle.blend, (3.0f / 533.0f) * 177.0f);
	// the last frame of a cycle blends towards its first
	EXPECT_EQ(anim_fill::FindFrame(3, 533, true, 400).next, 0);
	// a range's length is 533 x 5 / 4 = 666: at its whole duration it is on its last frame, blending towards the first
	const auto range = anim_fill::FindFrame(5, 533, false, 533);
	EXPECT_EQ(range.frame, 4);
	EXPECT_EQ(range.next, 0);
	EXPECT_EQ(range.blend, (5.0f / 666.0f) * 533.0f - 4.0f);
	// a time past the end stays on the last frame
	EXPECT_EQ(anim_fill::FindFrame(3, 533, true, 700).frame, 2);
}

TEST(AnimationFill, ALeanRangesTimeIsTheLagsShareOfItsLength)
{
	EXPECT_EQ(anim_fill::RangeTimeMs(-80.0f, 80.0f, 533), 0);
	EXPECT_EQ(anim_fill::RangeTimeMs(-23.5f, 80.0f, 533), 188);
	EXPECT_EQ(anim_fill::RangeTimeMs(0.0f, 80.0f, 533), 266);
	EXPECT_EQ(anim_fill::RangeTimeMs(80.0f, 80.0f, 533), 533);
}

TEST(AnimationFill, TheRestIsTheBonesCarriedDownSetBackAndCarriedDownAgainAsTheOriginal)
{
	const auto rest = anim_fill::RestFromLocals(Locals(), k_Parents);
	EXPECT_EQ(RowBits(rest.base), Expected(k_RestBase));
	EXPECT_EQ(RowBits(rest.inverseBase), Expected(k_RestInverse));
}

TEST(AnimationFill, AClipsFrameIsFilledAsTheOriginalFillsIt)
{
	// the bones the cycle turns blend their keys; the grandchild takes the stand frame's turn, the second child the
	// stand frame's place
	EXPECT_EQ(PoseBits(Filled(0)), Expected(k_Fill0));
	EXPECT_EQ(PoseBits(Filled(177)), Expected(k_Fill177));
	EXPECT_EQ(PoseBits(Filled(400)), Expected(k_Fill400));
	EXPECT_EQ(PoseBits(Filled(532)), Expected(k_Fill532));
}

TEST(AnimationFill, ThePoseIsFinishedInTheWorldAsTheOriginalFinishesIt)
{
	// the object's matrix: a turn by the engine's Y-X-Z rotation, each cell times 1/128, and a place
	const auto r = affine::RotationYXZ(0.5f, -0.25f, 1.0f);
	constexpr float k = 0.0078125f;
	const glm::mat4 object(r[0][0] * k, r[0][1] * k, r[0][2] * k, 0.0f, r[1][0] * k, r[1][1] * k, r[1][2] * k, 0.0f,
	                       r[2][0] * k, r[2][1] * k, r[2][2] * k, 0.0f, 1200.5f, 35.25f, -870.75f, 1.0f);
	std::vector<uint32_t> bits;
	for (const auto& m : anim_fill::ToWorld(Filled(177), k_Parents, object))
	{
		for (glm::length_t c = 0; c < 4; ++c)
		{
			for (glm::length_t e = 0; e < 3; ++e)
			{
				bits.push_back(std::bit_cast<uint32_t>(m[c][e]));
			}
		}
	}
	EXPECT_EQ(bits, Expected(k_World177));
}

TEST(AnimationFill, ALeanRangeAddsItsDifferenceFromItsMiddleFrameAsTheOriginal)
{
	EXPECT_EQ(PoseBits(Leaning(-80.0f)), Expected(k_LeanM80_0));
	EXPECT_EQ(PoseBits(Leaning(-23.5f)), Expected(k_LeanM23_5));
	EXPECT_EQ(PoseBits(Leaning(0.0f)), Expected(k_LeanP0_0));
	EXPECT_EQ(PoseBits(Leaning(80.0f)), Expected(k_LeanP80_0));
}
