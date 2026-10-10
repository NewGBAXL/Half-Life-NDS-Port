# Half-Life NDS Port

A port of Half-Life to the Nintendo DS and DSi, written in C++ for
[BlocksDS](https://blocksds.skylyrac.net/) on top of Nitro Engine Advanced.

**No game data is in this repository or in the ROM.** A converter turns your
own copy of Half-Life into the files the DS reads from its SD card.

## Status: milestone 11, "levels of detail"

Working:

- Any of the game's maps converts to a cooked level: BSP tree, PVS, clip hulls,
  and the map's faces as ready-made runs of geometry-engine commands with their
  lightmaps baked into vertex colours -- as the game shows them: a third
  stronger than the file has them and lifted by the power 0.8, cut off at
  full (`--light-scale`, `--light-power` for another curve), with its
  textures lifted by the same power. That curve was not read out of
  anything: it was fitted to the real game photographed beside the port at
  the same places (`tools/ref/measure.py`), which brought the mean
  difference over 120 patches of wall on three maps from 15.6 to 5.6 of
  255 (the port now 5% under the game, where it was 18% under).
  Neighbouring faces share vertices in
  quad strips, and faces are merged only where that provably changes nothing
  on screen (about 12% fewer vertices over the campaign).
- World textures in the DS's 4x4 compressed format; textures with holes
  (grates, fences) in A3I5, with soft edges. Both are sized per map to fit the
  texture and palette banks.
- The DS renders the world with PVS, frustum and back-face culling against a
  polygon and vertex budget. Single-pass rendering (60 fps) is the default; two-pass (30 fps, twice the
  capacity), in two methods, and a per-map automatic choice are in the pause
  menu. "Two-pass" joins the halves in video memory; "Two-pass DMA" builds
  each picture in main memory and shows it only when it is whole (a third
  texture bank is free, 190 KB of RAM is not, and it costs more CPU). Back faces are rejected on the CPU, surface by surface; the
  hardware's own test is off, because it goes by a polygon's first three
  vertices and misjudges very thin ones; the converter orders every polygon
  so that those three make a real corner. In two-pass mode each displayed frame is one game state: the left
  pass captures the whole composited screen, so the two halves never come
  from different moments. (What two-pass cannot keep is the 3D picture's
  18-bit colour: the half that is not being drawn comes from display
  capture, which is 15-bit. "Two-pass DMA" shows both halves from capture,
  so at least they match; plain "Two-pass" shows one half live.)
- On a DSi the world's surfaces go to the geometry engine as one display
  list, fed by the DSi's own DMA (NDMA) while the processor works out where
  the models' bones are. (Not while the DSP option is on: a DMA to the
  geometry engine and the DSP's traffic do not get on.) NEA's own display
  lists use NDMA there too (`NEA_DisplayListEnableNDMA`).
- A view with too much in it loses what is least seen first. Pressure (0 to
  3; `pN` on the overlay) rises while passes run out of budget and leaves
  out, more with each step, world surfaces and brush entities only a few
  pixels across and all but the nearest models. Only what is still too much
  is cut off at a distance, and the hardware's fog is brought in just short
  of that distance, so the far end fades out instead of ending in a hole.
  A view that fits has neither.
- Brush entities are drawn and solid: opaque, cut-out, see-through (glass),
  flat-colour, and invisible-but-solid.
- Doors (sliding and hinged), buttons, lifts and trains on path corners,
  ladders, walls that switch on and off, and the wiring between them:
  triggers, relays, multi_managers, multisources, delays and killtargets.
  Moving brushes carry and push the player, and turn back when blocked.
- Maps chain: level changes with landmarks carry the player (and the tram
  they are riding) into the next map. The opening tram ride runs from c0a0 to
  the station in c0a0e by itself, platforms that lower the tram included.
- Fans and doors that turn about any axis; boxes that break (with USE, until
  there is a crowbar) and boxes that are pushed; volumes that hurt, push and
  teleport; health, and dying.
- Water: swimming, sinking, jumping out onto a ledge, drowning, slime and
  lava; liquid surfaces sway. The sky, as a box of six pictures behind the
  world.
- Saving: at every level change, at the map's own save points, and from the
  pause menu. The game continues from the save when the console is switched
  on again.
- Models: the game's studio models, cooked to a DS format (one skin texture a
  model, animation as keys) and skinned by the hardware's matrix stack, at
  several levels of detail (below).
  Scientists, guards and props stand and animate in the maps; scripted
  sequences play (an actor goes to its mark as soon as a script takes it,
  waits there, acts when told, and what waits on it happens when it is
  done); actors cross level changes with the player. The crowbar is in hand.
- Monsters: headcrabs run at the player and leap, zombies walk up and swing,
  barnacles hang with their tongues down and pull up what walks into them.
  They move by the game's own rule for things that walk (a step is made if,
  lifted a stair's height, nothing is in the way and there is floor where it
  ends), straight at the player where that can be walked and otherwise from
  path node to path node: the converter joins the map's nodes up and works
  out the way from each to each, so the console only looks it up.
  Monstermakers make them.
