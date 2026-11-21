# oracle: daily guide

Ask the original `runblack.exe` what exact number it gives. The host maps the exe and runs its functions natively
(no emulation), at the game's FPU settings. [README.md](README.md) has the install, the protocol, the spec format
and the internals.

**Use it when** a port must match the original bit for bit and the answer is a number: a formula, an angle, a
rounding, a random draw, a table the game computes, a constant. Find the function first with
`python oracle.py sym <regex>` (bw1-decomp's `symbols.txt`), read its C in bw1-decomp, then
`python disasm_exe.py <addr|symbol>:<len hex>` to see its callees and globals. Run everything from this folder. A
start-up costs about half a second.

## Five recipes

**1. One call.** The convention and the return type come from the decorated name (`--conv`, `--ret` override them).
`*f32:1,0,2` passes a pointer to a new buffer (a point; a map position is `*i32:x,z,0`), `*out:N` a pointer to N zero
bytes; both are printed back after the call. `f32:0x3F800000` passes raw bits.

```
python oracle.py quick GetHeadingFromPoints *f32:1,0,2 *f32:0,0,0
python oracle.py quick NormaliseAngle f32:7
python oracle.py quick _LHRand__FlRU --ret u32 u32:100 *u32:0x88F89F     (no type information: say --ret)
```
Float returns print the f32 a caller's `fstp dword` stores, the f64 and the 80-bit value.

**2. A global or a constant** (after the start-up baseline; types i8..u64, f32, f64, `:count`):
```
python oracle.py peek <addr|symbol> f32
python oracle.py peek <addr|symbol> f64:2
```

**3. Globals in, globals out.** `--set` writes before the call, `--get` reads after it (same request):
```
python oracle.py quick NormaliseAngle f32:7 --set <addr>=i32:1,2 --get <addr>:i32:2
```
In Python: `o.call(fn, conv, args, ret, set_globals=[(where, 'u32', v)], get_globals=[(where, 'f32', 3)])` and
`r['globals']`; `o.poke(where, t, v)` / `o.peek(where, t, n)` persist until `o.reset()`. `where`: `0x..`, a symbol,
or either `+0xOFF`.

**4. Stub a callee** that would touch the renderer, files, the OS or heap objects: calls to it return a chosen value
and pop `/N` argument bytes (stdcall / thiscall callees pop theirs). A function is patched at its entry (a jmp); an
import (`DLL!Name`) through its import slot. Each stub counts its calls.
```
python oracle.py quick IsTalkingWithDelay --this *out:14320 --stub <callee>=i32:0 --stub KERNEL32.dll!GetTickCount=u32:150
```
Python: `s = o.stub(target, 'f32', 1.25, pop=0)`, `o.stub_calls(s)`. Stubs live until `o.reset()`; `o.snapshot()`
keeps them (gen_vectors does so for a spec's `stubs`).

**4b. Capture a callee's arguments.** Add `@` and one item per stack argument to a stub: a 4-byte type (`u32`, `i32`,
`f32`), or `pK:type` for a pointer whose K dwords are copied as the call is made (a point passed by reference is
`p3:f32`). `@N` records N plain dwords. ECX (the `this` of a thiscall) is recorded too. The first 16 calls are kept.
```
python oracle.py quick <fn> --conv thiscall --this *f32:<values> --local-seed 0x12345 \
    --stub <callee>=u32:0x11223344@u32,p3:f32 --stub AddLiquidParticle=void@p3:f32,p3:f32,u32,f32,i32 i32:3
```
prints each call's arguments. Python: `s = o.stub(target, 'void', capture=['p3:f32', 'u32'], max_calls=16)`, then
`o.stub_args(s)` gives `[{'ecx': bits, 'args': [...]}]`.

**5. The game's random numbers.** The game object lives on the heap, so `fake_game` points its pointer at a fake
holding the synced seed (GameRand, GameFloatRand) and the local seed (LocalRand, LocalFloatRand). `lh` is the
generator under all four.
```
python oracle.py rng game --seed 0x88F89F --n 10 --arg 100
python oracle.py rng localfloat --seed 0x88F89F --n 5 --arg 1.0
python oracle.py quick GameRand u32:100 u32:0 u32:0 --seed 0x88F89F      (prints the seeds after)
```
Python: `o.rng('gamefloat', 2.5, n=100, seed=0x88F89F)` gives `[(bits, value, seed_after)]`.

## Golden vectors (thousands of cases)

Copy the closest spec in `specs/` (with one line of `notes` saying what it is), then:
```
python gen_vectors.py specs/MySpec.json --out <a scratch folder>    (try it)
python gen_vectors.py specs/MySpec.json                             (into vectors/, when it is right)
```
Spec keys for daily needs: `globals` (fixed-address inputs / outputs), `stubs`, `fake_game`, `chain` (an input from
the previous case's output: draw sequences). Give both `symbol` and `addr`: bw1-decomp renames symbols (a fixed
return type changes the decorated name) while the address never moves, so a vanished symbol only warns.

Examples in `specs/`: `GetHeadingFromPoints` (the camera heading between two points, an x87 result),
`MakeShapeCircle` and its variants (the creature dance shapes' points), `GameRand` and the variant `GameFloatRand`
(2000-draw chains from a fixed seed).

## Vectors into a gtest

`cmp/compare.cpp` shows how to read a vector file: `doc["inputs"]` / `doc["outputs"]` name the fields, each case is
`{"in": {name: "0x.."}, "out": {...}}` (or `"error"` when the exe faulted), every value its exact bits in hex; one
lambda per port in `Ports()` maps "in" to openblack's outputs.

For openblack's own unit tests (CI has no exe and no vector files), embed a few cases as constants:
```
python vectors_to_cpp.py vectors/GameRand.json --n 12 > <scratch>/GameRandCases.inc
```
and paste the table into a test under `test/`:
```cpp
// (the table vectors_to_cpp.py printed: struct GameRandCase {max, seed, ret, seed_after}, k_GameRandCases)
TEST(GameRandomOracle, SeededRandomMatchesTheOriginal)
{
	for (const auto& c : k_GameRandCases)
	{
		uint32_t seed = c.seed;
		const uint32_t r = c.max == 0 ? 0 : openblack::game_random::SeededRandom(c.max, seed);
		EXPECT_EQ(r, c.ret);
		EXPECT_EQ(seed, c.seed_after);
	}
}
```
Floats are compared as bits: `std::bit_cast<float>(c.scale)` in, `std::bit_cast<uint32_t>(result)` against
`c.ret_f32`. Pick the hand-picked edge cases (they come first) plus a spread. Code comments stay plain English (no
addresses or decompiled names); the addresses and the evidence go in the wiki, `docs/bw1-notes`.

## Limits

- Every import traps (the OS, the CRT heap, the game's DLLs): the error names the import and its caller. Stub it, or
  stub the callee that reaches it.
- No static initialisers run: start-up tables are zero until a `setup` builds them (see the `GetDistance` spec).
  Heap objects (the game, creatures, villagers) do not exist: pass `this` buffers of the class's size, fake pointers,
  or stub the accessors. Virtual calls need a real vtable. Renderer and file functions: stub them or leave them.
- **x87 vs SSE2.** On an Intel family 6 CPU the baseline makes the float-to-integer conversion take its x87 branch;
  a Pentium 4 takes SSE2 (the `.sse2` variants force it). They differ only for NaN and out-of-range conversions.
  Calls run at the game's control word 0x007F (24-bit precision): every fmul / fadd rounds to float while fsin /
  fcos / fpatan results stay extended until the next operation; openblack's x64 SSE code must reproduce that step by
  step. The vectors record `machine` and `fpu_cw`.
- One thread; a call stuck in a loop is aborted (`timeout_ms`, 5 s). Memory a faulting call wrote stays until reset.

## Safety

- The exe is opened read only (`fopen "rb"`). Patches and stubs only touch the in-memory image, which `reset`
  restores. Keep the oracle's copy of the exe read-only.
- The oracle never runs the game or openblack. Put its work files in a scratch folder of your own.
