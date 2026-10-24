/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The original's x87 arithmetic (Common/GUtilsX87): the rounding of each operation to a float's mantissa, the sine and
// cosine kept unrounded until the first operation on them. The tables are the original's own bits: the FPU's sine and
// cosine and their products and sums, run on the processor at the game's control word, and three of the game's routines
// (the fish farm's swim step, its shoal's scare and the fish puzzle's net) run in the original executable, with the
// same steps written below in the original's order. The arctangent, the arccosine, the power and the logarithm are
// pinned to the processor's instructions and the C runtime's own functions, run in the original executable.

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <limits>

#include <gtest/gtest.h>

#include "Common/GUtilsX87.h"
#include "X87RuntimeGolden.h"

namespace x87 = openblack::gutils::x87;
namespace golden = openblack::test::x87_runtime_golden;

namespace
{

[[nodiscard]] float F(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}

[[nodiscard]] uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

[[nodiscard]] uint64_t Bits(double value)
{
	return std::bit_cast<uint64_t>(value);
}

// angle bits, then the FPU's sine and cosine (80 bits) as the two doubles that sum to them exactly
struct X87SinCosCase
{
	uint32_t angle;
	uint64_t sinHigh;
	uint64_t sinLow;
	uint64_t cosHigh;
	uint64_t cosLow;
};
constexpr X87SinCosCase k_SinCosCases[] = {
    {0x00000000u, 0x0000000000000000u, 0x0000000000000000u, 0x3FF0000000000000u, 0x0000000000000000u},
    {0x80000000u, 0x8000000000000000u, 0x0000000000000000u, 0x3FF0000000000000u, 0x0000000000000000u},
    {0x00000001u, 0x36A0000000000000u, 0x0000000000000000u, 0x3FF0000000000000u, 0x0000000000000000u},
    {0x80000001u, 0xB6A0000000000000u, 0x0000000000000000u, 0x3FF0000000000000u, 0x0000000000000000u},
    {0x006CE3EEu, 0x380B38FB80000000u, 0x0000000000000000u, 0x3FF0000000000000u, 0x0000000000000000u},
    {0x0DA24260u, 0x39B4484C00000000u, 0x0000000000000000u, 0x3FF0000000000000u, 0x0000000000000000u},
    {0x2EDBE6FFu, 0x3DDB7CDFE0000000u, 0x0000000000000000u, 0x3FF0000000000000u, 0x0000000000000000u},
    {0x38D1B717u, 0x3F1A36E2DF44599Cu, 0xBBBC700000000000u, 0x3FEFFFFFFD50CE26u, 0x3C54000000000000u},
    {0x3F000000u, 0x3FDEAEE8744B05F0u, 0xBC57800000000000u, 0x3FEC1528065B7D50u, 0xBC88900000000000u},
    {0x3F800000u, 0x3FEAED548F090CEEu, 0x3C40800000000000u, 0x3FE14A280FB5068Cu, 0xBC8B700000000000u},
    {0x40000000u, 0x3FED18F6EAD1B446u, 0xBC70300000000000u, 0xBFDAA22657537205u, 0x3C76F00000000000u},
    {0x40490FDBu, 0xBE7777A5CF72CFF8u, 0x3B1A800000000000u, 0xBFEFFFFFFFFFFFDEu, 0x3C8AE00000000000u},
    {0x40C90FDBu, 0x3E8777A5CF72CFDEu, 0x3B26080000000000u, 0x3FEFFFFFFFFFFF76u, 0x3C84880000000000u},
    {0x501502F9u, 0xBFDF334C78938DE5u, 0xBC78D00000000000u, 0x3FEBF098901D6FEDu, 0xBC82580000000000u},
    {0x5D5E0B6Bu, 0xBFCB94E0A91D0FAFu, 0xBC67700000000000u, 0x3FEF3F8DD41FDDF4u, 0x3C7F900000000000u},
    {0x5F01103Du, 0x43E02207A0000000u, 0x0000000000000000u, 0x43E02207A0000000u, 0x0000000000000000u},
    {0xDF01103Du, 0xC3E02207A0000000u, 0x0000000000000000u, 0xC3E02207A0000000u, 0x0000000000000000u},
    {0x7149F2CAu, 0x46293E5940000000u, 0x0000000000000000u, 0x46293E5940000000u, 0x0000000000000000u},
    {0x7F7FC99Eu, 0x47EFF933C0000000u, 0x0000000000000000u, 0x47EFF933C0000000u, 0x0000000000000000u},
    {0x40B9035Au, 0xBFDEC4D7C00D0A95u, 0xBC44000000000000u, 0x3FEC0F276AAC4B5Bu, 0x3C76500000000000u},
    {0x40224942u, 0x3FE2392B13D2D5BDu, 0xBC72500000000000u, 0xBFEA4DDFEEE5CF1Eu, 0x3C8E100000000000u},
    {0xC0EC98C1u, 0xBFECAB3E5D86D8B5u, 0xBC8CA80000000000u, 0x3FDC6E8FF0AC4EB2u, 0xBC75A00000000000u},
    {0xC0F911A6u, 0xBFEFEB9B0CDED5FAu, 0x3C84680000000000u, 0x3FB20D838E35D6F7u, 0xBC3EE00000000000u},
    {0xC0F7FAC2u, 0xBFEFD3351987D06Fu, 0x3C79E00000000000u, 0x3FBABBFA0180D190u, 0xBC55F00000000000u},
    {0x3FCF4E4Fu, 0x3FEFF6415269B9C9u, 0xBC66200000000000u, 0xBFA8F74863113021u, 0x3C49B00000000000u},
    {0xC02A986Eu, 0xBFDD542B2FF20F29u, 0x3C79400000000000u, 0xBFEC713068C6A183u, 0xBC70A00000000000u},
    {0x403CCC69u, 0x3FC86088E3ED6A09u, 0xBC66F00000000000u, 0xBFEF6A113C5A17CDu, 0x3C82F00000000000u},
    {0x40CA4198u, 0x3FA31AB3292746C1u, 0x3C4FE80000000000u, 0x3FEFFA4B984F4E2Eu, 0x3C65C00000000000u},
    {0x40C2687Fu, 0xBFCA6C7115AF2D50u, 0xBC58200000000000u, 0x3FEF4F8C48BDE687u, 0x3C87900000000000u},
    {0x3ED4B843u, 0x3FD9D4E26FFE7280u, 0xBC61700000000000u, 0x3FED4715D5E1AEC7u, 0x3C71E00000000000u},
    {0x40DC6DE8u, 0x3FE234DAEB602B23u, 0x3C7C400000000000u, 0x3FEA50DC5C6389E6u, 0xBC52400000000000u},
    {0x40EF20D1u, 0x3FEDB3E3F8E5E978u, 0x3C7C200000000000u, 0x3FD7CFCE5CC28AAEu, 0xBC77C00000000000u},
    {0x3C979291u, 0x3F92F20B47875627u, 0xBC38F80000000000u, 0x3FEFFE9909B3460Fu, 0xBC8D180000000000u},
    {0x3F91109Eu, 0x3FECFC81AB9D0374u, 0xBC81300000000000u, 0x3FDB1D2A8972D798u, 0x3C60900000000000u},
    {0x3FBBBD81u, 0x3FEFD3AC59997850u, 0x3C82680000000000u, 0x3FBA9864C24E7B7Eu, 0xBC31A00000000000u},
    {0x400D6016u, 0x3FE9B398B682D34Fu, 0x3C8DA80000000000u, 0xBFE310587B011847u, 0xBC8FB00000000000u},
    {0xC0D615EEu, 0xBFD955950184E398u, 0xBC7E180000000000u, 0x3FED62D7DE50F080u, 0xBC7E600000000000u},
    {0x40FF3ADDu, 0x3FEFC3282023FCDAu, 0x3C86000000000000u, 0xBFBF2491B890FAFAu, 0xBC52580000000000u},
    {0xC0D1B16Du, 0xBFD10DC16AB779F0u, 0xBC74000000000000u, 0x3FEED7D0DE843AF9u, 0xBC80180000000000u},
    {0xC445EB9Du, 0xBF1C8671CD1B7A76u, 0xBBB4880000000000u, 0x3FEFFFFFFCD25080u, 0xBC86F00000000000u},
    {0x443C1A53u, 0xBFEFFFFFFBD4D4E6u, 0xBC85480000000000u, 0xBF2055720B57F071u, 0xBBCBD80000000000u},
    {0xC53681C5u, 0x3FEFFFFFF2820941u, 0x3C89680000000000u, 0x3F2D629D8CF5AD2Cu, 0x3BC7080000000000u},
    {0x44255386u, 0x3FEFFFFFF546CD11u, 0x3C8F800000000000u, 0x3F2A3283C23060B1u, 0x3BA3000000000000u},
    {0xC4EFBE25u, 0xBFEFFFFFE8186D2Du, 0x3C73F00000000000u, 0x3F338E92AC6DE065u, 0x3BD2700000000000u},
    {0x442D92AEu, 0x3F1BDF6D7618E859u, 0xBBBEB80000000000u, 0xBFEFFFFFFCF71BE9u, 0xBC5EC00000000000u},
    {0x44603B2Cu, 0xBFEFFFFFFA954E42u, 0xBC89880000000000u, 0xBF229E821197BBB2u, 0xBB94400000000000u},
    {0xC4943E70u, 0x3FEFFFFFFF313794u, 0x3C80100000000000u, 0xBF0CC2887F0E2654u, 0xBB9AC00000000000u},
    {0x493CE44Du, 0x3FEDD78E0C434DB7u, 0xBC8EF80000000000u, 0xBFD71ABFDD24DD19u, 0xBC46000000000000u},
    {0x495281DFu, 0x3FDB4BA82C75F77Eu, 0x3C73700000000000u, 0xBFECF19716280DC6u, 0xBC72E00000000000u},
    {0x494737E0u, 0x3FE5333B67E046CAu, 0xBC52800000000000u, 0x3FE7F8496E9F5E37u, 0x3C68400000000000u},
    {0xC9640A9Au, 0x3FE4AEBCC83D4999u, 0xBC75100000000000u, 0x3FE86AF83061BEB0u, 0xBC47000000000000u},
    {0x4967213Cu, 0x3FEF5BC07FD284E9u, 0x3C83A00000000000u, 0x3FC980C857167A77u, 0xBC2E000000000000u},
    {0x4913E82Eu, 0x3FEAD2405CAD9B00u, 0xBC80900000000000u, 0xBFE1740C5CB3EC08u, 0x3C08000000000000u},
    {0xD43F6A27u, 0xBFE11FF6C506DE83u, 0xBC8A880000000000u, 0x3FEB083D7C635928u, 0x3C69E00000000000u},
    {0x489247C4u, 0xBFC32F3FC5F37147u, 0xBC60A00000000000u, 0x3FEFA377297B8349u, 0xBC8B000000000000u},
};

// angles whose FPU sine or cosine is one 64th bit off ours: angle, multiplier, then the FPU's
// sine x multiplier and cosine x multiplier stored to floats
struct X87LastBitCase
{
	uint32_t angle;
	uint32_t times;
	uint32_t sinTimes;
	uint32_t cosTimes;
};
constexpr X87LastBitCase k_LastBitCases[] = {
    {0xC0863104u, 0xC0FA6DEBu, 0xC0D976A5u, 0x40786414u}, {0x40861795u, 0x45E6BC51u, 0xC5C80183u, 0xC56619BAu},
    {0xC0EDE11Au, 0x45AAB09Au, 0xC59BD65Eu, 0x450B47B7u}, {0xBF151E59u, 0xC4EC2C00u, 0x4481EB91u, 0xC4C539BBu},
    {0x40A643E8u, 0x3F84F372u, 0xBF6B6F4Du, 0x3EF72DB1u}, {0x3FE945B8u, 0xBF3060D5u, 0xBF2AD2B8u, 0x3E2FAB8Fu},
    {0x4082AFFFu, 0xC3C11FAFu, 0x439C3B07u, 0x43630EC2u}, {0x4036D863u, 0x3C08449Bu, 0x3B190F73u, 0xBC02C8FEu},
    {0x400887BEu, 0x41C98317u, 0x41AA7718u, 0xC156EE15u}, {0x40B15BB4u, 0x40713E0Eu, 0xC022CC7Fu, 0x40320782u},
    {0xC084A0DFu, 0x3F25263Eu, 0x3F0B3D4Eu, 0xBEB19D33u}, {0x40BDB17Fu, 0x3B03E048u, 0xBA377CF7u, 0x3AF7481Eu},
};

// angle a, multiplier m, angle b, then the FPU's sin(a) x m, cos(a) x m, sin(a) + sin(b) and
// cos(a) + 1, each from the unrounded sine or cosine, rounded once, stored to a float
struct X87OperationCase
{
	uint32_t a;
	uint32_t m;
	uint32_t b;
	uint32_t sinTimes;
	uint32_t cosTimes;
	uint32_t sinPlusSin;
	uint32_t cosPlusOne;
};
constexpr X87OperationCase k_OperationCases[] = {
    {0x40F3D6D4u, 0x4322DE19u, 0xC0F3D731u, 0x431E6DB9u, 0x42170FB8u, 0xB72C803Du, 0x3F9DAE2Bu},
    {0x3FCB17BFu, 0x3C35F19Du, 0xBFCB17C4u, 0x3C35EBC0u, 0xB938BD96u, 0x32227667u, 0x3F7BF042u},
    {0xBFFDD14Bu, 0x3A8C2356u, 0x3FFDCF4Eu, 0xBA806726u, 0xB9E08C3Bu, 0x37CBE1F9u, 0x3F197361u},
    {0x4096CE0Fu, 0xBD87168Cu, 0xC096CF8Du, 0x3D87168Cu, 0xB79270A1u, 0xB38B26B1u, 0x3F8008ACu},
    {0x3FD371F0u, 0xBC25909Fu, 0xBFD371EAu, 0xBC250540u, 0x3A56A65Du, 0xB378EBC9u, 0x3F6B41A9u},
    {0xC049110Cu, 0xBE6AE434u, 0x404911BAu, 0xB78C1802u, 0x3E6AE434u, 0xB82E0000u, 0x31362067u},
    {0x3F1BDF40u, 0x4287F105u, 0xBF1BDF27u, 0x421B808Eu, 0x425F05FCu, 0x35A40EF7u, 0x3FE8FF53u},
    {0xC002C202u, 0x408FC5B9u, 0x4002C1C2u, 0xC0800879u, 0xC002D025u, 0x36E8EBDBu, 0x3F0B89A1u},
    {0x40F2DC38u, 0x3EA722CDu, 0xC0F2DC21u, 0x3EA151B5u, 0x3DAED6E1u, 0x36407C0Fu, 0x3FA17993u},
    {0x3FC910E5u, 0x3F06182Eu, 0xBFC91188u, 0x3F06182Eu, 0xB78B864Bu, 0x305D7E45u, 0x3F7FFDEBu},
    {0xC0491122u, 0xBC9F2F12u, 0x4049108Eu, 0xB5CB8F7Eu, 0x3C9F2F12u, 0x38140000u, 0x31515079u},
    {0x3FC90067u, 0xC54DBF10u, 0xBFC8FF4Cu, 0xC54DBF0Fu, 0xBFC6B211u, 0x328D8A05u, 0x3F800F74u},
    {0x3FD3DD89u, 0xC2C04712u, 0xBFD3DD2Cu, 0xC2BF97DAu, 0x4101ACB7u, 0xB57ADD8Fu, 0x3F6A6B34u},
    {0xBF79BA72u, 0xBE639F3Au, 0x3F79BA6Bu, 0x3E3C7796u, 0xBDFF478Cu, 0xB47B37ADu, 0x3FC7C6C2u},
    {0xBFCC4892u, 0xBD51A721u, 0x3FCC4704u, 0x3D519622u, 0x3AA8D577u, 0x35A01AA5u, 0x3F798EBEu},
    {0x3F8BEAB8u, 0x3BCE3E99u, 0xBF8BEAF1u, 0x3BB72825u, 0x3B3DA2A2u, 0xB651A32Bu, 0x3FBAD891u},
    {0xBF9A3BEAu, 0xC3320616u, 0x3F9A3ADCu, 0x43263E1Cu, 0xC27EBE10u, 0xB7412F93u, 0x3FADCA50u},
    {0xC0490ED5u, 0x3C002D6Du, 0x40491274u, 0xB502FF7Eu, 0xBC002D6Du, 0xB967C000u, 0x3105B1FFu},
    {0x3FBCFB42u, 0x403D81C1u, 0xBFBCFBCBu, 0x403CA9DAu, 0x3E8EDEE8u, 0xB5CE8D8Fu, 0x3F8C1002u},
    {0xBFBD2D9Au, 0xC54C6BBFu, 0x3FBD2B9Bu, 0x454B8A5Eu, 0xC3979CEEu, 0xB6BD8F13u, 0x3F8BDDE2u},
    {0x44FA2639u, 0x3F13E3AFu, 0x404BAFBCu, 0x3817062Du, 0xBF13E3AFu, 0xBD27AAEFu, 0x31057BDBu},
    {0x426EC2D6u, 0xC32E7F7Eu, 0xC0DDDC2Cu, 0x3ACF0928u, 0x432E7F7Eu, 0xBF1AEB05u, 0x2E342F7Bu},
    {0xC45B2047u, 0xBE025242u, 0x3EDAE952u, 0x360FF543u, 0x3E025242u, 0x3ED44B2Du, 0x2F1C306Fu},
    {0xC29D1465u, 0x41673EFDu, 0xC077FA85u, 0x397D8750u, 0xC1673EFDu, 0x3F2B4EECu, 0x2F19DB33u},
    {0x44502169u, 0x41C6FBBFu, 0x4098D372u, 0x39FE6B90u, 0xC1C6FBBFu, 0xBF7F7AF6u, 0x2F5141B9u},
    {0x43858486u, 0x3AD4AA24u, 0x40DB1B27u, 0x337C42C1u, 0xBAD4AA24u, 0x3F08D553u, 0x30341A27u},
    {0x43C1353Au, 0xC5CB03B2u, 0xC075C0EFu, 0xBED4734Bu, 0x45CB03B2u, 0x3F249AC5u, 0x310C2CD7u},
    {0xC4D3AA32u, 0x3B4AD6E7u, 0x3FB46EB7u, 0x35079DF7u, 0xBB4AD6E7u, 0x3F7CB964u, 0x3264DF5Fu},
    {0x40923726u, 0x44D334FFu, 0xBF6F623Cu, 0xC4D10BE6u, 0xC3710F8Du, 0xBFE5AFB5u, 0x3F5B7A13u},
    {0x3E9F3072u, 0x3BF79FE9u, 0x3F8EF3ECu, 0x3B178324u, 0x3BEBC07Fu, 0x3F9A31AFu, 0x3FF9DCE4u},
    {0xBF92FD2Bu, 0x454217D8u, 0x401DEB0Cu, 0xC53107BEu, 0x449F273Cu, 0xBE9364BBu, 0x3FB47A9Eu},
    {0xC0CDE5BFu, 0x3AC33AB4u, 0xC0876D43u, 0xB96B1C88u, 0x3AC1012Du, 0x3F3C7F7Cu, 0x3FFE8A99u},
    {0x402492FCu, 0x46190F69u, 0xC06C73B4u, 0x45A53942u, 0xC600D9FCu, 0x3F88504Du, 0x3E21F5E4u},
    {0xC06AE24Fu, 0x3F4BFD9Cu, 0xC0E73800u, 0x3ECDB59Bu, 0xBF302982u, 0xBE9C095Bu, 0x3E0BB1D3u},
    {0x40B0C518u, 0x422F948Eu, 0xC0ECA07Au, 0xC1F1B313u, 0x41FEBEC4u, 0xBFCAD47Fu, 0x3FDCDB26u},
    {0x40B7A5A6u, 0xC541D246u, 0xC0FC345Du, 0x44C8B35Au, 0xC525D221u, 0xBFC23933u, 0x3FED8225u},
    {0x4000944Bu, 0xC485A59Eu, 0x40736026u, 0xC472086Du, 0x43E2DB89u, 0x3E953B4Eu, 0x3F135D28u},
    {0x40827E52u, 0xBBA4131Cu, 0x40F0D230u, 0x3B8424BDu, 0x3B4282AFu, 0x3E109618u, 0x3ED08314u},
    {0xBF0A8ADDu, 0x4127C11Au, 0xC0ABC4F3u, 0xC0ACD63Fu, 0x410FC847u, 0x3E8E289Fu, 0x3FEDB57Au},
    {0x408E42C2u, 0xBF3E10F3u, 0x40378B86u, 0x3F375837u, 0x3E48658Cu, 0xBF31C024u, 0x3F3C8580u},
    {0x408415B2u, 0xC312E02Du, 0x403E972Au, 0x42F4F236u, 0x42A22553u, 0xBF2BC48Fu, 0x3EE56278u},
    {0x40E5D003u, 0xBC986A71u, 0xC046A5CBu, 0xBC6E7D75u, 0xBC3DDAC8u, 0x3F3EA186u, 0x3FCFB881u},
    {0x3FF7D9BDu, 0xC226629Du, 0xC02EF354u, 0xC21B647Cu, 0x416DE5C5u, 0x3F0983B9u, 0x3F247E29u},
    {0xC0312828u, 0xBEAC8DDBu, 0xBFC93EB8u, 0x3DFBDABBu, 0x3EA0A810u, 0xBFAEB4BFu, 0x3D8D35B8u},
};

// the fish: x, z, heading, speed, turning rate, fright, the frame's milliseconds, the target's x and z;
// then the exe's x, z and heading after the step
struct FishStepCase
{
	uint32_t x;
	uint32_t z;
	uint32_t heading;
	uint32_t speed;
	uint32_t rate;
	uint32_t fright;
	uint32_t ms;
	uint32_t targetX;
	uint32_t targetZ;
	uint32_t outX;
	uint32_t outZ;
	uint32_t outHeading;
};
constexpr FishStepCase k_FishStepCases[] = {
    {0x00000000u, 0x00000000u, 0x3F5F7112u, 0x411FA88Bu, 0x402A2686u, 0x00000000u, 100, 0x419759D6u, 0x41B46DFAu, 0x3F242C0Cu,
     0x3F43B6CAu, 0x3F1B61A9u},
    {0x00000000u, 0x00000000u, 0xC028A884u, 0x4130CA37u, 0x40620C05u, 0x3F9216E7u, 16, 0x41F3F692u, 0x41874858u, 0xBF1E521Au,
     0xBEAF958Du, 0xC0250AA1u},
    {0x00000000u, 0x00000000u, 0xBF827FD0u, 0x41801A71u, 0x3ED9086Eu, 0x401633C0u, 200, 0xC1537094u, 0x41D958F9u, 0x4056B59Au,
     0xC0AE9A04u, 0xBF7A259Au},
    {0xBE16E4F9u, 0xBFD48577u, 0x3FF1EFCAu, 0x4050B815u, 0x409EE5E2u, 0x3FB2B226u, 200, 0xC1F086DFu, 0x410125A0u, 0xBF0E8FE6u,
     0xBED7F344u, 0x4018BF79u},
    {0x00000000u, 0x00000000u, 0x3FDAF722u, 0x40D28CFFu, 0x409BBCD6u, 0x00000000u, 200, 0x4275E3C4u, 0x3FC67296u, 0xBDBBDD78u,
     0x3F26CBAFu, 0x3F9CAB99u},
    {0x00000000u, 0x00000000u, 0xC017DCB4u, 0x4035351Cu, 0x4015ACEBu, 0x00000000u, 16, 0x4284FA56u, 0xC28769E0u, 0xBD055FECu,
     0xBD010160u, 0xC01577A2u},
    {0xBFE28BE3u, 0xBE98F361u, 0xBD0AAA00u, 0x3FC540D1u, 0x409622CEu, 0x3E80A0E7u, 1, 0xC21E5264u, 0x3F7B55D7u, 0xBFE23389u,
     0xBE98FF59u, 0xBD1DE1A8u},
    {0x00000000u, 0x00000000u, 0xC00EDA86u, 0x41821AF2u, 0x3F325051u, 0x40204EDAu, 33, 0x41E1165Cu, 0x42109FFDu, 0xBFA8C0FBu,
     0xBFD8DBAFu, 0xC010531Fu},
    {0x00000000u, 0x00000000u, 0x3EDFF5ABu, 0x4088EE5Fu, 0x3FACF419u, 0x00000000u, 100, 0x3E9D7E1Du, 0x3E134C3Du, 0x3EC6763Eu,
     0x3E399D73u, 0x3E9AC73Au},
    {0xBFA75365u, 0xBE200E67u, 0xBEFDF2C9u, 0x418EB062u, 0x3F965B79u, 0x4011D638u, 50, 0xC1ADED72u, 0x412E68D7u, 0x3FEA41B1u,
     0xBFED4EC0u, 0xBF0E028Au},
    {0x3F189AC2u, 0xBF806125u, 0x3EA1042Cu, 0x40FB0CDCu, 0x3FEB2B02u, 0x00000000u, 200, 0xC08EC1FAu, 0xC02978D1u, 0x3FABCC1Eu,
     0xBF42A23Au, 0x3E05E5F0u},
    {0x00000000u, 0x00000000u, 0xBFC746A0u, 0x41218196u, 0x40932F6Du, 0x00000000u, 33, 0xBF25714Du, 0x42393FAEu, 0x3B984D10u,
     0xBEAA88B1u, 0xBFDAB44Fu},
    {0x3F37FE06u, 0xBFA99AA5u, 0xBF1B467Fu, 0x417B809Du, 0x3F134CFDu, 0x3F8A27BAu, 231, 0x4219DB9Cu, 0x429BCA55u, 0x40A981E1u,
     0xC0900BF3u, 0xBF0C8B99u},
    {0x00000000u, 0x00000000u, 0x3F4A9CBBu, 0x41873BD1u, 0x406E75C1u, 0x3FADC14Eu, 1, 0x4121F335u, 0x4123EC6Au, 0x3D42A654u,
     0x3D45058Au, 0x3F49A88Cu},
    {0x3EA432ECu, 0xBF493882u, 0xBEFF9970u, 0x41011CB2u, 0x40989309u, 0x40387BC6u, 33, 0x42181661u, 0xC1AAC070u, 0x3FA0C0A6u,
     0xBFA5E1FBu, 0xBEAF0A44u},
    {0x3F3FF349u, 0x3F371C3Au, 0x400D033Bu, 0x411AEE7Bu, 0x4080DEAFu, 0x00000000u, 100, 0x409F44F6u, 0xC0A1AA0Au, 0x3E359B1Cu,
     0x3FBF8584u, 0x4026C95Eu},
    {0x3F8C8FD8u, 0x3F1F7465u, 0x3F83973Au, 0x40EE1892u, 0x40839E22u, 0x3F462566u, 16, 0xC1046FC8u, 0xC16EBBC6u, 0x3FA65458u,
     0x3F74E556u, 0x3F76559Bu},
    {0x00000000u, 0x00000000u, 0x3F72CC2Fu, 0x41852420u, 0x40405D2Bu, 0x00000000u, 101, 0xC0E6F22Eu, 0xC120F0AEu, 0x3F785F17u,
     0x3FAD155Fu, 0x3F25DA1Eu},
    {0xBF1CD281u, 0x3E5BD9A5u, 0x3E99ED4Au, 0x417F7D10u, 0x3F80468Au, 0x3F33BC9Bu, 101, 0xC1E51A83u, 0xC1079635u, 0x406A6DA8u,
     0x3FC5276Cu, 0x3E4D3BBFu},
    {0x00000000u, 0x00000000u, 0x40490FDDu, 0x40977D46u, 0x4039780Cu, 0x3FC864C8u, 244, 0xC278ADCAu, 0xC2762616u, 0xBFF2620Au,
     0xB58F691Cu, 0xC03683D8u},
    {0x00000000u, 0x00000000u, 0xC03CDA9Cu, 0x418AAE2Au, 0x401B9EF8u, 0x00000000u, 101, 0x40A6F54Du, 0x3F80F4F3u, 0xBFD9DD2Fu,
     0xBEA846A5u, 0xC04C6A82u},
    {0x3EB8D408u, 0xBE75DA88u, 0x3DB14E58u, 0x412480F7u, 0x408DA167u, 0x3F52849Du, 44, 0xC086A570u, 0xBF22F961u, 0x3FEE9939u,
     0xBDE089C4u, 0xBDDD8682u},
    {0x3F91E3F4u, 0x3FD4542Cu, 0x4029B618u, 0x40287E93u, 0x408B5508u, 0x3FE0F5E1u, 101, 0x41788148u, 0xC16C257Fu, 0x3E5792ACu,
     0x4009DFF4u, 0x404593E6u},
    {0xBF4CFC45u, 0xBF401556u, 0xC035FBFEu, 0x41470169u, 0x400BE420u, 0x3FA52441u, 100, 0xC18D2B6Fu, 0xC0BDA179u, 0xC0B1CE83u,
     0xC00D88FBu, 0xC027FEC8u},
    {0x3FD4DE68u, 0xBF1D555Fu, 0x3FC90FD9u, 0x4086AD46u, 0x3F256D33u, 0x3F31A644u, 101, 0x3FD4DE8Eu, 0x41B41C9Du, 0x3FD4DE6Au,
     0x3F0E0FCFu, 0x3FC0CA63u},
    {0x44600DD6u, 0xC402F9AFu, 0x400791FCu, 0x40DEDE35u, 0x3FB6D7D8u, 0x00000000u, 0, 0x445C536Du, 0xC3F9B863u, 0x44600DD6u,
     0xC402F9AFu, 0x400791FCu},
    {0x3DADA194u, 0xBFC53D55u, 0x3F4227F1u, 0x417D735Fu, 0x409886D2u, 0x00000000u, 0, 0x41E7C59Fu, 0x41AB16CCu, 0x3DADA194u,
     0xBFC53D55u, 0x3F4227F1u},
    {0x00000000u, 0x00000000u, 0x4029C262u, 0x41496301u, 0x408D08F0u, 0x00000000u, 101, 0xC181CB54u, 0xC19D2ED3u, 0xBF8E3854u,
     0x3F1763BAu, 0x4045F75Fu},
    {0xBF73851Bu, 0xBEA37640u, 0x3FAD13A9u, 0x411C8561u, 0x3F9DDB74u, 0x00000000u, 62, 0xC0B0A6E0u, 0xC1A711D8u, 0xBF51D7BCu,
     0x3E8BAEF0u, 0x3FA34A26u},
    {0xBE33E21Du, 0x3F73F618u, 0xBFD3014Eu, 0x41982EA1u, 0x408CCAD1u, 0x3F9EC6F2u, 200, 0x42AF3830u, 0x423C0868u, 0xBF4422CBu,
     0xC0D44312u, 0xBF9AB02Eu},
    {0x445FE7F6u, 0xC416DC3Cu, 0xBFC90FDEu, 0x4125FF97u, 0x405DC476u, 0x00000000u, 16, 0x4473262Eu, 0xC41018AEu, 0x445FE7F6u,
     0xC416E6DCu, 0xBFC1F726u},
    {0x44B42725u, 0x448D9478u, 0x3E3CBB53u, 0x415B5682u, 0x401476A0u, 0x00000000u, 100, 0x44B84336u, 0x448E5899u, 0x44B45245u,
     0x448D9C82u, 0xBD433D1Cu},
};

// the net: centre x y z, size, phase, the frame's milliseconds; then the exe's size and phase after the
// step, the seven points of the ring (x y z each) and the seven wobbles
struct NetRingCase
{
	uint32_t centre[3];
	uint32_t size;
	uint32_t phase;
	uint32_t ms;
	uint32_t outSize;
	uint32_t outPhase;
	uint32_t points[21];
	uint32_t wobbles[7];
};
constexpr NetRingCase k_NetRingCases[] = {
    {{0xBF3D27F8u, 0xBF8A693Bu, 0xBF5805E9u},
     0x4013BF8Cu,
     0x3F970783u,
     1,
     0x40139EC7u,
     0x3F97490Cu,
     {0x41BA9D39u, 0xBF8A693Bu, 0xBF5805E9u, 0x416440BAu, 0xBF8A693Bu, 0x418FC588u, 0xC0C30216u,
      0xBF8A693Bu, 0x41B4F293u, 0xC1B35ED5u, 0xBF8A693Bu, 0x411990D0u, 0xC1B35ED4u, 0xBF8A693Bu,
      0xC1349192u, 0xC0C30219u, 0xBF8A693Bu, 0xC1C272F1u, 0x416440C0u, 0xBF8A693Bu, 0xC19D45E4u},
     {0x3E422033u, 0xBE92E39Au, 0x3E67A984u, 0xBEBA0FD2u, 0xBD4605FCu, 0x3E7F1F4Du, 0x3EDE8477u}},
    {{0x3ECF6EA6u, 0xBD9C0074u, 0xBF8A2064u},
     0x401DCBF2u,
     0x45363893u,
     109,
     0x400FD83Cu,
     0x45363C10u,
     {0x41BF0C06u, 0xBD9C0074u, 0xBF8A2064u, 0x4170AC1Bu, 0xBD9C0074u, 0x418A331Au, 0xC09A32CCu,
      0xBD9C0074u, 0x41AE76D9u, 0xC1A5F751u, 0xBD9C0074u, 0x4111B4B8u, 0xC1A5F750u, 0xBD9C0074u,
      0xC1343CD4u, 0xC09A32CFu, 0xBD9C0074u, 0xC1BFBAE4u, 0x4170AC21u, 0xBD9C0074u, 0xC19B7723u},
     {0x3EEFF488u, 0x3DDA571Fu, 0xBE32AB65u, 0xBEFF64B6u, 0xBECC1CEEu, 0x3EF9A693u, 0x3DE71049u}},
    {{0xBEF08A9Eu, 0x3DA3A346u, 0x3F304762u},
     0x40091EEEu,
     0x45274F54u,
     100,
     0x3FF8A442u,
     0x45275287u,
     {0x419FA47Fu, 0x3DA3A346u, 0x3F304762u, 0x41443DA6u, 0x3DA3A346u, 0x418542C2u, 0xC0A07975u,
      0x3DA3A346u, 0x41A4D01Cu, 0xC196FA4Au, 0x3DA3A346u, 0x4118CFBDu, 0xC196FA49u, 0x3DA3A346u,
      0xC102C6D4u, 0xC0A07978u, 0x3DA3A346u, 0xC199CBA6u, 0x41443DABu, 0x3DA3A346u, 0xC1747C94u},
     {0x3EDE0837u, 0x3CCBCEACu, 0xBDC2C513u, 0xBEFED0BBu, 0xBEAFF1C1u, 0x3EECF1AFu, 0x3E43EF83u}},
    {{0x00000000u, 0x00000000u, 0x00000000u},
     0x3F85D3A3u,
     0x44976EB4u,
     215,
     0x3F1D92CBu,
     0x44977C77u,
     {0x40E4F77Eu, 0x00000000u, 0x00000000u, 0x408EC229u, 0x00000000u, 0x40B30370u, 0xBFCBCCB3u,
      0x00000000u, 0x40DF39E1u, 0xC0CE4ABDu, 0x00000000u, 0x4046B09Au, 0xC0CE4ABCu, 0x00000000u,
      0xC046B09Fu, 0xBFCBCCB7u, 0x00000000u, 0xC0DF39E1u, 0x408EC22Du, 0x00000000u, 0xC0B3036Du},
     {0x3EB8A1B7u, 0x3EF8FA54u, 0xBEFEE3C3u, 0xBE3E4845u, 0xBEE3DE82u, 0x3E9F897Eu, 0xBEC7806Eu}},
    {{0xBD4DA909u, 0xBFF17432u, 0x3F9E479Eu},
     0x3F51A298u,
     0x452EFDB0u,
     0,
     0x3F51A298u,
     0x452EFDB0u,
     {0x411237F6u, 0xBFF17432u, 0x3F9E479Eu, 0x40B5BA06u, 0xBFF17432u, 0x4106BB3Bu, 0xC0061347u,
      0xBFF17432u, 0x41231EECu, 0xC10543FDu, 0xBFF17432u, 0x40A72699u, 0xC10543FCu, 0xBFF17432u,
      0xC030059Au, 0xC0061349u, 0xBFF17432u, 0xC0F71A06u, 0x40B5BA0Bu, 0xBFF17432u, 0xC0BE52A2u},
     {0xBEC463C2u, 0x3D804B7Du, 0x3C02DBB2u, 0x3EF69D00u, 0x3E8CCBC2u, 0xBED86578u, 0xBE89BC5Eu}},
    {{0x00000000u, 0x00000000u, 0x00000000u},
     0x3C249AEDu,
     0x43DC5C6Du,
     0,
     0x3C249AEDu,
     0x43DC5C6Du,
     {0x3F8CDC1Au, 0x00000000u, 0x00000000u, 0x3F2FA636u, 0x00000000u, 0x3F5C41D8u, 0xBE7AC106u,
      0x00000000u, 0x3F895400u, 0xBF7DD210u, 0x00000000u, 0x3EF4778Cu, 0xBF7DD20Fu, 0x00000000u,
      0xBEF47792u, 0xBE7AC10Bu, 0x00000000u, 0xBF895400u, 0x3F2FA63Au, 0x00000000u, 0xBF5C41D5u},
     {0x3E9F4FC4u, 0xBE25172Cu, 0x3DBE168Eu, 0xBEE3BCD9u, 0xBE3DBF41u, 0x3EB86E94u, 0x3EB25B0Cu}},
    {{0x00000000u, 0x00000000u, 0x00000000u},
     0x3C073B16u,
     0x45066680u,
     0,
     0x3C073B16u,
     0x45066680u,
     {0x3F8A909Eu, 0x00000000u, 0x00000000u, 0x3F2CC9A1u, 0x00000000u, 0x3F58AB38u, 0xBE76AB34u,
      0x00000000u, 0x3F87173Fu, 0xBF79AF74u, 0x00000000u, 0x3EF07BF3u, 0xBF79AF72u, 0x00000000u,
      0xBEF07BF9u, 0xBE76AB39u, 0x00000000u, 0xBF87173Eu, 0x3F2CC9A5u, 0x00000000u, 0xBF58AB35u},
     {0x3BE414A0u, 0xBED5785Eu, 0x3EBF64ABu, 0xBE597A73u, 0x3E0C9105u, 0x3D95A50Au, 0x3EFD6A2Du}},
    {{0x00000000u, 0x00000000u, 0x00000000u},
     0x3D4CC6E0u,
     0x410E1674u,
     61,
     0x00000000u,
     0x41100A2Au,
     {0x3F800000u, 0x00000000u, 0x00000000u, 0x3F1F9D07u, 0x00000000u, 0x3F48261Cu, 0xBE63DC88u,
      0x00000000u, 0x3F7994E0u, 0xBF66A5E6u, 0x00000000u, 0x3EDE25FEu, 0xBF66A5E5u, 0x00000000u,
      0xBEDE2603u, 0xBE63DC8Cu, 0x00000000u, 0xBF7994E0u, 0x3F1F9D0Bu, 0x00000000u, 0xBF482619u},
     {0xBEE982ABu, 0xBED674C0u, 0x3EE809EBu, 0x3EA98483u, 0x3EFDD4CBu, 0xBED991B5u, 0x3E85F120u}},
    {{0x3FF5CDEDu, 0xBFF450DFu, 0x3EEC54F5u},
     0x3F6FEB45u,
     0x3FCCB661u,
     16,
     0x3F67BA1Eu,
     0x3FD0CEF4u,
     {0x413F8E11u, 0xBFF450DFu, 0x3EEC54F5u, 0x41030044u, 0xBFF450DFu, 0x4105207Au, 0xBEA1FEDCu,
      0xBFF450DFu, 0x41242EB4u, 0xC0E45A7Du, 0xBFF450DFu, 0x409A554Du, 0xC0E45A7Bu, 0xBFF450DFu,
      0xC0799565u, 0xBEA1FEECu, 0xBFF450DFu, 0xC1156964u, 0x41030046u, 0xBFF450DFu, 0xC0ECB651u},
     {0xBCF7BC71u, 0xBEDF6352u, 0x3ECB81E8u, 0xBE366753u, 0x3E30CAD1u, 0x3D13C863u, 0x3EFF6974u}},
    {{0x00000000u, 0x00000000u, 0x00000000u},
     0x3CAACAD1u,
     0x4102ABDFu,
     100,
     0x00000000u,
     0x4105DF12u,
     {0x3F800000u, 0x00000000u, 0x00000000u, 0x3F1F9D07u, 0x00000000u, 0x3F48261Cu, 0xBE63DC88u,
      0x00000000u, 0x3F7994E0u, 0xBF66A5E6u, 0x00000000u, 0x3EDE25FEu, 0xBF66A5E5u, 0x00000000u,
      0xBEDE2603u, 0xBE63DC8Cu, 0x00000000u, 0xBF7994E0u, 0x3F1F9D0Bu, 0x00000000u, 0xBF482619u},
     {0xBE7B46D9u, 0xBEFF927Eu, 0x3EFAED50u, 0x3D3466C2u, 0x3EB88911u, 0xBE3E05FCu, 0x3EED4AFEu}},
};

/// The shoal: the random draw, its centre, the scare point, a fish and its heading; then the exe's new target and
/// the fish's heading after the update (turned away from the point when it is within 8 metres)
struct ShoalScareCase
{
	uint32_t draw;
	uint32_t centre[3];
	uint32_t point[3];
	uint32_t fish[3];
	uint32_t heading;
	uint32_t outTarget[3];
	uint32_t outHeading;
};
constexpr ShoalScareCase k_ShoalScareCases[] = {
    {17329,
     {0xC1D33854u, 0xC0B02AC0u, 0xC2477CD7u},
     {0xC1F83E7Au, 0xC0B83E82u, 0xC243A417u},
     {0xC1F83E7Au, 0xC0B83E82u, 0xC243A417u},
     0xC03C89D3u,
     {0xC1E2F531u, 0xC0B02AC0u, 0xC248EE1Eu},
     0x00000000u},
    {5259,
     {0x3F58D610u, 0xBFDD2F09u, 0x3FD60A4Bu},
     {0x405AECBAu, 0xC0CE3381u, 0x3F1695F1u},
     {0x405AECBAu, 0xC0CE3381u, 0x3F1695F1u},
     0x3F5FAE9Bu,
     {0x3FF4EA11u, 0xBFDD2F09u, 0x40574EC3u},
     0x00000000u},
    {30680,
     {0xC2369945u, 0x40A071DCu, 0xC1363410u},
     {0xC226763Au, 0x3FEE415Du, 0xC10EF797u},
     {0xC224C686u, 0x3FA246DDu, 0xC10817A9u},
     0xBFB8902Cu,
     {0xC22F3B16u, 0x40A071DCu, 0xC142AB9Bu},
     0x3F4B7E4Cu},
    {24600,
     {0xC4A15EE0u, 0xC4560F61u, 0xC4AD7F7Au},
     {0xC4A18B0Bu, 0xC455B0F3u, 0xC4AD381Du},
     {0xC4A0DDC3u, 0xC4558223u, 0xC4AD6129u},
     0xBF0B35B0u,
     {0xC4A15E92u, 0xC4560F61u, 0xC4ADBF7Au},
     0xBE6E2CD5u},
    {29673,
     {0x41305022u, 0xC0487B04u, 0x40B69803u},
     {0x4164598Eu, 0xC0B0552Fu, 0x4094639Eu},
     {0x41699841u, 0xC1154DBEu, 0xBFBF5CF0u},
     0x3FE432CCu,
     {0x414AD831u, 0xC0487B04u, 0x4092CFECu},
     0xBFC239D8u},
    {8663,
     {0x422AE5F0u, 0x4240F2DEu, 0x41F12B9Bu},
     {0x423D5C33u, 0x424E0C29u, 0x4202222Du},
     {0x423D5C33u, 0x424E0C29u, 0x4202222Du},
     0x3FCFC99Eu,
     {0x422A2D20u, 0x4240F2DEu, 0x42008D72u},
     0x00000000u},
    {32092,
     {0x3F246CF6u, 0x3F0BF95Fu, 0xBF16CD5Bu},
     {0x3CA3F118u, 0xC01F0300u, 0x4019B4D7u},
     {0x40A72EACu, 0xBEAAA9E5u, 0xC043A2D8u},
     0x4038BC5Fu,
     {0x40280924u, 0x3F0BF95Fu, 0xBF58E320u},
     0xBF4F28F1u},
    {4482,
     {0xBFC65835u, 0x3FF554A3u, 0x3FD102B3u},
     {0xC0A214B4u, 0xBF0D6B93u, 0x3FF67F8Au},
     {0xC04F1A98u, 0x402AE5B2u, 0x4070750Bu},
     0xC005DE54u,
     {0xBE79B1A8u, 0x3FF554A3u, 0x40497656u},
     0x3F4939E2u},
    {1864,
     {0xC22E8343u, 0xC228E922u, 0x421160A5u},
     {0xC21A8F74u, 0xC21F6847u, 0x42041247u},
     {0xC21B47F9u, 0xC22328A9u, 0x4204C9DAu},
     0x401F2377u,
     {0xC22704B2u, 0xC228E922u, 0x42142D2Cu},
     0x4016F5F7u},
    {12941,
     {0x446D55AAu, 0x44892703u, 0x444C39CCu},
     {0x446C6561u, 0x44890CE5u, 0x444C8BA8u},
     {0x446C894Bu, 0x4488F7CBu, 0x444C7BD5u},
     0x403B3947u,
     {0x446CF08Eu, 0x44892703u, 0x444C8849u},
     0xBED47D98u},
    {17903,
     {0xBF8FAB86u, 0xC1E0A7EAu, 0x41501D17u},
     {0xC0148006u, 0xC1DB2FBFu, 0x4158AB51u},
     {0xBFE2540Bu, 0xC1D493F8u, 0x41649D6Bu},
     0x3FEB2080u,
     {0xC04270A2u, 0xC1E0A7EAu, 0x4146EBD2u},
     0x3F6F1D5Eu},
    {5742,
     {0x449761CBu, 0x44CE10EAu, 0x4452DD2Bu},
     {0x44973C27u, 0x44CE80F0u, 0x4453CD90u},
     {0x44974087u, 0x44CE706Bu, 0x4453A0BCu},
     0x403B47FDu,
     {0x44977EC3u, 0x44CE10EAu, 0x44534F4Eu},
     0xBFB0634Au},
    {22642,
     {0xC48C9E66u, 0x43D70B04u, 0xC4C0E1CBu},
     {0xC48C484Fu, 0x43D5500Au, 0xC4C0AF36u},
     {0xC48C4070u, 0x43D5B7BFu, 0xC4C08FF8u},
     0x3F3B70BFu,
     {0xC48CB596u, 0x43D70B04u, 0xC4C11D72u},
     0x3FA978A7u},
    {26885,
     {0xC18BB247u, 0x41BDA08Eu, 0x4209CA94u},
     {0xC16806E5u, 0x4197EEBDu, 0x420BB4A5u},
     {0xC1951253u, 0x41C7BCA6u, 0x4208D92Eu},
     0x3F603F7Eu,
     {0xC184D6E1u, 0x41BDA08Eu, 0x42029030u},
     0xC03E1B5Cu},
    {18811,
     {0xC247CEA5u, 0x421E91F7u, 0x40CC8C49u},
     {0xC2460DB3u, 0x4212BBD7u, 0x40831326u},
     {0xC246E0E9u, 0x420D1860u, 0x40F50736u},
     0xBFCE04A6u,
     {0xC24EF4C0u, 0x421E91F7u, 0x40AFD246u},
     0x3FD077B5u},
    {27839,
     {0x3D01EEFAu, 0xBF6FC1A1u, 0xBF149849u},
     {0x402E6AB2u, 0xC04DC105u, 0xC093260Eu},
     {0x40622E64u, 0xC083CB1Eu, 0xC0839836u},
     0xBEAB8D9Eu,
     {0x3F9A04CBu, 0xBF6FC1A1u, 0xC00CE3AFu},
     0x3F0A86FDu},
    {8449,
     {0xC1C9BD1Au, 0xC1EDF4C5u, 0xC19C422Au},
     {0xC1B41811u, 0xC1EC44DBu, 0xC191112Bu},
     {0xC1D40B8Au, 0xC1C7C11Fu, 0xC157F614u},
     0x3ED02478u,
     {0xC1CA8712u, 0xC1EDF4C5u, 0xC18C4726u},
     0x40120B67u},
    {20335,
     {0x4208F9F7u, 0x421EA3C2u, 0xC167A4FAu},
     {0x42055AD5u, 0x420CB52Fu, 0xC168D3B3u},
     {0x4207C3C3u, 0x42095194u, 0xC1738EAAu},
     0xC0022876u,
     {0x42032A47u, 0x421EA3C2u, 0xC17DA308u},
     0xBF56C1F5u},
    {7173,
     {0x41CE0CA2u, 0xC1100127u, 0xC1763BCDu},
     {0x41CDE5C2u, 0xC11B868Cu, 0xC14E7C32u},
     {0x41D5D66Fu, 0xC125C969u, 0xC1451C89u},
     0xBFA29834u,
     {0x41D127B4u, 0xC1100127u, 0xC156D79Du},
     0x3F08821Cu},
    {27463,
     {0xC1B3FAF7u, 0xC064B0A7u, 0xC1C71496u},
     {0xC1AD6B7Bu, 0xC06F3868u, 0xC1ACB901u},
     {0xC1C4F19Fu, 0x403B8C4Fu, 0xC1ABCDD6u},
     0x3FC5F3ACu,
     {0xC1AB9102u, 0xC064B0A7u, 0xC1D4B081u},
     0x40469060u},
    {2226,
     {0xBE8A00CFu, 0x3FD3D025u, 0x3F327F0Cu},
     {0x4044D074u, 0xBFF492D3u, 0xBFE5C8A4u},
     {0x406CFBC3u, 0xBFCE93F2u, 0xBFCA8294u},
     0x3F36AF51u,
     {0x3FC687B9u, 0x3FD3D025u, 0x3FC33B62u},
     0x3EA790DAu},
    {8317,
     {0xBF4A2A9Eu, 0xBFE7A730u, 0xBEDD6602u},
     {0x3E7DA20Cu, 0xC0595416u, 0xBEA35F8Au},
     {0x3F070CECu, 0xC08245B4u, 0xBF864D7Fu},
     0xBFD52101u,
     {0xBF56764Au, 0xBFE7A730u, 0x3FC89398u},
     0xBF9A3671u},
    {20510,
     {0xC22CC98Cu, 0xC1B79E2Au, 0xC23E11D1u},
     {0xC227D54Eu, 0xC1CA9F90u, 0xC2302283u},
     {0xC21B8611u, 0xC1BBA61Fu, 0xC231FC91u},
     0x3FEE2795u,
     {0xC232692Cu, 0xC1B79E2Au, 0xC243C273u},
     0xBE18E5F9u},
    {29637,
     {0xC1DA644Cu, 0x419199B1u, 0x4239C4B7u},
     {0xC1D241FFu, 0x418161F4u, 0x422F92ABu},
     {0xC1D03441u, 0x41831204u, 0x422E412Fu},
     0xBF6CF455u,
     {0xC1CD3028u, 0x419199B1u, 0x42354002u},
     0xBF68B73Du},
    {2943,
     {0xC4E7D251u, 0x41BA28F1u, 0x441A845Bu},
     {0xC4E7A2AEu, 0x41B9F592u, 0x441A5FF1u},
     {0xC4E8778Cu, 0x418B81EFu, 0x441B859Cu},
     0xBF4C78B6u,
     {0xC4E79C3Du, 0x41BA28F1u, 0x441AC8D1u},
     0xBF4C78B6u},
    {2398,
     {0x445AC8BEu, 0x43019B86u, 0x44757A30u},
     {0x4459CB49u, 0x4302F58Bu, 0x4475A9A4u},
     {0x445B2222u, 0x430436B1u, 0x44784108u},
     0xBEA63F02u,
     {0x445B3B72u, 0x43019B86u, 0x4475B2FEu},
     0xBEA63F02u},
    {11809,
     {0x41A459F6u, 0x417684CCu, 0x41E4E770u},
     {0x41C94984u, 0x4181A3B1u, 0x4205E764u},
     {0x41D73AB1u, 0x41CFBB0Au, 0x41F02C42u},
     0x3FD8F328u,
     {0x419A1F49u, 0x417684CCu, 0x41F13500u},
     0x3FD8F328u},
    {8726,
     {0x4428537Eu, 0xC4139EDDu, 0x449C3990u},
     {0x4428F14Du, 0xC413F4A3u, 0x449C37E2u},
     {0x4428CDD7u, 0xC41692C8u, 0x449C2A8Cu},
     0x3F6AA768u,
     {0x44284667u, 0xC4139EDDu, 0x449C793Au},
     0x3F6AA768u},
    {10017,
     {0xC45B8B4Fu, 0x4312769Cu, 0xC4C8529Bu},
     {0xC45B2D11u, 0x4311ACA4u, 0xC4C7BE42u},
     {0xC45D96B0u, 0x4316FC2Bu, 0xC4C88DA4u},
     0xBF4A7FD3u,
     {0xC45BB733u, 0x4312769Cu, 0xC4C8167Cu},
     0xBF4A7FD3u},
    {4694,
     {0xC2017A26u, 0x41BF5D5Eu, 0xBF8B5D06u},
     {0xC207F875u, 0x41BB76BFu, 0xC0735741u},
     {0xC22A5EB9u, 0x4181BA39u, 0x3F799DF1u},
     0xBF85752Du,
     {0xC1F90279u, 0x41BF5D5Eu, 0x3EF4BB24u},
     0xBF85752Du},
    {27574,
     {0xC23EAB4Du, 0x41890925u, 0x4229537Eu},
     {0xC24F4E7Au, 0x418004EFu, 0x421BD6B3u},
     {0xC2688E94u, 0x4129EF10u, 0x422A89C7u},
     0xBFD9B507u,
     {0xC23A517Eu, 0x41890925u, 0x42229CD9u},
     0xBFD9B507u},
    {20399,
     {0x44433AD0u, 0x44025F2Cu, 0x444BBFB9u},
     {0x4443EC82u, 0x44025646u, 0x444ADC25u},
     {0x44432B88u, 0x44007B0Bu, 0x44483B7Au},
     0xC032755Bu,
     {0x4442DEEBu, 0x44025F2Cu, 0x444B669Eu},
     0xC032755Bu},
};

/// The fish farm's swim step as the original orders it: the frame's time, the fright's push, the move along the
/// heading and the turn towards the target (the animation frame is left out)
struct FishAfterStep
{
	float x;
	float z;
	float heading;
};

[[nodiscard]] FishAfterStep FishStep(const FishStepCase& c)
{
	float dt = x87::Store(x87::Mul(static_cast<double>(c.ms), 0.001f));
	double push = 1.0;
	float fright = F(c.fright);
	if (fright != 0.0f)
	{
		fright = std::max(x87::Store(x87::Sub(fright, dt)), 0.0f);
		push = fright < 1.0f ? x87::Add(x87::Mul(x87::Sub(1.0, x87::Sub(1.0, fright)), 3.0), 1.0) : 4.0;
	}
	if (!(dt < 0.1f))
	{
		dt = 0.1f;
	}
	const float heading = F(c.heading);
	const auto [sin, cos] = x87::SinCos(heading);
	const double speed = F(c.speed);
	const float dx = x87::Store(x87::Mul(x87::Mul(x87::Mul(cos, speed), dt), push));
	const double dz = x87::Mul(x87::Mul(x87::Mul(sin, speed), dt), push);
	const float x = x87::Store(x87::Add(dx, F(c.x)));
	const float z = x87::Store(x87::Add(dz, F(c.z)));
	// Which side of the heading the target is
	const double side = x87::Sub(x87::Mul(sin, x87::Sub(F(c.targetX), x)), x87::Mul(x87::Sub(F(c.targetZ), z), cos));
	const double turn = side < 0.0 ? 1.0 : -1.0;
	float turned = x87::Store(x87::Add(x87::Mul(x87::Mul(turn, F(c.rate)), dt), heading));
	const auto turns = static_cast<int32_t>(x87::Mul(turned, 0.15915494f));
	turned = x87::Store(x87::Sub(turned, x87::Mul(static_cast<double>(turns), 6.2831855f)));
	if (turned > 3.14159274f)
	{
		turned = x87::Store(x87::Sub(turned, 6.2831855f));
	}
	return {x, z, turned};
}

/// The fish puzzle's net as the original orders it: it shrinks, its seven points stand round the centre on a ring of
/// its size, and each wobbles with the cosine of its own phase
struct NetAfterStep
{
	float size;
	float phase;
	float points[21];
	float wobbles[7];
};

[[nodiscard]] NetAfterStep NetStep(const NetRingCase& c)
{
	NetAfterStep after {};
	const double dt = x87::Mul(static_cast<double>(c.ms), 0.001f);
	after.size = std::max(x87::Store(x87::Sub(F(c.size), x87::Add(dt, dt))), 0.0f);
	const double radius = x87::Add(x87::Mul(after.size, 10.0f), 1.0f);
	for (int i = 0; i < 7; ++i)
	{
		const auto [sin, cos] = x87::SinCos(x87::Mul(static_cast<double>(i), 0.8975979f));
		after.points[3 * i] = x87::Store(x87::Add(F(c.centre[0]), x87::Store(x87::Mul(cos, radius))));
		after.points[3 * i + 1] = x87::Store(x87::Add(F(c.centre[1]), 0.0));
		after.points[3 * i + 2] = x87::Store(x87::Add(F(c.centre[2]), x87::Store(x87::Mul(sin, radius))));
	}
	after.phase = x87::Store(x87::Add(x87::Add(dt, dt), F(c.phase)));
	for (int i = 0; i < 7; ++i)
	{
		after.wobbles[i] = x87::Store(x87::Mul(x87::Cos(x87::Add(static_cast<double>(i * i), after.phase)), 0.5f));
	}
	return after;
}

/// The fish farm shoal's scare as the original orders it: a new target two metres from the centre at a random angle
/// (the cosine stored to a float first, the sine unrounded), and a fish within 8 metres of the scare point turned to
/// face away from it (the arctangent, unrounded, stored to a float)
struct ShoalAfterScare
{
	float target[3];
	float heading;
};

[[nodiscard]] ShoalAfterScare ShoalScare(const ShoalScareCase& c)
{
	ShoalAfterScare after {};
	// The game's random float between 0 and 2 pi from the C library's draw
	const double angle =
	    x87::Add(x87::Mul(x87::Mul(static_cast<double>(c.draw), F(0x38000100u)), x87::Sub(6.2831855f, 0.0)), 0.0);
	const auto [sin, cos] = x87::SinCos(angle);
	const float cosine = x87::Store(cos);
	const x87::Extended minusSin {-sin.high, -sin.low};
	const float x = x87::Store(x87::Add(x87::Add(x87::Mul(cosine, 2.0), x87::Mul(minusSin, 0.0)), 0.0));
	const float z = x87::Store(x87::Add(x87::Add(x87::Mul(cosine, 0.0), x87::Mul(sin, 2.0)), 0.0));
	after.target[0] = x87::Store(x87::Add(F(c.centre[0]), x));
	after.target[1] = x87::Store(x87::Add(F(c.centre[1]), 0.0));
	after.target[2] = x87::Store(x87::Add(F(c.centre[2]), z));
	const double dx = x87::Sub(F(c.point[0]), F(c.fish[0]));
	const double dy = x87::Sub(F(c.point[1]), F(c.fish[1]));
	const double dz = x87::Sub(F(c.point[2]), F(c.fish[2]));
	const double distance = x87::Add(x87::Add(x87::Mul(dz, dz), x87::Mul(dy, dy)), x87::Mul(dx, dx));
	after.heading = F(c.heading);
	if (distance < 64.0)
	{
		// The FPU's arctangent of two floats, stored to a float: atan2 in double gives its bits
		after.heading = x87::Store(std::atan2(x87::Sub(F(c.fish[2]), F(c.point[2])), x87::Sub(F(c.fish[0]), F(c.point[0]))));
	}
	return after;
}

} // namespace

