// oracle_host: calls functions of the original runblack.exe natively, one JSON request per line on stdin, one JSON
// response per line on stdout (protocol in README.md).
//
// The host is linked at a fixed high base (build.bat: /BASE:0x60000000 /FIXED /DYNAMICBASE:NO), so the exe's range
// 0x00400000..+SizeOfImage is free. The exe has no relocations: its headers and sections are copied to their RVAs in
// one VirtualAlloc at its ImageBase (PAGE_EXECUTE_READWRITE), .bss is the zeroed tail of each section. Neither the entry
// point nor the CRT / static initialisers run. Every IAT slot points at a trap stub: an import call aborts the request
// and reports which import it was. Faults inside a call (access violation, divide by zero, int3, ...) are caught by a
// vectored handler and reported; a watchdog aborts calls that run too long. Before every call the x87 is reset and
// loaded with the game's control word (fn_007DEE00: fninit; and cw, 0xFCFF = 0x007F, 24-bit precision, round to
// nearest); the host's control word and MXCSR are restored afterwards.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <io.h>
#include <malloc.h>
#include <windows.h>

#if !defined(_M_IX86)
#error oracle_host must be built for 32-bit x86 (build.bat calls vcvars32.bat)
#endif

namespace
{

const char* const k_HostVersion = "oracle_host 1";
constexpr uint16_t k_GameFpuCw = 0x007F;    // fninit (0x037F) & 0xFCFF: 24-bit precision, round to nearest, all masked
constexpr uint32_t k_DefaultMxcsr = 0x1F80; // power-on MXCSR
constexpr uint32_t k_ArenaPreferred = 0x20000000;
constexpr uint32_t k_ArenaSize = 64u << 20;
constexpr uint32_t k_GuardBytes = 16;
constexpr uint8_t k_GuardFill = 0xFD;
constexpr uint32_t k_MaxStackDwords = 256;
constexpr uint32_t k_DefaultTimeoutMs = 5000;
constexpr uint32_t k_WorkerStack = 16u << 20; // reserve of the thread that runs the calls

// ---------------------------------------------------------------------------------------------------------------
// Minimal JSON

struct JValue
{
	enum Type
	{
		Null,
		Bool,
		Number,
		String,
		Array,
		Object
	};
	Type type = Null;
	bool boolean = false;
	std::string text; // a number's source text, or a string's value
	std::vector<JValue> items;
	std::vector<std::pair<std::string, JValue>> members;

	const JValue* Find(const char* key) const
	{
		for (const auto& m : members)
		{
			if (m.first == key)
			{
				return &m.second;
			}
		}
		return nullptr;
	}
};

class JParser
{
public:
	JParser(const char* begin, const char* end)
	    : p_(begin)
	    , end_(end)
	{
	}

	bool Parse(JValue& out, std::string& error)
	{
		if (!ParseValue(out, 0))
		{
			error = error_;
			return false;
		}
		SkipWs();
		if (p_ != end_)
		{
			error = "trailing characters after the JSON value";
			return false;
		}
		return true;
	}

private:
	const char* p_;
	const char* end_;
	std::string error_;

	void SkipWs()
	{
		while (p_ < end_ && (*p_ == ' ' || *p_ == '\t' || *p_ == '\r' || *p_ == '\n'))
		{
			++p_;
		}
	}
	bool Fail(const char* message)
	{
		error_ = message;
		return false;
	}
	bool Match(const char* word)
	{
		const size_t n = strlen(word);
		if (static_cast<size_t>(end_ - p_) >= n && memcmp(p_, word, n) == 0)
		{
			p_ += n;
			return true;
		}
		return false;
	}
	bool ParseValue(JValue& v, int depth)
	{
		if (depth > 64)
		{
			return Fail("JSON nested too deep");
		}
		SkipWs();
		if (p_ >= end_)
		{
			return Fail("unexpected end of JSON");
		}
		const char c = *p_;
		if (c == '{')
		{
			return ParseObject(v, depth);
		}
		if (c == '[')
		{
			return ParseArray(v, depth);
		}
		if (c == '"')
		{
			v.type = JValue::String;
			return ParseString(v.text);
		}
		if (Match("true"))
		{
			v.type = JValue::Bool;
			v.boolean = true;
			return true;
		}
		if (Match("false"))
		{
			v.type = JValue::Bool;
			v.boolean = false;
			return true;
		}
		if (Match("null"))
		{
			v.type = JValue::Null;
			return true;
		}
		if (c == '-' || (c >= '0' && c <= '9'))
		{
			const char* start = p_;
			++p_;
			while (p_ < end_ &&
			       ((*p_ >= '0' && *p_ <= '9') || *p_ == '.' || *p_ == 'e' || *p_ == 'E' || *p_ == '+' || *p_ == '-'))
			{
				++p_;
			}
			v.type = JValue::Number;
			v.text.assign(start, p_);
			return true;
		}
		return Fail("unexpected character in JSON");
	}
	static void AppendUtf8(std::string& s, uint32_t cp)
	{
		if (cp < 0x80)
		{
			s += static_cast<char>(cp);
		}
		else if (cp < 0x800)
		{
			s += static_cast<char>(0xC0 | (cp >> 6));
			s += static_cast<char>(0x80 | (cp & 0x3F));
		}
		else if (cp < 0x10000)
		{
			s += static_cast<char>(0xE0 | (cp >> 12));
			s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
			s += static_cast<char>(0x80 | (cp & 0x3F));
		}
		else
		{
			s += static_cast<char>(0xF0 | (cp >> 18));
			s += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
			s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
			s += static_cast<char>(0x80 | (cp & 0x3F));
		}
	}
	bool ParseHex4(uint32_t& out)
	{
		if (end_ - p_ < 4)
		{
			return Fail("bad \\u escape");
		}
		out = 0;
		for (int i = 0; i < 4; ++i)
		{
			const char c = *p_++;
			out <<= 4;
			if (c >= '0' && c <= '9')
			{
				out |= static_cast<uint32_t>(c - '0');
			}
			else if (c >= 'a' && c <= 'f')
			{
				out |= static_cast<uint32_t>(c - 'a' + 10);
			}
			else if (c >= 'A' && c <= 'F')
			{
				out |= static_cast<uint32_t>(c - 'A' + 10);
			}
			else
			{
				return Fail("bad \\u escape");
			}
		}
		return true;
	}
	bool ParseString(std::string& out)
	{
		++p_; // the opening quote
		out.clear();
		while (p_ < end_)
		{
			const char c = *p_++;
			if (c == '"')
			{
				return true;
			}
			if (c != '\\')
			{
				out += c;
				continue;
			}
			if (p_ >= end_)
			{
				break;
			}
			const char e = *p_++;
			switch (e)
			{
			case '"':
			case '\\':
			case '/':
				out += e;
				break;
			case 'b':
				out += '\b';
				break;
			case 'f':
				out += '\f';
				break;
			case 'n':
				out += '\n';
				break;
			case 'r':
				out += '\r';
				break;
			case 't':
				out += '\t';
				break;
			case 'u':
			{
				uint32_t cp = 0;
				if (!ParseHex4(cp))
				{
					return false;
				}
				if (cp >= 0xD800 && cp < 0xDC00 && end_ - p_ >= 6 && p_[0] == '\\' && p_[1] == 'u')
				{
					p_ += 2;
					uint32_t low = 0;
					if (!ParseHex4(low))
					{
						return false;
					}
					cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
				}
				AppendUtf8(out, cp);
				break;
			}
			default:
				return Fail("bad escape in JSON string");
			}
		}
		return Fail("unterminated JSON string");
	}
	bool ParseArray(JValue& v, int depth)
	{
		v.type = JValue::Array;
		++p_;
		SkipWs();
		if (p_ < end_ && *p_ == ']')
		{
			++p_;
			return true;
		}
		while (true)
		{
			v.items.emplace_back();
			if (!ParseValue(v.items.back(), depth + 1))
			{
				return false;
			}
			SkipWs();
			if (p_ < end_ && *p_ == ',')
			{
				++p_;
				continue;
			}
			if (p_ < end_ && *p_ == ']')
			{
				++p_;
				return true;
			}
			return Fail("expected , or ] in JSON array");
		}
	}
	bool ParseObject(JValue& v, int depth)
	{
		v.type = JValue::Object;
		++p_;
		SkipWs();
		if (p_ < end_ && *p_ == '}')
		{
			++p_;
			return true;
		}
		while (true)
		{
			SkipWs();
			if (p_ >= end_ || *p_ != '"')
			{
				return Fail("expected a key in JSON object");
			}
			std::string key;
			if (!ParseString(key))
			{
				return false;
			}
			SkipWs();
			if (p_ >= end_ || *p_ != ':')
			{
				return Fail("expected : in JSON object");
			}
			++p_;
			v.members.emplace_back(std::move(key), JValue {});
			if (!ParseValue(v.members.back().second, depth + 1))
			{
				return false;
			}
			SkipWs();
			if (p_ < end_ && *p_ == ',')
			{
				++p_;
				continue;
			}
			if (p_ < end_ && *p_ == '}')
			{
				++p_;
				return true;
			}
			return Fail("expected , or } in JSON object");
		}
	}
};

void AppendEscaped(std::string& out, const std::string& s)
{
	out += '"';
	for (const unsigned char c : s)
	{
		switch (c)
		{
		case '"':
			out += "\\\"";
			break;
		case '\\':
			out += "\\\\";
			break;
		case '\n':
			out += "\\n";
			break;
		case '\r':
			out += "\\r";
			break;
		case '\t':
			out += "\\t";
			break;
		default:
			if (c < 0x20)
			{
				char buf[8];
				snprintf(buf, sizeof(buf), "\\u%04x", c);
				out += buf;
			}
			else
			{
				out += static_cast<char>(c);
			}
		}
	}
	out += '"';
}

std::string Hex(uint64_t v, int digits)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "0x%0*llX", digits, static_cast<unsigned long long>(v));
	return buf;
}

