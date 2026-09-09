#ifndef SONIC1_CONSTANTS_H
#define SONIC1_CONSTANTS_H

#include "types.h"

/* ===========================================================================
   Constants — translated from _Constants.asm
   =========================================================================== */

/* ---------------------------------------------------------------------------
   Driver sizes
   --------------------------------------------------------------------------- */
#define Size_of_SegaPCM             0x6978
#define Size_of_DAC_driver_guess    0x1760

/* ---------------------------------------------------------------------------
   Clocks
   --------------------------------------------------------------------------- */
#define Master_Clock        53693175
#define M68000_Clock        (Master_Clock / 7)
#define Z80_Clock           (Master_Clock / 15)
#define FM_Sample_Rate      (M68000_Clock / (6 * 6 * 4))
#define PSG_Sample_Rate     (Z80_Clock / 16)

/* ---------------------------------------------------------------------------
   VDP addresses
   --------------------------------------------------------------------------- */
#define vdp_data_port       0xC00000
#define vdp_control_port    0xC00004
#define vdp_counter         0xC00008
#define psg_input           0xC00011
#define debug_reg           0xC0001C

/* ---------------------------------------------------------------------------
   Z80 / YM2612 addresses
   --------------------------------------------------------------------------- */
#define z80_ram             0xA00000
#define z80_dac3_pitch      0xA000EA
#define z80_dac_status      0xA01FFD
#define z80_dac_sample      0xA01FFF
#define z80_ram_end         0xA02000
#define ym2612_a0           0xA04000
#define ym2612_d0           0xA04001
#define ym2612_a1           0xA04002
#define ym2612_d1           0xA04003
#define z80_68k_bank        0xA06000
#define psg_input_z80       0xA07F11
#define z80_68k             0xA08000
#define z80_68k_end         0xA10000
#define z80_bus_request     0xA11100
#define z80_reset           0xA11200

/* ---------------------------------------------------------------------------
   I/O addresses
   --------------------------------------------------------------------------- */
#define console_version     0xA10001
#define port_1_data_hi      0xA10002
#define port_1_data         0xA10003
#define port_2_data_hi      0xA10004
#define port_2_data         0xA10005
#define port_1_control_hi   0xA10008
#define port_1_control      0xA10009
#define port_2_control_hi   0xA1000A
#define port_2_control      0xA1000B
#define expansion_control_hi 0xA1000C
#define expansion_control   0xA1000D
#define serial_1_tx_hi      0xA1000E
#define serial_1_tx         0xA1000F
#define serial_1_rx_hi      0xA10010
#define serial_1_rx         0xA10011
#define serial_1_control_hi 0xA10012
#define serial_1_control    0xA10013
#define serial_2_tx_hi      0xA10014
#define serial_2_tx         0xA10015
#define serial_2_rx_hi      0xA10016
#define serial_2_rx         0xA10017
#define serial_2_control_hi 0xA10018
#define serial_2_control    0xA10019
#define serial_exp_tx_hi    0xA1001A
#define serial_exp_tx       0xA1001B
#define serial_exp_rx_hi    0xA1001C
#define serial_exp_rx       0xA1001D
#define serial_exp_control_hi 0xA1001E
#define serial_exp_control  0xA1001F

/* ---------------------------------------------------------------------------
   Misc addresses
   --------------------------------------------------------------------------- */
#define memory_mode         0xA11000
#define mars_connection     0xA130EC
#define sram_port           0xA130F1
#define mapper_bank_1       0xA130F3
#define mapper_bank_2       0xA130F5
#define mapper_bank_3       0xA130F7
#define mapper_bank_4       0xA130F9
#define mapper_bank_5       0xA130FB
#define mapper_bank_6       0xA130FD
#define mapper_bank_7       0xA130FF
#define security_addr       0xA14000
#define security_flag       0xA14101

/* ---------------------------------------------------------------------------
   SRAM
   --------------------------------------------------------------------------- */
/* sram_start depends on AddressSRAM configuration.
   Standard Sonic 1: AddressSRAM = $A130F1, so bit 0 is set. */
#define sram_start          0x200001
#define sram_end            0x210000

/* ---------------------------------------------------------------------------
   VDP registers
   --------------------------------------------------------------------------- */
#define vreg_mode1          0x8000
#define vreg_mode2          0x8100
#define vreg_fgvram         0x8200
#define vreg_winvram        0x8300
#define vreg_bgvram         0x8400
#define vreg_spritevram     0x8500
#define vreg_bgcolor        0x8700
#define vreg_hintrate       0x8A00
#define vreg_mode3          0x8B00
#define vreg_mode4          0x8C00
#define vreg_hscrollvram    0x8D00
#define vreg_autoinc        0x8F00
#define vreg_planesize      0x9000
#define vreg_winxpos        0x9100
#define vreg_winypos        0x9200
#define vreg_dmalen         0x94009300
#define vreg_dmasrc         0x96009500
#define vreg_dmamode        0x9700

/* ---------------------------------------------------------------------------
   VRAM data
   --------------------------------------------------------------------------- */
#define vram_fg             0xC000
#define vram_win            0xA000
#define vram_bg             0xE000
#define vram_sprites        0xF800
#define vram_hscroll        0xFC00

