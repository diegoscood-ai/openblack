# Coordenadas, terreno, matrices y Zoomer

Matemáticas básicas del motor original (LH3D) y cómo se portan a openblack: el punto fijo de las posiciones, con sus
celdas y su espiral; la altura exacta del terreno; la convención de las matrices LH, y el interpolador `Zoomer`. Todo
es **fiel** (verificado en el ejecutable) y está portado, salvo lo que se marca en [Pendiente](#pendiente).

- [MapCoords](#mapcoords): punto fijo, celdas, `InBounds`, vecinos y espiral (`ecs::map_coords`)
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
- Las distancias (`GetDistance` 0x74CCB0, `GetDistanceInMetres` 0x74CD70): son el sistema siguiente, `gutils_distance`.

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
- Distancias sobre MapCoords: la raíz de tabla 0x74F620 / `hypotenuse` 0x74F680 está copiada tres veces (WaterQueries,
  AnimalLairs, CHLApi), y `TownQueries::GetDistanceInMetres` usa `std::hypot`. Es el sistema `gutils_distance`.
- Lo que queda en double en `TownQueries::GetDistanceInMetres` (`std::hypot`) es del sistema `gutils_distance`.

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
- No tiene variables de entorno propias.

## Fuentes

- Desensamblado W120 (`dev\tmp_dis\bwdis.py`): 0x603160, 0x603340, 0x603430, 0x6041C0, 0x6042C0, 0x605470, 0x605C40,
  0x5E1860, 0x5E1950, 0x74CA10, 0x74CA60, 0x74D7E0, 0x74D810, 0x74F520, 0x74F540, 0x7A1400, 0x525100..0x525260,
  0x63AFF2, 0x7204D0 (0x72056B..0x720595), 0x882730 (0x8827C7..0x882810) y 0x7DEE00. De la 2.ª pasada: 0x6014C0,
  0x54F650+0x2A0, 0x5FBB40..0x5FBD10, 0x72F5C0..0x72F6E0, 0x725000..0x725180, 0x5ED080, 0x41A8B0, 0x41A640..0x41A780,
  0x419490, 0x60FC50, 0x7238C0, 0x420E10, 0x771BE0, 0x772BE0, 0x74CDE0 y 0x7409C0..0x740A60.
- Informes: `dev\tmp_dis\unify2\PLAN.md` §1, `map_coords_grid_original.md` (sobre todo su «Verificación adversaria»)
  y `map_coords_grid_openblack.md`.
- bw1-decomp: `src/Black/MapCoords.h`, `Map.h`, `Utils.h` y `Lionhead/LH3DLib/development/LH3DMapCoords.h`.
