# Object list

Every object Sonic 1 can spawn, with its disassembly source and whether this
port implements it. The ID, name, description and source columns come straight
out of `disasm/_incObj/` and `src/constants.h`; the ported/not-ported status
mirrors the `obj_map[]` dispatch table in `src/objects.c`, so the two must be
updated together when an object is added (see the last section).

| | |
|---|---|
| Object IDs in the game | 134 (`$01`-`$8C`; `$02`-`$07` do not exist) |
| Ported | 72 |
| Not ported | 62 |
| &nbsp;&nbsp;of those, unused by the game | 7 |
| &nbsp;&nbsp;of those, actually reachable | 55 |
| Disasm object source files | 110 (some hold two or three IDs) |

Coverage per zone, counting only the objects the disassembly attributes to it:

| Zone | Ported | Pending | Unused |
|---|---|---|---|
| Green Hill (GHZ) | 16 | 1 | 1 |
| Marble (MZ) | 23 | 0 | 1 |
| Spring Yard (SYZ) | 14 | 0 | — |
| Star Light (SLZ) | 8 | 10 | — |
| Labyrinth (LZ) | 5 | 15 | — |
| Sandopolis (SBZ) | 5 | 19 | — |
| Final (FZ) | 0 | 3 | — |
| Global (no zone named) | 27 | 10 | 5 |

## How to read a row

- **ID** — the byte the ASM object loader writes as `obType`
  (`disasm/_inc/Object Pointers.asm`); the `#define id_*` token is in
  `src/constants.h`.
- **Disasm source** — the file under `disasm/_incObj/` the C code is a 1:1
  translation of. Several IDs sharing one file are listed on the same source
  because the ASM keeps them in a single routine (Object 13/14, 25/37, 3D/48...).
- **Status** — *Ported* means the ID is in `obj_map[]` and its section exists in
  `src/objects.c`. *Not ported* means the ID falls through to
  `NullObject_Main`, which deletes the object on the spot, exactly like the ASM
  `NullObject`. Such objects are simply absent from the level, which is why the
  missing ones are mostly the whole of Labyrinth, Sandopolis and Star Light.
- **Routine** — the `obRoutine` dispatcher for the object inside
  `src/objects.c`, which is sorted by object ID.
- **Zones** — from the disassembly file name and its header comment. *any* means
  the disassembly does not name a zone (global objects such as Sonic, the HUD,
  rings, monitors, the signpost or the cards).

## All objects