- Weapons: the crowbar, the pistol and the shotgun, found where the game
  leaves them, with its numbers at the middle difficulty (a zombie shrugs off
  most of a bullet, as it does there). Ammunition, health kits, batteries and
  the wall chargers; armour. What is killed or picked up stays so when a map
  is come back to, and in the save.
- Sound: the game's own, converted to the 4-bit ADPCM the DS's sound chip
  plays by itself. Weapons, monsters, doors, buttons, breakage, footsteps and
  pick-ups; the maps' ambient sounds, louder and to one side as the player
  nears them; the announcer and the scripted lines of scientists and guards
  (from the game's sentences.txt), read off the card as they are spoken; and
  the music, decoded from the game's MP3s and streamed from the card. A map's
  sounds have an allowance of memory (350 KB); the longest are cooked at half
  the rate where a map has more than that.
- On a DSi the sound has more to it. The output runs at 47.6 kHz. With
  "DSi SOUND" switched on in the pause menu (it is **off** unless it is, and
  it is an experiment: tried on a DSi, it was not judged worth taking
  further, and nothing more is being built on the DSP; off is exactly what
  a DS does),
  the DSP
  plays the music in stereo (cooked as a second file for each track), and
  adds the echo of the room: the ARM7 captures what the sixteen channels are
  playing, the ARM9 passes it to the DSP, and the DSP puts out a reverb of it
  set by the map's own room markers (env_sound). The DSP program is in
  `teak/`, and is assembly, all of it: compiled C runs on this chip no
  faster than on the ARM9, and what it is good at -- multiply and add in
  one step, a loop that costs nothing to go round -- a compiler does not
  use. What the program must work out is written in C once, in one header
  (`shared/dsp/hl_dsp.h`, its model, which the host tests run), and
  `tools/dspsim` runs the real program in an emulated DSP and holds every
  pair of samples that comes out against the model's: a quiet scene, and
  one with everything as loud as it goes. By the same tool, a pair of
  samples costs 258 steps of the processor where the C program this
  replaced took 588 (6.3% of the processor against 14.4%, with music and
  echo both going). A DS is unchanged.
  The DSP is built not to make a noise when something goes wrong: its
  interrupt only hands finished samples to the speakers (the working out is
  in its main loop, and a port left short gets the last sample, fading); the
  music goes to it in blocks, each behind a mark and with the decoders' own
  state, so that a slip is over within 86 ms; a block out of step is silence
  and is reported, the track is started again from where it was, and after
  three such faults (or a DSP that stops answering) the game goes back to
  the DS's way for the session and says so in `perf.txt`.
  melonDS (1.1) is no judge of it: it lets the DSP run only when the game
  touches one of its registers, so music from the DSP comes out there in
  bursts with silence between.
- More to fight: alien slaves zap from a distance and claw up close,
  bullsquids spit, bite and whip, houndeyes run up and let off a blast. Hand
  grenades bounce, and burst after three seconds. Bolts, spit and blasts are
  drawn as plain shapes.
- Turrets, by the game's own rules: the miniturret (on a floor or hung
  from a ceiling), the sentry and the big turret. Folded away until one sees
  someone or is switched on by name (a door, a tripwire's relay); out,
  sweeping and pinging; onto whoever it sees, turning as the game's does and
  firing ten rounds a second with no spread; away again when nobody has
  come. A sentry is woken by a touch or a shot and wakes the others of its
  name; plating turns a bullet; one nearly done for goes wild. The big one
  has to spin up. Their guns are aimed by the models' own bone controllers,
  which the converter now keeps (the barnacle's tongue is one too).
- A hit to the head counts three times, as in the game. Where a shot lands
  is found on the model's own hit boxes, posed as the monster stands; a shot
  that goes through its outline but between the boxes still counts, as a
  hit to the body.
- The bullsquid is as wide as the game has it: the converter cooks the
  world's third clip hull, and it walks only where that has room.
- A monster the map marks as a prisoner goes for nobody (what is glimpsed
  during the cascade), and a map's "kill" of a name takes people and
  monsters away as well as brushes.
- The game's sprites. The converter reads its `.spr` files and cooks each
  to one small texture, its frames side by side (`spr/` on the card;
  `shared/HlSprite.hpp`). Most of them are "additive" in the game -- light,
  added to what is behind -- which the DS cannot do: how bright a texel was
  becomes how solid it is. Against the dark that is the same; against
  something bright it is dimmer than the game's.
- What the maps show and shake, from their own entities
  (`shared/HlEffects.hpp`): sprites and glows (`env_sprite`, `env_glow`),
  beams and lasers (`env_beam`, `env_laser`: straight or thrown about like
  lightning, striking and striking again as the map times them, burning
  whoever stands in one that burns), explosions (`env_explosion`: seen,
  heard, felt and hurting), shakes of the view (`env_shake`), sparks
  (`env_spark`), things thrown out (`env_shooter`, `gibshooter`),
  `env_render` making a brush see-through or unseen, and fades of the
  screen (`env_fade`).
- The port's own effects are drawn with the game's pictures: a grenade's
  fireball, an alien slave's lightning, a turret's tracer and muzzle flash,
  a bullsquid's spit, a houndeye's shock wave; a flash at the player's own
  gun, a puff where a shot lands on something that bleeds; the view shakes
  at an explosion near by.
- The HEV suit is taken where the game has it (it is what opens the way on),
  and armour is for someone wearing it.
- The seated and the dead are let down onto what is under them, as the game
  lets them down (a scientist at a desk in the low box he is given, onto the
  floor his chair stands on; a body, and furniture, as a point); a scientist
  at a desk goes between his five ways of sitting.
- People sent somewhere by a script walk there as the monsters walk: round
  what is in the way, by the map's path nodes. Only those the landmark can
  see cross a level change, and only if there is room for them on the other
  side.
- The renderer listens to the hardware: it reads how many polygons and
  vertices a frame really used, whether any were dropped, and how close the
  rasteriser came to not finishing its lines, and moves its budget by that --
  up, where the hardware throws away what is off the screen (often a fifth of
  what was counted), and down when a frame was too much. The same numbers are
  on the debug overlay and, for each map played, in `perf.txt` on the card.
- Player movement with Half-Life's own numbers: walking, jumping, crouching,
  steps, climbing, sliding along walls, in fixed point throughout (the DS has
  no FPU).
- The arithmetic. Everything that runs every frame is compiled as ARM
  (the build tool had been compiling the `.arm.cpp` files as Thumb, where
  every 64-bit multiply is a call), and the compiler's 64-bit divide and
  the fixed-point square root go to the console's own divider and
  square-root unit (`source/HwMath.s`): same answers, a tenth of the time.
  A build with `HLDS_PROFILE` checks the hardware's sums against the slow
  way at start-up and says so in `perf.txt`.
- On a DSi the 47.6 kHz output is asked of NEA
  (`NEA_SoundEnableDSiOutput`); it is the same output the game had by
  asking libnds directly.
- Soldiers (`shared/HlGrunt.hpp`): the game's human grunt by its rules and
  its numbers. Cooked with the head, gun and skin of what the map has him
  carry (MP5, shotgun, hand grenades, the launcher). He sees what is in
  front of him within 2048 units, and turns to a shot or a blast; fires
  bursts of three rounds a tenth of a second apart, four bursts running
  (a shotgun: a shell of five pellets), standing or crouched, and never
  with one of his own in the line; reloads when the clip is out, behind
  cover if there is any; kicks whoever is within reach; throws a grenade in
  the game's arc, or launches one at where the player will be -- never from
  within 256 units, never onto his own, never twice in six seconds, and
  first of all when the player is out of sight; runs from a grenade that
  lands by him; takes cover nine times in ten when he is hurt, and limps
  under twenty health. His helmet takes twenty off a round, a round in the
  head counts three times, and he dies as he was hit.
  Squads are the game's: by the map's names where it names them and flags
  a leader, else whoever of his kind a soldier can see within 1024 units
  when the map starts. The whole squad learns of the player at once; the
  leader calls it, waves the others on the first time, and opens fire while
  they make for cover; two of a squad fire at once and two may have a
  grenade in hand; the rest hide, or go in for a line of fire. Cover is the
  nearest place the player has no line to: a few steps to one side, or one
  of the map's path nodes. They talk -- the game's own lines for a grenade,
  the alert, cover, a charge, a taunt, and the questions and answers of
  soldiers with nothing to do -- off the card, one line at a time.
  Monstermakers make them, scripts move them, and what a map has one set
  off when he sees the player, is hurt or dies is set off.
- Tripmines: armed two and a half seconds after the map starts (one, where
  the map says), a beam to the first thing in its way; the mine goes off
  when the beam gets shorter or longer by anything -- the player, a
  monster, a door -- or when it is shot or caught in a blast: 150, to 375
  units, so that one sets off the next.
- The MP5 and the .357, found where the game leaves them. The MP5 fires ten
  rounds a second from a clip of fifty (9 mm, which it shares with the
  pistol), and **R** launches a grenade that goes off at what it touches;
  the .357 is six rounds of forty. With any other gun in hand, R reloads.
- What is taken far past dead comes apart: into the game's own pieces (the
  skull and four more for a person, four for an alien), which fly, tumble,
  bounce, lie, and are gone after a while. A body caught in a blast does
  too. What the maps throw out (`env_shooter`) is the model the map names.
- The big turret's eye glows red at each ping, where the model has it.
- Beams whose ends are things that move follow them (the lasers on the
  trains of c1a1, the cascade's); a beam with one end left to chance
  strikes somewhere new each time, as far as the world lets it; one with
  several targets of a name picks among them; rings are rings.
- Everything see-through -- glass, sprites, beams, effects -- is drawn in
  one order, far to near, and what the effects cost is counted against the
  polygon budget with the rest.
- Sound in layers, through maxmod (by way of NEA where NEA has the call).
  Maxmod plays samples out of a "sound bank"; ours is not a file but the
  map's cooked sounds where they already are in memory, each handed over as
  a small header that points at it, so nothing is cooked twice and the
  hardware still plays the ADPCM itself, loops included. Every sound is of
  a layer -- the player's own, the world's, the voice, the loops of a
  place, the menu -- and each layer has its share of the fourteen voices
  the hardware really has beside the music (3, 4, 2, 4, 1), which no other
  layer can take from it; over its share a layer has only what is free,
  and gives it back, the quietest first, to whoever is short
  (`shared/HlMixer.hpp`). If the hardware refuses a sound anyway, what
  matters least gives its channel up and the sound is asked for again:
  nothing is counted as played that was not. So a far headcrab does not
  cut off the player's gun.
  **Two people may speak at once**: a line cuts in only on the same
  speaker's last (the same place -- or no place, which is the voice heard
  everywhere), and the next word of a line is read from the card while the
  one before it is said. A scripted line comes from where its speaker
  stands. What has a place goes on being placed as the view moves and
  turns. Sounds are played at the **pitch** the map asks for; up to eight
  looping ambiences run at once where voices are free; what moves (a lift,
  a train, a door, a fan) is heard for as long as it moves, if its sound is
  one that goes round, and a sound whose file has no loop point is played
  once, as the game plays it. Buttons have the whole of the game's list of
  sounds, and a door or button that will not work answers with its locked
  sound and line -- and, the day it works for them, with its unlocked
  one. A line is said word by word as the game's sentences.txt marks it:
  each word at its own pitch and loudness, cut short where it says, with
  the pauses of its commas and full stops. A voice that gives way to
  another sound is faded over a few frames, not cut (a hardware channel
  is kept back for that: thirteen voices and one on its way out). The
  player is heard on ladders, in water and landing, and a step sounds of
  what it lands on -- metal, dirt, a vent, a grate, tiles -- by the game's
  own list of what its textures are made of (`sound/materials.txt`): the
  converter marks each texture, and the console finds the floor under the
  feet once a step (`shared/HlMaterial.hpp`). What the models themselves
  sound like as they move -- a footstep in a walk, a keypad in a scene --
  is in their animations, and is played from where they are.
  **In the pause menu everything stops where it is, music included, and
  goes on from there** (a DS's channel cannot be stopped and continued: it
  is slowed to the slowest the hardware has and silenced, which costs a
  sound a quarter of a second for ten of menu); the menu's clicks are
  heard meanwhile. The music, on a DS, is a maxmod stream fed from the
  card.
- The pause menu has two pages: RESUME, SAVE, NEW GAME, OPTIONS, COMMAND;
  and under OPTIONS the rendering mode, the DSi's sound, a volume slider
  each for the music, the effects, the voices and the ambience, which
  side the touch screen's buttons are, and the
  debug readout (off unless switched on; it is drawn along the top of the
  touch screen in every rendering mode). Buttons act when the stylus is
  lifted from them; sliders follow it. The command line has a keyboard of
  the port's own.
- The touch screen is laid out as the DS's better first-person games lay
  theirs out (Dementium II is the model), in the suit's amber on dark: a
  bar along the top with MENU, USE in the middle and RELOAD (faint when
  there is nothing to reload; health and the suit's charge are on the top
  screen and not said twice); a framed aim pad; JUMP, the lamp and DUCK in a column beside it; and along the
  bottom a slot for each weapon, always in the same place, with the
  game's own picture of it (cooked from your `sprites/320hud1.spr`), the
  one in hand lit with its rounds under it. A touch on a slot takes that
  weapon; a touch on the one in hand reloads it, as RELOAD does (the MP5
  too, which R cannot reload while it has grenades to launch). One table (`shared/HlTouchLayout.hpp`) says where everything
  is, to the code that draws and the code that tests a touch alike. For
  a stylus in the left hand, "Buttons: left" in the options mirrors it.
- Light that changes. The world's light is baked into its vertices, but
  not all of the game's light is steady: lights a switch turns on and off
  (its "styles" 32 and up) and lights that flicker, pulse or strobe (1 to
  12, the game's own ten-a-second patterns). Where such light reaches a
  face, the converter gives every vertex of it a colour word of its own
  and records what the vertex is without that light and what each style
  adds (`LightPatch`); the console works those words out again, in the
  level's own geometry stream, whenever a style's strength changes
  (`shared/HlLightStyle.hpp`). A named `light` is a thing the map's
  switches can use. (People standing in such light are not relit by it.)
- More of the game's machinery: a `func_train` keeps the angles it is
  given (the tram's door at the station is a quarter turn round from how
  it was built); `func_plat` and `func_platrot` (the round lift down to
  the test chamber travels and turns, and whoever rides it turns with
  it); lifts and trains have their going and stopping sounds;
  `func_pendulum`; a named `infodecal` leaves its mark when triggered
  (the microwave's last act); `env_beverage` (a can, and a point of
  health, while the machine has any); `env_funnel`.
- The picture is the player's to tune: OPTIONS > GRAPHICS in the pause
  menu (`shared/HlFog.hpp` has what each choice stands for).
  - **Fog**: *off*, *when needed* (only while a view too heavy to draw
    is being cut short -- what the fog was always for) or *always* (a
    steady distance fog). **Strength** is how solid it gets at its far
    end; it used to be solid black right up to the cut, a wall: it now
    comes on along a curve and stops at seven tenths unless asked for
    more. **Distance** is where an always-fog is whole (768 to 6144
    units) and how close to the cut the other kind is allowed to sit.
    **Colour**: black, grey, blue, brown or green -- the empty distance
    behind everything takes the same colour, so a cut does not show as
    an edge.
  - **Model detail** (low / normal / high): how far off people and
    monsters step down to their coarser meshes -- half, as cooked for,
    or twice the distance.
  - **World detail**: how readily small far surfaces are left out when a
    frame is heavy -- sooner, as before, or never (then the view is cut
    shorter instead).
  - **Models at once**: 4 to 12, or what the machine is good for (8 on a
    DS, 12 on a DSi).
    Who those are is decided by what they are before how near they are
    (`shared/HlDrawPlan.hpp`): an enemy alive and in view is always drawn,
    limit or no limit, and so are the people of the place; what gives way
    for them is, in order, bodies and furniture, things to pick up, the far
    enemies' detail, the detail of the weapon in hand, and at the last up
    to 300 polygons of the world's far end. When that has had to happen
    the frame's stains, pieces and gibs are cut back as well. And "in
    view" follows the shape of the thing: a barnacle is in view by any
    part of its tongue, not only by its mouth on the ceiling -- under a
    high ceiling it used not to be drawn at all unless looked up at. The
    debug overlay says when anything in view was left out, by kind
    (`cut A0 B0 C1 D3`: A, enemies, is never to be anything but 0).
  - **Draw distance**: far (8192 units), medium (3072) or near (1536).
  The hardware's fog is 32 steps of density over a band of fixed widths:
  a light or near fog shows its steps.
- Textures that change, the game's four kinds:
  - **Cycling** (`+0name` .. `+9name`): screens and panels that play
    their pictures ten a second. The level lists a cycle's pictures one
    after another, all cooked at one size, and surfaces name the first;
    which is bound is the console's to say (`TextureCache::shown`). The
    geometry carries no texture choice of its own, so it costs nothing to
    draw -- only the pictures' video memory, which the budget counts.
  - **Switched** (`+0name` / `+Aname`): the other face of a brush -- a
    light fixture switched off, a button pressed in, a health or suit
    charger with nothing left -- shown by what the thing is doing
    (`BrushState::frame`, worked out each tick).
  - **Sliding**: a `func_conveyor` carries whoever stands on it at its
    speed, runs the other way when used, and its `scroll` texture slides
    with it through the texture matrix. (The same texture on a plain wall
    stays still, as in the game.)
  - **Varied** (`-0name` .. `-9name`): each face wears one of the
    texture's variants, by where the face is, so big floors and walls do
    not visibly repeat. Decided by the converter; at run time they are
    ordinary textures. It costs about a hundredth more surfaces and some
    texture memory; `--plain-tiles` cooks without it. The choice is not
    the game's own (that comes from the C library's random numbers): the
    same idea, another scatter.
- What a shot, a blow and a fall do, as in the game:
  - **Marks on walls** -- bullet holes, the crowbar's dent, blood behind
    what is shot, struck or blown up and under what dies or lands in pieces
    (red for people and the barnacle, yellow for the aliens, none for
    machines; the player's own when they are hit), the scorch of a blast,
    cracks on glass that is shot and stands. Scientists and guards can be
    hurt and killed, and the scene that needed them goes without. The
    stains the map's makers left (its unnamed `infodecal`s) are on the
    walls near the player: the nearest sixteen on a DS and thirty-two on
    a DSi within 640 units, each as large as its picture is in the game
    (48 units for most blood, 96 and 80 for the pools, 64 for a hand's
    print, 96 for a scorch or a crack), each one itself -- two side by
    side are never made one -- and the walls' own cracks and dents among
    them. They are the game's own decals (your `decals.wad`, cooked to
    one 256x64 texture), each drawn as one quad in its wall's plane with
    the hardware's "same depth" test, which is what that test is for. A
    ring of them (32 on a DS, 64 on a DSi); one on a door or a pane goes
    with it. They are kept cheap: a new mark where one of its kind already
    is takes that one's place (ten blows on one spot are one mark, a pane
    has one crack, glass that never breaks takes none from a blow); each
    fades out after twenty or forty seconds (twice that on a DSi); only
    as many as there are are counted against the frame, and no more are
    drawn than cover about a quarter of the screen between them. Dust flies off a wall that is hit, and now and then the
    round whines away.
  - **Breaking** -- what breaks throws pieces *of itself*: small tumbling
    patches of the broken brush's own texture (glass see-through), as
    many and as heavy as its material says, which fall, lie and go. No
    shard models, no extra video memory. Each piece, speck and casing is
    one triangle; the solid ones are sent in runs that share everything
    (the plain ones in one, each texture's together) and only broken
    glass takes a turn among what is see-through. They are gone in a
    second or two (half as long again on a DSi), sooner once they lie,
    and the far ones are not drawn at all when a frame is heavy.
  - **Being hurt** -- the screen reddens (a red sheet, as the game does
    it), a mark on each side of its middle the blow came from, and the
    head is knocked back. Falls hurt by the game's rule (safe to 580
    units a second, fatal at 1024). Dying, the camera sinks to the floor
    and rolls over before the game goes back.
  - **The view** -- each weapon's own kick when it fires, the bob of a
    walk and the lean into a sidestep (the game's numbers; off unless
    "View bob" in the options turns them on), the dip of a landing, casings thrown
    from the pistol, the MP5 and the shotgun (`shared/HlView.hpp`).