std::string HexBytes(const uint8_t* p, size_t n)
{
	static const char k_Digits[] = "0123456789abcdef";
	std::string s;
	s.reserve(n * 2);
	for (size_t i = 0; i < n; ++i)
	{
		s += k_Digits[p[i] >> 4];
		s += k_Digits[p[i] & 15];
	}
	return s;
}

/// The 80-bit value as 20 hex digits, sign and exponent first (byte 9 down to byte 0)
std::string Hex80(const uint8_t* p)
{
	static const char k_Digits[] = "0123456789ABCDEF";
	std::string s = "0x";
	for (int i = 9; i >= 0; --i)
	{
		s += k_Digits[p[i] >> 4];
		s += k_Digits[p[i] & 15];
	}
	return s;
}

std::string JsonDouble(double v, int digits)
{
	if (std::isnan(v))
	{
		return "\"NaN\"";
	}
	if (std::isinf(v))
	{
		return v > 0 ? "\"Infinity\"" : "\"-Infinity\"";
	}
	char buf[64];
	snprintf(buf, sizeof(buf), "%.*g", digits, v);
	return buf;
}

// ---------------------------------------------------------------------------------------------------------------
// Call machinery (globals only: the naked thunks address them absolutely)

volatile uint32_t g_inCall = 0;
uint32_t g_target = 0;
uint32_t g_nargs = 0;
uint32_t g_args[k_MaxStackDwords];
uint32_t g_regEax = 0, g_regEbx = 0, g_regEcx = 0, g_regEdx = 0, g_regEsi = 0, g_regEdi = 0;
uint32_t g_savedEsp = 0, g_savedSeh = 0, g_espAfter = 0;
uint32_t g_retEax = 0, g_retEdx = 0;
volatile uint32_t g_status = 0; // 1 returned, 2 import trap, 3 exception, 4 timeout
uint32_t g_trapIndex = 0, g_trapFrom = 0;
uint32_t g_excCode = 0, g_excAddr = 0, g_excInfo0 = 0, g_excInfo1 = 0, g_excParams = 0;
uint16_t g_gameCw = k_GameFpuCw;
uint16_t g_hostCw = 0;
uint32_t g_gameMxcsr = k_DefaultMxcsr;
uint32_t g_hostMxcsr = 0;
uint8_t g_fpuState[108];
uint8_t g_fpuIn[8 * 10];
uint32_t g_fpuInCount = 0;
uint32_t g_trapCommonPtr = 0;

uint32_t g_imageBase = 0;
uint32_t g_imageSize = 0;

// C4733: the thunks only put back the SEH head saved before the call
#pragma warning(disable : 4733)
__declspec(naked) void RecoverStub()
{
	__asm {
		mov esp, g_savedEsp
		mov g_inCall, 0
		fnsave g_fpuState
		ldmxcsr g_hostMxcsr
		fldcw g_hostCw
		mov eax, g_savedSeh
		mov dword ptr fs:[0], eax
		cld
		popad
		ret
	}
}

/// Reached from every import stub with eax = the import's index and [esp] = the return address into the exe
__declspec(naked) void TrapCommon()
{
	__asm {
		mov g_trapIndex, eax
		mov eax, dword ptr [esp]
		mov g_trapFrom, eax
		mov g_status, 2
		jmp RecoverStub
	}
}

__declspec(naked) void CallThunk()
{
	__asm {
		pushad
		mov eax, dword ptr fs:[0]
		mov g_savedSeh, eax
		mov g_savedEsp, esp
		mov ecx, g_nargs
	push_loop:
		test ecx, ecx
		jz pushed
		dec ecx
		push dword ptr g_args[ecx * 4]
		jmp push_loop
	pushed:
		stmxcsr g_hostMxcsr
		ldmxcsr g_gameMxcsr
		fnstcw g_hostCw
		fninit
		fldcw g_gameCw
		mov ecx, g_fpuInCount
		lea esi, g_fpuIn
	fpu_loop:
		test ecx, ecx
		jz fpu_done
		fld tbyte ptr [esi]
		add esi, 10
		dec ecx
		jmp fpu_loop
	fpu_done:
		mov eax, g_regEax
		mov ebx, g_regEbx
		mov ecx, g_regEcx
		mov edx, g_regEdx
		mov esi, g_regEsi
		mov edi, g_regEdi
		cld
		mov g_inCall, 1
		call dword ptr g_target
		mov g_inCall, 0
		mov g_espAfter, esp
		mov g_retEax, eax
		mov g_retEdx, edx
		mov g_status, 1
		fnsave g_fpuState
		stmxcsr g_gameMxcsr
		ldmxcsr g_hostMxcsr
		fldcw g_hostCw
		mov esp, g_savedEsp
		mov eax, g_savedSeh
		mov dword ptr fs:[0], eax
		cld
		popad
		ret
	}
}

uint8_t g_cvtIn[10];
uint32_t g_cvtF32 = 0;
uint64_t g_cvtF64 = 0;
uint16_t g_cvtHostCw = 0;

/// What the caller's fstp dword / fstp qword of ST0 stores, under the game's control word
void ConvertExtended()
{
	__asm {
		fnstcw g_cvtHostCw
		fninit
		fldcw g_gameCw
		fld tbyte ptr g_cvtIn
		fst dword ptr g_cvtF32
		fstp qword ptr g_cvtF64
		fninit
		fldcw g_cvtHostCw
	}
}

uint64_t g_ldF64 = 0;

/// An f64 made 80-bit by fld qword (exact)
void LoadF64To80(uint8_t out[10])
{
	__asm {
		fnstcw g_cvtHostCw
		fninit
		fld qword ptr g_ldF64
		fstp tbyte ptr g_cvtIn
		fninit
		fldcw g_cvtHostCw
	}
	memcpy(out, g_cvtIn, 10);
}

HANDLE g_mainThread = nullptr; // the worker thread that runs the calls
DWORD g_mainThreadId = 0;
volatile DWORD g_deadline = 0;

LONG CALLBACK VectoredHandler(EXCEPTION_POINTERS* ep)
{
	if (!g_inCall || GetCurrentThreadId() != g_mainThreadId)
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}
	const EXCEPTION_RECORD* er = ep->ExceptionRecord;
	g_excCode = er->ExceptionCode;
	g_excAddr = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(er->ExceptionAddress));
	g_excParams = er->NumberParameters;
	g_excInfo0 = er->NumberParameters > 0 ? static_cast<uint32_t>(er->ExceptionInformation[0]) : 0;
	g_excInfo1 = er->NumberParameters > 1 ? static_cast<uint32_t>(er->ExceptionInformation[1]) : 0;
	g_status = 3;
	ep->ContextRecord->Eip = static_cast<DWORD>(reinterpret_cast<uintptr_t>(&RecoverStub));
	return EXCEPTION_CONTINUE_EXECUTION;
}