/* ---------------------------------------------------------------------------
   Sprite data
   --------------------------------------------------------------------------- */
#define sprites_max             80
#define spritelayer_num         (1 << 3)
#define spritelayer_size_bits   7
#define spritelayer_size        (1 << spritelayer_size_bits)
#define spritetable_entrysize   8

/* ---------------------------------------------------------------------------
   Various sizes
   --------------------------------------------------------------------------- */
#define tile_size               (8 * 8 / 2)
#define chunk_size              0x200
#define plane_size_64x32        (64 * 32 * 2)

#define layout_row_interlaced   0x40
#define layout_row              (layout_row_interlaced * 2)

/* ---------------------------------------------------------------------------
   Zone IDs
   --------------------------------------------------------------------------- */
enum {
    id_GHZ  = 0,
    id_LZ   = 1,
    id_MZ   = 2,
    id_SLZ  = 3,
    id_SYZ  = 4,
    id_SBZ  = 5,
    id_EndZ = 6,
    id_SS   = 7
};

/* ---------------------------------------------------------------------------
   Act IDs
   --------------------------------------------------------------------------- */
enum {
    act1 = 0,
    act2 = 1,
    act3 = 2,
    act4 = 3
};

/* ---------------------------------------------------------------------------
   Zone / Act combined IDs
   --------------------------------------------------------------------------- */
#define id_GHZ_act1    ((id_GHZ  << 8) + act1)
#define id_GHZ_act2    ((id_GHZ  << 8) + act2)
#define id_GHZ_act3    ((id_GHZ  << 8) + act3)

#define id_LZ_act1     ((id_LZ   << 8) + act1)
#define id_LZ_act2     ((id_LZ   << 8) + act2)
#define id_LZ_act3     ((id_LZ   << 8) + act3)
#define id_LZ_act4     ((id_LZ   << 8) + act4)  /* SBZ3 */

#define id_MZ_act1     ((id_MZ   << 8) + act1)
#define id_MZ_act2     ((id_MZ   << 8) + act2)
#define id_MZ_act3     ((id_MZ   << 8) + act3)

#define id_SLZ_act1    ((id_SLZ  << 8) + act1)
#define id_SLZ_act2    ((id_SLZ  << 8) + act2)
#define id_SLZ_act3    ((id_SLZ  << 8) + act3)

#define id_SYZ_act1    ((id_SYZ  << 8) + act1)
#define id_SYZ_act2    ((id_SYZ  << 8) + act2)
#define id_SYZ_act3    ((id_SYZ  << 8) + act3)

#define id_SBZ_act1    ((id_SBZ  << 8) + act1)
#define id_SBZ_act2    ((id_SBZ  << 8) + act2)

#define id_FZ          ((id_SBZ  << 8) + act3)  /* real SBZ3 / Final Zone */

#define id_EndZ_good   ((id_EndZ << 8) + act1)
#define id_EndZ_bad    ((id_EndZ << 8) + act2)

/* ---------------------------------------------------------------------------
   Special Stage
   --------------------------------------------------------------------------- */
#define ss_emeralds_num     6
#define ss_giantring_rings  50
#define ss_continue_rings   50
#define ss_rotatespeed      0x40
#define ss_timeout          30
#define ss_blocksize        24

/* ---------------------------------------------------------------------------
   Common colors (BGR nibble format)
   --------------------------------------------------------------------------- */
#define cBlack      0x000
#define cWhite      0xEEE
#define cBlue       0xE00
#define cGreen      0x0E0
#define cRed        0x00E
#define cYellow     (cGreen + cRed)
#define cAqua       (cGreen + cBlue)
#define cMagenta    (cBlue + cRed)

/* ---------------------------------------------------------------------------
   Joypad input — bit positions
   --------------------------------------------------------------------------- */
#define bitUp       0
#define bitDn       1
#define bitL        2
#define bitR        3
#define bitB        4
#define bitC        5
#define bitA        6
#define bitStart    7

/* Joypad input — button masks */
#define btnUp       (1 << bitUp)
#define btnDn       (1 << bitDn)
#define btnL        (1 << bitL)
#define btnR        (1 << bitR)
#define btnB        (1 << bitB)
#define btnC        (1 << bitC)
#define btnA        (1 << bitA)
#define btnStart    (1 << bitStart)
#define btnDir      (btnUp | btnDn | btnL | btnR)
#define btnABC      (btnA  | btnB  | btnC)

/* ---------------------------------------------------------------------------
   Sprite render flags (obRender / BuildSprites)
   --------------------------------------------------------------------------- */
#define sprite_xflip_bit        0
#define sprite_yflip_bit        1
#define sprite_cam_field_bit    2
#define sprite_cam_bg_bit       3
#define sprite_customheight_bit 4
#define sprite_rawmappings_bit  5
#define sprite_looping_bit      6
#define sprite_rendered_bit     7

#define sprite_xflip        (1 << sprite_xflip_bit)
#define sprite_yflip        (1 << sprite_yflip_bit)
#define sprite_cam_screen   0
#define sprite_cam_field    (1 << sprite_cam_field_bit)
#define sprite_cam_bg       (1 << sprite_cam_bg_bit)
#define sprite_customheight (1 << sprite_customheight_bit)
#define sprite_rawmappings  (1 << sprite_rawmappings_bit)
#define sprite_looping      (1 << sprite_looping_bit)
#define sprite_rendered     (1 << sprite_rendered_bit)

