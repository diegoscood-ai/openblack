# Objetos y recursos

Vasijas y montones de comida y madera, coger por tandas, el almacén, los objetos estáticos y las rocas, los campos y los
sonidos (coger, LHAudio y QMixer, canales, mano en el agua, ambiente). Todo **fiel** salvo lo marcado **(inferido)**.
Los árboles están en [trees.md](trees.md) y la carga del mapa en [map-loading.md](map-loading.md).

- [Vasijas y montones](#vasijas-y-montones-pot--pileresource)
- [Coger por tandas](#coger-por-tandas-multi-pick-up)
- [Almacén](#almacén-storagepit)
- [Objetos estáticos](#objetos-estáticos-mobilestatic-rocas)
- [Campos](#campos-field-informe-tmp_disfieldfield_notestxt)
- [Sonidos](#sonidos-informe-tmp_dissoundnotestxt)
- [Movido a otras páginas](#movido-a-otras-páginas)
- [Pendiente](#pendiente) · [Ganchos de prueba](#ganchos-de-prueba) · [Fuentes](#fuentes)

## Vasijas y montones (Pot / PileResource)

- `Pot::Create` (0x66CF10): potType 0 = Pot simple, 1 = PileFood, 2 = PileWood. Las vasijas de mano (HandWood 11,
  HandFood 12) también son montones.
- **Vasija simple**: escala `min(5, cantidad/scaleEvery + 0.25)` (`Pot::GetScaleFromAmount` 0x66D4A0). `scaleEvery` no
  se usa para nada más.
- **Montón**: no escala; se hunde. `PileResource::SetSize` (0x66E900): objetivo `(GetProportionRaised − 1)·altura`,
  animado en 1 s con el Zoomer; se dibuja en `GetAltitude(pos) + desplazamiento` y no se ve si está enterrado del todo.
- `GetProportionRaised` (0x66F1B0 madera / 0x66EB60 comida): x = cantidad/maxInPot en [0,1];
  p = x > 0 ? 0.05 + 0.95x : 0; comida: 1 − (1 − p)². Una sola copia, `ecs::object::GetProportionRaised`
  ([engine-math.md](engine-math.md#tamaño-de-los-objetos)); el radio 2D de una pila de comida es
  `GetProportionRaised × Object::Get2DRadius` (0x66F180), así que vacía mide 0.
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

Movido a [trees.md](trees.md) (arrancar, soltar, bosques, crecimiento, dibujado).

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
  neblina y el N·L: en openblack va en la x de la quinta columna de la instancia, negativo
  (`−1 − r·65536 − g·256 − b`, `lh3d_colour::PackInstanceTint`, [rendering-objects.md](rendering-objects.md#los-campos-de-color-del-objeto-en-la-instancia)), solo si el mod world.foliage no pone su `MeshTint`. Los
  maduros se mecen: la columna 1 (eje arriba) se cizalla en z world con `1,75 × escala × T0[i]`, `T0 = −0,03·cos(fase)`
  de 16 fases (`Tree::PreDraw` 0x74A7C0: velocidad Random(1, 2) cada 2 s, fase += ms·vel·0,00106061; el ángulo del
  viento es siempre 0), `i` fijo por campo (en el original, bits de su dirección); solo la matriz dibujada.
  `ecs::FieldDrawColour`, `ecs::WindSway`. Los árboles usan la misma tabla ([trees.md](trees.md#dibujado)). Con el mod world.foliage
  (`fields = wheat`) el campo se dibuja con plantas que crecen por etapas en vez de la malla (mod-library.md).

## Sonidos (informe `tmp_dis\sound\notes.txt`)

- El tono de LHAudio es un **porcentaje** de la frecuencia del wav (100 = normal). Al empezar (0x1001278B, enteros sin
  signo): d = desviación·p/100, p = p − d + rand·2d/32767 (0 → 100), frecuencia = rate·p/100 (división entera; lo mismo
  en `LHSampleSetPitch` 0x10013520, que no hace nada si el canal ya tiene ese p). El tono, el volumen, los bucles
  (+0x248) y el modo del .sad solo cuentan si su bit está en las banderas de +0x244 (0x1, 0x20, 0x40, 0x400) y quien
  llama no los ha puesto (máscara +0x1C de las opciones); si no, 100, 127, 0 y 3. openblack pasaba el número crudo.
- **Volumen** (verificado con Unicorn, `tmp_dis\agua\re\emu_qmixer.py`): LHaudio manda a `QSWaveMixSetVolume`
  floor(maestro·v/127)·258 (0x100133C1; maestro = `AudioSampleMasterVolume` de BWSetup = 127 → v·258, 0..32766) y
  QMixer lo guarda como vol/32767 (0x18007AE5) y lo **multiplica** por la ganancia de la distancia (0x1800AE20):
  ganancia lineal v·258/32767 (`sample_play::QMixerGain`, `Sound::volume`). El "user param" del .sad
  (`LHSampleGetUserParam` 0x10014230) es la mitad alta de ese mismo u32 (+0x25C >> 16).
- Coger de un montón, campo o piscifactoría: **un solo canal en bucle** (G_PICKUPWOOD 98 para madera; G_PICKUPFOOD 44
  para lo demás) cuyo tono sube a ftol(60 + 180·t²) % por turno; se para al soltar o al acabarse. Dejar en un montón:
  G_PileFood/Wood(Small) según la cantidad (< 200 pequeños). openblack: `HandSystem::UpdatePickupSound` con
  `audio::PlaySoundEffect` (modo 2 del .sad, dueño 0, `audio::SetPitch`, `audio::StopSoundEffect`); trazado con `OPENBLACK_HAND_TEST_FISH=1`: un solo arranque y
  tono 0,60 → 1,02 en los 3 s del gancho. El original lo pone 3D (+0x0C 0, no sigue a nadie) en el punto +0xC8 del
  estado de la interfaz, que es **la mano**: `GInterface::Process` → `fn_005D2250` manda en el paquete 0x15 la
  posición de la mano (`CHand`+0x78, `Morphable::position`, 0x5D2350); `GPacket` 0x63CA9E → 0x5DBFB0 la guarda en
  +0xA4 (y la cámara en +0xB0/+0xBC); `GInterfaceStatus::Process` 0x5DC4E7 → `fn_005DBC60` calcula la velocidad de la
  mano con +0xA4 − +0xC8 y copia +0xA4 a +0xC8 (0x5DBF1F) **antes** de `ProcessInInteract` (0x5DC574), que llega a
  `UpdateMultiPickup`. Así que suena donde está la mano al empezar (el modo 2 no lo mueve después) y no arranca con la
  cámara a más de 180 de la mano. openblack: la posición del `Transform` de la mano.
- El reproductor ya **no pone AL_PITCH = 1 cada fotograma** (`AudioPlayer::UpdateSource` lo hacía y borraba el tono
  del .sad y el de `SetEmitterPitch`): el tono se fija al crear la fuente y con `SetSourcePitch`.
- **Distancias** (`QSWaveMixSetDistanceMapping {min, max, escala}`, LHaudiodllR 0x10012159): .sad +0x268 / +0x26C /
  +0x270 si las banderas 0x80 / 0x100 / 0x200 están puestas; si no, 1 / 9999 / 0,3 (`LH_SamplePlayOptions` 0x10010E90).
  QMixer (0x1800ACDF, 0x1802CE50; banderas del canal 0x103/0x111 de 0x10012065: ni 0x800 "tope en max" ni 0x1000
  "lineal"; verificado con Unicorn): ganancia 1 hasta min (o con escala 0), `min / (min + escala·(d − min))` hasta max
  (min/d con escala 1), **0 más allá de max** (el canal sigue sonando, mudo). La escala es por canal porque LHaudio no
  usa el mezclador por hardware (opción `UseHardware` de `HKCU\Software\Lionhead Studios Ltd\Audio\Override`, que no
  existe; con ella llamaría a `QSWaveMixSetListenerRolloff(4)`). En openblack: `AL_INVERSE_DISTANCE_CLAMPED` con referencia = min y rolloff =
  escala (`AudioPlayer::SetSourceDistance`), y el canal más allá de max queda mudo en `AlSampleOutput` (los 16
  canales de `audio::sample_play`; desde la fase B5 del audio no hay emisores `AudioEmitter`). `Sound` guarda además `cloneGroup` (+0x118), `playMode` (+0x274 con 0x400, si no 3),
  `atmosGroup` (+0x11A) y `atmosFrequency` (i32 en +0x27C, −1 = no es de atmósfera; comprobado con `offsetof`).
- `GAudio::PlaySoundEffect` 0x429E30 con posición: **no empieza** si la cámara está más lejos que el max del .sad
  (+0x26C crudo, `LHSampleGetMaxDistance` 0x10014170; el max del mapeo si es 0) → `audio::PlaySoundEffect` (`Audio.h`; antes `AudioManager::PlayAt`, retirado en la fase B5 del audio).
  Además (0x429F36..0x429FD9, y lo mismo en `SamplePlayAnimEffect` 0x42A4B0): con la panorámica puesta por un guion
  (+0x45E8 y +0x45EC) no suena ningún sample de user param 1 (InGame tiene 60, editor 82, p. ej. `G_WaterFlow`);
  dentro de la ciudadela (`g_game+0x205A28 == 1`) solo los de user param 2; tras `SET_GAME_SOUND false` (GScript+0x90,
  0x7100B0, que además hace `LHSampleStopAll`) solo los bancos de diálogo HelpSprites y Villagers. Los choques de
  física van por `SamplePlayAnimEffect` → `LHSamplePlayAnimEffect` 0x100146F0: no empiezan a más del max del .sad ni de
  800 (0x426E6B) y el dueño del canal es el objeto. Todo en `Audio/SamplePlay` (`PlaySoundEffect`, `PlayAnimEffect`).
- **Canales y modos** (`LHSamplePlay` 0x100113B0: reparto 0x10011020, arranque 0x10011420; `Audio/SamplePlay`): 16
  canales (+0xCC, constructor 0x1001535E). Cada canal recuerda banco, dueño (+0x20 de las opciones: 0 ninguno, −1 el
  del ambiente, o el objeto), sample, grupo de clones (.sad +0x118) y prioridad (.sad +0x240). Modo 1: un canal libre.
  Modo 2: **nada** (ni posición nueva) si un canal del mismo banco y dueño toca el mismo sample o uno del mismo grupo
  (> 0); si no, uno libre. Modo 3: el canal del mismo banco, dueño y sample (se reinicia), si no el del mismo grupo
  (> 0), si no uno libre. Sin libre: el de menor prioridad si es menor que la del sample (se reinicia); si no, no suena.
  `G_BigSplash` (modo 2) no vuelve a sonar mientras suene el mismo sample del mismo objeto; los diez `G_HandInWater`
  (grupo 4, modo 3, sin dueño) se reinician unos a otros; los sueltos del ambiente (modo 3, dueño −1) reinician su
  canal si vuelven a salir mientras suenan. Solo cuentan para los 16 los samples que pasan por `sample_play`: los
  golpes de física, el agua y el polvo de la mano, el bucle de coger, el ambiente, las `SoundTags` (cascada) y los
  del barco. Aún van por su cuenta (temas de otras sesiones): `AnimationSounds`, las rocas, el silbido de la cámara,
  `G_RockPast` y los `PlaySample` de coger/plantar/romper de la mano.
- **Seguimiento y oyente, una vez por turno** (`fn_004270D0` desde `ProcessAudioGameTurn`): `LHSampleUpdate3DChannels`
  0x10014310 lleva cada canal 3D con +0x0C (1 por defecto) y sin AtmosInfo a la posición de su dueño
  (`Get3DSoundPos`, función del juego 0x427200): sin dueño, **la cámara**; dueño −1 o que ya no está, se para; a más de
  su max (+0x6C) de la cámara, se para. Los choques ponen +0x0C = (código A de la tabla ≠ 0x16, 0x64689F); la mano en
  el agua y el de coger, 0. Después `LHListenerUpdate`: el oyente de QMixer = la cámara (posición, delante y arriba,
  velocidad 0) **solo una vez por turno** (y no en pausa ni en los 5 primeros turnos); openblack igual
  (`AudioManager::UpdateListener`).
- Los .sad son wavs RIFF (`QSWaveMixOpenWaveEx`); openblack probaba antes MPEG y dr_mp3 encontraba tramas dentro de
  alguno (`G_BigSplash_03` duraba una fracción de segundo): ahora un RIFF va directo al lector de wav.
- **Ejes del oyente**: openblack es levógiro y OpenAL dextrógiro; posiciones y velocidades ya iban con x ↔ z, la
  orientación (at, up) no, y la izquierda y la derecha salían al revés. Ahora pasa por el mismo intercambio.
- **Mano en el agua / agarrar tierra** (`StartLandscapeGrip` fn_005D1AB0): una sola rama elegida por el **bit de agua
  de la celda** (`InBounds && IsLand` 0x5D1F94; fuera del mapa = agua), no por la altura. Agua: anillo,
  `G_HandInWater_01..10` = InGame 99 + contador (0xD18228, 0..9, avanza aunque se descarte), **3D en (x; 0,2; z)**,
  vol 127, tono 100 ±5 %, min 40 / max 150 / escala 4, y el susto de los peces. Tierra: polvo y
  `G_HandGrabLand_01..06` = InGame 4 + LocalRand(6) (0x5D1FC4, `SoundTag` sin objeto, 2D: is3D 0, vol 10, tono
  60 ±15 %). Los diez de agua están en el **grupo de clones 4** y se tocan en modo 3 sin objeto: `LHSamplePlay`
  (0x10011146..0x100111BC) reinicia el canal del anterior → **uno a la vez** (`sample_play`, HandFish.cpp).
  Traza: `OPENBLACK_AUDIO_TRACE=1`.
  - Condiciones (resueltas): la rama de tierra (0x5D1FA8) no hace polvo ni sonido si `g_game+0x25005C` (el
    **HelpSystem**) tiene la **pantalla panorámica** puesta (+0x45E8) **por un guion** (+0x45EC = número de tarea del
    guion, `GScript::SetWideScreen` 0x6F7BF0 → `HelpSystem::SetWideScreen` 0x5C6AD0; los vídeos y la reproducción
    pasan 0); la de agua (0x5D1FF0) no hace nada con el **juego en pausa** (`g_game+0x14` bit 4, que conmuta
    `PauseGame` 0x54AE20) ni si el objeto 3D de la mano (`CHand+0x482C`) dibuja algo sostenido (+0x8C, lo pone su
    `SetHeldG3D` vt+0x234 = 0x816830; vacío también con una `SpellSeed` que no se dibuja en la mano). En openblack:
    `ScreenFade::IsWideScreenOn` (solo `SET_WIDESCREEN`), `Game::IsPaused` y `!_held` (`HandPlacement.cpp`).
- **Ambiente (atmos)** (hecho, 2026-09-30; `Audio/SoundMap`, `Audio/AtmosBanks`; informe `tmp_dis\agua\audio.md` §1-3):
  - Zona de la celda: `Terrain::GetAtmosType` 0x7352B0 = `(flags >> 2) & 0xF` (bits 2..5 del byte 7; **1 = SEA** fuera
    del mapa o sin bloque). Tabla 0x9CB048 de 14 tipos {nombre, banco, de día}: 1 SEA `ocean.sad`, 2 STILL_FRESH_WATER
    `lake.sad`, 3 COASTAL `shore.sad`, 4 JUNGLE*, 5 ARCTIC, 6 DESERT*, 7 COUNTRYSIDE*, 8 SWAMP*, 9 RUNNING_WATER
    `stream.sad` (ningún mapa base lo usa), 10 STRATOSPHERE `high.sad`, 11 NIGHT* `night.sad`, 12 RAIN, 13 WIND
    (* = de día). Los códigos del editor de Daniels118 son `tipo << 1` (el bit 1 es la celda de agua que no se dibuja).
  - `GSoundMap::Update` 0x71D6F0, cada turno desde `GGame::EndTurn` (antes de `ProcessSoundTags`): receptor = cámara;
    11 × 11 celdas alrededor (radio 50); por tipo, número de celdas y la esquina más cercana. Volumen = radial (1 hasta
    20, 0 a 50) × fundido por la altura de la cámara sobre el suelo en esa esquina (1 bajo 120, 0 a 250); los de día
    además × weatherFade × (1 − noche), noche = max(0, tipo de cielo − 1). La costa se calcula sin eso y el **mar =
    min(radial, 1 − costa)**: a menos de 20 de una celda COASTAL el mar calla (el punto `1464, 2016` de Land1 da SEA 0,
    COASTAL 1; `1300, 2016` da SEA 1). STRATOSPHERE con la altura absoluta: 0 bajo 200, sube hasta 1500, baja hasta
    44444. NIGHT = fundido(cámara) × (1 − fracción de celdas de mar) × weatherFade × noche. RAIN = lluvia/70, WIND =
    (|viento| − 15)/30. `CameraWeather()` lee el clima con `ecs::weather::atmos::GetWeatherSmooth(cámara, true)`
    (`LH3DAtmos::GetWeatherSmooth` 0x835180; el byte 3 es el nublado).
  - `GAudio`: objetivos = volúmenes del mapa, **todo 0 dentro de la ciudadela**: el símbolo
    `HelpSystem::GetWideScreenControl` 0x4282F0 está mal puesto, es `g_game+0x205A28 == 1` (`GoInsideCitadel` 0x554004,
    2 durante el vídeo del hechizo que cae); la panorámica no toca el ambiente (sin interior de ciudadela: nunca). El
    `fn_00429100` copia 15 floats, así que el 15.º (la x de la cámara) cae en `current[0]` (NONE, sin banco: solo se ve
    en la traza). `ProcessAtmosBanks` 0x428FE0: paso 0,04 entre 0,1 y 0,8 y 0,02 fuera (0 → 1 en 33 turnos, 3,3 s),
    grupo 1 si GAudio+0x190 > −0,6 (double en 0x8C4A08) si no 2, `LHAtmosSetBankVolume(trunc(cur·127))`.
    GAudio+0x190 lo pone `fn_005E2240(a)` = 2a − 1 con a = clamp(a, 0, 1), desde `fn_0064AC30` en
    `GPlayer::ProcessPlayers` cada turno: a = (alineamiento del **jugador de más influencia en la posición de la
    cámara**, `MapCoords::CalculateMostInfluentialPlayer` + `GPlayer::GetAlignmentValue`, + 1) / 2; `GAudio::Reset` (al
    limpiar el mapa) lo deja en 0. openblack: `atmos_banks::Alignment()` lee `GameQueries::cameraAlignment`, que
    `ecs::audio_queries` saca de `ecs::effects::alignment::GetInterfaceAlignment()` (el fn_0064AC30 de Milagros, una
    vez por turno) con la fórmula de fn_005E2240; la misma x que el objetivo del alineamiento del cielo, así que el
    gancho de prueba del cielo y el deslizador de depuración también le llegan (audio.md, «Fase C: C2»).
    Solo desde el turno 6 y sin pausa (`g_game+0x14 & 4`); si no, `AtmosProcess(0)`: se paran los bucles y **solo los
    canales con AtmosInfo** (0x10001EBF: bucles = 1, sueltos = su entrada), no los demás samples. Con un vídeo
    (`g_game+0x250188`) no corre `LHAtmosProcess(1)` (openblack no tiene vídeos en partida). Al cambiar de mapa
    (`GAudio::Reset`) se paran ambiente y samples, pero los volúmenes de los bancos se conservan.
  - Mezclador del DLL (`fn_10001610`, `LHAtmosProcess` 0x100018B0): muestras con +0x27C = 0 son **bucles** 2D (vol del
    .sad o 127, fundido de entrada +5 por turno sin tope hasta llegar a su volumen, ganancia banco·fade/127; se cortan
    en seco cuando el banco llega a 0); +0x27C = f > 0 son **sueltos** en **una sola cola** para todos los bancos,
    próximo = contador + 4f + rand·12f/32767, y al registrar un banco contador = cabeza − 20. **Como mucho uno por
    turno** (la cabeza, si toca): suena si su banco no está a 0, relativo al oyente en QMixer (x, y, 0) con x, y =
    2 − rand·4/32767; si |x| + |y| ≤ 1 se multiplican por 4 y si quedan en (0, 0) van a 5·(a, b), con a, b = ±1 que
    rotan (a' = −a, b' = −a·b). Se vuelve a poner en la cola aunque no haya sonado. Los sueltos de otro grupo que el de
    su banco bajan 5 por turno. En Land1 hay 15 bucles y 400 sueltos.
  - **Ejes del modo relativo** (verificado con Unicorn, `emu_polar.py`): LHaudio pasa la posición relativa (+0x14) a
    polares (0x10012269): acimut = atan2(x, y) en grados (con π tomado como 1/0,318471 y |ftol| de los negativos),
    elevación = atan(z/|xy|), alcance |xyz|; QMixer (0x1800AA85) vuelve a derecha = r·cos(el)·sin(ac), arriba =
    r·sin(el), delante = r·cos(el)·cos(ac). O sea **x derecha, y delante, z arriba**: los sueltos (x, y, 0) quedan en el
    plano horizontal alrededor del oyente, a 2..7,1 (p. ej. (2, 2) delante a la derecha, (0, −4) detrás), atenuados con
    su mapeo (min 1, escala 2 o 4: a (2, 2) con escala 2, 0,215). En openblack: `sample_play::PolarRelative` y el emisor
    relativo en el espacio del oyente de OpenAL (derecha, arriba, −delante). Trazas: `OPENBLACK_AUDIO_TRACE=1` (canales)
    y `OPENBLACK_ATMOS_TRACE=<n>`. Comprobado en Land1 (cámara en 2120, 40, 2400): las olas del lago (lake.sad, tono
    160 del .sad) suenan a AL_PITCH 1,600 en (−1, 1) y (1, 2).

## Movido a otras páginas

- Árboles: reglas, fuego y sacrificio → [trees.md](trees.md) ([tirón](trees.md#tirón-handstatetug-enter-0x5b7df0--update-0x5b8070-en-handtreescpp),
  [reglas de coger](trees.md#reglas-de-coger-y-bigforest), [fuego](trees.md#fuego), [sacrificio](trees.md#sacrificio)).
- Creación desde CHL → [map-loading.md](map-loading.md#creación-desde-chl-create-27--create_with_angle_and_scale-252).
- Niebla del mapa (CREATE_MIST) → [map-loading.md](map-loading.md#niebla-del-mapa-create_mist).
- Animales y rebaños → [map-loading.md](map-loading.md#animales-y-rebaños-create_flock-create_new_animal).
- Datos de simulación del mapa → [map-loading.md](map-loading.md#datos-de-simulación-del-mapa-solo-datos-nada-se-dibuja).
- Piscifactorías → [map-loading.md](map-loading.md#piscifactorías-create_fish_farm--create_town_fish_farm).
- Barco de los misioneros (`PetitNavire`) → [water.md](water.md#barco-de-los-misioneros-petitnavire).
- Porcentaje de construcción de un Feature →
  [map-loading.md](map-loading.md#porcentaje-de-construcción-de-un-feature-built_percentage-propiedad-chl-22).
- Puzle de los peces: el lado del guion → [water.md](water.md#puzle-de-los-peces).
- Objetos del guion del mapa →
  [map-loading.md](map-loading.md#objetos-del-guion-del-mapa-farolas-hogueras-árboles-muertos-puertas).

## Pendiente

- Coger por tandas: las partículas del pescado (`S_Spangle_A`).
- `PileFood::Draw`: confirmar que la vfunc 0xE8 de LH3DObject es el desplazamiento de UV (**inferido**).
- Campos: la alineación y la lluvia en el crecimiento; los oficios de aldeano que los siembran (sin ellos los campos
  quedan vacíos salvo con el mod `world.crops`).
- Sonidos: desde B4 del audio todos (coger, arrancar, plantar, triturar, rocas, silbidos, montones) van por los 16
  canales como en el original, 2D o 3D según su sitio ([audio.md](audio.md#b4-los-llamadores-del-mundo-en-los-canales)).

## Ganchos de prueba

- `OPENBLACK_HAND_TEST_FIELD=1`: coger de un campo.
- `OPENBLACK_HAND_TEST_FISH=1`: coger de una piscifactoría; traza el bucle de coger (un arranque, tono 0,60 → 1,02).
- `OPENBLACK_AUDIO_TRACE=1`: canales de `sample_play` (mano en el agua, choques, ambiente).
- `OPENBLACK_ATMOS_TRACE=<n>`: el ambiente (mapa de sonido y bancos).
- Árboles y carga del mapa: los ganchos de [trees.md](trees.md#ganchos-de-prueba) y
  [map-loading.md](map-loading.md#ganchos-de-prueba).

## Fuentes

- `C:\Users\diewgarc\dev\tmp_dis\field\field_notes.txt` y `draw_colour_sway_notes.txt`: campos.
- `C:\Users\diewgarc\dev\tmp_dis\sound\notes.txt`: sonidos de coger y de LHAudio.
- `C:\Users\diewgarc\dev\tmp_dis\agua\re\emu_qmixer.py` y `emu_polar.py`: volumen, distancias y ejes de QMixer (Unicorn).
- `C:\Users\diewgarc\dev\tmp_dis\agua\audio.md` §1-3: el ambiente.
- `C:\Users\diewgarc\dev\decomp_pickup`: coger, sostener y soltar (interfaz, CHand, objetos).
