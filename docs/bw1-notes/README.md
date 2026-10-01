# Notas de Black & White 1 para openblack (rama local `local/hand-hbn`)

Guía de lo descubierto al reconstruir el comportamiento original a partir de `runblack.exe` (v1.42 no oficial sobre
v1.20, disposición W120) y de los datos originales, y de cómo lo hace openblack. Todo lo que aquí se afirma está
verificado en el ejecutable o medido, salvo donde se marca **(inferido)** o **(aproximado)**.

Proyecto **solo local**: no se publica nada (ni push, ni PRs, ni forks).

- [Páginas por área](#páginas-por-área)
- [¿Dónde busco…?](#dónde-busco)
- [Cómo están escritas las páginas](#cómo-están-escritas-las-páginas)
- [Filosofía](#filosofía)
- [Referencias externas locales](#referencias-externas-locales)

## Páginas por área

**Herramientas y base del motor**

| Página | Contenido |
|---|---|
| [tooling.md](tooling.md) | Desensamblado, símbolos, herramientas de openblack, formatos de datos, LND y mapas de BWLandEditor |
| [engine-math.md](engine-math.md) | Coordenadas, altura del terreno, matrices LH, Zoomer |
| [openblack-internals.md](openblack-internals.md) | Dónde está cada cosa en el código, compilar, tests, ganchos de prueba, depurar cierres, trampas (Vulkan, makeRef), commits con varias sesiones |

**La mano y los objetos**

| Página | Contenido |
|---|---|
| [hand-and-interface.md](hand-and-interface.md) | Mano: colocación, estados, agarre, lanzamiento, objeto bajo el cursor |
| [objects-and-resources.md](objects-and-resources.md) | Montones y vasijas, coger por tandas, almacén, objetos estáticos y rocas, campos, sonidos (coger, LHAudio y QMixer, canales, ambiente) |
| [trees.md](trees.md) | Árboles y bosques: arrancar y el tirón, reglas de coger, soltar y replantar, madera y GTreeInfo, API para los oficios de aldeano, búsquedas, crecimiento, dibujado, fuego del árbol, sacrificio |
| [map-loading.md](map-loading.md) | Carga del mapa y funciones del guion: CREATE de CHL, niebla del mapa, rebaños y animales, datos de simulación, piscifactorías, `BUILT_PERCENTAGE`, objetos del guion (farolas, hogueras, árboles muertos, puertas, ciudadela planeada), `IsOkToCreateAtPos`, ciudades y ciudadela |
| [physics.md](physics.md) | Físicas del original: objetos lanzados, choques, daño, mar, edificios y rocas que se rompen |

**Seres vivos**

| Página | Contenido |
|---|---|
| [animation.md](animation.md) | Aldeanos y animales: clips ANM, qué clip por estado, velocidad, tamaño, índice de creación, sonidos de los clips, objetos en la mano, dibujo entre turnos |
| [villagers.md](villagers.md) | Aldeanos: campos del original, flags +0xE0, creación, cambios de estado (SetTopState / SetCurrentAndDestinationState), saltos de la velocidad, supuestos |
| [animals.md](animals.md) | Animales: IA completa del original (herbívoros, depredadores y caza, aves, reacciones, bandadas, edad, mano, física, muerte, aldeanos como presa), clips por especie, diferencias que quedan |

**Mundo, tiempo y gráficos**

| Página | Contenido |
|---|---|
| [day-night-weather.md](day-night-weather.md) | Reloj de día y noche (hora visual y de guion, ciclo, guiones), luces de noche, clima; tiempo y clima del juego (LH3DAtmos, GClimate, tormentas, lluvia) |
| [rendering.md](rendering.md) | Estados D3D, terreno, luz, mar, neblina, sombras, cielo y nubes, reflejos, peces, anillos, partículas, ríos, niebla del mapa, humo, fundido, fuentes y texto |
| [parity.md](parity.md) | Tabla de paridad del motor gráfico: cada etapa del original y su estado en openblack |
| [original-frame.md](original-frame.md) | Mapa del fotograma original (orden de dibujo, modos de render, estados, niveles de detalle) |
| [audio.md](audio.md) | Motor de audio (GAudio, LHaudio, QMixer, capas de openblack), bancos y formatos (.sad, .sas, música MP2), música (LHMusic, GameMusic), voces y textos, CHL de audio, fase A hecha y fases B/C |
| [water-queries.md](water-queries.md) | Consultas de agua (costa, río y agua potable más cercanas) y máscara `LandAvoid` de la criatura |
| [camera-tracks.md](camera-tracks.md) | `Data\camera.edt`: cámaras `Cam%d`, pistas `Track%d` (`LH3DWay`), `WALK_PATH` de los MobileObject (tiburones) |

**Magia**

| Página | Contenido |
|---|---|
| [magic.md](magic.md) | Núcleo de la magia: tablas de info.dat, ciclo de vida de los hechizos, cánticos, eventos y efectos, reglas de lanzamiento, semillas y milagros de un uso, lanzar desde la mano y gestos, culto y poder de oración, influencia, alineación, reacciones, vida, modelo del fuego, orden en el turno; suposiciones auditadas |
| [miracles.md](miracles.md) | Cada milagro: comida y madera, agua, curar, bosque, bandadas, bola de fuego y rayo, escudos, teletransporte, tormenta y tornado, explosión de rayo; los de la criatura (pendientes) |
| [particles.md](particles.md) | Motor de partículas (PSys): tipos de partícula, registro de clases, PSys enlazado al hechizo, jerarquías, creadores (mallas, cadenas, mapas de luz, niebla), sonido de las partículas, índice de reglas |

**Mods**

| Página | Contenido |
|---|---|
| [mod-library.md](mod-library.md) | Librería de mods: menú, `settings.cfg`, `--mod`, tipos de mod, cómo programar uno, catálogo |
| [mods.md](mods.md) | El `AllMeshes.g3d` modificado de la instalación y el mod HD-Tweaks |

**En camino** (plan `C:\Users\diewgarc\dev\WIKI_PLAN.md`): `water.md` (el agua en el juego),
`villagers.md` (oficios de los aldeanos), `audio.md` (motor de audio) y `rendering-objects.md` (sale de rendering; las
partículas de rendering irán a `particles.md`).

## ¿Dónde busco…?

| Tema | Página |
|---|---|
| Una dirección o símbolo de `runblack.exe`, los scripts de desensamblado | [tooling.md](tooling.md) |
| Altura del terreno, coordenadas, matrices | [engine-math.md](engine-math.md) |
| Coger, soltar, lanzar, el cursor | [hand-and-interface.md](hand-and-interface.md) |
| Comida y madera, almacén, campos | [objects-and-resources.md](objects-and-resources.md) |
| Árboles y bosques (tirón, replantar, crecimiento, fuego, sacrificio) | [trees.md](trees.md) |
| Qué crea el guion del mapa (CHL), nieblas, rebaños, farolas, ciudades | [map-loading.md](map-loading.md) |
| Golpes, daño, edificios que se rompen | [physics.md](physics.md) |
| Animaciones y velocidad de aldeanos y animales | [animation.md](animation.md) |
| Datos y estados de los aldeanos | [villagers.md](villagers.md) |
| Comportamiento de los animales | [animals.md](animals.md) |
| Hora del día, ventanas iluminadas, clima, tormentas, lluvia | [day-night-weather.md](day-night-weather.md) |
| Cómo se dibuja algo; si ya está como el original | [rendering.md](rendering.md), [parity.md](parity.md) |
| Orden del fotograma original | [original-frame.md](original-frame.md) |
| Magia: hechizos y cánticos, lanzar desde la mano, gestos, culto, influencia, alineación, reacciones, fuego | [magic.md](magic.md) |
| Un milagro concreto (comida, agua, curar, bosque, bandadas, bola de fuego, rayo, escudos, teletransporte, tormenta, explosión de rayo) | [miracles.md](miracles.md) |
| Partículas: tipos, clases de PSys, creadores, sonido de las partículas, qué regla está dónde | [particles.md](particles.md) |
| Costa, río o agua potable más cercana; `LandAvoid` de la criatura | [water-queries.md](water-queries.md) |
| Cámaras y pistas de `camera.edt`, recorridos de los tiburones | [camera-tracks.md](camera-tracks.md) |
| Activar o programar un mod | [mod-library.md](mod-library.md) |
| Ganchos de prueba (`OPENBLACK_*`), compilar, depurar | [openblack-internals.md](openblack-internals.md) y la sección «Ganchos de prueba» de cada página |

## Cómo están escritas las páginas

Cada página empieza con qué cubre y un índice. Cada tema lleva su estado: **fiel** (verificado en el original),
**(aproximado)**, **(inferido)**, **mod/propio** o **pendiente**. Al final: **Pendiente**, **Ganchos de prueba** y
**Fuentes** (informes de `C:\Users\diewgarc\dev\tmp_dis\…`). Un tema vive en una sola página; las demás enlazan.
Nunca se borran direcciones ni cifras al editar. Detalle en `C:\Users\diewgarc\dev\WIKI_PLAN.md` §2.

## Filosofía

1. Primero todo lo original, verificado en el ejecutable: openblack es una réplica, sin suposiciones. Cada constante o
   regla lleva su dirección o su dato de origen; lo que no se puede leer se marca y se dice.
2. Lo que se aparte del original va como **mod**, desactivado por defecto, en la librería de mods (`src/Mods/`,
   menú **Mods** del juego, [mod-library.md](mod-library.md)). No confundir con el `AllMeshes.g3d` modificado de la
   instalación ([mods.md](mods.md)).
3. Verificar antes de dar algo por bueno: compilar, tests y capturas automáticas (sin ratón si el usuario está usando
   el PC).
4. Varias sesiones trabajan a la vez: directrices en `C:\Users\diewgarc\dev\TEAM_GUIDELINES.md`, protocolo de build y
   commits en `BUILD_PROTOCOL.md` y tablero en `TEAM_STATUS.md` (misma carpeta).

## Referencias externas locales

- `C:\Users\diewgarc\dev\bw1-decomp`: decompilación coincidente de openblack/bw1-decomp; `config\BW1W120\symbols.txt`
  da nombres a las direcciones.
- `C:\Users\diewgarc\dev\decomp_pickup`: pseudo-C++ reconstruido de la mano, la interfaz y los objetos
  (`hand.cpp`, `interface.cpp`, `objects.cpp`, `multi.cpp` y sus `NOTES_*.md`).
- `C:\Users\diewgarc\dev\tmp_dis`: scripts de desensamblado, volcados e informes por tema.
- `C:\Users\diewgarc\dev\tools`: herramientas propias (scripts `lnd_*`, capturas, Real-ESRGAN).