| ID | `id_*` | Description | Zones | Status | Routine | Disasm source |
|---|---|---|---|---|---|---|
| `$01` | `id_SonicPlayer` | Sonic the Hedgehog | any | **Ported** | `SonicPlayer_Main` | `01 Sonic.asm` |
| `$08` | `id_Splash` | water splash (LZ) | LZ | Not ported | — | `08 LZ Water Splash.asm` |
| `$09` | `id_SonicSpecial` | Sonic the Hedgehog (in Special Stages) | any | **Ported** | `SonicSpecial_Main` | `09 Sonic in Special Stage.asm` |
| `$0A` | `id_DrownCount` | drowning countdown numbers and small bubbles that float out of Sonic's mouth (LZ) | LZ | Not ported | — | `0A LZ Drowning Countdown.asm` |
| `$0B` | `id_Pole` | breakable pole in wind tunnels that Sonic hangs onto (LZ) | LZ | Not ported | — | `0B LZ Pole that Breaks.asm` |
| `$0C` | `id_FlapDoor` | flapping door before wind tunnels (LZ) | LZ | Not ported | — | `0C LZ Flapping Door.asm` |
| `$0D` | `id_Signpost` | signpost at the end of a level | any | **Ported** | `Signpost_Main` | `0D Signpost.asm` |
| `$0E` | `id_TitleSonic` | Sonic on the title screen | any | **Ported** | `TitleSonic_Main` | `0E, 0F Title Screen - Sonic, Press Start, TM.asm` |
| `$0F` | `id_PSBTM` | "PRESS START BUTTON", "TM", and masking sprites on title screen | any | **Ported** | `PSBTM_Main` | `0E, 0F Title Screen - Sonic, Press Start, TM.asm` |
| `$10` | `id_Obj10` | blank (This was a Sonic animation test object in the prototype) | any | Not ported | — | `10 Unused - Blank.asm` |
| `$11` | `id_Bridge` | GHZ bridge (the main object, for the stumps refer to Object 1C) | GHZ | **Ported** | `Bridge_Main` | `11 GHZ Bridge.asm` |
| `$12` | `id_SpinningLight` | spinning light in hexagonal glass prism (SYZ) | SYZ | **Ported** | `SpinningLight_Main` | `12 SYZ Search Light.asm` |
| `$13` | `id_LavaMaker` | lava ball maker (MZ, SLZ) | MZ, SLZ | **Ported** | `LavaMaker_Main` | `13, 14 MZ, SLZ Fire Balls and Maker.asm` |
| `$14` | `id_LavaBall` | lava balls (MZ, SLZ) | MZ, SLZ | **Ported** | `LavaBall_Main` | `13, 14 MZ, SLZ Fire Balls and Maker.asm` |
| `$15` | `id_SwingingPlatform` | swinging platforms (GHZ, MZ, SLZ) - spiked ball on a chain (SBZ) | GHZ, MZ, SLZ, SBZ | **Ported** | `SwingingPlatform_Main` | `15 Swinging Platforms.asm` |
| `$16` | `id_Harpoon` | harpoon (LZ) | LZ | Not ported | — | `16 LZ Harpoon.asm` |
| `$17` | `id_Helix` | rotating helix of spikes on a horizontal pole (GHZ) | GHZ | **Ported** | `Helix_Main` | `17 GHZ Spiked Pole Helix.asm` |
| `$18` | `id_BasicPlatform` | basic platforms (GHZ, SYZ, SLZ) | GHZ, SYZ, SLZ | **Ported** | `Platform_Main` | `18 Platforms.asm` |
| `$19` | `id_Obj19` | blank (This was the infamous rolling GHZ ball level obstacle in the prototype) | GHZ | Not ported | — | `19 Unused - Blank.asm` |
| `$1A` | `id_CollapseLedge` | collapsing ledge (GHZ) | GHZ | **Ported** | `CollapseLedge_Main` | `1A, 53 Collapsing Ledges and Floors.asm` |
| `$1B` | `id_WaterSurface` | water surface (LZ) (Two objects are loaded, one for the left and one for the right side.) | LZ | Not ported | — | `1B LZ Water Surface.asm` |
| `$1C` | `id_Scenery` | scenery (GHZ bridge stump, SLZ lava thrower) | GHZ, SYZ, SLZ | **Ported** | `Scenery_Main` | `1C GHZ, SYZ Scenery.asm` |
| `$1D` | `id_MagicSwitch` | switch that activates when Sonic touches it (this is not used anywhere in the game) | any | Not ported | — | `1D Unused - Switch.asm` |
| `$1E` | `id_BallHog` | Ball Hog enemy (SBZ) | SBZ | Not ported | — | `1E, 20 Badnik - Ball Hog and Cannonball.asm` |
| `$1F` | `id_Crabmeat` | Crabmeat enemy (GHZ, SYZ) | GHZ, SYZ | **Ported** | `Crabmeat_Main` | `1F Badnik - Crabmeat.asm` |
| `$20` | `id_Cannonball` | cannonball that Ball Hog throws (SBZ) | SBZ | Not ported | — | `1E, 20 Badnik - Ball Hog and Cannonball.asm` |
| `$21` | `id_HUD` | SCORE, TIME, RINGS | any | **Ported** | `HUD_Main` | `21 HUD.asm` |
| `$22` | `id_BuzzBomber` | Buzz Bomber enemy (GHZ, MZ, SYZ) | GHZ, MZ, SYZ | **Ported** | `BuzzBomber_Main` | `22, 23 Badnik - Buzz Bomber and Missile.asm` |
| `$23` | `id_Missile` | Missile launched by Buzz Bomber and wall Newtron badniks | any | **Ported** | `Missile_Main` | `22, 23 Badnik - Buzz Bomber and Missile.asm` |
| `$24` | `id_UnusedExplosion` | Unused small explosion, originally used for the front-facing Ball Hog badnik from the prototype [...] | any | Not ported | — | `24 Unused - Small Explosion.asm` |
| `$25` | `id_Rings` | rings | any | **Ported** | `Ring_Main` | `25, 37 Rings.asm` |
| `$26` | `id_Monitor` | monitors | any | **Ported** | `Monitor_Main` | `26, 2E Monitors and Power-Ups.asm` |
| `$27` | `id_ExplosionItem` | Gray explosion from a destroyed enemy or monitor | any | **Ported** | `ExplosionItem_Main` | `27, 3F Explosions.asm` |
| `$28` | `id_Animals` | Animals from destroyed badniks, prison capsules, and ending | any | **Ported** | `Animals_Main` | `28, 29 Animals and Points.asm` |
| `$29` | `id_Points` | points that appear from destroyed badniks and other places | any | **Ported** | `Points_Main` | `28, 29 Animals and Points.asm` |
| `$2A` | `id_AutoDoor` | small vertical door (SBZ) | SBZ | Not ported | — | `2A SBZ Small Door.asm` |
| `$2B` | `id_Chopper` | Chopper enemy (GHZ) | GHZ | **Ported** | `Chopper_Main` | `2B Badnik - Chopper.asm` |
| `$2C` | `id_Jaws` | Jaws enemy (LZ) | LZ | Not ported | — | `2C Badnik - Jaws.asm` |
| `$2D` | `id_Burrobot` | Burrobot enemy (LZ) | LZ | Not ported | — | `2D Badnik - Burrobot.asm` |
| `$2E` | `id_PowerUp` | contents of monitors | any | **Ported** | `PowerUp_Main` | `26, 2E Monitors and Power-Ups.asm` |
| `$2F` | `id_LargeGrass` | large grass-covered platforms (MZ) | MZ | **Ported** | `LargeGrass_Main` | `2F, 35 MZ Large Grassy Platforms and Burning Grass.asm` |
| `$30` | `id_GlassBlock` | large green glass pillars (MZ) | MZ | **Ported** | `GlassBlock_Main` | `30 MZ Large Green Glass Blocks.asm` |
| `$31` | `id_ChainStomp` | stomping metal blocks on chains (MZ) | MZ | **Ported** | `ChainStomp_Main` | `31 MZ Chained Stompers.asm` |
| `$32` | `id_Button` | buttons/switches (MZ, SYZ, LZ, SBZ) | MZ, SYZ, LZ, SBZ | **Ported** | `Button_Main` | `32 Button.asm` |
| `$33` | `id_PushBlock` | pushable blocks (MZ, available but unused in LZ) | MZ, LZ | **Ported** | `PushBlock_Main` | `33 MZ, LZ Pushable Blocks.asm` |
| `$34` | `id_TitleCard` | Zone Title Cards | any | **Ported** | `TitleCard_Main` | `34 Title Cards.asm` |
| `$35` | `id_GrassFire` | fireball that sits on the floor (MZ) (appears when you walk on grass platforms with subtype $x5) | MZ | **Ported** | `GrassFire_Main` | `2F, 35 MZ Large Grassy Platforms and Burning Grass.asm` |
| `$36` | `id_Spikes` | Spikes | any | **Ported** | `Spikes_ObjectMain` | `36 Spikes.asm` |
| `$37` | `id_RingLoss` | rings flying out of Sonic when he's hit | any | **Ported** | `RingLoss_Main` | `25, 37 Rings.asm` |
| `$38` | `id_ShieldItem` | shield and invincibility stars | any | **Ported** | `ShieldItem_Main` | `38 Shield and Invincibility.asm` |
| `$39` | `id_GameOverCard` | "GAME OVER" and "TIME OVER" | any | **Ported** | `GameOverCard_Main` | `39 Game Over.asm` |
| `$3A` | `id_GotThroughCard` | "SONIC HAS PASSED" title card | any | **Ported** | `GotThroughCard_Main` | `3A Got Through Card.asm` |
| `$3B` | `id_PurpleRock` | purple rock (GHZ) | GHZ | **Ported** | `PurpleRock_Main` | `3B GHZ Purple Rock.asm` |
| `$3C` | `id_SmashWall` | smashable wall (GHZ, SLZ) | GHZ, SLZ | **Ported** | `SmashWall_Main` | `3C GHZ, SLZ Smashable Wall.asm` |
| `$3D` | `id_BossGreenHill` | Eggman (GHZ) - part 1 | GHZ | **Ported** | `BossGreenHill_Main` | `3D, 48 Boss - GHZ Main and Wrecking Ball.asm` |
| `$3E` | `id_Prison` | Prison capsule after boss fights | any | **Ported** | `Prison_Main` | `3E Prison Capsule.asm` |
| `$3F` | `id_Explosion` | Fiery explosion from destroyed boss, Walking Bomb badnik, or Ball Hog cannonball | any | **Ported** | `Explosion_Main` | `27, 3F Explosions.asm` |
| `$40` | `id_MotoBug` | Moto Bug enemy (GHZ) | GHZ | **Ported** | `MotoBug_Main` | `40 Badnik - Moto Bug.asm` |
| `$41` | `id_Springs` | springs | any | **Ported** | `Springs_ObjectMain` | `41 Springs.asm` |
| `$42` | `id_Newtron` | Newtron enemy (GHZ) | GHZ | **Ported** | `Newtron_Main` | `42 Badnik - Newtron.asm` |
| `$43` | `id_Roller` | Roller enemy (SYZ) | SYZ | **Ported** | `Roller_Main` | `43 Badnik - Roller.asm` |
| `$44` | `id_EdgeWalls` | edge walls (GHZ) | GHZ | **Ported** | `EdgeWalls_Main` | `44 GHZ Edge Walls.asm` |
| `$45` | `id_SideStomp` | unused sideways spiked metal stomper from beta version (MZ) | MZ | Not ported | — | `45 Unused - MZ Sideways Stomper.asm` |
| `$46` | `id_MarbleBrick` | solid blocks and blocks that fall from the ceiling (MZ) | MZ | **Ported** | `MarbleBrick_Main` | `46 MZ Bricks.asm` |
| `$47` | `id_Bumper` | pinball bumper (SYZ) | SYZ | **Ported** | `Bumper_Main` | `47 SYZ Bumper.asm` |
| `$48` | `id_BossBall` | wrecking ball on a chain that Eggman swings (GHZ) | GHZ | **Ported** | `BossBall_Main` | `3D, 48 Boss - GHZ Main and Wrecking Ball.asm` |
| `$49` | `id_WaterSound` | invisible waterfall sound effect trigger (GHZ) | GHZ | Not ported | — | `49 GHZ Waterfall Sound.asm` |
| `$4A` | `id_VanishSonic` | unused and unfinished Special Stage entry from beta | any | Not ported | — | `4A Unused - Special Stage Entry.asm` |
| `$4B` | `id_GiantRing` | Giant Ring for entry to Special Stage | any | **Ported** | `GiantRing_Main` | `4B, 7C Giant Ring and Flash.asm` |
| `$4C` | `id_GeyserMaker` | lava geyser / lavafall producer (MZ) | MZ | **Ported** | `GeyserMaker_Main` | `4C, 4D MZ Lava Geyser and Maker.asm` |
| `$4D` | `id_LavaGeyser` | lava geyser / lavafall (MZ) | MZ | **Ported** | `LavaGeyser_Main` | `4C, 4D MZ Lava Geyser and Maker.asm` |
| `$4E` | `id_LavaWall` | advancing wall of lava (MZ act 2) | MZ | **Ported** | `LavaWall_Main` | `4E MZ Wall of Lava.asm` |
| `$4F` | `id_Obj4F` | blank (This was the Splats badnik in the prototype) | any | Not ported | — | `4F Unused - Blank.asm` |
| `$50` | `id_Yadrin` | Yadrin enemy (MZ [unused], SYZ) | MZ, SYZ | **Ported** | `Yadrin_Main` | `50 Badnik - Yadrin.asm` |
| `$51` | `id_SmashBlock` | smashable green block (MZ) | MZ | **Ported** | `SmashBlock_Main` | `51 MZ Smashable Green Block.asm` |
| `$52` | `id_MovingBlock` | moving platform blocks (MZ, LZ, SBZ) | MZ, LZ, SBZ | **Ported** | `MovingBlock_Main` | `52 Moving Blocks.asm` |
| `$53` | `id_CollapseFloor` | collapsing floors (MZ, SLZ, SBZ) | MZ, SLZ, SBZ | **Ported** | `CollapseFloor_Main` | `1A, 53 Collapsing Ledges and Floors.asm` |
| `$54` | `id_LavaTag` | invisible lava tag / hurt marker (MZ) | MZ | **Ported** | `LavaTag_Main` | `54 MZ Invisible Lava Tag.asm` |
| `$55` | `id_Basaran` | Basaran enemy (MZ) | MZ | **Ported** | `Basaran_Main` | `55 Badnik - Basaran.asm` |
| `$56` | `id_FloatingBlock` | floating blocks (SYZ/SLZ), large doors (LZ) | SYZ, SLZ, LZ | **Ported** | `FloatingBlock_Main` | `56 SYZ, SLZ Floating Blocks and LZ Doors.asm` |
| `$57` | `id_SpikeBall` | spiked balls twirling on a chain (SYZ, LZ) | SYZ, LZ | **Ported** | `SpikeBall_Main` | `57 SYZ, LZ Spiked Ball and Chain.asm` |
| `$58` | `id_BigSpikeBall` | giant moving spiked metal balls (SYZ) | SYZ | **Ported** | `BigSpikeBall_Main` | `58 SYZ Big Spiked Ball.asm` |
| `$59` | `id_Elevator` | platforms that move when you stand on them (SLZ) | SLZ | Not ported | — | `59 SLZ Elevators.asm` |
| `$5A` | `id_CirclingPlatform` | platforms moving in circles (SLZ) | SLZ | Not ported | — | `5A SLZ Circling Platform.asm` |
| `$5B` | `id_Staircase` | blocks that form a staircase when touched (SLZ) | SLZ | Not ported | — | `5B SLZ Staircase.asm` |
| `$5C` | `id_Pylon` | metal pylons in foreground (SLZ) | SLZ | Not ported | — | `5C SLZ Foreground Pylon.asm` |
| `$5D` | `id_Fan` | fans (SLZ) | SLZ | Not ported | — | `5D SLZ Fan.asm` |
| `$5E` | `id_Seesaw` | seesaws (SLZ) | SLZ | Not ported | — | `5E SLZ Seesaw.asm` |
| `$5F` | `id_Bomb` | Walking Bomb enemy (SLZ, SBZ) | SLZ, SBZ | Not ported | — | `5F Badnik - Walking Bomb.asm` |
| `$60` | `id_Orbinaut` | Orbinaut enemy (LZ, SLZ, SBZ) | SLZ, LZ, SBZ | Not ported | — | `60 Badnik - Orbinaut.asm` |
| `$61` | `id_LabyrinthBlock` | multi-variant blocks (LZ) | LZ | Not ported | — | `61 LZ Blocks.asm` |
| `$62` | `id_Gargoyle` | gargoyle head that spits fireballs (LZ) | LZ | Not ported | — | `62 LZ Gargoyle.asm` |
| `$63` | `id_LabyrinthConvey` | platforms on a conveyor belt (LZ) | LZ | Not ported | — | `63 LZ Conveyor.asm` |
| `$64` | `id_Bubble` | air bubbles (LZ) | LZ | Not ported | — | `64 LZ Air Bubbles.asm` |
| `$65` | `id_Waterfall` | decorative waterfall objects (LZ) | LZ | Not ported | — | `65 LZ Waterfalls.asm` |
| `$66` | `id_Junction` | rotating disc junction that grabs Sonic (SBZ) | SBZ | Not ported | — | `66 SBZ Rotating Junction.asm` |
| `$67` | `id_RunningDisc` | disc that Sonic runs around (SBZ act 2) | SBZ | Not ported | — | `67 SBZ Running Disc.asm` |
| `$68` | `id_Conveyor` | conveyor belts (SBZ) | SBZ | Not ported | — | `68 SBZ Conveyor Belt.asm` |
| `$69` | `id_SpinPlatform` | stationary spinning platforms and trapdoors (SBZ) | SBZ | Not ported | — | `69 SBZ Spinning Platforms and Trapdoors.asm` |
| `$6A` | `id_Saws` | pizza cutters and speeding saws (SBZ) | SBZ | Not ported | — | `6A SBZ Saws and Pizza Cutters.asm` |
| `$6B` | `id_ScrapStomp` | stomper and sliding door (SBZ) and ancient lift at the start of SBZ3/LZ4 | SBZ | Not ported | — | `6B SBZ Stomper and Sliding Door.asm` |
| `$6C` | `id_VanishPlatform` | vanishing platforms (SBZ) | SBZ | Not ported | — | `6C SBZ Vanishing Platforms.asm` |
| `$6D` | `id_Flamethrower` | flame thrower (SBZ) | SBZ | Not ported | — | `6D SBZ Flamethrower.asm` |
| `$6E` | `id_Electro` | electrocution orbs (SBZ) | SBZ | Not ported | — | `6E SBZ Electrocuter.asm` |
| `$6F` | `id_SpinConvey` | spinning platforms that move around a conveyor belt (SBZ) | SBZ | Not ported | — | `6F SBZ Spin Platform Conveyor.asm` |
| `$70` | `id_Girder` | large girder block (SBZ) | SBZ | Not ported | — | `70 SBZ Girder Block.asm` |
| `$71` | `id_Invisibarrier` | invisible solid barriers | any | Not ported | — | `71 Invisible Solid Barriers.asm` |
| `$72` | `id_Teleport` | invisible teleporter system inside tubes (SBZ act 2) | SBZ | Not ported | — | `72 SBZ Teleporter.asm` |
| `$73` | `id_BossMarble` | Eggman (MZ) | MZ | **Ported** | `BossMarble_Main` | `73, 74 Boss - MZ Main and Fire.asm` |
| `$74` | `id_BossFire` | lava that Eggman drops (MZ) | MZ | **Ported** | `BossFire_Main` | `73, 74 Boss - MZ Main and Fire.asm` |
| `$75` | `id_BossSpringYard` | Eggman (SYZ) | SYZ | **Ported** | `BossSpringYard_Main` | `75, 76 Boss - SYZ Main and Blocks.asm` |
| `$76` | `id_BossBlock` | blocks that Eggman picks up (SYZ) | SYZ | **Ported** | `BossBlock_Main` | `75, 76 Boss - SYZ Main and Blocks.asm` |
| `$77` | `id_BossLabyrinth` | Eggman (LZ) | LZ | Not ported | — | `77 Boss - LZ Main.asm` |
| `$78` | `id_Caterkiller` | Caterkiller enemy (MZ, SBZ) | MZ, SBZ | **Ported** | `Caterkiller_Main` | `78 Badnik - Caterkiller.asm` |
| `$79` | `id_Lamppost` | lamppost | any | Not ported | — | `79 Lamppost.asm` |
| `$7A` | `id_BossStarLight` | Eggman (SLZ) | SLZ | Not ported | — | `7A, 7B Boss - SLZ Main and Spike Balls.asm` |
| `$7B` | `id_BossSpikeball` | exploding spike balls that Eggman drops (SLZ) | SLZ | Not ported | — | `7A, 7B Boss - SLZ Main and Spike Balls.asm` |
| `$7C` | `id_RingFlash` | Flash effect when you collect the Giant Ring | any | **Ported** | `RingFlash_Main` | `4B, 7C Giant Ring and Flash.asm` |
| `$7D` | `id_HiddenBonus` | hidden points at the end of a level | any | Not ported | — | `7D Hidden Bonuses.asm` |
| `$7E` | `id_SSResult` | Special Stage results screen | any | **Ported** | `SSResult_Main` | `7E, 7F Special Stage Results and Chaos Emeralds.asm` |
| `$7F` | `id_SSRChaos` | Chaos Emeralds from the Special Stage results screen | any | **Ported** | `SSRChaos_Main` | `7E, 7F Special Stage Results and Chaos Emeralds.asm` |
| `$80` | `id_ContScrItem` | Mini-Sonics on the Continue screen | any | Not ported | — | `80, 81 Continue Screen Elements and Sonic.asm` |
| `$81` | `id_ContSonic` | Sonic on the Continue screen | any | Not ported | — | `80, 81 Continue Screen Elements and Sonic.asm` |
| `$82` | `id_ScrapEggman` | Eggman (SBZ2) | SBZ | Not ported | — | `82, 83 SBZ Eggman Cutscene and Crumbling Floor.asm` |
| `$83` | `id_FalseFloor` | blocks that disintegrate Eggman presses a switch (SBZ2) | SBZ | Not ported | — | `82, 83 SBZ Eggman Cutscene and Crumbling Floor.asm` |
| `$84` | `id_EggmanCylinder` | cylinder Eggman hides in (FZ) | FZ | Not ported | — | `85,84,86 Boss - FZ Main, Cylinders, and Plasma Balls.asm` |
| `$85` | `id_BossFinal` | Eggman (FZ) | FZ | Not ported | — | `85,84,86 Boss - FZ Main, Cylinders, and Plasma Balls.asm` |
| `$86` | `id_BossPlasma` | energy balls (FZ) | FZ | Not ported | — | `85,84,86 Boss - FZ Main, Cylinders, and Plasma Balls.asm` |
| `$87` | `id_EndSonic` | Sonic on ending sequence. (Note that this object works strongly in tandem with End_MoveSonic.) | any | Not ported | — | `87, 88, 89 Ending Sequence Sonic, Emeralds, Logo.asm` |
| `$88` | `id_EndChaos` | chaos emeralds on the ending sequence | any | Not ported | — | `87, 88, 89 Ending Sequence Sonic, Emeralds, Logo.asm` |
| `$89` | `id_EndSTH` | "SONIC THE HEDGEHOG" text on the ending sequence. | any | Not ported | — | `87, 88, 89 Ending Sequence Sonic, Emeralds, Logo.asm` |
| `$8A` | `id_CreditsText` | "SONIC TEAM PRESENTS" and credits | any | **Ported** | `CreditsText_Main` | `8A Credits and Sonic Team Presents.asm` |
| `$8B` | `id_EndEggman` | Eggman on "TRY AGAIN" and "END" screens | any | Not ported | — | `8B, 8C Try Again, End Eggman, End Emeralds.asm` |
| `$8C` | `id_TryChaos` | chaos emeralds on the "TRY AGAIN" screen | any | Not ported | — | `8B, 8C Try Again, End Eggman, End Emeralds.asm` |

