/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The kinds of puff in the shared pool (Physics/TurnRules): how long each lives, its half size as it ages and the frame
// of the sheet it shows, pinned to the original's own numbers for every kind, at the edges of each curve and between.

#include <cstdint>

#include <array>
#include <bit>

#include <gtest/gtest.h>

#include "Physics/TurnRules.h"

using namespace openblack;
using openblack::physics::turn::PuffKind;

namespace
{
/// One puff drawn by the original: its kind (1 mote, 2 reversed mote, 3 smoke, 4 dust), the number from 0 to 15 it
/// was given, its age and largest half size, then the half size it was drawn at and its frame, every float as its bits
struct DrawCase
{
	int kind;
	uint32_t variant;
	uint32_t age;
	uint32_t size;
	uint32_t half;
	int32_t frame;
};

constexpr std::array<DrawCase, 200> k_DrawCases {{
    {1, 1u, 0x00000000u, 0x3F800000u, 0x00000000u, 1},   {1, 6u, 0x3C23D70Au, 0x40A00000u, 0x3E0AB6EFu, 6},
    {1, 11u, 0x3DCCCCCDu, 0x40000000u, 0x3F2625F3u, 11}, {1, 0u, 0x3DFDF3B6u, 0x40600000u, 0x3FB9E4F8u, 0},
    {1, 5u, 0x3E000000u, 0x41480000u, 0x40A78093u, 6},   {1, 10u, 0x3E010625u, 0x3F800000u, 0x3ED85B30u, 11},
    {1, 15u, 0x3E4CCCCDu, 0x40A00000u, 0x40605FEFu, 0},  {1, 4u, 0x3E7EF9DBu, 0x40000000u, 0x3FDCFF6Bu, 5},
    {1, 9u, 0x3E800000u, 0x40600000u, 0x40420A3Eu, 11},  {1, 14u, 0x3E808312u, 0x41480000u, 0x412D265Eu, 0},
    {1, 3u, 0x3E99999Au, 0x3F800000u, 0x3F53E8D4u, 5},   {1, 8u, 0x3F000000u, 0x40A00000u, 0x401E6666u, 12},
    {1, 13u, 0x3F333333u, 0x40000000u, 0x3EE665FAu, 2},  {1, 2u, 0x3F7FF972u, 0x40600000u, 0x3F93CEEFu, 9},
    {1, 7u, 0x3F800000u, 0x41480000u, 0x40840001u, 15},  {1, 12u, 0x3F800347u, 0x3F800000u, 0x3EA8FF08u, 4},
    {1, 1u, 0x3FA66666u, 0x40A00000u, 0x3FDA2463u, 11},  {1, 6u, 0x3FE00000u, 0x40000000u, 0x3DA8F5C3u, 4},
    {1, 11u, 0x3FFFDF3Bu, 0x40600000u, 0x3A96EB6Bu, 10}, {1, 0u, 0x40000000u, 0x41480000u, 0x00000000u, 0},
    {1, 5u, 0x3F25CD68u, 0x3F800000u, 0x3E890F1Au, 10},  {1, 10u, 0x3E9A7835u, 0x40A00000u, 0x4084292Du, 12},
    {1, 15u, 0x3FA6A3A4u, 0x40000000u, 0x3F2DD472u, 9},  {1, 4u, 0x3E14597Au, 0x40600000u, 0x3FDDAF09u, 5},
    {1, 9u, 0x3F892F90u, 0x41480000u, 0x40953985u, 1},   {1, 14u, 0x3F3B3B94u, 0x3F800000u, 0x3E57CFDFu, 3},
    {1, 3u, 0x3DED9048u, 0x40A00000u, 0x3FF61A33u, 3},   {1, 8u, 0x3F81E74Fu, 0x40000000u, 0x3F2E3663u, 0},
    {1, 13u, 0x3D99950Cu, 0x40600000u, 0x3F517D05u, 13}, {1, 2u, 0x3F5E06CFu, 0x41480000u, 0x403C50C3u, 8},
    {1, 7u, 0x3E0F105Cu, 0x3F800000u, 0x3EF338D7u, 8},   {1, 12u, 0x3E39C7BFu, 0x40A00000u, 0x404AADE5u, 13},
    {1, 1u, 0x3F595A94u, 0x40000000u, 0x3EE6EB22u, 7},   {1, 6u, 0x3FD3AC95u, 0x40600000u, 0x3E7123B8u, 3},
    {1, 11u, 0x3E7D8BE2u, 0x41480000u, 0x412BD166u, 12}, {1, 0u, 0x3EE498C1u, 0x3F800000u, 0x3F18E540u, 3},
    {1, 5u, 0x3FA09F77u, 0x40A00000u, 0x3FEC0582u, 15},  {1, 10u, 0x3FF29D0Eu, 0x40000000u, 0x3D44600Eu, 9},
    {1, 15u, 0x3F93BD05u, 0x40600000u, 0x3FB09383u, 8},  {1, 4u, 0x3F4B19B4u, 0x41480000u, 0x402526B1u, 10},
    {1, 9u, 0x3FF9EBDBu, 0x3F800000u, 0x3C5B05BBu, 8},   {1, 14u, 0x3DBECD7Bu, 0x40A00000u, 0x3FBF8A36u, 14},
    {1, 3u, 0x3FDBC497u, 0x40000000u, 0x3DC36B26u, 0},   {1, 8u, 0x3F1447ABu, 0x40600000u, 0x3F9FE7AFu, 12},
    {1, 13u, 0x3E93B79Bu, 0x41480000u, 0x4127CACCu, 15}, {1, 2u, 0x3E713D0Fu, 0x3F800000u, 0x3F526808u, 3},
    {1, 7u, 0x3F1DF154u, 0x40A00000u, 0x3FC22B59u, 11},  {1, 12u, 0x3FD0EDA8u, 0x40000000u, 0x3E1CC149u, 9},
    {1, 1u, 0x3EB91056u, 0x40600000u, 0x40276E10u, 3},   {1, 6u, 0x3F94E3C0u, 0x41480000u, 0x409DACA3u, 15},
    {2, 2u, 0x00000000u, 0x3F800000u, 0x00000000u, 13},  {2, 7u, 0x3C23D70Au, 0x40A00000u, 0x3E0AB6EFu, 8},
    {2, 12u, 0x3DCCCCCDu, 0x40000000u, 0x3F2625F3u, 3},  {2, 1u, 0x3DFDF3B6u, 0x40600000u, 0x3FB9E4F8u, 14},
    {2, 6u, 0x3E000000u, 0x41480000u, 0x40A78093u, 8},   {2, 11u, 0x3E010625u, 0x3F800000u, 0x3ED85B30u, 3},
    {2, 0u, 0x3E4CCCCDu, 0x40A00000u, 0x40605FEFu, 14},  {2, 5u, 0x3E7EF9DBu, 0x40000000u, 0x3FDCFF6Bu, 9},
    {2, 10u, 0x3E800000u, 0x40600000u, 0x40420A3Eu, 3},  {2, 15u, 0x3E808312u, 0x41480000u, 0x412D265Eu, 14},
    {2, 4u, 0x3E99999Au, 0x3F800000u, 0x3F53E8D4u, 9},   {2, 9u, 0x3F000000u, 0x40A00000u, 0x401E6666u, 2},
    {2, 14u, 0x3F333333u, 0x40000000u, 0x3EE665FAu, 12}, {2, 3u, 0x3F7FF972u, 0x40600000u, 0x3F93CEEFu, 5},
    {2, 8u, 0x3F800000u, 0x41480000u, 0x40840001u, 15},  {2, 13u, 0x3F800347u, 0x3F800000u, 0x3EA8FF08u, 10},
    {2, 2u, 0x3FA66666u, 0x40A00000u, 0x3FDA2463u, 3},   {2, 7u, 0x3FE00000u, 0x40000000u, 0x3DA8F5C3u, 10},
    {2, 12u, 0x3FFFDF3Bu, 0x40600000u, 0x3A96EB6Bu, 4},  {2, 1u, 0x40000000u, 0x41480000u, 0x00000000u, 14},
    {2, 6u, 0x3F25CD68u, 0x3F800000u, 0x3E890F1Au, 4},   {2, 11u, 0x3E9A7835u, 0x40A00000u, 0x4084292Du, 2},
    {2, 0u, 0x3FA6A3A4u, 0x40000000u, 0x3F2DD472u, 5},   {2, 5u, 0x3E14597Au, 0x40600000u, 0x3FDDAF09u, 9},
    {2, 10u, 0x3F892F90u, 0x41480000u, 0x40953985u, 13}, {2, 15u, 0x3F3B3B94u, 0x3F800000u, 0x3E57CFDFu, 11},
    {2, 4u, 0x3DED9048u, 0x40A00000u, 0x3FF61A33u, 11},  {2, 9u, 0x3F81E74Fu, 0x40000000u, 0x3F2E3663u, 14},
    {2, 14u, 0x3D99950Cu, 0x40600000u, 0x3F517D05u, 1},  {2, 3u, 0x3F5E06CFu, 0x41480000u, 0x403C50C3u, 6},
    {2, 8u, 0x3E0F105Cu, 0x3F800000u, 0x3EF338D7u, 6},   {2, 13u, 0x3E39C7BFu, 0x40A00000u, 0x404AADE5u, 1},
    {2, 2u, 0x3F595A94u, 0x40000000u, 0x3EE6EB22u, 7},   {2, 7u, 0x3FD3AC95u, 0x40600000u, 0x3E7123B8u, 11},
    {2, 12u, 0x3E7D8BE2u, 0x41480000u, 0x412BD166u, 2},  {2, 1u, 0x3EE498C1u, 0x3F800000u, 0x3F18E540u, 11},
    {2, 6u, 0x3FA09F77u, 0x40A00000u, 0x3FEC0582u, 15},  {2, 11u, 0x3FF29D0Eu, 0x40000000u, 0x3D44600Eu, 5},
    {2, 0u, 0x3F93BD05u, 0x40600000u, 0x3FB09383u, 6},   {2, 5u, 0x3F4B19B4u, 0x41480000u, 0x402526B1u, 4},
    {2, 10u, 0x3FF9EBDBu, 0x3F800000u, 0x3C5B05BBu, 6},  {2, 15u, 0x3DBECD7Bu, 0x40A00000u, 0x3FBF8A36u, 0},
    {2, 4u, 0x3FDBC497u, 0x40000000u, 0x3DC36B26u, 14},  {2, 9u, 0x3F1447ABu, 0x40600000u, 0x3F9FE7AFu, 2},
    {2, 14u, 0x3E93B79Bu, 0x41480000u, 0x4127CACCu, 15}, {2, 3u, 0x3E713D0Fu, 0x3F800000u, 0x3F526808u, 11},
    {2, 8u, 0x3F1DF154u, 0x40A00000u, 0x3FC22B59u, 3},   {2, 13u, 0x3FD0EDA8u, 0x40000000u, 0x3E1CC149u, 5},
    {2, 2u, 0x3EB91056u, 0x40600000u, 0x40276E10u, 11},  {2, 7u, 0x3F94E3C0u, 0x41480000u, 0x409DACA3u, 15},
    {3, 3u, 0x00000000u, 0x3F800000u, 0x00000000u, 19},  {3, 8u, 0x3C23D70Au, 0x40A00000u, 0x3E4BC6A8u, 24},
    {3, 13u, 0x3DCCCCCDu, 0x40000000u, 0x3F428F5Cu, 29}, {3, 2u, 0x3DFDF3B6u, 0x40600000u, 0x3FD06E5Du, 18},
    {3, 7u, 0x3E000000u, 0x41480000u, 0x40BB8000u, 23},  {3, 12u, 0x3E010625u, 0x3F800000u, 0x3EF1CA7Du, 28},
    {3, 1u, 0x3E4CCCCDu, 0x40A00000u, 0x40666666u, 17},  {3, 6u, 0x3E7EF9DBu, 0x40000000u, 0x3FDF3B43u, 22},
    {3, 11u, 0x3E800000u, 0x40600000u, 0x40440000u, 27}, {3, 0u, 0x3E808312u, 0x41480000u, 0x412EE667u, 16},
    {3, 5u, 0x3E99999Au, 0x3F800000u, 0x3F59999Au, 21},  {3, 10u, 0x3F000000u, 0x40A00000u, 0x40700000u, 27},
    {3, 15u, 0x3F333333u, 0x40000000u, 0x3FA66666u, 16}, {3, 4u, 0x3F7FF972u, 0x40600000u, 0x3FE005BCu, 21},
    {3, 9u, 0x3F800000u, 0x41480000u, 0x40C80000u, 27},  {3, 14u, 0x3F800347u, 0x3F800000u, 0x3EFFF972u, 16},
    {3, 3u, 0x3FA66666u, 0x40A00000u, 0x3FE00001u, 21},  {3, 8u, 0x3FE00000u, 0x40000000u, 0x3E800000u, 27},
    {3, 13u, 0x3FFFDF3Bu, 0x40600000u, 0x3AE56300u, 16}, {3, 2u, 0x40000000u, 0x41480000u, 0x00000000u, 22},
    {3, 7u, 0x3F25CD68u, 0x3F800000u, 0x3F2D194Cu, 24},  {3, 12u, 0x3E9A7835u, 0x40A00000u, 0x4087DD38u, 28},
    {3, 1u, 0x3FA6A3A4u, 0x40000000u, 0x3F32B8B8u, 19},  {3, 6u, 0x3E14597Au, 0x40600000u, 0x3FF0CE6Eu, 22},
    {3, 11u, 0x3F892F90u, 0x41480000u, 0x40B9A5AFu, 29}, {3, 0u, 0x3F3B3B94u, 0x3F800000u, 0x3F226236u, 17},
    {3, 5u, 0x3DED9048u, 0x40A00000u, 0x400BDDA1u, 21},  {3, 10u, 0x3F81E74Fu, 0x40000000u, 0x3F7C3162u, 28},
    {3, 15u, 0x3D99950Cu, 0x40600000u, 0x3F81587Au, 31}, {3, 4u, 0x3F5E06CFu, 0x41480000u, 0x40E28AAEu, 21},
    {3, 9u, 0x3E0F105Cu, 0x3F800000u, 0x3F0511F3u, 25},  {3, 14u, 0x3E39C7BFu, 0x40A00000u, 0x405328D2u, 30},
    {3, 3u, 0x3F595A94u, 0x40000000u, 0x3F9352B6u, 20},  {3, 8u, 0x3FD3AC95u, 0x40600000u, 0x3F1B23F6u, 27},
    {3, 13u, 0x3E7D8BE2u, 0x41480000u, 0x412D8F60u, 29}, {3, 2u, 0x3EE498C1u, 0x3F800000u, 0x3F46D9D0u, 18},
    {3, 7u, 0x3FA09F77u, 0x40A00000u, 0x3FEE7156u, 25},  {3, 12u, 0x3FF29D0Eu, 0x40000000u, 0x3DD62F20u, 31},
    {3, 1u, 0x3F93BD05u, 0x40600000u, 0x3FBD7537u, 19},  {3, 6u, 0x3F4B19B4u, 0x41480000u, 0x40F153EBu, 23},
    {3, 11u, 0x3FF9EBDBu, 0x3F800000u, 0x3CC284A0u, 30}, {3, 0u, 0x3DBECD7Bu, 0x40A00000u, 0x3FE364A8u, 16},
    {3, 5u, 0x3FDBC497u, 0x40000000u, 0x3E90EDA4u, 24},  {3, 10u, 0x3F1447ABu, 0x40600000u, 0x401F20A5u, 27},
    {3, 15u, 0x3E93B79Bu, 0x41480000u, 0x412B2624u, 31}, {3, 4u, 0x3E713D0Fu, 0x3F800000u, 0x3F54D28Eu, 20},
    {3, 9u, 0x3F1DF154u, 0x40A00000u, 0x405D492Cu, 26},  {3, 14u, 0x3FD0EDA8u, 0x40000000u, 0x3EBC4960u, 17},
    {3, 3u, 0x3EB91056u, 0x40600000u, 0x4037846Du, 19},  {3, 8u, 0x3F94E3C0u, 0x41480000u, 0x40A75C24u, 26},
    {4, 4u, 0x00000000u, 0x3F800000u, 0x00000000u, 20},  {4, 9u, 0x3C23D70Au, 0x40A00000u, 0x3ECAC083u, 25},
    {4, 14u, 0x3DCCCCCDu, 0x40000000u, 0x3FB851EBu, 30}, {4, 3u, 0x3DFDF3B6u, 0x40600000u, 0x4042A77Au, 19},
    {4, 8u, 0x3E000000u, 0x41480000u, 0x412F0000u, 24},  {4, 13u, 0x3E010625u, 0x3F800000u, 0x3F5FBE77u, 29},
    {4, 2u, 0x3E4CCCCDu, 0x40A00000u, 0x40800000u, 18},  {4, 7u, 0x3E7EF9DBu, 0x40000000u, 0x3FC04189u, 23},
    {4, 12u, 0x3E800000u, 0x40600000u, 0x40280000u, 28}, {4, 1u, 0x3E808312u, 0x41480000u, 0x4115CCCDu, 17},
    {4, 6u, 0x3E99999Au, 0x3F800000u, 0x3F333333u, 22},  {4, 11u, 0x3F000000u, 0x40A00000u, 0x40200000u, 28},
    {4, 0u, 0x3F333333u, 0x40000000u, 0x3F19999Au, 17},  {4, 5u, 0x3F7FF972u, 0x40600000u, 0x39B78800u, 22},
    {4, 10u, 0x3F800000u, 0x41480000u, 0x00000000u, 28}, {4, 15u, 0x3F800347u, 0x3F800000u, 0xB8D1C000u, 17},
    {4, 4u, 0x3FA66666u, 0x40A00000u, 0xBFBFFFFEu, 22},  {4, 9u, 0x3FE00000u, 0x40000000u, 0xBFC00000u, 28},
    {4, 14u, 0x3FFFDF3Bu, 0x40600000u, 0xC05FC6A7u, 17}, {4, 3u, 0x40000000u, 0x41480000u, 0xC1480000u, 23},
    {4, 8u, 0x3F25CD68u, 0x3F800000u, 0x3EB46530u, 25},  {4, 13u, 0x3E9A7835u, 0x40A00000u, 0x405F74E0u, 29},
    {4, 2u, 0x3FA6A3A4u, 0x40000000u, 0xBF1A8E90u, 20},  {4, 7u, 0x3E14597Au, 0x40600000u, 0x403F8C6Eu, 23},
    {4, 12u, 0x3F892F90u, 0x41480000u, 0xBF65A510u, 30}, {4, 1u, 0x3F3B3B94u, 0x3F800000u, 0x3E8988D8u, 18},
    {4, 6u, 0x3DED9048u, 0x40A00000u, 0x40834114u, 22},  {4, 11u, 0x3F81E74Fu, 0x40000000u, 0xBCF3A780u, 29},
    {4, 0u, 0x3D99950Cu, 0x40600000u, 0x3FF89D11u, 16},  {4, 5u, 0x3F5E06CFu, 0x41480000u, 0x3FD45572u, 22},
    {4, 10u, 0x3E0F105Cu, 0x3F800000u, 0x3F5C3BE9u, 26}, {4, 15u, 0x3E39C7BFu, 0x40A00000u, 0x4082F8CAu, 31},
    {4, 4u, 0x3F595A94u, 0x40000000u, 0x3E9A95B0u, 21},  {4, 9u, 0x3FD3AC95u, 0x40600000u, 0xC0126E05u, 28},
    {4, 14u, 0x3E7D8BE2u, 0x41480000u, 0x41167AAEu, 30}, {4, 3u, 0x3EE498C1u, 0x3F800000u, 0x3F0DB3A0u, 19},
    {4, 8u, 0x3FA09F77u, 0x40A00000u, 0xBFA31D53u, 26},  {4, 13u, 0x3FF29D0Eu, 0x40000000u, 0xBFE53A1Cu, 16},
    {4, 2u, 0x3F93BD05u, 0x40600000u, 0xBF0A2B23u, 20},  {4, 7u, 0x3F4B19B4u, 0x41480000u, 0x40254FAEu, 24},
    {4, 12u, 0x3FF9EBDBu, 0x3F800000u, 0xBF73D7B6u, 31}, {4, 1u, 0x3DBECD7Bu, 0x40A00000u, 0x40584878u, 17},
    {4, 6u, 0x3FDBC497u, 0x40000000u, 0xBFB7892Eu, 25},  {4, 11u, 0x3F1447ABu, 0x40600000u, 0x3FBC8295u, 28},
    {4, 0u, 0x3E93B79Bu, 0x41480000u, 0x410E4C47u, 16},  {4, 5u, 0x3E713D0Fu, 0x3F800000u, 0x3F43B0BCu, 21},
    {4, 10u, 0x3F1DF154u, 0x40A00000u, 0x3FF524AEu, 27}, {4, 15u, 0x3FD0EDA8u, 0x40000000u, 0xBFA1DB50u, 18},
    {4, 4u, 0x3EB91056u, 0x40600000u, 0x400F08DAu, 20},  {4, 9u, 0x3F94E3C0u, 0x41480000u, 0xC0028F70u, 27},
}};

PuffKind KindOf(int kind)
{
	switch (kind)
	{
	case 1:
		return PuffKind::Mote;
	case 2:
		return PuffKind::MoteReversed;
	case 3:
		return PuffKind::Smoke;
	default:
		return PuffKind::Dust;
	}
}
} // namespace