/* ---------------------------------------------------------------------------
   Collision types (obColType) — Sonic ReactToItem.asm
   Hitbox sizes are stored as box extents; col_?x? values are hitbox indexes
   --------------------------------------------------------------------------- */
#define col_none            0x00  /* marker for no-collision objects */
#define col_badnik          0x00  /* destroyable badniks */
#define col_boss            0x00  /* Eggman bosses */
#define col_item            0x40  /* monitors, rings, giant rings */
#define col_hurt            0x80  /* damaging objects when touched */
#define col_special         0xC0  /* objects with special collision properties */

#define col_40x40           0x01  /* GHZ ball */
#define col_24x40           0x02  /* (unused) */
#define col_40x24           0x03  /* (unused) */
#define col_8x32            0x04  /* GHZ spike pole, SYZ boss spike */
#define col_24x36           0x05  /* Ball Hog, Burrobot */
#define col_32x32           0x06  /* Crabmeat, Monitor, SBZ spikeball, Prison */
#define col_12x12           0x07  /* Cannonball, Crab/Buzz missile, Ring */
#define col_48x24           0x08  /* Buzz Bomber */
#define col_24x32           0x09  /* Chopper */
#define col_32x24           0x0A  /* Jaws */
#define col_16x16           0x0B  /* MZ fire, Fireball, Batbrain, LZ spikeball */
#define col_40x32           0x0C  /* Newtron, Motobug, Yadrin */
#define col_40x16           0x0D  /* Newtron */
#define col_28x28           0x0E  /* Roller */
#define col_48x48           0x0F  /* Bosses */
#define col_80x32           0x10  /* MZ vertical stomper */
#define col_32x48           0x11  /* MZ sideways stomper */
#define col_16x32           0x12  /* Giant ring */
#define col_64x224          0x13  /* MZ geyser */

/* ---------------------------------------------------------------------------
   Object variables — byte offsets into the 64-byte object structure
   --------------------------------------------------------------------------- */
#define obID            0
#define obRender        1
#define obGfx           2
#define obMap           4
#define obX             8
#define obSubpixelX     0xA
#define obScreenY       obSubpixelX
#define obY             0xC
#define obSubpixelY     0xE
#define obVelX          0x10
#define obVelY          0x12
#define obInertia       0x14
#define obHeight        0x16
#define obWidth         0x17
#define obPriority      0x18
#define obActWid        0x19
#define obFrame         0x1A
#define obAniFrame      0x1B
#define obAnim          0x1C
#define obPrevAni       0x1D
#define obTimeFrame     0x1E
#define obDelayAni      0x1F
#define obColType       0x20
#define obColProp       0x21
#define obStatus        0x22
#define obRespawnNo     0x23
#define obRoutine       0x24
#define ob2ndRout       0x25
#define obSolid         ob2ndRout
#define obAngle         0x26
#define obSubtype       0x28

/* ---------------------------------------------------------------------------
   Sonic-specific object variables
   --------------------------------------------------------------------------- */
#define flashtime       0x30
#define invtime         0x32
#define shoetime        0x34
#define angleright      0x36
#define angleleft       0x37
#define sticktoconvex   0x38
#define restartime      0x3A
#define jumping         0x3C
#define standonobject   0x3D
#define locktime        0x3E

/* ---------------------------------------------------------------------------
   Sonic's collision sizes
   --------------------------------------------------------------------------- */
#define sonic_width         (18 / 2)
#define sonic_height        (38 / 2)
#define sonic_roll_width    (14 / 2)
#define sonic_roll_height   (28 / 2)
#define sonic_solid_width   (22 / 2)
#define sonic_react_width   (16 / 2)
#define sonic_duck_height   (20 / 2)
#define sonic_quick_size    (20 / 2)

/* Sonic physics constants (from 01 Sonic.asm lines 5-8 and ObjectFall.asm line 5) */
#define son_maxspeed        0x600
#define son_acceleration    0x0C
#define son_deceleration    0x80
#define son_jumpspeed       0x680
#define gravity             0x38

/* ---------------------------------------------------------------------------
   Miscellaneous object scratch-RAM offsets
   --------------------------------------------------------------------------- */
#define objoff_29   0x29
#define objoff_2A   0x2A
#define objoff_2B   0x2B
#define objoff_2C   0x2C
#define objoff_2E   0x2E
#define objoff_2F   0x2F
#define objoff_30   0x30
#define objoff_31   0x31
#define objoff_32   0x32
#define objoff_33   0x33
#define objoff_34   0x34
#define objoff_35   0x35
#define objoff_36   0x36
#define objoff_37   0x37
#define objoff_38   0x38
#define objoff_39   0x39
#define objoff_3A   0x3A
#define objoff_3B   0x3B
#define objoff_3C   0x3C
#define objoff_3D   0x3D
#define objoff_3E   0x3E
#define objoff_3F   0x3F