// Each operation rounds to 24 bits, to the nearest and ties to even, and keeps the wide exponent
static_assert(x87::Round(1.0 + 0x1p-24) == 1.0);
static_assert(x87::Round(1.0 + 3 * 0x1p-24) == 1.0 + 0x1p-22);
static_assert(x87::Round(1.0 + 0x1p-24 + 0x1p-40) == 1.0 + 0x1p-23);
static_assert(x87::Mul(0x1p100, 0x1p100) == 0x1p200);
static_assert(x87::Mul(0x1p-100, 0x1p-100) == 0x1p-200);
// An unrounded value just above or below a halfway point goes to its own side
static_assert(x87::Round(x87::Extended {1.0 + 0x1p-24, 0x1p-80}) == 1.0 + 0x1p-23);
static_assert(x87::Round(x87::Extended {1.0 + 0x1p-24, -0x1p-80}) == 1.0);
// Worked out while compiling: the FPU's sin(1) and cos(1) stored to floats
static_assert(std::bit_cast<uint32_t>(x87::Store(x87::Sin(1.0f))) == 0x3F576AA4u);
static_assert(std::bit_cast<uint32_t>(x87::Store(x87::Cos(1.0f))) == 0x3F0A5140u);

TEST(GUtilsX87, EachOperationRoundsToAFloatsMantissaButNotItsRange)
{
	EXPECT_EQ(x87::Round(1.0 + 0x1p-24), 1.0);
	EXPECT_EQ(x87::Add(1.0f, 0x1p-24f), 1.0);
	EXPECT_EQ(x87::Add(1.0f, 0x1.8p-24f), 1.0 + 0x1p-23);
	EXPECT_EQ(x87::Div(1.0f, 3.0f), static_cast<double>(1.0f / 3.0f));
	EXPECT_EQ(x87::Sqrt(2.0f), static_cast<double>(std::sqrt(2.0f)));
	// A product past the float's range stays finite on the stack and only the store makes it infinite
	volatile float largest = 3.0e38f; // read at run time, so the compiler does not fold the overflow
	const double big = x87::Mul(largest, largest);
	EXPECT_TRUE(std::isfinite(big));
	EXPECT_EQ(x87::Div(big, 3.0e38f), static_cast<double>(3.0e38f));
	EXPECT_EQ(x87::Store(big), std::numeric_limits<float>::infinity());
	// and one below its smallest stays exact
	EXPECT_EQ(x87::Mul(x87::Mul(1.0e-30f, 1.0e-30f), 1.0e30f), x87::Mul(1.0e-30f, 1.0f));
	EXPECT_TRUE(std::isnan(x87::Round(std::numeric_limits<double>::quiet_NaN())));
	EXPECT_EQ(x87::Round(-std::numeric_limits<double>::infinity()), -std::numeric_limits<double>::infinity());
}

