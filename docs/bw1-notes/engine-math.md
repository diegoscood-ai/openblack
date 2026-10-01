# Coordenadas, terreno, matrices y Zoomer

Matemáticas básicas del motor original (LH3D) y cómo se portan a openblack: el punto fijo de las posiciones, con sus
celdas y su espiral; las distancias y sigmoides de `GUtils`; la altura exacta del terreno; la convención de las
matrices LH, y el interpolador `Zoomer`. Todo es **fiel** (verificado en el ejecutable) y está portado, salvo lo que se
marca en [Pendiente](#pendiente).

- [MapCoords](#mapcoords): punto fijo, celdas, `InBounds`, vecinos y espiral (`ecs::map_coords`)
- [Distancias de GUtils](#distancias-de-gutils): raíz de tabla, `hypotenuse`, `GetDistanceInMetres`,
  `FastDistance` y las sigmoides (`gutils`)
- [Altura del terreno](#altura-del-terreno)
- [Matrices LH](#matrices-lh)
- [Zoomer (LH3DLib)](#zoomer-lh3dlib)
- [Pendiente](#pendiente), [Ganchos de prueba](#ganchos-de-prueba), [Fuentes](#fuentes)

## MapCoords

✅ Fiel y portado en `src/ECS/MapCoords.{h,cpp}`, namespace `openblack::ecs::map_coords` (sesión «sistemas2»,
2026-10-01). Es la única representación de posición en el mapa del original y la usa todo: hay 136 llamadas al
constructor `MapCoords(LHPoint)`, 196 a `GetLHPoint`, 71 a `ToMap`, 80 a `InBounds` y 58 a `GUtils::Spiral`.

**Estructura** (bw1-decomp `MapCoords.h`): `{int32 x (+0), int32 z (+4), float altitude (+8)}`. x y z van en 16.16: la
palabra alta es la celda de 10 m y la baja la fracción (una celda = 0x10000). `altitude` es la altura **sobre el
suelo**; la absoluta es `GetAltitude(pos) + altitude`. `JustMapXZ = {int16 x, z}` es un paso de celdas.

**La FPU va a 24 bits** (fn_007DEE00, `and 0xFCFF` en 0x7DEE0D; se llama en `pc_main` y en cada `GGame::EndTurn`).
Por eso cada producto se redondea a float. La réplica exacta es `(int32)(m * 6553.6f)` hecho en float. Hacer el
producto en double da ±1 unidad (0,15 mm) en el 41 % de los valores.

| API (`ecs::map_coords`) | Original | Qué hace |
|---|---|---|
| `k_FixedPerMetre` = 6553.6f | [0x8AC400] = 0x45CCCCCD (6553.60009765625) | metros → 16.16 |
| `k_MetresPerFixed` = 10/65536 | [0x8AA3A4] = 0x39200000, exacto | 16.16 → metros |
| `k_MapCells` = 512 | g_game+0x59C4 / +0x59C8 (`GMap::Init(0x200, 0x200)`, 0x6014C8 / 0x6014F1) | celdas por lado |
| `ToFixed(m)` | `fld; fmul [0x8AC400]; __ftol` 0x7A1400 (`Set` 0x603346..0x603367, 258 copias en línea) | trunca hacia 0 |
| `ToMetres(f)` | `fild; fmul [0x8AA3A4]` (`GetLHPoint` 0x605C40 = `ConvertToLHPoint` 0x6041C0) | un solo redondeo (con más de 2^24, un `(float)f` previo redondearía dos veces) |
| `ToFixedGUtils(m)` | `fmul 65536 [0x8AC408]; fdiv 10 [0x99A1BC]; __ftol` (0x74D52F, 0x74D595, 0x74D85A … 0x74F3A3; `MapCoords::SetX` de bw1-decomp) | **otra** función: 1464 m da 9594470, y `ToFixed` da 9594471 |
| `Quantise(m)` | `ToMetres(ToFixed(m))` | la posición tal como la guarda un MapCoords; **no es idempotente** (`ToFixed(ToMetres(8090858))` = 8090857, como en el original) |
| `CellOf(f)`, `CellX`/`CellZ`, `Cell`, `CellOf(vec2/vec3)` | `xor; mov si, [ecx+2]` (palabra alta **sin signo**, `ToMap` 0x603433) | un negativo es la celda 0xFFFF: fuera |
| `SignedCellOf(f)` | `movsx` de `JustMapXZ` (0x5E1950; `ApplyEffectToMapPos` 0x525212) | la palabra alta con signo |
| `InBounds(MapCoords / ivec2 / vec3, cells = 512)` | `MapCoords::InBounds` 0x6042C0 = `JustMapXZ::InBounds` 0x5E1860: `jae` contra +0x59C8 (cx) y +0x59C4 (cz) | el mapa de 512, no la extensión de la tierra |
| `CellIndex` | `ToMap` 0x603430 = 0x5E1950: `cx * [+0x59C4] + cz`; la celda está en g_game + 0x59FC + índice·8 | −1 en vez de NULL |
| `AddCells(MapCoords&, JustMapXZ)` | `operator+=(JustMapXZ)` 0x605470: `add word [ecx+2]`, `add word [ecx+6]` | suma de 16 bits: la fracción se conserva |
| `k_Neighbours4` | 0xDA59FC (lo rellena 0x74CA10): (1,0), (0,1), (−1,0), (0,−1) | la tabla de la espiral; también se lee sola (0x602CC4, 0x768374, 0x770104) |
| `k_Neighbours8` | 0xDA59D8 (lo rellena 0x74CA60 y lo lee fn_00504143): (1,0), (1,1), (0,1), (−1,1), (−1,0), (−1,−1), (0,−1), (1,−1), (0,0) | otra tabla |
| `Spiral{dir = 1, count = 1}.Next()` | `GUtils::Spiral` 0x74D7E0: `dec [count]; jne` (0x74D7E9); `++dir; count = dir/2` (cdq/sub/sar, 0x74D7EF..0x74D7F7); luego `&tabla[dir & 3]` (0x74D7FE) | primero actualiza y luego lee |
| `SpiralIncrement(MapCoords&, Spiral&, step)` | `GUtils::SpiralIncrement` 0x74D810 (solo la llama `Town::FindClearArea`, en 0x741384) | la misma regla y luego `x = ToFixedGUtils(tabla.x · step + ToMetres(x))` (0x74D836..0x74D8A8) |
| `CellSpiralSize(r)` | `GetMapCellSpiralSizeFromRadius` 0x74F520: `n = ftol(r · 0.2 [0x8AA3AC])`; `cmp eax, 1; jae` (sin signo: solo el 0 pasa a 1); n² | 4 llamadores |
| `IncrementSpiralSize(r, step)` | `GetIncrementSpiralSizeFromRadius` 0x74F540: `n = ftol(r · −2 [0x8C7CE0] / step)`; (1 − n)² | 1 llamador |
| `FromWorld(vec3)` / `ToWorld(MapCoords)` | `MapCoords(LHPoint)` 0x603160 / `Set` 0x603340 (`altitude = y − GetAltitude(this)` en la posición truncada, 0x603371..0x60337C) y `GetLHPoint` 0x605C40 | la altura la da `GetHeightAt` de la isla (U3 de «sistemas»); sin isla es 0 |

**La espiral.** Todos los llamadores revisados empiezan con `dir = count = 1`: por ejemplo `Reaction::SpreadReaction`
0x6E3E51, `GMagicHealInfo::FindTargets` 0x5FBB9A, el rayo 0x6902A7..0x6902BB, el tornado 0x6D22E9 o `Tree::EndPhysics`
0x74B98A. Procesan primero la celda central y luego avanzan con `+= *Spiral()`. La comprobación de `InBounds` se hace en
cada celda; el radio no se recorta. Los pasos desde el arranque son (−1,0), (0,−1), (+1,0)×2, (0,+1)×2, (−1,0)×3,
(0,−1)×3, (+1,0)×4…

El número de celdas lo calcula cada llamador, y esas fórmulas se quedan en su sitio:
- `CellSpiralSize`, en 4 sitios.
- `ceil(2R/10)²` en la curación (0x5FBB51).
- `max(3, ceil(2R/10))²` en 0x604AF8.
- `ceil((r+20)/10)²` en la explosión (0x67E5F2).
- `4·ceil(R/10)²` en el rayo (0x690276).
- `ftol(ceil(…))²` en FireFly (0x52A6A7).

**Lo que no es MapCoords** (no se funde):
- La rejilla de 64×64 de fn_005E1890 / `SpellGrid` (`x>>16>>3`).
- La espiral polar de `SpellForest::SpellEvent` 0x725830.
- `fistp(x·0.1)` de `AttemptToAddSoundEvent` 0x6465DF (`sea_cells::RoundedCellOf`).
- `ftol(x·0.1 [0x8AC404])` de `GScript::GetLandHeight` 0x6FB1F0, de `LandAvoid` ([0x8AB22C]) y de la creación de
  `CitadelHeart` (0x8827C7..0x882810, con `jl`/`jg` contra 0..0x1FF). `CitadelArchetype.cpp` ya lo hace así, así que el
  «arreglo» que proponía el plan para la ciudadela era un falso positivo.
- Las distancias (`GetDistance` 0x74CCB0, `GetDistanceInMetres` 0x74CD70): ver [Distancias de GUtils](#distancias-de-gutils).

**Qué usa ya la API.**
- `sea_cells::CellOf/InBounds` y `MapInterface::GetGridCell` son envoltorios. `GetGridCell` ya no es UB con negativos:
  devuelve la palabra alta sin signo (0xFFFF).
- Se migraron estas copias:
  - animales: AnimalAI y AnimalAIDetail (la constante, `CellOf`, `InBounds` y la espiral), AnimalLairs,
    AnimalPredators y AnimalWallHug;
  - aldeanos y ciudad: VillagerSpeed y TownQueries (conversiones, tamaños, `SpiralIncrement` y la espiral de
    `CheckForClearArea`), AbodeQueries, StreetLantern y WorshipSite;
  - mapa y objetos: WaterQueries, PotResource, MapProduction, MapCollide, Trees y MobileWalkPaths;
  - efectos y magia: Reactions, EffectValues, FireEffect, CastRules, `magic::ToMap`, `Spell::castPos`, SpellFlock y
    SpellWater;
  - clima: Climate y WeatherLand;
  - partículas: PSys `Flock`.

**Arreglos de fidelidad** que trajo:
- Productos en double pasados a float:
  - WaterQueries: `k_WorldToFixed` era double, y también el `x·65536.0·0.1f` de `FindNearestStreamPos`.
  - TownQueries: usaba el double 6553.6 (ni siquiera el valor float); también `SpiralIncrement` y los tamaños.
  - AbodeQueries (la puerta, 0x63AFF2), SpellFlock y PSys `Flock`.
- Climate: la vuelta usaba 0.000152588f = 0x39200008 en lugar de [0x8AA3A4] = 0x39200000 (3,9 mm a 5 km).
- `InBounds` por la extensión de la tierra, con `>` estricto (AnimalAI.cpp, Reactions.cpp), pasa a 0x6042C0: x = 0 y
  las celdas sin bloque dentro del mapa ya cuentan.
- `floor` en lugar de truncar (FireEffect, CastRules): difería en x ∈ (−1,5e−4, 0).
- `GetGridCell(pos − radius)` con negativos (UB):
  - `ApplyEffectToMapPos` hace ahora lo del original: esquinas con `ToFixedGUtils(ToMetres(x) ∓ r)`, palabras altas
    con signo e `InBounds` en cada celda (0x52514F..0x525259).
  - `MapProduction` recorre las celdas con signo y salta las de fuera.
  - Un móvil fuera del mapa no entra en la rejilla (`ToMap` da NULL).
- `magic::ToMap` y el `castPos` de la mano (0x72056B..0x720595) ya truncan a 16.16. `ToWorld` no vuelve a truncar,
  porque una segunda ida y vuelta puede perder una unidad.
- `SpreadReaction`: el tamaño es `CellSpiralSize` (0x74F520, con su `jae` sin signo) y la posición avanza con `AddCells`
  sobre MapCoords (0x6E3F6E), en vez de sumar 10 m en float.
- `MobileWalkPaths`: la vuelta ya no redondea dos veces con más de 2^24 unidades.

**Segunda pasada (auditoría, 2026-10-01).** Lo que faltaba de la primera, comprobado otra vez en el binario:
- **Todas las espirales avanzan ya sobre un MapCoords**, no sobre una celda en un `int` ni sobre metros en float. En el
  original el llamador copia su MapCoords y lo mueve con `operator+=(JustMapXZ)` 0x605470, que suma **16 bits a la
  palabra alta**: la fracción no cambia y la celda da la vuelta. Por eso una espiral que empieza a la izquierda del mapa
  (x ∈ (−10, 0), celda 0xFFFF) entra en la celda 0 con el primer paso `+1`, mientras que con la celda en un `int` llega
  a 0x10000 y se queda fuera para siempre. Sitios corregidos y su original:
  - `Magic/CastRules.cpp` `FindHealTargets` ← `FindTargets` (copia 0x5FBB6B, `InBounds` 0x5FBBBA, `ToMap` 0x5FBBCB,
    `Spiral` 0x5FBCE5, `+=` 0x5FBCF1); el `InBounds(vec3(celda·10))` que daba la vuelta celda → metros → fijo pasa a
    `map_coords::InBounds(coords)`.
  - `ECS/Fire/FireEffect.cpp` (copia 0x72F5E1, `InBounds` 0x72F60C, `GetDistanceInMetres` 0x72F674, `Spiral` 0x72F6C3,
    `+=` 0x72F6D0). La distancia se mide entre los dos MapCoords: como la fracción es la misma, la diferencia son
    celdas enteras. `CellObjects` usa `map_coords::InBounds` en vez de su propia copia.
  - `Magic/Spells/SpellWater.cpp` (copia 0x7250A2, `Spiral` 0x725166, `+=` 0x725173; 9 celdas, `ebp = 9`).
  - `ECS/AnimalAI.cpp`: `CalcRandomPos` ← `Living::CalcRandomPos` (0x5ED0FE..0x5ED152 el punto inicial con
    `ToFixedGUtils`, `+=` 0x5ED1C8), `LookForFoodPos` ← `Animal::LookForGrazePos` (`+=` 0x41A945) y la fusión de bandadas
    (`+=` 0x41A76A); `ECS/AnimalPredators.cpp` `FindPrey` ← fn_00419490 (`+=` 0x41954B). `detail::Spiral::Advance`
    envuelve `AddCells`.
- `TownQueries`: `Ftol` recibe un **float** (el comentario decía «x87 extendida», que contradice el `and cw, 0xFCFF` de
  0x7DEE0D). La media de la congregación (0x7409F3..0x740A1E) hace `fild qword` (exacto) y `fdiv` a 24 bits, así que el
  cociente se redondea una vez a float antes del `__ftol`: con 100 posiciones pasa de 2^24 y el truncado podía salir una
  unidad distinto. `GetPosFromAngle` (0x74D580) ya es `ToFixedGUtils` en float.
- `Climate`: se quita `CellCentre()` (la palabra alta con desplazamiento **con signo** × 10). Ni `ProcessAll`
  (0x771DA0) ni `FindWhereToCreateStorm` (0x772D3E, 0x772D6F) leen la palabra alta: las dos construyen el LHPoint con
  `fild; fmul [0x8AA3A4]`, o sea `Centre()`. La celda del centro en `FindWhereToCreateStorm` (0x772C38, 0x772C65) sí es
  la palabra alta, pero **sin signo** (`xor eax, eax; mov ax, [ebp+0x16]`), y se suma con `fiadd`: ya es `CellOf`.
- `SpellFlock`: `DestinationAt` (0x7238E2..0x723905) lee la palabra alta sin signo (`CellOf`) y calcula en float, no en
  double; `IsPosOnCorridor` (0x420E67) usa `CellOf` igual.
- `VillagerSpeed`: la vuelta a metros va marcada **(inferido)**. `MobileWallHug::SetSpeed` 0x60FC50 guarda el u16 tal
  cual en +0x5A y el original nunca convierte esa velocidad a metros (la suma a un MapCoords); openblack la guarda en
  metros, así que usa `ToMetres` por ser la conversión del original en todos los demás sitios.
- `k_MapCells`: la cita estaba mal atribuida. 0x6014C8 y 0x6014F1 están **dentro** de `GMap::Init` 0x6014C0; la llamada
  `GMap::Init(0x200, 0x200)` está en `GGame::Init` (0x54F650+0x2A0).

## Distancias de GUtils

✅ Fiel y portado en `src/ECS/GUtilsDistance.{h,cpp}`, namespace `openblack::gutils` (sesión «sistemas2», 2026-10-01).
Es la familia de distancias del original (la unidad `Utils`, 0x74CCA0..0x74F780, más dos funciones de `MapCoords`): unas
**450 llamadas directas** en `runblack.exe`. Va encima de `ecs::map_coords`. Todas las distancias «buenas» son **2D
(x, z)**: la y no se usa nunca.

**Todo pasa por una raíz inversa de tabla.** `InvSqrt` 0x74F620 lee una tabla de 1024 entradas en 0xDA5A10 que se llena
una sola vez (fn_0074F590, con la bandera [0xDA6A10]; `GUtils::SetupUtils` 0x74CCA0 es solo un `jmp` a ella, y la llama
`GGame::InitOneTimeOnly` en 0x54F07B, justo **después** de poner la FPU a 24 bits). El error de la raíz es de **−0,097 %
a +0,092 %** (recorrido completo). Consecuencias que se ven: una celda de 10 m mide 10,0049 m, 100 m miden 100,0244 m y
400 m miden 400,0977 m. `1/InvSqrt(1)` no da 1, da 0,99951171875 (0x3F7FE000).

**La FPU va a 24 bits** (fn_007DEE00, `and cw, 0xFCFF` en 0x7DEE0D). Por eso **todo el módulo está en float**, sin
double y sin FMA: tres de las copias de openblack hacían la suma o el cociente en double y se desviaban (ver abajo).

| API (`openblack::gutils`) | Original | Notas |
|---|---|---|
| `InvSqrtTable()` | fn_0074F590, tabla 0xDA5A10, bandera [0xDA6A10] | entrada i = los 10 bits altos de la mantisa (`& 0x7FE000`) de 1/√f, con f = `0x3F000000 \| i << 14`; un 1 exacto se guarda como 0x7FE000 (0x74F5ED) |
| `InvSqrt(x)` | `_FUN_0074f620` 0x74F620 | `exp = ((0xBE000000 − (bits & 0x7F800000)) >> 1) & 0x7F800000`, mantisa de la tabla en `(bits >> 14) & 0x3FF`; el signo no se mira, y x = 0 da ≈ 2^63 |
| `Hypotenuse(int32, int32)` | `hypotenuse` 0x74F680 | 16.16 dentro y fuera: `x = dx·2^-16` [0x99A1D4], `s = float(z·z + x·x)`, `ftol(65536.0 [0x99A1D8] / InvSqrt(s))`. **Trunca** y no tiene corte en el cero |
| `Hypotenuse(float, float)` | `hypotenuse` 0x74F6C0 | 0 si \|a\| y \|b\| son los dos ≤ 1e-4 [0x8BF518] (`test ah, 0x41` también se cumple con una comparación no ordenada: un lado NaN cuenta como «≤ 1e-4»); si no `1 / InvSqrt(float(a·a + b·b))` [0x8AA390]. **No** trunca |
| `ConvertWholeDistanceToMeters(i)` | 0x74DCC0 | `fld 10 [0x99A1BC]; fmul 2^-16 [0x8AC41C]; fimul i`: el entero es exacto antes del redondeo (= `map_coords::ToMetres`) |
| `ConvertMetersToWholeDistance(m)` | 0x74DCE0 | `ftol(m / 10 · 65536 [0x8AC408])`; 65536 es potencia de dos, así que es `ToFixedGUtils` |
| `GetDistance(MapCoords, MapCoords)` | 0x74CCB0 = su gemela 0x74CCE0 (byte a byte) | `Hypotenuse(b.x − a.x, b.z − a.z)` |
| `GetDistanceToCell(MapCoords, JustMapXZ)` | fn_0074CD10, con fn_0074E2D0 = `(short(celda) << 16) + 0x8000` | al **centro** de la celda |
| `GetDistanceInMetres(...)` | 0x74CD70 = 0x74CD50 = `MapCoords::GetDistanceInMetres` 0x605CD0 | `ConvertWholeDistanceToMeters(GetDistance(a, b))`. **383 llamadas** (202 + 62 + 119). Sobrecargas: MapCoords, `vec3`/`vec2` en metros (truncando a 16.16 como 0x603160) e `ivec2` en 16.16 |
| `GetDistanceInMetresToCell` | fn_0074CD90 | |
| `GetDistance(vec3, vec3)` | `GUtils::GetDistance(LHPoint, LHPoint)` 0x74CDE0 | las diferencias x/z guardadas como float (0x74CDEC, 0x74CDFA) y luego `Hypotenuse(float, float)`: metros, sin pasar por MapCoords |
| `GetMetresDistanceSq` | `MapCoords::GetMetresDistanceSq` 0x605FB0 | el cuadrado **exacto** en float: sin tabla, así que **no** es `GetDistanceInMetres` al cuadrado (100 × 100 m da 20000, no 20018) |
| `FastDistance` | `GUtils::FastDistance` 0x74CE10 | `max + (min >> 1)` (`sar`) en unidades MapCoords: no es una longitud euclídea |
| `ChebyshevDistance` | fn_0074CED0 (con el `abs` fn_0074DD00) | `max(\|dx\|, \|dz\|)`, comparados **sin signo** (0x74CF05) |
| `k_Sigmoid`, `detail::k_SigmoidBits` | tabla 0xC23284, 41 floats en .data | se copian **los bits**; T[0] = 0 exacto y T[37..40] = 1 exactos. Es una logística `1/(1+e^(−1,0232·(i−20)))` *(inferido: por ajuste)* |
| `SigmoidThreshold(a, b)` | `GUtils::SigmoidThreshold` 0x74F170 | `a == 1 → 0` (0x74F174; un NaN en a también); si no `T[min(ftol((clamp(clamp(b,−1,1) − a,−1,1) + 1)·20,5 [0x99A1D0]), 40)]`. **El umbral es el PRIMER argumento** |
| `GetDistanceModifier(d, max)` | 0x74F290 = su copia fn_005ECA20 (`ret 8`) | `SigmoidThreshold(0,5 [push 0x3F000000], 1 − min(d, max)/max)`: **baja** con la distancia, de T[30] = 0,99996 en d = 0 a T[10] = 3,6e-5 en d ≥ max (21 de los 41 pasos). Con max = 0, `0/0` da NaN y sale T[0] = 0 |
| `DistanceChangeToBelief(x, y)` | `GBelief::DistanceChangeToBelief` 0x438770 = su copia fn_00657F30 | `SigmoidThreshold(−0,9 [0xBF666666], float(−(x/y)))`: otra curva sobre la misma tabla |
| `CreatureSigmoidThreshold(a, b)` | `Creature::SigmoidThreshold` 0x4F78C0 | añade `b ≤ 0 → 0` (fcomp 0; test ah, 0x41) |

**Rutinas que NO se funden** (dan resultados distintos): las dos `hypotenuse`; `GetDistanceInMetres` (cuantizada a
1/65536 de celda) frente a `GetDistance(LHPoint)` (float directo); la distancia a celda (al centro, +0x8000);
`GetMetresDistanceSq` (sin tabla); `FastDistance` y Chebyshev; `SigmoidThreshold` frente a la de Creature;
`GetDistanceModifier` (a = 0,5) frente a `DistanceChangeToBelief` (a = −0,9).

**Código muerto que no se porta** (sin `call`, referencias ni punteros): 0x74CDB0, 0x74CE50, 0x74CE80, 0x74F660,
0x74F720 y 0x74F740. Los símbolos W120 también fallan en dos sitios: 0x74CD50 se llama `ReactionInfo::GetInfo` (es la
gemela de `GetDistanceInMetres`) y `hypotenuse` 0x74F680 aparece como `void` cuando devuelve un int en eax.

**Arreglos de fidelidad que trajo.**

1. **`WorshipScore` fn_0073C590 tenía los argumentos de `SigmoidThreshold` al revés** (`WorshipPercentage.cpp`): pasaba
   `(x, 0,5)` en vez de `(0,5, x)`, con lo que la curva salía **en espejo**. Con d = 0 daba 0 donde el original da
   0,99996, y con d ≥ max daba 0,99996 donde el original da 3,6e-5. Como `AdjustWorshipersWorshipping` 0x73C0F0 ordena
   de mayor a menor puntuación (0x73C180..0x73C1A6), openblack mandaba a rezar **primero a los aldeanos más lejanos**;
   van primero los más cercanos. El error se repetía en `WorshipPercentage.h`, en `test_worship.cpp` y en `magic.md`.
2. **`WorshipScore` multiplica por vida³, no por vida²** (0x73C63A..0x73C644: `mov eax, 2`, y dos vueltas de
   `dec eax; fmul vida; jne` sobre st0 = vida; el modificador se multiplica al final, en 0x73C646).
3. **`VillagerFire::DistanceModifier` era un `smoothstep` inventado**: con max = 400 m daba 0,156 a 250 m donde el
   original da 0,0444, 0,352 a 220 m (original 0,264) y se saturaba a 1 / 0 en los extremos en vez de
   0,99996 / 3,6e-5. El umbral que sigue (> 0,1, [0x8AB22C]) cambiaba de sitio. Es `GetDistanceModifier(d, 400)`, con el
   400 inmediato en 0x765A77 y la distancia de 0x765A81.
4. **La tabla de `Trees.cpp` estaba redondeada a 4 decimales** (35 de las 41 entradas distintas; 20 de ellas dentro del
   rango 10..30, el único que usa `GetDistanceModifier`: T[1..10] valían 0 y T[30..36] valían 1).
5. **La tabla de `WorshipPercentage.cpp` estaba redondeada a 5 decimales** (lo mismo, T[1..8] = 0 y T[32..36] = 1).
6. **La sigmoide de `AnimalLairs.cpp` se calculaba en double**: en los saltos de la tabla (±40 ulp) salía otro índice en
   50-60 de 3321 casos, y 1 de 300 000 con valores aleatorios.
7. **`hypotenuse(int)` en double** (WaterQueries y AnimalLairs): distinta en 31 678 de 200 000 muestras frente a la
   emulación a 24 bits; casi siempre 1 unidad, pero al cambiar de cubo de la tabla llega a 2431 unidades = **0,37 m a
   1 km**.
8. **`TownQueries::GetDistanceInMetres` usaba `std::hypot`** sin la tabla (100 m daban 100 m, no los 100,0244 m del
   original) y convertía el entero a float **antes** de multiplicar, mientras que el original usa `fimul` sobre el
   entero exacto (distinto en 22 572 de 100 000 enteros por encima de 2^24, es decir más de 2560 m).
9. **`AnimalWallHug` `MoveToCircleHug`** hacía la raíz y el ×128 − 1 en double; en el original las dos constantes se
   cargan como `qword` pero la FPU está a 24 bits, así que es todo float, y la base es `GetMetresDistanceSq` 0x605FB0
   (0x60D9F0), el cuadrado exacto.
10. **`FeatureScriptCommands::FindNearestTown`** comparaba **distancias al cuadrado**; el original llama a fn_00605CD0
    (0x553016, 0x55302E) y compara la distancia, así que con la tabla y la cuantización dos ciudades casi empatadas
    podían salir al revés.
11. **`CastRules::FindHealTargets` medía desde el punto del lanzamiento** (el error venía de antes). El original
    (`GMagicHealInfo::FindTargets` 0x5FBB00) solo tiene un MapCoords en el marco, la copia del argumento
    ([ebp−0x24], 0x5FBB6B..0x5FBB82), y es el que **anda la espiral** (`operator+=` 0x605470 en 0x5FBCF1). Los dos cortes miden desde él:
    `GetDistanceInMetres(coords, objeto)` en 0x5FBBEE `< R`, y el cuadrado exacto 0x5FBC54..0x5FBCA3
    (`fild [ebp−0x24]` / `[ebp−0x20]`; `(coords − objeto)²` `< R²`, `test ah, 0x41`). Así la curación coge cualquier
    objeto a menos de R del **punto de la espiral** que visita su celda, no del centro del milagro.
12. **`AnimalFlee` `AnimalReaction`** (`ApplyReactionToLivingObjectsAtSquare` 0x6E3F90, la reacción en curso al
    comparar con una nueva) medía al iniciador. El original toma `Reaction::GetPos` 0x6E45C0 (0x6E4142), le saca la
    **celda** con fn_005E17C0 (las palabras altas, 0x6E414C) y mide con fn_0074CD90 (0x6E4157) desde el MapCoords del
    animal al **centro** de esa celda (`GetDistanceInMetresToCell`): hasta 7,07 m de diferencia en la distancia que
    alimenta la puntuación fn_006E4620 (0x6E4173).

**Qué usa ya la API.**

- Se borraron las tres copias de la tabla 1/√ y de `InvSqrt` (`WaterQueries.cpp`, `AnimalLairs.cpp`, `CHLApi.cpp`), las
  cuatro `hypotenuse` y las tres `SigmoidThreshold` privadas (AnimalLairs, Trees, WorshipPercentage).
- `WaterQueries` (`DistanceInMetres`, el corte de `NearestCoastal` y la distancia a los puntos de río),
  `AnimalLairs` (`MapDistance` y `ForestScore` fn_0053AD00), `CHLApi` `GET_DISTANCE` (0x6F8CA0 → 0x74CDE0),
  `TownQueries::GetDistanceInMetres` (y con ella `VillagerDecide` y los radios de búsqueda de ciudad).
- Sustitutos en float cambiados por la API **solo donde se ha leído la llamada del original**: `FireEffect` y
  `VillagerFire` (`Distance2D`, sus ocho usos leídos uno a uno: `HeatTransfer` fn_0072F980 0x72FA44,
  `NearestFireToFight` fn_00730070 0x73010A, `IsBesideFire` fn_0075ABA0 0x75ABC7, `OnFire` 0x75B27B,
  `ReactToFirePriority` 0x765610 en 0x76567E y 0x76582B, `ReactToFire` 0x765870 en 0x7658D9 y 0x765A81), `Reactions` `SpreadReaction` (0x6E3E91), `Climate` `FindWhereToCreateStorm` /
  `CreateStorm` / fn_00772330 (0x74CDE0), `CastRules` (el radio de curación, 0x5FBBEE, desde la espiral), `SpellFlock::WolfArrived` (0x421300),
  `SpellWater::ApplyWaterSpell` (0x7250EC), `EffectValues::ApplyEffectToMapPos` (0x525307), `Trees`
  (`DistanceToForest` 0x53A890 / 0x53AC20 y el bosque escénico), `AnimalAI` (`PosWithinDomain` 0x5ED010,
  `SetNewWander` 0x41A3F0, `KeepFlockMemberWithinFlockArea` 0x41ABB0), `AnimalFlee` (`ReactToFoodPriority` 0x5F1710,
  `SetupReactToFlyingObject` 0x4204A0, `ProcessReaction` 0x5F1270; `AnimalReaction` con `GetDistanceInMetresToCell`
  0x6E4157), `AnimalPredators` (fn_00419340), `AnimalWallHug` (0x60D9F0), `StreetLantern` (`GStreetLantern::Create`
  0x7346E0: recorre la celda con `MapCoords::FindType(0x1C)` 0x6045C0 y corta con `d < 0,5` [0x8AA3B4], `test ah, 1`
  en 0x73470C) y `FeatureScriptCommands::FindNearestTown` (fn_00552FF0). Ojo: `GStreetLantern::IsALaternWithinDistance`
  0x734A30 es **otra** rutina (la lista global de faroles g_game+0x205C34 y `d <= r`, `test ah, 0x41`), sin portar.

## Altura del terreno

`LH3DIsland::GetAltitude` (0x803090), portado exacto en `LandIsland::GetHeightAt`:

1. Celda `(x>>16, z>>16)`, fracciones `fx, fz` de 16 bits (se usan `>>8`, 0..255).
2. Bloques de 17×17 celdas (fila compartida): vecinos `+1` = z+1, `+17` = x+1. Altura de celda en `cell.altitude`.
3. Si el vértice base ≤ 4, las alturas ≤ 3 cuentan como 0 (borde del mar; global 0xC37BF4 activo).
4. El bit `split` de la celda (`properties +6 & 0x80`) elige la diagonal. La 4.ª esquina se extrapola de las otras tres
   para que la mezcla bilineal sea plana en el triángulo:
   - split y `fz > 0xFFFF - fx`: `c00 = v10 + v01 - v11`; split y no: `c11 = v10 + v01 - v00`
   - sin split y `fx > fz`: `c01 = v00 + v11 - v10`; sin split y no: `c10 = v00 + v11 - v01`
5. `atX1 = (c11 - c10)*fz + (c10<<8)`, `atX0 = (c01 - c00)*fz + (c00<<8)`,
   `h = (((atX1 - atX0)*fx) >> 8) + atX0`, resultado `h * 0.67 / 256`.

Verificado: Land1 en (1788.4, 2710) = **28.9173050**, el valor grabado del original. El render del terreno de openblack
usa la misma triangulación (coincide con el terreno físico de Bullet hasta el milímetro).

## Matrices LH

- `LHMatrix` es 3×3 por filas + traslación (convención de vectores fila, estilo D3D): **las filas de LH son las
  columnas de `glm::mat3`** en openblack.
- `LHMatrix::SetYXZMatrixOnly(y, x, z)` (0x7FAC10), con a=Y, b=X, c=Z:
  - fila 0 = (ca·cc − sa·sb·sc, −cb·sc, sa·cc + ca·sb·sc)
  - fila 1 = (sa·sb·cc + ca·sc, cb·cc, sa·sc − ca·sb·cc)
  - fila 2 = (−sa·cb, sb, ca·cb)
- La escala se aplica multiplicando las 9 componentes (uniforme).
- Solo Y: `glm::eulerAngleY(-yAngle)` da lo mismo que el original (así lo usan pots, árboles...).

## Zoomer (LH3DLib)

`Zoomer::SetDestinationWithSpeedAndTime(dest, destSpeed, T)` (0x407D60) y `Update(dt)` (0x442720). Polinomio de grado 4
en t que parte del valor y la velocidad actuales y llega a `dest` en T con velocidad `destSpeed` y aceleración 0.
Solución en tiempo normalizado (inversa de `[[1/24,1/6,1/2],[1/6,1/2,1],[1/2,1,1]]`):

```
r1 = dest - v0 - s0*T;  r2 = (destSpeed - s0)*T
e = 72 r1 - 48 r2;  d = -2 r2 - 2e/3;  c = 2 r2 + e/6
c2 = c/T²; c3 = d/T³; c4 = e/T⁴
valor(t) = v0 + s0 t + c2 t²/2 + c3 t³/6 + c4 t⁴/24
```

T < 0.001 fija el valor. Implementado en `src/Common/Zoomer.{h,cpp}`. Lo usan, entre otros, la distancia de la mano
(g_HandDistZoomer) y el hundimiento de los montones (T = 1 s).

## Pendiente

### MapCoords

Copias de MapCoords que aún no usan `ecs::map_coords` (estado a 2026-10-01, rama `local/sistemas2`):

**Aplazadas, porque milagros2 está editando esos archivos:**
- `PSys/Rules/Lightning.cpp`: ⚠ la espiral de `SpiralStep` (:58-70) está **mal**.
  - Lee la tabla antes de actualizar, usa `count = dir%2==0 ? dir/2+1 : (dir+1)/2` y arranca con `direction = 0`
    (:314). El original arranca con `dir = count = 1` (0x6902A7..0x6902BB) y llama a 0x74D7E0 en 0x69058B.
  - Por eso recorre la espiral negada y, como el lado es par (`4·ceil(R/10)²`), un conjunto de celdas distinto.
    `fn_00690880` comparte el fallo.
  - La celda inicial `(int)(x·0.1f)` (:310): antes de cambiarla hay que comprobar en el binario si el original hace
    ahí `ftol(x·0.1)`, como `CitadelHeart`.
- `PSys/Rules/Storm.cpp`:
  - la celda del polvo, `floor(x/10)` (:843-850);
  - `PotsByCell` y `start`, con `(int)(x·0.1f)` (:880, :1304, :1329): misma comprobación que en el rayo;
  - `SpiralStep` (:916-930): ya es correcta, solo falta usar `map_coords::Spiral`.
- `PSys/Rules/Explosion.cpp`: la espiral (:181-195) y las celdas de `GetGridCell`.
- `Magic/Objects/MagicTeleport.cpp`: `k_UnitsPerMetre` y `ToUnits` (:63, :86-89). El barrido cuadrado (:504-535)
  sustituye a `Spiral` (0x604D43), pero no cambia el resultado.
- `Magic/Spells/SpellForest.cpp`: `CellOf` y `spell_forest::ToMapCoords`, en double (:104-107, :395-401).
- `ECS/Systems/Implementations/HandSpellSeed.cpp:434`: el MapCoords del círculo, sin truncar.

**Audio** (lo migra «audio» en su B11):
- `Audio/ThingMusic.cpp:81-87`: la ida y vuelta en double.
- `Audio/SoundMap.cpp:133-137, 156-157, 188-193, 336-337`: ya en float y correctas; solo falta usar la API.

**Dudosas, no migradas:**
- `CHLApi.cpp:900` (`MOVE_GAME_THING`): truncar ahí haría una segunda conversión al caminar.
- `SpellResource.cpp:68`: `AddResourceToPos` ya convierte una vez, igual que el original.
- `Trees.cpp:1179-1186`: las celdas de la flexión de los árboles, con diferencias de celdas con signo.

**Fuera de este sistema, encontrado al auditarlo** (no se ha tocado; es de sus dueños):
- `Climate.cpp:231` (`FindWhereToCreateStorm`) compara con `other.outerRadius`, pero el original compara con el radio
  del clima que crea la tormenta: `fcomp [ebp+0x24]` en 0x772D9C, con `ebp = this`. En `ProcessAll` (0x771DBF) sí es
  `[esi+0x24]`, el del otro clima, así que la diferencia entre las dos rutinas es del original.
- `AnimalAI.cpp` `LookForFoodPos`: el número de celdas es `(radio/10)²` **en entero**; el original divide en float y
  trunca el cuadrado (`fild; fdiv 10 [0x8AB744]; fld st(0); fmul st(1); __ftol`, 0x41A8B3..0x41A8DD): con radio 35 da 12
  celdas, no 9.

**Otros:**
- Las posiciones de openblack son float en metros. Mientras no se guarden como MapCoords enteros, cada `ToFixed` de un
  valor ya cuantizado puede perder una unidad (0,15 mm). Las celdas no cambian: una posición en un múltiplo exacto de
  0x10000 vuelve intacta.
- Distancias sobre MapCoords: hechas, en [Distancias de GUtils](#distancias-de-gutils).
- Revisión de milagros2 (2026-10-01): `WeatherLand.h` y `WorshipSite.cpp` cambian también `floor` por la celda de
  MapCoords (palabra alta). `PotResource` podría recorrer su espiral con `AddCells` 0x605470, por coherencia con el resto.
  `Climate::CellCentre` se queda para las dos distancias de ProcessClimate fn_00772330 (palabra alta sin signo × 10,
  0x7724A6..0x7724DE y 0x772510..0x772548); el resto de los lectores usa `Centre()`.

### Distancias de GUtils

Copias de distancias que aún no usan `openblack::gutils` (estado a 2026-10-01, rama `local/sistemas2`). La regla ha sido
migrar **solo** donde se ha leído en el binario que el original llama a `GetDistance*` / `hypotenuse`; lo demás se deja.

**Aplazadas, porque milagros2 está editando esos archivos:**
- `Magic/Objects/MagicTeleport.cpp:148-153`: `FastDistance` 0x74CE10 (ya es exacta; solo hay que borrar la copia) y
  `Distance2D` (:81-84, usos :171, :198 «fn_00605CD0», :204, :515, :550).
- `Magic/Objects/MapShield.cpp:499-500`, `PSys/Rules/Explosion.cpp:354-414`, `PSys/Rules/Storm.cpp:1333-1334`,
  `Magic/Spells/SpellStormAndTornado.cpp:196-198` y `Magic/Spells/SpellForest.cpp:167, :232`: `glm::length` /
  `glm::distance` donde el original llama a 0x74CD70 / fn_00605CD0.

**De otros dueños, sin autorización todavía:**
- Milagros: `ECS/Influence/Influence.cpp:118-121` (`detail::DistanceXZ`, `std::hypot`, cita 0x74CD70),
  `ECS/Systems/Implementations/VillagerWorship.cpp:161-164` (`FlatDistance`),
  `Worship/WorshipSite.cpp:131, :147`, `Magic/Spells/SpellShield.cpp:73, :239, :258`,
  `Magic/Script/CHLFire.cpp:74` y `Magic/Script/CHLSpells.cpp:185`.
- «sistemas» (tiene el archivo abierto): `ECS/PotResource.cpp:131, :155` (`IsCloseToEqual` 0x6053C0 =
  `GetDistanceInMetres <= r`).
- «audio» (hito B11): `ECS/Fire/FireSound.cpp:38-52` (`CameraDistance`: además la cámara no pasa por MapCoords) y
  `Audio/GameQueries.h:47, :72-76` (`nearestTown`, descrito pero sin implementar en `Game.cpp`).

**Dudosas, no migradas** (no consta en el binario que el original use ahí la rutina):
- `Worship/Citadel.cpp:61` (`NearestTownOfTribe`) y `ECS/Trees.cpp:244, :718, :799, :1038`: sin dirección en la cita.
- `ECS/Weather/Climate.cpp:431` (`ProcessAll`): hace `d2 > r²` y luego `sqrt(d2)`, que no es la forma de una llamada a
  `GetDistance`; `GClimate::ProcessAll` 0x771DBA sí llama una vez a 0x74CDE0, pero no se ha leído dónde.
- `ECS/AnimalFlee.cpp:596, :619, :653` y `ECS/AnimalPredators.cpp:379, :488, :549, :692`: el informe los marca
  *(inferido)* por su sitio en el archivo, no por una lectura.
- `ECS/Systems/Implementations/PathfindingSystem.cpp:100, :218`: portan `MobileWallHug::MoveTo` 0x60AF20, que llama a
  `GetMetresDistanceSq` 0x605FB0 *(inferido)*; haría falta migrar antes su entrada a MapCoords.
- `ECS/Effects/EffectValues.cpp` y `ECS/Fire/FireEffect.cpp` usan `gutils::GetDistanceInMetres(vec3, vec3)`, que
  trunca las posiciones a 16.16 cada vez. Mientras openblack guarde las posiciones en float, eso puede perder una unidad
  (0,15 mm) respecto a un MapCoords guardado.

**Sin portar todavía en openblack** (no hay copia que migrar, la API ya las tiene listas): `GetDistanceToCell` /
`GetDistanceInMetresToCell` en `CreatureMental` 0x4D2B3D (la de `ApplyReactionToLivingObjectsAtSquare` 0x6E4157 ya
la usa, en `AnimalFlee`), `ChebyshevDistance` (fn_0074CED0, un llamador),
`DistanceChangeToBelief` (0x438770, desde los `GetImpressiveValue`) y `CreatureSigmoidThreshold` (0x4F78C0, desde
`CreatureDesires::GetIncrementFromSources`).

**Cambios que se ven y hay que comprobar con captura:** quién va primero a rezar (`WorshipScore`: ahora los más
cercanos, y con vida³), quién va a apagar un fuego (`VillagerFire`, 400 m), a quién cura el milagro de curar (ahora
todo lo que queda a menos de R del punto de la espiral, en un cuadrado de `ceil(2R/10)` celdas de lado), cuándo un
animal cambia de reacción (distancia al centro de la celda de la reacción en curso), el crecimiento del árbol con el milagro de
agua (`GetDistanceModifier(tamaño, 3)`) y las guaridas de los depredadores (la sigmoide ya no se calcula en double).

**Siguen con `glm::length` (para migrar cuando milagros2 suba su tanda 2a):** MagicTeleport.cpp:81-83 (cita
fn_00605CD0), SpellShield.cpp:73/239/258, SpellForest.cpp:167/232, MapShield.cpp:517 y SpellStormAndTornado.cpp:198.

## Ganchos de prueba

- `test_map_coords` (`test/test_map_coords.cpp`) comprueba:
  - las constantes, por bits;
  - `ToFixed`, `ToFixedGUtils` y `ToMetres` con los valores de 24 bits (1464 m → 9594471 frente a 9594470;
    16777217 → 0x45200001);
  - la ida y vuelta que pierde una unidad;
  - celdas e `InBounds` con negativos;
  - `AddCells`: la fracción se conserva, la celda da la vuelta por 0xFFFF y, al revés, una espiral que empieza en la
    celda 0xFFFF (x ∈ (−10, 0)) entra en la celda 0;
  - las dos tablas de vecinos;
  - la secuencia exacta de la espiral y el cuadrado de 4×4 que cubre;
  - `SpiralIncrement` y los dos tamaños.
- `test_gutils_distance` (`test/test_gutils_distance.cpp`) comprueba:
  - los 41 dwords de la tabla 0xC23284, por bits, y que `k_Sigmoid` son esos mismos bits;
  - las entradas 0, 1, 2, 511, 512 (el 0x7FE000 del 1 exacto), 513 y 1023 de la tabla 1/√;
  - `InvSqrt(1)` = 0x3F7FE000 y el ≈ 2^63 del cero;
  - `Hypotenuse(int)` con 0, una celda (65568), la diagonal (92691) y el lado del mapa (33570824);
  - `Hypotenuse(float)` por bits, con el corte de 1e-4 en los dos lados;
  - las dos conversiones de unidades, incluido el `fimul` por encima de 2^24;
  - `GetDistanceInMetres` (100 m → 100,0244; 400 m → 400,098), la distancia al centro de una celda,
    `GetMetresDistanceSq` (sin tabla), `FastDistance` y Chebyshev;
  - `SigmoidThreshold` con el umbral en el primer argumento, los dos recortes y el caso `a == 1`;
  - `GetDistanceModifier` con los 400 m de `ReactToFire` y los 3 de `Tree::ApplyWaterSpell`, y el `max = 0`;
  - `DistanceChangeToBelief`.
  - el corte de `Hypotenuse(float)` con un lado NaN (devuelve 0, como la comparación no ordenada del original).
- `test_worship` llama a `worship::percentage::WorshipScore` de verdad: un aldeano con vida 0,5 en el centro del
  lugar de culto da 0,5³ · 0,99996 y uno con vida 1 a más de d2 da 3,6e-5 (con los argumentos al revés o con vida²
  falla).
- No tienen variables de entorno propias.

## Fuentes

- Desensamblado W120 (`dev\tmp_dis\bwdis.py`): 0x603160, 0x603340, 0x603430, 0x6041C0, 0x6042C0, 0x605470, 0x605C40,
  0x5E1860, 0x5E1950, 0x74CA10, 0x74CA60, 0x74D7E0, 0x74D810, 0x74F520, 0x74F540, 0x7A1400, 0x525100..0x525260,
  0x63AFF2, 0x7204D0 (0x72056B..0x720595), 0x882730 (0x8827C7..0x882810) y 0x7DEE00. De la 2.ª pasada: 0x6014C0,
  0x54F650+0x2A0, 0x5FBB40..0x5FBD10, 0x72F5C0..0x72F6E0, 0x725000..0x725180, 0x5ED080, 0x41A8B0, 0x41A640..0x41A780,
  0x419490, 0x60FC50, 0x7238C0, 0x420E10, 0x771BE0, 0x772BE0, 0x74CDE0 y 0x7409C0..0x740A60.
- Informes: `dev\tmp_dis\unify2\PLAN.md` §1, `map_coords_grid_original.md` (sobre todo su «Verificación adversaria»)
  y `map_coords_grid_openblack.md`.
- bw1-decomp: `src/Black/MapCoords.h`, `Map.h`, `Utils.h` y `Lionhead/LH3DLib/development/LH3DMapCoords.h`.
- Distancias de GUtils: desensamblado de 0x74CCA0:1D0, 0x74CE6E:C0, 0x74DCC0:50, 0x74DD00, 0x74E2D0,
  0x74F170:A0, 0x74F290:40, 0x74F580:1A0, 0x74F620, 0x74F680, 0x74F6C0, 0x605CD0, 0x605FB0, 0x5ECA20, 0x657F30,
  0x438770, 0x4F78C0, 0x73C5C0..0x73C64E (vida³), 0x552FF0, 0x5252E0, 0x60D9D0 y 0x6E3E60; bytes de 0xC23284
  (164 B), 0x99A1D0, 0x99A1D4, 0x99A1D8, 0x99A1BC, 0x8AC408, 0x8AC41C, 0x8AC400, 0x8AA390, 0x8AA3A4, 0x8AB678,
  0x8AB680, 0x8AB41C, 0x8BF518 y 0x930670. Informes: `dev\tmp_dis\unify2\PLAN.md` §6,
  `gutils_distance_original.md` (con su «Verificación adversaria») y `gutils_distance_openblack.md`.
- bw1-decomp para las distancias: `src/Black/Utils.h` (solo firmas; `GetDistance` aparece como `void`),
  `MapCoords.h:137` y `Lionhead/LH3DLib/development/LH3DMath.h:33`.