TEST(DustPuffKinds, EachKindIsDrawnAsTheOriginalDrawsIt)
{
	for (const auto& c : k_DrawCases)
	{
		const auto kind = KindOf(c.kind);
		const auto age = std::bit_cast<float>(c.age);
		const auto size = std::bit_cast<float>(c.size);
		EXPECT_EQ(std::bit_cast<uint32_t>(physics::turn::DustPuffSize(kind, age, size)), c.half)
		    << "kind " << c.kind << " age " << age << " size " << size;
		EXPECT_EQ(physics::turn::DustPuffFrame(kind, c.variant, age), c.frame)
		    << "kind " << c.kind << " variant " << c.variant << " age " << age;
	}
}

TEST(DustPuffKinds, DustLivesASecondAndTheCreaturesPuffsTwo)
{
	// a puff goes once its age is past its life: at its life it is still there
	EXPECT_EQ(physics::turn::PuffLife(PuffKind::Dust), 1.0f);
	EXPECT_EQ(physics::turn::PuffLife(PuffKind::Smoke), 2.0f);
	EXPECT_EQ(physics::turn::PuffLife(PuffKind::Mote), 2.0f);
	EXPECT_EQ(physics::turn::PuffLife(PuffKind::MoteReversed), 2.0f);
	// only the motes are added to what is behind them
	EXPECT_TRUE(physics::turn::IsAdditive(PuffKind::Mote));
	EXPECT_TRUE(physics::turn::IsAdditive(PuffKind::MoteReversed));
	EXPECT_FALSE(physics::turn::IsAdditive(PuffKind::Smoke));
	EXPECT_FALSE(physics::turn::IsAdditive(PuffKind::Dust));
}

TEST(DustPuffKinds, ANewPuffHasNoSizeAndShowsTheFrameItWasGiven)
{
	EXPECT_EQ(physics::turn::DustPuffSize(PuffKind::Dust, 0.0f, 5.0f), 0.0f);
	EXPECT_EQ(physics::turn::DustPuffSize(PuffKind::Mote, 0.0f, 5.0f), 0.0f);
	EXPECT_EQ(physics::turn::DustPuffFrame(PuffKind::Dust, 3, 0.0f), 16 + 3);
	EXPECT_EQ(physics::turn::DustPuffFrame(PuffKind::Smoke, 15, 0.0f), 16 + 15);
	EXPECT_EQ(physics::turn::DustPuffFrame(PuffKind::Mote, 3, 0.0f), 3);
	EXPECT_EQ(physics::turn::DustPuffFrame(PuffKind::MoteReversed, 3, 0.0f), 12);
}