TEST(GUtilsX87, TheSineAndCosineAreTheFPUsUnroundedValues)
{
	for (const auto& c : k_SinCosCases)
	{
		const auto [sin, cos] = x87::SinCos(F(c.angle));
		EXPECT_EQ(Bits(sin.high), c.sinHigh) << std::hex << c.angle;
		EXPECT_EQ(Bits(sin.low), c.sinLow) << std::hex << c.angle;
		EXPECT_EQ(Bits(cos.high), c.cosHigh) << std::hex << c.angle;
		EXPECT_EQ(Bits(cos.low), c.cosLow) << std::hex << c.angle;
	}
}

TEST(GUtilsX87, AnAngleThatIsNoNumberHasNoSine)
{
	for (const double angle : {std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
	                           std::numeric_limits<double>::quiet_NaN()})
	{
		const auto [sin, cos] = x87::SinCos(angle);
		EXPECT_TRUE(std::isnan(sin.high));
		EXPECT_TRUE(std::isnan(cos.high));
	}
}

TEST(GUtilsX87, WhereTheFPUsLastBitDiffersItsFloatResultsDoNot)
{
	for (const auto& c : k_LastBitCases)
	{
		const auto [sin, cos] = x87::SinCos(F(c.angle));
		EXPECT_EQ(Bits(x87::Store(x87::Mul(sin, F(c.times)))), c.sinTimes) << std::hex << c.angle;
		EXPECT_EQ(Bits(x87::Store(x87::Mul(cos, F(c.times)))), c.cosTimes) << std::hex << c.angle;
	}
}