DWORD WINAPI Watchdog(LPVOID)
{
	while (true)
	{
		Sleep(25);
		if (!g_inCall || static_cast<LONG>(GetTickCount() - g_deadline) < 0)
		{
			continue;
		}
		if (SuspendThread(g_mainThread) == static_cast<DWORD>(-1))
		{
			continue;
		}
		CONTEXT ctx {};
		ctx.ContextFlags = CONTEXT_CONTROL;
		if (g_inCall && GetThreadContext(g_mainThread, &ctx) && ctx.Eip >= g_imageBase && ctx.Eip < g_imageBase + g_imageSize)
		{
			g_excAddr = ctx.Eip;
			g_status = 4;
			ctx.Eip = static_cast<DWORD>(reinterpret_cast<uintptr_t>(&RecoverStub));
			SetThreadContext(g_mainThread, &ctx);
		}
		ResumeThread(g_mainThread);
	}
}

bool SafeCopy(void* dst, const void* src, size_t n)
{
	__try
	{
		memcpy(dst, src, n);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

// ---------------------------------------------------------------------------------------------------------------
// The exe image

struct Import
{
	std::string dll;
	std::string name;
	uint32_t iat = 0;
};

std::string g_exePath;
std::vector<uint8_t> g_pristine;
std::vector<uint8_t> g_snapshot;
uint32_t g_snapshotMxcsr = k_DefaultMxcsr;
bool g_hasSnapshot = false;
std::vector<Import> g_imports;
uint8_t* g_stubs = nullptr;
uint32_t g_entryPoint = 0;
DWORD g_loadError = 0; // GetLastError of a failed VirtualAlloc of the image

std::string DescribeRegion(uint32_t addr)
{
	MEMORY_BASIC_INFORMATION mbi {};
	if (!VirtualQuery(reinterpret_cast<void*>(static_cast<uintptr_t>(addr)), &mbi, sizeof(mbi)))
	{
		return "VirtualQuery failed";
	}
	char buf[160];
	snprintf(buf, sizeof(buf), "region 0x%08X size 0x%X state 0x%X type 0x%X (allocation base 0x%08X)",
	         static_cast<unsigned>(reinterpret_cast<uintptr_t>(mbi.BaseAddress)), static_cast<unsigned>(mbi.RegionSize),
	         static_cast<unsigned>(mbi.State), static_cast<unsigned>(mbi.Type),
	         static_cast<unsigned>(reinterpret_cast<uintptr_t>(mbi.AllocationBase)));
	return buf;
}

/// The regions of [lo, hi) that are not free, for a failed VirtualAlloc
std::string MemoryMap(uint32_t lo, uint32_t hi)
{
	std::string out;
	for (uint32_t a = lo; a < hi;)
	{
		MEMORY_BASIC_INFORMATION mbi {};
		if (!VirtualQuery(reinterpret_cast<void*>(static_cast<uintptr_t>(a)), &mbi, sizeof(mbi)) || !mbi.RegionSize)
		{
			break;
		}
		const auto base = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(mbi.BaseAddress));
		if (mbi.State != MEM_FREE)
		{
			char buf[96];
			const char* state = mbi.State == MEM_COMMIT ? "commit" : "reserve";
			const char* type = mbi.Type == MEM_IMAGE ? "image" : mbi.Type == MEM_MAPPED ? "mapped" : "private";
			snprintf(buf, sizeof(buf), "%s0x%08X+0x%X %s %s", out.empty() ? "" : ", ", base,
			         static_cast<unsigned>(mbi.RegionSize), state, type);
			out += buf;
		}
		a = base + static_cast<uint32_t>(mbi.RegionSize);
		if (a <= base)
		{
			break;
		}
	}
	return out;
}

bool LoadExe(const std::string& path, bool reserved, std::string& error)
{
	FILE* f = fopen(path.c_str(), "rb");
	if (!f)
	{
		error = "cannot open " + path;
		return false;
	}
	std::vector<uint8_t> file;
	uint8_t chunk[65536];
	size_t n = 0;
	while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0)
	{
		file.insert(file.end(), chunk, chunk + n);
	}
	fclose(f);
	if (file.size() < 0x400 || file[0] != 'M' || file[1] != 'Z')
	{
		error = "not an MZ executable";
		return false;
	}
	const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(file.data());
	if (static_cast<size_t>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS32) > file.size())
	{
		error = "bad e_lfanew";
		return false;
	}
	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(file.data() + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
	    nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
	{
		error = "not a 32-bit x86 PE image";
		return false;
	}
	g_imageBase = nt->OptionalHeader.ImageBase;
	g_imageSize = nt->OptionalHeader.SizeOfImage;
	g_entryPoint = g_imageBase + nt->OptionalHeader.AddressOfEntryPoint;
	// A relaunched host finds the range already reserved by its parent (RelaunchReserved): commit it there
	void* base = VirtualAlloc(reinterpret_cast<void*>(static_cast<uintptr_t>(g_imageBase)), g_imageSize,
	                          (reserved ? 0 : MEM_RESERVE) | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (base != reinterpret_cast<void*>(static_cast<uintptr_t>(g_imageBase)))
	{
		g_loadError = GetLastError();
		char buf[256];
		snprintf(buf, sizeof(buf), "VirtualAlloc(0x%08X, 0x%X) failed (error %lu); at the base: ", g_imageBase, g_imageSize,
		         g_loadError);
		error =
		    buf + DescribeRegion(g_imageBase) + "; used below the image end: " + MemoryMap(0x10000, g_imageBase + g_imageSize);
		return false;
	}
	auto* image = static_cast<uint8_t*>(base);
	const uint32_t headers = std::min<uint32_t>(nt->OptionalHeader.SizeOfHeaders, static_cast<uint32_t>(file.size()));
	memcpy(image, file.data(), headers);
	const auto* sections = IMAGE_FIRST_SECTION(nt);
	for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i)
	{
		const auto& s = sections[i];
		const uint32_t virtualSize = s.Misc.VirtualSize ? s.Misc.VirtualSize : s.SizeOfRawData;
		uint32_t raw = std::min<uint32_t>(s.SizeOfRawData, virtualSize);
		if (s.PointerToRawData >= file.size())
		{
			raw = 0;
		}
		raw = std::min<uint32_t>(raw, static_cast<uint32_t>(file.size()) -
		                                  std::min<uint32_t>(s.PointerToRawData, static_cast<uint32_t>(file.size())));
		if (s.VirtualAddress + virtualSize > g_imageSize)
		{
			error = "a section lies outside SizeOfImage";
			return false;
		}
		memcpy(image + s.VirtualAddress, file.data() + s.PointerToRawData, raw); // the rest stays zero (.bss)
	}

	// The imports: every IAT slot -> a stub "mov eax, index; jmp [g_trapCommonPtr]"
	const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	g_trapCommonPtr = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&TrapCommon));
	std::vector<std::pair<uint32_t, Import>> slots;
	if (dir.VirtualAddress && dir.Size)
	{
		for (auto* d = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(image + dir.VirtualAddress); d->Name; ++d)
		{
			const std::string dll = reinterpret_cast<const char*>(image + d->Name);
			const uint32_t names = d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk;
			for (uint32_t j = 0;; ++j)
			{
				const uint32_t thunk = *reinterpret_cast<const uint32_t*>(image + names + 4 * j);
				if (!thunk)
				{
					break;
				}
				Import imp;
				imp.dll = dll;
				if (thunk & IMAGE_ORDINAL_FLAG32)
				{
					imp.name = "#" + std::to_string(thunk & 0xFFFF);
				}
				else
				{
					imp.name = reinterpret_cast<const char*>(image + thunk + 2);
				}
				imp.iat = g_imageBase + d->FirstThunk + 4 * j;
				g_imports.push_back(imp);
			}
		}
	}
	const size_t stubBytes = std::max<size_t>(16, g_imports.size() * 16);
	g_stubs = static_cast<uint8_t*>(VirtualAlloc(nullptr, stubBytes, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
	if (!g_stubs)
	{
		error = "cannot allocate the import stubs";
		return false;
	}
	memset(g_stubs, 0xCC, stubBytes);
	for (size_t k = 0; k < g_imports.size(); ++k)
	{
		uint8_t* s = g_stubs + 16 * k;
		s[0] = 0xB8; // mov eax, imm32
		const auto index = static_cast<uint32_t>(k);
		memcpy(s + 1, &index, 4);
		s[5] = 0xFF; // jmp dword ptr [imm32]
		s[6] = 0x25;
		const auto target = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&g_trapCommonPtr));
		memcpy(s + 7, &target, 4);
		const auto stub = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(s));
		memcpy(reinterpret_cast<void*>(static_cast<uintptr_t>(g_imports[k].iat)), &stub, 4);
	}
	FlushInstructionCache(GetCurrentProcess(), g_stubs, stubBytes);
	g_pristine.assign(image, image + g_imageSize);
	return true;
}

