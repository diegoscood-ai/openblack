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
