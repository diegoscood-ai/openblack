# Consultas de agua y máscara `LandAvoid`

Dos módulos del original que responden "¿dónde hay agua?" y "¿por dónde puede andar la criatura?". Ninguno tiene
consumidor todavía en openblack (faltan los trabajos de los aldeanos y la criatura), pero los dos están portados y
probados: `src/ECS/WaterQueries.{h,cpp}` y `src/3D/LandAvoid.{h,cpp}`.

## 1. Consultas de agua (`GUtils`, `GStream`, `Abode`)

Las posiciones del original son `MapCoords`: x y z en 16.16 de celda (mundo × 6553,6 truncado, 10 unidades de mundo
por celda) e y = altura **sobre el suelo**, no altitud del mundo. El puerto mantiene ese detalle dentro del módulo y
expone `glm::vec3` en unidades de mundo.

| Función | Dirección | Qué hace |
|---|---|---|
| `GUtils::GetDistanceInMetres` | 0x74CD70 | distancia xz de dos `MapCoords`: `hypotenuse(int,int)` 0x74F680 (16.16) × 10/65536 |
| `hypotenuse` / raíz inversa | 0x74F680 / 0x74F620 | raíz inversa por tabla de 1024 mantisas (`crt_xc_fn_atexitCleanupReg_Utils_0074F580`): ~10 bits, error ≈ 0,1 % |
| `GUtils::FindNearestCoastalTo` | 0x74E2E0 | espiral cuadrada de celdas desde el punto hasta encontrar una celda **dentro del mapa de juego** y `MapCoords::IsCoastal`, o hasta que la espiral se aleja más que el radio (máx. 999999 pasos) |
| `GUtils::Spiral` | 0x74D7E0 | el paso de la espiral: tabla 0xDA59FC = (+1,0), (0,+1), (−1,0), (0,−1), con `dir/2` pasos por tramo; suma **al word alto**, así que la fracción de celda del punto de partida no se pierde (`operator+=` 0x605470) |
| `GStream::FindNearestPosTo` | 0x733D30 | el punto de río (`CREATE_STREAM_POINT`) más cercano en xz, **estrictamente** más cerca que el radio; la y del resultado es la altitud del punto menos el suelo allí |
| `GUtils::FindNearestDrinkingWater` | 0x74E3A0 | primero el río; si lo hay, busca además una costa que no esté más lejos **del punto de partida** que ese río (si la encuentra, gana la costa) y devuelve "encontrado" en cualquier caso; sin río, la costa dentro del radio |
| `Abode::FindNearestDrinkingWater` | 0x407020 | lo anterior desde la casa a su caché: bit 0 de +0x7C = encontrado, +0x80 = la posición (**solo cambia si encuentra algo**) |
| `Abode::GetNearestWaterPos` | 0x405FC0 | lee esa caché (falso si el bit está a 0) |

Radios del original: **200** al crear la casa (`Abode::Abode` 0x4013E0), **400** el pastor cuando la casa aún no tiene
agua (`Villager::ShepherdMoveFlockToWater` 0x768CC0), **500** el tigre (`Tiger::CalculeLairPos` 0x4214DD).

Detalles que importan:

- La espiral **para en cuanto se aleja del radio**, no recorre el cuadrado completo: por eso no siempre devuelve la
  costa estrictamente más cercana, sino la primera de la espiral.
- `IsCoastal` (0x6036A0) es **tierra** en la línea de costa (`!hasWater && coastLine`), no agua: el agua potable es una
  celda de tierra junto al mar, y el bebedor se acerca a ella. Los predicados están en `ECS/SeaCells`.
- `Tiger::CalculeLairPos` (0x421470) llama a `FindNearestDrinkingWater(pos, 500)` **para cada bosque** pero no usa el
  resultado para colocar la guarida: la respuesta solo fija la distancia que compara con la puntuación del bosque
  (`fn_0053AD00`, `SigmoidThreshold` de árboles y distancia), y esa distancia no se lee después. La guarida sigue
  siendo el bosque mejor puntuado, así que `ECS/AnimalPredators.cpp` no cambia (solo lo anota).

## 2. Máscara `LandAvoid` de la criatura

`LandAvoid` (0xD559B0) son 512×512 bytes `[z][x]`, uno por celda del terreno, que se construyen **una vez por paisaje**
en `GLandscape::Open` (0x5E5541) con `ValidateLandAvoid` (0x6E7BA0) y `FloodAnalyse` (0x6E7FA0, `RoutePlan.cpp`):

1. Para cada celda, las altitudes de sus **cuatro esquinas** (`(x,z)`, `(x,z+1)`, `(x+1,z)`, `(x+1,z+1)`; 0 fuera del
   mapa o sin bloque) × 0,67: si `max − min > 10` → **1** (demasiado inclinada); si las cuatro son 0 → **1** (mar
   profundo); si no → **4** (candidata andable).
2. La semilla del flood es la última celda 4 de la última tirada de celdas 4 que termina en un 1 (por filas: el
   contador se reinicia en cada fila; una tirada que llega al final de la fila no cuenta). En Land1 sale **(191, 362)**.
3. `FloodAnalyse` inunda los 4 vecinos ((−1,0), (0,−1), (+1,0), (0,+1)): las 4 alcanzadas pasan a **0** y las 1
   vecinas del flood a 5. Al final 1 → **2**, 4 → **2** (no alcanzables) y 5 → **1**.
