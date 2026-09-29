# Coordenadas, terreno, matrices y Zoomer

## MapCoords

- Punto fijo 16.16: `fixed = mundo * 6553.6` (1 celda = 10 unidades = 65536). Mundo = `fixed * 0.000152588`.
- Estructura `{x int, z int, altitude float}`; la altura final de un objeto suele ser `GetAltitude(pos) + altitude`.

## Altura del terreno: `LH3DIsland::GetAltitude` (0x803090)

Portado exacto en `LandIsland::GetHeightAt`:

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

T < 0.001 fija el valor. Implementado en `src/Common/Zoomer.{h,cpp}`. Lo usan la distancia de la mano
(g_HandDistZoomer) y el hundimiento de los montones (T = 1 s).