TEST(GUtilsX87, AnOperationOnASineRoundsItsExactResultOnce)
{
	for (const auto& c : k_OperationCases)
	{
		const auto [sin, cos] = x87::SinCos(F(c.a));
		EXPECT_EQ(Bits(x87::Store(x87::Mul(sin, F(c.m)))), c.sinTimes) << std::hex << c.a << " " << c.m;
		EXPECT_EQ(Bits(x87::Store(x87::Mul(F(c.m), cos))), c.cosTimes) << std::hex << c.a << " " << c.m;
		EXPECT_EQ(Bits(x87::Store(x87::Add(sin, x87::Sin(F(c.b))))), c.sinPlusSin) << std::hex << c.a << " " << c.b;
		EXPECT_EQ(Bits(x87::Store(x87::Add(cos, 1.0f))), c.cosPlusOne) << std::hex << c.a;
	}
}

TEST(GUtilsX87Oracle, TheFishSwimStepGivesTheOriginalsBits)
{
	for (const auto& c : k_FishStepCases)
	{
		const auto after = FishStep(c);
		EXPECT_EQ(Bits(after.x), c.outX) << std::hex << c.heading;
		EXPECT_EQ(Bits(after.z), c.outZ) << std::hex << c.heading;
		EXPECT_EQ(Bits(after.heading), c.outHeading) << std::hex << c.heading;
	}
}

