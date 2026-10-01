# La mano y la interfaz

Detalle completo en `C:\Users\diewgarc\dev\decomp_pickup` (hand.cpp, interface.cpp y sus NOTES).

## Animación y malla

- Animaciones `Data\CTR\hh.HBN` (pack Lionhead, bloque de morph en 0x2C). Especificación `Data\hndspec5.txt`
  (69 nodos, 26 presentes). Nodos C = poses centrales; L*_lr / L*_fb = capas direccionales −1..+1.
- Malla `Data\CreatureMesh\Hand_Boned_Base2.l3d`: 22 huesos (0 palma, 8–19 dedos, puntas 10/13/16/19, índice = 19,
  20/21 pulgar). El origen del modelo **es el punto de agarre** en los estados de sujetar.
- Escala: `3.2 * handScale / 555.294`; `CHand::SetDistanceFromView` (0x46C0D0): d<10 → (d/10)^0.8; d>150 →
  ×(d/150)·(1 − 0.3·(d−150)/1650).

## Dónde se pone la mano: `ObtainRequiredHandPosition` (0x5B5E70)

1. Objeto bajo el cursor (ver abajo). Si no hay, el terreno.
2. Con objeto:
   - aldeanos (Living que no son criatura): distancia = |centro − cámara| − radio 2D, corregida por la altura;
   - resto: el punto donde el rayo corta la malla;
   - si llevas algo: el rayo de búsqueda apunta al punto del terreno + 0.6·altura de agarre, y el punto final se
     separa hacia la cámara la mitad del radio 2D del objeto sujetado.
3. La distancia a la cámara se suaviza con el Zoomer: **0.1 s** si se acerca, **0.28 s** si se aleja (2.0 s al dar a
   la criatura o en un modo de cámara concreto: no implementado). Nunca más lejos que el terreno bajo el cursor.
4. `HandStateNormal` pone la mano exactamente ahí (sin más suavizado).

## Objeto bajo el cursor

- `GInterface::SendObjectDrawCollision` (0x5D56C0): cada objeto dibujado se prueba con colisión exacta por
  triángulos; gana el más cercano; se ignora lo que está en la mano.
- **Aldeanos** (bandera "humano" del LH3DObject, solo `Villager::CallVirtualFunctionsForCreation` 0x74FC70): sin prueba
  por triángulos. Cuentan si el ratón está dentro del **círculo en pantalla de su esfera envolvente**
  (`LH3DBoundingBox::CheckRegionOnScreen` 0x868C80; radio = escala × semidiagonal de la caja), a la distancia
  `|(x, y + semialto·escala, z) − cámara| − (R + plano cercano)`; el plano cercano es 0,3 + 0,16 × altura de la cámara
  sobre el terreno (0,3..3,5, `LandFeature::GetNearClipping` 0x5E2F30). Animales y criatura: triángulos, como el resto.
  Informe: `tmp_dis\iface\hover_humans.md`.
- `UpdateInterfaceCollide` (0x5D5A70): la distancia del terreno cuenta 2,3 más (fn_005D5980); si aun así queda
  delante, el objeto solo cuenta si el punto del terreno cae dentro de su huella XZ.
- Si al hacer clic no hay nada: `FindObjectNearMapCoord` (0x5D39E0), el más cercano en ±5 unidades y solo si está
  más cerca que el punto pulsado; primero los peces de una piscifactoría si es agua. **No es un alcance de hover**:
  openblack usa solo el objeto del pick por triángulos (antes tenía un radio inventado de ≥3,5 unidades).
