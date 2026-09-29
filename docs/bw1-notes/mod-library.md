# Librería de mods

Todo lo que cambia el juego original es un **mod**, desactivado por defecto. La librería (`src/Mods/`) los registra,
genera el menú **Mods**, guarda su estado en `mods.cfg` y los activa desde la línea de comandos.

## Para el jugador

- Menú **Mods** del juego: una casilla por mod (con descripción al pasar el ratón), agrupadas por categoría; las
  opciones (p. ej. muestras de MSAA) debajo. `*` = hace falta reiniciar.
- `mods.cfg` junto al ejecutable (se crea al cambiar algo en el menú):
  ```
  graphics.msaa = on
  graphics.msaa.samples = 4x
  water.living = on
  data.MiPackDeTexturas = on
  ```
- Línea de comandos, solo para esa sesión (no se guarda): `--mod water.living`, `--mod graphics.msaa=off`,
  `--mod graphics.msaa.samples=8x`. Los interruptores anteriores siguen como atajos: `--msaa N`, `--mipmaps`,
  `--anisotropic`, `--enhanced-graphics` (= MSAA 4× + anisótropo), `--living-water`, `--ground-static-objects`.

## Mods integrados

| Id | Qué hace | Reinicio |
|---|---|---|
| `graphics.msaa` (+ `samples` 2x/4x/8x/16x) | Antialiasing multimuestreo y alpha to coverage en hojas y vallas | no |
| `graphics.mipmaps` | Mipmaps y filtrado trilineal | sí |
| `graphics.anisotropic` | Filtrado anisótropo (incluye los mipmaps) | sí |
| `graphics.terrain-x2` (+ `method` density/upscale/both) | Terreno más nítido: cada material repetido dos veces por bloque (density, por defecto; wrap Repeat), escalado ×2 con Lanczos-3 al cargar (upscale; `Graphics/TextureUpscale`) o ambos. Solo escalar apenas se nota: cada material de 256 px cubre un bloque de 160 unidades | sí |
| `water.living` | El mar refleja todo, el reflejo ondula despacio en bucle y la superficie deriva (sin la ondulación por filas) | no |
| `world.ground-statics` | Baja las rocas y objetos estáticos que flotan hasta el suelo | no |

Detalles de cada uno en [rendering.md](rendering.md) y [openblack-internals.md](openblack-internals.md).

## Mods de datos

- Una carpeta por mod en `Mods/` junto al ejecutable, con la misma estructura que el juego (`Data/...`,
  `Scripts/...`) y un `mod.cfg` opcional:
  ```
  name = Agua azul
  description = Sustituye Sky.raw y Skya.raw
  ```
- Id `data.<carpeta>`. Se activan en el menú (con reinicio) o con `--mod data.<carpeta>`.
- Un archivo del mod sustituye al del juego con la misma ruta; si dos mods lo tienen, gana la carpeta posterior en
  orden alfabético. Los archivos que solo están en el mod también se ven (p. ej. mapas o texturas nuevas).
- Cómo funciona: `FileSystemInterface::AddOverridePath`; `FindPath` mira primero los mods (solo archivos, nunca
  carpetas) y `Iterate` mezcla la carpeta del juego con la de cada mod.

## Para programar un mod integrado

1. Una clase derivada de `mods::Mod` en el archivo de su categoría (`GraphicsMods.cpp`, `WaterMods.cpp`,
   `WorldMods.cpp`, o uno nuevo con su `Register…Mods` en `BuiltinMods.h`).
2. `Info`: id estable (`categoria.nombre`), nombre, descripción, categoría, `restartRequired`.
3. Opciones con `AddOption({"id", "Etiqueta", {"elección1", "elección2"}, índicePorDefecto})`; se leen con
   `GetChoice("id")`.
4. `Apply()`: pone en marcha el estado actual. Se llama al arrancar (después de `mods.cfg` y la línea de comandos) y
   cada vez que el mod o una opción cambia. Lo normal es escribir un interruptor de `EngineConfig` que lee el motor.
5. El motor nunca decide por su cuenta: todo lo que no es original mira un interruptor que solo pone un mod.

Pendiente (nivel 3): mods externos (Lua o DLL) sobre esta misma API.
