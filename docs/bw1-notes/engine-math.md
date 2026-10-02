# Coordenadas, terreno, tamaño de los objetos, reloj del juego, matrices y Zoomer

Matemáticas básicas del motor original (LH3D) y cómo se portan a openblack: el punto fijo de las posiciones, con sus
celdas y su espiral; las distancias y sigmoides de `GUtils`; el reloj del juego; la altura exacta del terreno; la
convención de las matrices LH; el tamaño de los objetos (radio 2D y altura), y el interpolador `Zoomer`. Todo es **fiel** (verificado
en el ejecutable) y está portado, salvo lo que se marca en [Pendiente](#pendiente).

- [MapCoords](#mapcoords): punto fijo, celdas, `InBounds`, vecinos y espiral (`ecs::map_coords`)
- [Distancias de GUtils](#distancias-de-gutils): raíz de tabla, `hypotenuse`, `GetDistanceInMetres`,
  `FastDistance` y las sigmoides (`gutils`)
- [Tamaño de los objetos](#tamaño-de-los-objetos): radio 2D, radio y altura, con las redefiniciones de las clases y
  las derivadas, a nivel de malla y de objeto (`ecs::object`)
- [Reloj del juego](#reloj-del-juego): el turno, los ms del turno, la fracción, el dt del fotograma, la pausa y la
  velocidad (`game_clock`)
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

## Tamaño de los objetos

✅ Fiel y portado en `src/ECS/ObjectMetrics.{h,cpp}`, namespace `openblack::ecs::object` (sesión «sistemas2»,
2026-10-01). Son las funciones virtuales de `Object` que dan el radio 2D, el radio y la altura de un objeto a partir de
la caja de su malla: unas **540 llamadas** en el original (151 a vt+0x64, 48 a vt+0x60 y 343 a vt+0x42C, recuento
heurístico). openblack las tenía escritas a mano 33 veces, con 6 envoltorios, y cada copia conocía como mucho una de las
redefiniciones de las clases.

**La caja de la malla.** `LH3DMesh::ComputeBoundingBox` **0x8081B0** (al cargar) une las cajas de todas las submallas y
guarda en +0x18..+0x20 el centro, en **+0x24 / +0x28 / +0x2C las semiextensiones** `(max − min) × 0,5` [0x8AA3B4]
(0x80831C..0x80835B) y en **+0x30 la semidiagonal** `√((hz² + hy²) + hx²)` (0x80835E..0x808379). Una malla animada
(flag +4 bit 0x100) pasa antes por `LH3DAnim::SetTransform` 0x83A1D0; openblack no lo hace (ver Pendiente).

**Dos niveles.** El original tiene las dos cosas, y no dan lo mismo:
- **Nivel de malla**: lee los campos en línea, sin pasar por la vtable, así que **no ve ninguna redefinición**.
  `IsSuitableForFixed` 0x603E1E..0x603E5B, fn_00604020 0x604042, `Scaffold` 0x6E956A y 0x6EAC14..0x6EAC56, 0x7350A5,
  la criatura 0x4778E9..0x4779B1; semialturas en `Field::Draw` 0x5287B9..0x5287D3, `PhysOb::Initialise` 0x7FB7D9,
  `Tree::Draw` 0x74ABB0, `WorshipTotem::Create` 0x780995, `CitadelHeart` 0x4653FE / 0x467777,
  `Abode::DrawPercentFull` 0x407111, `TownArtifact::Draw` 0x51C9A3 y otros. Ahí un campo mide lo que su malla.
- **Nivel de objeto**: la llamada virtual, con la tabla de redefiniciones de las `??_7` de symbols.txt que derivan de
  `Object` (barrido de las ranuras +0x60, +0x64, +0x120, +0x13C, +0x42C, +0x568, +0x590, +0x5F4, +0x630, +0x64C,
  +0x6C4, +0x798 y +0x7C4 en todas las vtables, 2026-10-01). Las clases que **no** son `Object` no están cubiertas:
  la `Citadel` 0x8C7E68 (y los `Planned*`, `SpellSeedGraphic`) se queda con `GameThing` 0x405140 / 0x405150 = 0,
  `GameThingWithPos::GetHeight` 0x405500 = 0 y `GetScale` 0x4247E0 = 1; `SpellShield` 0x72B440 (`GetSpellMagnitude`
  0x7202C0) / 0x72B450, `SpellStormAndTornado` 0x72D950 / 0x72D960, `Town` 0x73D6E0, `GArena` 0x424780, `Reaction`
  0x55C7D0, `BuildingSite` 0x43D050 y `AtomCore` 0x673C70 tienen su propio `GetRadius` / `Get2DRadius`; `GStreetLight`
  0x735110 (radio 20 [0x8C7658], fn_00735060) y `Mist` 0x6067D0 (`Mist::Get2DRadius` 0x606660) su propio
  `GetDistanceFromObject(MapCoords)`. Nadie las pide a la API: el templo de openblack es el `CitadelHeart` (`Temple`),
  que es un `Object`.

Cada sitio se porta **al nivel que usa el original en ese punto**: una API de un solo nivel metería un campo de 5 m en
`IsSuitableForFixed` o en las obras. Todo va en float (FPU a 24 bits, fn_007DEE00), sin double ni FMA.

| API (`ecs::object`) | Original | Qué hace |
|---|---|---|
| **Nivel de malla** | | |
| `HalfExtents(box)`, `MeshHalfExtents(meshId)` | +0x24/+0x28/+0x2C, 0x80831C..0x80835B | `(max − min) × 0,5`; sin malla, nada |
| `HalfDiagonal(half)`, `MeshHalfDiagonal(meshId)` | +0x30, 0x80835E..0x808379 | `√((hz² + hy²) + hx²)`, en ese orden |
| `Radius2D(half, s)`, `MeshRadius2D(meshId, s)` | 0x6381B1..0x6381E2; en línea en 0x603E1E, 0x604042, 0x6E956A, 0x6EAC22, 0x7350A5 | `s × max(hx, hz)` (el `fcompp` toma hx si hz < hx) |
| `Height(half, s)`, `MeshHeight(meshId, s)` | 0x638136..0x63813D; en línea en `Tree::Draw` 0x74ABA2..0x74ABC0 | `2 × (hy × s)` (`fmul` y luego `fadd st0, st0`) |
| `MeshHalfHeight(meshId)` | 0x5287C5, 0x7FB7D9, 0x74ABB0 | +0x28, sin escala |
| **Nivel de objeto** | | |
| `GetScale(e)` | vt+0x120: Object 0x402520 = el campo +0x50; Creature 0x47B190 → `GetUserSize` 0x4EF4F0 | la escala uniforme del `Transform` (x); la de un `MapShield` es su `objectScale` (`SetScale` 0x639200), no la dibujada. La de la criatura es la del `Transform` **(inferido)** |
| `GetScaleField(e)` | el campo +0x50 que lee `GetHeight` (0x638139) | no la virtual |
| `ObjectGet2DRadius(e)` | `Object::Get2DRadius` 0x638180 en sí (la llama directa `PileFood` en 0x66F192) | `GetScale × max(+0x24, +0x2C)`; sin malla 0 (0x6381E9) |
| `ObjectGetHeight(e)` | `Object::GetHeight` 0x638120 en sí | `2 × +0x28 × [+0x50]`; sin malla 0 (0x638140) |
| `Get2DRadius(e)` | vt+0x64 | Field 0x528E80 y FishFarm 0x52C470 = **5** [0x8AB6E4]; MagicTeleport 0x5FCCB0 → 0x5FCCA0 = **6** [0x92C108]; MagicFireBall 0x682D20 = `GetScale × 1` [0x935910]; PileFood / MagicFood / PuzzleGrain 0x66F180 = `GetProportionRaised × Object::Get2DRadius`; Creature 0x477F40 (sin portar: ver Pendiente); el resto, 0x638180 |
| `GetRadius(e)` | vt+0x60: Object 0x638110 = `jmp [vt+0x64]` | igual que `Get2DRadius` (Creature 0x4792C0 repite su lectura) |
| `GetHeight(e)` | vt+0x42C | MagicFireBall 0x682D30 = `jmp [vt+0x64]`; Creature 0x477F50 = tamaño × **15** [0x8C2C40] (el tamaño, la escala del `Transform`: **(inferido)**); el resto 0x638120 (Field, FishFarm y PileFood **no** la cambian) |
| `GetTopPos(e)` | vt+0x630: Object 0x638160; MapShield / MagicShield / PhysicalShield 0x72C1C0 = **0** | `altitud (+0x1C, sobre el suelo) + GetHeight` |
| `GetHeightForHandAboveInteractObject(e)` | vt+0x64C: Object 0x638150 = `jmp [vt+0x42C]`; FishFarm 0x52C840 = **5** [0x8AB6E4] | |
| `GetMeshRadius(e)` | vt+0x568: Object 0x636BD0 = +0x30 sin escala; Field 0x528A30 / FishFarm 0x52C480 = 5 | |
| `PileFoodProportionRaised`, `PileWoodProportionRaised`, `GetProportionRaised(e)` | vt+0x86C: PileFood 0x66EB60, PileWood 0x66F1B0 | ver abajo |
| **Derivadas** (rutinas propias encima de la API) | | |
| `GetHoldRadius(e, above)` | Object 0x638C00: ABOVE (`GetHoldType` = 1) → `GetHeight × 0,75` [0x8AB274], si no `Get2DRadius`; Tree 0x74B610 / DeadTree 0x5110E0 = `Get2DRadius × 0,2` [0x8AB244] | el tipo de agarre lo sabe la mano; SpellSeed 0x728640 (`GetScale × info+0x150`) lo pone quien llama |
| `GetDefaultFireRadius(e)` | Object 0x639AC0 = `jmp [vt+0x64]`; DeadTree 0x510E10 = `GetHeight × 0,35` [0x8D6974]; WorshipSite 0x77DE10 → 0x77DDD0 = **14** [0x99C9EC] | |
| `GetVillagerHugRadius(e)` | Object 0x4026B0 = `Get2DRadius × 1,05 + 0,0005` [0x8AA3A0] [0x8AA39C]; Tree 0x74A1A0 = `min(Get2DRadius × 0,1, 0,25)` [0x8AB22C] [0x8AB3D4] | |
| `GetRoutePlanRadius(e)` | vt+0x7C4: Object 0x6384C0 sin criatura = `Get2DRadius` (0x6384CF); Tree 0x74A140 (copia de 0x74A1A0); CitadelHeart 0x4680C0 = `Get2DRadius × 0,33` [0x8CA268] (en openblack, `Temple`) | la rama con criatura, sin portar |
| `GetDistanceFromObject(a, b)` | vt+0x6C4: Object 0x637FB0; WorshipSite 0x77DE20 | `GetDistanceInMetres − (R2D(b) + R2D(a))`; el lugar de culto mide desde `CalculateCentrePos` 0x77DD40 y resta `14 + R2D(b)` (`GetRealRadius` 0x77DDD0, 0x77DE36..0x77DE60) |
| `GetDistanceFromObject(a, punto)` | vt+0x13C: 0x5702B0 (Object 0x4027C0 la llama) | `GetDistanceInMetres − GetRadius`. Ninguna clase `Object` la redefine |
| `IsTouching(a, b, m)`, `IsTouching(a, punto)` | 0x637E00 (`≤ m`), 0x637E30 (`≤ 0`) | por las dos de arriba, con sus redefiniciones |
| `GetBoundingSphere(e)` | vt+0x798: Object 0x637730; Living 0x5ED2F0; MobileStatic 0x608F40 | `h = GetHeight × 0,5`; `r = √(R2D² + h²)`; centro = el del MapCoords con `y = (GetAltitude + altitud) + h`. El suelo es el de la isla (`LandIsland::HeightAt`, la U3 de «sistemas», por `map_coords::ToWorld`). Living (aldeanos, animales) y MobileStatic (rocas, árboles muertos y talados, hogueras, fragmentos, piedras de teletransporte) usan `R2D × 0,5` (0x5ED30D / 0x608F5D). Creature 0x479970 → `LH3DCreature::GetBoundingSphere` 0x47F8D0, sin portar: usa la de `Object` **(inferido)** |
| `WorshipSiteCentre(e)` | `WorshipSite::CalculateCentrePos` 0x77DD40 | `derecha × 12,55 [0x99C9E8] − delante × 26,1 [0x99C9E4] + posición`, por componente (0x77DD61..0x77DDB1). La matriz es la del `Transform` del lugar **(inferido**: `[this+0x40]+0x14`, como ya leía `WorshipScore`, que ahora la llama) |

**GetProportionRaised** (0x66EB60, comida): `p = cantidad / maxAmountInPot` (`fild` de 64 bits sin signo, `fidiv`);
p < 0 → 0 sin suelo; p > 1 → 1; **p = 0 se queda en 0** (0x66EBB7..0x66EBC2); si no, `p = (1 − 0,05)·p + 0,05`
[0x933014]. Devuelve `1 − (1 − p)²` recortado a 0..1. La de la madera (0x66F1B0) aplica el suelo si p > 0 y recorta, sin
el cuadrado. Con `maxAmountInPot = 0` el original da inf (→ 1) o NaN (→ 0), lo mismo que dividir por 1.

**Qué se arregló** (los 8 arreglos de fidelidad del plan, §4):
1. **Field = 5 m** en el fuego (`fire::traits`, y con él FireEffect, FireGraphic, Explosion y VillagerFire), los
   animales (`GetWorkingPos` 0x639550 usa vt+0x60), el pueblo (`CheckForClearArea` 0x741457), la curación (Heal), los
   bosques (`AddTreeAround` 0x439220, vt+0x60) y SpellFlock / SpellWater (que ya lo tenía aparte).
2. **FishFarm = 5 m**: no estaba en ningún sitio.
3. **PileFood × proporción** en todas las consultas de objeto (antes solo en `pot_resource`).
4. **MagicFireBall** (radio y altura = escala) fuera del fuego: Heal y todo lo que pasa por la API.
5. **La pila vacía da 0**, no 0,0975 (`PotResource.cpp`, y su test `test_food_wood`).
6. **El campo se hundía el doble**: `Fields.cpp` guardaba la altura entera; el original suma `2·v·escala·[m+0x28]` con la
   **semi**altura (0x5287C2..0x5287D1). Ahora es la semialtura del nivel de malla.
7. **Sin malla, 0**: la mano (0,5 / 1,0), la pila al hundirse (1,0) y el campo (1,0) daban tamaños inventados.
8. **El MapShield con una sola escala**: `GetScale` de un escudo es su `objectScale` en todas las consultas.

Además: `Trees.h` citaba `ComputeBoundingBox` en 0x808180 (es **0x8081B0**). `Tree::Draw` (la copa al doblarse,
0x74ABA2..0x74ABC2) se porta al nivel de malla; la altura del árbol más alto (fn_0053A740, 0x53A75D), el árbol talado
(0x5116C2) y el susurro de más de 10 m (0x74B1CF) al de objeto. `GetDefaultFireRadius` de un lugar de culto es 14 m.

Migrados al nivel de objeto: `EffectValues` (ver abajo), `FireObjectTraits`, `PotResource`, `PotArchetype::SetSize`
(0x66E90A / 0x66E918), `SpellWater`, `SpellFlock` (`fn_006D0C20` con vt+0x60), `Heal` (incluida la escala de regla,
vt+0x64 en 0x6A0DC3), `OneOffSpellSeed` (el adelanto del Z-sorter, vt+0x60), `SpellDispenser` (0x722B46),
`TestDispensers`, `TownQueries`, `Trees`, `Rocks` (y con él `LanternSounds` y la física de rocas), `AnimalFlee`,
`AbodeArchetype` (0x40327E / 0x40329A) y `HandHolding::ComputeHoldParameters` (la altura por vt+0x42C, la de la semilla
con la malla de su info por `Object::GetHeight` 0x638120; el radio por vt+0x64 como `Object::GetHoldRadius`
0x638C22..0x638C26, así que una pila de comida cogida, también la HandFood de la mano, lleva su `GetProportionRaised`).
`HandSystem::Update` ya no repite la proporción con 1600 fijo: solo vuelve a pedir los parámetros cada fotograma
(info.dat: HandFood es potType 1 = PileFood con `maxAmountInPot` 1600, así que solo cambia la mano vacía, que ahora mide
0). Al nivel de malla: `Fields` y la copa de `Trees`.

`effects::ObjectHeight` / `Object2DRadius` quedan como envoltorios de `ObjectGetHeight` / `ObjectGet2DRadius` (la rutina de
`Object` **sin** redefiniciones, como hacían) solo para los llamadores aplazados.

## Reloj del juego

✅ Fiel y portado en `src/GameClock.{h,cpp}`, namespace `openblack::game_clock` (sesión «sistemas2», 2026-10-02).
Game lo mueve: lo pone en marcha en `LoadMap`, decide los turnos en `Update` y calcula el reloj del fotograma justo
después. El original **no tiene una función «dame el tiempo»**: `GGame::Loop` 0x54CF20 calcula el reloj una vez por
vuelta y lo deja en campos de GGame que cientos de lectores leen en línea (379 referencias al turno, 157 a
`g_game_time_inc`, 29 a la fracción). openblack lo tenía escrito unas 37 veces, con fidelidades distintas.

**El temporizador de la partida** es un `LHTimer` en g_game +0x205D68 (+0x100 base, +0x104 ms acumulados, +0x108
factor de velocidad, 0 = parado, +0x10C factor guardado). `MSeconds` 0x43EB70 = `ftol((GetTickCount − base) · factor +
acumulado)`; `Stop` 0x43E9C0 acumula y pone el factor a 0; `SetSpeedUpFactor` 0x43EBC0 rebasa si está en marcha y, si
está parado, solo guarda el factor. Para arrancarlo (inicio de Loop 0x54CF93, `ResetLocalGameTimer` 0x54C690, quitar
la pausa) se pone el factor a 1e-5 (0x3727C5AC) y luego `SetSpeedUpFactor(guardado)`: así se rebasa y **el tiempo
parado no cuenta**. Todo en float (FPU a 24 bits).

**Cuándo hay turno** (`LocalTimerSaysDoATurn` 0x54C4A0, llamado desde `ProcessNetworkPackets` 0x54CD45):
- toca cuando `MSeconds ≥ turno · 100`, con el 100 escrito a mano (0x54C4F0). La comparación es **absoluta**, así que
  el sobrante de un turno pasa al siguiente;
- en pausa (un jugador) nunca (0x54C528);
- con más de 2000 ms (0x7D0) de retraso llama a `ResetLocalGameTimer` 0x54C570, que pone el temporizador en
  `turno · 100` desde ahora (0x54C615). La respuesta de esa llamada es la de la muestra de antes (`setge` 0x54C567);
- como mucho **1 turno por fotograma** en un jugador (10 en red): `neg; sbb; and 9; inc` en 0x54CD0F..0x54CD18. El
  bucle pregunta primero al temporizador y luego el tope (0x54CD52), así que tras el último turno del fotograma el
  temporizador se consulta una vez más.

**El turno** g_game +0x205A40 sube **al empezar** el turno y solo sin pausa (`GGame::StartTurn` 0x54E4FD..0x54E507),
antes de `ProcessTurn` 0x54E5C0 y `EndTurn` 0x54E960. Durante el turno todo el juego lee ya el número nuevo.

**El reloj del fotograma** (`GGame::Loop` 0x54D2A8..0x54D3A6, después de los turnos y antes de dibujar):
- sin pausa: `Δ = MSeconds − muestra anterior`; con el mismo turno `resto += Δ`; con turno nuevo
  `resto += Δ − 100` (0x54D316); luego `resto` se limita a 0..99 (0x54D325..0x54D337);
- `visual = turno · 100 + resto` (0x54D343); si es menor que el anterior, se guarda ese valor menor (0x54D350: el reloj
  **sí va hacia atrás**) y `resto = 0` (0x54D356); lo que nunca es negativo es el dt, que ese fotograma vale 0;
- `g_game_time_inc` [0xEA9EC0] = g+0x250540 = g+0x205D48 = `visual − anterior` (0x54D366/0x54D374/0x54D380): **ms
  enteros**, como mucho 199, que siguen la velocidad;
- la **fracción** g+0x205D64 = `resto · 0,01` [0x8C4B10] (0x54D392): va de 0 a 0,99 y va un turno por detrás;
- en pausa solo el dt vale 0 (0x54D39A); **la fracción se conserva**;
- `NetworkTurnsThisFrame` vuelve a 0 después de dibujar (0x54D3C3).

**El reloj de pared** `g_delta_time` [0xC38134] es otro `LHTimer` (`LH3DTech::g_timer` 0xEA1B78), leído en
`LH3DRender::StartFrame` 0x82F14E: ms del fotograma, 1 si sale ≤ 0 (0x82F195), y no se para en pausa. Su constructor
estático (fn_008189F0, en la tabla `__xc_a` en 0x9C7D60) lo deja **parado** (velocidad 0, guardada 1);
`LH3DTech::RenderInitialization` 0x818C61..0x818CA3 (llamada por `LH3DRender::Open` en 0x82B540) lo arranca: factor
1e-5, `elapsed = MSeconds` (≈ 0), base = `GetTickCount`, factor = el guardado (1). Cuenta, pues, los ms **desde que
arranca el motor**, no desde que arranca la máquina. `SetSpeed` 0x5537F0 pone además [0xD00DA8] = 0 por las dos ramas
(0x5538AD, 0x5538C8); esa dirección solo se escribe en todo el exe (también en `GNetwork::ProcessOnePacket` 0x634B40),
nadie la lee: no se porta **(inferido: no se ha visto ningún lector indexado)**.

| API (`game_clock`) | Original | Qué hace |
|---|---|---|
| `k_MsPerTurn` = 100, `MsPerTurn()`, `SetMsPerTurn()` | [0xD01A38]: `GGame::Init` 0x54F4A5, `SET_GAME_TICK_TIME` 0x714DBE | los ms del turno que lee la lógica |
| `k_SchedulerMsPerTurn` = 100 | literales 0x54C4F0, 0x54D316, 0x54D343, 0x54C615, 0x5550A3, 0x553810 | el 100 del planificador y del reloj del fotograma (no lee [0xD01A38]) |
| `k_TurnSeconds` = 0,1f | `push 0x3DCCCCCD` en `ProcessTurn` 0x54E5D1, 0x54E6C3, 0x54E775 | los segundos de turno que se pasan a mano |
| `k_MaxLagMs`, `k_MaxTurnsPerFrame` | 0x54C553, 0x54CD0F | 2000 ms; 1 turno por fotograma |
| `Timer` (`MSeconds`, `Stop`, `SetSpeedUpFactor`, `Start`) | LHTimer 0x43EB70 / 0x43E9C0 / 0x43EBC0; arranque 0x54CF93 | el temporizador |
| `Turn()`, `SetTurn()` | g+0x205A40 | el turno |
| `TimerSaysDoATurn()`, `TurnDue()`, `StartTurn()`, `ResetLocalTimer()` | 0x54C4A0, 0x54CD45, 0x54CD93 + 0x54E507, 0x54C570 | el planificador |
| `Start(paused)` | Loop 0x54CF6B..0x54D003 y 0x54D1F7 | temporizador desde 0, dt y fracción a 0, `ResetLocalGameTimer` |
| `OnLoad()` | `ResolveLoad` 0x555080 | dt y fracción a 0, `visual = turno · 100` (los estáticos de Loop no se tocan) |
| `Pause(bool)`, `IsPaused()` | `PauseGame` 0x54AE20 (0x54AE7C..0x54AEE7) | la bandera g+0x14 bit 2 y el temporizador parado / rearrancado |
| `SetSpeed(v)`, `Speed()` | `GGame::SetSpeed` 0x5537F0 (0x553800; `SetSpeedUpFactor` en línea en 0x553835) | el factor de velocidad; el tiempo ya pasado se queda con la velocidad de antes |
| `UpdateFrameClock()` | `GGame::Loop` 0x54D2A8..0x54D3A6, 0x54D3C3 | resto, reloj visual, dt y fracción |
| `FrameGameMs()`, `FrameGameSeconds()` | [0xEA9EC0]; `· 0,001` [0x8AA3B0] | ms enteros de juego del fotograma |
| `TurnFraction()` | g+0x205D64 | la fracción del turno |
| `VisualMs()` | g+0x25053C | el reloj visual |
| `StartEngineTimer()` | `RenderInitialization` 0x818C61..0x818CA3 (`Reset()` lo deja parado como fn_008189F0) | arranca el reloj de pared desde ≈ 0; lo llama `InitializeEngine` (Locator.cpp) al crear el renderer |
| `UpdateRealClock()`, `FrameRealMs()`, `EngineMs()` | `StartFrame` 0x82F14E..0x82F195; `g_timer` 0xEA1C78..0xEA1C80 | el reloj de pared |
| `CameraFrameMs(playingBack)` | `GetCameraTimeInc` 0x555820 | dt de juego al reproducir la interfaz grabada, si no el de pared |
| `ClampedFrameMs(inTemple)` | fn_005557E0 | pared en el templo, de juego fuera; ≤ 0 → 0, tope 500 |
| `TicksForSeconds(s)` | `ftol(1000 / [0xD01A38] · s)` (división entera): `NumGameTicksPerSecond` 0x711630 y en línea en 0x70CCDE, 0x711338, 0x5C61F6 y `GetTicksToChangeOver` 0x66CD00 | segundos → turnos; con [0xD01A38] = 0 el original fallaría en el `div` (0x711635, sin comprobar): openblack da 0 **(inferido)** |

Ojo con el nombre de 0x711630: en realidad es el `SetTime` de un temporizador del guion (guarda el turno en +0x28 y los
turnos en +0x2C, fn_00711610); la conversión es la misma que la de las otras cuatro.

Velocidad de openblack: `Game::SetGameSpeed(m)` sigue recibiendo el multiplicador de la duración del turno (2 lento,
0,5 rápido) y llama a `SetSpeed(1 / m)`.

**Lo que se arregló** (antes cada cosa llevaba su reloj):
- **El sobrante del turno.** `GameLogicLoop` hacía un turno si habían pasado 100 ms desde el anterior y ponía la marca
  en el fotograma del turno: a 30 fps los turnos duraban 133 ms (un 25 % más lentos). Ahora 3 s a 33 ms por fotograma
  dan 30 turnos.
- **El turno sube al empezar.** `_turnCount` subía al final; durante el turno se leía uno menos que en el original.
- **La pausa.** No paraba el reloj: al quitarla hacía un turno en el mismo fotograma y la fracción saltaba a 0,99. La
  fracción valía 0 en pausa; ahora se congela.
- **El orden del fotograma.** Los turnos van antes que el reloj del fotograma y que todo lo que se mueve por
  fotograma (aldeanos, animales, tiburones, barco, anillos, peces, luciérnagas, la pantalla ancha, la magia), como en
  `GGame::Loop`. Las diez copias de `_paused ? 0 : dt / mult` de Game.cpp leen `FrameGameMs()` / `FrameGameSeconds()`:
  ms enteros, 0 en pausa, ≤ 199.
- **El mar.** `RendererSea.cpp` tomaba como dt el tiempo desde el arranque (`desc.time`, que no se rebasa nunca) y
  `ScrollRows` lo acumulaba otra vez: el mar se desplazaba cada vez más deprisa. Ahora lee `g_game_time_inc`
  (0x879963, 0x87A130).
- **Fracciones propias.** HandGrain (0x5B2D41/0x5B2D61), el PSys (0x67370D) y las luciérnagas (0x52ADF6) usaban el
  reloj de pared o uno propio, sin velocidad ni pausa y con tope 1. Ahora leen `TurnFraction()`.
- **Relojes propios.** HandFish y HandResources (`ProcessInInteract` una vez por turno) contaban turnos con el dt real
  de la mano; ahora cuentan los turnos del juego. Los fragmentos de los edificios rotos (`Fragment::ProcessTimer`) van
  con el turno, y el dt de la física es el de juego. TownBelief usaba el reloj de pared donde el original suma
  `g_game_time_inc · 0,002` (0x69D855). PetitNavire (`g_carry`) y los tiburones (`s_Clock`) reconstruían los ms
  enteros: ya llegan enteros.
- **Conversiones.** Los dispensadores (0x70CCDE, 0x711338), el final de los textos de ayuda (0x5C61F6) y la rampa de
  coger de un montón (0x66CD00, antes en float y sin truncar) usan `TicksForSeconds`. Cánticos y ayuda leen
  `MsPerTurn()`. `SET_GAME_TICK_TIME` (antes lanzaba una excepción) escribe [0xD01A38] y nada más.
- **Las copias del turno** (`reactions::Turn`, `magic::CurrentTurn`, la de la bola de fuego, que no leía nadie, el
  clima y su bucle, los árboles, los animales, los aldeanos y el humo de chimenea, que no miraba si había Game) leen
  `Turn()`. `magic::k_TurnMs`, `Chants.h`, `DayNightClock.cpp`, `FireFlies.cpp`, `ChimneySmoke.cpp` y `HelpSystem`
  toman sus constantes del reloj.
- **El reloj de los textos de ayuda** (`queries.nowMs`) era un reloj propio aproximado; ahora es `EngineMs()`, el
  `g_timer` que lee el original en 0x5C6250.

**Arreglos de la auditoría** (2026-10-02):
- **El reloj de pared no se arrancaba.** `engineTimer` se quedaba con base 0 y velocidad 1: `EngineMs()` y
  `UpdateRealClock()` daban los ms de `steady_clock` desde su época (el arranque de la máquina). En float, con más de
  4,6 h de máquina encendida (2^24 ms) los ms iban de 2 en 2 (con días, de 32 en 32: el reloj de los textos y
  `FrameRealMs` se cuantizaban), y con más de 24,8 días el paso a int32 se desbordaba. Ahora `Reset()` lo deja parado
  como fn_008189F0 y `StartEngineTimer()` (RenderInitialization 0x818C71) lo arranca desde ≈ 0.
- **El contador de fotogramas del mar** [0xFA938C] (`(frame + 1) & 15`) avanzaba sin pausa; el original lo avanza solo
  si `g_game_time_inc != 0` (0x879B0A / 0x879B41).
- **Direcciones y textos.** `++NetworkTurnsThisFrame` está en 0x54CD93 (en 0x54CE58 está la llamada a
  `ProcessOneGameTurn`). El reloj visual **sí** puede ir hacia atrás (0x54D350); lo que no es negativo es el dt.
- **`atmos::UpdateGame`** (WeatherLoop.cpp) lleva `k_TurnSeconds` (0x54E5D1) en lugar del 0,1f a mano.
- **[0xD01A38] en tiempo de ejecución.** Las luciérnagas (`FireFly::Process` fn_0052AF90, 0x52AF93: `fild [0xD01A38];
  fmul 0,001`) y el viento de la mano del humo de chimenea (fn_005DBC60 0x5DBD4E: `1000 / [0xD01A38]`) leen la
  variable cada turno: ahora `MsPerTurn()` (antes la constante `k_MsPerTurn`, y `SET_GAME_TICK_TIME` no les llegaba).
  `Chants.h` solo tiene el valor por defecto; `Spell.cpp` ya lo rellena con `MsPerTurn()`.
- **HelpSystem en float.** Los segundos de lectura (0x5C6211..0x5C6225) y `ShownLongEnough` (fn_005C68C0) se
  calculaban en double; ahora en float (la FPU a 24 bits).
- **Los fragmentos** (`Fragment::ProcessTimer` 0x76EAF0) los llama `GGame::ProcessTurn` en 0x54E768: ahora van desde
  `Game::GameLogicLoop` y no desde la física con un `static` del turno (que no se reiniciaba al cargar un mapa).

**Cambios que se ven y hay que comprobar con captura:** a 30 fps la partida va un 25 % más deprisa (los turnos duran
100 ms); el primer turno se juega en el primer fotograma; al quitar la pausa no hay salto; con la pausa puesta los
aldeanos y animales se quedan donde estaban (antes volvían a la posición del principio del turno); el mar se desplaza a
velocidad constante; las pilas que se recogen de piscifactorías, campos y montones siguen la velocidad del juego y se
paran en pausa; el grano de la mano, las luciérnagas y los efectos de partículas interpolan con la fracción del juego
(se paran en pausa y siguen la velocidad); los símbolos de creencia de los pueblos se paran en pausa.

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

### Tamaño de los objetos

Estado a 2026-10-01, rama `local/sistemas2`. La regla ha sido migrar **solo** donde se ha leído qué nivel usa el
original (la llamada virtual o la lectura en línea).

**Aplazadas, porque milagros2 está editando esos archivos** (usan todavía `effects::Object2DRadius` / `ObjectHeight`,
la rutina de `Object` sin redefiniciones):
- `PSys/Rules/Storm.cpp:1265, :1336` → `object::Get2DRadius` (vt+0x64).
- `PSys/Rules/Lightning.cpp:126` → `object::GetHeight` (vt+0x42C).
- `Magic/Spells/SpellForest.cpp:169` → `object::Get2DRadius` (el comentario de :161 dice que Field 0x528E80 no está
  portado: ya lo está).
- `Magic/Core/SpellSeed.cpp:137, :143` → `object::GetHeight` / `Get2DRadius` (falta leer el original).
- `Magic/Objects/MapShield.cpp:526-547` (`map_shield::Get2DRadius` / `GetHeight`, ya con `objectScale`) →
  `object::Get2DRadius` / `GetHeight`; `CollisionScale` (:568) es `object::GetScale`.
- `Magic/Objects/MagicTeleport.h:31` (`k_Radius = 6`) → `object::k_MagicTeleportRadius` / `Get2DRadius`.
- `PSys/Rules/Explosion.cpp:392, :425, :504` usa `fire::traits::Radius`, que ya es la API; no hace falta tocarlo.
- Cuando se muevan todos, se borran los dos envoltorios de `EffectValues`.

**De «sistemas» (tiene los archivos abiertos):**
- `HandPlacement.cpp:409-416` (pila bloqueada, 0x5B3EA8: 1,0 sin malla), `:683-690` (radio del ser vivo bajo la mano) y
  `:708-716` (radio de lo sostenido).
- `HandTrees.cpp:202-207` (el tronco del árbol talado: `0,2 × Get2DRadius` = `GetHoldRadius` de Tree, 0,3 sin malla) y
  `:331-341` (el polvo de las raíces: otra fórmula, semiejes sin escala).
- `3D/Foliage.cpp:506-508` (mod `world.foliage`): no porta nada del original, pero debería usar `object::Get2DRadius`
  (con el campo de 5 m).

**De «audio» (hito B11):** `Audio/LanternSounds.cpp:92, :131` llaman a `Rocks::Height`, que ahora es
`object::GetHeight`: el valor ya es el de la API; solo falta llamar a la API directamente.

**PLAUSIBLES sin cerrar, no tocados:**
- `Physics/PhysicsObjects.cpp:282-284` (`SetUpBody`, R5/H4): el original mezcla la semialtura en línea de
  `PhysOb::Initialise` 0x7FB7D9 (`escala·[m+0x28]·1000`) con vt+0x42C (`SetUpPhysOb@Villager` 0x5F0007).
- `HandHolding.cpp:187-192` (coger un árbol: `maxima.y`, no max − min) y `PSys/TownBelief.cpp:191` (`maxima.y` del
  centro del pueblo): falta leer `UR_TownCentreBelief` 0x69C17A.
- `ComputeHoldParameters` sigue con su propia tabla de tipos de agarre en vez de `object::GetHoldRadius` (la tabla de
  la mano trae también la bajada y el tipo; los radios ya son los de la API).
- MagicFireBall en la física y en Storm: depende de si la bola es una entidad con `Mesh` en esas consultas.

**Dudosas, no migradas** (no consta qué hace el original en ese sitio):
- `HandHolding.cpp` (el anillo de agua de lo lanzado, `0,5 × |Size| × escala`, 1 sin malla) y
  `Graphics/PhysicsShadows.cpp:155` (la misma semidiagonal × escala): en el original el anillo usa el campo +0x178 del
  `PhysicsObject` (0x6466AA: `1 / r` y `2 r`), que viene de su inicialización; no se ha leído de dónde sale.
- `Physics/PhysicsObjects.cpp:302, :308` (`rockHalfHeight = 0,5 × Size().y`): ¿es la semialtura en línea de
  `PhysOb::Initialise` 0x7FB7D9 (`MeshHalfHeight`, sin escala)? Sin comprobar.
- `HandTrees.cpp:375, :409` (`0,5 × Size().x`): sin dirección (y el archivo es de «sistemas»).
- `Physics/PhysicsObjects.cpp:554-563` (`Radius2D`, usado en :638 y :1227): la fase ancha de openblack, sin dirección.
- `PSys/TownBelief.cpp:183-196` (la altura de la cima del tótem): sin dirección.
- `ECS/FireFlies.cpp:98-108` (`MeshHeight` + 2 de casas y farolas): fn_0052B1D0 solo es el filtro (`IsAbode` /
  `IsStreetLight`); no se ha leído dónde se suma la altura.
- `Physics/PartialBuild.cpp:148` (el corte de la obra): está en fn_00816AD0, sin leer.
- `ECS/Trees.cpp:1104-1114` (`MeshHalfDiagonal` de las fuentes que doblan árboles): escala × +0x30, y `GetMeshRadius`
  0x636BD0 no lleva escala; sin dirección.

**Revisión de milagros2 (2026-10-01):** cambios correctos que salen de la API y no estaban declarados: PileFood ×
proporción (0x66F180) y MagicTeleport = 6 (0x5FCCB0) cuentan ya en el fuego y en el agua, y el MapShield se mide con su
`objectScale`. El centro del fuego de un WorshipSite es `GetDefaultFireCentrePos` 0x77DDE0 (= `CalculateCentrePos`
0x77DD40, altitud sobre la tierra por `Set` 0x603340), junto a su radio de 14 m (0x77DE10). Siguen con
`effects::Object2DRadius` / `ObjectHeight` (aplazados de milagros2): SpellForest.cpp:169/193, Storm.cpp:1265,
Lightning.cpp:126 y SpellSeed.cpp:137/143.

**Sin portar:**
- `Creature::Get2DRadius` 0x477F40 / `GetRadius` 0x4792C0 leen el LH3DCreature (`[[+0x160]+0x58]+0x5228`), que openblack
  no tiene: por ahora una criatura usa la fórmula de `Object` **(inferido)**. La altura (0x477F50 = tamaño × 15) sí está,
  tomando la escala del `Transform` como el `GetUserSize` 0x4EF4F0 **(inferido)**.
- La rama con criatura de `GetRoutePlanRadius` 0x6384D8 (necesita `NavRadius` 0x480A60).
- `Creature::GetBoundingSphere` 0x479970 (`LH3DCreature::GetBoundingSphere` 0x47F8D0): la criatura usa la de `Object`
  **(inferido)**.
- Las clases que no son `Object` (Citadel, SpellShield, SpellStormAndTornado, Town, GArena, Reaction, BuildingSite,
  AtomCore, GStreetLight, Mist: ver arriba) no pasan por la API; si alguna llega a pedirla, hay que añadir su rama.
- `GetNearestPosOfObject` 0x636D30 (necesita `Get3DAngleFromXZ` 0x74D270 y `GetPosFromAngle` 0x74D580 en `gutils`; no
  hay llamador).
- Los otros `GetScale`: `ShowNeedsVisuals` 0x55DD80 (+0x58), `PlannedMultiMapFixed` 0x4050C0 y `SpellSeedGraphic`
  0x727340.
- Los sitios del nivel de malla que openblack aún no tiene (`IsSuitableForFixed` 0x603E1E, `Scaffold` 0x6E956A /
  0x6EAC14, 0x7350A5, `PhysOb::Initialise` 0x7FB7D9...): la API está lista para ellos.
- La caja de una malla animada: el original la calcula después de `LH3DAnim::SetTransform` 0x83A1D0 (bit 0x100);
  openblack une las cajas de los vértices tal cual. Aldeanos y animales podrían tener semiejes algo distintos (sin medir).

**Cambios que se ven y hay que comprobar con captura:** el campo se hunde la mitad al vaciarse (con el mod de plantas
apagado); un campo o una piscifactoría miden 5 m para el fuego, el pueblo (dónde caben los edificios), los animales y los
milagros; una pila de comida mide según lo llena que está (y vacía, 0) para el fuego y el pueblo; un lugar de culto
ardiendo usa 14 m; la altura de una criatura para el fuego y la curación es 15 × su escala; una pila de comida cogida
del mapa (PileFood, MagicFood, PuzzleGrain) abre la mano según lo llena que está, y la HandFood vacía la cierra del todo.

### Reloj del juego

Estado a 2026-10-02, rama `local/sistemas2`.

**Aplazadas, porque milagros2 está editando esos archivos:**
- `Magic/Objects/MapShield.cpp:58, :387, :408-409`: su fracción propia (`g_LastTurn`, reloj de pared, tope 1) →
  `game_clock::TurnFraction()` (`PhysicalShield::DrawShield` 0x72CEEC/0x72CF01 es un lerp con la fracción).
- `PSys/Rules/Explosion.cpp:82, :262`: el `k_BeamFxTurns · 0,1f` → `game_clock::k_TurnSeconds` y el
  `ftol(1000/[0xD01A38]·4)` del comentario → `TicksForSeconds(4)`.
- `ECS/Systems/Implementations/HandSpellSeed.cpp:194-197`: el envoltorio `CurrentTurn()` → `game_clock::Turn()`.

**Abiertos por «sistemas»** (no imprescindibles; se dejan para cuando fusione):
- `Magic/MagicLoop.cpp:117, :123`: `k_TurnMs · 0,001f` → `game_clock::k_TurnSeconds` (`MusicMood`/`SpellSounds` y
  `CHand::GameTurnUpdate`; falta leer si el original lee ahí [0xD01A38] o el 0,1f a mano).
- `ECS/Fire/FireGraphic.cpp:91, :510`: el `g_Turn` que pone `graphic::SetTurn` → `game_clock::Turn()` (ya recibe el
  mismo valor).
- `Worship/SpellSeedGraphic.cpp:479`: el turno leído en línea.
- `Graphics/Renderer.cpp:1030` (brillo del sol, reloj de pared sin pausa ni velocidad: qué dt usa el original es
  **(inferido)**), `Renderer.cpp:1157-1162`, `night_lights::Update` en `Renderer.cpp:1214` (el mismo `milliseconds`
  de pared con tope de 100 y `IsPaused() ? 0`; falta leer qué dt usa fn_005E5830), `RendererSmoke.cpp:99-105` y `RendererMists.cpp:147` (este, de
  milagros2): sus `static lastTime` con tope de 100 ms → `FrameGameMs()` (el humo fn_007F8E00 recorta a 100 **s**).
- `Game.cpp:610/612` (campos y árboles con dt real): **(inferido)**, sin leer en `Field::Draw` 0x5286D7 ni en
  `Tree::PreDraw`; si es `g_game_time_inc` (0x5286D7 lo lee) hay que pasarles `FrameGameSeconds()`.

**De audio (hito B11):** `Audio/SoundTags.cpp:145` (`k_MsPerTurn` local, marcado «(inferred)»: es [0xD01A38],
0x54F4A5) → `game_clock::MsPerTurn()`; la copia doble de `audio::TickCount` / `MusicStream` → `game_clock::TickCount()`.

**Sin portar o dudosos:**
- **La física por turno.** `PhysicsObject::GameTurnUpdate` (0x646046) hace los 20 subpasos dentro del turno. openblack
  los reparte entre los fotogramas con un acumulador (`PhysicsObjects.cpp:1317`), ahora con el dt de juego. Pasarlos
  al turno pide dibujar los objetos interpolados con la fracción (fn_00646FE0 0x647096 lo hace con sus reflejos).
- **TownBelief** avanza fase, ángulos y temporizador de pelea con un paso fijo de 0,1 por **fotograma**
  (`k_Step`, `TownBelief.cpp:53`): debería ser por turno o con el dt, falta leer `PlayerSymbolSprite` /
  fn_0069D3D0.
- **La cámara** va con el dt del perfilador (µs reales); el original elige con `GetCameraTimeInc` 0x555820
  (`CameraFrameMs()`, ms enteros de pared). No se ha cambiado.
- `PSysManager.cpp:242` (`seconds · 1000 / [0xD01A38]`, ahora con `MsPerTurn()`): no se ha leído en qué orden redondea
  el original al crear un efecto visual puntual; se deja la fórmula.
- `CHLApi.cpp:802` `DllGettime` (029 DLL_GETTIME) sigue vacía: falta leer qué empuja.
- `Help/HelpSystem.cpp`: `ReadSpeedFactor` (fn_005C6CB0, devuelve double; la prueba `test_help_system` lo compara
  en double) y `_endMs = segundos · 1000 + ahora` (0x5C6279..0x5C629B, `fmul; fiadd` a 24 bits) siguen en double:
  pasarlos a float le toca al dueño del sistema de ayuda (audio).
- La bandera g+0x14 bit 0x400000 («haz turno siempre», **(inferido)**), el segundo `ProcessNetworkPackets` tras dibujar
  si g+0x205D58 (0x54D3C9) y el turno propio de 100 ms del templo y la cinemática en pausa (0x54CCA9): no los hay en
  openblack.
- `LoadMap` hace a la vez de partida nueva y de `ResolveLoad` (**(inferido)**: openblack no carga partidas guardadas).
- `ECS/AnimalAnimations.cpp:441` (`movedLastTurn · 10`): los 10 turnos por segundo van implícitos; falta leer el
  original.
- `Magic/Objects/ShieldDebugHooks.h:23` dice que el PSys no sigue la velocidad: ya no es así (su fracción es la del
  juego).

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
- `test_object_metrics` (`test/test_object_metrics.cpp`) comprueba, con cajas de malla de prueba
  (`object::detail::SetMeshBoxProviderForTests`):
  - los campos de la caja (semiejes, semidiagonal), `Radius2D` y `Height`;
  - que el nivel de malla no ve redefiniciones y da 0 sin malla;
  - la base de `Object` y el 0 sin malla o sin `Mesh`;
  - Field y FishFarm = 5 (radio y `GetMeshRadius`, la altura sigue siendo la de la malla), MagicTeleport = 6,
    MagicFireBall = escala en radio y altura, la escala de objeto del MapShield y la altura de la criatura;
  - `GetProportionRaised` de comida y de madera, con la pila vacía a 0, y el radio de la pila de comida;
  - `GetHoldRadius`, `GetDefaultFireRadius` (árbol muerto, lugar de culto), `GetVillagerHugRadius` y
    `GetRoutePlanRadius` de árbol, las dos distancias, `IsTouching`, `GetBoundingSphere` y `GetTopPos`;
  - las redefiniciones de las derivadas (`DerivedOverrides`): la esfera de Living y MobileStatic, el `GetTopPos` del
    escudo, la altura de la mano sobre la piscifactoría, el radio de ruta del `CitadelHeart` y la distancia y el
    `IsTouching` del lugar de culto con su `WorshipSiteCentre`.
- `test_food_wood`: `PileFoodProportionRaised(0, 1000)` es 0.
- `test_game_clock` (`test/test_game_clock.cpp`) mueve el reloj con un `GetTickCount` de prueba
  (`game_clock::SetTickSource`) y fotogramas de 33 ms. Comprueba:
  - el primer turno en el primer fotograma, con el turno ya subido, `visual = 100` y dt 100;
  - 3 s a 33 ms dan 30 turnos (no 22), en los fotogramas 4, 7, 10… con huecos de 3 o 4;
  - el resto, la fracción y el dt fotograma a fotograma (33, 0,33… 0,99 y, con el turno 2, 32);
  - el tope de 99 del resto, el dt de 199 y un solo turno por fotograma;
  - el retraso de más de 2 s: se tira y el siguiente turno espera a `turno · 100`;
  - la pausa: ningún turno, dt 0, la fracción congelada, y al quitarla el tiempo parado no cuenta (sin turno extra);
  - la velocidad: el tiempo ya pasado conserva la de antes, el dt la sigue, y en pausa solo se guarda;
  - `OnLoad` (`visual = turno · 100`) y `Start` (el turno siguiente toca enseguida);
  - `TicksForSeconds` (trunca; con `SetMsPerTurn(300)`, 1000 / 300 = 3 por segundo);
  - el reloj de pared (≥ 1), `EngineMs` y los dos selectores (tope de 500); parado (0 y 1 ms) hasta
    `StartEngineTimer`, y luego cuenta desde ≈ 0;
  - `EngineTimerAfterLongUptime`: con `GetTickCount` = 0xCE000000 (40 días) los ms del motor son exactos.
- Variable de entorno: `OPENBLACK_START_PAUSED=1` empieza la partida en pausa (`game_clock::Start(true)`).
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
- Reloj del juego: desensamblado de 0x54C4A0, 0x54C570, 0x54CC30 (0x54CD0F..0x54CD58), 0x54D2A8..0x54D3D3, 0x54AE60:90,
  0x5557E0:60, 0x555820, 0x82F14E:50, 0x711630:30, 0x711610, 0x711280:F0, 0x70CC30:140, 0x66CD00, 0x66CD30:B0,
  0x5C6250:50, 0x714DB0 y, en la auditoría, 0x818C60:60, 0x8189F0:190, 0x54CD93, 0x54D338:50, 0x879B0A:40,
  0x54E5C0, 0x52AF90, 0x5DBD4E, 0x5C61B0:D0, 0x5C68C0:E0, 0x5537F0:E0, 0x634B40, 0x76EAF0 y 0x54E763. Informes: `dev\tmp_dis\unify2\PLAN.md` §2, `game_clock_original.md` (con su «Verificación
  adversaria») y `game_clock_openblack.md`. bw1-decomp: `src/Black/Game.cpp` (`PauseGame`, `SetSpeed`,
  `LocalTimerSaysDoATurn`, `ResetLocalGameTimer`, `ProcessNetworkPackets`, `Loop` l. 1834-1996, `ResolveLoad`).
- Tamaño de los objetos: desensamblado de 0x638110:E0, 0x8082C0:C0, 0x66EB60:C0, 0x66F180, 0x66F1B0:80, 0x477F40,
  0x47B190, 0x4EF4F0, 0x638C00, 0x74B610, 0x5110E0, 0x728640, 0x639AC0, 0x510E10, 0x77DE10, 0x77DDD0, 0x4026B0,
  0x74A1A0, 0x74A140, 0x6384C0, 0x637730, 0x637FB0, 0x5702B0, 0x4027C0, 0x637E00, 0x636D30; los sitios migrados
  0x53A740, 0x5116A0, 0x74AB80, 0x74B12F, 0x439220, 0x639550, 0x66E900, 0x722B30, 0x403270, 0x6A0D51 y 0x5287A0.
  Informes: `dev\tmp_dis\unify2\PLAN.md` §4, `object_radius_height_original.md` (con su «Verificación adversaria»)
  y `object_radius_height_openblack.md`. bw1-decomp: `src/Black/Object.cpp:1062-1104`; decomp_pickup:
  `multi.cpp:285-330`.
