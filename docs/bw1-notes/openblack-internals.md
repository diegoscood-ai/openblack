# openblack por dentro (rama local)

## Compilar, probar, ejecutar

- `C:\Users\diewgarc\dev\openblack\build_openblack.bat configure|build` (desde bash:
  `cmd //c "C:\\Users\\diewgarc\\dev\\openblack\\build_openblack.bat build"`). **Tras añadir .cpp, `configure`**
  (las fuentes se recogen con GLOB).
- Tests en `cmake-build-presets\ninja-multi-vcpkg\bin\Release`: `test_camera` (11 grabaciones del original),
  `test_set_camera_pos`, `test_game_initialize`, `test_load_scene`, `test_mobile_wall_hug`, `test_fixed`,
  `test_interpolator`. Deben pasar todos.
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
| `Common/Zoomer` | Zoomer de LH3DLib |
| `ECS/StoragePitStore` | Lógica del almacén |
| `ECS/StaticGrounding` | Mod: asentar objetos estáticos |
| `Graphics/TextureMipmaps` | Mod: cadena de mips en CPU |
| `Archetypes/PotArchetype` | Crear vasijas/montones, `SetSize`, `UpdateSizes` |

## Render

- Matriz de instancia (mat4 por objeto): se aprovechan las `w` de las columnas de rotación:
  - `[0][3]` = 1 − opacidad (`components::Alpha`; esos objetos van en la vista `RenderPass::MainBlended`);
  - `[1][3]` = desplazamiento de textura V (`components::UvScroll`);
  - `[2][3]` = desplazamiento vertical que respeta el shader de terreno (`PileSink` con `MorphWithTerrain`).
- `MorphWithTerrain`: el shader pega el objeto al terreno (`y += altura_terreno − y_original`); anula cualquier
  desplazamiento vertical salvo el de `[2][3]`.
- El búfer de instancias crece con margen y se sube con `bgfx::copy` (con `makeRef` y un `resize` se leía memoria
  liberada: artefactos al crear y destruir mallas cada fotograma).
- `L3DSubMesh` guarda en CPU posiciones e índices (`GetCollisionPositions/Indices`) para picking y medidas;
  `L3DMesh::RayIntersect` hace el test de triángulos.
- Texturas: si un material pide una textura inexistente y la malla trae skin incrustada, se usa esa (mods).
- **Trampa**: ninja no recompila `vs_object_instanced.sc` / `vs_object_hm_instanced.sc` al cambiar `vs_object.sc`:
  hay que tocarlos (`touch`). Con shaders viejos desaparecen todos los objetos.

- `RenderContext::entityInstances`: entidad → (malla, índice de instancia, `morphWithTerrain`,
  `receivesDynamicShadow`), para dibujar una entidad concreta (reflejos, sombra sobre objetos).
- `LandIslandInterface::GetUnflattenedHeightAt`: `GetAltitude` sin el aplanado del mar (búsqueda de las piscifactorías).

## Mods

Librería en `src/Mods/` ([mod-library.md](mod-library.md)). Los mods escriben interruptores de `EngineConfig`
(`msaa`, `textureMipmaps`, `anisotropicFiltering`, `livingWater`, `groundStaticObjects`) que lee el motor.
- `world.ground-statics`: baja cada objeto estático hasta que su vértice más bajo toca el suelo (recuerda cuánto en
  `MobileStatic::groundedDrop`). Limitación conocida: una roca apoyada en una punta sigue pareciendo flotar; se
  resolverá con físicas.

## Variables de entorno de depuración

`OPENBLACK_PROFILE=<s>` (resumen del perfilador en el log), `OPENBLACK_CAMERA_FLY="ox,oy,oz,fx,fy,fz"` (el vuelo
tarda unos miles de fotogramas: usar `-n 8000 --screenshot-frame 7900`), `OPENBLACK_PRINT_ALTITUDE="x,z"` (altura,
terreno físico y objetos cercanos), `OPENBLACK_MARK_LOWEST=1` (marca el vértice más bajo de las rocas cercanas),
`OPENBLACK_DUMP_STATIC_GAPS=1`, `OPENBLACK_HAND_TRACE=1`, `OPENBLACK_HAND_TEST_ROCK="x,z"`
(+ `_FOOD`, `_NO_BOULDER`), `OPENBLACK_HAND_TEST_TREE="x,z[,dead][,roots][,store]"`,
`OPENBLACK_HAND_TEST_STORE_TAKE="madera,comida"`, `OPENBLACK_HAND_ANIM=<nodo>`, `OPENBLACK_NO_PICKUP_PSYS=1`,
`OPENBLACK_HAND_TEST_HOLD=<escala>` (la mano empieza sosteniendo una roca), `OPENBLACK_HAND_TEST_FISH=1` (chapoteo y
pesca en el primer banco con la acción mantenida 3 s; con `OPENBLACK_HAND_TRACE=1` escribe `Fish trace`), `OPENBLACK_START_UNPAUSED=1` (turnos y
scripts desde el primer fotograma), `OPENBLACK_TEST_FADE="r,g,b,segundos"` (`SET_FADE`), `OPENBLACK_TEST_WIDESCREEN=1`.

Puntos útiles de Land1: playa de inicio `1464,2016` (agua poco profunda); arena seca `1478,2129`; almacén del pueblo
`1826.8,2641.4` (cámara `1818,75,2612,1824,44,2636`).

## Tests y datos de prueba

- El terreno de prueba lo genera `lndtool` en `test/mock/CMakeLists.txt`. Se corrigió un fallo que borraba los puntos
  anteriores del mismo bloque; la celda bajo la cámara de `test_set_camera_pos` ahora es llana.

## Pruebas con ratón

`C:\Users\diewgarc\dev\drive*.ps1` mueven el ratón real (SetCursorPos, mouse_event): **no lanzarlas si el usuario está
usando el PC**. Para verificar sin ratón: capturas con `OPENBLACK_CAMERA_FLY` y los ganchos de prueba.