4. Las celdas 0 cuyo terreno tiene el bit de agua (o no tiene bloque) pasan a **6**: agua por la que se puede andar.

Valores finales: **0** tierra alcanzable, **6** agua andable, **1** celda a evitar pegada a la zona alcanzable, **2**
inalcanzable. `fn_00483890(pos, r)` (r = 7,1 en `fn_00483850`; 7,05 en `fn_00483870`, giro) acepta la posición si su
celda es 0 o 6 y ningún centro de celda `(10i+5, 10j+5)` del 3×3 vecino que no sea 0/6 queda a menos de r.

La criatura, por tanto, **vadea** hasta donde las cuatro esquinas de la celda tienen altitud 0 y no nada (en
`ctrspec27.txt` no hay animación de nadar). Los lagos de Land1 (altitud 1 con agua) también salen **6**: se puede
entrar en ellos.

En openblack: `land_avoid::Validate(island)` se llama al cargar el paisaje (`Game::LoadLandscape`), `At(x, z)` da el
valor y `IsPosValid(pos, radio)` es `fn_00483890`. Recuento en Land1: 12 949 celdas 0, **1866** celdas 6 (el anillo de
agua somera y los lagos), 3671 celdas 1 y 243 658 celdas 2.

**Gancho** `OPENBLACK_DUMP_LAND_AVOID=1` (o `=<fichero>.png`) vuelca la máscara al cargar el paisaje: un píxel por
celda, x a la derecha y z hacia abajo, **verde** 0, **azul** 6, **rojo** 1, **gris** 2 (magenta = valor imposible), y
escribe en el log la semilla y los recuentos. Comprobado con los puntos de Land1: `(146,201)` mar abierto → rojo (mar
profundo junto a la zona alcanzable), `(147-148,201)` costa somera → azul, `(149,201)` y `(178,271)` tierra → verde,
los lagos `(213,241)` y `(216,309)` → azul.

## 3. El agua en los guiones (CHL)

- **`GET_LAND_HEIGHT`** (`GScript::GetLandHeight` 0x6FB1F0): celda `(int)(x·0,1)`, `(int)(z·0,1)` (truncado hacia 0, así
  que −10 < x < 0 sigue siendo la celda 0); fuera de 0..511, sin bloque o **altitud 0** devuelve **−10,0**
  (0xC1200000, "está en el mar"); si no, la altura interpolada (`LH3DIsland::GetAltitude`). openblack devolvía siempre
  la altura: ahora `sea_cells::ScriptLandHeight` hace la regla completa (`CHLApi.cpp`, `GET_LAND_HEIGHT` 151). Lo usan
  los retos `Baywatch` (Land2) y `LostBrother` (Land1). Comprobado: mar abierto de Land1 (1464, 2016) → −10, tierra seca
  (1788,4; 2710) → 28,917.
- **`GET_PROPERTY`** (`GScript::GetProperty` 0x70DAE0, tabla de saltos 0x70E78C sobre `propiedad − 1`): de momento están
  las dos del agua, las demás siguen sin implementar.
  - `FLYING` (5, 0x70DCF8) = bit 6 (0x40) de `Object+0x24` = **el objeto tiene un `PhysicsObject`** (también los
    obstáculos en reposo, igual que `Object::IsActuallyInTheAir` 0x639410).
  - `DROWNING` (6, 0x70DD0A) = la virtual `IsDrowning` (vt +0x17C): `Villager` 0x756B30 = estado **16** DROWNING;
    `Object` 0x63A780 = tiene `PhysicsObject` **y** el centro de masas (po +0xCC) está por debajo de y = 0;
    `GameThingWithPos` 0x4052D0 = 0. En openblack es `ecs::IsDrowning` (`ECS/VillagerDrowning`). La usan los retos
    `CreatureSavingPeopleDrowningMan` y `PiperSetFree` (Land1) y `ThrowBlokeMain` / `EndOfFlyingNutter` (Land3).
  - Con una cosa que ya no existe el original escribe "Thing no longer valid" y apila un **float 0**; openblack hace lo
    mismo (sale a menudo en el log porque los guiones referencian objetos que openblack aún no crea).
- **`SET_PROPERTY`** (0x70F380 → fn_0070E820, tabla 0x70F2BC): `FLYING`, `DROWNING` y `MOVING` van al caso de error
  ("Cannot Set Property %d") y **no cambian nada**; portado así.
- **Enum arreglado**: `ObjectPropertyType` de `src/ScriptHeaders/ScriptEnums.h` juntaba dos entradas en
  `InHandGrabypeSpeed`, así que desde `IN_HAND_GRAB` (10) todo iba desplazado un valor (`SPEED` 11 … `BUILT_PERCENTAGE`
  22, `ZPOS` 25). Ahora son `InHandGrab` y `Speed` como en `bw1-decomp include/chlasm/ScriptEnums.h`. Nadie usaba los
  números antes (comprobado con grep), así que el arreglo no rompe nada.

## 4. Pruebas

`test_water_queries` (Land1 real, ver [openblack-internals.md](openblack-internals.md#tests-y-datos-de-prueba)):
máscara coherente con los predicados de celda, costa más cercana (y que la espiral conserva la fracción de celda),
punto de río más cercano y su y, agua potable no más lejos que el río, caché de la casa y distancia con la raíz inversa
del original.