- Mensaje de la cantidad en la mano ("Cantidad: N", fuente `j0`, amarillo con sombra, sin fondo): ver
  [rendering.md](rendering.md#texto-fuentes-del-original-y-el-mensaje-de-la-mano); se ve mientras la mano sostiene comida o madera.
- Las mallas con huesos (aldeanos, animales) se prueban en su pose de reposo, que es como se dibujan
  (`L3DSubMesh`: posiciones de colisión × cadena de huesos del grupo). Los animales cuentan como Living al colocar la
  mano (distancia al centro menos el radio 2D).
- **Excluir todo lo que se mueve con la mano** (objeto sujetado, árbol arrancándose, raíces, partículas): si no, el
  rayo choca con ello y la mano sube hacia la cámara sin fin.

## Estados de acción de la interfaz (`GInterface+0x44`, tabla 0x5D7960)

| Estado | Nombre en el exe | Uso |
|---|---|---|
| 0 | NORMAL | reposo |
| 2 | LANDSCAPE LOCK | agarrar el terreno (cámara) |
| 3–6 | LOCKED SELECT… | coger de un montón por tandas |
| 7 | WAIT FOR PLACE IN HAND | tras coger, espera al paquete |
| 12 | **IN THROW** | segunda pulsación con la mano llena: soltar/lanzar al soltar |
| 13 | grab (225 ms) | pulsación de coger |

Cada estado tiene un "estado de cursor" (`GInterface+0x3AC`); IN THROW = **0x17**.

## Sujetar, muelle y lanzamiento (`HandStateHolding::Update` 0x5B3C70)

- Tipos de agarre (jump table 0x5B568C), altura base h: ABOVE 0.2; MAGIC 3.2·escala; GRAIN/TREE/SIDE/VILLAGER
  max(lowering, 1.9); +0.1·altura si el objeto está enraizado. `lowering = GetHeight·GetHoldLoweringMultiplier`.
- Poses: ABOVE = Chold_above en dur·0.5·(1−grip), grip = min(1, R/(3.2·s·1.2)); SIDE/TREE/VILLAGER = Chold_side en
  (dur>>1)·grip, grip = min(1, R/(3.2·s)). R = GetHoldRadius (en madera es constante; la comida se abre con la cantidad).
- **Muelle (inercia)**: pasos de 10 ms, a = 260·d − 40·v, |v| ≤ 124. **Solo se activa en el estado IN THROW**
  (comprobación `0x3AC == 0x17` en 0x5B4603). Tras coger y soltar el botón, la mano sigue al cursor sin inercia.
- Al soltar en IN THROW: un solo camino para lanzar y dejar (`HandSystem::Release`): `ApplyThisToMapCoord` y luego
  `Object::InitialisePhysicsFromHand` con la velocidad del muelle, que lanza si |v_xz|² > 4 y si no deja el objeto
  (una vasija de la mano: |v|² ≤ 5). Detalle en [physics.md](physics.md#el-agua-en-los-golpes-y-al-soltar).
- Dejar el objeto **sobre el mar no lo coloca**: `Object::InitialisePhysicsFromHand` (0x636F00) solo "aterriza" en
  tierra seca (altitud ≥ 4) o en una celda de altitud > 1, así que sobre el agua el objeto queda en física y flota o
  se hunde (ver [physics.md](physics.md) y [water.md](water.md#hundirse-ahogarse-y-borrarse)). En tierra tampoco hay "colocar": el cuerpo baja al suelo y un aldeano, un
  animal, una valla o un árbol derecho salen de la física al momento; lo demás se asienta con la física. Una vasija de la mano soltada en el mar tampoco
  deja montón: el recurso se pierde (`Pot::AddResourceToPos` 0x66F270). Un **aldeano** soltado ahí se hunde en ~0,4 s y
  pasa 60 s ahogándose (clip 252 con grito y chapoteos) antes de morir; un animal desaparece a los ~7,5 s. Si la celda
  de agua tiene altitud ≥ 2 el aldeano sí "aterriza", pero `Villager::EndPhysics` ve `IsWater` y también se ahoga (en
  Land1 no hay ninguna celda de agua tan alta).
- Balanceo: hasta 0.3 rad según el suavizado del ratón (±80 px, referencia 1024 de ancho). El giro ±π/2 de lado solo
  ocurre mientras hay una entrega a la criatura pendiente.

## Coger

- Umbral de 225 ms entre tocar y coger. Los montones no se pueden tocar: la pulsación empieza a coger por tandas al
  momento.
- Rocas con radio 2D > 3.6 no se pueden levantar (`Rock::ValidForPlaceInHand`). Pulsar sobre ellas las golpea y las parte en dos; ver [physics.md](physics.md).
- La mano nunca llama a `CanBePickedUp`; la puerta es `GInterface::PlaceObjectInMagicHand` (0x5DA6F0).