TEST(GUtilsX87Oracle, TheFishPuzzlesNetGivesTheOriginalsBits)
{
	for (const auto& c : k_NetRingCases)
	{
		const auto after = NetStep(c);
		EXPECT_EQ(Bits(after.size), c.outSize);
		EXPECT_EQ(Bits(after.phase), c.outPhase);
		for (int i = 0; i < 21; ++i)
		{
			EXPECT_EQ(Bits(after.points[i]), c.points[i]) << "point value " << i;
		}
		for (int i = 0; i < 7; ++i)
		{
			EXPECT_EQ(Bits(after.wobbles[i]), c.wobbles[i]) << "wobble " << i;
		}
	}
}

TEST(GUtilsX87Oracle, TheFishShoalsScareGivesTheOriginalsBits)
{
	for (const auto& c : k_ShoalScareCases)
	{
		const auto after = ShoalScare(c);
		for (int i = 0; i < 3; ++i)
		{
			EXPECT_EQ(Bits(after.target[i]), c.outTarget[i]) << c.draw << " target " << i;
		}
		EXPECT_EQ(Bits(after.heading), c.outHeading) << c.draw;
	}
}

namespace
{

[[nodiscard]] double D(uint64_t bits)
{
	return std::bit_cast<double>(bits);
}

[[nodiscard]] bool SameBits(x87::Extended value, uint64_t high, uint64_t low)
{
	return Bits(value.high) == high && value.low == D(low);
}

/// value and the 64-bit value (high, low) one unit of the 64th bit apart
[[nodiscard]] bool OneUnitApart(x87::Extended value, uint64_t high, uint64_t low)
{
	const auto difference = x87::detail::Add({value.high, value.low}, {-D(high), -D(low)});
	const double unit = std::ldexp(1.0, std::ilogb(std::max(std::abs(value.high), std::abs(D(high)))) - 63);
	return difference.low == 0.0 && (std::abs(difference.high) == unit || std::abs(difference.high) == unit / 2.0);
}

/// The heading of a thrown fireball from its direction, in the original's order: the yaw's arctangent kept unrounded
/// into its cosine and sine, the pitch's arctangent plus 0.3 held within a quarter turn, its cosine stored
[[nodiscard]] std::array<float, 3> FireballHeading(float x, float y, float z)
{
	const auto yaw = x87::SinCos(x87::Atan2(z, x));
	double pitch = x87::Add(x87::Atan2(y, x87::Sqrt(x87::Add(x87::Mul(x, x), x87::Mul(z, z)))), 0.3f);
	if (!(pitch > -1.5707964f))
	{
		pitch = -1.5707964f;
	}
	else if (!(pitch < 1.5707964f))
	{
		pitch = 1.5707964f;
	}
	const auto [sinPitch, cosPitch] = x87::SinCos(pitch);
	const float cosPitchStored = x87::Store(cosPitch);
	return {x87::Store(x87::Mul(yaw.cos, cosPitchStored)), x87::Store(sinPitch), x87::Store(x87::Mul(yaw.sin, cosPitchStored))};
}

} // namespace

