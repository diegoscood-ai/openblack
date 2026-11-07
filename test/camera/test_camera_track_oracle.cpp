/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <vector>

#include <EDTFile.h>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/CameraTrack.h"

// The script camera along three of the game's camera.edt tracks (the glade where the creature is chosen, the start of
// Land 4 and the lost brother's), against what the original game gives for the same samples. The original was run
// with one runner per track, sampled every 40 milliseconds from the start to the track's duration, then 5 before the
// start and far past the end, as the script camera mode asks it each turn. Every value is its exact bits.
using namespace openblack;
using namespace openblack::camera_track;

namespace
{

struct TrackCase
{
	int32_t track;
	int32_t sample;
	uint32_t segment;
	std::array<uint32_t, 3> position;
	std::array<uint32_t, 3> focus;
};

constexpr int32_t k_SampleStep = 40;
constexpr std::array<int32_t, 2> k_AfterTheSweep {-5, 100000};

// Every eighth, sixteenth or fourth sample of each track, its last sample of the sweep and the two after it
constexpr std::array k_TrackCases {
    TrackCase {13, 0, 0u, {0x44dc4226, 0x4232d0f8, 0x452d9d06}, {0x44ddf922, 0x42073bc2, 0x452f82fc}},
    TrackCase {13, 320, 0u, {0x44dcd375, 0x423191c8, 0x452e3804}, {0x44de9c3e, 0x4207132e, 0x45302caa}},
    TrackCase {13, 640, 0u, {0x44dd745a, 0x42302028, 0x452ee59b}, {0x44df4e30, 0x4206e766, 0x4530eb84}},
    TrackCase {13, 960, 0u, {0x44de217e, 0x422e89a8, 0x452fa178}, {0x44e00c41, 0x4206b8e7, 0x4531ba7f}},
    TrackCase {13, 1280, 0u, {0x44ded7e8, 0x422cdb15, 0x453067aa}, {0x44e0d41b, 0x42068815, 0x453294fa}},
    TrackCase {13, 1600, 0u, {0x44df93c2, 0x422b232b, 0x45313358}, {0x44e1a27f, 0x4206558f, 0x45337556}},
    TrackCase {13, 1920, 0u, {0x44e05322, 0x42296c36, 0x453201bd}, {0x44e27644, 0x42062170, 0x45345835}},
    TrackCase {13, 2240, 0u, {0x44e1110b, 0x4227c783, 0x4532ccbd}, {0x44e34ada, 0x4205eca7, 0x4535369b}},
    TrackCase {13, 2560, 0u, {0x44e1cb5e, 0x42263fd3, 0x45339159}, {0x44e41ede, 0x4205b761, 0x45360cef}},
    TrackCase {13, 2880, 0u, {0x44e27fe3, 0x4224e084, 0x45344c69}, {0x44e4f0e6, 0x420581c5, 0x4536d769}},
    TrackCase {13, 3200, 0u, {0x44e32a5e, 0x4223b921, 0x4534f8a8}, {0x44e5bd37, 0x42054c90, 0x45378ff2}},
    TrackCase {13, 3520, 0u, {0x44e3c7e7, 0x4222d6a2, 0x4535922f}, {0x44e6819c, 0x4205181b, 0x453831ef}},
    TrackCase {13, 3840, 1u, {0x44e4722d, 0x42226740, 0x45362e40}, {0x44e75713, 0x4204f05b, 0x4538b951}},
    TrackCase {13, 4160, 1u, {0x44e54170, 0x42226fa0, 0x4536e1f5}, {0x44e87548, 0x4204ccc6, 0x453946c2}},
    TrackCase {13, 4480, 1u, {0x44e6287d, 0x4222ba0f, 0x4537a1d2}, {0x44e9bdb5, 0x42049ef4, 0x4539d769}},
    TrackCase {13, 4800, 1u, {0x44e7192d, 0x4223106e, 0x4538618d}, {0x44eb0fcb, 0x420458d9, 0x453a674f}},
    TrackCase {13, 5120, 1u, {0x44e8059b, 0x42233d03, 0x45391528}, {0x44ec4b7d, 0x4203eca7, 0x453af2b1}},
    TrackCase {13, 5440, 1u, {0x44e8e311, 0x422309b4, 0x4539b2eb}, {0x44ed54a4, 0x42034a61, 0x453b77aa}},
    TrackCase {13, 5760, 2u, {0x44e9afc2, 0x42224bfc, 0x453a36f0}, {0x44ee2bd2, 0x420258a3, 0x453c013a}},
    TrackCase {13, 6080, 2u, {0x44ea9f96, 0x422119ab, 0x453ac4b5}, {0x44ef13ee, 0x42012075, 0x453ca75c}},
    TrackCase {13, 6400, 2u, {0x44ebaea8, 0x421f90b6, 0x453b5d04}, {0x44f0084a, 0x41ff6a78, 0x453d614e}},
    TrackCase {13, 6720, 2u, {0x44ecd3d6, 0x421dcc0e, 0x453bfd2e}, {0x44f105d8, 0x41fc3d39, 0x453e2984}},
    TrackCase {13, 7040, 2u, {0x44ee04fe, 0x421be837, 0x453ca1fd}, {0x44f208a8, 0x41f8cb52, 0x453ef9c0}},
    TrackCase {13, 7360, 2u, {0x44ef394a, 0x4219ff98, 0x453d48e9}, {0x44f30dec, 0x41f52338, 0x453fccb2}},
    TrackCase {13, 7680, 2u, {0x44f068de, 0x42182b29, 0x453deff8}, {0x44f413a8, 0x41f14fdc, 0x45409daa}},
    TrackCase {13, 8000, 2u, {0x44f185ef, 0x42168cda, 0x453e91e1}, {0x44f512b3, 0x41ed700c, 0x454163e5}},
    TrackCase {13, 8320, 2u, {0x44f28cd0, 0x421537b0, 0x453f2f14}, {0x44f60ce4, 0x41e97fc2, 0x45421da5}},
    TrackCase {13, 8640, 2u, {0x44f3710c, 0x42144bc5, 0x453fc30d}, {0x44f6fc4e, 0x41e59916, 0x4542c316}},
    TrackCase {13, 8960, 3u, {0x44f42986, 0x4213f1f3, 0x45404d76}, {0x44f7da46, 0x41e1e570, 0x45434d3b}},
    TrackCase {13, 9280, 3u, {0x44f4c51d, 0x4214b650, 0x4540f21a}, {0x44f8cdd4, 0x41de23a0, 0x4543da6a}},
    TrackCase {13, 9600, 3u, {0x44f54fa0, 0x42164afb, 0x4541ada4}, {0x44f9e1c2, 0x41d9ebd4, 0x4544715a}},
    TrackCase {13, 9920, 3u, {0x44f5d041, 0x42183654, 0x4542719b}, {0x44fb0736, 0x41d5251a, 0x454506df}},
    TrackCase {13, 10240, 3u, {0x44f64d2d, 0x421a1458, 0x45433248}, {0x44fc3238, 0x41cfbb36, 0x4545920c}},
    TrackCase {13, 10560, 3u, {0x44f6cb22, 0x421b9274, 0x4543e5a9}, {0x44fd581e, 0x41c9a3bd, 0x45460b60}},
    TrackCase {13, 10880, 3u, {0x44f74cce, 0x421c6ea9, 0x454482ad}, {0x44fe6e1b, 0x41c2e8e5, 0x45466c66}},
    TrackCase {13, 11200, 3u, {0x44f7d888, 0x421c7ae2, 0x4545070d}, {0x44ff7460, 0x41bb5c08, 0x4546b30e}},
    TrackCase {13, 11520, 4u, {0x44f870b6, 0x421b74ff, 0x45456a92}, {0x45003812, 0x41b26177, 0x4546d95d}},
    TrackCase {13, 11840, 4u, {0x44f9545c, 0x4217f908, 0x4545b6b6}, {0x4500d559, 0x41a3347c, 0x4546c20a}},
    TrackCase {13, 12160, 4u, {0x44fa68d7, 0x4212d470, 0x4545ef51}, {0x4501785f, 0x4190e3aa, 0x45467a34}},
    TrackCase {13, 12480, 4u, {0x44fb83b8, 0x420d32e8, 0x4546169e}, {0x450211cb, 0x417caf2e, 0x45461d7b}},
    TrackCase {13, 12800, 4u, {0x44fc85b5, 0x4207f10b, 0x45462ebe}, {0x45029685, 0x415b60e7, 0x4545c063}},
    TrackCase {13, 13120, 4u, {0x44fd5e48, 0x42038cc3, 0x45463a20}, {0x450301c2, 0x41401b0d, 0x45456f78}},
    TrackCase {13, 13440, 5u, {0x44fe0731, 0x42003c8c, 0x45463b60}, {0x450344c1, 0x412f08ac, 0x45453b0c}},
    TrackCase {13, 13760, 5u, {0x44febc42, 0x41f975f4, 0x454638a0}, {0x4503739e, 0x4121c12b, 0x45450efc}},
    TrackCase {13, 14080, 5u, {0x44ff8512, 0x41f1b120, 0x454632ae}, {0x4503a39d, 0x4112f428, 0x4544dadc}},
    TrackCase {13, 14400, 5u, {0x45002de5, 0x41e95f78, 0x45462944}, {0x4503d450, 0x410328d8, 0x4544a090}},
    TrackCase {13, 14720, 5u, {0x45009dcd, 0x41e0aba6, 0x45461c0f}, {0x4504057f, 0x40e5a65f, 0x454461bf}},
    TrackCase {13, 15040, 5u, {0x45010ecf, 0x41d7d4f0, 0x45460adb}, {0x45043680, 0x40c51b1a, 0x4544209a}},
    TrackCase {13, 15360, 5u, {0x45017f3b, 0x41cef77e, 0x4545f52f}, {0x4504676f, 0x40a56385, 0x4543de56}},
    TrackCase {13, 15680, 5u, {0x4501ebaf, 0x41c6518c, 0x4545dad5}, {0x450497a4, 0x4087b14f, 0x45439d22}},
    TrackCase {13, 16000, 5u, {0x45025052, 0x41be2b2d, 0x4545bbe2}, {0x4504c643, 0x405a9ab6, 0x45435f7a}},
    TrackCase {13, 16320, 5u, {0x4502ad11, 0x41b67e48, 0x45459723}, {0x4504f430, 0x402d137e, 0x4543258a}},
    TrackCase {13, 16640, 5u, {0x4502fbce, 0x41afc427, 0x45456dbf}, {0x45051f58, 0x400a6c63, 0x4542f340}},
    TrackCase {13, 16960, 6u, {0x45033e1a, 0x41a9c445, 0x45453c48}, {0x45054bcd, 0x3fe37742, 0x4542c53a}},
    TrackCase {13, 17280, 6u, {0x450377e3, 0x41a39fda, 0x4544f645}, {0x4505828e, 0x3fbcb9c4, 0x45428dde}},
    TrackCase {13, 17600, 6u, {0x4503acaa, 0x419cfeb4, 0x4544981e}, {0x4505c381, 0x3fa079e4, 0x45424c70}},
    TrackCase {13, 17920, 6u, {0x4503dd54, 0x4196048a, 0x4544245e}, {0x45060e24, 0x3f8f6bc9, 0x454202c3}},
    TrackCase {13, 18240, 6u, {0x45040b5a, 0x418ee54c, 0x45439f86}, {0x45066160, 0x3f89f988, 0x4541b3b2}},
    TrackCase {13, 18560, 6u, {0x450438d1, 0x4187e8e6, 0x45431073}, {0x4506bb46, 0x3f901b96, 0x45416358}},
    TrackCase {13, 18880, 6u, {0x450468af, 0x418164b3, 0x4542801a}, {0x4507195e, 0x3fa13dd4, 0x454116d3}},
    TrackCase {13, 19200, 6u, {0x45049f99, 0x4177561e, 0x4541f7e4}, {0x450779e1, 0x3fbcb445, 0x4540d38e}},
    TrackCase {13, 19520, 6u, {0x4504e240, 0x416e924e, 0x45418756}, {0x4507d821, 0x3fe0d47d, 0x4540a220}},
    TrackCase {13, 19840, 7u, {0x45053f0d, 0x41692974, 0x454121e5}, {0x45081752, 0x4005ca17, 0x45409972}},
    TrackCase {13, 20160, 7u, {0x4505ba85, 0x4166a55e, 0x4540b3b2}, {0x4508616b, 0x402b63dd, 0x4540a97c}},
    TrackCase {13, 20480, 7u, {0x450647f9, 0x416694a4, 0x45404b00}, {0x4508af8c, 0x405bbd21, 0x4540ca61}},
    TrackCase {13, 20800, 7u, {0x4506da8f, 0x416884ef, 0x453ff62d}, {0x4508f927, 0x40887887, 0x4540f4ce}},
    TrackCase {13, 21120, 7u, {0x45076687, 0x416c0ab9, 0x453fc32d}, {0x45093630, 0x40a2c29d, 0x454121d6}},
    TrackCase {13, 21440, 8u, {0x4507e44b, 0x4170d7cc, 0x453fc37d}, {0x45097497, 0x40c8ef00, 0x45416406}},
    TrackCase {13, 21760, 8u, {0x45086ec5, 0x4177e9e0, 0x45400690}, {0x4509c59c, 0x4106fd5d, 0x4541da54}},
    TrackCase {13, 22080, 8u, {0x4508f87d, 0x4180de44, 0x454075fc}, {0x450a1396, 0x412d63f2, 0x4542641a}},
    TrackCase {13, 22400, 8u, {0x45096d58, 0x4187584b, 0x4540f70a}, {0x450a5977, 0x414f8ae3, 0x4542edf7}},
    TrackCase {13, 22720, 9u, {0x4509b86a, 0x418f8ceb, 0x45416e6f}, {0x450a91e3, 0x41652470, 0x45436429}},
    TrackCase {13, 23040, 9u, {0x4509a682, 0x419d54f1, 0x4541ee55}, {0x450aba26, 0x41679bd4, 0x4543edce}},
    TrackCase {13, 23360, 9u, {0x450963ba, 0x41ada571, 0x454274f8}, {0x450adca2, 0x415ff69e, 0x45447ebd}},
    TrackCase {13, 23680, 9u, {0x45093818, 0x41bab93c, 0x4542e544}, {0x450b0683, 0x4159579d, 0x4544ed9a}},
    TrackCase {13, 24000, 10u, {0x45094a90, 0x41c1b97d, 0x4543327e}, {0x450b62e9, 0x4158b15a, 0x45454724}},
    TrackCase {13, 24320, 10u, {0x45099354, 0x41c394b0, 0x4543632c}, {0x450bfa6e, 0x415b758a, 0x45459194}},
    TrackCase {13, 24640, 10u, {0x4509d1b2, 0x41c40df8, 0x454381d2}, {0x450c6fa4, 0x415e0554, 0x4545c0db}},
    TrackCase {13, 24960, 10u, {0x4509f49f, 0x41c46fed, 0x45439425}, {0x450cb2a0, 0x415f6f8b, 0x4545dd11}},
    TrackCase {13, 25280, 10u, {0x450a0110, 0x41c4b4ac, 0x45439bfb}, {0x450ccbfa, 0x415feab9, 0x4545e911}},
    TrackCase {13, 25360, 10u, {0x450a01c1, 0x41c4b9a9, 0x45439c77}, {0x450ccd70, 0x415ff164, 0x4545e9cf}},
    TrackCase {13, -5, 0u, {0x44dc4226, 0x4232d0f8, 0x452d9d06}, {0x44ddf922, 0x42073bc2, 0x452f82fc}},
    TrackCase {13, 100000, 10u, {0x450a01cc, 0x41c4b9f9, 0x45439c7f}, {0x450b1e3f, 0x41584fa8, 0x45451014}},
    TrackCase {29, 0, 0u, {0x45078338, 0x427f4cc0, 0x45193ad5}, {0x450d1ec0, 0x41f97ad0, 0x451fa9ea}},
    TrackCase {29, 640, 0u, {0x4506bdc6, 0x428a7e96, 0x4519c22a}, {0x450c79b0, 0x41f6ab72, 0x4520477a}},
    TrackCase {29, 1280, 0u, {0x4505da2c, 0x4296b673, 0x451a4af1}, {0x450bc1ba, 0x41f283ca, 0x4520ec12}},
    TrackCase {29, 1920, 0u, {0x4504dcb6, 0x42a41e10, 0x451ad58d}, {0x450af95f, 0x41ed3c1f, 0x45219738}},
    TrackCase {29, 2560, 0u, {0x4503c9b0, 0x42b28580, 0x451b625e}, {0x450a231d, 0x41e70cb4, 0x45224871}},
    TrackCase {29, 3200, 0u, {0x4502a568, 0x42c1bcd9, 0x451bf1c7}, {0x45094176, 0x41e02dd0, 0x4522ff40}},
    TrackCase {29, 3840, 0u, {0x4501742b, 0x42d19432, 0x451c842c}, {0x450856ea, 0x41d8d7ba, 0x4523bb2d}},
    TrackCase {29, 4480, 0u, {0x45003a44, 0x42e1dba0, 0x451d19ec}, {0x450765f9, 0x41d142b4, 0x45247bbd}},
    TrackCase {29, 5120, 0u, {0x44fdf802, 0x42f2633a, 0x451db36d}, {0x45067124, 0x41c9a704, 0x45254071}},
    TrackCase {29, 5760, 0u, {0x44fb7b5b, 0x43017d8a, 0x451e510e}, {0x45057ae8, 0x41c23cf2, 0x452608d2}},
    TrackCase {29, 6400, 0u, {0x44f90730, 0x4309b9a2, 0x451ef332}, {0x450485c8, 0x41bb3cc0, 0x4526d463}},
    TrackCase {29, 7040, 0u, {0x44f6a113, 0x4311d832, 0x451f9b16}, {0x45039311, 0x41b4d706, 0x4527a3b4}},
    TrackCase {29, 7680, 0u, {0x44f45aac, 0x4319a280, 0x4520468f}, {0x4502a8dc, 0x41af5b17, 0x4528732c}},
    TrackCase {29, 8320, 0u, {0x44f230db, 0x432128bc, 0x4520f974}, {0x4501c4f2, 0x41aae53f, 0x4529467b}},
    TrackCase {29, 8960, 0u, {0x44f03262, 0x43283e0e, 0x4521b274}, {0x4500ec44, 0x41a7bdfb, 0x452a1b13}},
    TrackCase {29, 9600, 0u, {0x44ee6800, 0x432eca25, 0x452271f6}, {0x4500215c, 0x41a61e6e, 0x452af076}},
    TrackCase {29, 10240, 0u, {0x44ece1b6, 0x43349825, 0x45233452}, {0x44fed4a3, 0x41a63a46, 0x452bc1de}},
    TrackCase {29, 10880, 0u, {0x44eb9831, 0x4339ccc6, 0x452401d7}, {0x44fd8459, 0x41a84ad5, 0x452c9761}},
    TrackCase {29, 11520, 0u, {0x44ea9cc3, 0x433e2fb4, 0x4524d705}, {0x44fc5eba, 0x41ac8d52, 0x452d6c3a}},
    TrackCase {29, 12160, 0u, {0x44e9f827, 0x4341a89c, 0x4525b442}, {0x44fb68db, 0x41b33ae0, 0x452e3fec}},
    TrackCase {29, 12800, 0u, {0x44e9b318, 0x43441f2d, 0x452699ee}, {0x44faa7d0, 0x41bc8c9d, 0x452f11f9}},
    TrackCase {29, 13440, 1u, {0x44e9d55f, 0x4345689e, 0x45277f6a}, {0x44fa3c93, 0x41c645b4, 0x452fb6dc}},
    TrackCase {29, 14080, 1u, {0x44ea6c3f, 0x43452260, 0x45285609}, {0x44fa33c0, 0x41d15262, 0x45302baf}},
    TrackCase {29, 14720, 1u, {0x44eb7084, 0x43434f2f, 0x45291492}, {0x44fa6b17, 0x41e07933, 0x4530a1b6}},
    TrackCase {29, 15360, 1u, {0x44ecd738, 0x43401922, 0x4529bc72}, {0x44fadc8e, 0x41f32fdd, 0x453117eb}},
    TrackCase {29, 16000, 1u, {0x44ee94e6, 0x433bab81, 0x452a4ee4}, {0x44fb81f6, 0x420472ad, 0x45318d1a}},
    TrackCase {29, 16640, 1u, {0x44f09ea0, 0x43363055, 0x452acd5a}, {0x44fc554c, 0x421087ea, 0x45320043}},
    TrackCase {29, 17280, 1u, {0x44f2ea01, 0x432fd011, 0x452b394e}, {0x44fd50ca, 0x421d955b, 0x4532706e}},
    TrackCase {29, 17920, 1u, {0x44f569da, 0x4328baee, 0x452b93c6}, {0x44fe6d7c, 0x422b494b, 0x4532dc2a}},
    TrackCase {29, 18560, 1u, {0x44f8145e, 0x432117c4, 0x452bde63}, {0x44ffa5db, 0x4239650e, 0x453342a2}},
    TrackCase {29, 19200, 1u, {0x44fae360, 0x431902b6, 0x452c1ad5}, {0x45007b14, 0x4247bb46, 0x4533a364}},
    TrackCase {29, 19840, 1u, {0x44fdc079, 0x4310c6c3, 0x452c4998}, {0x45012983, 0x4255cc79, 0x4533fbdc}},
    TrackCase {29, 20480, 1u, {0x45005528, 0x43087243, 0x452c6cc0}, {0x4501dd80, 0x42638359, 0x45344c37}},
    TrackCase {29, 21120, 1u, {0x4501cae7, 0x43002fd6, 0x452c8581}, {0x45029409, 0x42709798, 0x4534933d}},
    TrackCase {29, 21760, 1u, {0x450336ee, 0x42f08bec, 0x452c94f5}, {0x450347a5, 0x427c9781, 0x4534cef8}},
    TrackCase {29, 22400, 1u, {0x4504971b, 0x42e19718, 0x452c9cbb}, {0x4503f6f1, 0x4283ae38, 0x4534ff05}},
    TrackCase {29, 23040, 1u, {0x4505e8db, 0x42d3b70c, 0x452c9e24}, {0x4504a068, 0x42885a2e, 0x45352294}},
    TrackCase {29, 23680, 1u, {0x4507234b, 0x42c7659f, 0x452c9a79}, {0x45053f57, 0x428c1ec9, 0x4535380c}},
    TrackCase {29, 24320, 1u, {0x45083bf9, 0x42bd23fa, 0x452c9347}, {0x4505ce37, 0x428ece78, 0x45353e5b}},
    TrackCase {29, 24960, 2u, {0x4509372f, 0x42b4eae6, 0x452c8998}, {0x45065421, 0x429068c0, 0x453533e1}},
    TrackCase {29, 25600, 2u, {0x450a2a09, 0x42b01055, 0x452c4d66}, {0x45075e45, 0x428fea70, 0x4534d032}},
    TrackCase {29, 26240, 2u, {0x450b29dc, 0x42aea57a, 0x452bc096}, {0x4508608e, 0x428b2921, 0x45340d08}},
    TrackCase {29, 26880, 2u, {0x450c2db3, 0x42afa64d, 0x452af5be}, {0x45095a15, 0x42833df2, 0x45330361}},
    TrackCase {29, 27520, 2u, {0x450d2c00, 0x42b20d52, 0x4529fffc}, {0x450a4967, 0x42728f4e, 0x4531cce8}},
    TrackCase {29, 28160, 2u, {0x450e1b3f, 0x42b4d3f4, 0x4528f2ad}, {0x450b2d17, 0x425ccf5d, 0x4530838a}},
    TrackCase {29, 28800, 2u, {0x450ef4c7, 0x42b6fc5b, 0x4527dd93}, {0x450c0683, 0x4247352a, 0x452f3ce1}},
    TrackCase {29, 29440, 2u, {0x450facc4, 0x42b77937, 0x4526d6ba}, {0x450cd203, 0x42343a80, 0x452e161a}},
    TrackCase {29, 30080, 2u, {0x45103a7f, 0x42b54c01, 0x4525f0b4}, {0x450d8eac, 0x42260c53, 0x452d27fe}},
    TrackCase {29, 30720, 3u, {0x4510b1e9, 0x42afc2b0, 0x45250a46}, {0x450dabb6, 0x42247518, 0x452d0565}},
    TrackCase {29, 31360, 3u, {0x45112aae, 0x42a76983, 0x4523f932}, {0x450dc91e, 0x42232c6e, 0x452cde68}},
    TrackCase {29, 32000, 3u, {0x45119dcd, 0x429d0fdb, 0x4522c974}, {0x450dea42, 0x4221fa69, 0x452cb190}},
    TrackCase {29, 32640, 3u, {0x451203f6, 0x42918b20, 0x452187c6}, {0x450e0ca4, 0x422101b9, 0x452c83ef}},
    TrackCase {29, 33280, 3u, {0x45125653, 0x4285a313, 0x45203f62}, {0x450e2dea, 0x42206400, 0x452c5a60}},
    TrackCase {29, 33920, 3u, {0x45128d55, 0x427476ab, 0x451efe8d}, {0x450e4b71, 0x42204410, 0x452c3a20}},
    TrackCase {29, 34560, 3u, {0x4512a24f, 0x42601926, 0x451dced3}, {0x450e6304, 0x4220c3ec, 0x452c27e7}},
    TrackCase {29, 35200, 3u, {0x45128d40, 0x424fbf67, 0x451cbab3}, {0x450e722e, 0x42220bd5, 0x452c290c}},
    TrackCase {29, 35840, 4u, {0x451247d2, 0x42453a7f, 0x451bd420}, {0x450e7d23, 0x4229d50a, 0x452c87f8}},
    TrackCase {29, 36480, 4u, {0x4511c506, 0x423d8415, 0x451aeee7}, {0x450e8a8c, 0x423c6a14, 0x452d7726}},
    TrackCase {29, 37120, 4u, {0x45110df2, 0x4237acd3, 0x451a067a}, {0x450e9175, 0x425095fb, 0x452e81b0}},
    TrackCase {29, 37760, 4u, {0x45102f58, 0x42335eaa, 0x45191dc6}, {0x450e932a, 0x4265d140, 0x452f9f40}},
    TrackCase {29, 38400, 4u, {0x450f34d6, 0x42303ef0, 0x4518369e}, {0x450e90f7, 0x427baec2, 0x4530c8e9}},
    TrackCase {29, 39040, 4u, {0x450e2bb8, 0x422df985, 0x45175472}, {0x450e8c29, 0x4288cd4a, 0x4531f5ae}},
    TrackCase {29, 39680, 4u, {0x450d202c, 0x422c35ef, 0x451679a0}, {0x450e860c, 0x42938d53, 0x45331def}},
    TrackCase {29, 40320, 4u, {0x450c1ea6, 0x422a9cc9, 0x4515a8c8}, {0x450e7fed, 0x429dd737, 0x453439b7}},
    TrackCase {29, 40960, 4u, {0x450b339c, 0x4228d6af, 0x4514e48c}, {0x450e7b18, 0x42a76ab4, 0x45354111}},
    TrackCase {29, 41600, 4u, {0x450a6b84, 0x42268c3c, 0x45142f8e}, {0x450e78da, 0x42b00785, 0x45362c05}},
    TrackCase {29, 41720, 4u, {0x450a4aff, 0x422608de, 0x45140fa1}, {0x450e78d0, 0x42b104f9, 0x453646d4}},
    TrackCase {29, -5, 0u, {0x45078338, 0x427f4cc0, 0x45193ad5}, {0x450d1ec0, 0x41f97ad0, 0x451fa9ea}},
    TrackCase {29, 100000, 4u, {0x450a4aff, 0x422608de, 0x45140fa1}, {0x450e7600, 0x42233c50, 0x452c3535}},
    TrackCase {39, 0, 0u, {0x44a9b8d8, 0x423f65b4, 0x4521d3b2}, {0x44aad913, 0x420d14f9, 0x45209fef}},
    TrackCase {39, 160, 0u, {0x44a9b932, 0x423ce98c, 0x4521c0fe}, {0x44aadc3a, 0x420cf524, 0x45209b10}},
    TrackCase {39, 320, 0u, {0x44a9b8ec, 0x423a30e3, 0x4521acd4}, {0x44aadf04, 0x420cd558, 0x4520957e}},
    TrackCase {39, 480, 0u, {0x44a9b85a, 0x42375ae4, 0x452197f2}, {0x44aae1a2, 0x420cb586, 0x45208f95}},
    TrackCase {39, 640, 0u, {0x44a9b7bf, 0x42348260, 0x45218301}, {0x44aae439, 0x420c95bc, 0x452089a6}},
    TrackCase {39, 800, 0u, {0x44a9b751, 0x4231bc37, 0x45216e85}, {0x44aae6e9, 0x420c7605, 0x452083ea}},
    TrackCase {39, 960, 0u, {0x44a9b734, 0x422f1896, 0x45215ae2}, {0x44aae9c4, 0x420c5676, 0x45207e91}},
    TrackCase {39, 1120, 0u, {0x44a9b782, 0x422ca572, 0x45214877}, {0x44aaecd7, 0x420c3743, 0x452079bf}},
    TrackCase {39, 1280, 0u, {0x44a9b84c, 0x422a637a, 0x4521373f}, {0x44aaf02f, 0x420c1822, 0x4520757d}},
    TrackCase {39, 1440, 0u, {0x44a9b995, 0x4228633c, 0x452127b6}, {0x44aaf3be, 0x420bf9d5, 0x452071ec}},
    TrackCase {39, 1600, 0u, {0x44a9bb5b, 0x42269faa, 0x452119b3}, {0x44aaf789, 0x420bdc17, 0x45206f04}},
    TrackCase {39, 1760, 0u, {0x44a9bda4, 0x4225135a, 0x45210d0f}, {0x44aafb9b, 0x420bbe98, 0x45206cb9}},
    TrackCase {39, 1920, 0u, {0x44a9c054, 0x4223c966, 0x45210228}, {0x44aaffc7, 0x420ba26b, 0x45206b16}},
    TrackCase {39, 2080, 1u, {0x44a9c360, 0x4222ba60, 0x4520f8d2}, {0x44ab042f, 0x420b86b6, 0x45206a06}},
    TrackCase {39, 2240, 1u, {0x44a9c7dd, 0x4221b041, 0x4520ef10}, {0x44ab0976, 0x420b6978, 0x452069a1}},
    TrackCase {39, 2400, 1u, {0x44a9cdfc, 0x42209e4a, 0x4520e477}, {0x44ab0f2a, 0x420b4dc3, 0x45206a08}},
    TrackCase {39, 2560, 1u, {0x44a9d57d, 0x421f872e, 0x4520d929}, {0x44ab152c, 0x420b335f, 0x45206b1e}},
    TrackCase {39, 2720, 1u, {0x44a9de1f, 0x421e6e20, 0x4520cd52}, {0x44ab1b67, 0x420b1a23, 0x45206cca}},
    TrackCase {39, 2880, 1u, {0x44a9e7aa, 0x421d5511, 0x4520c10f}, {0x44ab21bf, 0x420b01c8, 0x45206ef8}},
    TrackCase {39, 3040, 1u, {0x44a9f1d0, 0x421c407f, 0x4520b49a}, {0x44ab2814, 0x420aea42, 0x45207188}},
    TrackCase {39, 3200, 1u, {0x44a9fc64, 0x421b3176, 0x4520a803}, {0x44ab2e56, 0x420ad337, 0x45207466}},
    TrackCase {39, 3360, 1u, {0x44aa0713, 0x421a2c9e, 0x45209b8a}, {0x44ab345d, 0x420abc9c, 0x45207774}},
    TrackCase {39, 3520, 1u, {0x44aa11a7, 0x421933ea, 0x45208f48}, {0x44ab3a17, 0x420aa62c, 0x45207a9b}},
    TrackCase {39, 3680, 1u, {0x44aa1be0, 0x42184a37, 0x45208367}, {0x44ab3f68, 0x420a8fb7, 0x45207dc3}},
    TrackCase {39, 3840, 1u, {0x44aa2592, 0x42177102, 0x452077f9}, {0x44ab443e, 0x420a78e3, 0x452080d6}},
    TrackCase {39, 4000, 1u, {0x44aa2e4a, 0x4216af3f, 0x45206d5d}, {0x44ab4866, 0x420a61f4, 0x452083ad}},
    TrackCase {39, 4160, 1u, {0x44aa35f2, 0x421604c2, 0x45206397}, {0x44ab4bdb, 0x420a4a70, 0x45208639}},
    TrackCase {39, 4320, 2u, {0x44aa3d01, 0x42159040, 0x452059a7}, {0x44ab4cf4, 0x420a2ef6, 0x452088f4}},
    TrackCase {39, 4480, 2u, {0x44aa4404, 0x42158079, 0x45204ef9}, {0x44ab48b8, 0x420a1696, 0x45208bb5}},
    TrackCase {39, 4640, 2u, {0x44aa4a35, 0x4215985a, 0x4520453b}, {0x44ab42e4, 0x420a02cf, 0x45208e19}},
    TrackCase {39, 4800, 2u, {0x44aa4f2a, 0x4215b5a8, 0x45203d5a}, {0x44ab3daa, 0x4209f377, 0x45209000}},
    TrackCase {39, 4960, 2u, {0x44aa52ce, 0x4215ca58, 0x45203792}, {0x44ab39e0, 0x4209e827, 0x45209167}},
    TrackCase {39, 5120, 2u, {0x44aa553c, 0x4215d452, 0x452033be}, {0x44ab378e, 0x4209e06d, 0x45209257}},
    TrackCase {39, 5280, 2u, {0x44aa569e, 0x4215d735, 0x45203198}, {0x44ab3663, 0x4209dbe9, 0x452092df}},
    TrackCase {39, 5440, 2u, {0x44aa572f, 0x4215d788, 0x452030b8}, {0x44ab35f5, 0x4209da03, 0x45209318}},
    TrackCase {39, -5, 0u, {0x44a9b8d8, 0x423f65b4, 0x4521d3b2}, {0x44aad913, 0x420d14f9, 0x45209fef}},
    TrackCase {39, 100000, 2u, {0x44aa5732, 0x4215d788, 0x452030b2}, {0x44ab4cb7, 0x420a4365, 0x452086e7}},
};

std::optional<std::vector<uint8_t>> ReadCameraEdits()
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		return std::nullopt;
	}
	std::ifstream file(std::filesystem::path(game) / "Data" / "camera.edt", std::ios::binary);
	if (!file.is_open())
	{
		return std::nullopt;
	}
	return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

