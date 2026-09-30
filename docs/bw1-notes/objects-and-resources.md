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

- Arrancar: el tirón empieza **al pulsar** (StartGrab 0x5D1740 llama a `CHand::PickUp(obj, 1)` en el momento; el umbral
  de 225 ms es solo para los demás objetos); el árbol se inclina hacia la mano y sale cuando la mano se ha movido más de
  peso/1000 m en horizontal desde donde lo agarró (detalle y diferencia con el original en «Tirón», abajo). Sonido TreeBreak, montón de raíces
  (malla 593, 15 s) y raíces colgando (malla 592).
- El montón de raíces (el cráter) es un `LH3DObject::Create(1)`, **morfable** (fn_00825240 → UpdateMelting vt+0x1E8 una
  vez al crearlo): se amolda al terreno como los campos y almacenes (`MorphWithTerrain` en `HandSystem::Uproot`).
- **Soltar** (`Object::InitialisePhysicsFromHand` 0x636F00 + `Tree::EndPhysics` 0x74B830): el árbol cuenta como «dejado
  con cuidado» (bandera 8 del objeto físico) solo si la normal del terreno bajo él apunta arriba (y ≥ 0,7, pendiente
  menor de unos 45°) **y** llega casi derecho (los ángulos x y z de su matriz YXZ ≤ 0,2 rad ≈ 11,5°; un árbol en la mano
  toma el «arriba» de la mano, que sigue la superficie, así que en una ladera va inclinado). Entonces: en tierra y sin
  arder, se replanta; en agua, árbol muerto. Si no cumple, cae con físicas y acaba como árbol muerto. El sonido
  (`Tree::DropSfx` 0x74BC60, G_PLANTTREE + tick%3) lo lanza `PhysicsObject::RemoveObject` 0x646B44 en **todo** soltado
  con cuidado que acabe en tierra, replantado o no. Lanzado = árbol muerto siempre.
- **Bosque al replantar** (0x74B8BF): espiral por las celdas del mapa hasta 25 + 10 m; por cada objeto fijo
  `d = distancia − su radio 2D`. Un objeto de un pueblo (o parte del templo) a menos de 25 m ⇒ el árbol es «de pueblo»
  (bit 1 de +0x5E = `isNonScenic`, ¡se pone a **1** dentro del pueblo!) y se une al bosque **del pueblo**, que gana a
  cualquier otro; si no, hereda el bosque del árbol con bosque más cercano (sin límite propio, solo los 35 m de la
  búsqueda); sin ninguno y fuera de pueblo, crea un bosque nuevo. Efectos: humo blanco `SmokyStuff` en el suelo (en
  openblack, el polvo del agarre), `SPOT_VISUAL_FOREST_CREATED` (0x2C) **siempre que no sea en pueblo**,
  `StartImmersion(0x2E)`, mímica de criatura y alineación buena (estos tres sin portar).
  *Desviación*: el original saca el bosque del pueblo de una lista que el pueblo guarda (Town +0x608) y openblack no
  modela esa lista: el primer árbol plantado en un pueblo crea su bosque (`ecs::TownForestId`).
- Sobre un almacén = madera `woodValue·escala·GLandBalance[5]` (`Tree::GetDefaultResource` 0x74B7A0, × vida). Un **árbol
  muerto** da menos: `DeadTree::GetDefaultResource` 0x511330 = `woodValue·escala` sin vida ni balance de tierra.
- Valores de madera (info.dat): roble 800, haya/cedro 700, abedul/olivo 500, ciprés 400, conífera/pino 350,
  palmera 300, seto 100, arbusto 15.

### Crecimiento (`Tree::Process` 0x74A290, `Tree::Grow` 0x74A3F0)

- Solo crecen los árboles **de un bosque**: en el original únicamente `Forest::Process` 0x539DA0 recorre sus árboles, así
  que los árboles sueltos del guion (todos los de Land 1 y Land 2, que llevan bosque −1) no crecen nunca. Los de Land 3
  (65), Land 4 (82) y Land 5 (164) sí.
- Un árbol nace «creciendo» (bit 0 de +0x5E) solo si su `maxSize` es distinto del tamaño con el que se crea, y su
  contador (+0x60) arranca en un turno al azar de [0, growTurns) (ctor 0x749E00).
- Cada `growTurns` turnos (10 en los 22 tipos, o sea 1 s): `amt = growAmt · (1 + 0,01·rainMultiplier·lluvia) ·
  (1 + 0,5·alineación del terreno)`, y `escala = min(escala + amt, maxSize)`. `growAmt` 0,01 (0,02 conífera/pino, 0,005
  roble/olivo/palmera). Al llegar al máximo deja de crecer. `SetScale` es virtual y rehace la colisión: el círculo de
  obstáculo sigue al tamaño.
- openblack: `src/ECS/Trees.cpp` (`ProcessTreesTurn`, `GrowTree`), llamado desde `Game::Update` con los campos. **Sin
  clima ni alineación de terreno todavía**: lluvia 0 y alineación 0, así que `amt = growAmt`. Ganchos
  `OPENBLACK_TEST_TREE_GROWTH="x,z"` (dos brotes, uno con bosque y otro sin), `OPENBLACK_TREE_TRACE=1` (cada paso
  de crecimiento y el brillo) y `OPENBLACK_TEST_REPLANT="x,z,gradosDeInclinación"` (suelta un árbol ahí y dice si se
  replanta, cae o queda muerto).
- **Bosques** (`Forest`, ctor 0x539BD0; `ECS/Trees.cpp`): un bosque es un objeto con centro e id (CREATE_FOREST, o
  `new Forest(pos, 0)` al replantar fuera de todo bosque; id 0 = el siguiente libre). CREATE_TREE y CREATE_NEW_TREE
  buscan el id del guion en la lista de bosques y, si no existe, el árbol **no tiene bosque** (0x7162BE): los ids 0-6
  de Land5 y el −1 de Land1/Land2 quedan sin bosque. Cada turno (`Forest::Process` 0x539DA0): un bosque vacío espera
  2000 turnos y se borra; si no, crecen sus árboles y puede plantar uno nuevo: `r = 2000 + azar(1000)`,
  `f = min(1, 0,05·crecidos)`, `T` = turnos desde el último árbol que plantó cualquier bosque (global 0xCD04C8),
  `c` = intentos del bosque (+1 por turno); si `c·f·T/300 > r`, planta junto a uno de los `azar(n/2+1)` crecidos más
  cercanos a su centro (`Forest::CreateNewTree` 0x539FD0) y `c` vuelve a 0. **Plantar junto a un árbol**
  (fn_0053A010): 32 ángulos desde uno al azar (2π/32 entre ellos) × 5 radios (entero 5-9 al azar, luego `(r+2) % 10`),
  el primer sitio libre; el árbol nuevo es del tipo del padre, tamaño 0,1, máximo `0,8 + azar(0,4)` y ángulo al azar.
  «Libre» (fn_0074C180) = sin objeto fijo (círculo de 0,5) y en tierra; el original lee `(collide & 8) == 0 || IsWater`,
  la parte del agua parece invertida y se ha tomado como «no en agua» [supuesto]. Con un bosque de 20 árboles crecidos
  sale más o menos un árbol nuevo en el mundo cada minuto y medio (comprobado en Land3: el bosque 19 plantó uno).