uint8_t* ImagePtr()
{
	return reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(g_imageBase));
}

void ResetImage(bool pristine)
{
	if (pristine)
	{
		g_hasSnapshot = false;
		g_snapshot.clear();
	}
	if (g_hasSnapshot)
	{
		memcpy(ImagePtr(), g_snapshot.data(), g_imageSize);
		g_gameMxcsr = g_snapshotMxcsr;
	}
	else
	{
		memcpy(ImagePtr(), g_pristine.data(), g_imageSize);
		g_gameMxcsr = k_DefaultMxcsr;
	}
	FlushInstructionCache(GetCurrentProcess(), ImagePtr(), g_imageSize);
}

void TakeSnapshot()
{
	g_snapshot.assign(ImagePtr(), ImagePtr() + g_imageSize);
	g_snapshotMxcsr = g_gameMxcsr;
	g_hasSnapshot = true;
}

// ---------------------------------------------------------------------------------------------------------------
// Scratch buffers

struct Buffer
{
	std::string name;
	uint32_t addr = 0;
	uint32_t size = 0;
};

uint8_t* g_arena = nullptr;
uint32_t g_arenaUsed = 0;
std::vector<Buffer> g_buffers;

bool InitArena(std::string& error)
{
	g_arena = static_cast<uint8_t*>(VirtualAlloc(reinterpret_cast<void*>(static_cast<uintptr_t>(k_ArenaPreferred)), k_ArenaSize,
	                                             MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
	if (!g_arena)
	{
		g_arena = static_cast<uint8_t*>(VirtualAlloc(nullptr, k_ArenaSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
	}
	if (!g_arena)
	{
		error = "cannot allocate the scratch arena";
		return false;
	}
	return true;
}

uint32_t ArenaAddr()
{
	return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_arena));
}

// ---------------------------------------------------------------------------------------------------------------
// Value parsing

bool ParseUnsignedText(const std::string& s, uint64_t& out)
{
	if (s.empty())
	{
		return false;
	}
	const char* p = s.c_str();
	char* end = nullptr;
	errno = 0;
	if (s.size() > 2 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
	{
		out = _strtoui64(p + 2, &end, 16);
	}
	else
	{
		out = _strtoui64(p, &end, 10);
	}
	return errno == 0 && end && *end == '\0' && end != p;
}

/// An integer: a JSON integer, or a string "0x..", "-0x..", decimal, "@buffer", "@buffer+off", "@buffer-off"
bool ToInteger(const JValue& v, int64_t& out, std::string& error)
{
	if (v.type == JValue::Number)
	{
		if (v.text.find_first_of(".eE") != std::string::npos)
		{
			const double d = strtod(v.text.c_str(), nullptr);
			if (d != std::floor(d))
			{
				error = "not an integer: " + v.text;
				return false;
			}
			out = static_cast<int64_t>(d);
			return true;
		}
		const bool negative = v.text[0] == '-';
		uint64_t u = 0;
		if (!ParseUnsignedText(negative ? v.text.substr(1) : v.text, u))
		{
			error = "bad integer: " + v.text;
			return false;
		}
		out = negative ? -static_cast<int64_t>(u) : static_cast<int64_t>(u);
		return true;
	}
	if (v.type == JValue::Bool)
	{
		out = v.boolean ? 1 : 0;
		return true;
	}
	if (v.type != JValue::String)
	{
		error = "expected an integer or an address";
		return false;
	}
	const std::string& s = v.text;
	if (!s.empty() && s[0] == '@')
	{
		size_t cut = s.find_first_of("+-", 1);
		const std::string name = s.substr(1, cut == std::string::npos ? std::string::npos : cut - 1);
		int64_t offset = 0;
		if (cut != std::string::npos)
		{
			uint64_t u = 0;
			if (!ParseUnsignedText(s.substr(cut + 1), u))
			{
				error = "bad buffer offset in " + s;
				return false;
			}
			offset = s[cut] == '-' ? -static_cast<int64_t>(u) : static_cast<int64_t>(u);
		}
		for (const auto& b : g_buffers)
		{
			if (b.name == name)
			{
				out = static_cast<int64_t>(b.addr) + offset;
				return true;
			}
		}
		error = "unknown buffer @" + name;
		return false;
	}
	const bool negative = !s.empty() && s[0] == '-';
	uint64_t u = 0;
	if (!ParseUnsignedText(negative ? s.substr(1) : s, u))
	{
		error = "bad integer: " + s;
		return false;
	}
	out = negative ? -static_cast<int64_t>(u) : static_cast<int64_t>(u);
	return true;
}

bool ToU32(const JValue& v, uint32_t& out, std::string& error)
{
	int64_t i = 0;
	if (!ToInteger(v, i, error))
	{
		return false;
	}
	if (i < -2147483648LL || i > 0xFFFFFFFFLL)
	{
		error = "integer out of 32-bit range";
		return false;
	}
	out = static_cast<uint32_t>(i);
	return true;
}

bool IsHexBits(const std::string& s)
{
	return s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X');
}

bool SpecialFloat(const std::string& s, double& out)
{
	if (s == "nan" || s == "NaN")
	{
		out = std::numeric_limits<double>::quiet_NaN();
		return true;
	}
	if (s == "inf" || s == "+inf" || s == "Infinity" || s == "+Infinity")
	{
		out = std::numeric_limits<double>::infinity();
		return true;
	}
	if (s == "-inf" || s == "-Infinity")
	{
		out = -std::numeric_limits<double>::infinity();
		return true;
	}
	return false;
}

/// f32 bits from a number (strtof: one rounding from the decimal), "0x........" raw bits, nan / inf / -inf
bool ToF32Bits(const JValue& v, uint32_t& bits, std::string& error)
{
	if (v.type == JValue::String && IsHexBits(v.text))
	{
		uint64_t u = 0;
		if (!ParseUnsignedText(v.text, u) || u > 0xFFFFFFFFull)
		{
			error = "bad f32 bits " + v.text;
			return false;
		}
		bits = static_cast<uint32_t>(u);
		return true;
	}
	float f = 0.0f;
	double special = 0.0;
	if (v.type == JValue::String && SpecialFloat(v.text, special))
	{
		f = static_cast<float>(special);
	}
	else if (v.type == JValue::Number || v.type == JValue::String)
	{
		char* end = nullptr;
		f = strtof(v.text.c_str(), &end);
		if (!end || *end != '\0')
		{
			error = "bad f32 value " + v.text;
			return false;
		}
	}
	else
	{
		error = "expected an f32 value";
		return false;
	}
	memcpy(&bits, &f, 4);
	return true;
}

bool ToF64Bits(const JValue& v, uint64_t& bits, std::string& error)
{
	if (v.type == JValue::String && IsHexBits(v.text))
	{
		if (!ParseUnsignedText(v.text, bits))
		{
			error = "bad f64 bits " + v.text;
			return false;
		}
		return true;
	}
	double d = 0.0;
	if (v.type == JValue::String && SpecialFloat(v.text, d))
	{
	}
	else if (v.type == JValue::Number || v.type == JValue::String)
	{
		char* end = nullptr;
		d = strtod(v.text.c_str(), &end);
		if (!end || *end != '\0')
		{
			error = "bad f64 value " + v.text;
			return false;
		}
	}
	else
	{
		error = "expected an f64 value";
		return false;
	}
	memcpy(&bits, &d, 8);
	return true;
}

bool HexToBytes(const std::string& hex, std::vector<uint8_t>& out, std::string& error)
{
	std::string h;
	for (const char c : hex)
	{
		if (c != ' ' && c != '_')
		{
			h += c;
		}
	}
	if (h.size() % 2)
	{
		error = "odd number of hex digits";
		return false;
	}
	out.clear();
	for (size_t i = 0; i < h.size(); i += 2)
	{
		int v = 0;
		for (int k = 0; k < 2; ++k)
		{
			const char c = h[i + k];
			v <<= 4;
			if (c >= '0' && c <= '9')
			{
				v |= c - '0';
			}
			else if (c >= 'a' && c <= 'f')
			{
				v |= c - 'a' + 10;
			}
			else if (c >= 'A' && c <= 'F')
			{
				v |= c - 'A' + 10;
			}
			else
			{
				error = "bad hex digit";
				return false;
			}
		}
		out.push_back(static_cast<uint8_t>(v));
	}
	return true;
}

/// The value of an argument object: "bits" wins over "v"
const JValue* ArgValue(const JValue& arg)
{
	if (const JValue* b = arg.Find("bits"))
	{
		return b;
	}
	return arg.Find("v");
}

// ---------------------------------------------------------------------------------------------------------------
// Requests

enum class Conv
{
	Cdecl,
	Stdcall,
	Thiscall,
	Fastcall
};

struct Response
{
	std::string body; // the members after {"ok":..., without the braces
	bool ok = true;
	std::string error;
	std::vector<std::string> warnings;

	void Add(const char* key, const std::string& rawJson)
	{
		body += ",\"";
		body += key;
		body += "\":";
		body += rawJson;
	}
	void AddString(const char* key, const std::string& s)
	{
		std::string v;
		AppendEscaped(v, s);
		Add(key, v);
	}
	std::string Finish(const JValue* id) const
	{
		std::string out = ok ? "{\"ok\":true" : "{\"ok\":false";
		if (id)
		{
			out += ",\"id\":";
			if (id->type == JValue::String)
			{
				AppendEscaped(out, id->text);
			}
			else if (id->type == JValue::Number)
			{
				out += id->text;
			}
			else
			{
				out += "null";
			}
		}
		if (!ok)
		{
			out += ",\"error\":";
			AppendEscaped(out, error);
		}
		out += body;
		if (!warnings.empty())
		{
			out += ",\"warn\":[";
			for (size_t i = 0; i < warnings.size(); ++i)
			{
				if (i)
				{
					out += ',';
				}
				AppendEscaped(out, warnings[i]);
			}
			out += ']';
		}
		out += '}';
		return out;
	}
};

bool Fail(Response& r, const std::string& message)
{
	r.ok = false;
	r.error = message;
	return false;
}

std::string ImportName(uint32_t index)
{
	if (index < g_imports.size())
	{
		return g_imports[index].dll + "!" + g_imports[index].name;
	}
	return "?";
}

bool AllocBuffers(const JValue& req, Response& r)
{
	g_buffers.clear();
	g_arenaUsed = 0;
	const JValue* alloc = req.Find("alloc");
	if (!alloc)
	{
		return true;
	}
	if (alloc->type != JValue::Array)
	{
		return Fail(r, "alloc must be an array");
	}
	for (const auto& a : alloc->items)
	{
		const JValue* name = a.Find("name");
		const JValue* size = a.Find("size");
		if (!name || name->type != JValue::String || name->text.empty())
		{
			return Fail(r, "alloc entries need a name");
		}
		std::vector<uint8_t> init;
		std::string err;
		if (const JValue* hex = a.Find("hex"))
		{
			if (hex->type != JValue::String || !HexToBytes(hex->text, init, err))
			{
				return Fail(r, "alloc " + name->text + ": " + (err.empty() ? "hex must be a string" : err));
			}
		}
		uint32_t bytes = static_cast<uint32_t>(init.size());
		if (size && !ToU32(*size, bytes, err))
		{
			return Fail(r, "alloc " + name->text + ": " + err);
		}
		if (bytes < init.size())
		{
			return Fail(r, "alloc " + name->text + ": hex is longer than size");
		}
		uint32_t fill = 0;
		if (const JValue* f = a.Find("fill"))
		{
			if (!ToU32(*f, fill, err) || fill > 0xFF)
			{
				return Fail(r, "alloc " + name->text + ": fill must be a byte");
			}
		}
		uint32_t align = 16;
		if (const JValue* al = a.Find("align"))
		{
			if (!ToU32(*al, align, err) || align == 0 || (align & (align - 1)))
			{
				return Fail(r, "alloc " + name->text + ": align must be a power of two");
			}
		}
		const uint32_t start = (g_arenaUsed + align - 1) & ~(align - 1);
		if (static_cast<uint64_t>(start) + bytes + k_GuardBytes > k_ArenaSize)
		{
			return Fail(r, "scratch arena exhausted");
		}
		uint8_t* p = g_arena + start;
		memset(p, static_cast<int>(fill), bytes);
		if (!init.empty())
		{
			memcpy(p, init.data(), init.size());
		}
		memset(p + bytes, k_GuardFill, k_GuardBytes);
		g_arenaUsed = start + bytes + k_GuardBytes;
		Buffer b;
		b.name = name->text;
		b.addr = ArenaAddr() + start;
		b.size = bytes;
		g_buffers.push_back(b);
	}
	return true;
}

bool ApplyWrites(const JValue& req, Response& r)
{
	const JValue* writes = req.Find("write");
	if (!writes)
	{
		return true;
	}
	if (writes->type != JValue::Array)
	{
		return Fail(r, "write must be an array");
	}
	for (const auto& w : writes->items)
	{
		const JValue* addr = w.Find("addr");
		const JValue* hex = w.Find("hex");
		std::string err;
		uint32_t a = 0;
		std::vector<uint8_t> bytes;
		if (!addr || !ToU32(*addr, a, err))
		{
			return Fail(r, "write: " + (err.empty() ? std::string("missing addr") : err));
		}
		if (!hex || hex->type != JValue::String || !HexToBytes(hex->text, bytes, err))
		{
			return Fail(r, "write: " + (err.empty() ? std::string("missing hex") : err));
		}
		if (!bytes.empty() && !SafeCopy(reinterpret_cast<void*>(static_cast<uintptr_t>(a)), bytes.data(), bytes.size()))
		{
			return Fail(r, "write: address " + Hex(a, 8) + " is not writable");
		}
		if (a < g_imageBase + g_imageSize && a + bytes.size() > g_imageBase)
		{
			FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(static_cast<uintptr_t>(a)), bytes.size());
		}
	}
	return true;
}

bool DoReads(const JValue& req, Response& r)
{
	const JValue* reads = req.Find("read");
	if (!reads)
	{
		return true;
	}
	if (reads->type != JValue::Array)
	{
		return Fail(r, "read must be an array");
	}
	std::string out = "[";
	for (size_t i = 0; i < reads->items.size(); ++i)
	{
		const JValue& rd = reads->items[i];
		const JValue* addr = rd.Find("addr");
		const JValue* len = rd.Find("len");
		std::string err;
		uint32_t a = 0;
		uint32_t n = 0;
		if (!addr || !ToU32(*addr, a, err) || !len || !ToU32(*len, n, err))
		{
			return Fail(r, "read: " + (err.empty() ? std::string("needs addr and len") : err));
		}
		if (n > (16u << 20))
		{
			return Fail(r, "read: len too large");
		}
		std::vector<uint8_t> bytes(n);
		if (n && !SafeCopy(bytes.data(), reinterpret_cast<const void*>(static_cast<uintptr_t>(a)), n))
		{
			return Fail(r, "read: address " + Hex(a, 8) + " is not readable");
		}
		if (i)
		{
			out += ',';
		}
		out += '"';
		out += HexBytes(bytes.data(), bytes.size());
		out += '"';
	}
	out += ']';
	r.Add("read", out);
	return true;
}

void CheckGuards(Response& r)
{
	for (const auto& b : g_buffers)
	{
		const uint8_t* g = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(b.addr)) + b.size;
		for (uint32_t i = 0; i < k_GuardBytes; ++i)
		{
			if (g[i] != k_GuardFill)
			{
				r.warnings.push_back("the call wrote past the end of @" + b.name);
				break;
			}
		}
	}
}

bool IntClass(const std::string& t)
{
	return t == "i32" || t == "u32" || t == "ptr" || t == "i16" || t == "u16" || t == "i8" || t == "u8" || t == "bool";
}

bool DoCall(const JValue& req, Response& r)
{
	std::string err;
	uint32_t target = 0;
	if (!ToU32(*req.Find("addr"), target, err))
	{
		return Fail(r, "addr: " + err);
	}
	if (target < g_imageBase || target >= g_imageBase + g_imageSize)
	{
		// allowed (e.g. code placed in a buffer), but almost always a mistake
		r.warnings.push_back("addr " + Hex(target, 8) + " is outside the exe image");
	}
	Conv conv = Conv::Cdecl;
	std::string convName = "cdecl";
	if (const JValue* c = req.Find("conv"))
	{
		convName = c->text;
		if (convName == "cdecl")
		{
			conv = Conv::Cdecl;
		}
		else if (convName == "stdcall")
		{
			conv = Conv::Stdcall;
		}
		else if (convName == "thiscall")
		{
			conv = Conv::Thiscall;
		}
		else if (convName == "fastcall")
		{
			conv = Conv::Fastcall;
		}
		else
		{
			return Fail(r, "conv must be cdecl, stdcall, thiscall or fastcall");
		}
	}
	std::string retType = "void";
	if (const JValue* rt = req.Find("ret"))
	{
		retType = rt->text;
	}
	static const char* const k_RetTypes[] = {"void", "i32", "u32", "i16", "u16", "i8",  "u8",
	                                         "bool", "ptr", "i64", "u64", "f32", "f64", "st0"};
	bool known = false;
	for (const char* t : k_RetTypes)
	{
		known = known || retType == t;
	}
	if (!known)
	{
		return Fail(r, "unknown ret type " + retType);
	}

	g_regEax = g_regEbx = g_regEcx = g_regEdx = g_regEsi = g_regEdi = 0;
	int regsUsed = 0; // fastcall: ECX, then EDX
	if (const JValue* t = req.Find("this"))
	{
		if (conv != Conv::Thiscall && conv != Conv::Fastcall)
		{
			return Fail(r, "\"this\" needs conv thiscall or fastcall");
		}
		if (!ToU32(*t, g_regEcx, err))
		{
			return Fail(r, "this: " + err);
		}
		regsUsed = 1;
	}
	else if (conv == Conv::Thiscall)
	{
		return Fail(r, "thiscall needs \"this\"");
	}

	std::vector<uint32_t> stack;
	if (const JValue* args = req.Find("args"))
	{
		if (args->type != JValue::Array)
		{
			return Fail(r, "args must be an array");
		}
		for (size_t i = 0; i < args->items.size(); ++i)
		{
			const JValue& a = args->items[i];
			const JValue* type = a.Find("type");
			const JValue* value = ArgValue(a);
			const std::string where = "args[" + std::to_string(i) + "]: ";
			if (!type || type->type != JValue::String || !value)
			{
				return Fail(r, where + "needs type and v (or bits)");
			}
			const std::string& t = type->text;
			if (IntClass(t))
			{
				uint32_t v = 0;
				if (!ToU32(*value, v, err))
				{
					return Fail(r, where + err);
				}
				if (t == "i16")
				{
					v = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(v)));
				}
				else if (t == "u16")
				{
					v &= 0xFFFF;
				}
				else if (t == "i8")
				{
					v = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(v)));
				}
				else if (t == "u8" || t == "bool")
				{
					v &= 0xFF;
				}
				if (conv == Conv::Fastcall && regsUsed < 2)
				{
					(regsUsed == 0 ? g_regEcx : g_regEdx) = v;
					++regsUsed;
				}
				else
				{
					stack.push_back(v);
				}
			}
			else if (t == "f32")
			{
				uint32_t bits = 0;
				if (!ToF32Bits(*value, bits, err))
				{
					return Fail(r, where + err);
				}
				stack.push_back(bits);
			}
			else if (t == "f64" || t == "i64" || t == "u64")
			{
				uint64_t bits = 0;
				if (t == "f64")
				{
					if (!ToF64Bits(*value, bits, err))
					{
						return Fail(r, where + err);
					}
				}
				else
				{
					int64_t i64 = 0;
					if (!ToInteger(*value, i64, err))
					{
						return Fail(r, where + err);
					}
					bits = static_cast<uint64_t>(i64);
				}
				stack.push_back(static_cast<uint32_t>(bits));
				stack.push_back(static_cast<uint32_t>(bits >> 32));
			}
			else
			{
				return Fail(r, where + "unknown type " + t);
			}
		}
	}
	if (stack.size() > k_MaxStackDwords)
	{
		return Fail(r, "too many stack arguments");
	}

	if (const JValue* regs = req.Find("regs"))
	{
		struct
		{
			const char* name;
			uint32_t* reg;
		} const k_Regs[] = {{"eax", &g_regEax}, {"ebx", &g_regEbx}, {"ecx", &g_regEcx},
		                    {"edx", &g_regEdx}, {"esi", &g_regEsi}, {"edi", &g_regEdi}};
		for (const auto& m : regs->members)
		{
			bool found = false;
			for (const auto& kr : k_Regs)
			{
				if (m.first == kr.name)
				{
					if (!ToU32(m.second, *kr.reg, err))
					{
						return Fail(r, "regs." + m.first + ": " + err);
					}
					found = true;
				}
			}
			if (!found)
			{
				return Fail(r, "regs: unknown register " + m.first + " (eax ebx ecx edx esi edi)");
			}
		}
	}

	g_fpuInCount = 0;
	if (const JValue* push = req.Find("fpu_push"))
	{
		if (push->type != JValue::Array || push->items.size() > 8)
		{
			return Fail(r, "fpu_push must be an array of at most 8 values");
		}
		for (const auto& p : push->items)
		{
			const JValue* type = p.Find("type");
			const JValue* value = ArgValue(p);
			uint8_t* slot = g_fpuIn + 10 * g_fpuInCount;
			if (!type || !value)
			{
				return Fail(r, "fpu_push entries need type and v (or bits)");
			}
			if (type->text == "f80")
			{
				std::vector<uint8_t> bytes;
				if (value->type != JValue::String || !IsHexBits(value->text) || value->text.size() != 22 ||
				    !HexToBytes(value->text.substr(2), bytes, err))
				{
					return Fail(r, "fpu_push f80 needs \"0x\" + 20 hex digits");
				}
				for (int i = 0; i < 10; ++i)
				{
					slot[i] = bytes[9 - i];
				}
			}
			else if (type->text == "f32" || type->text == "f64")
			{
				uint64_t bits64 = 0;
				if (type->text == "f32")
				{
					uint32_t bits = 0;
					if (!ToF32Bits(*value, bits, err))
					{
						return Fail(r, "fpu_push: " + err);
					}
					float f = 0.0f;
					memcpy(&f, &bits, 4);
					const double d = f; // exact
					memcpy(&bits64, &d, 8);
				}
				else if (!ToF64Bits(*value, bits64, err))
				{
					return Fail(r, "fpu_push: " + err);
				}
				g_ldF64 = bits64;
				LoadF64To80(slot);
			}
			else
			{
				return Fail(r, "fpu_push type must be f32, f64 or f80");
			}
			++g_fpuInCount;
		}
	}

	uint32_t cw = k_GameFpuCw;
	if (const JValue* c = req.Find("fpu_cw"))
	{
		if (!ToU32(*c, cw, err) || cw > 0xFFFF)
		{
			return Fail(r, "fpu_cw must be a 16-bit value");
		}
	}
	g_gameCw = static_cast<uint16_t>(cw);
	uint32_t timeout = k_DefaultTimeoutMs;
	if (const JValue* t = req.Find("timeout_ms"))
	{
		if (!ToU32(*t, timeout, err))
		{
			return Fail(r, "timeout_ms: " + err);
		}
	}

	g_nargs = static_cast<uint32_t>(stack.size());
	for (size_t i = 0; i < stack.size(); ++i)
	{
		g_args[i] = stack[i];
	}
	g_target = target;
	g_status = 0;
	g_trapIndex = g_trapFrom = g_excCode = g_excAddr = g_excInfo0 = g_excInfo1 = g_excParams = 0;
	memset(g_fpuState, 0, sizeof(g_fpuState));
	g_deadline = GetTickCount() + timeout;
	CallThunk();
	g_gameCw = k_GameFpuCw;

	// the FPU after the call
	uint16_t sw = 0;
	uint16_t tw = 0;
	memcpy(&sw, g_fpuState + 4, 2);
	memcpy(&tw, g_fpuState + 8, 2);
	const int top = (sw >> 11) & 7;
	int depth = 0;
	for (int i = 0; i < 8; ++i)
	{
		if (((tw >> (2 * ((top + i) & 7))) & 3) == 3)
		{
			break;
		}
		++depth;
	}

	if (g_status == 2)
	{
		std::string trap = "{\"import\":";
		AppendEscaped(trap, ImportName(g_trapIndex));
		trap += ",\"iat\":\"" + Hex(g_trapIndex < g_imports.size() ? g_imports[g_trapIndex].iat : 0, 8) + "\",\"from\":\"" +
		        Hex(g_trapFrom, 8) + "\"}";
		r.Add("trap", trap);
		return Fail(r, "import trap: " + ImportName(g_trapIndex) + " called from " + Hex(g_trapFrom, 8));
	}
	if (g_status == 3)
	{
		std::string what = "exception " + Hex(g_excCode, 8);
		std::string extra;
		if (g_excCode == EXCEPTION_ACCESS_VIOLATION && g_excParams >= 2)
		{
			const char* access = g_excInfo0 == 0 ? "read" : g_excInfo0 == 1 ? "write" : "execute";
			what = std::string("access violation (") + access + " " + Hex(g_excInfo1, 8) + ")";
			extra = std::string(",\"access\":\"") + access + "\",\"target\":\"" + Hex(g_excInfo1, 8) + "\"";
		}
		else if (g_excCode == EXCEPTION_INT_DIVIDE_BY_ZERO)
		{
			what = "integer divide by zero";
		}
		else if (g_excCode == EXCEPTION_INT_OVERFLOW)
		{
			what = "integer overflow (idiv)";
		}
		else if (g_excCode == EXCEPTION_BREAKPOINT)
		{
			what = "breakpoint (int3)";
		}
		else if (g_excCode == EXCEPTION_ILLEGAL_INSTRUCTION)
		{
			what = "illegal instruction";
		}
		else if (g_excCode == EXCEPTION_STACK_OVERFLOW)
		{
			what = "stack overflow";
			_resetstkoflw();
		}
		r.Add("exception", "{\"code\":\"" + Hex(g_excCode, 8) + "\",\"addr\":\"" + Hex(g_excAddr, 8) + "\"" + extra + "}");
		return Fail(r, what + " at " + Hex(g_excAddr, 8));
	}
	if (g_status == 4)
	{
		r.Add("timeout", "{\"eip\":\"" + Hex(g_excAddr, 8) + "\"}");
		return Fail(r, "timeout after " + std::to_string(timeout) + " ms (eip " + Hex(g_excAddr, 8) + ")");
	}
	if (g_status != 1)
	{
		return Fail(r, "the call did not return (internal error)");
	}

	// what the callee popped: cdecl 0, the others all their stack arguments
	const auto popped = static_cast<int32_t>(g_espAfter - (g_savedEsp - 4 * g_nargs));
	const int32_t expected = conv == Conv::Cdecl ? 0 : static_cast<int32_t>(4 * g_nargs);
	if (popped != expected)
	{
		r.warnings.push_back("callee popped " + std::to_string(popped) + " bytes, " + convName + " with " +
		                     std::to_string(g_nargs) + " stack dwords expects " + std::to_string(expected));
	}
	r.Add("callee_pop", std::to_string(popped));
	r.Add("eax", "\"" + Hex(g_retEax, 8) + "\"");
	r.Add("edx", "\"" + Hex(g_retEdx, 8) + "\"");
	r.Add("fpu_sw", "\"" + Hex(sw, 4) + "\"");
	r.Add("fpu_depth", std::to_string(depth));

	const bool floatRet = retType == "f32" || retType == "f64" || retType == "st0";
	if (depth > 0)
	{
		r.Add("st0_hex", "\"" + Hex80(g_fpuState + 28) + "\"");
	}
	if (floatRet)
	{
		if (depth == 0)
		{
			return Fail(r, "ret " + retType + " but the FPU stack is empty after the call");
		}
		if (depth > 1)
		{
			r.warnings.push_back("the FPU stack holds " + std::to_string(depth) + " values after the call");
		}
		memcpy(g_cvtIn, g_fpuState + 28, 10);
		ConvertExtended();
		float f = 0.0f;
		double d = 0.0;
		memcpy(&f, &g_cvtF32, 4);
		memcpy(&d, &g_cvtF64, 8);
		if (retType == "f32")
		{
			r.Add("ret", JsonDouble(f, 9));
			r.Add("ret_hex", "\"" + Hex(g_cvtF32, 8) + "\"");
		}
		else if (retType == "f64")
		{
			r.Add("ret", JsonDouble(d, 17));
			r.Add("ret_hex", "\"" + Hex(g_cvtF64, 16) + "\"");
		}
		else
		{
			r.Add("ret", JsonDouble(d, 17));
			r.Add("ret_hex", "\"" + Hex80(g_fpuState + 28) + "\"");
			r.Add("ret_f32_hex", "\"" + Hex(g_cvtF32, 8) + "\"");
			r.Add("ret_f64_hex", "\"" + Hex(g_cvtF64, 16) + "\"");
		}
	}
	else
	{
		if (depth > 0)
		{
			r.warnings.push_back("the FPU stack holds " + std::to_string(depth) + " values after the call");
		}
		const uint64_t pair = (static_cast<uint64_t>(g_retEdx) << 32) | g_retEax;
		if (retType == "i32")
		{
			r.Add("ret", std::to_string(static_cast<int32_t>(g_retEax)));
			r.Add("ret_hex", "\"" + Hex(g_retEax, 8) + "\"");
		}
		else if (retType == "u32" || retType == "ptr")
		{
			r.Add("ret", std::to_string(g_retEax));
			r.Add("ret_hex", "\"" + Hex(g_retEax, 8) + "\"");
		}
		else if (retType == "i16")
		{
			r.Add("ret", std::to_string(static_cast<int16_t>(g_retEax)));
			r.Add("ret_hex", "\"" + Hex(g_retEax & 0xFFFF, 4) + "\"");
		}
		else if (retType == "u16")
		{
			r.Add("ret", std::to_string(g_retEax & 0xFFFF));
			r.Add("ret_hex", "\"" + Hex(g_retEax & 0xFFFF, 4) + "\"");
		}
		else if (retType == "i8")
		{
			r.Add("ret", std::to_string(static_cast<int8_t>(g_retEax)));
			r.Add("ret_hex", "\"" + Hex(g_retEax & 0xFF, 2) + "\"");
		}
		else if (retType == "u8" || retType == "bool")
		{
			r.Add("ret", std::to_string(g_retEax & 0xFF));
			r.Add("ret_hex", "\"" + Hex(g_retEax & 0xFF, 2) + "\"");
		}
		else if (retType == "i64")
		{
			r.Add("ret", std::to_string(static_cast<int64_t>(pair)));
			r.Add("ret_hex", "\"" + Hex(pair, 16) + "\"");
		}
		else if (retType == "u64")
		{
			r.Add("ret", std::to_string(pair));
			r.Add("ret_hex", "\"" + Hex(pair, 16) + "\"");
		}
	}
	CheckGuards(r);
	return true;
}