std::array<uint32_t, 3> Bits(const glm::vec3& v)
{
	return {std::bit_cast<uint32_t>(v.x), std::bit_cast<uint32_t>(v.y), std::bit_cast<uint32_t>(v.z)};
}

} // namespace

TEST(CameraTrackOriginal, TheGamesTracksGiveTheOriginalsEyeAndFocus)
{
	const auto bytes = ReadCameraEdits();
	if (!bytes)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set, or no Data/camera.edt in it";
	}
	edt::EDTFile file;
	ASSERT_EQ(file.Open(*bytes), edt::EDTResult::Success);

	for (const int32_t number : {13, 29, 39})
	{
		SCOPED_TRACE(number);
		const auto found = file.GetTracks().find(number);
		ASSERT_NE(found, file.GetTracks().end());
		const auto& track = found->second;
		WayRunner runner(track.position);
		std::vector<int32_t> samples;
		for (int32_t sample = 0; sample <= track.position.duration; sample += k_SampleStep)
		{
			samples.push_back(sample);
		}
		samples.insert(samples.end(), k_AfterTheSweep.begin(), k_AfterTheSweep.end());

		size_t checked = 0;
		for (const int32_t sample : samples)
		{
			const auto point = TrackPointAt(track, runner, sample);
			for (const auto& c : k_TrackCases)
			{
				if (c.track != number || c.sample != sample)
				{
					continue;
				}
				SCOPED_TRACE(sample);
				EXPECT_EQ(runner.GetSegment(), c.segment);
				EXPECT_EQ(Bits(point.position), c.position);
				EXPECT_EQ(Bits(point.focus), c.focus);
				++checked;
			}
		}
		EXPECT_EQ(checked, static_cast<size_t>(std::ranges::count(k_TrackCases, number, &TrackCase::track)));
	}
}
