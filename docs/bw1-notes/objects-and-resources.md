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

## Campos (Field, informe `tmp_dis\field\field_notes.txt`)

- Los 6 GFieldTypeInfo son iguales: ageGrowth 80, ageRecolt 1200 (maduro), timesToSow 30, foodValueTakenWithHand 25,
  totalFoodInField 350, maxFarmerInFarm 10, sol 0,5/1,5, lluvia 1,5/1,5, ratioBeforeRipe 0,2. El símbolo `IsUnripe`
  (0x5298D0) devuelve **maduro** (crecimiento ≥ 1200).
- `Process` 0x529020 cada 10 turnos (+ un desfase 0..9), con los 30 cultivos sembrados y sin madurar:
  d = 2·(0,5·alineación + 1)·(0,5 creciendo | 1,5 madurando o con lluvia); crecimiento += d, comida += d·350/1200.
- `RemoveFood(n)` 0x5295A0: 0 sin comida o sin sembrar; coste = n maduro, (int)(1,2·n) sin madurar; si no alcanza:
  sin madurar se vacía y da (int)(0,2·n); maduro da lo que queda y se borra entero (hay que sembrar otra vez).
- Mano: botón de acción sobre el campo (selección bloqueada; necesita crecimiento > 0 y comida > 1). Empieza con
  (int)min(25, comida), **la mitad si está maduro**, quitado del campo; por turno (int)min(8 + 62t², comida), t =
  min(turnos/60, 1), ≤ 20000 − lo de la mano, la mitad si maduro; la mano recibe n (rareza). Partículas de grano
  (`SF_MultiPickUpFood`), sonido G_PICKUPFOOD. No se puede devolver.
- Dibujo `Draw` 0x528570: una malla (MSH_T_WHEAT); solo con crecimiento ≥ 20 y comida ≥ 25; se hunde v = comida/350 − 1
  en 1 s (y += 2·v·escala·alto) y se desvanece por debajo de v = −0,8; color de oliva a verde claro creciendo, a
  blanco madurando; los maduros se mecen con el viento de los árboles.
- openblack: `ecs/Fields`, `HandFish.cpp` (`TryPickUpField`, `UpdateFieldPickUp`), gancho `OPENBLACK_HAND_TEST_FIELD=1`.
  **Diferencias**: no hay granjeros, así que los campos empiezan sembrados y maduros y se vuelven a sembrar solos al
  vaciarse; faltan el tinte de color, el vaivén y la alineación/lluvia en el crecimiento.

## Sonidos (informe `tmp_dis\sound\notes.txt`)

- El tono de LHAudio es un **porcentaje** de la frecuencia del wav (100 = normal): AL_PITCH = p/100; la desviación del
  .sad (±%) se sortea al empezar. El tono y el volumen del .sad solo cuentan si su bit está en las banderas de
  +0x244 (0x1 tono, 0x20 volumen 0..127); si no, 100 y 127. openblack pasaba el número crudo (casi siempre 0).
- Coger de un montón, campo o piscifactoría: **un solo canal en bucle** (G_PICKUPWOOD 98 para madera; G_PICKUPFOOD 44
  para lo demás) cuyo tono sube a 60 + 180·t² % por turno; se para al soltar o al acabarse. Dejar en un montón:
  G_PileFood/Wood(Small) según la cantidad (< 200 pequeños). openblack: `HandSystem::UpdatePickupSound`,
  `AudioManager::SetEmitterPitch`.
