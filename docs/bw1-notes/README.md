# Notas de Black & White 1 para openblack (rama local `local/hand-hbn`)

Guía de lo descubierto al reconstruir el comportamiento original a partir de `runblack.exe` (v1.42 no oficial sobre
v1.20, disposición W120) y de los datos originales. Todo lo que aquí se afirma está verificado en el ejecutable o
medido, salvo donde se marca **(inferido)**.

Proyecto **solo local**: no se publica nada (ni push, ni PRs, ni forks).

| Página | Contenido |
|---|---|
| [tooling.md](tooling.md) | Desensamblado, símbolos, herramientas de openblack, formatos de datos, mapas de BWLandEditor |
| [engine-math.md](engine-math.md) | Coordenadas, altura del terreno, matrices LH, Zoomer |
| [hand-and-interface.md](hand-and-interface.md) | Mano: colocación, estados, agarre, lanzamiento, objeto bajo el cursor |
| [objects-and-resources.md](objects-and-resources.md) | Montones, vasijas, almacén, árboles (reglas, fuego, sacrificio), rocas, partículas, campos, sonidos de coger |
| [openblack-internals.md](openblack-internals.md) | Dónde está cada cosa en el código, render, tests, ganchos de prueba, depurar cierres, trampas (Vulkan, makeRef) |
| [parity.md](parity.md) | Tabla de paridad del motor gráfico: cada etapa del original y su estado en openblack |
| [original-frame.md](original-frame.md) | Mapa del fotograma original (orden de dibujo, modos de render, estados, niveles de detalle) |
| [rendering.md](rendering.md) | Estados D3D, luz, mar, sombras, cielo, reflejos, peces, anillos, fundido, fuentes y texto del original |
| [mod-library.md](mod-library.md) | Librería de mods: menú, `mods.cfg`, `--mod`, mods de datos, cómo programar uno |
| [mods.md](mods.md) | Paquetes modificados (`AllMeshes.g3d`) y sus diferencias con el original |

## Filosofía

1. Primero todo lo original, verificado en el ejecutable.
2. Lo que se aparte del original va como **mod**, desactivado por defecto, en la librería de mods (`src/Mods/`,
   menú **Mods** del juego, [mod-library.md](mod-library.md)). No confundir con el `AllMeshes.g3d` modificado de la
   instalación (`mods.md`).
3. Verificar antes de dar algo por bueno: compilar, tests, y capturas automáticas (sin ratón si el usuario está usando
   el PC).

## Referencias externas locales

- `C:\Users\diewgarc\dev\bw1-decomp`: decompilación coincidente de openblack/bw1-decomp; `config\BW1W120\symbols.txt`
  da nombres a las direcciones.
- `C:\Users\diewgarc\dev\decomp_pickup`: pseudo-C++ reconstruido de la mano, la interfaz y los objetos
  (`hand.cpp`, `interface.cpp`, `objects.cpp`, `multi.cpp` y sus `NOTES_*.md`).
- `C:\Users\diewgarc\dev\tmp_dis`: scripts de desensamblado y volcados.
