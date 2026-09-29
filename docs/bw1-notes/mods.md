# Paquetes modificados

El `Data\AllMeshes.g3d` actual es un mod del usuario (626 mallas, texturas 0x01–0x70). No hay copia del paquete base.
Referencias de mallas originales: el paquete de Creature Isle (`...\CreatureIsle\Data\AllMeshes.g3d`, **otros
índices**: consultar su `AllMeshes.h`). El paquete `Ultimate\Data\AllMeshes.g3d` es otro mod (704 mallas).

## Texturas incrustadas

- Las palmeras (mallas 586–589) llevan su textura incrustada con id **0x1001**, pero su material pide 0x85–0x88, que no
  existen en el paquete. Ultimate repite el patrón. En el juego original se ven bien; openblack usa la skin
  incrustada de la malla cuando el material no encuentra su textura.
- En `LH3DMesh::Create` (0x806460) las skins incrustadas se registran con `fn_008379E0`, y el valor 0x1001 podría ser
  formato + id (**inferido**).
- Escáner: `python C:\Users\diewgarc\dev\tmp_dis\mod\scan_skins.py <pack.g3d> <salida.txt>` lista las mallas cuyos
  materiales piden texturas que no están ni en el paquete ni incrustadas.

## Origen de las mallas (rocas flotantes)

- Las mallas de rocas del mod tienen el origen desplazado respecto a las originales:
  `MSH_Z_SPELLROCK01` vértice más bajo +0.8 (original −0.6), `MSH_BOULDER3_LIME` +0.2 (original −0.2).
- Las altitudes de los scripts están pensadas para las mallas originales, así que con el mod quedan en el aire (en
  Land1, 74 de 243 objetos estáticos flotan más de 5 cm). El original no lo corrige.
- Además, muchas rocas del mod quedan apoyadas en una punta por su giro: aunque el vértice más bajo toque el suelo,
  parecen flotar. Solución prevista: físicas.
