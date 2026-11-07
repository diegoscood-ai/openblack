// compare.cpp: openblack's ports against the oracle's golden vectors (../vectors/*.json, made by gen_vectors.py).
//
// openblack's own sources are compiled in (read-only, from the openblack tree: src/3D/ObjectMatrix.cpp,
// src/3D/AffineMatrix.cpp, src/Common/GUtilsAngle.cpp, src/Common/GUtilsDistance.cpp), 64-bit with the flags of openblack's
// Release build (build_cmp.bat), so what runs here is what runs in the game. Every output is compared bit for bit;
// a mismatch is printed with the exact bits of both sides.
//
//   compare.exe [--max N] [--json results.json] [vector files or directories...]   (default: ..\vectors)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <cmath>
#include <cstdint>
#include <cstdio>

#include <bit>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <windows.h>

#include "3D/AffineMatrix.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/DanceShapes.h"

using json = nlohmann::ordered_json;
namespace fs = std::filesystem;
namespace affine = openblack::affine;
namespace gutils = openblack::gutils;
using openblack::map_coords::MapCoords;

namespace
{

// ---------------------------------------------------------------------------------------------------------------
// Values

uint64_t Bits(const json& v)
{
	return std::stoull(v.get<std::string>(), nullptr, 16);
}

float F32(const json& v)
{
	return std::bit_cast<float>(static_cast<uint32_t>(Bits(v)));
}

int32_t I32(const json& v)
{
	return static_cast<int32_t>(static_cast<uint32_t>(Bits(v)));
}

std::string Hex(uint64_t v, int digits)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "0x%0*llX", digits, static_cast<unsigned long long>(v));
	return buf;
}

std::string HexF32(float f)
{
	return Hex(std::bit_cast<uint32_t>(f), 8);
}

std::string HexF64(double d)
{
	return Hex(std::bit_cast<uint64_t>(d), 16);
}

std::vector<float> F32s(const json& v)
{
	std::vector<float> out;
	for (const auto& x : v)
	{
		out.push_back(F32(x));
	}
	return out;
}

json HexF32s(const float* p, size_t n)
{
	json a = json::array();
	for (size_t i = 0; i < n; ++i)
	{
		a.push_back(HexF32(p[i]));
	}
	return a;
}

/// glm::mat3 with m[r][c] = LHMatrix cell 3r + c (ObjectMatrix.h's convention)
glm::mat3 Mat3(const std::vector<float>& cells)
{
	glm::mat3 m;
	for (int r = 0; r < 3; ++r)
	{
		for (int c = 0; c < 3; ++c)
		{
			m[r][c] = cells[static_cast<size_t>(3 * r + c)];
		}
	}
	return m;
}

affine::AffineMatrix Mat43(const std::vector<float>& cells)
{
	affine::AffineMatrix m;
	for (size_t i = 0; i < 12; ++i)
	{
		m.m[i] = cells[i];
	}
	return m;
}

MapCoords Coords(const json& in, const std::string& prefix)
{
	return {I32(in[prefix + ".x"]), I32(in[prefix + ".z"]), 0.0f};
}

// ---------------------------------------------------------------------------------------------------------------
// The ports: given a case's "in" (and the file's constants), the outputs openblack computes (hex like the vectors)

struct Port
{
	std::string function; ///< the vector file's "function"
	std::string name;     ///< what openblack runs
	std::function<json(const json& in, const json& constants)> run;
};