## Not ported, by zone

The reachable ones, so the remaining work is visible. IDs marked *unused* in the
disassembly are listed separately below and need no port: the game never places
them.

### Green Hill (GHZ) — 1 pending

| ID | `id_*` | Description | Disasm source |
|---|---|---|---|
| `$49` | `id_WaterSound` | invisible waterfall sound effect trigger (GHZ) | `49 GHZ Waterfall Sound.asm` |

### Star Light (SLZ) — 10 pending

| ID | `id_*` | Description | Disasm source |
|---|---|---|---|
| `$59` | `id_Elevator` | platforms that move when you stand on them (SLZ) | `59 SLZ Elevators.asm` |
| `$5A` | `id_CirclingPlatform` | platforms moving in circles (SLZ) | `5A SLZ Circling Platform.asm` |
| `$5B` | `id_Staircase` | blocks that form a staircase when touched (SLZ) | `5B SLZ Staircase.asm` |
| `$5C` | `id_Pylon` | metal pylons in foreground (SLZ) | `5C SLZ Foreground Pylon.asm` |
| `$5D` | `id_Fan` | fans (SLZ) | `5D SLZ Fan.asm` |
| `$5E` | `id_Seesaw` | seesaws (SLZ) | `5E SLZ Seesaw.asm` |
| `$5F` | `id_Bomb` | Walking Bomb enemy (SLZ, SBZ) | `5F Badnik - Walking Bomb.asm` |
| `$60` | `id_Orbinaut` | Orbinaut enemy (LZ, SLZ, SBZ) | `60 Badnik - Orbinaut.asm` |
| `$7A` | `id_BossStarLight` | Eggman (SLZ) | `7A, 7B Boss - SLZ Main and Spike Balls.asm` |
| `$7B` | `id_BossSpikeball` | exploding spike balls that Eggman drops (SLZ) | `7A, 7B Boss - SLZ Main and Spike Balls.asm` |