/* ---------------------------------------------------------------------------
   Boss variables (aliases into generic object offsets)
   --------------------------------------------------------------------------- */
#define obBossHits   obColProp
#define obBossX      objoff_30
#define obBossY      objoff_38
#define obBossFlash  objoff_3E

/* ---------------------------------------------------------------------------
   Object size
   --------------------------------------------------------------------------- */
#define object_size_bits    6
#define object_size         (1 << object_size_bits)

/* ---------------------------------------------------------------------------
   Animation flags
   --------------------------------------------------------------------------- */
#define afEnd           0xFF
#define afBack          0xFE
#define afChange        0xFD
#define afRoutine       0xFC
#define afReset         0xFB
#define af2ndRoutine    0xFA

#define aniXFlip        0x20
#define aniYFlip        0x40

/* ---------------------------------------------------------------------------
   Background music IDs
   --------------------------------------------------------------------------- */
#define bgm__First      0x81
#define bgm_GHZ         0x81
#define bgm_LZ          0x82
#define bgm_MZ          0x83
#define bgm_SLZ         0x84
#define bgm_SYZ         0x85
#define bgm_SBZ         0x86
#define bgm_Invincible  0x87
#define bgm_ExtraLife   0x88
#define bgm_SS          0x89
#define bgm_Title       0x8A
#define bgm_Ending      0x8B
#define bgm_Boss        0x8C
#define bgm_FZ          0x8D
#define bgm_GotThrough  0x8E
#define bgm_GameOver    0x8F
#define bgm_Continue    0x90
#define bgm_Credits     0x91
#define bgm_Drowning    0x92
#define bgm_Emerald     0x93
#define bgm__Last       0x93

/* ---------------------------------------------------------------------------
   Sound effect IDs
   --------------------------------------------------------------------------- */
#define sfx__First      0xA0
#define sfx_Jump        0xA0
#define sfx_Lamppost    0xA1
#define sfx_A2          0xA2
#define sfx_Death       0xA3
#define sfx_Skid        0xA4
#define sfx_A5          0xA5
#define sfx_HitSpikes   0xA6
#define sfx_Push        0xA7
#define sfx_SSGoal      0xA8
#define sfx_SSItem      0xA9
#define sfx_Splash      0xAA
#define sfx_AB          0xAB
#define sfx_HitBoss     0xAC
#define sfx_Bubble      0xAD
#define sfx_Fireball    0xAE
#define sfx_Shield      0xAF
#define sfx_Saw         0xB0
#define sfx_Electric    0xB1
#define sfx_Drown       0xB2
#define sfx_Flamethrower 0xB3
#define sfx_Bumper      0xB4
#define sfx_Ring        0xB5
#define sfx_SpikesMove  0xB6
#define sfx_Rumbling    0xB7
#define sfx_B8          0xB8
#define sfx_Collapse    0xB9
#define sfx_SSGlass     0xBA
#define sfx_Door        0xBB
#define sfx_Teleport    0xBC
#define sfx_ChainStomp  0xBD
#define sfx_Roll        0xBE
#define sfx_Continue    0xBF
#define sfx_Basaran     0xC0
#define sfx_BreakItem   0xC1
#define sfx_Warning     0xC2
#define sfx_GiantRing   0xC3
#define sfx_Bomb        0xC4
#define sfx_Cash        0xC5
#define sfx_RingLoss    0xC6
#define sfx_ChainRise   0xC7
#define sfx_Burning     0xC8
#define sfx_Bonus       0xC9
#define sfx_EnterSS     0xCA
#define sfx_WallSmash   0xCB
#define sfx_Spring      0xCC
#define sfx_Switch      0xCD
#define sfx_RingLeft    0xCE
#define sfx_Signpost    0xCF
#define sfx__Last       0xCF

/* ---------------------------------------------------------------------------
   Special sound effects
   --------------------------------------------------------------------------- */
#define spec__First     0xD0
#define sfx_Waterfall   0xD0
#define spec__Last      0xD0

/* ---------------------------------------------------------------------------
   Sound commands
   --------------------------------------------------------------------------- */
#define flg__First      0xE0
#define bgm_Fade        0xE0
#define sfx_Sega        0xE1
#define bgm_Speedup     0xE2
#define bgm_Slowdown    0xE3
#define bgm_Stop        0xE4
#define flg__Last       0xE4

/* ---------------------------------------------------------------------------
    Pattern Load Cue IDs
    --------------------------------------------------------------------------- */
#define plc_slot_size               6          /* size of a PLC slot (4B src + 2B dest) */
#define plc_slot_count              16         /* max queued PLCs */

#define plcid_Main             0
#define plcid_Main2            1
#define plcid_Explode          2
#define plcid_GameOver         3
#define plcid_GHZ              4
#define plcid_GHZ2             5
#define plcid_LZ               6
#define plcid_LZ2              7
#define plcid_MZ               8
#define plcid_MZ2              9
#define plcid_SLZ              10
#define plcid_SLZ2             11
#define plcid_SYZ              12
#define plcid_SYZ2             13
#define plcid_SBZ              14
#define plcid_SBZ2             15
#define plcid_TitleCard        16
#define plcid_Boss             17
#define plcid_Signpost         18
#define plcid_Warp             19
#define plcid_SpecialStage     20
#define plcid_GHZAnimals       21
#define plcid_LZAnimals        22
#define plcid_MZAnimals        23
#define plcid_SLZAnimals       24
#define plcid_SYZAnimals       25
#define plcid_SBZAnimals       26
#define plcid_SSResult         27
#define plcid_Ending           28
#define plcid_TryAgain         29
#define plcid_EggmanSBZ2       30
#define plcid_FZBoss           31