std::vector<Port> Ports()
{
	std::vector<Port> ports;
	ports.push_back({"SetYXZMatrixOnly", "affine::RotationYXZ (src/3D/ObjectMatrix.cpp)", [](const json& in, const json& k) {
		                 const glm::mat3 m = affine::RotationYXZ(F32(in["y"]), F32(in["x"]), F32(in["z"]));
		                 float cells[9];
		                 for (int i = 0; i < 9; ++i)
		                 {
			                 cells[i] = m[i / 3][i % 3];
		                 }
		                 // the port returns the rotation only; the translation is the caller's, untouched
		                 return json {{"m", HexF32s(cells, 9)}, {"t", k["t"]}};
	                 }});
	ports.push_back({"GetYXZ", "affine::DecomposeYXZ (src/3D/ObjectMatrix.cpp)", [](const json& in, const json&) {
		                 float y = 0.0f;
		                 float x = 0.0f;
		                 float z = 0.0f;
		                 affine::DecomposeYXZ(Mat3(F32s(in["m"])), y, x, z);
		                 return json {{"y", HexF32(y)}, {"x", HexF32(x)}, {"z", HexF32(z)}};
	                 }});
	ports.push_back({"ArcTanOctant", "affine::ArcTanOctant (src/3D/ObjectMatrix.cpp), double -> float / double",
	                 [](const json& in, const json&) {
		                 const double r = affine::ArcTanOctant(F32(in["a"]), F32(in["b"]));
		                 // the 80-bit "ret" has no counterpart: compared are the caller's fstp dword and fstp qword
		                 return json {{"ret_f32", HexF32(static_cast<float>(r))}, {"ret_f64", HexF64(r)}};
	                 }});
	ports.push_back({"GetAngleFromXZ", "gutils::GetAngleFromXZ (src/Common/GUtilsAngle.cpp)", [](const json& in, const json&) {
		                 return json {{"ret", Hex(gutils::GetAngleFromXZ(Coords(in, "from"), Coords(in, "to")), 4)}};
	                 }});
	ports.push_back({"SetInverse", "affine::Inverse (src/3D/AffineMatrix.cpp)", [](const json& in, const json&) {
		                 const auto r = affine::Inverse(Mat43(F32s(in["m"])));
		                 return json {{"inv", HexF32s(r.m.data(), 12)}};
	                 }});
	ports.push_back({"SetInverse", "affine::Inverse (src/3D/ObjectMatrix.cpp)", [](const json& in, const json&) {
		                 const auto cells = F32s(in["m"]);
		                 glm::mat4x3 m;
		                 for (int r = 0; r < 4; ++r)
		                 {
			                 for (int c = 0; c < 3; ++c)
			                 {
				                 m[r][c] = cells[static_cast<size_t>(3 * r + c)];
			                 }
		                 }
		                 const glm::mat4x3 inv = affine::Inverse(m);
		                 float out[12];
		                 for (int i = 0; i < 12; ++i)
		                 {
			                 out[i] = inv[i / 3][i % 3];
		                 }
		                 return json {{"inv", HexF32s(out, 12)}};
	                 }});
	ports.push_back({"Mul", "affine::Mul (src/3D/AffineMatrix.cpp)", [](const json& in, const json&) {
		                 const auto r = affine::Mul(Mat43(F32s(in["a"])), Mat43(F32s(in["b"])));
		                 return json {{"a", HexF32s(r.m.data(), 12)}};
	                 }});
	ports.push_back({"GameRand",
	                 "GameRandomProduction::GameRand's rule: 0 for 0 with no draw, else SeededRandom "
	                 "(src/Common/GameRandomProduction.cpp, GameRandom.cpp)",
	                 [](const json& in, const json&) {
		                 uint32_t seed = static_cast<uint32_t>(Bits(in["seed"]));
		                 const auto max = static_cast<uint32_t>(Bits(in["max"]));
		                 const uint32_t r = max == 0 ? 0u : openblack::game_random::SeededRandom(max, seed);
		                 return json {{"ret", Hex(r, 8)}, {"seed", Hex(seed, 8)}};
	                 }});
	ports.push_back({"GameFloatRand", "FloatRand (src/Common/GameRandom.cpp)", [](const json& in, const json&) {
		                 uint32_t seed = static_cast<uint32_t>(Bits(in["seed"]));
		                 const float r = openblack::game_random::FloatRand(F32(in["scale"]), seed);
		                 return json {{"ret_f32", HexF32(r)}, {"seed", Hex(seed, 8)}};
	                 }});
	for (const auto& [function, index] : std::vector<std::pair<std::string, size_t>> {{"MakeShapeCircle", 0},
	                                                                                  {"MakeShapeWaveyCircle", 1},
	                                                                                  {"MakeShapeSpiral", 2},
	                                                                                  {"MakeShapeLoop", 3},
	                                                                                  {"MakeShapeTriangle", 8},
	                                                                                  {"MakeShapeHeart", 9},
	                                                                                  {"MakeShapeOctagon", 10}})
	{
		ports.push_back({function, "dance_shapes::Build(" + std::to_string(index) + ") (src/ECS/DanceShapes.cpp)",
		                 [index](const json&, const json&) {
			                 const auto shape = openblack::ecs::dance_shapes::Build(index);
			                 json points = json::array();
			                 for (const auto& p : shape)
			                 {
				                 points.push_back(Hex(static_cast<uint32_t>(p.x), 8));
				                 points.push_back(Hex(static_cast<uint32_t>(p.y), 8));
			                 }
			                 return json {{"points", points}};
		                 }});
	}
	ports.push_back({"GetDistance", "gutils::GetDistance (src/Common/GUtilsDistance.cpp)", [](const json& in, const json&) {
		                 const int32_t d = gutils::GetDistance(Coords(in, "a"), Coords(in, "b"));
		                 return json {{"ret", Hex(static_cast<uint32_t>(d), 8)}};
	                 }});
	ports.push_back(
	    {"ConvertAngle3DToGame", "gutils::ConvertAngle3DToGame (src/Common/GUtilsAngle.cpp)", [](const json& in, const json&) {
		     return json {{"ret", Hex(gutils::ConvertAngle3DToGame(F32(in["r"])), 8)}};
	     }});
	return ports;
}