### Labyrinth (LZ) — 15 pending

| ID | `id_*` | Description | Disasm source |
|---|---|---|---|
| `$08` | `id_Splash` | water splash (LZ) | `08 LZ Water Splash.asm` |
| `$0A` | `id_DrownCount` | drowning countdown numbers and small bubbles that float out of Sonic's mouth (LZ) | `0A LZ Drowning Countdown.asm` |
| `$0B` | `id_Pole` | breakable pole in wind tunnels that Sonic hangs onto (LZ) | `0B LZ Pole that Breaks.asm` |
| `$0C` | `id_FlapDoor` | flapping door before wind tunnels (LZ) | `0C LZ Flapping Door.asm` |
| `$16` | `id_Harpoon` | harpoon (LZ) | `16 LZ Harpoon.asm` |
| `$1B` | `id_WaterSurface` | water surface (LZ) (Two objects are loaded, one for the left and one for the right side.) | `1B LZ Water Surface.asm` |
| `$2C` | `id_Jaws` | Jaws enemy (LZ) | `2C Badnik - Jaws.asm` |
| `$2D` | `id_Burrobot` | Burrobot enemy (LZ) | `2D Badnik - Burrobot.asm` |
| `$60` | `id_Orbinaut` | Orbinaut enemy (LZ, SLZ, SBZ) | `60 Badnik - Orbinaut.asm` |
| `$61` | `id_LabyrinthBlock` | multi-variant blocks (LZ) | `61 LZ Blocks.asm` |
| `$62` | `id_Gargoyle` | gargoyle head that spits fireballs (LZ) | `62 LZ Gargoyle.asm` |
| `$63` | `id_LabyrinthConvey` | platforms on a conveyor belt (LZ) | `63 LZ Conveyor.asm` |
| `$64` | `id_Bubble` | air bubbles (LZ) | `64 LZ Air Bubbles.asm` |
| `$65` | `id_Waterfall` | decorative waterfall objects (LZ) | `65 LZ Waterfalls.asm` |
| `$77` | `id_BossLabyrinth` | Eggman (LZ) | `77 Boss - LZ Main.asm` |