/* ---------------------------------------------------------------------------
    Boss locations
    --------------------------------------------------------------------------- */
/* Green Hill Zone */
#define boss_ghz_x      0x2960
#define boss_ghz_y      0x300
#define boss_ghz_end    (boss_ghz_x + 0x160)

/* Labyrinth Zone */
#define boss_lz_x       0x1DE0
#define boss_lz_y       0xC0
#define boss_lz_end     (boss_lz_x + 0x250)

/* Marble Zone */
#define boss_mz_x       0x1800
#define boss_mz_y       0x210
#define boss_mz_end     (boss_mz_x + 0x160)

/* Star Light Zone */
#define boss_slz_x      0x2000
#define boss_slz_y      0x210
#define boss_slz_end    (boss_slz_x + 0x160)

/* Spring Yard Zone */
#define boss_syz_x      0x2C00
#define boss_syz_y      0x4CC
#define boss_syz_end    (boss_syz_x + 0x140)

/* Scrap Brain Zone act 2 cutscene */
#define boss_sbz2_x     0x2050
#define boss_sbz2_y     0x510

/* Final Zone */
#define boss_fz_x       0x2450
#define boss_fz_y       0x510
#define boss_fz_end     (boss_fz_x + 0x2B0)

/* ---------------------------------------------------------------------------
   Tile flags
   --------------------------------------------------------------------------- */
#define Tile_Prio    (1 << 15)
#define Tile_Pal1    (0 << 13)
#define Tile_Pal2    (1 << 13)
#define Tile_Pal3    (2 << 13)
#define Tile_Pal4    (3 << 13)

/* ---------------------------------------------------------------------------
   VRAM ArtTile definitions
   Multiply by tile_size ($20) to get the actual VRAM byte address.
   --------------------------------------------------------------------------- */

/* -- Shared -------------------------------------------------------------- */
#define ArtTile_GHZ_MZ_Swing              0x380
#define ArtTile_MZ_SYZ_Caterkiller        0x4FF
#define ArtTile_GHZ_SLZ_Smashable_Wall    0x50F

/* -- Green Hill Zone ----------------------------------------------------- */
#define ArtTile_GHZ_Flower_4              (ArtTile_Level + 0x340)
#define ArtTile_GHZ_Edge_Wall             0x34C
#define ArtTile_GHZ_Flower_Stalk          (ArtTile_Level + 0x358)
#define ArtTile_GHZ_Big_Flower_1          (ArtTile_Level + 0x35C)
#define ArtTile_GHZ_Small_Flower          (ArtTile_Level + 0x36C)
#define ArtTile_GHZ_Waterfall             (ArtTile_Level + 0x378)
#define ArtTile_GHZ_Flower_3              (ArtTile_Level + 0x380)
#define ArtTile_GHZ_Bridge                0x38E
#define ArtTile_GHZ_Big_Flower_2          (ArtTile_Level + 0x390)
#define ArtTile_GHZ_Spike_Pole            0x398
#define ArtTile_GHZ_Giant_Ball            0x3AA
#define ArtTile_GHZ_Purple_Rock           0x3D0

/* -- Marble Zone --------------------------------------------------------- */
#define ArtTile_MZ_Block                  0x2B8
#define ArtTile_MZ_Animated_Magma         (ArtTile_Level + 0x2D2)
#define ArtTile_MZ_Animated_Lava          (ArtTile_Level + 0x2E2)
#define ArtTile_MZ_Torch                  (ArtTile_Level + 0x2F2)
#define ArtTile_MZ_Spike_Stomper          0x300
#define ArtTile_MZ_Fireball               0x345
#define ArtTile_MZ_Glass_Pillar           0x38E
#define ArtTile_MZ_Lava                   0x3A8

/* -- Spring Yard Zone ---------------------------------------------------- */
#define ArtTile_SYZ_Bumper                0x380
#define ArtTile_SYZ_Big_Spikeball         0x396
#define ArtTile_SYZ_Spikeball_Chain       0x3BA

/* -- Labyrinth Zone ------------------------------------------------------ */
#define ArtTile_LZ_Block_1                0x1E0
#define ArtTile_LZ_Block_2                0x1F0
#define ArtTile_LZ_Splash                 0x259
#define ArtTile_LZ_Gargoyle               0x2E9
#define ArtTile_LZ_Water_Surface          0x300
#define ArtTile_LZ_Spikeball_Chain        0x310
#define ArtTile_LZ_Flapping_Door          0x328
#define ArtTile_LZ_Bubbles                0x348
#define ArtTile_LZ_Moving_Block           0x3BC
#define ArtTile_LZ_Door                   0x3C4
#define ArtTile_LZ_Harpoon                0x3CC
#define ArtTile_LZ_Pole                   0x3DE
#define ArtTile_LZ_Push_Block             0x3DE
#define ArtTile_LZ_Blocks                 0x3E6
#define ArtTile_LZ_Conveyor_Belt          0x3F6
#define ArtTile_LZ_UnusedFace             0x440
#define ArtTile_LZ_Rising_Platform        (ArtTile_LZ_Blocks + 0x69)
#define ArtTile_LZ_Orbinaut               0x467
#define ArtTile_LZ_Cork                   (ArtTile_LZ_Blocks + 0x11A)