TEST(GUtilsX87Oracle, TheArctangentIsTheFPUs)
{
	for (const auto& c : golden::k_Atan2Cases)
	{
		const auto angle = x87::Atan2(F(c.y), F(c.x));
		EXPECT_TRUE(SameBits(angle, c.high, c.low)) << std::hex << c.y << " " << c.x;
		// the original's helper adds 2 pi, a float, to a negative angle: one rounding of the exact sum
		const double positive = angle.high < 0.0 ? x87::Add(angle, 6.2831855f) : angle.high;
		EXPECT_EQ(Bits(positive), c.positive) << std::hex << c.y << " " << c.x;
	}
}

TEST(GUtilsX87Oracle, WhereTheFPUsArctangentRoundsItsLastBitTheOtherWayItsFloatResultsDoNot)
{
	for (const auto& c : golden::k_Atan2LastBitCases)
	{
		const auto angle = x87::Atan2(F(c.y), F(c.x));
		const x87::Extended fpu {D(c.high), D(c.low)};
		EXPECT_TRUE(OneUnitApart(angle, c.high, c.low)) << std::hex << c.y << " " << c.x;
		EXPECT_EQ(Bits(x87::Store(angle)), Bits(x87::Store(fpu))) << std::hex << c.y << " " << c.x;
		EXPECT_EQ(Bits(x87::Round(angle)), Bits(x87::Round(fpu))) << std::hex << c.y << " " << c.x;
		EXPECT_EQ(Bits(angle.high + angle.low), Bits(fpu.high + fpu.low)) << std::hex << c.y << " " << c.x;
		const double positive = angle.high < 0.0 ? x87::Add(angle, 6.2831855f) : x87::Round(angle);
		EXPECT_EQ(Bits(positive), Bits(c.positive == c.high ? x87::Round(fpu) : D(c.positive)))
		    << std::hex << c.y << " " << c.x;
	}
}