### Sandopolis (SBZ) — 19 pending

| ID | `id_*` | Description | Disasm source |
|---|---|---|---|
| `$1E` | `id_BallHog` | Ball Hog enemy (SBZ) | `1E, 20 Badnik - Ball Hog and Cannonball.asm` |
| `$20` | `id_Cannonball` | cannonball that Ball Hog throws (SBZ) | `1E, 20 Badnik - Ball Hog and Cannonball.asm` |
| `$2A` | `id_AutoDoor` | small vertical door (SBZ) | `2A SBZ Small Door.asm` |
| `$5F` | `id_Bomb` | Walking Bomb enemy (SLZ, SBZ) | `5F Badnik - Walking Bomb.asm` |
| `$60` | `id_Orbinaut` | Orbinaut enemy (LZ, SLZ, SBZ) | `60 Badnik - Orbinaut.asm` |
| `$66` | `id_Junction` | rotating disc junction that grabs Sonic (SBZ) | `66 SBZ Rotating Junction.asm` |
| `$67` | `id_RunningDisc` | disc that Sonic runs around (SBZ act 2) | `67 SBZ Running Disc.asm` |
| `$68` | `id_Conveyor` | conveyor belts (SBZ) | `68 SBZ Conveyor Belt.asm` |
| `$69` | `id_SpinPlatform` | stationary spinning platforms and trapdoors (SBZ) | `69 SBZ Spinning Platforms and Trapdoors.asm` |
| `$6A` | `id_Saws` | pizza cutters and speeding saws (SBZ) | `6A SBZ Saws and Pizza Cutters.asm` |
| `$6B` | `id_ScrapStomp` | stomper and sliding door (SBZ) and ancient lift at the start of SBZ3/LZ4 | `6B SBZ Stomper and Sliding Door.asm` |
| `$6C` | `id_VanishPlatform` | vanishing platforms (SBZ) | `6C SBZ Vanishing Platforms.asm` |
| `$6D` | `id_Flamethrower` | flame thrower (SBZ) | `6D SBZ Flamethrower.asm` |
| `$6E` | `id_Electro` | electrocution orbs (SBZ) | `6E SBZ Electrocuter.asm` |
| `$6F` | `id_SpinConvey` | spinning platforms that move around a conveyor belt (SBZ) | `6F SBZ Spin Platform Conveyor.asm` |
| `$70` | `id_Girder` | large girder block (SBZ) | `70 SBZ Girder Block.asm` |
| `$72` | `id_Teleport` | invisible teleporter system inside tubes (SBZ act 2) | `72 SBZ Teleporter.asm` |
| `$82` | `id_ScrapEggman` | Eggman (SBZ2) | `82, 83 SBZ Eggman Cutscene and Crumbling Floor.asm` |
| `$83` | `id_FalseFloor` | blocks that disintegrate Eggman presses a switch (SBZ2) | `82, 83 SBZ Eggman Cutscene and Crumbling Floor.asm` |