std::string ImportsJson()
{
	std::string out = "[";
	for (size_t i = 0; i < g_imports.size(); ++i)
	{
		if (i)
		{
			out += ',';
		}
		out += "{\"dll\":";
		AppendEscaped(out, g_imports[i].dll);
		out += ",\"name\":";
		AppendEscaped(out, g_imports[i].name);
		out += ",\"iat\":\"" + Hex(g_imports[i].iat, 8) + "\"}";
	}
	out += ']';
	return out;
}

std::string Process(const JValue& req, bool& quit);

std::string ProcessBatch(const JValue& req, bool& quit)
{
	const JValue* batch = req.Find("batch");
	if (batch->type != JValue::Array)
	{
		return "{\"ok\":false,\"error\":\"batch must be an array\"}";
	}
	std::string out = "{\"ok\":true,\"batch\":[";
	for (size_t i = 0; i < batch->items.size(); ++i)
	{
		if (i)
		{
			out += ',';
		}
		out += Process(batch->items[i], quit);
	}
	out += "]}";
	return out;
}

std::string Process(const JValue& req, bool& quit)
{
	Response r;
	const JValue* id = req.Find("id");
	if (req.type != JValue::Object)
	{
		r.ok = false;
		r.error = "a request is a JSON object";
		return r.Finish(nullptr);
	}
	if (req.Find("batch"))
	{
		return ProcessBatch(req, quit);
	}
	if (const JValue* cmd = req.Find("cmd"))
	{
		if (cmd->text == "quit")
		{
			quit = true;
		}
		else if (cmd->text == "imports")
		{
			r.Add("imports", ImportsJson());
		}
		else if (cmd->text == "state")
		{
			r.Add("game_mxcsr", "\"" + Hex(g_gameMxcsr, 8) + "\"");
			r.Add("snapshot", g_hasSnapshot ? "true" : "false");
		}
		else if (cmd->text != "ping")
		{
			r.ok = false;
			r.error = "unknown cmd " + cmd->text + " (ping, state, imports, quit)";
		}
		return r.Finish(id);
	}
	if (const JValue* reset = req.Find("reset"))
	{
		if (reset->type == JValue::String && reset->text == "pristine")
		{
			ResetImage(true);
		}
		else if (reset->type == JValue::Bool && reset->boolean)
		{
			ResetImage(false);
		}
		else if (!(reset->type == JValue::Bool && !reset->boolean))
		{
			r.ok = false;
			r.error = "reset must be true or \"pristine\"";
			return r.Finish(id);
		}
	}
	if (!AllocBuffers(req, r) || !ApplyWrites(req, r))
	{
		return r.Finish(id);
	}
	if (!g_buffers.empty())
	{
		std::string b = "{";
		for (size_t i = 0; i < g_buffers.size(); ++i)
		{
			if (i)
			{
				b += ',';
			}
			AppendEscaped(b, g_buffers[i].name);
			b += ":\"" + Hex(g_buffers[i].addr, 8) + "\"";
		}
		b += '}';
		r.Add("buffers", b);
	}
	if (req.Find("addr") && !DoCall(req, r))
	{
		return r.Finish(id);
	}
	if (!DoReads(req, r))
	{
		return r.Finish(id);
	}
	if (const JValue* snap = req.Find("snapshot"))
	{
		if (snap->type == JValue::Bool && snap->boolean)
		{
			TakeSnapshot();
		}
	}
	return r.Finish(id);
}