- **Agua sobre un árbol** (`Tree::ApplyWaterSpell` 0x74C390, `ecs::ApplyWaterSpell`, para la sesión de milagros): uno
  que crece crece `waterMultiplier·growAmt`; con el subtipo de hechizo 0x17 también uno adulto, la mitad por
  `GetDistanceModifier(tamaño, 3)` (= `SigmoidThreshold(0,5, 1 − min(tamaño,3)/3)`, tabla de 41 pasos en 0xC23284), por
  encima de su máximo. Uno adulto de un bosque regado sin 0x17, pasados 40 turnos del último árbol del mundo, planta
  otro a su lado (el llamador da la alineación buena y la estadística 0xE). Falta el sonido 0x78 + tick%9.

### Dibujado (además del mecido, ver «Campos»)

- **Ranura de viento**: `round(yAngle·16/2π) & 15` (0x74A0E7), guardada al crear el árbol, así que los árboles orientados
  igual se mecen juntos (`components::Tree::windSlot`; antes openblack usaba un hash de la entidad).
- **Brillo por cámara** (`Tree::PreDraw` 0x74A883 → global 0xC22FA0, leído solo por código de árboles):
  `d = normalize(foco de la cámara − posición de la luz)`, `v = normalize_xz(dirección de vista)`,
  `b = dot < 0 ? 200 : 200 + 55·dot`, y `Tree::Draw` 0x74B077 multiplica cada canal RGB del color del árbol por `b/256`
  (0,781 … 0,996). **Rareza del original**: LH3D tiene una sola luz puntual y de día su único `setter` es código muerto,
  así que la luz se queda en el origen del mapa (0,0,0); los árboles se oscurecen un 22 % cuando la cámara mira hacia esa
  esquina. Al amanecer/atardecer el original mueve la luz a un foco pegado a la cámara (sin portar).
  openblack: `ecs::TreeBrightness()` en `ECS/Trees.cpp`, aplicado como color propio en la w de la cuarta columna de la
  instancia (igual que el tinte de los campos), `RenderingSystem.cpp`.
- **Sonido ambiente de hojas** (0x74B111): los árboles de más de 10 de alto con la cámara a ≤ 10 en x y z (y < 18 en y)
  suenan ~1 vez por segundo (`LocalRand(1000/msFotograma) == 1`): fila `{*,*,20,*,70}` de `editor.sad` =
  `G_TreeRustle_01..11` + `G_TreeCreak_01/02`. openblack: `ecs::UpdateTrees` + `AnimationSounds::PlayFromTable`.
- **Curvado junto a lo que pasa cerca** (`Tree::Draw` 0x74AB8B, `fn_005DF1B0`, tabla 0xD19A48): cada fotograma se apuntan
  en la tabla el objeto que lleva la mano (ranura 1: todo objeto en la mano se «dibuja en la mano»), los objetos físicos
  en vuelo (ranuras 3-13 por turno, `fn_00646FE0`) y la criatura del jugador (ranura 2; aún no hay), cada uno con su
  posición y un radio = escala × la semidiagonal de su malla (LH3DMesh +0x30). Cada fuente marca los árboles de las 3 × 3
  celdas de 10 m a su alrededor (gana la última). Un árbol marcado se curva si su copa no está por debajo de la fuente
  (base + altura ≥ y de la fuente) y la distancia horizontal `d` es menor que el radio `r`:
  `ángulo = 0,471239 · (1 − ((r − 0,75)·d/r + 0,75)/r)` (27° como mucho), alrededor del eje horizontal perpendicular a la
  dirección fuente→árbol, la copa **alejándose** de la fuente; solo la matriz dibujada, y en ese fotograma sin vaivén.
  Sonido al empezar a curvarse: clave `{c, *, *, 10, 75}` de `editor.sad` con `c = 3` si la curva es < 0,3 (sin
  muestras), 2 si < 0,67, 1 si no: `G_Crash_Tree_M_01..08` («rubbing trees»). openblack: `ecs::UpdateTrees`
  (`UpdateTreeBends`, `Tree::bendAngle/bendDirection`) y `RenderingSystem.cpp`. Comprobado con una roca en la mano
  (`OPENBLACK_HAND_TEST_HOLD=1.5`): los árboles cercanos se apartan y suenan `G_Crash_Tree_M_05/07`.

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
  **Diferencias**: el motor es fiel (el campo nace vacío y solo los granjeros lo siembran, y aún no hay oficios de
  aldeano, así que los campos se quedan vacíos); el mod **`world.crops`** los sustituye: empiezan sembrados y maduros,
  se vuelven a sembrar al vaciarse y crecen `speed` veces más rápido. Como la mano deja para siempre la última unidad
  de comida de un campo maduro (sus cantidades a la mitad y truncadas llegan a 0, y `RemoveFood` solo lo borra si se le
  pide más de lo que tiene), con el mod un campo maduro con menos de 25 (lo que necesita para dibujarse) cuenta como
  vacío y se borra. Falta la alineación/lluvia en el crecimiento.