### Final (FZ) — 3 pending

| ID | `id_*` | Description | Disasm source |
|---|---|---|---|
| `$84` | `id_EggmanCylinder` | cylinder Eggman hides in (FZ) | `85,84,86 Boss - FZ Main, Cylinders, and Plasma Balls.asm` |
| `$85` | `id_BossFinal` | Eggman (FZ) | `85,84,86 Boss - FZ Main, Cylinders, and Plasma Balls.asm` |
| `$86` | `id_BossPlasma` | energy balls (FZ) | `85,84,86 Boss - FZ Main, Cylinders, and Plasma Balls.asm` |

### No zone named in the disassembly — 10 pending

| ID | `id_*` | Description | Disasm source |
|---|---|---|---|
| `$71` | `id_Invisibarrier` | invisible solid barriers | `71 Invisible Solid Barriers.asm` |
| `$79` | `id_Lamppost` | lamppost | `79 Lamppost.asm` |
| `$7D` | `id_HiddenBonus` | hidden points at the end of a level | `7D Hidden Bonuses.asm` |
| `$80` | `id_ContScrItem` | Mini-Sonics on the Continue screen | `80, 81 Continue Screen Elements and Sonic.asm` |
| `$81` | `id_ContSonic` | Sonic on the Continue screen | `80, 81 Continue Screen Elements and Sonic.asm` |
| `$87` | `id_EndSonic` | Sonic on ending sequence. (Note that this object works strongly in tandem with End_MoveSonic.) | `87, 88, 89 Ending Sequence Sonic, Emeralds, Logo.asm` |
| `$88` | `id_EndChaos` | chaos emeralds on the ending sequence | `87, 88, 89 Ending Sequence Sonic, Emeralds, Logo.asm` |
| `$89` | `id_EndSTH` | "SONIC THE HEDGEHOG" text on the ending sequence. | `87, 88, 89 Ending Sequence Sonic, Emeralds, Logo.asm` |
| `$8B` | `id_EndEggman` | Eggman on "TRY AGAIN" and "END" screens | `8B, 8C Try Again, End Eggman, End Emeralds.asm` |
| `$8C` | `id_TryChaos` | chaos emeralds on the "TRY AGAIN" screen | `8B, 8C Try Again, End Eggman, End Emeralds.asm` |

