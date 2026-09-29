# Paridad del motor gráfico: original frente a openblack

Mapa completo del fotograma original (inglés, con direcciones): [original-frame.md](original-frame.md). Detalles de
lo ya hecho: [rendering.md](rendering.md). Las mejoras que no son del original van como mods
([mod-library.md](mod-library.md)).

Estado: **igual** (verificado), **aprox.** (funciona pero difiere), **falta**.

| Etapa | Original | openblack | Estado |
|---|---|---|---|
| Borrado | El color no se borra; el cielo cubre la pantalla | Borra a 0x274659 | aprox. (no se ve) |
| Cámara | FOV horizontal 70°, sin plano lejano, cercano 0,3–3,5 según altura | FOV 70°, cercano 0,3 + 0,16·altura (0,3–3,5), lejano 65536 | igual |
| Cielo | 9 imágenes (alineación × hora), 2 pasadas, se oscurece con tormenta, blanco con relámpago; umbrales de hora 3,5/7,5/8/8,5 | Tipo y alineación, umbrales del original | aprox. (sin clima) |
| Sol y luna | `sun.l3d` (6–18 h) y su resplandor; `moon.l3d` con fase por el reloj real y halo aditivo; sin estrellas | Igual (resplandor ocluido por el terreno con rayos por CPU; sin la luna reflejada) | aprox. |
| Nubes y sombras de nubes | 70 nubes (`mist.l3d` + `smoke.raw`) con el viento, color por alineación; sombras de `sclouds.raw` en la luminosidad de las celdas | Igual | igual |
| Tierra reflejada | Solo tierra, sin Z, media luz, sin small bump | Igual | igual |
| Reflejos de objetos en el mar | Mano (gris 0xA0A0A0 sin luz) y lo que sostiene, objetos físicos, el cuerpo de la criatura (y < 6, 0x65A0A0D0) y barcos, espejados y recortados sobre el agua, con el color que les dejó `fn_00801C90` | La mano, lo que sostiene y los objetos lanzados; faltan la criatura y los barcos (no existen aún) | aprox. |
| Bajo el agua | Peces de piscifactoría (15 sprites `misc0.raw` por granja), antes del mar; tiburones y SuperVillagers nadando cortados por y = 0 (`DrawCutByPlane`, 0xFF303070) | Los bancos de peces, dentro del objetivo de reflejo; no hay tiburones ni SuperVillagers en Land1 | aprox. |
| Mar | Modo 5, periodo 560 (nivel 4), viento, ondulación por filas, alfa 255→80, sin neblina | Igual salvo el viento | aprox. (sin viento) |
| Tabla de luz | `palette.raw` por hora y alineación | Igual (sin nubes ni relámpagos) | igual |
| Tierra | Bloques de delante a atrás, modo 14, tabla de luz por vértice, small bump | Igual | igual |
| Neblina de distancia | Por software: tierra por vértice, modelos una vez por objeto; no en mar, cielo ni partículas PSys | Igual (sin tormenta ni relámpago) | igual |
| Sombras estáticas | Horneadas en las texturas de bloque: Fixed/MobileObject/árboles, cizalla x += h, z += h, ×0,5 | Pasada `StaticShadow` de toda la isla cada fotograma (256 px/bloque), ×0,5 en `fs_terrain` | igual (sin el AA 4×2 del original) |
| Sombras dinámicas | Siluetas 32×32 de la criatura y la mano (y objetos lanzados, barcos, SuperVillagers) sobre la tierra; solo la mano y la criatura sobre objetos (modo 6, ZFUNC EQUAL, proyección vertical) | La mano sobre la tierra y sobre los objetos; los objetos físicos en vuelo sobre la tierra (`PhysicsShadows`, hasta 16); faltan la criatura, los barcos y los SuperVillagers | aprox. |
| Manchas de aldeanos y animales | Dos quads desde los pies (huesos 21 y 18) hacia +X+Z (2·escala, sobre el plano del terreno), ancho 0,4, alfa 1 → 0; animales: 4 quads desde los puntos EBone de su malla | Igual (aldeanos y animales terrestres) | igual |
| Huellas | Grabadas en las texturas de bloque | Pasada Footprint | aprox. |
| Luz de modelos | Tierra bajo el objeto (bilineal), sol (−1, 1, −1), ambiente 90/256, especular aditivo, mano ×1,5 | Igual | igual |
| Materiales L3D | Tipo = modo; chroma con prueba de alfa (ref − 5) **y** mezcla | Igual, con culling por material (bit 0 del byte +5; D3DCULL_CCW = CCW en bgfx) y wrap/clamp (bit 2) | igual |
| Orden de transparentes | Z-sorter de atrás a delante (máx. 2048) | Opacos juntos; cada instancia con primitivas mezcladas o que se desvanece, por separado de atrás a delante en `MainBlended` (secuencial) | igual |
| LOD de modelos | En este ejecutable la carga de `LevelOfDetail` está anulada (NOP en 0x823810 / 0x823B43): siempre LOD 1, sin fundido ni desaparición | Siempre LOD 1 | igual |
| Ventanas | Color de ventana de noche | — | falta |
| Mano | Z-sorter, luz ×1,5, muñeca con alfa | Igual | igual |
| Anillos de agua | `fn_005E5100`: `smoke.raw` horizontal, modo 13, 700 ms; del chapoteo de la mano, objetos que caen al agua, nadadores, tiburones y cebo | Igual (mano y objetos lanzados; no hay nadadores ni tiburones) | igual |
| Barcos | `PetitNavire` de las cinemáticas | — | falta |
| Partículas, hechizos, luciérnagas, destellos | PSys, todo en el Z-sorter | Motor PSys genérico (`src/PSys`): los archivos de hechizo con las ~25 clases más usadas, sprites ordenados por efecto; efectos de guion (`SPECIAL_EFFECT_*`); luciérnagas. Faltan mallas, niebla, cadenas, mapas de luz, las reglas de hechizos concretos y pasar a este motor los efectos de la mano | aprox. |
| Lluvia, nieve, relámpagos | Por tormenta, en casillas de 80×80 | — | falta (sin clima) |
| Correas, gestos, anillo de influencia | Modos 15, 13 y 6 | — | falta |
| Nombres, contadores, ayuda | Z-sorter y retrollamadas de fin de fotograma; fuentes `j0`/`f1`/`f3`; textos de `InfoScript2.txt` | Fuente `j0` original y el mensaje de la cantidad en la mano; faltan los demás mensajes, nombres y contadores | aprox. |
| Sprites | `LH3DSprite` en el Z-sorter | En la pasada principal, ordenados de atrás a delante con los modelos transparentes (`MainBlended`) | igual |
| Fundido de pantalla y bandas | `SET_FADE`/`SET_FADE_IN` por turno; bandas de `SET_WIDESCREEN` en 2 s; quads en FinishFrame | Igual (vista `ScreenOverlay`); sin el negro inicial de `OnNewGame` porque la intro aún no llega a su `SET_FADE_IN` | aprox. |
| Vídeo Bink | Superposición | — | falta |
| Templo y ciudadela | Luces propias, claves Citadel* | `TempleInterior` | aprox. |
| Gamma, posproceso, niebla D3D | No hay | No hay | igual |

Nivel de detalle: el del original, 4 por defecto (`--detail-level`, `Graphics/DetailLevel.h`).