/* -- Star Light Zone ----------------------------------------------------- */
#define ArtTile_SLZ_Seesaw                0x374
#define ArtTile_SLZ_Fan                   0x3A0
#define ArtTile_SLZ_Pylon                 0x3CC
#define ArtTile_SLZ_Swing                 0x3DC
#define ArtTile_SLZ_Orbinaut              0x429
#define ArtTile_SLZ_Fireball              0x480
#define ArtTile_SLZ_Fireball_Launcher     0x4D8
#define ArtTile_SLZ_Collapsing_Floor      0x4E0
#define ArtTile_SLZ_Spikeball             0x4F0

/* -- Scrap Brain Zone ---------------------------------------------------- */
#define ArtTile_SBZ_Caterkiller           0x2B0
#define ArtTile_SBZ_Moving_Block_Short    0x2C0
#define ArtTile_SBZ_Door                  0x2E8
#define ArtTile_SBZ_Girder                0x2F0
#define ArtTile_SBZ_Disc                  0x344
#define ArtTile_SBZ_Junction              0x348
#define ArtTile_SBZ_Swing                 0x391
#define ArtTile_SBZ_Saw                   0x3B5
#define ArtTile_SBZ_Flamethrower          0x3D9
#define ArtTile_SBZ_Collapsing_Floor      0x3F5
#define ArtTile_SBZ_Orbinaut              0x429
#define ArtTile_SBZ_Smoke_Puff_1          (ArtTile_Level + 0x448)
#define ArtTile_SBZ_Smoke_Puff_2          (ArtTile_Level + 0x454)
#define ArtTile_SBZ_Moving_Block_Long     0x460
#define ArtTile_SBZ_Horizontal_Door       0x46F
#define ArtTile_SBZ_Electric_Orb          0x47E
#define ArtTile_SBZ_Trap_Door             0x492
#define ArtTile_SBZ_Vanishing_Block       0x4C3
#define ArtTile_SBZ_Spinning_Platform     0x4DF

/* -- Final Zone ---------------------------------------------------------- */
#define ArtTile_FZ_Boss                   0x300
#define ArtTile_FZ_Eggman_Fleeing         0x3A0
#define ArtTile_FZ_Eggman_No_Vehicle      0x470

/* -- General Level Art --------------------------------------------------- */
#define ArtTile_Level                     0x000
#define ArtTile_Ball_Hog                  0x302
#define ArtTile_Bomb                      0x400
#define ArtTile_Crabmeat                  0x400
#define ArtTile_UnusedExplosion           0x41C
#define ArtTile_Buzz_Bomber               0x444
#define ArtTile_Chopper                   0x47B
#define ArtTile_Yadrin                    0x47B
#define ArtTile_Jaws                      0x486
#define ArtTile_Newtron                   0x49B
#define ArtTile_Burrobot                  0x4A6
#define ArtTile_Basaran                   0x4B8
#define ArtTile_Roller                    0x4B8
#define ArtTile_Moto_Bug                  0x4F0
#define ArtTile_Button                    0x50F
#define ArtTile_Button_Main               (ArtTile_Button + 4)
#define ArtTile_Spikes                    0x51B
#define ArtTile_Spring_Horizontal         0x523
#define ArtTile_Spring_Vertical           0x533
#define ArtTile_Shield                    0x541
#define ArtTile_Invincibility             0x55C
#define ArtTile_Game_Over                 0x55E
#define ArtTile_Title_Card                0x580
#define ArtTile_Animal_1                  0x580
#define ArtTile_Animal_2                  0x592
#define ArtTile_Explosion                 0x5A0
#define ArtTile_Monitor                   0x680
#define ArtTile_HUD                       0x6CA
#define ArtTile_HUDScore                  (ArtTile_HUD + 0x1A)
#define ArtTile_HUDScore_E                (ArtTile_HUDScore - 2)
#define ArtTile_HUDTimeMins               (ArtTile_HUD + 0x28)
#define ArtTile_HUDTimeSecs               (ArtTile_HUD + 0x2C)
#define ArtTile_HUDRings                  (ArtTile_HUD + 0x30)

#define ArtTile_Sonic                     0x780
#define ArtTile_Points                    0x797
#define ArtTile_Lamppost                  0x7A0
#define ArtTile_Ring                      0x7B2
#define ArtTile_Lives_Counter             0x7D4
#define ArtTile_Lives_Counter_Num         (ArtTile_Lives_Counter + 9)

/* -- Eggman -------------------------------------------------------------- */
#define ArtTile_Eggman                    0x400
#define ArtTile_Eggman_Weapons            0x46C
#define ArtTile_Eggman_Button             0x4A4
#define ArtTile_Eggman_Spikeball          0x518
#define ArtTile_Eggman_Trap_Floor         0x518
#define ArtTile_Eggman_Exhaust            (ArtTile_Eggman + 0x12A)