## Unused objects (no port needed)

The disassembly flags these as leftovers from the prototype. Nothing places
them in any act, so they can stay unimplemented.

| ID | `id_*` | Description | Disasm source |
|---|---|---|---|
| `$10` | `id_Obj10` | blank (This was a Sonic animation test object in the prototype) | `10 Unused - Blank.asm` |
| `$19` | `id_Obj19` | blank (This was the infamous rolling GHZ ball level obstacle in the prototype) | `19 Unused - Blank.asm` |
| `$1D` | `id_MagicSwitch` | switch that activates when Sonic touches it (this is not used anywhere in the game) | `1D Unused - Switch.asm` |
| `$24` | `id_UnusedExplosion` | Unused small explosion, originally used for the front-facing Ball Hog badnik from the prototype [...] | `24 Unused - Small Explosion.asm` |
| `$45` | `id_SideStomp` | unused sideways spiked metal stomper from beta version (MZ) | `45 Unused - MZ Sideways Stomper.asm` |
| `$4A` | `id_VanishSonic` | unused and unfinished Special Stage entry from beta | `4A Unused - Special Stage Entry.asm` |
| `$4F` | `id_Obj4F` | blank (This was the Splats badnik in the prototype) | `4F Unused - Blank.asm` |

## Shared object subroutines