/// Runs a port under SEH: openblack's code may fault where the exe does (an integer divide by zero, say)
struct Guarded
{
	const Port* port;
	const json* in;
	const json* constants;
	json out;
	std::string error;
};

void RunPort(Guarded* g)
{
	try
	{
		g->out = g->port->run(*g->in, *g->constants);
	}
	catch (const std::exception& e)
	{
		g->error = std::string("C++ exception: ") + e.what();
	}
}

DWORD SehRun(Guarded* g)
{
	__try
	{
		RunPort(g);
		return 0;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return GetExceptionCode();
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Comparison

enum class Kind
{
	Match,
	NanBits,    ///< both NaN, other bits
	SignedZero, ///< +0 vs -0
	Ulp1,       ///< one unit in the last place
	UlpMore,    ///< more than one ulp, same sign and finite
	Other       ///< integers that differ, NaN vs number, inf ...
};

const char* KindName(Kind k)
{
	switch (k)
	{
	case Kind::Match:
		return "match";
	case Kind::NanBits:
		return "nan bits";
	case Kind::SignedZero:
		return "signed zero";
	case Kind::Ulp1:
		return "1 ulp";
	case Kind::UlpMore:
		return ">1 ulp";
	default:
		return "different";
	}
}

int64_t UlpDistance(uint64_t a, uint64_t b, int bits)
{
	// ordered integer view of IEEE bits
	const uint64_t sign = 1ull << (bits - 1);
	const auto ordered = [&](uint64_t v) -> int64_t {
		return (v & sign) ? -static_cast<int64_t>(v & (sign - 1)) : static_cast<int64_t>(v);
	};
	const int64_t d = ordered(a) - ordered(b);
	return d < 0 ? -d : d;
}

Kind Classify(const std::string& type, uint64_t exe, uint64_t ob)
{
	if (exe == ob)
	{
		return Kind::Match;
	}
	if (type == "f32" || type == "f64")
	{
		const int bits = type == "f32" ? 32 : 64;
		const uint64_t expMask = type == "f32" ? 0x7F800000ull : 0x7FF0000000000000ull;
		const uint64_t manMask = type == "f32" ? 0x007FFFFFull : 0x000FFFFFFFFFFFFFull;
		const uint64_t signBit = 1ull << (bits - 1);
		const auto isNan = [&](uint64_t v) { return (v & expMask) == expMask && (v & manMask); };
		if (isNan(exe) && isNan(ob))
		{
			return Kind::NanBits;
		}
		if (isNan(exe) || isNan(ob))
		{
			return Kind::Other;
		}
		if (((exe | ob) & ~signBit) == 0)
		{
			return Kind::SignedZero;
		}
		const int64_t d = UlpDistance(exe, ob, bits);
		return d == 1 ? Kind::Ulp1 : Kind::UlpMore;
	}
	return Kind::Other;
}

std::string Show(const std::string& type, uint64_t bits)
{
	char buf[64];
	if (type == "f32")
	{
		snprintf(buf, sizeof(buf), "%s (%.9g)", Hex(bits, 8).c_str(), std::bit_cast<float>(static_cast<uint32_t>(bits)));
	}
	else if (type == "f64")
	{
		snprintf(buf, sizeof(buf), "%s (%.17g)", Hex(bits, 16).c_str(), std::bit_cast<double>(bits));
	}
	else if (type == "i32")
	{
		snprintf(buf, sizeof(buf), "%s (%d)", Hex(bits, 8).c_str(), static_cast<int32_t>(bits));
	}
	else
	{
		snprintf(buf, sizeof(buf), "%s (%llu)", Hex(bits, type == "u16" ? 4 : 8).c_str(),
		         static_cast<unsigned long long>(bits));
	}
	return buf;
}

std::string ShowInputs(const json& doc, const json& in)
{
	std::string s;
	for (const auto& schema : doc["inputs"])
	{
		const std::string name = schema["name"];
		const std::string type = schema["type"];
		const json& v = in[name];
		s += s.empty() ? "" : " ";
		s += name + "=";
		if (v.is_array())
		{
			s += "[";
			for (size_t i = 0; i < v.size(); ++i)
			{
				s += (i ? " " : "") + Show(type, Bits(v[i]));
			}
			s += "]";
		}
		else
		{
			s += Show(type, Bits(v));
		}
	}
	return s;
}

/// A string member, "" when missing or null
std::string Str(const json& doc, const char* key)
{
	return doc.contains(key) && doc[key].is_string() ? doc[key].get<std::string>() : std::string();
}

std::string OutputType(const json& doc, const std::string& name)
{
	for (const auto& o : doc["outputs"])
	{
		if (o["name"] == name)
		{
			return o["type"];
		}
	}
	return "?";
}

struct Tally
{
	size_t cases = 0;
	size_t matched = 0;    ///< every compared output equal
	size_t mismatched = 0; ///< at least one output differs
	size_t exeErrors = 0;  ///< the exe faulted (no golden output)
	size_t obFaults = 0;   ///< openblack faulted where the exe did not
	size_t bothFault = 0;  ///< both faulted
	size_t exeOnlyFault = 0;
	std::map<std::string, std::map<std::string, size_t>> kinds; ///< output -> kind -> values
	std::vector<std::string> examples;
	std::vector<std::string> outputsCompared;
	std::vector<std::string> outputsSkipped;
};

std::string ExceptionName(DWORD code)
{
	switch (code)
	{
	case EXCEPTION_INT_DIVIDE_BY_ZERO:
		return "integer divide by zero";
	case EXCEPTION_ACCESS_VIOLATION:
		return "access violation";
	case EXCEPTION_INT_OVERFLOW:
		return "integer overflow";
	default:
		return "exception " + Hex(code, 8);
	}
}

Tally Compare(const json& doc, const Port& port, size_t maxExamples)
{
	Tally t;
	const json& constants = doc.contains("constants") ? doc["constants"] : json::object();
	bool first = true;
	for (const auto& c : doc["cases"])
	{
		++t.cases;
		Guarded g {&port, &c["in"], &constants, {}, {}};
		const DWORD code = SehRun(&g);
		const bool obFault = code != 0 || !g.error.empty();
		const std::string obWhat = code ? ExceptionName(code) : g.error;
		if (c.contains("error"))
		{
			++t.exeErrors;
			if (obFault)
			{
				++t.bothFault;
			}
			else
			{
				++t.exeOnlyFault;
			}
			if (t.examples.size() < maxExamples)
			{
				t.examples.push_back("exe: " + c["error"].get<std::string>() +
				                     "; openblack: " + (obFault ? "faults too (" + obWhat + ")" : "returns " + g.out.dump()) +
				                     "\n      " + ShowInputs(doc, c["in"]));
			}
			continue;
		}
		if (obFault)
		{
			++t.obFaults;
			if (t.examples.size() < maxExamples)
			{
				t.examples.push_back("openblack faults (" + obWhat + "), the exe does not\n      " + ShowInputs(doc, c["in"]));
			}
			continue;
		}
		const json& exeOut = c["out"];
		if (first)
		{
			for (const auto& [name, _] : exeOut.items())
			{
				(g.out.contains(name) ? t.outputsCompared : t.outputsSkipped).push_back(name);
			}
			first = false;
		}
		bool all = true;
		std::string detail;
		for (const auto& [name, obValue] : g.out.items())
		{
			const std::string type = OutputType(doc, name);
			const json& exeValue = exeOut[name];
			const bool array = exeValue.is_array();
			const size_t n = array ? exeValue.size() : 1;
			for (size_t i = 0; i < n; ++i)
			{
				const uint64_t e = Bits(array ? exeValue[i] : exeValue);
				const uint64_t o = Bits(array ? obValue[i] : obValue);
				const Kind k = Classify(type, e, o);
				++t.kinds[name][KindName(k)];
				if (k != Kind::Match)
				{
					all = false;
					if (detail.size() < 600)
					{
						detail += "\n      " + name + (array ? "[" + std::to_string(i) + "]" : "") + ": exe " + Show(type, e) +
						          " openblack " + Show(type, o) + " [" + KindName(k) + "]";
					}
				}
			}
		}
		if (all)
		{
			++t.matched;
		}
		else
		{
			++t.mismatched;
			if (t.examples.size() < maxExamples)
			{
				t.examples.push_back(ShowInputs(doc, c["in"]) + detail);
			}
		}
	}
	return t;
}

} // namespace

int main(int argc, char** argv)
{
	size_t maxExamples = 6;
	std::string jsonOut;
	std::vector<fs::path> inputs;
	for (int i = 1; i < argc; ++i)
	{
		const std::string a = argv[i];
		if (a == "--max" && i + 1 < argc)
		{
			maxExamples = static_cast<size_t>(std::stoul(argv[++i]));
		}
		else if (a == "--json" && i + 1 < argc)
		{
			jsonOut = argv[++i];
		}
		else
		{
			inputs.emplace_back(a);
		}
	}
	if (inputs.empty())
	{
		inputs.push_back(fs::path(argv[0]).parent_path() / ".." / "vectors");
	}
	std::vector<fs::path> files;
	for (const auto& p : inputs)
	{
		if (fs::is_directory(p))
		{
			for (const auto& e : fs::directory_iterator(p))
			{
				if (e.path().extension() == ".json")
				{
					files.push_back(e.path());
				}
			}
		}
		else
		{
			files.push_back(p);
		}
	}
	std::sort(files.begin(), files.end());

	const auto ports = Ports();
	json summary = json::array();
	int failures = 0;
	for (const auto& file : files)
	{
		std::ifstream f(file);
		json doc;
		try
		{
			doc = json::parse(f);
		}
		catch (const std::exception& e)
		{
			printf("%s: %s\n", file.string().c_str(), e.what());
			++failures;
			continue;
		}
		const std::string function = Str(doc, "function");
		bool any = false;
		for (const auto& port : ports)
		{
			if (port.function != function)
			{
				continue;
			}
			any = true;
			Tally t;
			try
			{
				t = Compare(doc, port, maxExamples);
			}
			catch (const std::exception& e)
			{
				printf("== %s vs %s: %s\n", function.c_str(), port.name.c_str(), e.what());
				++failures;
				continue;
			}
			const std::string symbol = Str(doc, "symbol");
			printf("== %s %s (%s) [%s]\n   vs %s\n", function.c_str(), Str(doc, "addr").c_str(),
			       symbol.empty() ? "no symbol" : symbol.c_str(), file.filename().string().c_str(), port.name.c_str());
			printf("   %zu cases: %zu match, %zu mismatch; exe faults %zu (openblack faults too: %zu); openblack-only "
			       "faults %zu\n",
			       t.cases, t.matched, t.mismatched, t.exeErrors, t.bothFault, t.obFaults);
			std::string kinds;
			for (const auto& [output, perKind] : t.kinds)
			{
				kinds += (kinds.empty() ? "" : "; ") + output + ":";
				for (const auto& [k, n] : perKind)
				{
					kinds += " " + k + " " + std::to_string(n);
				}
			}
			std::string skipped;
			for (const auto& n : t.outputsSkipped)
			{
				skipped += (skipped.empty() ? "" : " ") + n;
			}
			printf("   values (%s)%s\n", kinds.c_str(), skipped.empty() ? "" : ("; not compared: " + skipped).c_str());
			for (const auto& e : t.examples)
			{
				printf("    - %s\n", e.c_str());
			}
			if (t.mismatched || t.obFaults || t.exeOnlyFault)
			{
				++failures;
			}
			summary.push_back({{"function", function},
			                   {"addr", Str(doc, "addr")},
			                   {"vectors", file.filename().string()},
			                   {"port", port.name},
			                   {"cases", t.cases},
			                   {"match", t.matched},
			                   {"mismatch", t.mismatched},
			                   {"exe_faults", t.exeErrors},
			                   {"both_fault", t.bothFault},
			                   {"openblack_only_faults", t.obFaults},
			                   {"values", t.kinds},
			                   {"examples", t.examples}});
		}
		if (!any)
		{
			printf("== %s: no openblack port registered in compare.cpp\n", function.c_str());
		}
	}
	if (!jsonOut.empty())
	{
		std::ofstream(jsonOut) << summary.dump(1) << "\n";
	}
	return failures ? 1 : 0;
}