/* -- End of Level -------------------------------------------------------- */
#define ArtTile_Giant_Ring                0x400
#define ArtTile_Giant_Ring_Flash          0x462
#define ArtTile_Prison_Capsule            0x49D
#define ArtTile_Hidden_Points             0x4B6
#define ArtTile_Warp                      0x541
#define ArtTile_Mini_Sonic                0x551
#define ArtTile_Bonuses                   0x570
#define ArtTile_Signpost                  0x680

/* -- Sega Screen --------------------------------------------------------- */
#define ArtTile_Sega_Tiles                0x000

/* -- Title Screen -------------------------------------------------------- */
#define ArtTile_Title_Japanese_Text       0x000
#define ArtTile_Title_Foreground          0x200
#define ArtTile_Title_Sonic               0x300
#define ArtTile_Title_Trademark           0x510
#define ArtTile_Level_Select_Font         0x680

/* Object IDs (from disasm "_inc/Object Pointers.asm") */
#define id_SonicPlayer                    0x01
#define id_Splash                         0x08
#define id_SonicSpecial                   0x09
#define id_DrownCount                     0x0A
#define id_Pole                           0x0B
#define id_FlapDoor                       0x0C
#define id_Signpost                       0x0D
#define id_TitleSonic                     0x0E
#define id_PSBTM                          0x0F
#define id_Obj10                          0x10
#define id_Bridge                         0x11
#define id_SpinningLight                  0x12
#define id_LavaMaker                      0x13
#define id_LavaBall                       0x14
#define id_SwingingPlatform               0x15
#define id_Harpoon                        0x16
#define id_Helix                          0x17
#define id_BasicPlatform                  0x18
#define id_Obj19                          0x19
#define id_CollapseLedge                  0x1A
#define id_WaterSurface                   0x1B
#define id_Scenery                        0x1C
#define id_MagicSwitch                    0x1D
#define id_BallHog                        0x1E
#define id_Crabmeat                       0x1F
#define id_Cannonball                     0x20
#define id_HUD                            0x21
#define id_BuzzBomber                     0x22
#define id_Missile                        0x23
#define id_UnusedExplosion                0x24
#define id_Rings                          0x25
#define id_Monitor                        0x26
#define id_ExplosionItem                  0x27
#define id_Animals                        0x28
#define id_Points                         0x29
#define id_AutoDoor                       0x2A
#define id_Chopper                        0x2B
#define id_Jaws                           0x2C
#define id_Burrobot                       0x2D
#define id_PowerUp                        0x2E
#define id_LargeGrass                     0x2F
#define id_GlassBlock                     0x30
#define id_ChainStomp                     0x31
#define id_Button                         0x32
#define id_PushBlock                      0x33
#define id_TitleCard                      0x34
#define id_GrassFire                      0x35
#define id_Spikes                         0x36
#define id_RingLoss                       0x37
#define id_ShieldItem                     0x38
#define id_GameOverCard                   0x39
#define id_GotThroughCard                 0x3A
#define id_PurpleRock                     0x3B
#define id_SmashWall                      0x3C
#define id_BossGreenHill                  0x3D
#define id_Prison                         0x3E
#define id_Explosion                      0x3F
#define id_MotoBug                        0x40
#define id_Springs                        0x41
#define id_Newtron                        0x42
#define id_Roller                         0x43
#define id_EdgeWalls                      0x44
#define id_SideStomp                      0x45
#define id_MarbleBrick                    0x46
#define id_Bumper                         0x47
#define id_BossBall                       0x48
#define id_WaterSound                     0x49
#define id_VanishSonic                    0x4A
#define id_GiantRing                      0x4B
#define id_GeyserMaker                    0x4C
#define id_LavaGeyser                     0x4D
#define id_LavaWall                       0x4E
#define id_Obj4F                          0x4F
#define id_Yadrin                         0x50
#define id_SmashBlock                     0x51
#define id_MovingBlock                    0x52
#define id_CollapseFloor                  0x53
#define id_LavaTag                        0x54
#define id_Basaran                        0x55
#define id_FloatingBlock                  0x56
#define id_SpikeBall                      0x57
#define id_BigSpikeBall                   0x58
#define id_Elevator                       0x59
#define id_CirclingPlatform               0x5A
#define id_Staircase                      0x5B
#define id_Pylon                          0x5C
#define id_Fan                            0x5D
#define id_Seesaw                         0x5E
#define id_Bomb                           0x5F
#define id_Orbinaut                       0x60
#define id_LabyrinthBlock                 0x61
#define id_Gargoyle                       0x62
#define id_LabyrinthConvey                0x63
#define id_Bubble                         0x64
#define id_Waterfall                      0x65
#define id_Junction                       0x66
#define id_RunningDisc                    0x67
#define id_Conveyor                       0x68
#define id_SpinPlatform                   0x69
#define id_Saws                           0x6A
#define id_ScrapStomp                     0x6B
#define id_VanishPlatform                 0x6C
#define id_Flamethrower                   0x6D
#define id_Electro                        0x6E
#define id_SpinConvey                     0x6F
#define id_Girder                         0x70
#define id_Invisibarrier                  0x71
#define id_Teleport                       0x72
#define id_BossMarble                     0x73
#define id_BossFire                       0x74
#define id_BossSpringYard                 0x75
#define id_BossBlock                      0x76
#define id_BossLabyrinth                  0x77
#define id_Caterkiller                    0x78
#define id_Lamppost                       0x79
#define id_BossStarLight                  0x7A
#define id_BossSpikeball                  0x7B
#define id_RingFlash                      0x7C
#define id_HiddenBonus                    0x7D
#define id_SSResult                       0x7E
#define id_SSRChaos                       0x7F
#define id_ContScrItem                    0x80
#define id_ContSonic                      0x81
#define id_ScrapEggman                    0x82
#define id_FalseFloor                     0x83
#define id_EggmanCylinder                 0x84
#define id_BossFinal                      0x85
#define id_BossPlasma                     0x86
#define id_EndSonic                       0x87
#define id_EndChaos                       0x88
#define id_EndSTH                         0x89
#define id_CreditsText                    0x8A
#define id_EndEggman                      0x8B
#define id_TryChaos                       0x8C