TEST(GUtilsX87Oracle, TheThrownFireballsHeadingGivesTheOriginalsBits)
{
	for (const auto& c : golden::k_FireballCases)
	{
		const auto heading = FireballHeading(F(c.in[0]), F(c.in[1]), F(c.in[2]));
		for (int i = 0; i < 3; ++i)
		{
			EXPECT_EQ(Bits(heading[i]), c.out[i]) << std::hex << c.in[0] << " " << c.in[1] << " " << c.in[2] << " out " << i;
		}
	}
}

TEST(GUtilsX87Oracle, TheArccosineIsTheRuntimes)
{
	for (const auto& c : golden::k_AcosCases)
	{
		const auto angle = x87::Acos(F(c.x));
		EXPECT_EQ(Bits(angle.high), c.angle) << std::hex << c.x;
		EXPECT_EQ(angle.low, 0.0) << std::hex << c.x;
	}
	// The one input seen (in 250 000) where the processor's own last bit of the arctangent moves the double the
	// runtime returns by one unit: the float the game stores is the same
	const auto angle = x87::Acos(F(0x3F32BBBAu));
	EXPECT_EQ(Bits(angle.high), 0x3FE988CB108DF57Bu);
	EXPECT_EQ(Bits(x87::Store(std::bit_cast<double>(uint64_t {0x3FE988CB108DF57Cu}))), 0x3F4C4659u);
	EXPECT_EQ(Bits(x87::Store(angle)), 0x3F4C4659u);
}