- Two things that could hold the game for good are closed. A player
  stuck fast in something (nothing got them out but a few units' nudge)
  is, after half a second, put somewhere clear up to 128 units off. And
  dying goes back to the latest save -- but three deaths on end, each
  within twenty seconds of going back, go back to the save made on
  coming into the map instead (a second save file), and failing that to
  the map's start: a save made a moment before certain death is no
  longer a loop with no way out.
- The game's writing over the picture: the HALF-LIFE title on the first
  map, the names of the introduction along the tram ride, each chapter's
  name as it begins. The converter reads the game's `titles.txt` (what
  each message says, where, in what colours, how it comes and goes) and
  cooks every `env_message` and the world's `chaptertitle` / `gametitle`
  into the level; the console writes them on sprites of the top screen,
  in front of the 3D picture and added to it as the game adds its own
  (`source/render/TopText.cpp`: video bank G, no texture memory, the same
  in all three rendering modes), in the game's title face, faded or
  written out letter by letter as the file asks.
- The suit's lamp, by the game's rules: it wants the suit; two minutes
  alight, twenty seconds to be full again; it starts at 99 of a hundred,
  lights with whatever is in it, and what is in it is kept from map to
  map and in a save. Its light is the game's too: a ball eighty units
  from middle to edge at the point the middle of the view meets (out to
  2000 units), the same size however far off, fading evenly from its
  middle, full strength to 500 units and fainter beyond. The world's
  light is baked into its vertices and cannot be relit, so the light is
  *projected*: the surfaces in it are drawn a second time with one small
  round picture of light, and the hardware itself works out where on
  that picture each vertex is from where the vertex is in the world (its
  texture matrix, in the mode that takes the vertex --
  `NEA_TEXGEN_POSITION`; `WorldDraw::lampPass`, `shared/HlLamp.hpp`). So
  it lies on the walls as a light does: round a corner or a pillar, long
  on a floor it skims. The surface's own packed geometry is sent again
  through a small filter (vertices as they are, the projection's
  constant for a texture coordinate, and for a colour the wall's own
  texture's average, so that the wall is lit towards what it is and not
  towards white) and drawn only where the depth is the very same. Doors
  and other solid brushes are lit the same way; whoever stands in the
  light is lit by a hardware light along the view. What it is not: added
  light (the DS blends towards a colour -- the wall's texture is seen
  fainter in the light's middle, where the game shows it brighter), or a
  caster of shadows. Its button is in the touch screen's column between
  JUMP and DUCK, with what is left in it; Select switches it too; the
  game's lamp icon is at the top right of the top screen whenever the
  suit is worn, dim while the lamp is off.
- Everything on the touch screen is drawn in the game's own fonts, which
  the converter cooks from your `fonts.wad` and `gfx.wad` (`ui/` on the
  card; `shared/HlFont.hpp` draws them on a screen of tiles). The libnds
  text console is gone. A card cooked before this has no fonts: then the
  same code writes in the 8x8 letters that are in the ROM anyway.
- A head-up display on the top screen, as the game has it, in its own
  digits and pictures (its 320-wide set, cooked to one small texture):
  health and armour at the bottom left, what is in the gun and what is
  carried for it at the bottom right, the MP5's grenades above that, a
  crosshair for what is aimed. Nothing, until the suit is worn. It is
  drawn as flat quads six units in front of the eye through the world's
  own projection, which is right in all three rendering modes.
- Loading is still: the last picture of the game stays on the top screen,
  frozen, until the next map's first picture is whole (`FrameFreeze`). The
  3D picture left on the screen is drawn by the hardware again every frame
  out of video memory, and a load writes over video memory: so the screen
  is captured first and the capture shown in front of the 3D layer. In
  single-pass that takes a bank of video memory, D, which textures no
  longer use there (the two-pass DMA mode never had it, and every map
  fits); plain two-pass captures into one of its own two; in the DMA mode
  the picture is in main memory already. The very first load has no
  picture to keep and shows the game's own loading picture (cooked from
  your `cached.wad`); a load that changes the rendering mode shows black.
  That first picture is itself captured and its texture let go, so nothing
  drawn out of texture memory is on the screen while a map's textures are
  written into it. (The engine used to switch all four texture banks off
  for every texture it loaded, the one being shown among them: a band
  across the picture a texture. It now switches only the bank it writes.)
  The screen is blacked out for the dozen frames the new map takes to put
  itself in order. One owner of the screen's brightness (`ScreenFade`)
  keeps the map's fades, the dimness of being under water and this
  curtain from undoing each other.
