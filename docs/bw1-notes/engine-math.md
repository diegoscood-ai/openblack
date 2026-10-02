# Coordenadas, terreno, tamaño de los objetos, reloj del juego, matrices y Zoomer

Matemáticas básicas del motor original (LH3D) y cómo se portan a openblack: el punto fijo de las posiciones, con sus
celdas y su espiral; las distancias y sigmoides de `GUtils`; el reloj del juego; la altura exacta del terreno; la
normal del terreno; las matrices LH; el tamaño de los objetos (radio 2D y altura), y el interpolador `Zoomer`. Todo es **fiel** (verificado
en el ejecutable) y está portado, salvo lo que se marca en [Pendiente](#pendiente).

- [MapCoords](#mapcoords): punto fijo, celdas, `InBounds`, vecinos y espiral (`ecs::map_coords`)
- [Distancias de GUtils](#distancias-de-gutils): raíz de tabla, `hypotenuse`, `GetDistanceInMetres`,
  `FastDistance` y las sigmoides (`gutils`)
- [Ángulos de GUtils](#ángulos-de-gutils): `LHArcTan`, las tablas COS/SIN, las conversiones, del ángulo a una
  posición, la diferencia y el sentido (`gutils`), y los puntos alrededor de un objeto (`ecs::object`)
- [Tamaño de los objetos](#tamaño-de-los-objetos): radio 2D, radio y altura, con las redefiniciones de las clases y
  las derivadas, a nivel de malla y de objeto (`ecs::object`)
- [Reloj del juego](#reloj-del-juego): el turno, los ms del turno, la fracción, el dt del fotograma, la pausa y la
  velocidad (`game_clock`)
- [Listas de objetos por celda](#listas-de-objetos-por-celda-ecsmap_cells): las dos listas ordenadas de cada celda,
  las celdas de un objeto, las búsquedas y el recorrido de las ciudades (`ecs::map_cells`)
- [Altura del terreno](#altura-del-terreno)
- [Normal del terreno](#normal-del-terreno): `LH3DIsland::GetNormal` y sus dos tablas (`land_normal`)
- [Matrices LH](#matrices-lh): los constructores de LHMatrix, la inversa y el modelo (`lh_matrix`)
- [Zoomer (LH3DLib)](#zoomer-lh3dlib): `Zoomer` y `Zoomer3d`, exactos al bit
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
    celdas enteras. `CellObjects` usa `map_coords::InBounds` en vez de su propia copia. El `CellObjects` de
    `PSys/Rules/Storm.cpp` ya no repite el `InBounds`: lo hace su único llamador, el bucle de fn_006D21B0 (0x6D2311).
  - `Magic/Spells/SpellWater.cpp` (copia 0x7250A2, `Spiral` 0x725166, `+=` 0x725173; 9 celdas, `ebp = 9`).
  - `ECS/AnimalAI.cpp`: `CalcRandomPos` ← `Living::CalcRandomPos` (0x5ED0FE..0x5ED152 el punto inicial con
    `ToFixedGUtils`, `+=` 0x5ED1C8), `LookForFoodPos` ← `Animal::LookForGrazePos` (`+=` 0x41A945) y la fusión de bandadas
    (`+=` 0x41A76A); `ECS/AnimalPredators.cpp` `FindPrey` ← fn_00419490 (`+=` 0x41954B). `detail::Spiral::Advance`
    envuelve `AddCells`.
- `TownQueries`: `Ftol` recibe un **float** (el comentario decía «x87 extendida», que contradice el `and cw, 0xFCFF` de
  0x7DEE0D). La media de la congregación (0x7409F3..0x740A1E) hace `fild qword` (exacto) y `fdiv` a 24 bits, así que el
  cociente se redondea una vez a float antes del `__ftol`: con 100 posiciones pasa de 2^24 y el truncado podía salir una
  unidad distinto. `GetPosFromAngle` (0x74D580) usa `ToFixedGUtils`; el coseno va en double (ver
  [Ángulos de GUtils](#ángulos-de-gutils)).
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

## Ángulos de GUtils

✅ Fiel y portado en `src/ECS/GUtilsAngle.{h,cpp}`, namespace `openblack::gutils` (sesión «sistemas2», 2026-10-02).
Es la familia de ángulos de la unidad `Utils` (0x74D0C0..0x74E2D0). Va encima de `ecs::map_coords` y de las
distancias.

- **Ángulo de juego**: entero de 11 bits, **2048 por vuelta**, guardado como `u16` (MobileWallHug +0x5C). 0 = +x,
  0x200 = +z, 0x400 = −x, 0x600 = −z. Es `atan2(dz, dx)` en 2048avos con la tabla de arcotangente: como mucho
  **2,27 pasos** de error (≈ 0,4°; a 10 m, unos 7 cm).
- **Ángulo 3D**: radianes en float, el mismo sentido, en [0, 2π) cuando sale de GUtils. `Get3DAngleFromXZ` **no** es
  `atan2` en float: es el ángulo de juego cuantizado y pasado a radianes.
- **Ángulo «Scawen»**: el 3D + π/2 (criatura, PBall, `Dove::Dying`, `GetFacingDirection`).
- Las rutinas de ángulo de juego son enteras (`LHArcTan` hace `shl 8; div`). La FPU va a 24 bits, pero **fsin/fcos no
  los redondea el control de precisión**: salen en extendida y los redondea a float, una sola vez, el `fmul` siguiente.
  Por eso `GetPosFromAngle`, `AddDistanceFromAngle` y `GetLHPointFromAngle` toman el coseno en **double** y redondean
  el producto una vez. Con `cosf` se redondea dos veces: con un modelo exacto de fcos, 38 de 20 000 valores (0,19 %)
  salen 1 unidad MapCoords distintos; con el double, ninguno.

| API (`openblack::gutils`) | Original | Notas |
|---|---|---|
| `ArcTanTable()` | tabla 0xC2307C (257 `u16`, .data) | `trunc(atan(i/256)·1024/π)`; en double con trunc da las 257 entradas (con round, 122 distintas). Solo la lee LHArcTan |
| `SinTable()`, `Sin(a)`, `Cos(a)` | tablas 0xC31614 (2560 `i32`) / 0xC31E14 = SIN + 512 | `trunc(65536·sin(i·2π/2048))`; las 2560 entradas coinciden (con round, 1220 distintas). El original indexa con `a & 0xFFFF`; aquí `a & 0x7FF` *(inferido: igual para todo lo que pasa el juego)* |
| `LHArcTan(dx, dz)` | `?LHArcTan@@YAXHH@Z` 0x74D0C0 | x = −dx (0x74D0C5); las ocho ramas con comparaciones con signo (`jl`) y divisiones sin signo; los empates a la primera rama; `& 0x7FF` (0x74D1E2). `n << 8` se queda con los 32 bits bajos, como el `shl` |
| `GetAngleFromDXDZ(dx, dz)` | 0x74D200 | `LHArcTan & 0xFFFF` |
| `GetAngleFromXZ(from, to)` | 0x74D240 (0x74D220 con cuatro enteros) | sobrecargas MapCoords, `ivec2` (16.16) y `vec2` (metros: cada punto pasa a MapCoords **antes** de restar) |
| `Get3DAngleFromXZ(from, to)` | 0x74D270 | `ConvertGameAngleTo3D(GetAngleFromDXDZ(to − from))` |
| `ConvertAngle3DToGame(r)` | 0x74DC30 | `ftol(r · 325,94931 [0x99A1C8]) & 0x7FF`: trunca, y un negativo da la vuelta (−0,5 → 1886). Ida y vuelta pierde 1 en **365 de 2048** ángulos |
| `ConvertGameAngleTo3D(a)` | 0x74DC50 | `(a & 0x7FF) · 0,0030679617 [0x99A1CC]`, un redondeo; es bit a bit `float(a) · 2π_f / 2048` |
| `ConvertScawenAngleToGameAngle(r)` | 0x74E290 | `ConvertAngle3DToGame(float(r − π/2 [0x8C78D8]))` |
| `ConvertGameAngleToScawenAngle(a)` | 0x74E2B0 | `float(2a) · 0,0015339808 [0x8C78DC] + π/2`, **sin** `& 0x7FF` |
| `GetXFromAngle` / `GetZFromAngle(a, int d)` | fn_0074D320 / 0x74D340 | `(C·d) >> 16` con `imul` de 32 bits y `sar` |
| `GetXFromAngle` / `GetZFromAngle(a, float d)` | fn_0074D360 / 0x74D380 | `float(C) · d · 2^-16` |
| `StepFromAngle(a, whole)` | fn_0074D3A0 / 0x74D3C0 | `((whole >> 4)·C) >> 12`, los dos `sar` (con signo: `whole` es `int32_t`). El paso de MobileWallHug |
| `StepFromAngle8(a, whole)` | fn_0074D3E0 / 0x74D400 | `((whole >> 8)·C) >> 8` |
| `GetX/ZByAngleMetersDistance(a, m)` | 0x74D420 / 0x74D450 | `ftol(float(C) · float(m / 10))` |
| `GetPosFromGameAngle(a, int whole)` | fn_0074D650 | `{StepFromAngle(a, whole), 0}` |
| `GetPosFromGameAngle(a, float m)` | fn_0074D6A0 | lo mismo con `whole = ConvertMetersToWholeDistance(m)`; el `sar 4` tira los 4 bits bajos |
| `GetPosFromAngle(r, m)` | 0x74D580 (60 llamadores) | `x = ftol(float(cos(r)·m) · 65536 / 10)`, z con sin, altitude 0 (el literal de `mov [esp+8], 0` 0x74D587); el `GetDistanceInMetres(origen, p)` de 0x74D5F4 se tira, y con él su origen temporal `{ftol(0 / 10), ftol(0 / 10), 0}` |
| `AddDistanceFromAngle(p, r, m)` | 0x74D510 | `p.x = ftol((float(cos(r)·m) + ToMetres(p.x)) · 65536 / 10)`, igual z; la altitude no cambia |
| `GetLHPointFromAngle(r, m)` | fn_0074D620 | `(cos(r)·m, 0, sin(r)·m)` en float |
| `GetAngleDifference(a, b)` *(nombre inferido)* | fn_0074D740 | `d = \|a − b\|`; `d > 0x400 ? 0x800 − d : d` |
| `GetAngleDirection(from, to)` *(nombre inferido)* | fn_0074D6F0 | `d = to − from`; 0 → 0; si `\|d\| > 0x400` (sin signo) da la vuelta; −1 si d < 0, si no +1. **Con \|d\| == 0x400 no da la vuelta**: +0x400 → +1, −0x400 → −1 |

`MapCoords` gana `operator+` 0x605520, `operator-` 0x6055C0, `+=` 0x605410 y `-=` 0x6054A0 (en `ECS/MapCoords.h`):
suman o restan x, z **y la altitude**.

**Puntos alrededor de un objeto** (`ecs::object`, `ObjectMetrics.h`): todas son `this + GetPosFromAngle(ángulo, r)`
con `MapCoords::operator+`, así que conservan la **altitude de this**. Cada una tiene su radio, y no se cambian unas por
otras:

| API (`ecs::object`) | Original | Ángulo y radio |
|---|---|---|
| `MapCoordsOf(e)` | Object +0x14 | `map_coords::FromWorld` de su `Transform` |
| `GetNearestPosOfObject(this, o)` | 0x636D30 | `G3D(this, o)`, `R2D(o) + R2D(this)` (vt +0x64 de los dos) |
| `GetNearestEdgeToPos(this, p)` | 0x636DA0 | `G3D(this, p)`, `R2D(this)` |
| `GetNearestEdge(this, ángulo, extra)` | 0x636DF0 | el ángulo lo da quien llama; `R2D(this) + extra` |
| `GetWorkingPos(this, o)` | 0x639550 | `G3D(this, o)`, `R(this) + R(o)` (**GetRadius**, vt +0x60) |
| `TreeGetWorkingPos(árbol, o)` | `Tree::GetWorkingPos` 0x74C040 | `G3D(árbol, o)`, `R2D(o) + 0,9` [0x8C5844]: solo el radio del otro |
| `BigForestGetArrivePos(bosque, v)` | `BigForest::GetArrivePos` 0x439360 | `G3D(bosque, v)`, `R(bosque) · 0,5` [0x8AA3B4] |

**Fuera de la API** (sin llamadores en el original): 0x74D2A0 (el ángulo entre dos LHPoint) y 0x74D770 (girar hacia un
ángulo con un paso máximo), sin `call`, `jmp`, `jcc` ni punteros en toda la imagen. 0x74D480 (el paso «octogonal») solo
lo llama fn_005E1890, sin portar. **No son de esta familia**: `LH3DMath::GetYAngle` 0x841290 y fn_007FAA50 (LH3D),
`Atan2Positive` 0x7DB770 (gestos), la conversión propia de PuzzleGame 0x6F184C.

**Arreglos de fidelidad que trajo.**

1. **`AngleDiff` de los animales a 180°** (`AnimalAI.cpp`): `((b − a + 1024) & 2047) − 1024` daba −0x400 cuando el
   giro era justo de +0x400, y el original (`GetAngleDirection` 0x74D6F0, `jbe` en 0x74D709) gira en positivo. Un animal
   que miraba justo al revés de su meta giraba al lado contrario, y el alabeo de los pájaros salía con el signo
   cambiado. `SetTowardsAngle` (0x418560) usa ahora `GetAngleDirection` y `GetAngleDifference`, como el original.
2. **`GetPosFromAngle` con el coseno en double** (antes `std::cos(float)`, doble redondeo): llega a todos los
   llamadores de `town_queries::GetPosFromAngle` (Abode, VillagerDecide, VillagerShield, la congregación).
3. **Ángulos en float sin cuantizar** pasados a `Get3DAngleFromXZ` + `GetPosFromAngle` sobre MapCoords:
   `VillagerFire` (`GetFireFightingPos` 0x75AAE2 / 0x75AB59 y la huida de `OnFire` 0x75B368), `Trees` (fn_0053A010
   0x53A094, `Tree::GetWorkingPos`, el borde del bosque de fn_0053ADB0 = `GetNearestEdgeToPos`,
   `BigForest::GetArrivePos` y `AddTreeAround` 0x439264), `WorshipSite::GetSpellIconPosFromSlot` 0x77AFC0,
   `AnimalFlee` (`Object::GetWorkingPos` 0x639550) y `Rock::SplitInTwo` 0x6E75B1 (`pos + o` y `pos − o`, 0x6E76A9 /
   0x6E76CE, sobre this +0x14: las dos mitades **conservan la altitude de la roca**, `map_coords::FromWorld` /
   `ToWorld`, como en el árbol y el bosque del punto 5; antes se ponían en el suelo).
4. **`GetSpellIconPosFromSlot` pone la altitude a 0** con ring > 0 (0x77B002, `mov [esp+0x14], 0` = MapCoords +8)
   antes del `+=`: el icono queda **en el suelo**. openblack conservaba la altura sobre el suelo del punto especial.
5. **`Tree::GetWorkingPos` y `BigForest::GetArrivePos` conservan la altitude** del árbol / bosque (`operator+`); antes
   se tomaba la altura del terreno sin más.
6. **`IsPosValidForTurnAngle`** (0x41B210): los centros de los dos círculos de giro son `me + fn_0074D6A0(a ± 0x200, R)`
   en MapCoords, con el `sar 4` que tira los 4 bits bajos de R, y la distancia es `GetDistanceInMetres` 0x74CD70 (con
   la tabla), no `glm::distance`. R pasa a metros con `ConvertWholeDistanceToMeters` (× 10 / 65536), no con / 6553,6.
   No hay prueba del giro: con `turnAngle` 0 el cociente es inf (NaN sin velocidad), `__ftol` da 0x80000000,
   R = −327680 m y las dos distancias lo superan (true); la rama `turn <= 0` que había se quitó (daba lo mismo).
7. **`CalcRandomPos`** (0x5ED0DB..0x5ED152): el desplazamiento aleatorio es `AddDistanceFromAngle` sobre el MapCoords
   del centro (antes sumaba en metros float). La salida final ya era la del original: `me + fn_0074D650(+0x5C, 10)` es
   `me + (0, 0)` porque `10 >> 4 = 0`. Los dos números al azar son `GameFloatRand` (0x5ED0BE el ángulo, 0x5ED0D2 el
   radio, este **siempre**, sin la rama `range > 0` que había): `GameFloatRand` 0x6DE530 / fn_005106B0 da 0 con 0 y si
   no `float(LHRand(0xFFFF)) · max · 1/65535` ([0x8D6050] = 0x37800080), también con `max` negativo. `SquarePos`
   (fn_0074F310) usa el mismo. *(Aproximado)* `LHRand` 0x7DB600 es aquí el generador de openblack (0..0xFFFE). El centro
   llega en metros, como openblack guarda las posiciones, y pasa a MapCoords con `FromMetres`; el original recibe el
   MapCoords, así que un centro que no lo fuera ya puede quedar a una unidad (`Quantise` no es idempotente).
8. **La formación de pájaros** (fn_0041E890, 0x41E96A..0x41EA05) no es `AddDistanceFromAngle`: x usa `row` y z usa
   `column`, y el orden es `(cos·row)·10` (`fimul` y luego `fmul 10`), dos redondeos, sobre el MapCoords del líder.
9. **`Dove::Dying`** (0x41F1B0): la velocidad es `(sin(s)·v, 0, −cos(s)·v)` con `s = ConvertGameAngleToScawenAngle`;
   igual en matemáticas, distinta en bits.
10. **`AngleOf(vec2)` de los animales** restaba en metros y luego truncaba: ahora cada punto pasa a MapCoords y se resta
    (`GetAngleFromXZ`), como el original. Nueve usos (AnimalAI, AnimalBirds, AnimalFlee, AnimalPredators,
    AnimalWallHug).
11. `VillagerFire` `OnFire`: los dos `GameFloatRand` (ángulo y distancia) iban como argumentos de una llamada, sin orden
    garantizado; ahora el ángulo va primero, como en 0x75B32D..0x75B34A.
12. **`SetNewWander`** (`Animal::SetNewWander(MapCoords const&, int, int)` 0x41A3F0): la distancia pasa a entero con
    `__ftol` (0x41A421) y se compara **como int** con `rMax` y `rMin` (0x41A426 `cmp; jle`, 0x41A430 `cmp; jge`). Antes
    se comparaba el float: con `d` en (rMax, rMax + 1) el animal iba hacia el centro y en el original no.
13. **`__ftol` 0x7A1400** es una sola función, `map_coords::FtoL` (`MapCoords.h`), usada por `ToFixed`,
    `ToFixedGUtils`, `CellSpiralSize`, `IncrementSpiralSize`, `ConvertMetersToWholeDistance` y GUtilsAngle (antes
    `static_cast`, indefinido fuera de rango). Imita la rama SSE2 (`HasSSE2` [0xE83A20], `cvttsd2si`, la que toma toda
    CPU actual): hacia 0, y 0x80000000 para NaN o fuera del rango de int32. La rama x87 (0x7A141F, `fistp qword` y la
    corrección hacia 0, los 32 bits bajos del int64) daría otro valor fuera de ese rango; no se reproduce.
14. Copias que quedaban: `VillagerCore.cpp` `setGameAngle` (la constante 0x99A1CC a mano) usa
    `gutils::ConvertGameAngleTo3D`, como `SetGameAngle` 0x60DAA1; `SpellFlock.cpp` (las dos orientaciones,
    0x74D240) usa `gutils::GetAngleFromXZ(created, target)` en vez de `AngleOfMapCoords` con la resta hecha.

**Qué usa ya la API.** `town_queries::GetAngleFromXZ` / `Get3DAngleFromXZ` / `GetPosFromAngle` y
`animal_ai::detail::Cos` / `Sin` / `Step` / `AngleOfMapCoords` son reenvíos de una línea a `gutils` (las copias de las
tablas y de `LHArcTan` se borraron de `AnimalAI.cpp`). `AngleOf(vec2)` y `AngleDiff` ya no existen.

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

Los envoltorios `effects::ObjectHeight` / `Object2DRadius` (la rutina de `Object` **sin** redefiniciones) ya no
existen: todos sus llamadores van por la API (2026-10-02).

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

## Listas de objetos por celda (`ecs::map_cells`)

`src/ECS/MapCells.{h,cpp}` (fase A de map_cell_queries, 2026-10-02, milagros2; investigación en
`dev\tmp_dis\unify2\map_cell_queries_original.md`, `map_cell_queries_PLAN_A.md` y `map_cell_queries_A_impl.md`). Es la
rejilla GMap del original (g_game+0x59B8, MapCell de 8 bytes en +0x59FC, 512 × 512 por `GMap::Init(0x200, 0x200)`
0x6014C0): cada celda tiene **dos listas enlazadas y ordenadas**, +0 la móvil (`SetFirstObjectMobile` 0x601B60) y +4
la fija (0x601B70).

**Qué lista.** La decide el **tipo** de la info (+0x10), no la clase: `DoesObjectTypeCountAsFixed` 0x601510, tabla
0x60152C (fijos 0, 6-9, 11, 12, 14, 18, 19, 21-26, 28, 29, 31-41, 43, 44; por encima de 0x2C, también −1 y −2 sin
signo, no). `InitialiseIsFixedForMapList` 0x63A640 lo guarda en el bit 15 de Object +0x24. El tipo se lee de info.dat
(`map_cells::TypeOf`); lo que openblack no guarda con fila va marcado (inferido) en el código.

**Qué extremo.**

| Clase (`InsertKind`) | Inserción | Extremo |
|---|---|---|
| SingleMapFixed (Tree, MagicTree, MapShield) | 0x52E620 → `Fixed::InsertMapObjectToCell` 0x52DEA0 | cabeza de la fija, su celda |
| MultiMapFixed (Abode, Field, Feature, AnimatedStatic, MobileStatic, DeadTree, BigForest, TotemStatue, WorshipSite, Temple, SpellIcon, MagicTeleport, Fragment) | 0x52E650 → `AssumeFixed` 0x52DEE0 en cada celda; hijos ordenados por x y z (`SortChildren` 0x52DC10) | cabeza de la fija, todas sus celdas |
| FishFarm | 0x52CA10; `GetNextPos` 0x52C940 da **una sola** posición, la suya | cabeza de la fija, su celda |
| Object (Villager, Animal, Creature, StreetLantern, Pot y pilas, OneOffSpellSeed, MobileObject, Shark) | 0x636740 → `Object::InsertMapObjectToCell` 0x636830 | con el bit 15, **cola** de la fija (ollas y pilas, farolas); sin él, cabeza de la móvil (doble enlace, +0x38; los orbes: su info `GMobileObjectInfo` 25 es del tipo 20) |
| SpellSeed, MagicFireBall, Town, Forest... | `ret` (0x728F30, 0x682D10) | fuera del mapa |

Borrar (`RemoveMapObjectFromCell` 0x6368D0) no cambia el orden de los demás. Mover (`MoveMapObject` vt+0x55C): un
objeto de una celda solo se reinserta si cambia de celda (0x636A40); un MultiMapFixed, si cambia su MapCoords
(0x52E4F0, `operator==` 0x605660). `ActualMoveMapObject` 0x638040 lo deja **en la cabeza**. `SetXYZAnglesAndScale`
(0x638F80 / 0x6074E0 / 0x608D60) también lo quita y lo vuelve a meter.

**Celdas de un MultiMapFixed** (`NewCollideDescriptor` 0x46A860 / `Init` 0x46AB10 / `GetNext` 0x46AD80;
`DescriptorCells`):

1. La forma es `map_collide::FromMesh` (NewCollide 0x829390) y `reach = escala · mesh+0x30 + 1` ([0x8AA390]).
2. Caja `ftol((c ∓ reach) · 0,1)` ([0x8AC404]). Si la esquina baja es negativa pasa a 0, y solo entonces la alta
   también (0x46ABC9..0x46ABE7).
3. x por fuera, z por dentro. Se marca la celda cuyo círculo de **7,1 m** (0x40E33333) en `(10i + 5, 10j + 5)` toca la
   forma, solo si está en el mapa. Si no se marca ninguna, la del medio, `(w/2)·d + d/2` (0x46AD06..0x46AD3B).
4. La inserción **se corta** en la primera celda marcada fuera del mapa (0x52E70D).
5. (aproximado) Sin malla en openblack (campos, piedras de teletransporte): la celda de su posición.

**Lecturas de una celda.**

- El recorrido del original (`GetFirstIterator` 0x6034D0 + fn_006827E0, y las copias en línea) es **la fija desde su
  cabeza y luego la móvil desde la suya** (`ForEachInCell` / `ObjectsInCell`). `MobileInCell` es solo la móvil
  (0x603490).
- `FindType(celda, t, anterior)` (0x6045C0 → `FindTypeOnMap` 0x6015E0): con −1, la fija y luego la móvil (al acabar la
  fija salta a la móvil si el tipo del anterior cuenta como fijo, 0x601621); con otro tipo, **solo su lista**
  (0x601646).
- `FindFixedOnMap` 0x601690. `IsFixed` (0x603790 → 0x601EA0) mira **solo la cabeza** de la fija: que sea un
  MultiMapFixed (+0x24 bit 1). `IsOwnCell` es fn_00604F40.

**Búsquedas.**

- `FindNearType` 0x6045F0: una sola lista (−1 es la móvil); no recorta a r.
- `FindNearForScript` 0x604370: cuadrado ±r con signo, la cuenta de z es `(alto & 0xFFFF) − bajo + 1`, el tótem de un
  sitio de culto (0x77CF30), `<` estricto desde FLT_MAX.
- `FindNearestInSpiral` fn_00604AF0 / fn_00604C30: `max(3, ceil(2r/10))²` celdas, `d < r`, corte `1,5·mejor + 10`
  ([0x8AB24C] / [0x930050]).
- `FindNearInfluenced` 0x604870: `GetDistanceModifier(d, r)` 0x74F290 (r es el último argumento, leído en 0x604A33).
  Aún no la usa nadie.
- `TallestOverlapping` fn_006022C0.

**Ciudades** (no usan celdas):

- `ForEachTown` / `TownsOf` = `GetNextPlayerAndNeutral` 0x550980 (huecos 0..7, el neutral el último) × la lista de cada
  jugador, que se rellena **por la cola** (fn_0064C090): la más vieja primero ((inferido) por `Town::id`).
- `GetNearestTown` 0x6020E0 y `GetNearestCitadel` 0x602200: `<` estricto desde r. `GetNearestTownWithCentre`
  fn_00602160.
- `GetNearestTownCells` 0x601F90: distancia octogonal en celdas; (aproximado) sin el rectángulo de la ciudad.
- `GetNearestTownToPos` 0x73B170: `0x7FFF` es cualquier casa; con otro tipo **acepta las ciudades que no lo tienen**.
- `FindNearestTownInList` fn_00552FF0: la lista global. **No tiene rama de ID** (leído): la primera siempre y luego
  `<`.

**Mantenimiento en openblack.**

- Ganchos `InsertMapObject` / `RemoveMapObject` / `MoveMapObject` / `OnAnglesOrScaleChanged` donde el original llama a
  la vtable. En la fase A los pone milagros2 en lo suyo: árboles de SpellForest, pilas, pedazos de la tormenta, orbes,
  MapShield, las piedras y el vivo teletransportado, y lo que lleva el tornado (`SetHeldOutOfMap`).
- Lo de los demás dueños entra por `Sync()`, que llama `MapProduction::Rebuild` al empezar cada turno, al cargar y en
  `Reactions`: primero las bajas (destruidos, en la mano, en física, cambio de clase) y luego las altas y los
  movimientos por índice de creación (inferido).
- Toda lectura se salta además lo que el original ya habría sacado: no válido, en la mano o volando.
- `OPENBLACK_MAPCELLS_CHECK=1` comprueba las listas en cada `Sync` y escribe `map_cells: N objects, M cells, E errors`.
- La API vieja (`MapInterface` / `MapProduction`, `effects::ObjectsInMapCell`) sigue para quien no ha migrado. Lo que
  le queda a cada dueño está en `map_cell_queries_A_impl.md`.

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

## Normal del terreno

**Fiel.** `LH3DIsland::GetNormal(const LH3DMapCoords&, LHPoint*)` 0x803630 (fastcall: ecx = coords, edx = out), portado
en `land_normal::OfCell` (`src/3D/LandNormal.{h,cpp}`) y llamado desde `LandIsland::GetNormalAt`, al lado de
`HeightAt`. Todos los llamadores del original construyen el MapCoords con `ftol(x·65536 [0x8AC408]·0,1 [0x8AC404])`
(fn_004427B0, 0x8126C0, 0x459105, 0x7FCBA2): `65536·0,1f` es exactamente `6553,6f`, así que es `map_coords::ToFixed`.

1. Celda `(int16)(x >> 16)`, `(int16)(z >> 16)`; fuera de [0, 0x200) → (0, 1, 0) (0x80363B..0x80366B). openblack
   compara con `GetCellsPerSide()` (512 en los mapas del juego).
2. `GetCell` 0x516AA0: NULL si no hay bloque (`g_index_block` = 0, 0x516ADC) → (0, 1, 0).
3. Alturas **en bruto** (sin aplanar junto al mar): h00 = [+4], h01 = [+0xC] (z+1), h10 = [+0x8C] (x+1),
   h11 = [+0x94], dentro del bloque de 17×17 (la fila del borde es del bloque: la celda 511 se lee).
4. Triángulo (bit `split` = [+6] & 0x80, 0x803698..0x803752):
   - con split: B = h11 en (10, 10) si `fz > 0xFFFF − fx`, si no h00 en (0, 0); P = h10 en (10, 0); Q = h01 en (0, 10);
   - sin split: B = h10 en (10, 0) si `fx > fz`, si no h01 en (0, 10); P = h11 en (10, 10); Q = h00 en (0, 0).
5. `dP = hP − hB`, `dQ = hQ − hB` en entero; `s = T1[|dP|]·T1[|dQ|]`; `P' = (Px − Bx, dP·0,67 [0xC3720C], Pz − Bz)`,
   `Q'` igual (Q'x = `−Bx`, un `fchs`).
6. `n = ((P'z·Q'y − P'y·Q'z)·s, (Q'z·P'x − P'z·Q'x)·s, (P'y·Q'x − Q'y·P'x)·s)` (0x8037BA..0x8037F5).
7. `k = fistp(((nz·nz + nx·nx) + ny·ny)·1023 [0x9A2BE8])` (al más cercano), `n *= T2[k]`; si `n.y < 0`, `n = −n`.

Tablas de la inicialización de la isla fn_00803890 (cada paso a 24 bits):
- T1 en 0xE9B2D8: `T1[i] = 1/√((0,67·i)² + 100)`, i = 0..255 (0x8038E3..0x803934). Las aristas P' y Q' son
  perpendiculares en xz y miden `√(100 + (0,67·d)²)`, así que `s = 1/(|P'||Q'|)`.
- T2 en 0xE9A2D8: `T2[0] = 1`, `T2[j] = 1/√(j·0,000977517 [0x9A2BEC = 1/1023])`, j = 1..1023 (0x803936..0x80396F).

El resultado es **casi unitario**: la cuantización de T2 deja un error relativo de hasta ~0,5/k (en una celda muy
empinada con k = 7, la longitud es 0,998). Nunca es nulo: la comprobación `dot(n, n) <= 0` que tenía PhysOb era código
muerto. **(port)** Con las altitudes de 16 bits de BWLandEditor, `|d|` puede pasar de 255: se calcula con la misma
fórmula; y el índice de T2 se limita a 1023 (en el original, `|n|² ≤ 1` lo garantiza).

**Quién la usa:**
- física (`PhysOb.cpp` `Normal`/`LandscapeNormal`: `AdjustToGroundLevel` 0x7FCBE2, `GroundAndWater` 0x7FD93F);
- la mano (`HandHolding.cpp`, `InitialisePhysicsFromHand` 0x63729E);
- la bola de fuego (`Fireball.cpp`, `GravityWithFloor` 0x6A1C7F);
- las sombras de aldeanos y animales (`Renderer.cpp`, fn_00812170 0x8126FD / 0x812859);
- la cámara del jugador (`DefaultWorldCameraModel.cpp`, `CameraModeNew3::FindBestAngle` 0x459144).

Antes, `GetNormalAt` eran diferencias centrales de ±0,1 m sobre `GetHeightAt` (que aplana junto al mar) y la física
tenía su propia copia (`glm::normalize` en lugar de T2, arriba en la celda 511 y sin mirar si hay bloque). No son
`GetNormal`, y no se tocan: `Foliage.cpp` `GroundNormal` (mod de plantas: sigue la malla dibujada) y `LandBlock.cpp`
(normal suave por vértice del render).

## Matrices LH

**Convenio.** Un `LHMatrix` son 3 filas + traslación (vector fila, p' = p·M): la fila k es la imagen del eje local k.
En glm es la **columna** k, con la misma memoria (`glm::mat4x3` son los 12 floats de un LHMatrix). Los giros de LH3D
van al revés que los de glm: el ángulo a del original es el −a de `glm::rotate`.

**Precisión.** La FPU va a 24 bits, pero fsin/fcos no los redondea el control de precisión. Lo que el original deja en
la pila se toma en double y lo redondea una vez el producto que lo usa. Lo que guarda (`fstp dword`) es un float.
Es la regla de gutils («Ángulos de GUtils»). **(aproximado)** El double no es el registro de 80 bits: en casos raros
cambia el último bit.

**API `openblack::lh_matrix`** (`src/3D/ObjectMatrix.{h,cpp}`):

| Función | Original | En glm |
|---|---|---|
| `YXZ(y, x, z)` | `LHMatrix::SetYXZMatrixOnly` 0x7FAC10 | `eulerAngleYXZ(−y, −x, −z)` = `Ry(−y)·Rx(−x)·Rz(−z)` |
| `AngleY(a)` | `AtomCore::SetAngleY` 0x674360; la rotación de `LH3DObject::SetPosition` 0x423140 y `Object::GetWorldMatrix` 0x638200 | `eulerAngleY(−a)` = `Ry(−a)`; = `YXZ(a, 0, 0)` bit a bit |
| `AngleXYZ(x, y, z)` | `AtomCore::SetAngleXYZ` 0x674200 | `Rz(−z)·Ry(−y)·Rx(−x)` (no es `eulerAngleXYZ`) |
| `RotateY(m, a)` | `LHMatrix::RotateY` 0x5198F0, en el sitio | `m·Ry(−a)` (**a la derecha**: los ejes del objeto) |
| `RotateZ(m, a)` | fn_0086AFA0, en el sitio | `m·Rz(−a)` (a la derecha) |
| `TurnRows(m, eje, a)` / `(m, eje, c, s)` | `UpdateRuleRotatePrincipalAxis` 0x6A1150, `AppearanceRuleTumble` 0x6A6200 | `R_eje(−a)·m` (**a la izquierda**: el mundo) |
| `AxisAngle(eje, a)` | fn_007FB180 (Rodrigues por filas) | `rotate(−a, eje)` |
| `Inverse(m)` | `LHMatrix::SetInverse` 0x7FB290 | inversa, con el tope del determinante |
| `SetPosition(p, a, s)` | `LH3DObject::SetPosition` 0x423140 (vt+0x20) | `T(p)·Ry(−a)·S(s)` |
| `Model(p, R, s)`, `Model(Transform)` | lo que escriben todos los `Set*` | `T(p)·R·S`, la posición tal cual |

Detalles, celda por celda:
- **SetYXZMatrixOnly 0x7FAC10** (a = Y, b = X, c = Z): `m0 = (ca·cc) − ((sc·sb)·sa)`, `m1 = −(sc·cb)`,
  `m2 = ((sc·sb)·ca) + (sa·cc)`, `m3 = ((sa·cc)·sb) + (sc·ca)`, `m4 = cc·cb`, `m5 = (sc·sa) − ((ca·cc)·sb)`,
  `m6 = −(cb·sa)`, `m7 = sb`, `m8 = cb·ca`. Se guardan en float `cb` (0x7FAC23), `sc` (0x7FAC39), `ca·cc` (0x7FAC41) y
  `sa·cc` (0x7FAC4F); `ca`, `sa`, `sb` y `cc` se quedan en la pila. No toca la traslación. CAnim lo llama con el float3
  guardado (v0, v1, v2) como `YXZ(v1, v0, v2)` (0x85F28E..0x85F29D).
- **SetAngleY 0x674360**: filas (c, 0, s) / (0, 1, 0) / (−s, 0, c), con `c` y `s` guardados en float.
- **SetAngleXYZ 0x674200**: filas (1, 0, 0) / (0, cx, −sx) / (0, sx, cx) (cx, sx en float). Luego, en cada fila,
  `(e0, e2) → (cy·e0 − sy·e2, cy·e2 + sy·e0)` (0x674244..0x6742A2) y `(e0, e1) → (cz·e0 + sz·e1, cz·e1 − sz·e0)`
  (0x6742D2..0x674330). `AtomCore::RandomiseOrientation` 0x6743E0 saca tres `PSysFloatRand(2π)`: el **primero es z**,
  el segundo y, el tercero x (cada `fstp [esp]` cae en el hueco del argumento que acaba de empujar).
- **RotateY 0x5198F0**: `r0' = c·r0 + s·r2`, `r2' = c·r2 − s·r0`; r1 y la traslación igual. **fn_0086AFA0**:
  `r0' = c·r0 − s·r1`, `r1' = c·r1 + s·r0`.
- **TurnRows**: en cada fila, eje Z (x, y) → (c·x + s·y, c·y − s·x); eje Y (x, z) → (c·x − s·z, c·z + s·x); eje X
  (y, z) → (c·y + s·z, c·z − s·y); la tercera componente no se toca. 0x6A1150 guarda `c` en float en Z e Y
  (0x6A117C, 0x6A1229) y deja `s` en la pila; su eje X (fn_006A12F0) y el Tumble 0x6A627E dejan los dos. Por eso hay
  dos sobrecargas.
- **fn_007FB180**: `m0 = ((1 − xx)·c) + xx`, `m3 = (xy − xy·c) + s·z`, `m1 = (xy − xy·c) − s·z`,
  `m6 = (xz − xz·c) − s·y`, `m2 = (xz − xz·c) + s·y`, `m4`, `m7 = (zy − zy·c) + s·x`, `m5 = (zy − zy·c) − s·x`, `m8`;
  traslación 0. La mano transforma (0, 1, 0) como vector fila (0x5B6EE8): `AxisAngle(eje, a)·v`.
- **SetInverse 0x7FB290**: `det = ((m2·m7 − m8·m1)·m3 + (m5·m1 − m2·m4)·m6) + (m8·m4 − m7·m5)·m0`. Si
  `|det| < 1e-10` [0xC371D4], `det = ±1e-10` con el signo de det (+ para 0; 0x7FB2C8..0x7FB2EE). Luego cada cofactor
  × `1/det`, y la traslación `−(t·A⁻¹)` (0x7FB392..0x7FB3DF).
- **SetPosition 0x423140**: cuatro ramas por a == 0 y s == 1 (0x423145 / 0x423151); con a ≠ 0, RotateY en línea sobre
  diag(s) (0x4231B3..0x42321C): filas (c·s, 0, s·s) / (0, s, 0) / (−s·s, 0, c·s). Con s ≠ 1 la traslación es `0 + p`
  (las celdas puestas a 0 más p: 0x423195..0x4231B0 y 0x423312..0x42332D), que convierte −0 en +0; con s == 1 se copia
  (mov, 0x42325A..0x423268). `lh_matrix::SetPosition` hace lo mismo.
- La escala multiplica las filas (en glm, las columnas) y la traslación se escribe tal cual (0x423195, 0x6382B7,
  0x607606): `Model`.

**Qué constructor usa cada objeto** (vt+0x63C, búsqueda en las vtables):
- `Object::GetWorldMatrix` 0x638200, solo Y (`T(x, GetAltitude + y, z)·Ry(−GetYAngle)·S`): Abode, Windmill, los
  animales, AnimatedStatic, Feature, BigForest, la criatura (vtable 0x8CCE4C), Field, Tree, Villager, los
  lugares de culto, los iconos de hechizo, Totem, StoragePit…
- `MobileObject::GetWorldMatrix` 0x607560 y `MobileStatic::GetWorldMatrix` 0x608DE0, YXZ:
  - MobileObject 0x607560: Arrow, Ball, Pot, PileWood, PileFood, Whale, MagicFood, MagicWood…
  - MobileStatic 0x608DE0: Bonfire, DeadTree, FelledTree, Rock, MagicTeleport, Fragment…
- `Game3DObject::SetPosition` 0x63B740 (LHPoint) / 0x63B680 (MapCoords): `T(p)·YXZ(y, x, z)·S`, con las 9 celdas × s.

**Cómo se usa en openblack.** Las creaciones con `AngleY` son los arquetipos de Abode, AnimatedStatic, BigForest,
Feature, Tree, Pot, MobileObject y Shark, además de la criatura, `DesignedScenery`, los ríos, los lugares de culto y sus
iconos, la ciudadela del guion y las marcas del suelo. Lo usan también al moverse: el tiburón, los caminos y el dibujo
de aldeanos y animales (ángulo «Scawen» = `angle + π/2`). También el escudo físico (`MapShield.cpp`: el RotateY en línea
de 0x72D4DB..0x72D558 sobre la identidad, con `c` en float, es `AngleY` bit a bit), el brillo que gira de los símbolos
de creencia (`TownBelief.cpp`: el ángulo del sprite +0x14 de 0x69D8C5 llevado como la matriz de SetAngleY) y
`billboard::YawToEye`. `YXZ` lo usan MobileStatic, DeadTree y la mano (`HandAnimator`). `AngleXYZ` va en
`RandomiseOrientation` (PSys). `RotateY`/`RotateZ` van en la luna (`billboard::MoonModel`) y en los barcos
(`PetitNavire`). `TurnRows` lo usan RotateAxis (PSys), el Tumble (`Sprinkle`) y las partículas de recoger
(`HandEffects`). `AxisAngle` va en la inclinación de la mano (`HandPlacement`) y en el giro de la física
(`PhysOb::Integrate`). `Model` lo usan el render y las cajas: `RenderingSystem`, `RenderingSystemTemple`, `Renderer`,
`CarriedProps`, `FeatureBuild`, `Buildings`, `Sharks` y `Archetypes/Utils`.

**Arreglado (2026-10-02, demostrado en el binario):**
- `PSys.cpp` RandomiseOrientation: era `eulerAngleXYZ(x, y, z)` (la composición transpuesta), con los aleatorios en
  orden x, y, z. Ahora es `z, y, x` y `AngleXYZ`.
- `HandEffects.cpp`, el volteo de los pedazos al recoger: iba a la derecha con +a y ahora va a la izquierda con −a,
  como 0x6A6200 y `Sprinkle.cpp`.
- `CreatureArchetype.cpp`: `eulerAngleY(+y)` pasa a `AngleY(y)`. Hoy no se ve: el guion pasa π.
- `RenderingSystem.cpp` / `RenderingSystemTemple.cpp`: el modelo era `R·T(p·R)·S`; ahora es `T(p)·R·S`. La traslación
  de antes era `R·Rᵀ·p`: a unos ulp de p si R es una rotación, pero lejos de p (proporcional a |p| ≈ 1000-3000 m) si no
  lo es. Hay tres usuarios cuya R no es una rotación, y en ellos el cambio se ve:
  - las bandas de la mano mientras vuelan (`HandMagicFX.cpp` SetTransform: `mat3(M)/escala` de una interpolación lineal
    de dos matrices);
  - los objetos que llevan los aldeanos en pendiente (`CarriedProps.cpp`: la cizalla shearX/shearZ de fn_0051B220);
  - el escudo físico entre dos turnos (`MapShield.cpp` DrawPhysical: interpola las filas, 0x72CEEC).
- `PhysOb::Integrate`, el giro: el sentido ya era el del original y ahora se construye como él. fn_007FE260
  (0x7FE706..0x7FE748) hace `inv = 1/ángulo` (fdiv), `eje = paso·inv`, `fn_007FB180(eje, ángulo)` y las filas por esa
  matriz (fn_0046D9D0: `r_k' = r_k·M`). Su par es `F × r` (0x7FE0F9..0x7FE11F y 0x7FD7FD..0x7FD823) y el de openblack
  `r × F`, así que la ω y el eje del original son los de openblack cambiados de signo: `AxisAngle(−paso·inv, ángulo)·R`.
  ≈ulp.
- `PSys/Rules/Shield.cpp` (VapourEndEffect, fn_0057D2B0 en 0x6A3D7C): se ha leído el sentido. Es el cuaternión
  `(cos(a/2), sin(a/2)·n)` de fn_0057D1D0, su matriz por fn_0057D0B0 (`m1 = xy + wz`: el giro de +a con la regla de la
  mano derecha, por filas) y las filas por ella (fn_007FAFF0). Gira cada fila +a sobre `n = last × p` (fn_006A3E20),
  como el `glm::rotate(+a, n)` de openblack. Las celdas no son las del cuaternión **(aproximado)**.

**Sin fuente, se dejan como estaban y quedan marcadas (inferido):**
- `HandHolding.cpp` HeldSway: falta leer qué ángulo y qué eje lleva cada fn_007FB180 (0x5B49B6..0x5B4ACE).
- Las bandas de la mano (`HandMagicFX.cpp`): solo cuadran si el hueso tiene Y y Z cambiados.
- `HandTrees.cpp`: el árbol tumbado y el tirón (0x5B8700).
- La flexión del árbol en `RenderingSystem.cpp` (0x74B016).
- El sol (`Renderer.cpp`, fn_0086C020).
- `TempleInterior.cpp` (siempre 0) y `HandArchetype.cpp` (`eulerAngleXYZ`, sin efecto: HandPlacement lo reescribe).
- El ángulo inicial del aldeano (0x74F950).
- Los montones del almacén: falta la escala en `AbodeArchetype.cpp`.
- La escala de solo X de los ríos.
- El sentido del alabeo de la mano (`HandPlacement.cpp`, el Zoomer CHand+0xD4). El original gira con
  `fn_007FB180(dir, [CHand+0xD4] + vt+0x14 + [esp+0x20])` (0x5B49A0..0x5B49C8, `rotate(−a)`), pero falta leer el eje
  dir ([esp+0xA4]) y cómo llega esa matriz a la de la mano (fn_007FAFF0 0x5B4AE5). openblack gira +roll de glm sobre
  `forward`. Hoy no se ve: el destino siempre es 0.

## Zoomer (LH3DLib)

**Fiel, comprobado bit a bit con el original.** API `openblack::Zoomer` y `openblack::Zoomer3d` en
`src/Common/Zoomer.{h,cpp}` (estructura de 0x30 bytes de bw1-decomp `Lionhead/LH3DLib/development/Zoomer.h`):
- `value` +0x00;
- `destination` +0x04;
- `destinationSpeed` +0x08;
- `speed` +0x0C;
- TimeM2 +0x10 (solo se pone a 0; no se guarda);
- `time` +0x14;
- `duration` +0x18;
- `startValue` +0x1C;
- `startSpeed` +0x20;
- `c2`, `c3`, `c4` +0x24..+0x2C (coeficientes de t²/2, t³/6, t⁴/24).

Es una cuártica que parte del valor y la velocidad de ahora y llega al destino en T con la velocidad de destino y
aceleración 0. Todo va en float y en el orden del x87:
- **`SetPosition(p)` 0x441AC0**: valor = destino = inicio = p, y lo demás a 0.
- **`SetDestinationWithSpeedAndTime(dest, vDest, T)` 0x407D60**:
  - con `T < 0,001` [0x8AA3B0] (o NaN), `SetPosition(dest)`;
  - si no: `A = (T·T)·0,5`, `B = (A·T)·0,33333334` [0x8AB26C], `C = (A·A)·0,16666667` [0x8AB268];
  - la matriz M (filas (C, B, A) / (B, A, T) / (A, T, 1)) se invierte con `lh_matrix::Inverse` (0x7FB290);
  - `r1 = (dest − inicio) − T·v_inicio`, `r2 = vDest − v_inicio`;
  - `c4 = (inv10·r2 + inv00·r1) + inv.t.x`, `c3 = (inv01·r1 + inv11·r2) + inv.t.y`,
    `c2 = (inv12·r2 + inv02·r1) + inv.t.z` (0x407E6D..0x407EC5).
- **El tope del determinante.** Para esta M, `det = −T⁶/144`, que baja de 1e-10 con **T < 0,0493 s**. Entonces los
  coeficientes salen × `(T⁶/144)/1e-10`. El zoomer casi no se mueve y salta al destino al acabar. Un paso de 0 a 10
  vale, a T/4: 2,6171875 con T = 2,5 s y 0,7444 con T = 0,04 s (la forma cerrada daría 2,617).
- **`Update(dt)` 0x442720**: `t = dt + time`. Si `t ≥ duration`, se queda en el destino y su velocidad, con
  `time = duration`: no extrapola. Si no, con `a = (t·t)·0,5`, `b = (t·a)·0,33333334` y `C = (a·a)·0,16666667`:
  - `speed = ((t·c2 + a·c3) + b·c4) + v_inicio`;
  - `value = ((((C·c4) + b·c3) + a·c2) + t·v_inicio) + inicio`.
- **`Zoomer3d`** (0x90 bytes: x +0x00, y +0x30, z +0x60):
  - `SetDestinationWithTime` 0x44E760: velocidad de destino 0. No hay `SetDestinationWithSpeedAndTime` de tres ejes:
    no se ha visto ningún sitio del binario que dé a un Zoomer3d una velocidad de destino distinta de 0. El eje x llama a 0x407D60; y y z son el mismo código en
    línea, y fn_00418A50 solo suma un 0 de más;
  - `GetCurrentValue` 0x4605D0;
  - `Update`: los tres `Zoomer::Update` (GCamera::Update 0x441FEE..0x442029).

**Usuarios:**
- La cámara del jugador (`Camera`): `Zoomer3d` de la posición (GCamera +0x118) y del foco (+0x88).
  - `CameraModeNew3` les da destino cada fotograma con `SetDestinationWithTime` (0x4604A4..0x4604D2).
  - GCamera::Update llama primero al modo (vt+8, 0x441FD9) y después a `Update(min(dt, 0,1 [0x8AB22C]))`.
  - `Camera::UpdateZoomers` hace eso.
- La cámara del guion (`script_camera`: posición, foco y FOV).
- El alabeo de la mano (`HandPlacement`, CHand+0xD4, 0,4 s) y su distancia (g_HandDistZoomer).
- Los animales (alabeo), los montones (hundimiento, 1 s), los campos (1 s), el tótem y las palomas y lobos del hechizo
  (fundido).
- **Las unidades importan.** El umbral de 0,001 y el tope del determinante dependen de T, así que un Zoomer tiene que
  ir en la unidad del original. El del tótem (TotemStatue +0x9C) va en **milisegundos**: SetWorshipPercentage 0x738270
  le da T = |Δ|·5200 [0x999A98] ms (con el umbral en ms, 0x738293, y una copia en línea de 0x407D60 desde 0x7382FC), y
  TotemStatue::Draw 0x738960 lo actualiza con los ms enteros del reloj (0x738967..0x7389B5). openblack le pasa ms y
  `segundos·1000` **(aproximado: no son ms enteros)**. En segundos, el tope habría frenado los cambios de |Δ| < 0,0095;
  en ms solo frena los de |Δ| < 9,5·10⁻⁶.
- Los llamadores no repiten el umbral: `Animal::SetTowardsAngle` llama a 0x407D60 directamente (0x4185D5), y el umbral
  de 0x407D67..0x407DA8 ya hace el `SetPosition`.
- Desaparece `ZoomInterpolator` (la cámara del jugador). Era la misma cuártica con t normalizado, no una quíntica. Sus
  diferencias eran estas:
  - con p0 == p1 ignoraba la velocidad;
  - dividía por p1 − p0;
  - extrapolaba con t > 1;
  - no tenía el umbral de 0,001 ni el tope del determinante.
- El `Vec3Zoomer` de ScriptCamera (sesión asistente) sigue siendo una copia de `Zoomer3d` (SetDestination =
  `SetDestinationWithTime`, Value / Destination = `GetCurrentValue` / `GetDestination`): pendiente de que su dueño lo
  migre (ver la Pendiente).

Comprobado: `test_camera` `ZoomerMatchesRecording` recorre las 11 grabaciones del original. Los coeficientes de cada
curva (51 669) y el valor y la velocidad de cada estado (51 636) salen **bit a bit**.

## Pendiente

### MapCoords

Copias de MapCoords que aún no usan `ecs::map_coords` (estado a 2026-10-02, rama `local/sistemas2`):

**Tanda 2 de los aplazados de milagros2, migrada (2026-10-02, sistemas2):**
- `PSys/Rules/Storm.cpp`: el polvo del tornado toma la celda de `MapCoords(LHPoint)` (0x6D2BA3; el port conserva la
  comprobación contra el lado de la isla, que el original no hace); `PotsByCell` usaba `map_coords::CellOf` (se borró en la fase A de map_cell_queries: las ollas van en la cola de la lista fija, y cada celda se recorre fija y luego móvil, 0x6D2327); la búsqueda
  de lo que el tornado se lleva (fn_006D21B0) recorre `map_coords::Spiral` + `AddCells` desde el MapCoords del tornado
  (`ToFixed`, 0x6D228D..0x6D22A7), con `InBounds` en cada celda (0x6D2311), la celda propia por el MapCoords del objeto
  (fn_00604F40), `GetDistanceInMetres` desde el MapCoords **inicial** (0x6D2398, no desde la celda que se recorre) y la
  posición del evento `CanDestroy` como el MapCoords en metros (0x6D23BA..0x6D2419). Su `SpiralStep` se ha borrado.
- `Magic/Objects/MagicTeleport.cpp`: `FastDistance` pasa por `gutils::FastDistance` sobre `FromMetres`;
  `AnyMultiMapFixedNear` (fn_00604C30) recorre `Spiral` + `AddCells` sobre `max(ftol(ceil(2R/10)), 3)²` celdas con
  `InBounds` (0x604CC9); `k_UnitsPerMetre` y `ToUnits` se han borrado.
- `Magic/Spells/SpellForest.cpp`: `spell_forest::ToMapCoords` es `ToMetres(FromMetres(p))` (0x725943..0x72595C: el
  producto por 6553,6 a 24 bits, ya no en double). `CellOf` ya iba por `MapInterface::GetGridCell` (= `map_coords`).
- `Magic/Core/SpellSeed.cpp` (fn_006022C0): la celda y el desplazamiento dentro de ella salen de `ToFixed`
  (0x6022E2..0x602300).
- `ECS/Systems/Implementations/HandSpellSeed.cpp`: el MapCoords del círculo es `ftol(x·6553,6)`, `ftol(z·6553,6)`,
  altitud 0 (0x5D33DD..0x5D3400).
- Paso de double a float (FPU del original a 24 bits, 0x7DEE0D), revisado por milagros2: `CellOffset` de SpellSeed
  (0x6022E2..0x602300), `TopOfObjectsUnder` (0x602388) y `ToMapCoords` de SpellForest (0x725943).

**Rayo y explosión, migrados (2026-10-02, sistemas2):** `PSys/Rules/Lightning.cpp` y `PSys/Rules/Explosion.cpp` ya
recorren sus celdas con `map_coords::Spiral` + `AddCells` desde el MapCoords del origen (`ToFixed`, 0x69024A /
0x6908B7 / 0x67E56A; ya no `(int)(x·0.1f)`), `InBounds` en cada celda y la celda propia por el MapCoords del objeto
(fn_00604F40). El rayo mide desde el MapCoords del objeto en metros (`fild · 10/65536`) con sus sumas en el orden del
x87 (cono 0x690410, círculo 0x690A06, renovar 0x6913BD sin raíz, largo de la rama 0x6923A0), la punta es
`ToWorld(MapCoordsOf) + object::GetHeight` (fn_00691E00, vt+0x42C), el suelo de los puntos de tierra es el del MapCoords
fn_004427B0 (= `ToFixed`: 65536·0,1f es 6553,6f exacto) y el enfriamiento `ftol(AverageLightmapLife / ([0xD01A38]·0,001))`
(0x691072, 0x6928FD) ya no usa el dt. La explosión busca con `gutils::GetDistanceInMetres` (0x74CD70) contra
`object::Get2DRadius` (vt+0x64), el anillo usa `object::GetRadius` (vt+0x60) y `(dz² + dy²) + dx²` (0x67EA1C), los
blancos son el MapCoords como punto (0x67E9E1), el turno es `game_clock::Turn()` (+0x205A40) y el humo dura
`TicksForSeconds(4)` turnos (0x67EEF8). Queda:
- `Lightning.cpp` `CanBeStruck`: el original pregunta antes `IsAvailable` (vt+0x2C, 0x69038E / 0x690997); el port no
  (es de milagros2).
- `Lightning.cpp`: `fn_00690C70` (blancos del gestor), `fn_00691E80` (altura del mapa de luz, ahora `LandAt + 0,1`) sin
  leer; el enfriamiento usa `effect.Random` en float donde el original llama a `PSysRand(int)` 0x6729E0. El original
  pasa el `ftol` directo a `PSysRand` (0x691091 / 0x69292F, sin mirar el signo) y el port se salta `PSysRand` con
  `steps <= 0`; `PSysRand` salta por el puntero [0xD4E0BC], sin leer qué hace con un negativo, así que el comentario
  de `LightmapSteps` («0 ms da +inf, sin enfriamiento») es *(inferido)*.
- `Explosion.cpp`: `manager::CreateSpotVisual` recibe segundos y los vuelve a pasar a turnos con `MsPerTurn()`; el
  original pasa turnos (`CreateSpotVisualWithSpecifiedDuration` 0x63E580, 60 y `TicksForSeconds(4) & 0xFFFF`), así que
  con un turno distinto de 100 ms la cuenta no es la misma. Su posición tampoco pasa por `MapCoords(LHPoint)` 0x603160.
  Además 0x67EEF8..0x67EF2C es una **copia en línea** (`div [0xD01A38]; fild qword; fmul 4; ftol; and 0xFFFF`), no una
  llamada a `NumGameTicksPerSecond` 0x711630; el resultado es el de `TicksForSeconds(4)`, pero el comentario del código
  debería decirlo (es de milagros2).

**Audio** (lo migra «audio» en su B11):
- `Audio/Services/ThingMusic.cpp:81-87`: la ida y vuelta en double.
- `Audio/Services/SoundMap.cpp:133-137, 156-157, 188-193, 336-337`: ya en float y correctas; solo falta usar la API.

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

Copias de distancias que aún no usan `openblack::gutils` (estado a 2026-10-02, rama `local/sistemas2`). La regla ha sido
migrar **solo** donde se ha leído en el binario que el original llama a `GetDistance*` / `hypotenuse`; lo demás se deja.

**Migradas en la tanda 2 (2026-10-02, sistemas2):** `MagicTeleport.cpp` (`Distance2D` = `GetDistanceInMetres`:
DoTeleport 0x5FC818 / 0x5FC826 por el gemelo 0x74CD50, fn_00604C30 0x604CFD, fn_0064D6B0; `FastDistance` por la API),
`MapShield.cpp` (`IsReactionBlockedByShield` 0x72B9B2), `Storm.cpp` (0x6D2398), `SpellStormAndTornado.cpp`
(`ReactToRainOnFire`, fn_0072DCC0 0x72DCE0), `SpellForest.cpp` (fn_005FADF0 0x5FAE30 y fn_007255C0 0x7255CF),
`SpellShield.cpp` (`GetNearestTown` 0x602112 / 0x602193, `IsUnder` 0x72BD3C desde castPos +0xCC, `FindShieldAt`
0x72BA4B desde **originalCastPos +0xC0** y con `Get2DRadius > distancia` estricto: antes medía desde castPos y aceptaba
`<=`) y
`ECS/PotResource.cpp` (`IsCloseToEqual` 0x6053C0, desde `Pot::AddResourceToPos` 0x66F375). Con la raíz de tabla de
GUtils las distancias ya no son exactas (100 m dan 100,02 m: `test_teleport` lo comprueba así).

**De otros dueños, sin autorización todavía:**
- Milagros: `ECS/Influence/Influence.cpp:118-121` (`detail::DistanceXZ`, `std::hypot`, cita 0x74CD70),
  `ECS/Systems/Implementations/VillagerWorship.cpp:161-164` (`FlatDistance`),
  `Worship/WorshipSite.cpp:131, :147`,
  `Magic/Script/CHLFire.cpp:74` y `Magic/Script/CHLSpells.cpp:185`.
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

### Ángulos de GUtils

Estado a 2026-10-02, rama `local/sistemas2`. Solo se ha migrado donde se ha leído que el original llama a esa rutina en
ese punto.

**Con el dueño del wall hug** (es un cambio de estado):
- `MobileWallHug::InitStepsXZ` 0x60BFA0 está copiada dos veces, en
  `ECS/Systems/Implementations/PathfindingSystem.cpp:40-51` (`InitializeStep(ToGoal)`) y
  `ECS/Villager/VillagerScript.cpp:86-93` (`InitStepsXZ`), con `glm::atan` en float y el paso `(cos, sin) · speed`. El
  original: `GetAngleFromXZ` → +0x5C y el paso `StepFromAngle(+0x5C, +0x5A)`. Hay que fundirlas y pasarlas a la API,
  pero `WallHug` guarda la velocidad en metros float y el ángulo en radianes. (PathfindingSystem :59 y :541 tienen
  además sus ángulos de rodeo propios.)
- `ECS/Villager/VillagerCore.cpp:959-961` (`LookAtPos`): lee el ángulo de juego como `lround(yAngle · 2048 / 2π)`. Lo
  fiel es guardar el `u16` +0x5C (`SetGameAngle` 0x60DA90 lo guarda tal cual; `SetYAngle` 0x60DAC0 con
  `ConvertAngle3DToGame`). Mientras no exista, el `lround` es lo correcto: `ConvertAngle3DToGame` daría a − 1 en 365 de
  los 2048 ángulos que escribe `setGameAngle`.

**Avisar a «animales»**: los cambios de `AnimalAI.cpp` (`AngleDiff`, `IsPosValidForTurnAngle`, `CalcRandomPos`, los
usos de `AngleOf`), `AnimalBirds.cpp` (formación y `BirdDying`), `AnimalFlee.cpp`, `AnimalPredators.cpp` y
`AnimalWallHug.cpp`.

**Revisar con el dueño de Worship**: con la altitude a 0 de `GetSpellIconPosFromSlot`, los iconos de los anillos > 0
quedan en el suelo (el del anillo 0 conserva la altura del punto especial).

**Aplazado (dueño)**: `PSys/Rules/Lightning.cpp:212` y `PSys/Rules/Storm.cpp:1504` (atan2 de PSys),
`Magic/Objects/MapShield.cpp:180` y `Magic/Spells/SpellForest.cpp:422` (la espiral polar de 0x725830): ninguno es copia
de GUtils, no hace falta tocarlos.

**Ya exactas, solo estilo**: las conversiones a mano `a · 2π_f / 2048` que quedan en `AnimalAI.cpp` (`FaceAngle`,
`SetTowardsAngle`) dan bit a bit `ConvertGameAngleTo3D` (sin el `& 0x7FF`). `AngleOfRotation` es la inversa de
`FaceAngle`, no una rutina del original.

**Sin copia que migrar**: `GScript::CastSpellAtPos` 0x70BDD1 calcula el ángulo y lo tira;
`Living::GetFleeingPositionFromStationaryObject` 0x5F2010 normaliza en float también en el original; 0x463670
(Citadel) es código muerto. FishShoals:207, TestDispensers:281/304, PhysicsObjects:490, Rivers:46,
FishFarmArchetype:68, WorshipSite:721, Climate:224 y SpellWater:179 hacen su propia trigonometría; Sharks:110 es
`LH3DMath::GetYAngle` (LH3D).

**Sin portar** (no hay copia, la API ya los tiene): la sobrecarga de cuatro enteros 0x74D220 (3 llamadores, ninguno
portado), `GetXByAngleMetersDistance` (`Creature::GetMovementDirection`, `GetRandomLookAhead`,
`RunAwayFromObjectReaction`), `StepFromAngle8` (Villager `Approach*`, PuzzleHorse), `GetLHPointFromAngle`
(`SetupInspectObject`) y las funciones de `ecs::object` que aún no llama nadie.

**Cambios que se ven y hay que comprobar con captura:** los iconos de hechizo de los anillos exteriores del lugar de
culto (ahora en el suelo), hacia dónde gira un animal que mira justo al revés de su meta, y las posiciones de trabajo
junto a árboles y bosques (con la altitude del árbol).

### Tamaño de los objetos

Estado a 2026-10-02, rama `local/sistemas2`. La regla ha sido migrar **solo** donde se ha leído qué nivel usa el
original (la llamada virtual o la lectura en línea).

**Migradas en la tanda 2 (2026-10-02, sistemas2):** `Storm.cpp` (`CanSuckUp` 0x6D214C y la búsqueda 0x6D238A:
`object::Get2DRadius`), `SpellForest.cpp` (fn_005FADF0 0x5FAE40: `Get2DRadius`, con el campo de 5 m de la API),
`SpellSeed.cpp` (fn_006022C0: el radio de la semilla es `MeshRadius2D` de la malla de su info × `GetScale`, 0x6022D9;
`GetTopPos` vt+0x630 en 0x602388; `Get2DRadius` vt+0x64 en 0x6023E9 / 0x6023F4), `MapShield.cpp` (`Get2DRadius`
0x72B908 / 0x72B9C2, `GetHeight` 0x72B948; `CollisionScale` es `object::GetScale`; las copias `map_shield::Get2DRadius`
/ `GetHeight` se han borrado), `MagicTeleport.h` (`k_Radius = object::k_MagicTeleportRadius`) y `EffectValues.cpp`
(`ApplyEffectToMapPos`: `GetHeight` vt+0x42C en 0x52536E). Los envoltorios `effects::ObjectHeight` / `Object2DRadius`
se han borrado.

**Sin migrar (dudosas o con más cambio que una sustitución):**
- `HandPlacement.cpp:411-422` (pila bloqueada): el original no mide ahí la pila con `GetHeight`. Con
  `IsLockedInInteract` (vt+0x6A0, 0x5B3EB3) toma la posición guardada en CHand+0x78, la pasa a MapCoords con
  `ftol(x · 65536 · 0,1)` (0x5B3ECD..0x5B3F26; es `ToFixed`: multiplicar por 2^16 es exacto y 0x3DCCCCCD · 2^16 =
  0x45CCCCCD = 6553,6f, así que redondea el mismo número real que `x · 6553,6f`, como en fn_004427B0), mide `GetAltitude` (0x5B3F3B) y llama a
  `GetHeightForHandAboveInteractObject` (vt+0x64C, 0x5B3F49). Falta leer qué hace con eso en 0x5B3FDE; cambiarlo toca
  el estado de la mano.
- `HandPlacement.cpp:719-725` (radio del ser vivo bajo la mano) y `:741-749` (radio de lo sostenido): sin dirección.
- `HandTrees.cpp:202-207` (el tronco del árbol talado: `0,2 × Get2DRadius` = `GetHoldRadius` de Tree, 0,3 sin malla) y
  `:331-341` (el polvo de las raíces: otra fórmula, semiejes sin escala): sin dirección del original.
- `3D/Foliage.cpp:506-508` (mod `world.foliage`): no porta nada del original; con `object::Get2DRadius` cambiaría lo
  que se ve (el campo pasaría a medir 5 m), así que se deja.

**De «audio» (hito B11):** `Audio/Services/LanternSounds.cpp:92, :131` llaman a `Rocks::Height`, que ahora es
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
`effects::Object2DRadius` / `ObjectHeight` (aplazados de milagros2): migrados en la tanda 2.

**Sin portar:**
- `Creature::Get2DRadius` 0x477F40 / `GetRadius` 0x4792C0 leen el LH3DCreature (`[[+0x160]+0x58]+0x5228`), que openblack
  no tiene: por ahora una criatura usa la fórmula de `Object` **(inferido)**. La altura (0x477F50 = tamaño × 15) sí está,
  tomando la escala del `Transform` como el `GetUserSize` 0x4EF4F0 **(inferido)**.
- La rama con criatura de `GetRoutePlanRadius` 0x6384D8 (necesita `NavRadius` 0x480A60).
- `Creature::GetBoundingSphere` 0x479970 (`LH3DCreature::GetBoundingSphere` 0x47F8D0): la criatura usa la de `Object`
  **(inferido)**.
- Las clases que no son `Object` (Citadel, SpellShield, SpellStormAndTornado, Town, GArena, Reaction, BuildingSite,
  AtomCore, GStreetLight, Mist: ver arriba) no pasan por la API; si alguna llega a pedirla, hay que añadir su rama.
- `GetNearestPosOfObject` 0x636D30: portado (ver [Ángulos de GUtils](#ángulos-de-gutils)); aún no lo llama nadie en
  openblack.
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

**Migradas en la tanda 2 (2026-10-02, sistemas2):**
- `MapShield.cpp`: la fracción de `DrawShields` es `game_clock::TurnFraction()` (`PhysicalShield::DrawShield`
  0x72CEEC / 0x72CF01, g_game +0x205D64); `g_LastTurn` y su reloj de pared se han borrado.
- `HandSpellSeed.cpp`: `game_clock::Turn()` en vez de su `CurrentTurn()`.
- `MagicLoop.cpp`: los segundos del turno de `spell_sounds::ProcessTurn` (fn_006D11A0 0x6D11AB..0x6D11C5) y de
  `hand_grain::GameTurnUpdate` (`CHand::GameTurnUpdate` 0x46E4E3..0x46E4FB) son `MsPerTurn() · 0,001f`: el original
  lee ahí [0xD01A38], no el 0,1f a mano.
- `FireGraphic.cpp`: las ráfagas de vapor y humo leen `game_clock::Turn()` (fn_00731AB0 0x731AF8 / 0x731B27,
  fn_00731E50 0x731E7B / 0x731EAE); `g_Turn` se ha borrado (`SetTurn` queda para la traza).
- `SpellSeedGraphic.cpp`: el turno de fn_00727350 (0x72736E) es `game_clock::Turn()`.
- `RendererMists.cpp` (fn_007FA300 0x7FA3BE), `RendererSmoke.cpp` (fn_007F8E00 0x7F8F25) y las nubes y la alineación
  del cielo de `Renderer.cpp` (fn_005E25C0 0x5E25FD, `GLandAlignement::DrawSky` 0x5E2160) hacen `fild
  g_game_time_inc`: usan `FrameGameMs()` en vez de su `static lastTime` con tope de 100 ms y la velocidad dividida.

**Siguen sin migrar:**
- `Graphics/Renderer.cpp`: el brillo del sol (reloj de pared sin pausa ni velocidad: qué dt usa el original es
  **(inferido)**).
- `Magic/MagicLoop.cpp` `magic::Update`: pasa `FrameGameSeconds() · 1000` a `one_off::UpdateFrames`,
  `mist_atoms::SubmitFrame` y `chain_atoms::AdvanceScroll`; el original les da los ms enteros (`FrameGameMs()`), y la
  ida y vuelta por 0,001f puede no ser exacta.
- Los lectores de `magic::k_TurnMs` (13 usos, `SpellSeedGraphic::ProcessTurn` entre ellos): es la constante
  `k_MsPerTurn`, no `MsPerTurn()`; da lo mismo mientras nadie cambie [0xD01A38].
- **Hecho en U7** («sistemas»): `Renderer::UpdateClouds` (nubes, alineamiento del cielo y `night_lights::Update`),
  `CollectChimneySmoke` y `CollectMists` leen `FrameGameMs()` en vez de su `static lastTime` de pared con tope de 100 ms.
  Lectores de [0xEA9EC0] en el original: DrawSky 0x5E2160, fn_005E25C0 0x5E25FD, fn_00823460 0x8234B6, fn_00823570
  0x82359F, fn_007F8E00 0x7F8F25 (el humo recorta a 100 **s**) y fn_007FA300 0x7FA3BE. (pendiente) el «rescan» de 1 s
  de `night_lights::Update` toma el mismo ms; de dónde sale en el original no está leído.
- `Game.cpp:610/612` (campos y árboles con dt real): **(inferido)**, sin leer en `Field::Draw` 0x5286D7 ni en
  `Tree::PreDraw`; si es `g_game_time_inc` (0x5286D7 lo lee) hay que pasarles `FrameGameSeconds()`.

**De audio (hito B11):** `Audio/Services/SoundTags.cpp:145` (`k_MsPerTurn` local, marcado «(inferred)»: es [0xD01A38],
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

### Matrices, Zoomer y normal

Estado a 2026-10-02, rama `local/sistemas` (informes `dev\tmp_dis\unify\U8_object_matrix.md`, `U9_zoomer_normal.md`
y `U8_changes.md`):
- Las rotaciones sin fuente de [Matrices LH](#matrices-lh) (HeldSway, bandas, árbol tumbado y tirón, flexión, sol,
  templo, mano, aldeano, almacén, ríos): se dejan como estaban hasta leer sus constructores.
- `Game3DObject::SetPositionAndXZYScale` 0x63B390 (`T(p)·Ry(−a)·diag(s·xz, (s·xz)·(y/xz), s·xz)`) y
  `Game3DObject::SetPosition` 0x63B740 no tienen función propia: openblack guarda la rotación y la escala por separado
  en `Transform`, y ningún sitio las necesita.
- `script_camera::Vec3Zoomer` (asistente) es una copia de `Zoomer3d` con la misma salida: pendiente de migrar.
- La cámara del jugador y la del guion tienen cada una sus `Zoomer3d`; en el original son los mismos de GCamera
  **(inferido)**. El dt de la cámara del jugador es el del fotograma en µs, no los ms enteros de `GetCameraTimeInc`
  0x555820 **(aproximado)**.
- `CameraModeNew3`: faltan el segundo ×2 de la duración, el 1,0 de `MaintainSpell & 0x40` y `[esp+0xB8]`
  (0x4601A9..0x46024A). No es del Zoomer.
- Mano: el Zoomer3d 0xD13FB0 del «arriba» al sostener (0,4 s), el estiramiento del tirón +0x11C (0,3 s) y las
  condiciones de 0x5B4251..0x5B42CD del alabeo. El sentido del giro del alabeo **(inferido)**: falta leer el eje de
  0x5B49C8 y lo que hace fn_007FAFF0 en 0x5B4AE5. La suma de 0x5B42DF pone c4·b antes que c3·a, otro orden que
  `Zoomer::Update`: el último bit **(aproximado)**.
- El FOV del jugador (TODO #707) no es un Zoomer.
- `LandIsland::GetNormalAt` compara con `GetCellsPerSide()` en lugar de 0x200 (igual en los mapas del juego).
- Las tablas de la normal se calculan como en fn_00803890 suponiendo la FPU a 24 bits durante la inicialización de la
  isla **(inferido)**.
- El dt del Zoomer del tótem: `segundos·1000` y no los ms enteros de 0x738967..0x7389B5 **(aproximado)**.
- `PSys/Rules/Shield.cpp`: las celdas del cuaternión de fn_0057D0B0 (el sentido sí está leído) **(aproximado)**.
- La interpolación del escudo físico entre turnos: el original interpola la matriz con la escala dentro (0x72CEEC);
  openblack interpola por separado las filas de la rotación y la escala.

## Ganchos de prueba

**Comprobado en el juego (2026-10-02, sistemas2, build = hand-hbn cc13b6b9, `--mod game.skip-intro=off`):**
- Culto (`OPENBLACK_TEST_WORSHIP="1,0.5"` + `OPENBLACK_WORSHIP_TRACE=1`, Land 2): van los 11 aldeanos más cercanos al
  lugar (232–278 m); con el fallo anterior iban los más lejanos. El desempate por vida³ no se ve (la vida no sale en la
  traza).
- Fuego (`OPENBLACK_TEST_FIRE="1785.2,2652.6,450,abode,20"` + `OPENBLACK_FIRE_TRACE=1`, Land 1): los 12 aldeanos cercanos
  reaccionan y lo apagan (215 → 220 ⇄ 216). **Sin comprobar en el juego:** el término de 400 m con aldeanos a 150–300 m
  de su pueblo (no hay gancho; solo `test_gutils_distance`).
- Curación (`OPENBLACK_TEST_HURT_VILLAGERS="1814.0,2660.5,30,0.3,0,200,1,0"`): cura las celdas que recorre la espiral
  (aldeanos a 13,3 m con R = 10) y no las que no visita, como el original.
- Iconos del lugar de culto (`OPENBLACK_TEST_WORSHIP_SITE="NORSE,0,...,11"`): el anillo exterior a ras de suelo
  (0x77B002), el interior en la plataforma. El paso entre anillos sigue en 7,5 (el original suma 15, 0x77B100): lo
  arregla milagros2.
- Reloj (`OPENBLACK_CLOCK_TRACE=1`, 1356 turnos): 10,01 turnos/s, 50 turnos cada 4,995–5,003 s con una sola partida; los
  tirones de carga se recuperan sin perder turnos.

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
- `test_gutils_angle` (`test/test_gutils_angle.cpp`) comprueba:
  - las constantes por bits;
  - las dos tablas contra el volcado del exe (sumas, una suma ponderada y entradas sueltas) y `COS = SIN + 512`;
  - `LHArcTan` en los ejes, las diagonales, un punto por octante, el desbordamiento del `shl 8` y el error ≤ 2,27
    pasos frente a atan2;
  - las conversiones: −0,5 → 1886, el NaN, los 365 ángulos que pierden 1 en la ida y vuelta, y Scawen por bits;
  - `StepFromAngle` con `whole` negativo (`sar`), el `imul` de 32 bits de 0x74D320, el (0, 0) de
    `GetPosFromGameAngle(a, 10)` y los 4 bits perdidos de 0x74D6A0;
  - `GetPosFromAngle` con dos casos en que `cosf` da otra unidad, y `AddDistanceFromAngle`;
  - `GetAngleDifference` y `GetAngleDirection` en ±0x400;
  - `MapCoords::operator+` / `operator-` con la altitude.
- `test_object_metrics` `PointsAroundAnObject`: las seis funciones de puntos alrededor de un objeto, cada una con su
  radio y la altitude de `this`.
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

- `test_zoomer` (`test/test_zoomer.cpp`):
  - `SetPosition`, el umbral de 0,001 s (también NaN y negativos) y la llegada en `t ≥ duration` sin extrapolar;
  - el paso de 0 a 10 en 2,5 s (2,6171875 a T/4) y el tope del determinante con T = 0,05 / 0,04 / 0,02 s, por bits;
  - velocidades de inicio y destino;
  - dos fotogramas grabados de la cámara del original;
  - `Zoomer3d`.
- `test_camera` `ZoomerMatchesRecording`: cada estado de zoomer de las 11 grabaciones, coeficientes y valores bit a
  bit. `ValidateRecordedData` carga los zoomers grabados en la `Camera` y usa `Camera::UpdateZoomers`.
- `test_lh_matrix` (`test/test_lh_matrix.cpp`):
  - adónde va cada eje con un cuarto de vuelta de cada constructor (los signos);
  - las igualdades bit a bit que salen de las celdas: `YXZ(a, 0, 0) = AngleY(a)`, `RotateY(I, a) = AngleY(a)` y
    `AxisAngle(X, a) = AngleXYZ(a, 0, 0)`;
  - la composición en glm de cada uno y las que no son (izquierda / derecha, `eulerAngleXYZ`, los +ángulos);
  - el tope de `Inverse` (±100 en lugar de 1e4);
  - `SetPosition` y `Model`.
- `test_land_normal` (`test/test_land_normal.cpp`):
  - las dos tablas, por bits;
  - una celda llana (1 + 1 ulp) y una pendiente;
  - los cuatro triángulos (por bits y contra el plano de las tres esquinas);
  - la cuantización de T2 en una celda empinada (longitud 0,998).

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
- Ángulos de GUtils: desensamblado de 0x74D0C0:120, 0x74D200:A0, 0x74D320:180, 0x74D510:180, 0x74D6F0:80,
  0x74DC30:50, 0x74E290:40, 0x605410, 0x6054A0, 0x605520, 0x6055C0, 0x636D30:110, 0x639550:60, 0x74C040:70,
  0x439240:50, 0x439360:70, 0x53A060:70, 0x6E7560:200, 0x77AFC0:C0, 0x75AA90:F0, 0x75B320:80, 0x41B210:140,
  0x418CD0:60, 0x41E930:F0 y 0x41F1B0:80; tablas 0xC2307C y 0xC31614 volcadas del exe. Informes:
  `dev\tmp_dis\unify2\angles_original.md` (con su «Verificación adversaria», que manda) y `angles_openblack.md`.
- Matrices, Zoomer y normal (2026-10-02, sistemas):
  - desensamblado de 0x7FAC10:B0, 0x5198F0:70, 0x86AFA0:70, 0x674200:160, 0x674360:50, 0x6743E0:60, 0x6A1150:D0,
    0x6A1218, 0x6A12C7, 0x6A12F0, 0x6A6250:70, 0x7FB180:110, 0x7FB290:160, 0x423140:250, 0x407D60:170, 0x442720:90,
    0x441F80:C0, 0x5B4200:180, 0x803630:260, 0x803890:F0 y 0x516AA0:50;
  - constantes leídas del exe: 0x9A2BEC, 0x9A2BE8, 0xC3720C, 0x8AB41C, 0xC371D4, 0x8AA3B0, 0x8AB26C, 0x8AB268 y
    0x8AB414;
  - informes `dev\tmp_dis\unify\U8_object_matrix.md`, `U9_zoomer_normal.md` y `U8_changes.md`.