TEST(GUtilsX87Oracle, ThePowerIsTheRuntimes)
{
	for (const auto& c : golden::k_PowCases)
	{
		EXPECT_EQ(Bits(x87::Pow(D(c.x), D(c.y))), c.power) << std::hex << c.x << " " << c.y;
	}
}

TEST(GUtilsX87Oracle, TheLogarithmIsTheFPUs)
{
	for (const auto& c : golden::k_LogCases)
	{
		EXPECT_TRUE(SameBits(x87::Log(F(c.x)), c.high, c.low)) << std::hex << c.x;
	}
	for (const auto& c : golden::k_LogLastBitCases)
	{
		const auto ln = x87::Log(F(c.x));
		const x87::Extended fpu {D(c.high), D(c.low)};
		EXPECT_TRUE(OneUnitApart(ln, c.high, c.low)) << std::hex << c.x;
		EXPECT_EQ(Bits(x87::Store(ln)), Bits(x87::Store(fpu))) << std::hex << c.x;
		EXPECT_EQ(Bits(x87::Round(ln)), Bits(x87::Round(fpu))) << std::hex << c.x;
	}
}

TEST(GUtilsX87, TheRuntimesSpecialPowers)
{
	constexpr double k_Infinity = std::numeric_limits<double>::infinity();
	// the parity of a negative base's exponent is tested on half of it at float precision: 2^25 + 1 counts as even
	EXPECT_EQ(x87::Pow(-1.0, 0x1p25 + 1.0), 1.0);
	EXPECT_EQ(x87::Pow(-1.0, 3.0), -1.0);
	EXPECT_TRUE(std::isnan(x87::Pow(-2.0, 0.5)));
	EXPECT_EQ(x87::Pow(-0.0, -3.0), -k_Infinity);
	EXPECT_EQ(x87::Pow(0.0, 0.0), 1.0);
	EXPECT_TRUE(std::isnan(x87::Pow(1.0, k_Infinity)));
	// past a double's range: infinite; below it the runtime cuts the bits that do not fit, towards zero
	EXPECT_EQ(x87::Pow(2.0, 1024.0), k_Infinity);
	EXPECT_EQ(x87::Pow(2.0, -1074.5), 0.0);
	EXPECT_EQ(Bits(x87::Pow(-2.0, -1075.0)), Bits(-0.0));
	EXPECT_EQ(Bits(x87::Pow(-2.0, -1077.0)), Bits(0.0));
	// the arccosine outside its domain, and at its ends
	EXPECT_TRUE(std::isnan(x87::Acos(1.5f).high));
	EXPECT_EQ(x87::Acos(1.0f).high, 0.0);
	EXPECT_EQ(Bits(x87::Acos(-1.0f).high), 0x400921FB54442D18u);
}