- **Color y vaivén de la malla** (`Field::Draw` 0x528570, detalle en `tmp_dis\field\draw_colour_sway_notes.txt`):
  `BlendColor` 0x5284C0 (k = 0 da a, 255 da b, `(a(255−k) + b·k)/255` truncado): creciendo, oliva (121,145,25) →
  verde claro (170,212,67) con k = 255·(1 − comida/350); madurando, oliva → blanco con k = 255·(crec − 80)/1120;
  maduro, blanco. Multiplica byte a byte la luz del terreno del objeto, `(c·tinte) >> 8` (fn_0080BF10), antes de la
  neblina y el N·L: en openblack va en el w de la cuarta columna de la instancia, negativo
  (`−1 − r·65536 − g·256 − b`; las ventanas usan > 1,5), solo si el mod world.foliage no pone su `MeshTint`. Los
  maduros se mecen: la columna 1 (eje arriba) se cizalla en z world con `1,75 × escala × T0[i]`, `T0 = −0,03·cos(fase)`
  de 16 fases (`Tree::PreDraw` 0x74A7C0: velocidad Random(1, 2) cada 2 s, fase += ms·vel·0,00106061; el ángulo del
  viento es siempre 0), `i` fijo por campo (en el original, bits de su dirección); solo la matriz dibujada.
  `ecs::FieldDrawColour`, `ecs::WindSway`. Los árboles (`Tree::Draw` 0x74B016) usan la misma tabla con factor 1: x de la columna 1 = 0, z = escala × T0[i], `i` = bits 2-5 de +0x5C; hecho en RenderingSystem salvo con el árbol inclinado por la mano (falta la curva junto a criaturas y objetos físicos, bits 6-9 de +0x5C, tabla 0xD19A48). Con el mod world.foliage
  (`fields = wheat`) el campo se dibuja con plantas que crecen por etapas en vez de la malla (mod-library.md).

## Sonidos (informe `tmp_dis\sound\notes.txt`)

- El tono de LHAudio es un **porcentaje** de la frecuencia del wav (100 = normal): AL_PITCH = p/100; la desviación del
  .sad (±%) se sortea al empezar. El tono y el volumen del .sad solo cuentan si su bit está en las banderas de
  +0x244 (0x1 tono, 0x20 volumen 0..127); si no, 100 y 127. openblack pasaba el número crudo (casi siempre 0).
- Coger de un montón, campo o piscifactoría: **un solo canal en bucle** (G_PICKUPWOOD 98 para madera; G_PICKUPFOOD 44
  para lo demás) cuyo tono sube a 60 + 180·t² % por turno; se para al soltar o al acabarse. Dejar en un montón:
  G_PileFood/Wood(Small) según la cantidad (< 200 pequeños). openblack: `HandSystem::UpdatePickupSound`,
  `AudioManager::SetEmitterPitch`.

## Árboles: reglas, fuego y sacrificio (investigado; fuego pendiente, tótem aplazado)