- Levels of detail for models, made with meshoptimizer
  (`thirdparty/meshoptimizer`; `tools/hlds-convert/core/src/ModelMesh.cpp`).
  A person is 600 to 850 triangles in the game and the DS draws 2048 a
  frame, so every model is cooked at three levels: about 350 triangles
  (what it always was thinned to), half of that, and about a third. The
  simplifier weighs texture coordinates and normals with the shape and
  only ever keeps vertices the model has, each on its own bone at its own
  texel, so skinning and the skin's seams are as they were; each level
  records how far it is from the game's own mesh (the distance all but
  three in a hundred of its vertices are within). The console draws each
  model at the coarsest level whose error is under a pixel and a half at
  its distance, a level coarser for each step of pressure for all but the
  nearest, and coarser still before it gives a model up for want of room:
  a far soldier costs 120 polygons where he cost 350. A model small to
  begin with has fewer levels, or the one.
  A DSi's model files have a fourth level on top: the game's own mesh,
  untouched, for the nearest model (and the weapon in hand) in a frame
  with room. A DS has not the memory for a second copy of every mesh.
- Strips. The DS has no index buffers: a vertex costs each time it is
  sent. Every model's triangles -- thinned or not -- are made into strips
  afresh, by meshoptimizer's strip builder and by a plain one, and whichever
  sends fewer vertices is kept per mesh (the game's own, with their fans,
  if neither does better). About 1.5 to 1.6 vertices a triangle, where a
  model as the game has it sends 1.6 to 1.9: a tenth to a sixth fewer
  vertices for the same picture.