/* PLC IDs for bosses (from disasm "_inc/Pattern Load Cues.asm") */
#define plcid_EggmanSBZ2                  0x1E
#define plcid_FZBoss                      0x1F

/* Level select */
#define levsel_sndtest_row                0x14
#define levsel_line_count                 21
#define levsel_line_length                24
#define levsel_sndtest_col                (levsel_line_length - 8)
#define levsel_start_row                  4
#define levsel_start_col                  8
#define levsel_vram_main                  (vram_bg + (levsel_start_row << 7) + (levsel_start_col << 1))
#define levsel_vram_sndtestnum            (levsel_vram_main + (levsel_sndtest_row << 7) + (levsel_sndtest_col << 1))
#define levsel_white                      (ArtTile_Level_Select_Font | Tile_Pal4 | Tile_Prio)
#define levsel_yellow                     (ArtTile_Level_Select_Font | Tile_Pal3 | Tile_Prio)

/* -- Continue Screen ----------------------------------------------------- */
#define ArtTile_Continue_Sonic            0x500
#define ArtTile_Continue_Number           0x6FC

/* -- Ending -------------------------------------------------------------- */
#define ArtTile_Ending_Flowers            0x3A0
#define ArtTile_Ending_Emeralds           0x3C5
#define ArtTile_Ending_Sonic              0x3E1
#define ArtTile_Ending_Eggman             0x524
#define ArtTile_Ending_Rabbit             0x553
#define ArtTile_Ending_Chicken            0x565
#define ArtTile_Ending_Penguin            0x573
#define ArtTile_Ending_Seal               0x585
#define ArtTile_Ending_Pig                0x593
#define ArtTile_Ending_Flicky             0x5A5
#define ArtTile_Ending_Squirrel           0x5B3
#define ArtTile_Ending_STH                0x5C5

/* -- Try Again Screen ---------------------------------------------------- */
#define ArtTile_Try_Again_Emeralds        0x3C5
#define ArtTile_Try_Again_Eggman          0x3E1

/* -- Special Stage ------------------------------------------------------- */
#define ArtTile_SS_Background_Clouds      0x000
#define ArtTile_SS_Background_Fish        0x051
#define ArtTile_SS_Wall                   0x142
#define ArtTile_SS_Plane_1                0x200
#define ArtTile_SS_Bumper                 0x23B
#define ArtTile_SS_Goal                   0x251
#define ArtTile_SS_Up_Down                0x263
#define ArtTile_SS_R_Block                0x2F0
#define ArtTile_SS_Plane_2                0x300
#define ArtTile_SS_Extra_Life             0x370
#define ArtTile_SS_Emerald_Sparkle        0x3F0
#define ArtTile_SS_Plane_3                0x400
#define ArtTile_SS_Red_White_Block        0x470
#define ArtTile_SS_Ghost_Block            0x4F0
#define ArtTile_SS_Plane_4                0x500
#define ArtTile_SS_W_Block                0x570
#define ArtTile_SS_Glass                  0x5F0
#define ArtTile_SS_Plane_5                0x600
#define ArtTile_SS_Plane_6                0x700
#define ArtTile_SS_Emerald                0x770
#define ArtTile_SS_Zone_1                 0x797
#define ArtTile_SS_Zone_2                 0x7A0
#define ArtTile_SS_Zone_3                 0x7A9
#define ArtTile_SS_Zone_4                 0x797
#define ArtTile_SS_Zone_5                 0x7A0
#define ArtTile_SS_Zone_6                 0x7A9

/* -- Special Stage Results ----------------------------------------------- */
#define ArtTile_SS_Results_Emeralds       0x541

/* -- Font ---------------------------------------------------------------- */
#define ArtTile_Sonic_Team_Font           0x0A6
#define ArtTile_Credits_Font              0x5A0

/* -- Error Handler ------------------------------------------------------- */
#define ArtTile_Error_Handler_Font        0x7C0

#endif /* SONIC1_CONSTANTS_H */
