# Partículas (PSys)

El motor de partículas del original tal como lo usan los milagros: los tipos de partícula, el registro de clases, el
PSys enlazado a un hechizo, las jerarquías, los creadores (mallas, cadenas, mapas de luz, niebla), el sonido de las
partículas y un índice de las reglas con la página donde está cada una. El dibujo de las partículas del mundo (y lo
que queda del PSys en el render) está en [El PSys en el mundo](#el-psys-en-el-mundo-formato-paso-dibujo-y-reglas-del-agua); los milagros, en
[miracles.md](miracles.md); el núcleo de la magia, en [magic.md](magic.md).

- [Tipos de partícula](#tipos-de-partícula-m0-srcpsysparticletypes)
- [Registro de clases de PSys](#registro-de-clases-de-psys-m0-srcpsyspsysregistry)
- [PSys enlazado al hechizo](#psys-enlazado-al-hechizo-psysspelllinkh)
- [Corrección en el núcleo del PSys: las jerarquías](#corrección-en-el-núcleo-del-psys-las-jerarquías)
- [Creadores](#creadores)
- [Sonido de las partículas](#sonido-de-las-partículas-lane-s-srcaudiospellsounds-srcpsysrulessoundcpp)
- [El PSys en el mundo: formato, paso, dibujo y reglas del agua](#el-psys-en-el-mundo-formato-paso-dibujo-y-reglas-del-agua)
- [Índice de reglas](#índice-de-reglas)
- [Pendiente](#pendiente)
- [Ganchos de prueba](#ganchos-de-prueba)
- [Fuentes](#fuentes)

## Tipos de partícula (M0, `src/PSys/ParticleTypes`)

`fn_0068EA00` rellena una vez la tabla de 150 nombres `0xD4EBB0` con `.\Data\Spells\ZSpellFiles\<nombre>.txt`; 121
tienen archivo y el resto queda a NULL (NONE, TORNADO, FIREWORK, FOOD_IN_HAND...). Hay nombres guardados a través de un
registro (`mov eax, str; mov [ecx+off], eax`), que la tabla antigua `psys\pt_table.md` perdía: FOOD y FOOD_POISONED →
SF_Food, HEAL y HEAL_FX → SF_HealChakra, LANDSCAPE_VORTEX_OUT_BEFORE → SF_LandscapeVortexInBefore (sic). Reconstruida
con `tmp_dis\miracles\ptnames.py`.

## Registro de clases de PSys (M0, `src/PSys/PSysRegistry`)

Nombre de clase → fábrica para los modificadores (reglas, emisores) y los creadores. `PSys.cpp` registra las clases que
ya había (`RegisterCoreModifiers`); cada archivo nuevo de reglas o creadores (`src/PSys/Rules/`, `src/PSys/Creators/`)
tendrá su `RegisterXxx()`, llamado desde la lista explícita `RegisterAll()` de `PSysRegistry.cpp`. Lo no registrado
sigue como "not ported yet" (sin efecto). Los creadores registrados derivan de `Creator` y llaman antes a
`ReadCreatorProperties`.

## PSys enlazado al hechizo (`PSys/SpellLink.h`)

- El hechizo es dueño de su efecto y lo avanza él mismo con su PSysProcessInfo (`manager::StartForSpell` /
  `ProcessForSpell`); `ProcessTurn` no lo toca.
- `StrengthFloatProvider` = `info.power`; `EventConditionTrueWhenEnabled` = `info.enabled`.
- `LandscapeCollide` con `SendEvent` manda el evento 3 (posición global, movimiento del paso, fuerza 1) antes de borrar
  el átomo. El evento 1 se manda al empezar (fn_00673070).
- (inf) Mientras una clase de PSys no está portada, el efecto de un hechizo la cuenta como creadora hasta que se cierra.
  Si no, un efecto sin clases portadas acabaría en el primer turno y cortaría el ciclo del hechizo.

## Corrección en el núcleo del PSys: las jerarquías

- En el original (ctor de `AtomCollection` 0x6748B0, `LocalToGlobal` 0x6751D0 / fn_006752D0, fn_00673DB0,
  `CommonInitNewAtom` 0x674C60): una colección está en jerarquía si **cualquier** antepasado es de un grupo marcado en
  `Hierarchies` (no solo el padre); su marco es el producto de las matrices locales de **los átomos antepasados cuyo
  grupo está marcado** (los demás no cuentan), y esa matriz lleva la **escala** del átomo (baseScale × ruleScale, la Y
  × el stretch). Un átomo nuevo nace en 0 si su padre es de grupo marcado y si no en la posición del padre.
- Portado en `Effect::CreateCollection`, `LocalToGlobal` / `GlobalToLocal` (nuevos), `GlobalPosition`,
  `SpawnPosition` y `PostUpdate` (el marco y su escala bajan por los átomos no marcados; la escala dibujada de un átomo
  en jerarquía también se multiplica). La cúpula lo necesita: los parches (grupo 2) y las chispas (grupo 4) están en el
  marco de la raíz, que crece de 0,1 a 1 en 2 s (`UR_ChangeScale`) y encoge al cerrarse. Cambia también, hacia el
  original, SF_Bonfire, SF_Smoke/Steam (raíz 0,8) y los efectos en la mano y sobre el pedestal que escalan su raíz
  (`SetScale`). Ninguna regla con `GlobalToLocal` propio (HandFollow, Fireball) está en una colección afectada.

## Creadores

Los creadores registrados derivan de `Creator` ([Registro de clases de PSys](particles.md#registro-de-clases-de-psys-m0-srcpsyspsysregistry)). Los de los milagros:

### Las mallas de partículas (`Creators/Mesh.cpp`, Particle3DObj::DrawAt 0x679FD0) y la cúpula del escudo

- **Fiel:** el ctor de `ParticleMeshCreator` 0x6A8960 pone `MeshChangeMaterialProps` = 1 y doble cara = 1 (antes se
  tomaba 0): así **`UseAdditiveAlpha` vale para la cúpula** (SF_DefenseSphere no da MeshChangeMaterialProps) y para el
  rayo. Con él, `GJUtils::SetMaterialProperties` 0x57E120 da el modo 13 (SRCALPHA / ONE, sin escribir Z): el paso de
  transparentes lo dibuja ahora así (`RenderContext::additiveInstances`, `Renderer.cpp`).
- **Fiel:** `UsePlayerColor` / `UsePlayerColorBlend` (fn_006A85E0, para **todos** los creadores, sprites incluidos: no
  se aplicaba en ninguno): pc = `GetPlayerColour` del jugador del efecto con alfa 0xFF (el negro del neutral pasa a
  blanco); con mezcla b = ftol(Blend × 255) & 0xFF ≠ 255, cada canal 255 + ((c − 255) b >> 8); el color del átomo × pc
  >> 8 por canal. La cúpula usa 0,5 (con el rojo del jugador 1: (254, 162, 162)). `psys::TintWithPlayerColour`.
- **Fiel:** `FaceCamera` (0x67A032): θ = atan2(d.z, d.x) − atan2(r2.z, r2.x) con d = posición − cámara en x/z y r2 la
  fila Z; r0' = cos θ r0 + sin θ r2, r2' = cos θ r2 − sin θ r0; la fila Y × `HeightStretch` (ctor 1).
  `FaceCameraSprite` (0x67A250) no lo usa ningún archivo; está portado (`billboard::FullSprite`) por completitud. Los
  dos están en [rendering-objects.md](rendering-objects.md#objetos-que-miran-a-la-cámara-billboards).
- Con esto **la cúpula del escudo ya se ve** (nota de la revisión 3a): las 32 placas
  `MSH_S_SPELLBALLSURFACE02` se dibujan en modo aditivo con el color del jugador y forman una burbuja clara
  (`m6b_dome4_t30.png`), en vez del parche apenas visible de antes.
- **`DrawCutByPlane` no recorta nada en la cúpula:** solo cambia la llamada (fn_00679F20: vt 0x11C en vez de vt 0x104),
  y la malla de una partícula es un `LH3DStaticObject` (`LH3DObject::Create(0)` 0x80B4F8, vtable 0x9A2974) cuyo vt 0x11C
  es fn_0080C050, un dibujo directo de sus primitivas. El corte por y = 0 es solo de los objetos animados (fn_00811C70,
  [rendering-objects.md](rendering-objects.md#cortar-por-el-plano-del-agua-drawcutbyplane)). La nota de la revisión 3a («sin recorte del suelo») queda resuelta: no hay nada que
  portar.
- **(aproximado)** el color va por el tinte de objeto de `vs_object` (−1 − r·65536 − g·256 − b en la x de la quinta
  columna, `lh3d_colour::PackInstanceTint`; sin `DrawWithLandscapeColor`, 1 + rgb con `PackInstanceColour`), que multiplica la luz del suelo: es lo que hace `DrawWithLandscapeColor` (fn_0080BEC0); sin esa marca el
  original pone solo el color (`SetColour` vt 0x2C → obj +0x4C / +0x50). Sin portar: `UseScriptHightlightPulse`
  (fn_0070A510), `CastHumanShadow` (lista 0xD4EDCC), `UseDynamicLighting` (bit 0x20), `UseGlobalAlpha` y el orden Z por
  objeto.
- **(aproximado)** `ParticleAnimCreator` (mariposas y murciélagos del bosque) se dibuja como malla quieta en su postura
  de reposo; falta el .anm (Particle3DAnim::DrawAt 0x67A8E0: `GetCycleTimeFromFrame` 0x6C85F0, `SpeedUpFactor`, la
  mezcla con MeshFileName1/2 entre FrameToStartBlend y FrameToEndBlend).

### Cadenas y mapas de luz (`PSys/Creators/{Chain,LightMap}.cpp`, `Graphics/RendererChain.cpp`)

- **`ParticleChainCreator`** 0x6AA900: los átomos de una colección son las articulaciones de **una** cinta. El dibujo
  por colección (fn_0067B3F0) la orienta a la cámara: en cada articulación el vector lateral es
  `normalize(cross(cámara − articulación, dirección del tramo)) · escala` y los vértices son `articulación ± lado`
  (la escala entera del PSR es la semianchura, 0x67B9E6..0x67BB0D; un lado exactamente nulo, p. ej. un tramo de
  longitud 0, se queda nulo, 0x67BA04..0x67BA39). Donde se juntan dos tramos, los vértices se mueven a su punto medio
  (0x67BD2D..0x67BE82). Índices por tramo (0, 1, 2) y (1, 3, 2) (0x67B77A..0x67B7D8). UV (fn_006C8920, con uv0 en
  cabeza + lado): la **U va a lo ancho**, `[(cuadro+FileOffset)·FrameWidth, +FrameWidth]/256`, con cuadro
  `FrameOfHead` en la última repetición, `FrameOfTail` en la primera y 0 en las demás. La **V va a lo largo**,
  `FrameHeight/256` por repetición; hay `NumTexturesForWholeChain` repeticiones, −1 = una por tramo. Detalle en
  [rendering-objects.md](rendering-objects.md#texturas-animadas-por-fotogramas).
  - Valores por defecto del creador: 64 de alto, 32 de ancho, −1 (ctor 0x6AA739..0x6AA747).
  - Así el rayo usa la tira 0 de `S_Lightning.raw` (núcleo blanco y halo cian), 4 veces.
  - Antes el port tenía 256×256 y los ejes cambiados, y la cinta salía casi transparente.

  Para esto el `Effect` tiene un `CollectChains` nuevo que devuelve las colecciones de tipo cadena con sus
  articulaciones **en orden** (el `Collect` normal las aplana). Se dibuja después de los sprites ordenados, en
  `MainBlended`. Una colección sin el bit 2 de +0x38 (las horquillas del rayo) se dibuja sin interpolar
  (fn_00679920 0x67999E); el fotograma sí se interpola siempre (0x679A79).
  - El desplazamiento vertical de la UV con el tiempo (chain +0x3C, `frame_anim::ChainScroll`, 0x67BE91, `fmod` por
    `FrameHeight/256`) está portado, pero su ritmo +0x4C solo lo ponen UR_SimpleBeam y UR_Plasma, sin portar: vale 0
    en todas las cadenas de openblack.
  - No portado: `UseDynamicLighting` (color × `clamp(0,6 + 0,4·(n·L))`).
  - `OPENBLACK_PSYS_CHAIN_TRACE=1` escribe por fotograma cuántas cintas hay, con cuántas articulaciones, su textura y de
    dónde a dónde van: sirve para separar «no se dibuja» de «no hay ninguna en ese fotograma».
- **`ParticleLightMapCreator`** 0x6A9D80: `GJBitmap::LoadBitmapFromFile(nombre, Pitch, 3, NumFramesInFile,
  NumFramesInUse)` son fotogramas cuadrados de `Pitch × Pitch` apilados, RGB (3 B/px) o grises (1 B/px) — el del rayo,
  `S_lightning_lightmap_with_border.raw`, son 1200 B = 5·5·16·3. El original los mete en la lista 0xD4EDB8 y
  `PSysLightMaps::AddDrawing` 0x6CA6E0 los **estampa** (fn_0086CFF0) en la textura de luz dinámica del terreno.
  - **Desviación (no es un mod, es una aproximación de dibujado):** el port no tiene esa textura dinámica, así que cada
    fotograma se escala a 32×32 en un atlas de 8×8 y el átomo se dibuja como un cuadrado aditivo plano **sobre el
    suelo** (`SetHorozontal`), con la altura del terreno bajo la punta. La luz no sigue la pendiente ni tiñe los objetos
    que están encima. `ShiftX`/`ShiftZ` (≈10, que compensan el +10 del estampado) no se usan.
- Arreglo de paso: `TextureBaseName` resuelve el nombre del `TextureFileName` con **las mayúsculas que el fichero tiene
  en `Data\Textures`** (los ficheros de hechizo escriben `S_Lightning.raw` y el fichero es `S_lightning.raw`), así que
  ahora encuentran su textura también los sprites que antes no se dibujaban.

### `ParticleMistCreator` (`PSys/Creators/Mist.cpp`)

- Constructor 0x6AA380: RandomiseScale 0, IsShadowMap 1, LoadLightMap 1, TakeRatioFromMatrix 0, Pitch 12, 1 cuadro,
  InitialScaleMin 1, Ratio 0. Propiedades 0x6B3C00 (Pitch 1..12, cuadros 1..32, InitialScaleMin y Ratio 0..5).
- `CreateParticleMist` 0x6AA610: escala del átomo = `RandomiseScale ? PSysFloatRand(InitialScaleMin, InitialScale) :
  InitialScale`. `CreateLH3DMist` 0x6AA5A0: un `LH3DObject` de tipo 7 (la cúpula `mist.l3d`), `k = Ratio` o
  `2,5 + LocalFloatRand(2,5)` si es 0, y `+0x80 |= 2` (la rama de efecto).
- `RenderParticleMist::DrawAt` 0x67A670: tamaño = escala del PSR, con TakeRatioFromMatrix `k = M[1][1]/M[0][0]`, color
  = el del átomo × `[0xFA26A4]` (la base de la tabla de luz del terreno, con alfa forzado a 0xFF): por canal
  `(c × g) >> 8`. Aquí cada fotograma `mist_atoms::SubmitFrame` (desde `magic::Update`) pasa cada átomo a
  `mists::Submit` de «mapa» (el mismo `DrawMist`). La base la lee
  `LandLightTable::Current().GetRawBase()` (la copia global de la última tabla, de la lane del agua; antes
  `LastBuiltBase` de la lane de la tormenta).
- Cada niebla del PSys lleva su contador de atlas (`Atom::mist`), como el original lleva uno por objeto. Empieza en
  `Random(0,16) & 15` (0x7F95F8; **(aproximado)** con `graphics::lh3d::Random`, un `rand()` de MSVC propio que comparte con las nieblas del mapa y las bocanadas de tormenta y no toca la serie del PSys) y solo
  avanza si la niebla sale en pantalla (`mists::InView`). **(aproximado)** con Ratio 0 se usa la media 3,75 en vez
  del azar por niebla. El mapa de sombra /
  luz del terreno de una niebla con `TextureFileName` (la tormenta) no está portado.
- SF_Water: la nube (183, 181, 255, 200), escala 0,2 (0,4 en PU), Ratio 2, y en su grupo 4 el cono de lluvia
  `MSH_S_RAIN_CONE` (`ParticleMeshCreatorAnimTextured`, de 0,1 a −24 m, escala 0,7 / 1,4, UV que se desliza).
- **Arreglo en `PSys.cpp` (`MakeCreator`)**: los creadores registrados se buscan antes de exigir que el nombre empiece
  por «Particle» y acabe en «Creator»; `ParticleMeshCreatorAnimTextured` acaba en «Textured», así que el cono de lluvia
  (y cualquier otro AnimTextured) no se creaba nunca.

## Sonido de las partículas (lane S, `src/Audio/SpellSounds`, `src/PSys/Rules/Sound.cpp`)

Los bucles y golpes de los milagros (piscina del teletransporte, tornado, escudo, rayos, bolas de fuego...) los suenan
los átomos del PSys. Informe: `visuals_sound.md` §3; lo de abajo está verificado en el exe y en `LHaudiodllR.dll`
(`tmp_dis\sound\dlldis.py`).

- **Propiedad `SOUND_ACTION`** (`SoundActionProperty::ReadProperty` 0x585A70, `src/PSys/SoundAction`):
  `<SOUND_*|NO_SOUND> LOOPING b ONLYONE b SOFTRELEASE b USESURFACE b`. El nombre se busca en `Data\SoundAction.h`, que
  el juego lee al vuelo (fn_00585590, `LHParseFile::FindEnumVal` 0x7BE530); desconocido o NO_SOUND → -1. Guarda
  LOOPING (bit 0), SOFTRELEASE (bit 2) y USESURFACE (bit 3); ONLYONE se lee y se tira. El código pone 0x22 (bit 1
  "retraso por distancia", bit 5 "altura del suelo") en los truenos.
- **PSysSoundAction** (0x18): acción, luego las ranuras que se pasan al banco (superficie +4, tamaño +8, alineamiento
  +0xC), FadeStep +0x10 y las marcas +0x14. Valores por defecto (ctor en línea, p. ej. CreateRuleAnAtom 0x69F350):
  superficie 1, **tamaño 2**, alineamiento 2, FadeStep 0. Corrección al informe: sin SoundRadiusFP el tamaño es 2, no 0
  (el resultado es el mismo en las filas "*").
- **`AtomCore::StartSound` 0x6745D0**: posición global del átomo (con 0x20, altura del terreno); con USESURFACE,
  `GSoundMap::GetSurfaceType`; alineamiento del jugador dueño (`GetDiscreteAlignmentValue` 0..6 → 1,1,2,2,2,3,3; aún
  sin enlace al jugador: se queda la ranura, ninguna fila de spells.sad lo mira). Crea un `PSysSound` (0x40, ctor
  fn_006D0F70) al principio de la lista del átomo (+0x2C) y en la global 0xD4EE70. Con el bit 1 no suena: +0x34 =
  distancia a la cámara / **347**. Si no, `GAudio::SamplePlayAnimEffect` 0x42A4B0 (modo 0) con los atributos
  {tamaño, alineamiento, 1, superficie, acción} sobre spells.sad.
- **`GSoundMap::GetSurfaceType` 0x71D8E0** (`src/Audio/SoundMap`): 6 fuera de la cuadrícula de 512 celdas o sin bloque;
  7 si la celda no es tierra (`MapCoords::IsLand` 0x603720: bit 0x10 de las propiedades de la celda, `hasWater`); si
  no, `surfaceSound` del material (1..8, otro → 3). Los sonidos de animación usan ahora esta misma función (antes
  "altura ≤ 0 → 7").
- **Banco** (`LHSamplePlayAnimEffect` 0x100146F0 del DLL): `LHFindAttribRow` (fila más exacta, `src/Audio/AnimEffectBank`,
  alias de `AnimEffectTable` del núcleo desde B2 del audio: sus tablas se copian de las del banco registrado,
  compartido con los sonidos de animación), una muestra al azar de la lista y **no suena si la cámara está más lejos
  que el maxDist de la muestra** (S_TeleportPool: 170). El modo de reproducción de la muestra (+0x274, bit 0x400) 2 =
  no hace nada si ya suena para ese objeto; así el bucle se puede "re-emitir" cada turno sin duplicarse. El argumento de
  modo: **0 tocar, 1 `LHSampleStop`, 2 `LHSampleReleaseLoop`** (resuelve la duda de `visuals_sound.md` §7.3).
  `GAudio::SamplePlayAnimEffect` además filtra por modos de ayuda/cine y el interior de la ciudadela: no portado.
- **Vida del PSysSound** (fn_006D11A0, desde `PSysGlobal::GameLoopEnd` 0x68F5B0 una vez por turno, con la duración
  del turno):
  - átomo vivo, LOOPING: si la cámara está a menos de **1200**, se re-emite (modo 0).
  - átomo vivo, retrasado: si +0x34 > 0 se le resta el turno; al pasar de 0 (estrictamente negativo) suena. Un retraso
    que caiga justo en 0 no suena nunca (se conserva).
  - átomo muerto (StopSound 0x674500 pone +0x38 = 0; el destructor del átomo hace lo mismo): si aún suena, con FadeStep
    baja el volumen `vol − FadeStep` (0..127, no menos de 0) cada turno, y una vez (+0x3C) manda el modo 2 con
    SOFTRELEASE (el bucle acaba su vuelta) o 1 sin él (corte). Cuando ya no suena, se borra.
  - `PSysSound::Get3DSoundPos` 0x6D1000: la posición dibujada del átomo (+0xF4), con 0x20 a ras de suelo.
- **Reglas** (`Rules/Sound.cpp`): `StartStopSoundOnCondition` 0x69DC40 (SoundCondition cierta o ninguna → suena si el
  átomo no tiene ya esa acción; falsa → la para con FadeStep), `AddSoundToAtom` 0x69DCA0 (una vez por átomo al llegar a
  Delay y con la condición; StopOtherSoundsFirst; el temblor de cámara `LH3DCameraChecker::Create` no está portado),
  `RemoveSoundFromAtom` 0x69DDD0 (una vez: para esa acción con FadeStep).
- **Quién suena al crear**: CreateRuleAnAtom (SoundOfCreate; con SoundRadiusFP: < Small 200 → 3, < Medium 500 → 2, si
  no 1), CreateRuleSphere (solo el primer átomo), EmitterRuleConical y UR_WillowWisp (SoundEmission en cada átomo).
  Para las lanes de bola de fuego: `SizeFromThrow` (> 0.6 → 1, > 0.3 → 2, si no 3) y `SizeFromImpactSpeed` de
  UpdateRuleGravityWithFloor fn_006A1630 (< Medium → 3, < Large → 2, si no 1; antes exige alfa ≥ MinAlpha, |v| ≥ Small
  y que el átomo no tenga ya esa acción).
- En openblack: `audio::spell_sounds::ProcessTurn` va en la ranura 11, dentro de `magic::ProcessTurnEnd`
  (`MagicLoop.cpp`), que `Game.cpp` llama justo después de `psys::manager::ProcessTurn` (la ranura 9, al final del
  bloque de scripts). Así el sonido ve los átomos de este turno, como en el original. El único cambio de orden que
  queda es que `GScript::Process` va antes de la ranura 9 y no entre la 11 y la 12. El cargador de bancos de
  `Game.cpp` dejaba de leer un
  .sad en la primera muestra vacía: spells.sad tiene una vacía en la 31, así que faltaban la 32..88 (piscina del
  teletransporte, rayos, fuegos artificiales...). Ahora la salta y sigue.
- Prueba: `OPENBLACK_PSYS_SOUND_TRACE=1 OPENBLACK_TEST_PSYS="SF_TeleportVortex,1478,2129,0,1,5"` en Land1 (cámara
  inicial a 157 del punto, dentro de los 170 de S_TeleportPool): "start SOUND_SPELL_TELEPORT_POOL (76) ... ->
  spells.sad/39 (S_TeleportPool.wav) looping", a los 5 s "release loop" y medio segundo después "deleted" (acaba su
  vuelta). `test_spell_sounds` comprueba el enum, las marcas, los tamaños y, con `OPENBLACK_GAME_PATH`, las filas
  reales de spells.sad.
- Sin portar: el alineamiento del dueño, el temblor de cámara, los filtros de estado de juego, las repeticiones finitas
  (loop > 0 se toca una vez) y el tope global de distancia del DLL (+0x44 del sistema de audio).

## El PSys en el mundo: formato, paso, dibujo y reglas del agua

Viene de rendering.md (la parte que el render tenía del PSys). **Fiel** salvo lo que se dice sin portar.

Informe completo (formato, 136 clases, fórmulas, tiempo de ejecución, dibujo, tablas de efectos):
`tmp_dis\psys\psys_report.md`; los 132 archivos descomprimidos en `tmp_dis\psys\zzz\`.
- Archivos: `Data\Spells\ZSpellFiles\SF_X_txt.zzz` (u32 tamaño + zlib) con texto del editor: cabecera
  `BEGINPROPERTIES` (DeleteOnCloseDown, Hierarchies[25], InitiallyCreated[25], MaxSpellAge) y bloques
  `BEGINCLASS <Clase> <Nombre>`. Un `.txt` suelto con el mismo nombre tiene prioridad (`LHLoadData`): sirve para mods.
- Modelo: cada modificador tiene `Group` (0..24) y `Condition`; una *colección* es una instancia viva de un grupo;
  `InitiallyCreated` crea las raíces en el origen; `NextGroups` da a cada átomo nuevo sus subcolecciones; `Hierarchies`
  pone los átomos hijos en el marco local del padre. Nada se mueve solo: solo las reglas.
- Paso por turno (dt = 0,1 s) con el estado de dibujo anterior y actual, interpolado al dibujar con la fracción del turno.
  El fotograma del átomo lo guarda fn_00673EA0 en [0, 2N) junto con el anterior, y solo lo mueve con PlayAnim
  (`Atom::playAnim`, +0x118; sin él ni paso ni vuelta); fn_00679920 interpola el **número**
  de fotograma y lo trunca (un escalón, sin mezclar dos fotogramas): `frame_anim::PSysFrameAdvance` /
  `PSysFrameLerp` / `PSysFrameIndex`, ver [rendering-objects.md](rendering-objects.md#texturas-animadas-por-fotogramas).
  Fin: sin átomos ni reglas de creación, o edad > MaxSpellAge; `CloseDown` activa `TrueOnCloseDown`, suelta las
  reglas `RemoveOnCloseDown` y borra al momento si `DeleteOnCloseDown`.
- Dibujo: cada efecto es un objeto del Z-sorter (`PSysManager::AddDrawing`), sus átomos en orden de lista; sprites de
  `S_SpriteSheet{1,2,3}` (8×8 celdas de 32 px, celda = (FileOffset + fotograma) & 63), quad orientado a la pantalla
  con giro atan2(M[0][2], M[0][0]) o plano XZ (`SetHorozontal`); modo 13 aditivo (102 de 137) o 6, sin escribir Z
  salvo `MaterialUpdateZBuffer`; sin luz ni neblina salvo `UseLandscapeColor` (no hecho).
- Guiones: `SPECIAL_EFFECT_POSITION` / `_OBJECT` (CHL 52/53) → `GParticleContainer` con la tabla `GSpotVisualInfo`
  (50 entradas → PARTICLE_TYPE → archivo); duración en segundos (−1 siempre, 0 la vida de la tabla); sigue al objeto
  y se cierra si desaparece; devuelve un objeto que el guion puede borrar.
- openblack: `src/PSys/PSysFile` (lector), `PSys` (colecciones, átomos, reglas: CreateRuleAnAtom/Sphere, emisores
  Simple/Disk/Conical, UR_WillowWisp, reglas de borrado, AR_FadeAlpha/FadeCollectionAlpha/FadeOutOnceConditionTrue,
  UR_ChangeScale, SetScale, SetAtomAlpha, UpdateRuleGravity, UR_UpdatePosnFromVelocity, UR_GustyWind (con un ruido
  propio: VLNoise3To1 sin portar), UpdateRuleRotatePrincipalAxis, FollowOrigin, UR_FollowParent, ForceConstant*,
  UR_SphereSurfaceTracer, UR_OrientSpriteWithRandomAngle; condiciones y proveedores de float), `PSysManager`
  (efectos, contenedores de guion, gancho de prueba) y `Graphics/RendererPSys.cpp`. Las clases sin portar se registran
  una vez en el log ("not ported yet") y no hacen nada.
- **`UpdateRuleGravityWithFloor`** (`PSys/Rules/Fireball.cpp`, una sola clase con la de los milagros; ctor 0x6A1510, `ModifyAtomCollection` 0x6A1880).
  Valores por defecto del ctor: MaxSpeed 100, Gravity 10, Damping 0, WindMagnification 100, UseWind 1, rebotes 0,5/0,5,
  GroundDrag 0, ImpactSpeed 5/20/40, MinAlphaForImpactSoundOrRipple 60, distancia de onda 2 (+0x40) y onda activada
  (+0x71 = 1), sin propiedad. Por átomo, sin la `Condition` por átomo:
  - amortiguar = UseDamping y (no DisableDampingForNonHuman o `IsHumanPlayerCasting` 0x673580, que es 0 sin `Spell`);
    viento igual con UseWind / DisableWindForNonHuman. Con viento: v += (viento·WindMagnification·0,1 − v)·Damping·dt
    (viento = `fn_00771B10` = `GClimate::GetWeather(p, 1)`: (int8 x/8, 0, int8 z/8); aquí `weather::GetWindAt(p, true)` de ECS/Weather); si no,
    con amortiguar: v ·= 1 − dt·Damping.
  - Se guarda v y el átomo se mueve **antes** de la gravedad. Suelo = `GetAltitude(x, z)`; punto más bajo = y global
    (`RenderParticle::GetLowestPoint` 0x6C79B0; el de malla, `Particle3DObj` 0x6C7AE0, no está porque no hay
    partículas de malla). Por encima: v.y −= clamp(v.y + MaxSpeed, 0, 1)·Gravity·gravedad del átomo·dt.
  - Por debajo: se sube al suelo; d = n·v con la normal del terreno; si d < 0, golpe (`fn_006A1630`) y rebote:
    vn = n·d, vt = v − vn, arrastre m = min(dt·GroundDrag, |vt|) en la dirección de vt (si |vt|² < 1e-4 la dirección
    es +x), v = vt·DampingHorozontalBounce·superficie − vn·DampingVerticalBounce. Superficie (UseSurfaceForBounce):
    tabla 0x937574 por `GetSurfaceType` = 1,1,1,1,1,1,**0,2** (agua profunda),**0,2** (somera),1,1,25.
  - Golpe `fn_006A1630`: nada si alfa < MinAlpha, si ImpactSound es NO_SOUND (−1), si |d| < ImpactSpeedSmall, si el
    sonido del átomo aún suena o si ImpactSoundCondition falla; nivel 3/2/1 según ImpactSpeedMedium/Large; y onda en el
    agua. Consecuencia: los trozos de `SF_ExplodeObject` (NO_SOUND) **nunca** hacen onda; solo la bola de fuego
    (`SF_FireBallThrow*`, SOUND_SPELL_FIREBALL_HIT) la hace. Los átomos de este motor aún no tocan sonidos
    (`AtomCore::StartSound` 0x6745D0), así que la espera "mientras suena" no se aplica. `CheckShieldDeflections`
    (escudos de criatura) no está. Prueba unitaria `test_psys_water`.
- **`UR_Explosion`** (`PSys/Rules/Explosion.cpp` de Milagros, [magic.md](magic.md); los anillos de aquí son
  `PSys/PSysWaterRings` `AddExplosionRings`, la única implementación; ctor 0x67E090, `ModifyAtomCollection` 0x67ECE0, `InitCollection`
  0x67E200), en `SF_BeamExplosionSingle/Many/Loads`. Por defecto InitialDelay 3,5, SmokeDelay 3, BeamDelay 0. Punto =
  `GetCurrentParentPos` (el +0x80 del átomo padre o el origen) con y = altitud del suelo. Si el efecto se cierra, cierra
  el contenedor del rayo. Con edad de colección > InitialDelay: anillos de agua (arriba) o chamuscado; > BeamDelay:
  punto visual BEAM_EXPLOSION_FX (magnitud 1, 60 turnos); > SmokeDelay: **SMOKE en tierra seca, STEAM sobre el agua**
  (`IsDryLand`), magnitud 8, 4 s. El 3.er argumento de `CreateSpotVisualWithSpecifiedDuration` es la magnitud del
  efecto (`GJPSysInterface::Create` 0x68F3A1 `SetScale`). El daño a los objetos, el `SpellEvent` 2 y el escudo los
  hace la de Milagros; la marca del suelo (`fn_008251C0`: una malla morfable 0x251 de escala 8, no una sombra; `TemporaryShadow` es
  `fn_00825090`) está en `ecs/GroundMarks` ([rendering-objects.md](rendering-objects.md#mallas-pegadas-al-suelo-land_morph));
  sin portar: los escombros de malla.
  Prueba: `OPENBLACK_TEST_PSYS="SF_BeamExplosionSingle,1464,2016,0,1"`, cámara `1452,14,2002,1464,0,2016`, captura en
  el fotograma 272 de 300 (anillos) o 360 de 400 (vapor); `OPENBLACK_PSYS_TRACE=1` escribe la explosión.
- Prueba: `OPENBLACK_TEST_PSYS="SF_Bonfire,1790,2630,0,1"` con la cámara `1775,45,2600,1790,30,2630`, `-n 5000`
  (hoguera con llamas y humo); `OPENBLACK_PSYS_TRACE=1` escribe átomos y edad de cada efecto cada 20 turnos.
- **Creencias sobre el centro del pueblo** (`src/PSys/TownBelief.cpp`; informe `tmp_dis\psys\towncentre_notes.md`): cada centro funcional tiene TOWN_BELIEF (SF_TownBelief, `UR_TownCentreBelief` 0x69BF30), que avanza una vez por fotograma con dt = 0,1 s. Un símbolo por jugador con creencia: el primero (rango 0) quieto 2 unidades sobre la cima del tótem; los demás giran (radio y velocidad por la creencia, a 2,5 por rango de altura) y el segundo pelea (destellos). Se dibuja con dos brillos de S_SpriteSheet3 (color del jugador y blanco girando) y el símbolo. El símbolo del humano es la celda del "player symbol" del perfil (registro; 0 sin él, como en esta instalación) copiada de ChooseSymbol (PlayerSymbol::OpenOnce 0x5DE2F0); los rivales usan imágenes .cps (no hecho). Base: el tótem (`components::TotemStatue` de campos): x/z del pedestal, y = baseY + alto de la malla del icono × escala + 2. Falta la columna SpellColumn del dueño.

## Índice de reglas

Cada clase del PSys que usan los milagros, con su dirección y dónde se describe. Las de `src/PSys/Rules/` se
registran en `PSysRegistry.cpp`; las que no, siguen como «not ported yet».

| Clase | Dirección | Archivo | Dónde |
|---|---|---|---|
| `CreateRuleAnAtom`, `CreateRuleSphere`, `EmitterRuleConical` | ctor 0x69F350 (CreateRuleAnAtom) | — | [Sonido de las partículas](particles.md#sonido-de-las-partículas-lane-s-srcaudiospellsounds-srcpsysrulessoundcpp) |
| `StartStopSoundOnCondition`, `AddSoundToAtom`, `RemoveSoundFromAtom` | 0x69DC40, 0x69DCA0, 0x69DDD0 | `Rules/Sound.cpp` | [Sonido de las partículas](particles.md#sonido-de-las-partículas-lane-s-srcaudiospellsounds-srcpsysrulessoundcpp) |
| `StrengthFloatProvider`, `EventConditionTrueWhenEnabled`, `LandscapeCollide` (SendEvent) | — | `SpellLink.h` | [PSys enlazado al hechizo](particles.md#psys-enlazado-al-hechizo-psysspelllinkh) |
| `MagnitudeFloatProvider` | UpdateParams 0x69DA90 | — | [El tamaño de la bola de fuego lanzada con la mano](magic.md#el-tamaño-de-la-bola-de-fuego-lanzada-con-la-mano-inferido-recuerdo-del-usuario) |
| `UR_FollowLocalHand`, `UR_FollowCastPosn` | 0x69A6A0, 0x69FE30 | `Rules/HandFollow.cpp` | [La mano](magic.md#la-mano-handmagicfxcpp-phandfx-y-el-efecto-en-la-mano) |
| `ZR_ChainGesture`, `CreateRuleMakeChain` | 0x68A080, 0x69FD10 | `Rules/Gesture.cpp` | [Efectos de utilidad](magic.md#efectos-de-utilidad-psysutilitycpp-psysutilitypsys-0xd4e0e8) |
| `UR_GesturingRecognised` | 0x6884F0 / 0x688910 | — | [Efectos de utilidad](magic.md#efectos-de-utilidad-psysutilitycpp-psysutilitypsys-0xd4e0e8) |
| `UR_HandSprinkle`, `UR_WillowWisp`, `AppearanceRuleTumble` | 0x6A0220, —, 0x6A6200 | `Rules/Sprinkle.cpp` (UR_HandSprinkle) | [Comida y madera](miracles.md#comida-y-madera-m3-magicspellsspellresource-magicobjectsmagicfoodwood-ecspotresource) |
| `UR_HealSpellChakra` | 0x6A0B20 | `Rules/Heal.cpp` | [`UR_HealSpellChakra` 0x6A0B20](miracles.md#ur_healspellchakra-0x6a0b20-psysruleshealcpp) |
| `CreateRuleFusedSphericalExplode` | 0x69F610 | `Rules/Heal.cpp` | [`CreateRuleFusedSphericalExplode` 0x69F610](miracles.md#createrulefusedsphericalexplode-0x69f610) |
| `UR_HealInHand` | 0x6A0F40 | `Rules/Heal.cpp` | [`UR_HealInHand` 0x6A0F40](miracles.md#ur_healinhand-0x6a0f40) |
| `UR_KPStretchHeight`, `UR_KPMoveAtoms` | 0x6A50C0, 0x6A60B0 | `Rules/KeyPoints.cpp` | [Las otras clases que faltaban](miracles.md#las-otras-clases-que-faltaban) |
| `UR_FollowTargets`, `UR_Flocking`, `EventConditionAtomNearVillagers` | 0x6A04B0, 0x683580, 0x67D8E0 | `Rules/Flock.cpp` | [Las partículas](miracles.md#las-partículas-psysrulesflockcpp-fiel) |
| `UR_ForestPath`, `ParticleGoodEvilCreator` | 0x6A3770, 0x6AAA00 | `Rules/Forest.cpp`, `PSys.h` | [Las otras clases que faltaban](miracles.md#las-otras-clases-que-faltaban) |
| `ParticleAnimWithCameraCreator`, `ParticleAnimCreator` | — | sin portar / (aproximado) | [Bosque](miracles.md#bosque-m4b-magicspellsspellforest-magicobjectsmagictree-ecstrees) |
| `UpdateRuleGravityWithFloor`, `CreateWithInitialDirection`, `AttatchFireBallToAtom`, `SetAtomHasBeenDeflected`, `UR_SideSpin`, `AR_FadeAlphaWithHeightAboveLandscape`, `AddSubCollectionsToAtom`, `UR_Trail` | 0x6A1880, 0x69E950, 0x682FD0, 0x6A26C0 | `Rules/Fireball.cpp` | [Bola de fuego](miracles.md#bola-de-fuego-magic_type-1-3-semilla-2-fire) |
| `UR_OrientSpriteWithVelocity` | 0x69A790 | `Rules/Orient.cpp` | [Las otras clases que faltaban](miracles.md#las-otras-clases-que-faltaban) |
| `UR_Lightning`, `UR_LightningStrike` | 0x6914C0, 0x6937A0 | `Rules/Lightning.cpp` | [Rayo](miracles.md#rayo-magic_type-4-6-semilla-6-lightning_bolt-psysruleslightningcpp) |
| `LightningForkFlicker` | 0x6B24D0 | sin portar | [Rayo](miracles.md#rayo-magic_type-4-6-semilla-6-lightning_bolt-psysruleslightningcpp) |
| `UR_AddDefensiveSphere`, `UpdateRuleShieldSpark`, `UR_InitialSpin`, `UR_VapourEndEffect`, `SetCollectionAlpha`, `UR_AtomsAtEPTarget`, `CheckShieldDeflections` | 0x6A2A60, 0x6A2BF0, 0x69E490, 0x6A39E0, 0x6A2720, 0x69A960, 0x6A2570 | `Rules/Shield.cpp` | [Las partículas](miracles.md#las-partículas-psysrulesshieldcpp) |
| `UR_SphereSurfaceTracer` | 0x6A32B0 | `PSys.cpp` | [Las partículas](miracles.md#las-partículas-psysrulesshieldcpp) |
| `ZR_SurfRevol`, `UR_ChangeScale` | 0x686370 | `Rules/SurfRevol` | [SF_TeleportVortex y ZR_SurfRevol](miracles.md#sf_teleportvortex-y-zr_surfrevol-srcpsysrulessurfrevol-srcgraphicsrenderersurfrevolcpp) |
| `UR_Explosion` | 0x67E200, 0x67ECE0, 0x67E900 | `Rules/Explosion.cpp` | [`UR_Explosion`](miracles.md#ur_explosion-r5-initcollection-0x67e200-modifyatomcollection-0x67ece0-actualización-0x67e900) |
| `SpreadingDiskEmitter`, `DiskEmitter`, `SetPSysCloseDown`, `EventConditionCollectionDelay` | 0x6A6610, 0x6A64D0, 0x6A26D0 | — | [Los archivos](miracles.md#los-archivos-fiel) |
| `UR_MoveAtom`, `UR_ChangeScaleXYZ` | 0x6A5E50, 0x6A5240 | — | [El rayo que se ve](miracles.md#el-rayo-que-se-ve-sf_beamexplosionfx-pt-138) |
| `UR_ExplodeObject`, `UR_ExplodeObject2`, `ER_EmitFromParentAtom`, `CreateRule_GameObjectRef` | 0x6814E0, 0x681560 | sin portar | [Sin portar / pendiente](miracles.md#sin-portar--pendiente) |
| `UR_CloudMoverNew`, `UR_CloudGather` | 0x6D41C0, 0x6D4A70 | `Rules/Storm` | [Los núcleos y las nubes](miracles.md#los-núcleos-y-las-nubes-ur_cloudmovernew-0x6d41c0-ur_cloudgather-0x6d4a70) |
| `UR_Tornado`, `UR_FollowParent` | 0x6D18B0 | `Rules/Storm` | [El tornado](miracles.md#el-tornado-ur_tornado-0x6d18b0-ctor-0x6d1680) |
| `UR_StormCast` | 0x6D59B0 | `Rules/Storm` | [El remolino](miracles.md#el-remolino-ur_stormcast-0x6d59b0-sf_stormcast) |
| `ParticleMeshCreator`, `ParticleMeshCreatorAnimTextured` | ctor 0x6A8960; 0x6A8B00, 0x6A8DA0 | `Creators/Mesh.cpp` | [Las mallas de partículas](particles.md#las-mallas-de-partículas-creatorsmeshcpp-particle3dobjdrawat-0x679fd0-y-la-cúpula-del-escudo) |
| `ParticleChainCreator`, `ParticleLightMapCreator` | 0x6AA900, 0x6A9D80 | `Creators/{Chain,LightMap}.cpp` | [Cadenas y mapas de luz](particles.md#cadenas-y-mapas-de-luz-psyscreatorschainlightmapcpp-graphicsrendererchaincpp) |
| `ParticleMistCreator` | ctor 0x6AA380 | `Creators/Mist.cpp` | [`ParticleMistCreator`](particles.md#particlemistcreator-psyscreatorsmistcpp) |
| `ParticlePointCreator`, `ParticleSpriteCreator` | — | ya estaban | [Sin portar / sin verificar](miracles.md#sin-portar--sin-verificar-curar) |

## Pendiente

- Sonido: el alineamiento del dueño, el temblor de cámara, los filtros de estado de juego, las repeticiones finitas
  y el tope global de distancia (ver [Sonido de las partículas](particles.md#sonido-de-las-partículas-lane-s-srcaudiospellsounds-srcpsysrulessoundcpp)).
- Cadenas: `UseDynamicLighting` (el desplazamiento de V es 0 en todos los datos); mapas de luz
  estampados en una textura de luz dinámica del terreno (no existe en el port).
- Mallas: `UseScriptHightlightPulse`, `CastHumanShadow`, `UseDynamicLighting`, `UseGlobalAlpha`, el orden Z por
  objeto, `FaceCameraSprite` y el .anm de `ParticleAnimCreator`.
- Niebla: un contador de atlas por niebla y el mapa de sombra / luz del terreno.
- PSys del mundo: creadores de malla, niebla, cadenas, animación, mapas de luz (se estampan en la luz del terreno), las
  reglas de hechizos y del pueblo (`UR_TownCentreBelief` ya está: ver arriba), `CreateRule_GameObjectRef` (el brillo de las llaves de la
  puerta de Land1, SF_HighlightOnObject), los sonidos, y pasar a este motor los efectos de la mano de `HandEffects.cpp`.
- Las reglas sin portar del índice (pedazos, `LightningForkFlicker`, `ER_EmitFromParentAtom`, `CreateRule_GameObjectRef`).

## Ganchos de prueba

- `OPENBLACK_TEST_PSYS`, `OPENBLACK_PSYS_SOUND_TRACE` y `OPENBLACK_PSYS_CHAIN_TRACE`, en
  [openblack-internals.md](openblack-internals.md#variables-de-entorno-de-depuración); `test_spell_sounds` y
  `test_lightning`.

## Fuentes

- `dev\tmp_dis\miracles\visuals_sound.md` (§3 el sonido), `dev\tmp_dis\sound\dlldis.py` (`LHaudiodllR.dll`),
  `dev\tmp_dis\miracles\ptnames.py` (los nombres de los tipos de partícula), `psys\pt_table.md` (la tabla antigua),
  `psys\part_render.md` y `dev\tmp_dis\miracles\impl\` (`m4a` niebla, `m6b` mallas, `m6s` jerarquías).