- One ROM for DS and DSi; DSi mode uses the 134 MHz clock and 16 MB of RAM.
  What a DSi has over a DS, apart from speed: sounds at the rate the game
  has them (up to 22 kHz, two megabytes a map: the converter writes those
  beside the DS's own, and says how many); twice as many effects, things in
  flight and pieces at once; twelve models drawn at once where a DS draws
  eight; the stereo music and room echo of its DSP,
  when that is switched on. Nothing is left out for a DS that it had
  before: the heaviest soldier maps (c1a3a, c2a2b1) leave it 1.6 MB and more
  free.

Not yet: friends who follow and talk; the rest of the game's weapons and
monsters; `env_bubbles`; the suit's voice, the chatter of scientists
and guards, the sounds of trains and lifts. Soldiers fight the player only
(not the aliens), do not drop their guns, do not come down ropes, and are
not eaten by barnacles; the player cannot yet lay a tripmine. What breaks
still comes apart into plain scraps (it is monsters, and what the maps throw
out, that have the game's pieces). Scientists and guards do not
think: they stand where the map puts them and go, in a straight line, where a
script sends them. A script whose actor this port does not have is taken as
played after a few seconds, so that what waits on it still happens. Monsters
do not open doors or jump gaps. A map that is come back to starts as it
started, except that what was broken, killed or picked up stays so.
The converter counts what it does not handle rather than refusing a map.

## Controls

| | |
|---|---|
| Stylus drag | look around |
| D-pad, or A B X Y | move |
| L | fire, or swing the crowbar |
| R | the MP5's grenade launcher; with any other gun, reload |
| Touch a weapon's slot | take that weapon; the one in hand: reload it |
| Touch RELOAD | reload |
| Touch JUMP, or tap the pad twice | jump (hold it, or the second tap, to keep it held) |
| Touch DUCK | crouch |
| Touch USE | press a button, open a use-only door, charge at a wall unit |
| Start, or touch MENU | pause menu: resume, save, new game, options (graphics: rendering mode, fog, detail; DSi sound, volumes, buttons left or right, view bob, debug readout), debug command line |
| Select, or touch the lamp | the suit's lamp |

The pause menu's COMMAND opens a debug command line on the on-screen
keyboard: `map <name>` goes to a map, `give` hands over every weapon with its
ammunition and the suit, `heal` restores health, `noclip` flies.

## Building

Requirements: BlocksDS with `nitro-engine-advanced` and `ptexconv` installed
under `$BLOCKSDSEXT`, Python 3, CMake 3.20+, a C++20 compiler, and optionally
Qt 6.8+ for the converter's GUI.
NEA has to be one with `NEA_MaterialGetTexParam` and `NEA_PaletteGetBase`
(the fork's two accessors for building display lists that change texture).

```sh
./setup-env.sh          # once: Python venv with the ROM build system
./make.sh               # builds half-life-nds-port.nds

cmake -S tools/hlds-convert -B build/convert
cmake --build build/convert -j
ctest --test-dir build/convert
```

Set `HLDS_VALVE_DIR` to a Half-Life `valve` folder to make the tests also
check collision and movement on a real map, run the whole tram ride through
its six maps, play the resonance cascade through to the map after it, fight
headcrabs and zombies in theirs, check that nobody in twenty-three maps
stands or walks inside the world, and pack every common sound (under a
minute, nothing is drawn or heard).

The DSP program needs the Teak LLVM toolchain and libteak in `$BLOCKSDSEXT`
(`llvm-teak/`, `libteak/`); `./make.sh` builds it and links it into the ROM,
and without them builds a ROM with no DSP audio. `tools/dspsim/run.sh` runs it
against its model (it needs teakra, built: see the script), and says how much
of the processor it uses; it is one of the test suites, skipped where teakra
is not built.

`tools/emu/` has scripts that run the ROM in melonDS without a display, for
looking at a change on the console's own renderer -- and, with `EMU_AUDIO`,
for writing what it would have played to a file.

## Comparing with the game

`tools/ref/` runs the real Half-Life -- your own Steam copy -- without a
screen, with the player put at a place and the game driven by the same kind
of script that drives the port in the emulator, and lays the two pictures
side by side:

```sh
tools/ref/compare.sh scientist c1a0 "-768 104 -180 0" "shot; key a 0.1; sleep 0.15; shot"
```

It runs the game out of a folder of its own beside `valve` (made by
`tools/ref/setup.sh`, removed by `remove.sh`), so your game's config and
saves are not touched. See `tools/ref/README.md`.

The crowbar was the first thing checked this way, and is now the game's: its
three swings at the air in turn and its two that land turn about, a blow on
a wall counted as a blow (it was a miss), every swing heard through the air,
a swing that meets nothing made again a tenth of a second on, a whole blow
after a second's rest and half of one on the heels of the last, three thuds
on flesh and two rings on a wall -- as loud as what the wall is made of lets
them be, with that material's own sound -- the dent a fifth of a second after
the blow, the hand left as its last swing left it, and put away before
another weapon comes out (`shared/HlMonsters.hpp`, `crowbarBlow`).

## Converting the game

```sh
build/convert/hlds-convert cook-all <path to>/Half-Life/valve -o <SD card root>
```

or run `build/convert/gui/hlds-convert-gui`. Everything goes to
`<card>/_nds/hl_nds/data/`. Other commands: `cook-map` (one map), `pack`
(the card's archives again, cooking nothing), `info` (what a map will cost
the DS to draw), `verify` (check a cooked level, or a map's archive, with the
DS's own reader; `--brushes` lists its brush entities).

**On the card** a map is one file, `maps/<map>.npac`: an archive (Nitro
Engine Advanced's NPAC, mounted by the console as a drive) holding all that
the map loads -- its level, textures, sprites, models and sounds -- in the
order it loads them. Opening a file is the slow part of reading a card, and a
map used to be between one and five hundred of them. `common.npac` holds
what belongs to no map (the letters, the head-up display, the menu's
sounds). Loose beside them: the music, which is played off the card as it
goes, the manifest, and what the game itself writes.

Inside an archive, what shrinks well is LZ77-compressed (the DS's own BIOS
format, inflated by the engine as a member is opened): levels and models
come to about half. A member is compressed only if it is a kilobyte or more
and comes down to three quarters of itself; textures of a few hundred bytes
and sounds, which are compressed as sound already, are stored as they are.
A level is in pieces of 256 KB at most, each compressed on its own, because
a member is inflated whole beside where it is going and a DS has not the
memory to do that to a level. `--no-compress` stores everything.

**Finding the way.** The game works out, the first time a map is run,
which of its path nodes can be walked between, by what size of creature and
through which doors, and writes that beside the map
(`maps/graphs/<map>.nod`); those files come with the game, and the converter
reads them (`NodGraph`): the nodes, the links for small things, people and
large things, and the door or breakable thing each link passes through. It
adds ways DOWN from ledges, which the game has not got. There is no table
of routes on the card: the console finds a route when it needs one
(`shared/HlWalk.hpp`, `pathStep`), over the links as they are at that
moment -- a door shut costs a little and is opened by whoever opens doors,
a locked one or something not yet broken is no way. Monsters go to where
they last saw the player, give up after a while and go home; soldiers take
cover they can reach. Scientists and guards follow the player on USE and
stay on USE again; a scientist runs out of sight of what would kill him, a
guard shoots it, and shoots a player who has shot him twice
(`shared/HlPeople.hpp`). The map's entity text, which the console never
read, stays in the stage (`maps/<map>.ent`) instead of in the level.

The loose files the archives are made from stay on the PC, in a **stage**
folder (`--stage <folder>`; by default `<card root>-stage`, beside the
card's): a texture or a sound shared by fifty maps is made once there and
found again by the next run.

`build/convert/hlds-testmap <folder>` writes a small synthetic map -- one room
with a door, a button, a lift and one brush of each drawing class -- that can
be cooked and booted in seconds and contains no game data. It is the quickest
way to look at a rendering or logic change on the console.

## Layout

| | |
|---|---|
| `source/` | the ARM9 program. `*.arm.cpp` is built as ARM; `*.itcm.arm.cpp` also runs from ITCM |
| `teak/` | the DSi's DSP program (music and echo), in assembly |
| `shared/` | file formats, fixed-point maths, collision and movement: compiled by both the ROM and the converter, so the host tests run the console's code |
| `tools/hlds-convert/` | the converter: `core` (no Qt), `cli`, `gui`, `tests` |
| `architectds_nea_mod/` | the ROM build system (ArchitectDS with Nitro Engine Advanced support) |
| `thirdparty/yas/` | serialization, used for the manifest |
| `thirdparty/minimp3/` | MP3 decoder (CC0), used by the converter for the music |

## Third-party code

- **meshoptimizer** 1.3 (`thirdparty/meshoptimizer/`), in the converter
  only: Copyright (c) 2016-2026 Arseny Kapoulkine, MIT licence (its
  `LICENSE.md` is beside it).
- **minimp3** and **yas** (`thirdparty/`), under their own licences.

## Provenance

The runtime is a rewrite for the DS, not a port of the game's source. The
hull trace and player movement follow the algorithms and constants of the
Quake/GoldSrc engines (as seen in xash3d-fwgs and the Half-Life SDK's
`pm_shared.c`) so that the game plays like itself; they were written anew in
fixed point. No Valve code or assets are included.