bool ReadLine(std::string& line)
{
	line.clear();
	char chunk[65536];
	while (fgets(chunk, sizeof(chunk), stdin))
	{
		line += chunk;
		if (!line.empty() && line.back() == '\n')
		{
			while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
			{
				line.pop_back();
			}
			return true;
		}
	}
	return !line.empty();
}

void WriteLine(const std::string& s)
{
	fwrite(s.data(), 1, s.size(), stdout);
	fputc('\n', stdout);
	fflush(stdout);
}

/// ImageBase and SizeOfImage from the file's headers (for the parent's reservation)
bool PeekImageRange(const std::string& path, uint32_t& base, uint32_t& size)
{
	FILE* f = fopen(path.c_str(), "rb");
	if (!f)
	{
		return false;
	}
	uint8_t head[4096] = {};
	const size_t n = fread(head, 1, sizeof(head), f);
	fclose(f);
	const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(head);
	if (n < sizeof(IMAGE_DOS_HEADER) || dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 ||
	    static_cast<size_t>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS32) > n)
	{
		return false;
	}
	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(head + dos->e_lfanew);
	base = nt->OptionalHeader.ImageBase;
	size = nt->OptionalHeader.SizeOfImage;
	return true;
}

/// The low address space of a running process is crowded before main (NLS mappings, heaps, injected DLLs), so the
/// exe's range is reserved in a suspended copy of this host before its loader runs; the copy commits it (LoadExe).
/// The copy shares stdin / stdout and dies with this process (a kill-on-close job). Returns the copy's exit code.
int RelaunchReserved(int argc, char** argv, uint32_t base, uint32_t size, std::string& error)
{
	char self[MAX_PATH];
	if (!GetModuleFileNameA(nullptr, self, MAX_PATH))
	{
		error = "GetModuleFileName failed";
		return -1;
	}
	std::string cmd = "\"" + std::string(self) + "\" --reserved";
	for (int i = 1; i < argc; ++i)
	{
		cmd += " \"" + std::string(argv[i]) + "\"";
	}
	const HANDLE handles[] = {GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), GetStdHandle(STD_ERROR_HANDLE)};
	for (HANDLE h : handles)
	{
		if (h && h != INVALID_HANDLE_VALUE)
		{
			SetHandleInformation(h, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
		}
	}
	STARTUPINFOA si {};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = handles[0];
	si.hStdOutput = handles[1];
	si.hStdError = handles[2];
	PROCESS_INFORMATION pi {};
	std::vector<char> line(cmd.begin(), cmd.end());
	line.push_back('\0');
	if (!CreateProcessA(self, line.data(), nullptr, nullptr, TRUE, CREATE_SUSPENDED, nullptr, nullptr, &si, &pi))
	{
		error = "CreateProcess of the reserved copy failed (" + std::to_string(GetLastError()) + ")";
		return -1;
	}
	HANDLE job = CreateJobObjectA(nullptr, nullptr);
	if (job)
	{
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits {};
		limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
		AssignProcessToJobObject(job, pi.hProcess); // best effort: without it the copy outlives a killed parent
	}
	void* got =
	    VirtualAllocEx(pi.hProcess, reinterpret_cast<void*>(static_cast<uintptr_t>(base)), size, MEM_RESERVE, PAGE_NOACCESS);
	if (got != reinterpret_cast<void*>(static_cast<uintptr_t>(base)))
	{
		error = "VirtualAllocEx(0x" + Hex(base, 8).substr(2) + ") in the suspended copy failed (" +
		        std::to_string(GetLastError()) + ")";
		TerminateProcess(pi.hProcess, 3);
		return -1;
	}
	ResumeThread(pi.hThread);
	WaitForSingleObject(pi.hProcess, INFINITE);
	DWORD code = 0;
	GetExitCodeProcess(pi.hProcess, &code);
	return static_cast<int>(code);
}