`disasm/_incObj/sub/` holds the routines every object calls. They live in
`src/objects.c` Part 1, next to the objects that use them, except where a
neighbouring module already owned the code.

| Disasm source | In this port | Where |
|---|---|---|
| `sub AddPoints.asm` | Ported | `src/objects.c` |
| `sub AnimateSprite.asm` | Ported | `src/objects.c` |
| `sub BossDefeated & BossMove.asm` | Ported | `src/objects.c` |
| `sub CalcAngle.asm` | Ported | `src/collision.c` |
| `sub CalcSine.asm` | Ported | `src/objects.c` |
| `sub CalcSqrt.asm` | Not ported | — (REV00 only, unused in this disasm) |
| `sub ChkObjectVisible.asm` | Ported | `src/objects.c` |
| `sub DeleteObject.asm` | Ported | `src/objects.c` |
| `sub DisplaySprite.asm` | Ported | `src/objects.c` |
| `sub ExitPlatform.asm` | Ported | `src/objects.c` |
| `sub FindFreeObj.asm` | Ported | `src/objects.c` |
| `sub FindNearestTile & FindFloor & FindWall.asm` | Ported | `src/collision.c` |
| `sub MvSonicOnPtfm.asm` | Ported | `src/objects.c` |
| `sub ObjectFall & SpeedToPos.asm` | Ported | `src/objects.c` |
| `sub ObjFloorDist.asm` | Ported | `src/objects.c` |
| `sub PlatformObject & SlopeObject.asm` | Ported | `src/objects.c` |
| `sub RandomNumber.asm` | Ported | `src/objects.c` |
| `sub RememberState.asm` | Ported | `src/objects.c` |
| `sub ResumeMusic.asm` | Ported | `src/objects.c` |
| `sub SmashObject.asm` | Ported | `src/objects.c` |
| `sub SolidObject.asm` | Ported | `src/objects.c` |
| `sub SolidWall.asm` | Ported | `src/objects.c` |

`sub ChkObjectVisible.asm` holds two entry points: `ChkObjectVisible`, which is
inlined at its only ported call site (Object 13, `LavaMaker_Main`), and
`ChkPartiallyVisible`, which is a real function in `src/objects.c` used by
Object 32. `sub SolidWall.asm` needs no separate port because the ASM entry
point is itself named `EdgeWall_SolidWall`.

## Sonic object routines

These are not separate objects: they are the routines that make up the Sonic
object itself (`$01`, `$09`) plus the debug mode. They are ported, so their
absence from the tables above is not a gap.

| Disasm source | In this port | Where |
|---|---|---|
| `Sonic AnglePos.asm` | Ported | `src/collision.c` (`Sonic_AnglePos`) |
| `Sonic Collision.asm` | Ported | `src/collision.c`, `src/objects.c` |
| `Sonic ReactToItem.asm` | Ported | `src/objects.c` (`KillSonic`, `HurtSonic`, `ReactToItem`) |
| `DebugMode.asm` | Partially ported | `src/debugmode.c` (see the note below) |

The one gap in `DebugMode.asm` is graphics, not logic: `src/data.c` still
exposes most of the debug mappings as unloaded `Map_*` pointers, so choosing an
item whose art is missing shows an empty sprite.

## Adding an object

1. Translate the routine from its `disasm/_incObj/XX *.asm` file into
   `src/objects.c`, in the section for that object ID, keeping the
   `obRoutine` dispatcher shape (`static void Xxx_Main(void *obj)` that
   `switch`es on `obRoutine(o)`).
2. Declare the dispatcher in the forward-declaration block at the top of
   `src/objects.c` if it is not self-contained.
3. Add one row to `obj_map[]`, in ascending ID order, with the ID and the
   disasm file name in the trailing comment.
4. Rebuild, then update this file: move the ID out of the pending table for
   its zone and into the ported rows above. The tables are meant to stay
   consistent with `obj_map[]`, so they are edited by hand on purpose.