Informe: `tmp_dis\trees2\` (`pick_rules.txt`, `treeinfo.txt`, `fire_notes.txt`, `totem_notes.txt`).
- **Tirón** (`HandStateTug` Enter 0x5B7DF0 / Update 0x5B8070, en HandTrees.cpp): al empezar, el ancla es la base del
  árbol y el plano de arrastre pasa por ella con la normal del terreno, a la altura de la mano vista a la distancia del
  ancla. Tras 0,13 s (el fundido del cambio de estado), cada fotograma la mano va al corte del rayo del ratón con ese
  plano; el agarre está en `base + arriba × bajada` (bajada = 0,1 × altura, mínimo 3,2 × escala de la mano × 0,3 al
  empezar); un muelle `F = 1000 × (mano − agarre)` (tope 600000) lo inclina con par `(r × F)/1000` y rozamiento
  cuadrático 4 alrededor de la base, y el tronco se estira hasta ×1,3 (Zoomer 0,3 s). Sale cuando `|F| > GetWeight`
  (escala³ × peso de info.dat): agarrado lejos del punto de agarre, sale enseguida. Soltado antes, vuelve a su postura.
  Al final de cada Update (también en los primeros 0,13 s) la mano se coloca en el agarre del tronco estirado
  (`CHand+0x78 = matriz × (0, bajada, 0)`); solo se dibuja ahí, el siguiente Update la vuelve a poner en el plano.
  **Consecuencia (2026-09-30, por confirmar con el original)**: como la mano antes de pulsar está sobre el rayo del
  ratón, el plano queda a la altura a la que ese rayo cruza el eje del árbol, así que el primer tirón es esa altura menos
  la bajada: una haya de escala 1 (18 m, agarre a 1,8 m, peso 1000) solo se inclina si se pulsa a menos de ~1 m del
  agarre (de 0,8 a 2,8 m sobre la base); pulsada en la copa sale a los 0,13 s. El openblack de antes de las físicas
  medía solo la distancia horizontal del cursor a la base y se inclinaba pulsara donde pulsara.
  **Lo que hace openblack (2026-09-30, a petición del usuario, que recuerda el original así)**: no se usa el muelle
  literal (además el estirado ×1,3 hacía que el árbol subiera y bajara). Al pulsar se guarda el punto agarrado y su
  distancia en el rayo del ratón; el tirón es cuánto se ha movido en horizontal la mano (el rayo a esa distancia) desde
  entonces. El árbol se inclina hacia ella hasta 0,25 rad y sale cuando pasa de peso/1000 m (escala³ × peso de
  info.dat). Agarrado en cualquier sitio y sin mover el ratón, no sale. Si algún día se puede probar el original, se
  puede comprobar con `tmp_dis\trees2\tugwatch.py` (lee la memoria de runblack.exe: plano, mano, agarre, estado).
  Gancho: `OPENBLACK_TEST_TUG="x,z,espera,mantener"` (+ `OPENBLACK_TEST_TUG_MOUSE2="x,y"`, el cursor se mueve 0,5 s
  después), trazas con `OPENBLACK_HAND_TRACE=1`.
- **Reglas de coger**: `Tree::ValidForPlaceInHand` = 1 e `IsTuggable` = 1 para los 22 tipos, a cualquier escala (arbustos,
  setos, palmeras, bosquecillos, dentro o fuera de pueblos). Solo lo impiden la bandera 0x2000 (partidas guardadas y
  puzles), estar fuera de la influencia o una selección bloqueada; entonces va por el camino de "tocar", que para
  árboles no hace nada. `BigForest` (**hecho**): no se tira; al agarrar (225 ms) `InterfaceSetInMagicHand` 0x4393C0
  hace `RemoveResource(WOOD, 350)` (madera del Conifer) y pone en la mano un Conifer nuevo (escala 1, ángulo 0).
  `RemoveResource` 0x4390D0: la madera del bosque (+0x84; al crearlo woodValue × escala, **inferido**) baja 350 y la
  escala pasa a madera/woodValue; sin madera suficiente da lo que queda y el bosque se borra. `AddTreeAround`
  0x439220: hasta 10 ángulos al azar a su radio; en tierra y sin objeto a menos de 4 (distancia + radio), un Pine de
  escala 0,05, ángulo al azar y tamaño máximo 0,5 + azar(0,5). openblack: `HandSystem::TakeTreeFromForest`
  (HandTrees.cpp), `BigForest::wood`, gancho `OPENBLACK_HAND_TEST_FOREST=1` (Land1: 15000 → 14650, escala 0,977). DeadTree/FelledTree: se cogen sin tirón. Arrancar: `G_TREEBREAK` + 1 empujón de
  alineación malvada (`GAlignment::Update`); replantar, bueno.
- Tabla GTreeInfo (info.dat, runtime = registro + 0x10, paso 0x140): madera 700 Beech/Cedar/Copse, 500 Birch/Olive,
  350 Conifer/Pine, 800 Oak, 300 palmeras, 400 Cypress, 100 setos, 15 arbustos; peso 1000 (arbustos 20, setos 100);
  capacidad calorífica 1000 (arbustos 100, setos 200); sacrificio 400/500/1000 (Oak)/250/350/100/200/110; temperatura
  de combustión 110 para todos.
- **Fuego** (`SpreadEffect.cpp` / FireEffect en Object+0x44; turno 0,1 s): temperatura T, Tc = max(110, 40); arde si
  T ≥ Tc. Ardiendo T += 0,1·T/(2Tc) hasta 2Tc; enfriando (T ≤ anterior) T −= (T + 10 − amb)·4·H·R·0,1·k/capacidad
  (k = 50 sobre agua con y < 2, 1 + 0,01·lluvia). Daño: vida −= (T − Tc)/Tc·0,001 por turno (muere en ~100 s);
  carbonizado con vida < 0,6. A vida 0 el árbol desaparece. Contagio: cada turno busca en R + 10 m, R =
  1,25·radio2D·clamp((T − 0,8Tc)/1,2Tc); calor q = min(10·dT, 0,5·(Ts − amb)·cap_s) → el objetivo gana q/cap_t (los
  arbustos prenden ~10× antes). Un árbol ardiendo se puede coger y sigue ardiendo; sostenido sobre algo que arde, o
  lanzado, prende lo que toca; al caer se vuelve DeadTree ardiendo. Sin rayos ni fuego aleatorio. Visual: color ×
  max(50, 255 − (1 − vida)·2550)/256 (casi negro al perder un 8 %), modo 230 + calor·25/255, escala × 5·vida por
  debajo de 0,2; llamas `FireGraphic` (sprites `S_Fire.raw`, humo `S_SpriteSheet3.raw`, luz `S_LMFireBall.raw`),
  2 llamas por árbol de 0,2·alto; sonido de fuego en bucle.
- **Sacrificio** (aplazado por el usuario): soltar un árbol apuntando al **WorshipTotem** (CitadelPart del sitio de
  culto; `ValidToApplyThisToObject` 0x74BD50): v = sacrificeValue·vida·(0,5 + 0,5·vida) al maná del sitio
  (+0xF0) y al total (+0xF4), fantasma del árbol (`GoolooGooloo`), `G_SACRIFICE_01`, número flotante rojo "%3.0f".
  Aldeanos ×1,25; comida, rocas y vasijas no. Necesita los sitios de culto (`CREATE_WORSHIP_SITE` aún no hace nada).

## Creación desde CHL (CREATE 27 / CREATE_WITH_ANGLE_AND_SCALE 252)

Desensamblado en `tmp_dis\mapa\chl_creatething_6F11A0.txt`.
- `GScript::CreateThing` 0x6F1B20 y `CreateWithAngleAndScale` 0x6F2E10 (ángulo en grados, ×0,0174533) solo aceptan
  los tipos 1..41 y llaman al switch `fn_006F11A0` (tabla 0x6F1A70). Si no se crea nada, el guion recibe **0**
  ("Thing not created"). openblack devolvía la entidad 0 (una entidad válida) para todo lo no soportado.
- El subtipo 5000 solo vale para Timer, SpellDispenser, Whale, Ark, Marker, Ball, Poo y Scaffold.
- **Marker** (`fn_0070D8D0`): un `ScriptMarker` con solo la posición. `MapCoords::Set` 0x603340 guarda
  `y − GetAltitude`, `GET_POSITION` 0x6F88A0 devuelve `GetAltitude + relY` y `ScriptMarker::PhysicsEditorCreate`
  0x561030 no hace nada, así que el marcador devuelve exactamente el vector con que se creó (y = 0 en CHL).
- Los demás objetos se quedan en el suelo: `PhysicsEditorCreate` (GameThingWithPos 0x401980, MobileStatic 0x55D720,
  Bonfire 0x4397C0) pone relY = 0; tras crear, 0x6F1591-0x6F1A42 rehace la matriz en `GetAltitude(pos) + relY` con
  solo el ángulo Y y la escala del objeto (sin inclinación X/Z).
- Casos: Feature `fn_00527350`(ángulo, escala); Villager `Villager::Create` 0x74FBE0 con edad grownUpAge + 1,
  VillagerChild con edad 10 (sin pueblo ni casa); Animal y Bird `fn_00419C20`; MobileStatic y Rock: subtipo 6 →
  GBaseOnly `fn_00609340` (sin ángulo ni escala), 7 y 59 → `GStreetLantern::Create`, el resto `fn_00608770` (info 8 →
  Bonfire, rocas con info +0x128 = 2); MobileObject `0x607000`, Poo = MobileObject 5, Ark = 23; Tree
  `Tree::Create` 0x749EE0 (sin bosque); AnimatedStatic `0x421F50`. Abode, Town, Dance, Flock, InfluenceRing, Citadel,
  WorshipSite, SpellSeed, Mist, Field, ComputerPlayer y TotemStatue dan "Invalid create type" también en el original.
  Pendientes en openblack: Reward, Creature, DeadTree, WeatherThing, Store, Timer, Vortex, Whale, Ball, OneShotSpell,
  PuzzleGame, Totem, SpellDispenser, Highlight y Scaffold.
- openblack: `CreateScriptObject` (CHLApi.cpp), `MarkerArchetype`. El círculo de Singing Stones ya se monta en
  (2496,67, 2246,33) sobre el suelo. Las funciones CHL sin implementar se registran una sola vez por función.

## Niebla del mapa (CREATE_MIST)

- `CREATE_MIST` "AFNFF" (0x7155C9) → `Mist::Create` 0x6063D0(pos con relY = F1, tamaño F3, color N2, k F4) →
  `CallVirtualFunctionsForCreation` 0x606420: `LH3DObject::Create(7)` (LH3DMist) en `GetAltitude(x, z) + F1`,
  +0x88 = F3, +0x90 = N2 >> 24 (alfa), bandera +0x80 bit 1; solo si F4 ≠ 1, +0x8C = F4 y bit 2. El constructor de
  LH3DMist 0x7F9560 pone +0x88 = 1, +0x8C = 3, +0x90 = 0x80 y el contador +0x84 = Random(0, 16) & 15.
- `Mist::SetFade` 0x606800(tamaño inicial, tamaño final, alfa inicial, alfa final, segundos): pone ya el tamaño y el
  alfa iniciales y `fn_00606880` suma un paso por turno de 0,1 s durante segundos × 10 turnos (alfa limitado a
  0..255); `Get2DRadius` 0x606660 = escala × la mayor semiextensión x/z de la malla. Lo usan `CREATE_MIST` 263 y
  `SET_MIST_FADE` 264 de CHL (una llamada de cada en challenge.chl; aún sin hacer).
- Land1 tiene 17 (pantano, cueva del flautista...). openblack: `MistArchetype`, `components::Mist`,
  `Renderer::DrawMists` (dibujo en rendering.md).

## Animales y rebaños (CREATE_FLOCK, CREATE_NEW_ANIMAL)

Desensamblado en `tmp_dis\mapa\all_cases.txt` (casos 24, 25 y 49).
- **CREATE_FLOCK** "NAANNN" (0x71634A): `Flock::Flock` 0x52F780(A1, el jugador actual, id N0) → id en +0x8C, +0x60/+0x6C
  = A1, +0x50 = 0x50, +0x52 = 0x1E, en la lista g_game+0x205C44 (se inserta delante); `SetDomainCentrePos`(A2) → +0x14.
  Radio del dominio +0x50 = N3 (0 → 0x50). Con `VERSION` ≥ 2,1 (0xD9957C; todas las tierras traen 2,3): distancia del
  rebaño +0x52 = N4 y pueblo N5; antes, pueblo N4 y +0x52 se queda en 0x1E. Con pueblo: +0x34 y la lista del pueblo
  +0xF08. Invisible (solo simulación).
- **CREATE_NEW_ANIMAL** (0x716543; CREATE_ANIMAL 0x71649F igual con edad 0): busca el rebaño por +0x8C en esa lista
  (el más nuevo con ese id) y el pueblo con `FindTownWithID` → `fn_00419D10`(pos, info, pueblo, rebaño, edad).
  - Con rebaño: edad 0 → GameRand(20) + 5; crea el animal (`fn_00419E00`) y lo une (`fn_0052FA50`: lista +0x3C
    ordenada por el byte +0xD4 del ser, +0x48 miembros, `Living::SetFlock`); si el animal no se puede pastorear y el
    rebaño tiene pueblo, el rebaño sale de la lista del pueblo y +0x34 = 0; +0x88 = máximo de miembros.
  - Sin rebaño (`fn_00419C20`, también el CREATE de CHL): edad 0 → **GameRand(40)** + 5; el animal recibe un rebaño
    propio (`Flock(Living*)` 0x52F950, en su posición, sin id de guion, +0x50 = info.domainRadius (+0x25C),
    +0x52 = (int)info.flockDistance (+0x21C), sin pueblo).
  - Pueblo del animal (`fn_00417C50`, +0xE0 y la lista +0x984 del pueblo): solo lo guardan los que se pueden pastorear
    (`IsOkToBeShepherd`, vtable +0xBA4 = 0x41D0E0 → 1); los demás, ninguno.
- **Clases** (`fn_00419E00`, salto por info.animalInfo +0x1F4, 27 casos): terrestres (león, tigre, lobo, leopardo,
  SpellWolf, PieceLion/Wolf/Villager; ctor 0x41FD30 o 0x416EB0), de pasto (oveja, tortuga, vaca, caballo, cerdo,
  PieceSheep; ctor 0x41D0B0, se pueden pastorear) y voladores (cuervo, paloma, golondrina, pichón, gaviota, murciélago,
  SpellDove y SpellBat; ctor `Dove` 0x41DCF0). Los tipos 5 (cabra), 7 (cebra), 17-19 y > 26 (caballo, vaca, tortuga y
  cerdo de puzle) **no crean nada**.
- **Voladores**: el ctor `Dove` 0x41DCF0 llama al de Animal (0x416EB0, que llama a `Living::SetState` 0x5F2A80),
  reinicia campos (`fn_00417900`) y pone la altitud de su MapCoords (+0x1C) = info.altitudeNormal (+0x278: paloma,
  pichón y murciélago 20, cuervo, golondrina y gaviota 40); `Game3DObject::SetPosition` 0x63B680 los pone en
  `GetAltitude + altitudeNormal`. `CallVirtualFunctionsForCreation` es 0x41F240 (la de Animal más una llamada al
  objeto 3D). `StandAnimation`: paloma 8 (DOVE_FLAP), golondrina 27 (SWALLOW_FLAP), gaviota 22 (SEAGULL_TAKEOFF),
  murciélago 2 (BAT_GLIDE). **Falta** decodificar su vuelo (estados de Living y el `Process` de Dove): openblack aún no
  los crea (81 de los 116 animales de Land1), pero cuentan como Object en el contador de creación.
- openblack: `components::Flock`, `Animal::flock/town`, `Town::flocks`, `RegistryContext::flocks`, `AnimalArchetype`.

## Datos de simulación del mapa (solo datos, nada se dibuja)

- **SET_TOWN_UNINHABITABLE** (caso 5, 0x715542): pueblo +0x5F4 = 1 (`Town::uninhabitable`).
- **CREATE_TOWN_CENTRE** (caso 9, 0x71577C): pueblo o el más cercano (`fn_00552FF0`); `IsOkToCreateAtPos` 0x404B10;
  `Abode::Create`; si es un TownCentre: pueblo +0x9A4 = el centro si estaba vacío (`Town::centre`) y
  `Town::SetWorshipPercentage`(N5·0,001) 0x73C060, que guarda +0x5C0 **solo si el pueblo tiene lugar de culto** (si no,
  0) y lo pasa a la estatua tótem. Sin pueblo, `TotemStatue::SetWorshipPercentage` 0x738270. Todas las tierras pasan 0.
- **CREATE_PLANNED_ABODE** (caso 8, comparte código con CREATE_ABODE): pueblo o el más cercano, si no nada; tipo de
  abode 0x404 (TownCentre) → `PlannedTownCentre::Create` 0x7444D0, si no `PlannedAbode::Create` 0x405600 (pos, info,
  pueblo, ángulo N4·0,001, escala N5·0,001; comida y madera no se usan); invisibles (`PlannedMultiMapFixed::Draw`
  0x648930 = `ret`). openblack: `Town::plannedAbodes`.
- **CREATE_ARENA** (caso 69) → `fn_00424820` → GArena 0x4246F0 (pos, radio +0x30, lista g_game+0x205C7C); su
  GLightSheet solo se dibuja durante un combate. openblack: `components::Arena`.
- **Clima** (casos 60-63): `CREATE_WEATHER_CLIMATE`(id, info, pos, r1, r2) → `fn_00771300`: id 0 = `GClimate(0)`
  0x771020 (ignora el resto); si no, GClimate 0x771170 (pos +0x14, radios ordenados +0x20/+0x24, id +0x28, info +0x2C;
  lluvia y temperatura iniciales del rango de la estación, no portado), lista g_game+0x205CF4. `_RAIN`(id, F1, N2, N3,
  N4) → +0x34 {F1, N2, N3, (u8)N4}; `_TEMP`(id, F1, F2) → +0x44/+0x48; `_WIND`(id, F1, F2, F3) → +0x4C..; el clima se
  busca por id (`fn_007731B0`, el más nuevo); id 0 usa el clima del mundo g_game+0x250534, creado al vuelo; id
  desconocido no hace nada. Land1: zonas 1 (2701, 2567; −35/−32 grados, nieve), 2 y 3. openblack:
  `components::Climate`.
- **CREATE_DRINK_WAYPOINT** (caso 95) → 0x770BC0 (WayPoint.cpp, lista g_game+0x205C74): punto donde bebe la criatura.
  Land1 tiene 47. openblack: `components::DrinkWaypoint`.
- **FIRE_FLY_SPELL_REWARD_PROB** (caso 88): `GMagicInfo::GetInfoFromText` 0x5FB3B0 compara sin mayúsculas con el nombre
  de los 42 efectos de magia (el primero que coincide; "NONE" siempre es el 0; si no hay, 42) → 0x52B630: fuera de
  rango no hace nada; si no, tabla 0xCCFBAC[i] = p y rehace las sumas acumuladas en 0xCCFB04. No se reinicia entre
  tierras.
- **Globales**: `VERSION` → 0xD9957C; `SET_LAND_NUMBER` → g_game+0x205A08 (0 en el ctor de GGame);
  `SET_TOWN_INFLUENCE_MULTIPLIER` / `SET_PLAYER_INFLUENCE_MULTIPLIER` → g_game+0x250078 / +0x25007C, que
  `GGame::Init` 0x54F66F pone a 1 antes del guion. openblack: `Game::GetMapScriptGlobals`.

## Piscifactorías (CREATE_FISH_FARM / CREATE_TOWN_FISH_FARM)

- Caso 31 (0x7166E1): 0x52C7B0(pos, GFishFarmInfo[N1] (0xCCFC78 + 0x128·i; info.dat solo trae el 0), sin pueblo).
  Caso 32 (0x716722): sin el pueblo no crea nada; si no, lo mismo con él. El ctor 0x52C360 guarda en +0x8C **siempre
  el pueblo más cercano** (`Town::GetNearestTownToPos` 0x73B170, cualquier tribu), sea cual sea el del guion.
- Banco de peces (`CallVirtualFunctionsForCreation` 0x52CC10, revisado): con [0xC37BF4] = 0 (sin aplanar el mar,
  `GetAltitude` 0x803090 usa la altura cruda), anillos de radio 2, 4... < 50 y 32 direcciones; la primera dirección con
  altura exactamente 0 en dos radios seguidos da el centro. openblack ya lo hacía igual (`GetUnflattenedHeightAt`), así
  que los 9 de Land1 sin banco (los del lago del pueblo 2 y otros) salen igual que en el original con los mismos
  datos; no se cambió la búsqueda.

## Objetos del guion del mapa (farolas, hogueras, árboles muertos, puertas)

Desensamblado en `tmp_dis\mapa\all_cases.txt`, `d_streetlantern.txt`, `d_deadtree_isok.txt` y `d_animstatic_cvffc.txt`.
- **Parámetros**: en el bloque de argumentos del guion, el entero del parámetro i está en +0x6000 + 4i y el float en
  +0x6030 + 4i. `GMobileStaticInfo` ocupa 300 bytes en memoria (0xD3A6D8 + 300·i: MS[6] = 0xD3ADE0, MS[7] = 0xD3AF0C,
  MS[8] = 0xD3B038) y 284 en `info.dat`: en memoria el registro de info.dat empieza en +0x10 (el tipo de objeto está en
  info +0x10 y el clip de un AnimatedStatic en +0x128, que es +0x118 en `GAnimatedStaticInfo` de openblack). Las 61
  infos de MobileStatic tienen el tipo de objeto 0x1C (MOBILE_STATIC).
- **CREATE_STREET_LANTERN** (caso 80, 0x717720) → `GStreetLantern::Create` 0x7346E0(pos, &MS[N1]): no crea nada si en la
  celda del mapa de la posición (`MapCoords::FindType` 0x6045C0 → `MapCell::FindTypeOnMap` 0x6015E0; celdas de 10
  unidades) hay un objeto de tipo 0x1C a menos de 0,5 m en x/z (`GUtils::GetDistanceInMetres` 0x74CD70); cualquier
  cosa hecha con una info de MobileStatic: rocas, hogueras, farolas, árboles muertos. +0x58 = (info ≠ MS[7]).
  `CallVirtualFunctionsForCreation` 0x734810: malla 148 (MSH_B_CAMPFIRE) si +0x58, si no 398 (MSH_O_TOWNLIGHT);
  `SetPosition((x, GetAltitude + y, z), ángulo 0, escala 1)` (sin giro de 180°), la luz `fn_00823240`(ese punto,
  +0x58) en +0x5C y, **en las dos clases** (no mira +0x58, corregido: antes esta nota decía "solo en la de pueblo"), el
  sonido 0x93 en +0x60 (`fn_0071E8C0` = `SoundTag::Create`), salvo si el objeto lleva la marca UNAVAILABLE (+0xA & 1);
  detalle del sonido en [day-night-weather.md](day-night-weather.md). Land1: 8 de tipo 7 y 4 de tipo 59
  (farolillos de campo con la malla de la hoguera, **no** hogueras). El CREATE de CHL con 7 o 59 va por el mismo sitio.
  openblack: `StreetLanternArchetype`, `components::StreetLantern` / `LanternLight`; `night_lights` pone las luces
  según `LanternLight` (antes por la malla, y las hogueras de verdad salían con luz de farolillo).
- **CREATE_BONFIRE** "AFFF" (caso 73, 0x7176AE) → `fn_00439850`(pos, F1 temperatura, F2 ángulo Y, F3 escala) → ctor
  0x4395C0: `Rock`(pos, MS[8], ángulo, escala) y `CreateSpotVisualWithSpecifiedDuration`(pos, 25 SF_Bonfire, 1,0, −1 =
  siempre, la hoguera); la temperatura no se usa al crear (Land1 trae 24,7, que openblack tomaba por el ángulo). Sin
  luz de farolillo. openblack: `BonfireArchetype`.
- **MobileStatic**: `CREATE_MOBILESTATIC` "ANFF" (caso 41) → `fn_00608770`(pos, info, 0, 0, F2 ángulo, F3 escala):
  MS[8] → `Bonfire::Create` con temperatura 100; info +0x128 = 2 → `Rock`; MS[6] → nada; el resto `MobileStatic`.
  `CREATE_MOBILE_STATIC` "ANFFFFF" (caso 42) → `fn_00608840`(pos con relY = F2, info, 0, 0, F3, F4, F5, F6): MS[6] →
  GBaseOnly `fn_00609340`; MS[7] → nada; el resto `fn_00608770`(…, F4, F6); después `SetXYZAnglesAndScale`(F3, F4, F5,
  F6) sobre lo creado (también la base y la hoguera). openblack: `MobileStaticArchetype::CreateFromInfo` /
  `CreateWithXYZAngles`.
- **CREATE_DEAD_TREE** "ALNFFFF" (caso 43, 0x716E64) → `fn_00510BB0`(pos, GTreeInfo[N2], jugador, F3, F4, F5, F6, 0):
  ctor 0x510A30 = `Rock`(MS[3], ángulo 0, escala 1) + `SetLife`(F3); con 0xCC5F10 = 0, `GetDeadTreeMesh` 0x510C60 es la
  malla normal del tipo; luego `SetXYZAnglesAndScale`(F4, F5, F6, 1), la matriz de MobileStatic (x = F4, y = F5,
  z = F6). Land1: 3 (tipos 12, 4 y 4, vida 1, ángulos pequeños). openblack hacía un árbol quemado vivo; ahora
  `DeadTreeArchetype` (`components::DeadTree`, sin Tree ni bosque, se puede coger).
- **CREATE_POT** (caso 38): `IsOkToCreateAtPos` y, si la cantidad N3 ≤ 0 (0x716B19), nada. Quita los 4 montones de
  madera vacíos de Land1.
- **CREATE_NEW_FEATURE** (caso 75): con N5 ≠ 0 crea un `PlannedFeature` 0x527440 (no se dibuja); ninguna tierra lo usa.
- **Nombres**: features `fn_00527740` y animated statics `fn_00422600` comparan con `_stricmp` (si no hay, devuelven el
  número de infos, 0x4C / 0x10); `GAbodeInfo::GetInfoFromText` 0x405A70 recorre las 9 tribus, compara el prefijo con
  `_strnicmp`, exige '_' y la descripción con `_stricmp` (16 por tribu); si no, −1. El original usa el resultado sin
  comprobarlo; openblack registra el fallo y se salta el comando (desviación de robustez deliberada; antes lanzaba).
- **AnimatedStatic** (`CallVirtualFunctionsForCreation` 0x422300): pone el clip de info +0x128 (Norse Gate 191, Gate
  Stone Plinth 195, Piper Cave Entrance 189); `Draw` 0x422770 lo avanza o retrocede según esté abierta, limitado a su
  duración: cerrada es t = 0 (openblack: `SkeletalAnimation` parada en 0). Con malla 212 (Norse Gate, `fn_004230D0`)
  crea 2 `Game3DObject` con la malla 398 en (∓15, 30, 0) de la matriz de la puerta (filas con escala + traslación),
  ángulo 0 y escala 1, cada uno con la luz `fn_00823240`(su posición, 0).
- **CREATE_PLANNED_CITADEL** (caso 20): pueblo y jugador obligatorios; `fn_00467DD0` (PlannedTownCitadelHeart en el
  pueblo) y guarda la posición en 0xC5E258. El templo de verdad sale de `PlannedTownCitadelHeart::CreatePlannedNoFixedCheck`
  0x467EF0 (vtable +0x504: la `Citadel` del jugador si no tiene, `fn_00462B10`, y `CitadelHeart::Create` 0x464E20), que
  llama `Town::AddBuildingSiteNoFixedCheck` 0x73B8A0 desde `Town::RequestBestPlanned`, `Town::ForceBuildingOfPlannedAtPos`
  0x73E560 (`GScript::BuildBuilding` 0x6FAB30 de CHL, y 0x641774 tras `StartPlaygroundGame` con 0xC5E258) y
  `Scaffold::TryToBuildPlannedBuilding`. `GGame::Birthday` → `GPlayer::Birthday` → `Town::Birthday` solo rehace
  estadísticas. Al cargar el mapa **no hay templo**, solo el plan (GameThingWithPos 0x4C, sin malla, sin celda, sin
  índice de creación, sin aplanado; `Draw` 0x648930 = `ret`). Info "Citadel Heart" (info.dat 0x115C0): madera 5,
  timeToBuild 150, desireToBeBuilt 1,0, malla 564 BuildingDummyCitadel; tipo de abode del plan 0x804 (cívico).
  - Conversión 0x467EF0 (arg `float life`): el jugador es el **dueño del pueblo** (`Town+0x2C`, el de CREATE_TOWN o el
    neutral), no el del script (ese solo se valida). `CitadelHeart::Create`(pos, info, citadel, ángulo del plan, escala
    del plan, life, 1): el 1 marca "en construcción" (MultiMapFixed 0x52E1E0, +0x58 bit 1, +0x5C = 0).
    `CallVirtualFunctionsForCreation` 0x4675A0 crea el LH3D tipo 8 a **escala 1** con y = altitud(origen) + alt y llama
    0x882730 (malla B_FIRST_TEMPLE, % construido, **aplana la tierra**): el aplanado es al convertir. Lugares de culto
    (`fn_00464F50`) solo si life ≥ 1. Luego heart+0x94 = pueblo, `PostCreatePlanned` 0x648C50 y se borra el plan.
  - `AddBuildingSiteNoFixedCheck` pasa siempre life 0,0 y crea un `CitadelBuildingSite` (0x468DC0 → 0x43D1E0); lo
    terminan los aldeanos (`CitadelHeart::Built` 0x465000). Con vida < 1 `Draw` 0x882A40 usa `DrawPartialyBuilt`
    0x816AD0 (sin decodificar).
  - Disparadores: **Land 1** = CHL `FollowUs`: `BUILD_BUILDING((1915.05, 0, 2508.89), 1.0)` (la pos del plan de
    Land1.txt:95; `GetPlannedAtPos` 0x73E4C0 coge el plan más cercano a menos de radio de la malla 564 × escala + 1 m),
    luego `CALL_NEAR(Citadel 18)` + `SET_PROPERTY(22, 0.375)`; `PreventCitadelCompletion` lo limita a 0,9 y
    `CheckCitadel` espera 1. **Lands 2-5** = IA: `Villager::CheckSatisfyCivicBuildings` 0x758E90 (deseo del pueblo
    FOR_CIVIC_BUILDING 6, 0x748330) → `RequestBestPlanned` 0x73A650 → `GetBestPlanned` 0x73A140 (máscara 4).
  - **CREATE_CITADEL** (`Citadel::CreateCitadel` 0x463240) pasa (ángulo, 1,0, 1,0, 0) a `CitadelHeart::Create`: la
    escala del script se **ignora** (Kapa's Land1 Playground pasa 0, otros mapas 300 o 4121) y sale construido.
  - openblack: CREATE_CITADEL dibuja a escala 1 (antes usaba la del script: en Kapa's Land1 Playground el templo era
    invisible). CREATE_PLANNED_CITADEL exige pueblo y jugador válidos (si no, nada), el templo es del dueño del pueblo
    (`Town::owner`) y se dibuja a escala 1. **Desviación pendiente**: como no hay deseos de pueblo, sitios de
    construcción, BUILD_BUILDING/SET_PROPERTY 22/CALL_NEAR ni dibujo parcial, el templo se sigue creando ya construido
    (y aplanando) al cargar, para que no desaparezca de Land 1-5. Hacerlo fiel requiere portar todo lo anterior.
- **IsOkToCreateAtPos** 0x638C40: falla si `MapCoords::CollideCollideWithFixe` 0x604FE0 → `MapCell::CollideWithFixe`
  0x601D10 da el bit 8 y la celda no es agua. El bit 8 sale de un círculo `NewCollide::Obj` de radio 0,5 (0x82AD90)
  contra el `GetCollideData` (vtable +0x858) de cada objeto fijo de la lista +4 de la celda (`Obj::Collide` 0x829140);
  los demás bits vienen de `MapCell::Collide` 0x601BD0 (bit 0x10 del bloque de tierra, fuera del mapa). Informe
  completo: `tmp_dis\mapa\flecos_isok.md` (simulación `isok\sim.py`).
  - **Quién lo llama**: solo CREATE_TREE (27, 0x716235), CREATE_NEW_TREE (28, 0x7162EE), CREATE_POT (38, 0x716B0C, antes
    de mirar la cantidad) y CREATE_MOBILEOBJECT (40, 0x716C71). Si falla, no crea nada, no escribe nada y el guion sigue.
    Ángulo y escala no se usan. Los handlers CHL no lo llaman. CREATE_TOWN_CENTRE usa otro (`GAbodeInfo::IsOkToCreateAtPos`
    0x404B10, sin portar: en Land1-5 no rechaza ninguno). Abodes, campos, features, mobile statics, hogueras y árboles
    muertos se crean sin mirar nada.
  - **La prueba**: círculo de 0,5 en (x, z) del guion (la altura no cuenta, `MapCoords(char*)` deja y = 0) contra los
    objetos de **su celda**; prueba 2D `dx² + dz² <= (ra + rb)²` y luego los hijos. Con agua en la celda (bit 0x10,
    `hasWater`) se crea siempre; fuera del mapa (o en un bloque vacío) también.
  - **Formas**: árbol = círculo de 0,3 en su posición, solo en su celda (0x74C5F0). MultiMapFixed (abode, centro,
    campo 594, feature, animated static, mobile static, roca, hoguera, árbol muerto, dispensador) = `NewCollide(LH3DObject)`
    0x829390 desde el bbox de la malla: centro del bbox girado con `x' = x·cos a − z·sin a`, `z' = x·sin a + z·cos a`;
    semiejes `max(1, escala·mitad)` en x y z; si largo/corto > 1,4, círculo exterior `sqrt(ex²+ez²)` con
    `int(largo/corto)+1` hijos de radio corto en fila por el eje largo (0x82ADD0 / 0x828F40); si no, un círculo de
    `max(ex, ez)`. Se mete en cada celda cuyo círculo (centro de la celda, 7,1) la toca. El bbox (0x8081B0) pasa las
    mallas con huesos (flag 0x100) por `LH3DAnim::SetTransform`, como el de openblack. Sin collide data: BigForest,
    vasijas, mobile objects, aldeanos, animales, farolas y planificados.
  - **openblack** (`ECS/MapCollide.h/.cpp`, `openblack::ecs::map_collide`): rejilla de celdas que se vacía en
    LOAD_LANDSCAPE y se llena con los parámetros del guion (malla del `Mesh` del objeto creado, ángulo Y y escala del
    guion, no el Transform). Sin registrar aún: piscifactorías, CitadelHeart (0x468FB0) y WorshipSite (0x77E490), sin
    decodificar. `OPENBLACK_LOG_ISOK=1` escribe una línea por rechazo (orden, posición, qué lo tapa).
  - **Resultado** (comprobado con `OPENBLACK_DUMP_ENTITY_COUNTS`): Land1 1395 → 1351 árboles (44 rechazos: los 43 de
    `sim.py` − 2 bajo la Piper Cave Entrance + 3 bajo los árboles muertos), mobile objects 51; Land2 921 → 915 (+1
    vasija); Land3 1397 → 1373 y 26 → 20 mobile objects; Land4 799 → 764 y 1 → 0 (+1 vasija); Land5 804 → 782 y
    19 → 13; LandT 341 → 316. Diferencias con `sim.py`: la Piper Cave Entrance es una malla con huesos y `sim.py` usaba
    los vértices sin transformar; los árboles muertos `sim.py` no los modelaba (son Rock con la malla normal del tipo,
    creados con ángulo 0 y escala 1, y los ángulos del guion son casi 0). Captura: Land1 junto al Boulder1 Lime
    (2120, 2494), ya sin los árboles de encima.