DWORD WINAPI RequestLoop(LPVOID)
{
	std::string line;
	bool quit = false;
	while (!quit && ReadLine(line))
	{
		if (line.empty())
		{
			continue;
		}
		JValue req;
		std::string parseError;
		JParser parser(line.data(), line.data() + line.size());
		if (!parser.Parse(req, parseError))
		{
			std::string msg;
			AppendEscaped(msg, "bad JSON: " + parseError);
			WriteLine("{\"ok\":false,\"error\":" + msg + "}");
			continue;
		}
		WriteLine(Process(req, quit));
	}
	return 0;
}

} // namespace

int main(int argc, char** argv)
{
	_setmode(_fileno(stdin), _O_BINARY);
	_setmode(_fileno(stdout), _O_BINARY);
	std::string exe;
	if (const char* env = getenv("RUNBLACK_EXE"))
	{
		exe = env;
	}
	bool reserved = false;
	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "--exe") == 0 && i + 1 < argc)
		{
			exe = argv[++i];
		}
		else if (strcmp(argv[i], "--reserved") == 0)
		{
			reserved = true;
		}
		else
		{
			std::string msg;
			AppendEscaped(msg, std::string("unknown argument ") + argv[i] + " (usage: oracle_host [--exe path])");
			WriteLine("{\"ok\":false,\"ready\":false,\"error\":" + msg + "}");
			return 2;
		}
	}
	if (exe.empty())
	{
		WriteLine("{\"ok\":false,\"ready\":false,\"error\":\"no exe: pass --exe <path of runblack.exe> or set "
		          "RUNBLACK_EXE\"}");
		return 2;
	}
	g_exePath = exe;
	std::string error;
	// the exe at its ImageBase first, then the arena at its preferred address (or anywhere)
	if (!LoadExe(exe, reserved, error))
	{
		uint32_t base = 0;
		uint32_t size = 0;
		if (!reserved && g_loadError == ERROR_INVALID_ADDRESS && PeekImageRange(exe, base, size))
		{
			std::string relaunchError;
			const int code = RelaunchReserved(argc, argv, base, size, relaunchError);
			if (code >= 0 || relaunchError.empty())
			{
				return code;
			}
			error += "; relaunch: " + relaunchError;
		}
		std::string msg;
		AppendEscaped(msg, error);
		WriteLine("{\"ok\":false,\"ready\":false,\"error\":" + msg + "}");
		return 2;
	}
	if (!InitArena(error))
	{
		std::string msg;
		AppendEscaped(msg, error);
		WriteLine("{\"ok\":false,\"ready\":false,\"error\":" + msg + "}");
		return 2;
	}
	AddVectoredExceptionHandler(1, VectoredHandler);

	std::string ready = "{\"ok\":true,\"ready\":true,\"host\":\"";
	ready += k_HostVersion;
	ready += "\",\"exe\":";
	AppendEscaped(ready, exe);
	ready += ",\"image_base\":\"" + Hex(g_imageBase, 8) + "\",\"image_size\":\"" + Hex(g_imageSize, 8) + "\",\"entry\":\"" +
	         Hex(g_entryPoint, 8) + "\",\"imports\":" + std::to_string(g_imports.size()) + ",\"arena\":\"" +
	         Hex(ArenaAddr(), 8) + "\",\"fpu_cw\":\"" + Hex(k_GameFpuCw, 4) + "\"}";
	WriteLine(ready);

	// The calls run on a worker created now, so that its big stack cannot take the exe's range (the main thread's
	// stack is placed before main runs: keep the PE default there)
	DWORD workerId = 0;
	HANDLE worker = CreateThread(nullptr, k_WorkerStack, RequestLoop, nullptr,
	                             CREATE_SUSPENDED | STACK_SIZE_PARAM_IS_A_RESERVATION, &workerId);
	if (!worker)
	{
		WriteLine("{\"ok\":false,\"error\":\"cannot create the worker thread\"}");
		return 2;
	}
	g_mainThread = worker;
	g_mainThreadId = workerId;
	CreateThread(nullptr, 0, Watchdog, nullptr, 0, nullptr);
	ResumeThread(worker);
	WaitForSingleObject(worker, INFINITE);
	return 0;
}
