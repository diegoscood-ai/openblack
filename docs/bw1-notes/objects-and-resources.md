# Objetos y recursos

## Vasijas y montones (Pot / PileResource)

- `Pot::Create` (0x66CF10): potType 0 = Pot simple, 1 = PileFood, 2 = PileWood. Las vasijas de mano (HandWood 11,
  HandFood 12) también son montones.
- **Vasija simple**: escala `min(5, cantidad/scaleEvery + 0.25)` (`Pot::GetScaleFromAmount` 0x66D4A0). `scaleEvery` no
  se usa para nada más.
- **Montón**: no escala; se hunde. `PileResource::SetSize` (0x66E900): objetivo `(GetProportionRaised − 1)·altura`,
  animado en 1 s con el Zoomer; se dibuja en `GetAltitude(pos) + desplazamiento` y no se ve si está enterrado del todo.
- `GetProportionRaised` (0x66F1B0 madera / 0x66EB60 comida): x = cantidad/maxInPot en [0,1];
  p = x > 0 ? 0.05 + 0.95x : 0; comida: 1 − (1 − p)².
- Al crearse, todo montón empieza enterrado (`−altura`) y sube en 1 s (`CallVirtualFunctionsForCreation` 0x66E300).
- Escalas de creación: **MagicFood 0.3**, **MagicWood 0.7** (constructores 0x5FA9F0 / 0x600E20); el resto 1.
- `PileFood::Draw` (0x51BF80): el montón de comida del almacén (info 2) y la comida mágica (info 10) desplazan la
  textura en V `0.25·(1 − clamp(desplazamiento/altura + 1))` (LH3DObject vfunc 0xE8, **inferido** como desplazamiento de
  UV). El grano parece quieto y el montón "encoge".
- `FoodPile` (info 8, MSH_B_WORSHIPGRAIN) es el montón de los lugares de culto; su malla está desplazada.

## Coger por tandas (multi pick-up)

- Pulsación sobre un montón: 25 al momento a una vasija de mano. Cada turno de 0.1 s: `(int)(8 + 62·t²)`, t = n/60,
  hasta **20000** por tanda (`maxAmountCanBePickedUp`). Verificado: 413 a los 3 s, 1746 a los 6 s.
- La mano queda fija en x,z sobre el montón, y = suelo + altura del montón.
- Soltar el botón deja la vasija en la mano; otra pulsación + soltar la deja o la lanza.
- Dejar: se une a un montón o almacén del mismo recurso cercano (radio aproximado 15 m) o crea MagicWood/MagicFood.
- Partículas (`SF_MultiPickUpWood/Food`, `ER_MultiPickup::ModifyAtomCollection` 0x6A77C0): 8 por segundo, cada una va
  en línea recta en 1 s del suelo bajo la mano a la posición actual de la mano; se destruyen al dejar de coger.
  Madera = malla MSH_I_OFFERING_WOOD a 0.35 con `AppearanceRuleTumble`; comida = granos de S_SpriteSheet1 (32 frames,
  20 fps); pescado = S_Spangle_A (no implementado).

## Almacén (StoragePit)

- Un único total para 5 montones de madera + 1 de comida (`StoragePit::AddResource` 0x732F60 /
  `RemoveResource` 0x7332A0).
- Añadir madera: montones 1→5, cada uno hasta 5000 salvo el último (sin tope). Sacar: **5→1**, da igual desde qué
  montón se coja. Un montón a 0 queda enterrado y no se ve.
- `PotStructure::GetResource` (0x66EF00): un montón de almacén informa del total del almacén.

## Objetos estáticos (MobileStatic, rocas)

- Posición `GetAltitude(pos) + altitud del script`, rotación `SetYXZMatrixOnly(y, x, z)`, escala uniforme
  (`Game3DObject::SetPosition` 0x63B680, `MobileStatic::GetWorldMatrix` 0x608DE0).
- El original **no** los asienta en el suelo: `GetAltitudeFondation` solo se usa en edificios.

## Árboles

- Arrancar: tras 225 ms el árbol se inclina hacia la mano y sale al alejar el cursor peso/1000 m. Sonido TreeBreak,
  montón de raíces (malla 593, 15 s) y raíces colgando (malla 592).
- Soltar suave en tierra = replantar (PlantTree, bosque cercano en 25 m o bosque nuevo). Lanzado o en el agua =
  árbol muerto (DeadTree).
- Sobre un almacén = madera `woodValue·escala`.
- Valores de madera (info.dat): roble 800, haya/cedro 700, abedul/olivo 500, ciprés 400, conífera/pino 350,
  palmera 300, seto 100, arbusto 15.
