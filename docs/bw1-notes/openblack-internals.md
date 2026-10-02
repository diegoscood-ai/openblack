# openblack por dentro (rama local)

## Compilar, probar, ejecutar

- `C:\Users\diewgarc\dev\openblack\build_openblack.bat configure|build` (desde bash:
  `cmd //c "C:\\Users\\diewgarc\\dev\\openblack\\build_openblack.bat build"`). **Tras añadir .cpp, `configure`**
  (las fuentes se recogen con GLOB).
- Tests en `cmake-build-presets\ninja-multi-vcpkg\bin\Release`: `test_camera` (11 grabaciones del original),
  `test_set_camera_pos`, `test_game_initialize`, `test_load_scene`, `test_mobile_wall_hug`, `test_fixed`,
  `test_zoomer`, `test_lh_matrix`, `test_land_normal`… (todos los `test_*.exe`). Deben pasar todos.
- Copia portable: `B&W\BnW_openblack\openblack.exe` + `Jugar.bat`.
- Sin `-W/-H`, la ventana ocupa el 85 % del escritorio útil y va centrada; tamaños mayores se reducen.
- openblack arranca **en pausa**: los scripts y la búsqueda de caminos no corren hasta quitarla.

## Mapa del código de la mano

| Archivo | Contenido |
|---|---|
| `HandSystem.cpp` | Máquina de estados de pulsaciones, animación, interfaz pública |
| `HandPlacement.cpp` | Geometría de la mano, `Place`, objeto bajo el cursor, `ResolveCursorPoint` (ORHP) |
| `HandHolding.cpp` | Coger, poses, muelle, soltar, lanzar, objetos lanzados |
| `HandResources.cpp` | Montones, vasijas, coger por tandas, dejar, almacenes |
| `HandTrees.cpp` | Arrancar, raíces, replantar, árboles muertos |
| `HandEffects.cpp` | Polvo al agarrar, partículas al coger (grano, madera, destellos de pez) |
| `HandFish.cpp` | Chapoteo al agarrar el agua, pescar en las piscifactorías |
| `HandDebugHooks.cpp` | Todas las variables de entorno de prueba |
| `Common/Zoomer` | Zoomer y Zoomer3d de LH3DLib ([engine-math.md](engine-math.md#zoomer-lh3dlib)) |
| `3D/ObjectMatrix` | `lh_matrix`: constructores de LHMatrix ([engine-math.md](engine-math.md#matrices-lh)) |
| `3D/LandNormal` | `land_normal`: LH3DIsland::GetNormal ([engine-math.md](engine-math.md#normal-del-terreno)) |
| `ECS/StoragePitStore` | Lógica del almacén |
| `ECS/StaticGrounding` | Mod: asentar objetos estáticos |
| `Graphics/TextureMipmaps` | Mod: cadena de mips en CPU |
| `Archetypes/PotArchetype` | Crear vasijas/montones, `SetSize`, `UpdateSizes` |

## Render

- Matriz de instancia (mat4 por objeto): se aprovechan las `w` de las columnas de rotación:
  - `[0][3]` = 1 − opacidad (`components::Alpha`; con la tabla 0xC387C8, en la cola si su malla tiene la marca 0x200 y
    si no al momento en la vista principal);
  - `[1][3]` = desplazamiento de textura V (`components::UvScroll`);
  - `[2][3]` = `components::MeshTint` (1e6 y más; ver mod-library.md). El shader de huellas usa solo xyz de esas columnas.
- `MorphWithTerrain` (el original: `LH3DObject::UpdateMelting` 0x8168F0; objetos de tipo 3D 1 = morphable, ver
  `dev\tmp_dis\morph\morph_notes.txt`; la API común es `land_morph`, ver
  [rendering-objects.md](rendering-objects.md#mallas-pegadas-al-suelo-land_morph)): cada vértice sube `GetAltitude(xz del vértice) − GetAltitude(xz del origen)`,
  así que la altura propia del objeto (hundirse una pila o un campo) se conserva. vs_object calcula GetAltitude exacto
  (las 4 esquinas de la celda sin filtrar, su diagonal `split` y el aplanado junto al mar) con el mapa de alturas RG32F
  (altitud, split; las 17×17 celdas de cada bloque). Antes era bilineal y desalineado media celda (errores de 8-50
  unidades en los bordes del mapa: huecos bajo campos y edificios). Los objetos que se desvanecen (`Alpha`) siguen
  pegados al terreno (en el original pasan por el mismo `Draw`).
- Los edificios que **no** siguen el terreno (casas de todas las tribus, molino, dispensador, tótem, maravillas de las
  tribus 1, 2, 5 y 6) se hunden al crearse (`Abode::CallVirtualFunctionsForCreation` 0x403270, en
  `AbodeArchetype::Create`): hasta el suelo más bajo bajo las 4 esquinas xz de la caja de su malla
  (`GetAltitudeFondation` 0x63ABC0, nunca por encima del origen), como mucho `max(0,2·radio 2D, 0,8)` (radio =
  escala × la mayor semiextensión en x o z, 0x638180). Sustituye a la altitud del script. Solo con una isla cargada
  (`UnloadedIsland` lanza en `GetHeightAt`). El templo aplana el terreno a su alrededor
  (0x882730: plano hasta 35 unidades, mezcla hasta 70; hecho en `CitadelArchetype::FlattenLandUnderTemple`).
  **Desviación:** openblack lo hace al cargar (crea el templo ya hecho); el original, al convertir el plano en templo
  (`CitadelHeart::Create` → 0x4675A0 → 0x882730; `dev\tmp_dis\mapa\flecos_citadel.md`). **Pendiente:** la entrada
  (`Entrance.l3d`) sigue el terreno.
- **Tótem del centro del pueblo** (`components::TotemStatue`, `CreateTotemStatue` en AbodeArchetype.cpp; notas en
  `dev\tmp_dis\totem\totem_notes.txt`): `TownCentre::CreateTotemIfNecessary` 0x743DA0 → `TotemStatue::Create`
  0x737CC0. Dos mallas estáticas sin hundimiento: el pedestal de la tribu (`InfoConstants.totemStatue[tribu].plinth`,
  BuildingPlayerIconPlinth*) en el punto especial 6 del centro (`GetTotemPos` 0x743F20, con la matriz del centro y
  subido como su morph), con su ángulo Y y escala; encima (+2,729, 0x999A9C) el icono: la criatura del jugador
  (BuildingPlayerIcon<Especie>) o, sin criatura, la mano (BuildingSpellHand, lo único que hay ahora). Suben
  `8 × fracción de culto` (Draw 0x738960; aún sin culto: 0). Las creencias (GBelief::DrawBelief 0x438800) van a
  `y del punto 6 + alto de la malla del icono × escala` (Object::GetHeight 0x638120). Pendiente: mirar al lugar de
  culto (AddToPlayer 0x738130) y el icono de la criatura.
- El búfer de instancias crece con margen y se sube con `bgfx::copy` (con `makeRef` y un `resize` se leía memoria
  liberada: artefactos al crear y destruir mallas cada fotograma).
- `L3DSubMesh` guarda en CPU posiciones e índices (`GetCollisionPositions/Indices`) para picking y medidas;
  `L3DMesh::RayIntersect` hace el test de triángulos.
- Texturas: si un material pide una textura inexistente y la malla trae skin incrustada, se usa esa (mods).
- **Trampa**: ninja no recompila las variantes que solo hacen `#include` de su base al cambiar la base
  (`vs_object.sc`, `vs_static_shadow.sc`…): `vs_object_instanced*.sc`, `vs_object_hm_instanced*.sc` y
  `vs_static_shadow_instanced_static.sc` (7 archivos) hay que tocarlos (`touch`). Con shaders viejos desaparecen
  objetos (todos, o los árboles). `dev\verify_head.bat` ya lo hace antes de compilar (2026-10-01).

- `RenderContext::entityInstances`: entidad → (malla, índice de instancia, `morphWithTerrain`,
  `receivesDynamicShadow`), para dibujar una entidad concreta (reflejos, sombra sobre objetos).
- `LandIslandInterface::GetUnflattenedHeightAt`: `GetAltitude` sin el aplanado del mar (búsqueda de las piscifactorías).

- **Trampa: búfer de uniformes de Vulkan.** bgfx copia en cada llamada todo el bloque de uniformes del vertex shader
  a un búfer por fotograma de 128 B × 65535 = 8 MB, sin comprobarlo en Release. `vs_object` con `u_model[128]` son
  ~8 KB por llamada: con ~1000 llamadas se desbordaba y caía en `ScratchBufferVK::write` (Kapa's Land1). Las mallas sin
  huesos usan las variantes `*_static` (`BGFX_CONFIG_MAX_BONES 1`, `Renderer::StaticVariant`); al añadir shaders
  de objetos, crear también su variante.
  Las mallas con hasta 32 huesos (aldeanos 22, casi todos los animales) usan las variantes `*B32`
  (`Renderer::BonesVariant32`): cada aldeano o animal con pose es su propio draw, y los que quedan fuera de la vista
  no se dibujan (`SphereInView` en el bucle de instancias). Además bgfx va parcheado (overlay de vcpkg
  `vcpkg-overlay-ports/bgfx`, `raise-vulkan-limits.patch`, activado en `CMakePresets.json` con
  `VCPKG_OVERLAY_PORTS`): el pool de descriptor sets pasa de 1024 a 8192 por frame en vuelo (con ~2000-2700 draws
  se agotaba y caía en `getDescriptorSet` dentro del driver: Greek, Tibetan, Demon, Kapa's Land1, Ultimate Sandbox)
  y el búfer de uniformes de 128 a 512 B por draw (32 MB). Ganchos: `OPENBLACK_DRAW_STATS=1` (draws por frame en el
  log) y `OPENBLACK_TEST_MAP_CYCLE="<frames>:<guion>,<guion>..."` (carga el siguiente guion cada N frames, como el
  menú "Load Island"; rutas relativas a Scripts, p. ej. `Playgrounds/TwoGods.txt`).
- **Trampa: `bgfx::makeRef` sobre datos locales.** bgfx los lee más tarde; usar `bgfx::copy` salvo que el búfer viva
  hasta después del siguiente `bgfx::frame()` (tres casos en `LandIsland::LoadFromFile`, ya corregidos).

## Depurar un cierre

- `Common/CrashHandler`: una excepción no atendida o `std::terminate` escriben la pila en stderr y en
  `openblack_crash.txt` (directorio de trabajo). Con nombres y líneas solo si el `.pdb` está al lado: compilar
  `RelWithDebInfo` con `C:\Users\diewgarc\dev\tools\build_rwdi.bat` (sale en `bin\RelWithDebInfo`).
- Cierre sin aclarar (30-09-2026): una build RelWithDebInfo cayó al cargar Land1 con 0xC0000005 en
  `btCollisionWorld::updateSingleAabb` (`stepSimulation`, desde `Game::Update`). No se sabe la causa ni si ya está
  arreglado; si reaparece, buscar un `btCollisionObject` liberado sin `removeCollisionObject`.
- `OPENBLACK_FLUSH_LOG=1`: el registro se escribe línea a línea (no se pierden las últimas antes de un cierre).
- Guiones: `LHScriptX::Script` salta la línea que no entiende (`ScriptError`, `LexerException`) y lo registra
  ("line skipped"); los mapas de escaramuza traen erratas (comilla doble, argumento vacío, palabras sueltas) que el
  original tolera. Un número entero vale donde se espera un decimal y viceversa; los enteros se saturan.
  `CREATE_BASE_WITH_ANGLE` aún no existe (se salta). Los `.lnd` de los dioses guardan en `blockSize` el tamaño de todos
  los bloques juntos.

## Mods

Librería en `src/Mods/` ([mod-library.md](mod-library.md)). Los mods escriben interruptores de `EngineConfig`
(`msaa`, `textureMipmaps`, `anisotropicFiltering`, `livingWater`, `groundStaticObjects`) que lee el motor.
- `world.ground-statics`: baja cada objeto estático hasta que su vértice más bajo toca el suelo (recuerda cuánto en
  `MobileStatic::groundedDrop`). Limitación conocida: una roca apoyada en una punta sigue pareciendo flotar; se
  resolverá con físicas.

## Variables de entorno de depuración

`OPENBLACK_PROFILE=<s>` (resumen del perfilador en el log), `OPENBLACK_CAMERA_FLY="ox,oy,oz,fx,fy,fz"` (el vuelo
tarda unos miles de fotogramas: usar `-n 8000 --screenshot-frame 7900`; con `--mod game.skip-intro=off` la intro de
Land 1 se funde a negro hacia los 90 s de juego (`SetAviSequence` / `ObjectDelete` en el log) y a 7900 salen fotos
negras o a medio fundido, así que ahí usar `-n 6000 --screenshot-frame 5900`, con el vuelo ya quieto, y mirar que
la foto no salga negra; las nubes con `OPENBLACK_CLOUD_SEED` nacen igual pero avanzan con los milisegundos reales,
así que dos fotos no se comparan píxel a píxel en el cielo), `OPENBLACK_DUMP_COAST_ALPHA=1` (o `=<fichero>.png`: vuelca la textura del alfa costero, x a la derecha y z hacia
abajo desde el primer bloque de la isla, que sale en el log; comparar con `tmp_dis\agua\sea_coast_alpha.py`, que
empieza en el bloque 0), `OPENBLACK_DUMP_BLOCK_TEXTURE=1` (o `=<fichero>.png`: la textura de bloque RGBA de toda la
isla, color y alfa costero, misma orientación; comparar con `tmp_dis\agua\re\cmp_block_dump.py`),
`OPENBLACK_PRINT_ALTITUDE="x,z"` (altura de juego, de la malla dibujada y sin aplanar,
terreno físico y objetos cercanos), `OPENBLACK_MARK_LOWEST=1` (marca el vértice más bajo de las rocas cercanas),
`OPENBLACK_SEA_TRACE=1` (cada 500 fotogramas, las filas del mar: primera fila, n, 1/z y su paso, fila superior suave,
fotograma del mar y deriva; ver [rendering.md](rendering.md#mar-skyraw--skyaraw)),
`OPENBLACK_DUMP_STATIC_GAPS=1`,
`OPENBLACK_DUMP_LAND_AVOID=1` (o `=<fichero>.png`: la máscara `LandAvoid` de la criatura al cargar el paisaje, un
píxel por celda, verde 0, azul 6, rojo 1, gris 2; ver [water.md](water.md#máscara-landavoid-de-la-criatura)), `OPENBLACK_HAND_TRACE=1`, `OPENBLACK_HAND_TEST_ROCK="x,z"`
(+ `_FOOD`, `_NO_BOULDER`), `OPENBLACK_HAND_TEST_TREE="x,z[,dead][,roots][,store]"`,
`OPENBLACK_TEST_TREE_GROWTH="x,z"` (dos brotes ahí, uno en un bosque y otro sin bosque: solo crece el primero),
`OPENBLACK_TREE_TRACE=1` (cada paso de crecimiento, los árboles que planta un bosque, el curvado de copas y el brillo),
`OPENBLACK_TEST_REPLANT="x,z,grados"` (suelta ahí un árbol inclinado esos grados y dice si se replanta, cae con físicas
o queda muerto),
`OPENBLACK_HAND_TEST_STORE_TAKE="madera,comida"`, `OPENBLACK_HAND_ANIM=<nodo>`, `OPENBLACK_NO_PICKUP_PSYS=1`,
`OPENBLACK_HAND_TEST_HOLD=<escala>` (la mano empieza sosteniendo una roca),
`OPENBLACK_HAND_TEST_DROP="x,z,segundos[,tipo]"` (la mano sujeta una roca 0, una vasija de 300 de comida 1 o de madera 2,
el primer aldeano 3, el primer árbol 4 o el primer animal 5, y lo suelta suave allí tras esos segundos de juego: sobre el mar debe caer con físicas, ver
[physics.md](physics.md#el-agua-en-los-golpes-y-al-soltar)), `OPENBLACK_TEST_SEA="x,z,tipo[,altura]"` (`villager|animal|tree|pot|rock`: crea ese objeto a esa altura, 2 por defecto, sobre el punto y lo mete en física sin velocidad; el log da la celda, la densidad, el radio y `GET_LAND_HEIGHT` ahí y en la tierra de referencia, y el contador del aldeano que se ahoga cada 100 turnos; con `OPENBLACK_PHYSICS_TRACE=1` se ve hundirse, ver [water.md](water.md#hundirse-ahogarse-y-borrarse); con `OPENBLACK_TEST_CUT=1` además el objeto lleva `components::CutByPlane` y su parte bajo el agua se dibuja cortada en 0xFF303070, ver [rendering-objects.md](rendering-objects.md#cortar-por-el-plano-del-agua-drawcutbyplane)), `OPENBLACK_TEST_SHARK=1` ([water.md](water.md#tiburones-clase-whale); la parte de los tiburones de `FollowUs` en Land 1: dos `SharkArchetype` en `CONVERT_CAMERA_FOCUS(221)` y `(230)` con `WALK_PATH` por las pistas 21 y 20 de `camera.edt`; `="pista,cámara[,adelante[,desde[,hasta]]]"` uno solo; ver [camera-tracks.md](camera-tracks.md)), `OPENBLACK_WALK_PATH_TRACE=1` (cada turno de cada `WALK_PATH`: muestra, tramo, t, punto del foco y posición puesta), `OPENBLACK_TEST_JC_SPECIAL="6[,modo[,fotogramas[,ms]]]"` (el barco de los misioneros, [water.md](water.md#barco-de-los-misioneros-petitnavire), `PLAY_JC_SPECIAL(6)`, esos fotogramas después de tener paisaje; modo 1 empieza en la travesía; `ms` avanza el barco de golpe en pasos de 33 ms para fotos en un momento dado, p. ej. `6,0,7880,7200` con `-n 8000 --screenshot-frame 7900` y `OPENBLACK_CAMERA_FLY="1892,14,3176,1866,6,3161"` da la salpicadura; `OPENBLACK_BOAT_TRACE=1` escribe modo, tiempo, casco y sprites cada 500 ms), `OPENBLACK_TEST_BUILT_PERCENTAGE="p"` (el ArkDryDock de Land 1 donde lo crea `TheMissionaries`, con `BUILT_PERCENTAGE` = p; cámara `1905,22,3180,1881,8,3154`), `OPENBLACK_TEST_FISH_PUZZLE="x,z[,dentro]"` (el `PuzzleGame` 14 del guion en (x, 0, z), procesado una vez: cebo, red de 7 flotadores y los 2 bancos; cuando se cierra la red el turno siguiente escribe `PuzzleGame 14 played`; los bancos solo cuentan con la cámara a menos de 300; en Land 4 `2497.9,3628.35`; con `dentro` = 1 todos los peces empiezan en el cebo y la red se cierra a los 500 ms; con `OPENBLACK_HAND_TRACE=1` escribe `Fish puzzle: inside N/30` y `net closed`, ver [water.md](water.md#puzle-de-los-peces)), `OPENBLACK_TEST_SPLASH="x,z"` (un chapoteo de
la mano por segundo), `OPENBLACK_AUDIO_TRACE=1` (cada arranque de `Audio/SamplePlay` con modo, dueño, canal y ganancia, cada canal parado y por qué, cada sample que acaba y cada corte 3D por la distancia; los emisores de `AudioManager` ya no existen desde la fase B5
del audio),
`OPENBLACK_HAND_TEST_FISH=1` (chapoteo y
pesca en el primer banco con la acción mantenida 3 s; con `OPENBLACK_HAND_TRACE=1` escribe `Fish trace`), `OPENBLACK_START_PAUSED=1` (arranca en pausa como el openblack de antes; por defecto el juego corre desde el primer fotograma, como el original: turnos y
scripts desde el primer fotograma), `OPENBLACK_TEST_FADE="r,g,b,segundos"` (`SET_FADE`), `OPENBLACK_TEST_VIEW_VILLAGER="n[,distancia[,ángulo]]"` (la cámara mira al aldeano n desde esa distancia y lado; `dev\tools\shot_villager.sh` lo lanza desde una copia en `dev\hdp_run` para no bloquear el exe de las demás sesiones; los aldeanos caminan, así que de lejos pueden salir del encuadre; ojo: pone la vista una sola vez, al haber paisaje, y la intro de unos 5 s de Land 1 mueve luego la cámara, así que en una captura tardía la vista ya no está sobre el aldeano: no sirve para capturas de ANTES/DESPUÉS, usar `OPENBLACK_CAMERA_FLY`, que la sujeta cada turno), `OPENBLACK_TEST_WIDESCREEN=1`, `OPENBLACK_TEST_CHIMNEY=all` (todas las chimeneas echan humo aunque no haya nadie en casa; [rendering-objects.md](rendering-objects.md#humo-de-las-chimeneas-lh3dsmoke)), `OPENBLACK_CLOUD_SEED=<n>` (semilla fija del `rand()` de las nubes del cielo; sin ella, la hora como el original), `OPENBLACK_TEST_SKY_ALIGNMENT=<-1..1>` (objetivo de la alineación del cielo: −1 mala, 0 neutral, 1 buena; en vez del deslizador de depuración), `OPENBLACK_LOG_ISOK=1` (una línea `isok:` en el log por cada árbol, vasija u objeto móvil del guion que `IsOkToCreateAtPos` no deja crear, con lo que lo tapa; ver [map-loading.md](map-loading.md#objetos-del-guion-del-mapa-farolas-hogueras-árboles-muertos-puertas)), `OPENBLACK_TEST_HD_TWEAKS=<frame>:<textures>,<smooth>` (cambia el mod HD-Tweaks en ese fotograma, como el menú; `dev\tools\shot_hand.sh` para la mano), `OPENBLACK_SCENERY_TRACE=1` (el decorado fijo de Land 3/4, `ECS/DesignedScenery`: creación, cambio de tierra y cada anillo de la cascada con su V; ver [water.md](water.md#decorado-fijo-por-tierra-cascada-de-land-3-arca-y-dinosaurio-de-land-4)), `OPENBLACK_SOUND_TAG_TRACE=1` (`Audio/SoundTags`: cada etiqueta creada, borrada, soltada o con retardo, y cada 50 turnos el canal de cada una), `OPENBLACK_AUDIO_TEST_VIEW` / `_ANIM` / `_LANTERN` / `_NO_WIDESCREEN` (cámara en un aldeano o en una farola, clip forzado, sin el filtro de la pantalla ancha: [audio.md](audio.md#ganchos-de-prueba)), `OPENBLACK_ATMOS_TRACE=<n>` (cada n turnos las líneas del `GSoundMap::Dump` original — `Sound Map Calc Update X=%d Z=%d %s Count=%d`, `Sound Map At Hand ...`, `Sound Radius=%3.3f Distance=%3.3f DistanceAboveLand=%3.3f` — y de `ProcessAtmosBanks` — `%s Vol=%3.3f Sent=%d Step=%d` por banco —, más una línea "(openblack)" con los 14 volúmenes y sus celdas; y cada arranque de bucle y suelto del ambiente), `OPENBLACK_LANTERN_SOUND_TRACE=1` (escribe `Lantern sound:` en el log: arranque, corte y suelta del bucle de cada farola con su distancia, y cada 50 turnos el número de farolas, si es de noche y la distancia de la más cercana; probarlo con `OPENBLACK_TIME_OF_DAY=22` y la cámara a menos de 5 unidades de la punta de una farola). `OPENBLACK_PSYS_SOUND_TRACE=1`: traza de los sonidos del PSys (inicio con acción, tamaño, superficie, distancia y muestra de spells.sad; "too far" si la cámara está más lejos que el maxDist de la muestra; suelta del bucle o corte al morir el átomo; borrado), ver [magic.md](particles.md#sonido-de-las-partículas-lane-s-srcaudiospellsounds-srcpsysrulessoundcpp). Influencia ([magic.md](magic.md#influencia-m1i-srcecsinfluence)): `OPENBLACK_TEST_INFLUENCE="x,z[;x,z...]"` escribe en el registro, en los turnos 2 y 100, lo que daría `GET_INFLUENCE(0, 0, pos)` en cada punto (con el valor bruto, si hay anillo anti, y los radios de ciudadelas y ciudades); `OPENBLACK_TEST_INFLUENCE_RING="x,z,radio[,anti[,jugador]]"` crea en el turno 1 un anillo como `INFLUENCE_POSITION`; `OPENBLACK_INFLUENCE_EVERYWHERE=1` es la marca "GatheringFlag" del original (influencia 1 en todas partes). Tiempo y clima ([magic.md](day-night-weather.md#tiempo-y-clima-m6a-srcecsweather)): `OPENBLACK_TEST_WEATHER="x,z,radio[,lluvia[,fundido[,temperatura[,sheetMin,sheetMax[,forkMin,forkMax]]]]]"` (los intervalos de relámpago de nube y de rayo ramificado del descriptor +0x30..+0x3C; con ellos `GWeather::Update` arranca el destello fn_00837290, que la tormenta del milagro nunca pide: fn_006D5730 los deja a 0) registra en el turno 1 una tormenta estática hecha como la del milagro de tormenta (interior `max(radio, 60)`, exterior `max(2,5·radio, interior + 20, 80)`, corregido por la lane del milagro de tormenta: los tres `fcomp; test ah, 0x41; je` de fn_006D5730 se quedan con el valor solo si es mayor, lluvia 100, nublado 80, 20 grados, vida casi infinita, fundido 1 s) y escribe en los turnos 2 y 30 lo que devuelve `GClimate::ComputeWeather` en el centro, dentro, en los dos radios y fuera (temperatura, lluvia, nieve, nublado, viento en bytes y en m/s, `GetMaxRainingOrSnowing` y `GetTemp`); la rejilla es de 40 m, así que los puntos de una misma celda dan el mismo valor. `OPENBLACK_TEST_WEATHER_AT="x,z[;x,z...]"` escribe lo mismo en esos puntos. `OPENBLACK_WEATHER_TRACE=1` escribe cada día de juego los climas (temperatura y objetivo, viento, deseo de lluvia, días secos y lloviendo, tormentas), cada 50 turnos las tormentas, y una vez por segundo lo que dibuja la lluvia (`Rain: N baldosas`). Culto y milagros de un uso ([magic.md](magic.md#culto-de-dónde-salen-los-milagros-m7-srcworship-ecssystemsimplementationsvillagerworship)), todos en el turno 1 salvo donde se diga: `OPENBLACK_TEST_WORSHIP_SITE="<TRIBU>[,<SEMILLA>...]"` crea el lugar de culto de esa tribu en la ciudadela del jugador, como `CREATE_WORSHIP_SITE`, con un icono de cada semilla (los guiones no le dan al jugador humano ningún centro de pueblo construido, así que su ciudadela nunca tendría lugar propio); `OPENBLACK_TEST_WORSHIP_PLAYER="<n>"` hace que estos ganchos actúen como el jugador n en vez del humano; `OPENBLACK_TEST_TOWN_SPELL="<ciudad>,<MAGIA>[;...]"` es `SET_MAGIC_IN_OBJECT(ciudad, magia, 1)` (la ciudad guarda la magia y su dueño la habilita); `OPENBLACK_TEST_MANA="<cánticos>"` es `GAME_SET_MANA` en el primer lugar de culto de ese jugador; `OPENBLACK_TEST_WORSHIP="<ciudad>,<fracción>"` es `Town::SetWorshipPercentage` (lo que hace arrastrar el tótem); `OPENBLACK_TEST_TAP_ICON="<SEMILLA>[,turno...]"` toca el icono de esa semilla del jugador en esos turnos (por defecto el 5) y escribe la magia, lo que hace falta, el resultado, el almacén y si se queda cargando; `OPENBLACK_TEST_TAP="x,z,turno"` toca el objeto tocable más cercano a ese punto (un icono, un icono del centro del pueblo o una bola de un uso); `OPENBLACK_TEST_DISPENSER="<ABODE>,x,z,<MAGIA>[,segundos]"` crea un dispensador como el guion del desafío de Land1 (`GiveSpellDispenserReward`); `OPENBLACK_TEST_FIREFLY_REWARD="x,z[,n]"` sortea n veces la recompensa de las luciérnagas allí. `OPENBLACK_CAMERA_LOCK="ox,oy,oz,fx,fy,fz"` pone la cámara ahí cada turno (en Land 1 el guion coge la cámara con START_CAMERA_CONTROL y su intro espera a MOVE_GAME_THING; sin `OPENBLACK_CAMERA_LOCK`, `OPENBLACK_CAMERA_FLY` también sujeta la cámara en su punto final cada turno; lo usa el mod [test.miracle-dispensers](mod-library.md#testmiracle-dispensers)). `OPENBLACK_WORSHIP_TRACE=1` escribe cada lugar de culto al crearse (jugador, tribu, hueco, posición y ángulo), sus iconos, y cada turno su cuenta de cánticos (`icons`, `N` bailarines, `C` capacidad, `k` intensidad, tensión, batería y máximo, disponible y daño por bailarín), además de la carga de los iconos, los orbes de los dispensadores y los aldeanos que van a adorar. Físicas: ver [physics.md](physics.md#ganchos-de-prueba).

Milagros ([magic.md](magic.md#núcleo-de-los-hechizos-m1-srcmagiccore-srcmagicspells-srcecseffects)), una vez en el
primer turno con el mapa cargado: `OPENBLACK_TEST_SPELL="<magia>,x,z[,radio[,duración[,jugador[,curl]]]]"` lanza como
`SPELL_AT_POS` (creador el jugador neutral, que repone los cánticos; con `jugador` 0..7 lanza ese jugador, que no
repone, y -1 es el neutral; radio 10 y la duración de `timerWhenPlayerCasting` si no se dan (-2 también la pide);
«desde» 30 m sobre el punto; `curl` es el del guion, PSysProcessInfo +0x34, del que salen los giros de los escudos) y
escribe lo que respondería la comprobación de la clase (vt 0x30) allí; `OPENBLACK_TEST_SEED="<semilla>[,pu]"` pone en la mano una
semilla cargada, como `OneOffSpellSeed::CreateSpellIntoHand`; `OPENBLACK_TEST_ONESHOT="<semilla>,x,z[,pu[,tap]]"` crea
una bola de un uso en el suelo (con `tap`, la toca y pasa a la mano). `<magia>` es un número de MAGIC_TYPE o el nombre
del info.dat (`FIRE`, `HEAL`, `STORM_PU2`...); `<semilla>` un número de SPELL_SEED_TYPE o su nombre (`FIRE`, `HEAL`,
`STORM`...). `OPENBLACK_SPELL_TRACE=1` escribe cada turno `Spell trace` por hechizo (cánticos, nivel de seguridad,
fuerza, coste del turno, edad, cerrado, PSys y átomos), cada evento aplicado y las búsquedas de objetivos de curar.
Con ella (o con `OPENBLACK_HAND_TRACE=1`) `Pot::AddResourceToPos` escribe `Pot trace` (en qué pila o almacén entra cada
cantidad, a qué distancia y con qué radio, y las pilas nuevas con su radio 2D) y cada grano de comida o madera
`SpellResource event` (unidades, cánticos pagados, tierra seca, fuerza), y `Grain trace` da cada turno el alzado de la
mano del chorro (t, altura, inclinación y si está fijada); ver
[miracles.md](miracles.md#comida-y-madera-m3-magicspellsspellresource-magicobjectsmagicfoodwood-ecspotresource).

Curar (M4, [miracles.md](miracles.md#curar-m4-m4h-magicspellsspellhealcpp-psysruleshealcpp)):
`OPENBLACK_TEST_HURT_VILLAGERS="x,z,radio,vida[,envenenado[,turno[,curar[,repetir]]]]"` pone a `vida` (0..1) a todos los
aldeanos a menos de `radio` de (x, z), los envenena con un 1, y con `curar` 1 (HEAL) o 2 (HEAL_PU_ONE) lanza ahí mismo
el milagro como `SPELL_AT_POS`. Ocurre en el turno `turno` (1 por defecto) y se repite cada `repetir` turnos (0 = una
sola vez; útil para que haya un chakra encendido en la captura). Luego escribe cada cambio: `Heal test: turn +N villager
E life a -> b, poisoned p, glow (specular) r,g,b` (el brillo del chakra; `none` cuando no hay chakra).

Lanzar desde la mano y gestos (M2, [magic.md](magic.md#lanzar-desde-la-mano-gestos-y-efectos-de-la-mano-m2-srcmagicgestures-srcmagichand-handspellseedcpp)):
- `OPENBLACK_TEST_CAST="press@t0,release@t1[,press@t2,release@t3...][,shot@t]"` pulsa y suelta la acción (el botón
  derecho) a esos segundos desde que existe el mapa, sumado al ratón real. Con una semilla en la mano
  (`OPENBLACK_TEST_SEED`) recorre el camino real: arma y lanza al soltar (HAND_GESTURE), lanza al pulsar
  (HAND_POSITION) o lanza cada turno mientras se mantiene (IN_HAND). La mano está donde diga `OPENBLACK_MOUSE_AT`.
  `shot@t` (al final) pide una captura en ese momento a `OPENBLACK_TEST_SHOT_PATH`, para capturas a una hora de juego y
  no a un fotograma (el ritmo de fotogramas varía).
- `OPENBLACK_TEST_CAST_PATH="x0,z0,x1,z1"`: durante la primera pulsación la mano recorre esa línea sobre el terreno (el
  sprinkle de comida, madera y agua solo suelta al moverla).
- `OPENBLACK_TEST_THROW_VEL="vx,vy,vz"`: la velocidad de la mano (ThrowVelocity del estado de la interfaz) que recibe el
  hechizo al lanzarse (la bola de fuego sale con ella).
- `OPENBLACK_TEST_GESTURE="<GESTO>[,tamañoPx[,cx,cy[,t]]][;<GESTO>...]"` o `=<fichero.txt>` (píxeles «x y» por línea):
  dibuja la primera plantilla del gesto (nombre de GESTURE_TYPE o número; un `-` delante lo refleja) de `tamañoPx` de
  ancho (200) centrada en la fracción de ventana (cx, cy) (el centro), t segundos de juego después de existir el mapa
  (0,5), como mensajes de ratón cada 28 ms. Al acabar escribe qué gestos encaja el búfer. La cámara no debe moverse
  (un vuelo de `OPENBLACK_CAMERA_FLY` borra el búfer).
- `OPENBLACK_GESTURE_TRACE=1`: gestos reconocidos (plantilla, espejo, tramo), círculos (posición y tamaño), eventos de
  ayuda y lanzamientos de la mano (también con `OPENBLACK_SPELL_TRACE=1`).

Fuego y rayo (M5, [magic.md](magic.md#fuego-m5-srcecsfire) y [miracles.md](miracles.md#bola-de-fuego-y-rayo-m5-magicobjectsmagicfireball-psysrulesfireballlightning)):
`OPENBLACK_TEST_FIRE="x,z,T[,clase[,turno]]"` pone el objeto con datos de fuego más cercano a (x, z) a la temperatura T
(`SetTemperature`) o, si T ≤ 1, lo enciende con esa velocidad (`SetOnFire`); la clase puede ser `any`, `tree`, `abode`,
`villager` o `field`, y el turno (contado desde el primero con el mapa cargado) permite esperar a que la cámara llegue.
`OPENBLACK_FIRE_TRACE=1` escribe los fuegos nuevos, los objetos que se consumen, los fuegos borrados, las reacciones y
los aldeanos que huyen o apagan, y cada 20 turnos lo que dibuja cada objeto ardiendo (`Fire: graphic of fire ...`:
llamas, escala y alfa de la primera, vapor y humo), que sirve para comprobar las llamas sin captura. El rayo y la bola
de fuego se lanzan con `OPENBLACK_TEST_SPELL="LIGHTNING_BOLT,x,z,radio,duración"` y `"FIREBALL,x,z"`; con una duración
larga (por ejemplo 300 s) el rayo sigue cayendo hasta que la cámara llega. `OPENBLACK_SPELL_TRACE=1` escribe cada evento
tipo 3 de las puntas con sus números y, por turno, `Lightning: N targets ... M of K forks struck` con el origen, el rumbo
del cono y la escala de las horquillas. `OPENBLACK_PSYS_CHAIN_TRACE=1` escribe por fotograma las cintas del PSys
(cuántas, con cuántas articulaciones, su textura y de dónde a dónde van), que es la forma de distinguir «la cinta no se
dibuja» de «en ese fotograma no había ninguna» (el rayo parpadea: una horquilla solo se ve el turno en que golpea).

`OPENBLACK_ZSORTER_TRACE=1` escribe una vez por segundo (cada 60 fotogramas dibujados) una línea `ZSorter trace:` con
lo que lleva la cola única de transparentes del fotograma (`graphics::zsorter`, `Renderer::DrawPass`): el total, las
entradas perdidas por el tope de 0x800 (`NewZObject` 0x83F31C), cuántas hay de cada clase (modelos, desvanecidos, nubes,
casillas de lluvia, sprites del barco, sprites, mallas y cadenas de los efectos `Sorted`, efectos `Queued`, nieblas,
humo, sprites, mano) y la clave (distancia al
cuadrado) de la primera y de la última; ver
[rendering-objects.md](rendering-objects.md#la-cola-única-de-transparentes-lh3dzsorter).

`OPENBLACK_ORB_TRACE=1` escribe, **cada fotograma dibujado** y desde `Renderer::DrawScene` (justo después de ordenar la
lista de atrás a delante), dos clases de línea en el registro con el logger `graphics`:

- `Orb trace: surface (<textura>) path <p> sorted <k>/<n> origin (x, y, z)` por cada superficie `ZR_SurfRevol` (el
  disco del dispensador, el charco del teletransporte): el camino de su efecto (0 `Sorted`: dibujada al momento, antes
  de toda la cola, `k` = −1; 1 `Queued`: `k` es el sitio de la entrada de su efecto) y el origen del efecto.
- `Orb trace: orb <entidad> phase <p> frame <f> packed[1][3] <v> uv (u, v) alpha <a> sorted <k>/<n> key <d> sortPoint (x, y, z) inView <b>`
  por cada `components::OneOffSpellSeed` (la burbuja de una bola de un uso): la fase y el fotograma de su hoja 4×4
  (`OneOffSpellSeed::UpdateFrame` 0x72A570), el valor empaquetado que lleva al shader en `[1][3]`
  (`frame_anim::PackUvOffset`) con la UV que representa, el alfa (1 − `[0][3]`), si entró en la lista ordenada y en qué
  sitio con qué clave (−1 = no entró: la lista deja fuera las instancias con alfa 0), el punto de orden de
  `OneOffSpellSeed::Draw` 0x518E90 y el resultado de la prueba de volumen de vista. Si la entidad no tiene instancia
  este fotograma escribe `Orb trace: orb <entidad> has no instance this frame`.

Sirve para dos cosas: comprobar que el disco del dispensador se dibuja **antes** que la burbuja y
seguir la burbuja en la vuelta 15 → 0 de su hoja. La escena es
`OPENBLACK_TEST_DISPENSER="NORSE_ABODE_SPELL_DISPENSER,1826,2670,10,2"` con
`OPENBLACK_CAMERA_LOCK="1816,52,2656,1826,37,2670"` (guion en `dev\_scratch\sistemas\orbfix\shot_orb.sh`). Son unas 2
líneas por fotograma, así que conviene limitar los fotogramas con `-n`.

Teletransporte ([miracles.md](miracles.md#teletransporte-m6t-srcmagicobjectsmagicteleport-srcecssystemsimplementationsvillagerteleport)): `OPENBLACK_TEST_TELEPORT="x0,z0,x1,z1[,jugador[,modo]]"` planta dos piedras de teletransporte como `SPELL_AT_POS` (la B en x1,z1 y la A en x0,z0; jugador 7 = neutral y gratis, 0 = PLAYER_ONE gasta cánticos). `modo`: `walk` (por defecto, el aldeano más cercano a A anda hacia B dos turnos antes y la reacción de A lo desvía por las piedras), `drop` (un segundo después se suelta el aldeano sobre A, salto forzado como `fn_005FC4F0`), `none` (solo las piedras). `OPENBLACK_TEST_TELEPORT_TURN=<n>` retrasa el inicio (el vuelo de una captura tarda ~160 turnos). `OPENBLACK_TELEPORT_TRACE=1` (o `OPENBLACK_SPELL_TRACE=1`) escribe las piedras, el reparto de la reacción, los saltos (de qué piedra a cuál, el ahorro en metros) y el `PayFor` del hechizo (un salto útil suma cánticos, uno forzado hacia atrás cuesta, R13). Los discos usan `ZR_SurfRevol` (`RendererSurfRevol.cpp`).

Bosque ([miracles.md](miracles.md#bosque-m4b-magicspellsspellforest-magicobjectsmagictree-ecstrees)):
`OPENBLACK_TEST_MAGIC_TURN=<n>` hace que `OPENBLACK_TEST_SPELL`, `_SEED` y `_ONESHOT` esperen al turno de juego n (para
que la cámara de `OPENBLACK_CAMERA_FLY` ya esté allí); `OPENBLACK_TEST_FOREST_SHOT="<turnos>,<ruta.png>[;<turnos>,<ruta>...]"`
pide una captura esos turnos después de que la semilla del bosque toque tierra (el número de fotograma de
`--screenshot-frame` varía con la velocidad de dibujo; esta petición sustituye a la de la línea de órdenes si coinciden).
Ejemplo: `OPENBLACK_TEST_MAGIC_TURN=330 OPENBLACK_TEST_SPELL=NATURE,1790,2625
OPENBLACK_CAMERA_FLY=1772,52,2604,1790,36,2625 OPENBLACK_TEST_FOREST_SHOT="3,a.png;120,b.png"` con `-n 11000`.

Agua ([miracles.md](miracles.md#agua-m4a-magicspellsspellwater-psyscreatorsmist)): `OPENBLACK_TEST_SPELL=WATER,x,z,10,6` (o
`WATER_PU1`; la nube sale 30 m sobre el punto, las gotas caen alrededor del punto) y
`OPENBLACK_TEST_WATER_SHOT="<turnos>,<ruta.png>[;...]"`: capturas esos turnos de juego después del primer `Process` del
hechizo (también mientras se cierra). Con `OPENBLACK_SPELL_TRACE=1` cada gota escribe su punto, su distancia, los objetos
regados y el último anillo; cada campo, sus cultivos, crecimiento y comida; un árbol, el brote que planta; un objeto que
arde, la reacción 34. Con la mano: `OPENBLACK_TEST_SEED=WATER OPENBLACK_TEST_CAST="press@20,release@32"
OPENBLACK_TEST_CAST_PATH="x0,z0,x1,z1"`. Apagar un fuego: `OPENBLACK_TEST_FIRE="1818.6,2628.4,500,tree,110"` y el agua
en el turno 200 (`OPENBLACK_TEST_MAGIC_TURN=200`), cámara `1810,36,2620,1818.6,31,2628.4`.

Bandadas ([miracles.md](miracles.md#bandadas-m4c-magicspellsspellflock-psysrulesflockcpp)): `OPENBLACK_TEST_SPELL=FLYING_FLOCK,x,z` o `GROUND_FLOCK,x,z` (24 / 25; desde 30 m sobre el punto, el jugador neutral, así que salen hacia +x alternando el lado) y `OPENBLACK_TEST_FLOCK_SHOT="<turnos>,<ruta.png>[;...]"` (capturas esos turnos de juego después del lanzamiento). Con `OPENBLACK_SPELL_TRACE=1` cada animal escribe su salida y destino y, cada 10 turnos, cada miembro su posición, estado y alfa. Para que la cámara los siga: `OPENBLACK_TEST_VIEW_ANIMAL="0,35,-90" OPENBLACK_TEST_ANIMAL_SPECIES=20 OPENBLACK_TEST_VIEW_LOCK=1` (22 los lobos).

Escudos ([miracles.md](miracles.md#escudos-m6-shield-magicspellsspellshield-magicobjectsmapshield-psysrulesshield)):
`OPENBLACK_TEST_SHIELD_SHOT="<turnos>,<ruta.png>[;...]"` pide una captura esos turnos de juego después de crearse el
primer MapShield (como la del bosque). `OPENBLACK_TEST_SHIELD_FRAMES="<turnos>,<n>,<prefijo>[@<lento>]"` hace n capturas
en fotogramas seguidos (`<prefijo>_<i>.png`, turno y fracción en el log; `@<lento>` alarga el turno pero **no** la
interpolación del PSys, ver [miracles.md](miracles.md#ganchos-y-capturas)). Con `OPENBLACK_SPELL_TRACE=1` se escriben
`SpellShield::InitWithPos` (radio,
anillos anti, reacción, ciudad, coste por turno), cada turno el escudo físico (`t`, curva de crecer, escala dibujada y
la del objeto, ángulo, altura, muriendo y si tiene cuerpo en las físicas) y cada golpe físico (momento y cánticos
pagados). Ejemplo del escudo físico creciendo: `OPENBLACK_TEST_MAGIC_TURN=300
OPENBLACK_TEST_SPELL="PHYSICAL_SHIELD,1826.8,2641.4,40,-2,-1,2" OPENBLACK_CAMERA_FLY=1720,90,2560,1826.8,40,2641.4
OPENBLACK_TEST_SHIELD_SHOT="7,a.png;10,b.png;13,c.png;25,d.png"` con `-n 16000`. Una roca contra él:
`OPENBLACK_TEST_PHYSICS="1745,2641.4,25,12,6,0,1.0,1"` (12 m/s: rebota).

Explosión de rayo ([miracles.md](miracles.md#explosión-de-rayo-y-clases-de-psys-que-faltaban-m6b-psysrulesexplosionkeypointsorientforestcpp)):
`OPENBLACK_TEST_SPELL="BEAM_EXPLOSION,x,z"` (también `BEAM_EXPLOSION_PU1` y `_PU2`, que reparten varias explosiones) y
`OPENBLACK_TEST_EXPLOSION_SHOT="<turnos>,<ruta.png>[;...]"`, que pide capturas esos turnos de juego después del primer
paso de la primera explosión (como la del escudo). Con `OPENBLACK_SPELL_TRACE=1` se escriben `Explosion: started ...`
(centro, margen del escudo, radio de búsqueda y celdas de la espiral) con cada objetivo (distancia, radio y clase), cada
objeto destruido (anillo, explotados y borrados) y lo que no está portado (las mallas en pedazos), y la marca del suelo que deja (`Explosion: ground mark`).
Ejemplo: `OPENBLACK_TEST_MAGIC_TURN=300 OPENBLACK_TEST_SPELL="BEAM_EXPLOSION_PU2,1790,2600"
OPENBLACK_CAMERA_FLY=1700,140,2480,1790,40,2600 OPENBLACK_TEST_EXPLOSION_SHOT="12,a.png;40,b.png;80,c.png"` con
`-n 16000`.

Tormenta ([miracles.md](miracles.md#tormenta-tormenta-eléctrica-y-tornado-m6-storm-magicspellsspellstormandtornado-psysrulesstorm-ecsweatherlightningflashstormclouds)):
`OPENBLACK_TEST_SPELL="STORM,x,z,60"` (o `STORM_PU1` con rayos, `STORM_PU2` con tornado; el radio es el del hechizo,
recortado a 20..1000) lanza como `SPELL_AT_POS` (desde 30 m encima: sin rumbo, así que la tormenta no tiene viento
propio). `OPENBLACK_TEST_STORM_SHOT="<turnos>,<ruta.png>[;...]"` pide una captura esos turnos de juego después del
primer turno de la primera tormenta; `OPENBLACK_TEST_STORM_STRIKE_SHOT="<n>,<ruta.png>[;...]"` la pide en el turno del
rayo n.º n desde las nubes (los rayos duran 1 o 2 turnos). `OPENBLACK_TEST_STORM_PILE="x,z,cantidad[,wood]"` pone un
montón de comida (o de madera) allí al lanzarse la primera tormenta, para que lo coja el tornado.
`OPENBLACK_TEST_STORM_CLOUDS="x,z,radio[,nubes[,negrura[,elevación]]]"` registra en el primer fotograma con tierra una
tormenta con nubes (`GWeather::DrawClouds`: interior = radio, exterior = 3 × radio, 8 nubes, negrura 0,5, elevación 160
por defecto; lluvia 100, fundido 1 s). `OPENBLACK_STORM_TRACE=1` escribe el registro de la tormenta del milagro (radios,
lluvia, nublado, viento, fundido), cada rayo (de qué nube, el siguiente, su vida), cada cosa que coge el tornado (qué
es, hasta qué altura sube) y, cada 10 turnos de cada hechizo de tormenta, su edad, cánticos, el tiempo en su centro
(`ComputeWeather`), sus átomos, los objetos que lleva y el destello en ese punto. Ejemplo del tornado:
`OPENBLACK_TEST_MAGIC_TURN=200 OPENBLACK_TEST_SPELL="STORM_PU2,1826.8,2641.4,60"
OPENBLACK_TEST_STORM_PILE="1830,2650,400" OPENBLACK_CAMERA_FLY=1775,60,2595,1830,45,2650
OPENBLACK_TEST_STORM_SHOT="95,a.png;120,b.png"` con `-n 12000`.

Puntos útiles de Land1: `1464,2016` es **mar abierto** (altitud 0, celda de agua que no se dibuja), no la playa;
orilla de altitud 1 en `1485,2015`; tierra seca de altitud 44 en `1788.4,2710`; arena seca `1478,2129`; almacén del pueblo
`1826.8,2641.4` (cámara `1818,75,2612,1824,44,2636`); árbol junto al almacén `1818.6,2628.4` (el suelo está a 29,4 m:
cámara `1810,36,2620,1818.6,31,2628.4`). El vuelo de `OPENBLACK_CAMERA_FLY` tarda unos 4000 fotogramas, que con el mapa
cargado son unos 150-180 turnos de juego: para ver algo que dura poco, conviene lanzarlo con el parámetro de turno.

## Tests y datos de prueba

- `test_food_wood` (M3): la spline de HandStateGrain y su bucle, el coste de cada grano, los sonidos de pila,
  `GetProportionRaised`, la emisión de SF_Food (18 granos/s con la mano quieta, 1 por 2,5 m al moverla) y la lectura de
  ARRAY con flotantes; con `OPENBLACK_GAME_PATH` las filas reales de comida y madera.
- `test_spell_forest` (M4b): la espiral del bosque, la escala objetivo, cuántos árboles quiere, el coste y
  GetMaxObjectsToCreate; con `OPENBLACK_GAME_PATH` la fila NATURE real y los `magicTreeTypes`.
- `test_shield` (M6, [miracles.md](miracles.md#escudos-m6-shield-magicspellsspellshield-magicobjectsmapshield-psysrulesshield)):
  el recorte y el coste del radio, las curvas de ProcessShield, los ayudantes de la esfera (dentro, cruce, intersección,
  rebote), el registro de DefensiveSphere con un efecto real y `DoAnyShieldDeflections`, y el marco de las jerarquías
  del PSys (los antepasados marcados, con su escala); con `OPENBLACK_GAME_PATH`, las filas reales de los dos escudos.
- `test_influence`: la curva de los anillos, el radio de ciudad y ciudadela, los anillos anti y los que siguen a un objeto.
- `test_lightning` (M5, [miracles.md](miracles.md#rayo-magic_type-4-6-semilla-6-lightning_bolt-psysruleslightningcpp)): que
  `UR_Lightning`, `UR_LightningStrike`, `ParticleChainCreator` y `ParticleLightMapCreator` están registrados, las
  propiedades que leen y la UV por tramo de la cinta; con `OPENBLACK_GAME_PATH`, los `SF_LightningBolt*` y
  `SF_LightningStrike` reales y el tamaño del `.raw` del mapa de luz.
- `test_spell_chants` (la economía de cánticos de [magic.md](magic.md), sin mundo): el rayo de un jugador y el del
  jugador neutral, el escudo mantenido y el poder tribal, los casos límite de la fuerza y el reparto del alineamiento;
  con `OPENBLACK_GAME_PATH` también las filas reales del rayo y del escudo.
- `test_worship` (la economía del culto de [magic.md](magic.md), sin mundo salvo un registro): la capacidad y la
  batería del lugar por bailarines, su cuenta de fin de turno (intensidad del baile, batería y daño por
  bailarín), la tensión, el fallo de la reserva de mantenimiento, `UseChants`, el exceso de `AddToChantStore`,
  cuántos aldeanos pide una ciudad, `SigmoidThreshold` y las probabilidades de las luciérnagas.
- `test_magic_tables` (tablas de milagros, [magic.md](magic.md)): con `OPENBLACK_GAME_PATH=<instalación>` también
  comprueba el `Scripts\info.dat` real; sin ella esa prueba se salta.
- `test_spell_sounds` (sonido del PSys, [particles.md](particles.md#sonido-de-las-partículas-lane-s-srcaudiospellsounds-srcpsysrulessoundcpp)): igual, con `OPENBLACK_GAME_PATH` lee también
  `Data\SoundAction.h`, `spells.sad` y `SF_TeleportVortex`.
- `test_weather` (tiempo y clima, [day-night-weather.md](day-night-weather.md#tiempo-y-clima-m6a-srcecsweather)), sin mundo: la aritmética de
  bytes, la fecha y las estaciones, el fundido y el `CalcAtmos` de una tormenta, la rejilla de 40 m y la bajada por
  altura, el borrado en dos turnos, `KILL_STORMS_IN_AREA`, la suma de los climas, la oscilación de la temperatura, la
  tormenta que crea un clima con deseo 1 y los bytes de `CREATE_WEATHER_STORM`.
- `test_water_queries` (consultas de agua y `LandAvoid`) necesita los datos originales: se **salta** si no se define
  `OPENBLACK_TEST_GAME_PATH=<carpeta del juego>`. Carga `Scripts\Land1.txt` una vez para toda la suite
  (ojo: los tests de magia usan `OPENBLACK_GAME_PATH`, este `OPENBLACK_TEST_GAME_PATH`).
- El terreno de prueba lo genera `lndtool` en `test/mock/CMakeLists.txt`. Se corrigió un fallo que borraba los puntos
  anteriores del mismo bloque; la celda bajo la cámara de `test_set_camera_pos` ahora es llana.

## Commits con varias sesiones

El protocolo está en `C:\Users\diewgarc\dev\BUILD_PROTOCOL.md` (candado de build, turno en `commit_queue.txt`).
Para commitear solo tus hunks de un archivo que también tocan otras sesiones, **no** usar
`git apply --unidiff-zero`: coloca las inserciones según las líneas del árbol de trabajo y no las de HEAD, y rompió
HEAD dos veces (f9c0b08c, arreglado en 6fde22cf; 09fc3b05, arreglado en 6a0b1a4b). Usar
`dev\hunks2.py list|build <base> <archivo> <picks>` y `dev\commit_build.py <config.json>`, que montan el commit
en un índice temporal sobre HEAD.

## Pruebas con ratón

`C:\Users\diewgarc\dev\drive*.ps1` mueven el ratón real (SetCursorPos, mouse_event): **no lanzarlas si el usuario está
usando el PC**. Para verificar sin ratón: capturas con `OPENBLACK_CAMERA_FLY` y los ganchos de prueba.
