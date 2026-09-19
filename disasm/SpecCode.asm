; ===========================================================================
; ---------------------------------------------------------------------------
; Special Stage
; ---------------------------------------------------------------------------

; SpecialStage:
GM_Special:		; white fade-out from previous game mode
		move.w	#sfx_EnterSS,d0				; set special stage entry sound
		bsr.w	QueueSound2				; play it
		bsr.w	PaletteWhiteOut				; fade-out to white
; ---------------------------------------------------------------------------

		; load special stage patterns
		disable_ints					; disable interrupts
		lea	(vdp_control_port).l,a6			; load VDP control port
		move.w	#vreg_mode3|%0011,(a6)			; line scroll mode (per-row horizontally, full-screen vertically)
		move.w	#vreg_mode1|%000100,(a6)		; use 8-colour mode
		move.w	#vreg_hintrate|175,(v_hblank_hreg).w	; set HBlank counter to scanline 175 (even though horizontal interrupts aren't used here...)
		move.w	#$9011,(a6)				; 128-cell hscroll size
		disable_display					; disable screen output
		bsr.w	ClearScreen				; wipe screen
		enable_ints					; enable interrupts

		fillVRAM 0, ArtTile_SS_Plane_1*tile_size+plane_size_64x32, ArtTile_SS_Plane_5*tile_size ; clear nametables
		bsr.w	SS_BGLoad				; load background clouds/bubbles/birds/fish mappings
		moveq	#plcid_SpecialStage,d0			; load special stage patterns
		bsr.w	QuickPLC				; execute PLCs immediately (no queue)

		clearRAM v_objspace				; clear object RAM space
		clearRAM v_levelvariables			; clear various level variables
		clearRAM v_timingvariables			; clear various timing variables
		clearRAM v_ngfx_buffer				; clear Nemesis decompression buffer

		clr.b	(f_wtr_state).w				; clear water state
		clr.w	(f_restart).w				; clear level restart flag
		moveq	#palid_Special,d0			; load special stage palette...
		bsr.w	PalLoad_Fade				; ...into the palette fade-in buffer
		jsr	(SS_Load).l				; load SS layout data (based on last stage entered and collected emeralds)

	if FixBugs
		; Set custom level boundaries so that the fixed
		; debug mode will not break for Special Stages.
		move.w	#$2B0,(v_limittop2).w			; set top boundary
		move.w	#$7D0,(v_limitbtm2).w			; set bottom boundary
		move.w	#$2E0,(v_limitleft2).w			; set left boundary
		move.w	#$7A0,(v_limitright2).w			; set right boundary
	endif

		move.l	#0,(v_screenposx).w			; reset X-camera position
		move.l	#0,(v_screenposy).w			; reset Y-camera position
		move.b	#id_SonicSpecial,(v_player).w		; load special stage Sonic object
		bsr.w	PalCycle_SS				; initialize palette cycle and background for fade-in
		clr.w	(v_ssangle).w				; set stage angle to "upright"
		move.w	#ss_rotatespeed,(v_ssrotate).w		; set initial stage rotation speed ($40, see object 09)
		move.w	#bgm_SS,d0				; play special stage BG music
		bsr.w	QueueSound1				; play it

		move.w	#0,(v_btnpushtime1).w			; clear button push counters for demos
		lea	(DemoDataPtr).l,a1			; load demo data
		moveq	#6,d0					; hardcoded to load the entry for the Special Stage demo
		lsl.w	#2,d0					; multiply by 4 for longword-based indexing
		movea.l	(a1,d0.w),a1				; get demo pointer for current level
		move.b	1(a1),(v_btnpushtime2).w		; load initial demo key press duration
		subq.b	#1,(v_btnpushtime2).w			; subtract 1 from demo key pressduration

		clr.w	(v_rings).w				; clear rings
		clr.b	(v_lifecount).w				; clear extra lives flags when getting 100/200 rings

		move.w	#0,(v_debuguse).w			; exit debug mode if necessary
		move.w	#1800,(v_generictimer).w		; run regular demos for 30 seconds
		tst.b	(f_debugcheat).w			; has debug cheat been entered?
		beq.s	SS_NoDebug				; if not, branch
		btst	#bitA,(v_jpadhold1).w			; is A button held?
		beq.s	SS_NoDebug				; if not, branch
		move.b	#1,(f_debugmode).w			; enable debug mode

SS_NoDebug:
		enable_display					; enable screen out-put
		bsr.w	PaletteWhiteIn				; fade-in from white

; ---------------------------------------------------------------------------
; Special Stage main loop
; ---------------------------------------------------------------------------

SS_MainLoop:
		bsr.w	PauseGame				; handle pausing the game when pressing start
		move.b	#id_VBlank_SpecialStage,(v_vblank_routine).w ; set VBlank routine to $0A
		bsr.w	WaitForVBlank				; wait until VBlank has finished
		bsr.w	MoveSonicInDemo				; simulate controls in demos (immediately returns outside demos)
		move.w	(v_jpadhold1).w,(v_jpadhold2).w		; copy controller 1 inputs to Sonic player object inputs

		jsr	(ExecuteObjects).l			; execute Special Stage object
		jsr	(BuildSprites).l			; build sprites
		jsr	(SS_ShowLayout).l			; render Special Stage layout
		bsr.w	SS_BGAnimate				; animate Special Stage background

		tst.w	(f_demo).w				; is demo mode on?
		beq.s	SS_ChkEnd				; if not, branch
		tst.w	(v_generictimer).w			; is there time left on the demo?
		beq.w	SS_ToSegaScreen				; if not, return to Sega screen

SS_ChkEnd:
		cmpi.b	#id_Special,(v_gamemode).w		; is game mode still the Special Stage?
		beq.w	SS_MainLoop				; if yes, loop game mode
; ---------------------------------------------------------------------------

		; Exiting Special Stage...
		tst.w	(f_demo).w				; are we exiting from a demo?
	if Revision=0
		bne.w	SS_ToSegaScreen				; if yes, return to Sega Screen
	else
		; REV01 added a small convenience improvement by returning straight
		; to the title screen when pressing start during the Special Stage demo,
		; rather than always being forced back to the Sega screen.
		bne.w	SS_ToNextScreen				; if yes, return to next game mode
	endif

		move.b	#id_Level,(v_gamemode).w		; set screen mode to $0C (level)
		cmpi.w	#id_FZ+1,(v_zone_act).w			; is level number higher than FZ (0502)?
		blo.s	SS_Finish				; if not, branch
		clr.w	(v_zone_act).w				; set to GHZ1 (possibly as a failsafe)


SS_Finish:
		move.w	#60,(v_generictimer).w			; run fade-out for one second
		move.w	#$003F,(v_pfade_start).w		; set palette fade-out position and size
		clr.w	(v_palchgspeed).w			; do first palette brightening immediately

SS_FinLoop:
		move.b	#id_VBlank_Continue,(v_vblank_routine).w ; set VBlank routine to $16 (uses the same one as the continue screen)
		bsr.w	WaitForVBlank				; wait until VBlank has finished
		bsr.w	MoveSonicInDemo				; continue updating demo controls during fade-out
		move.w	(v_jpadhold1).w,(v_jpadhold2).w		; continue copying 1P inputs to Sonic object (even though controls are locked...)
		jsr	(ExecuteObjects).l			; continue executing objects during fade-out
		jsr	(BuildSprites).l			; continue building sprites during fade-out
		jsr	(SS_ShowLayout).l			; continue rendering Special Stage layout
		bsr.w	SS_BGAnimate				; continue to animate background

		subq.w	#1,(v_palchgspeed).w			; decrement palette fade-out delay
		bpl.s	SS_FinLoop_NoBrighten			; if time remains, branch
		move.w	#2,(v_palchgspeed).w			; reset palette fade-out delay
		bsr.w	WhiteOut_ToWhite			; brighten palette further

; loc_47D4:
SS_FinLoop_NoBrighten:
		tst.w	(v_generictimer).w			; has fade-out loop finished?
		bne.s	SS_FinLoop				; if not, loop
; ---------------------------------------------------------------------------

		; Fade-out done, load Special Stage Results screen
		disable_ints					; disable interrupts
		lea	(vdp_control_port).l,a6			; load VDP control port
		move.w	#vreg_fgvram|(vram_fg>>10),(a6)		; set foreground nametable address
		move.w	#vreg_bgvram|(vram_bg>>13),(a6)		; set background nametable address
		move.w	#vreg_planesize|%000001,(a6)		; 64-cell hscroll size
		bsr.w	ClearScreen				; wipe screen

		locVRAM	ArtTile_Title_Card*tile_size		; set VRAM location for title card font
		lea	(Nem_TitleCard).l,a0			; load title card patterns
		bsr.w	NemDec					; decompress Nemesis-compressed graphics directly to VRAM

		jsr	(Hud_Base).l				; load basic HUD graphics
		enable_ints					; enable interrupts

		moveq	#palid_SSResult,d0			; load Special Stage results screen palette...
		bsr.w	PalLoad					; ...directly to active palette
		moveq	#plcid_Main,d0				; load main patterns (rings, etc.)
		bsr.w	NewPLC					; add to new PLC queue
		moveq	#plcid_SSResult,d0			; load Special Stage results screen patterns
		bsr.w	AddPLC					; add to PLC queue

		move.b	#1,(f_scorecount).w			; update score counter
		move.b	#1,(f_endactbonus).w			; update ring bonus counter
		move.w	(v_rings).w,d0				; get rings collected in Special Stage
		mulu.w	#10,d0					; award 100 bonus points per collected ring
		move.w	d0,(v_ringbonus).w			; set rings bonus

		move.w	#bgm_GotThrough,d0			; play end-of-level music
		jsr	(QueueSound2).l	 			; play it

		clearRAM v_objspace				; clear object RAM

		move.b	#id_SSResult,(v_ssrescard).w		; load Special Stage Results screen object
; ---------------------------------------------------------------------------

SS_NormalExit:		; Special Stage results screen loop
		bsr.w	PauseGame				; allow pausing during the results screen
		move.b	#id_VBlank_TitleCards,(v_vblank_routine).w ; set VBlank routine to $0C
		bsr.w	WaitForVBlank				; wait until VBlank has finished
		jsr	(ExecuteObjects).l			; execute SSR objects
		jsr	(BuildSprites).l			; build sprites
		bsr.w	RunPLC					; load SSR patterns
		tst.w	(f_restart).w				; has the SSR object signaled that we can exit?
		beq.s	SS_NormalExit				; if not, loop results screen
		tst.l	(v_plc_buffer).w			; is PLC buffer empty?
		bne.s	SS_NormalExit				; if not, loop (pointless here, SSR object has its own check)
; ---------------------------------------------------------------------------

		; Exit Special Stage normally
		move.w	#sfx_EnterSS,d0				; play special stage exit sound
		bsr.w	QueueSound2 				; play it
		bsr.w	PaletteWhiteOut				; fade-out to white
		rts						; return to MainGameLoop
; ===========================================================================

SS_ToSegaScreen:
		move.b	#id_Sega,(v_gamemode).w			; set game mode to Sega screen
		rts						; return to MainGameLoop
; ===========================================================================

	if Revision<>0
; SS_ToLevel: <-- old misnomer
SS_ToNextScreen:
		cmpi.b	#id_Level,(v_gamemode).w		; was demo exited with the instruction to go to a level next?
		beq.s	SS_ToSegaScreen				; if yes, return to the Sega screen instead (if demo finished)
		rts						; otherwise, go to new game mode (which is the title screen, if demo was aborted)
	endif
; ENd of function GM_Special

; ===========================================================================
; ===========================================================================
; ---------------------------------------------------------------------------
; Special stage	background mappings loading subroutine
; ---------------------------------------------------------------------------

SS_BGLoad:
	; --- Load mappings for the birds and fish ---
		ssbg_animalsize:	equ 8

		lea	(v_ram_start).l,a1			; buffer
		lea	(Eni_SSBg1).l,a0			; load mappings for the birds and fish
		move.w	#ArtTile_SS_Background_Fish|Tile_Pal3,d0 ; add this to each tile
		bsr.w	EniDec					; decompress fish/bird mappings to RAM

		locVRAM	ArtTile_SS_Plane_1*tile_size+$1000,d3	; d3 = VDP address for $5000 in VRAM
		lea	(v_ram_start+(ssbg_animalsize*ssbg_animalsize*2)).l,a2
		moveq	#7-1,d7					; number of canvases for frames of bird/fish and in-between

; Each frame of bird/fish animation is stored as a canvas in VRAM. The game switches between them by changing the BG nametable register.
.loop_canvas:
		move.l	d3,d0					; copy VDP command
		moveq	#4-1,d6					; number of rows visible
		moveq	#0,d4					; first square is blank (i.e. blank-bird-blank-bird-etc.)
		cmpi.w	#4-1,d7
		bhs.s	.loop_rows				; branch if canvas is bird
		moveq	#1,d4					; first square is fish (i.e. fish-blank-fish-blank-etc.)

.loop_rows:
		moveq	#8-1,d5					; number of squares in a row

.loop_birdfish:
		movea.l	a2,a1					; get address of tilemap as stored in RAM
		eori.b	#1,d4					; switch between blank square and bird/fish
		bne.s	.is_birdfish				; branch if set to bird/fish
		cmpi.w	#7-1,d7
		bne.s	.skip_birdfish				; branch if not first frame
		lea	(v_ram_start).l,a1			; use tilemap for checkerboard pattern

	.is_birdfish:
		movem.l	d0-d4,-(sp)
		moveq	#ssbg_animalsize-1,d1
		moveq	#ssbg_animalsize-1,d2
		bsr.w	TilemapToVRAM				; copy tilemap for 1 bird or fish from RAM to VRAM
		movem.l	(sp)+,d0-d4

	.skip_birdfish:
		addi.l	#(ssbg_animalsize*2)<<16,d0		; skip 8 cells ($10 bytes)
		dbf	d5,.loop_birdfish			; repeat for all squares in 1 row

		addi.l	#((ssbg_animalsize-1)*$80)<<16,d0	; skip 7 rows ($380 byes)
		eori.b	#1,d4					; stagger blank/birdfish pattern
		dbf	d6,.loop_rows				; repeat for all rows (4 in total)

		addi.l	#$1000<<16,d3				; add $1000 to VRAM address
		bpl.s	.vdp_ok					; branch if valid VDP command
		swap	d3
		addi.l	#$C000,d3				; fix VDP command
		swap	d3

	.vdp_ok:
		adda.w	#ssbg_animalsize*ssbg_animalsize*2,a2	; read from next tilemap
		dbf	d7,.loop_canvas				; repeat for all canvases

	; --- Load mappings for the bubbles and clouds ---
		lea	(v_ram_start).l,a1
		lea	(Eni_SSBg2).l,a0			; load mappings for clouds/bubbles
		move.w	#ArtTile_SS_Background_Clouds|Tile_Pal3,d0
		bsr.w	EniDec					; decompress to buffer in RAM

		copyTilemap	v_ram_start,ArtTile_SS_Plane_5*tile_size,64,32		; copy tilemap for bubbles to VRAM
		copyTilemap	v_ram_start,ArtTile_SS_Plane_5*tile_size+$1000,64,64	; copy tilemap for clouds to VRAM

		rts

; ===========================================================================
; ---------------------------------------------------------------------------
; Special stage palette cycling and background canvas updating routine
; ---------------------------------------------------------------------------

PalCycle_SS:
		tst.w	(f_pause).w				; is game paused?
		bne.s	.exit					; if yes, branch
		subq.w	#1,(v_palss_time).w			; decrement timer
		bpl.s	.exit					; branch if time remains

		lea	(vdp_control_port).l,a6
		move.w	(v_palss_num).w,d0			; get cycle index counter
		addq.w	#1,(v_palss_num).w			; increment
		andi.w	#$1F,d0					; read only bits 0-4
		lsl.w	#2,d0					; multiply by 4
		lea	(SS_Timing_Values).l,a0
		adda.w	d0,a0

		; Time
		move.b	(a0)+,d0				; get time byte
		bpl.s	.use_time				; branch if not -1
		move.w	#$200-1,d0				; use $1FF if -1

	.use_time:
		move.w	d0,(v_palss_time).w			; set time until next palette change

		; Anim
		moveq	#0,d0
		move.b	(a0)+,d0				; get BG mode byte
		move.w	d0,(v_ssbganim).w
		lea	(SS_BG_Modes).l,a1
		lea	(a1,d0.w),a1				; jump to mode data

		; FG VRAM
		move.w	#vreg_fgvram,d0				; VDP register - FG nametable address
		move.b	(a1)+,d0				; apply address from mode data
		move.w	d0,(a6)					; send VDP instruction

		; Y coordinate
		move.b	(a1),(v_scrposy_vdp).w			; get byte to send to VSRAM

		; BG VRAM
		move.w	#vreg_bgvram,d0				; VDP register - BG nametable address
		move.b	(a0)+,d0				; apply address from list
		move.w	d0,(a6)					; send VDP instruction
		move.l	#$40000010,(vdp_control_port).l		; set VDP to VSRAM write mode
		move.l	(v_scrposy_vdp).w,(vdp_data_port).l	; update VSRAM

		; Palette cycle index
		moveq	#0,d0
		move.b	(a0)+,d0				; get palette offset
		bmi.s	PalCycle_SS_2				; branch if $80+
		lea	(Pal_SSCyc1).l,a1			; use palette cycle set 1
		adda.w	d0,a1
		lea	(v_palette_line_3+$E).w,a2
		move.l	(a1)+,(a2)+
		move.l	(a1)+,(a2)+
		move.l	(a1)+,(a2)+				; write palette

	.exit:
		rts
; ===========================================================================

PalCycle_SS_2:	; usepalcycle2 flag set
		move.w	(v_palss_index).w,d1			; get SS palette index ID (unused, this is always 0)
		cmpi.w	#$80|$A,d0				; is offset $80-$89?
		blo.s	.offset_80_89				; if yes, branch
		addq.w	#1,d1

	.offset_80_89:
		mulu.w	#$2A,d1					; d1 = always 0 or $2A
		lea	(Pal_SSCyc2).l,a1			; use palette cycle set 2
		adda.w	d1,a1
		andi.w	#$7F,d0					; ignore bit 7
		bclr	#0,d0					; clear bit 0
		beq.s	.offset_even				; branch if already clear

		; extrapalline4 flag set
		lea	(v_palette_line_4+$E).w,a2
		move.l	(a1),(a2)+
		move.l	4(a1),(a2)+
		move.l	8(a1),(a2)+				; write palette

	.offset_even:
		adda.w	#$C,a1
		lea	(v_palette_line_3+$1A).w,a2
		cmpi.w	#$A,d0					; is offset 0-8?
		blo.s	.offset_0_8				; if yes, branch
		subi.w	#$A,d0
		lea	(v_palette_line_4+$1A).w,a2

	.offset_0_8:
		move.w	d0,d1
		add.w	d0,d0
		add.w	d1,d0					; multiply d0 by 3
		adda.w	d0,a1
		move.l	(a1)+,(a2)+
		move.w	(a1)+,(a2)+				; write palette
		rts

; ===========================================================================
SSTimingData:	macro time,anim,vram,index,usepalcycle2,extrapalline4
		dc.b	(time-1), (anim), ((vram)*tile_size)>>13
		dc.b	(index)|(usepalcycle2<<7)|(extrapalline4)
		endm

SS_Timing_Values:
		; Time until next, BG mode index, BG namespace address in VRAM, palette offset
		; Flags (if true): use PalCycle_SS_2, affect some extra colors on palette line 4
		SSTimingData  4,  0, ArtTile_SS_Plane_6, $12, TRUE,  FALSE
		SSTimingData  4,  0, ArtTile_SS_Plane_6, $10, TRUE,  FALSE
		SSTimingData  4,  0, ArtTile_SS_Plane_6,  $E, TRUE,  FALSE
		SSTimingData  4,  0, ArtTile_SS_Plane_6,  $C, TRUE,  FALSE
		SSTimingData  4,  0, ArtTile_SS_Plane_6,  $A, TRUE,  TRUE
		SSTimingData  4,  0, ArtTile_SS_Plane_6,   0, TRUE,  FALSE
		SSTimingData  4,  0, ArtTile_SS_Plane_6,   2, TRUE,  FALSE
		SSTimingData  4,  0, ArtTile_SS_Plane_6,   4, TRUE,  FALSE
		SSTimingData  4,  0, ArtTile_SS_Plane_6,   6, TRUE,  FALSE
		SSTimingData  4,  0, ArtTile_SS_Plane_6,   8, TRUE,  FALSE
		SSTimingData  8,  8, ArtTile_SS_Plane_6,   0, FALSE, FALSE
		SSTimingData  8, $A, ArtTile_SS_Plane_6,  $C, FALSE, FALSE
		SSTimingData  0, $C, ArtTile_SS_Plane_6, $18, FALSE, FALSE
		SSTimingData  0, $C, ArtTile_SS_Plane_6, $18, FALSE, FALSE
		SSTimingData  8, $A, ArtTile_SS_Plane_6,  $C, FALSE, FALSE
		SSTimingData  8,  8, ArtTile_SS_Plane_6,   0, FALSE, FALSE

		SSTimingData  4,  0, ArtTile_SS_Plane_5,   8, TRUE,  FALSE
		SSTimingData  4,  0, ArtTile_SS_Plane_5,   6, TRUE,  FALSE
		SSTimingData  4,  0, ArtTile_SS_Plane_5,   4, TRUE,  FALSE
		SSTimingData  4,  0, ArtTile_SS_Plane_5,   2, TRUE,  FALSE
		SSTimingData  4,  0, ArtTile_SS_Plane_5,   0, TRUE,  TRUE
		SSTimingData  4,  0, ArtTile_SS_Plane_5,  $A, TRUE,  FALSE
		SSTimingData  4,  0, ArtTile_SS_Plane_5,  $C, TRUE,  FALSE
		SSTimingData  4,  0, ArtTile_SS_Plane_5,  $E, TRUE,  FALSE
		SSTimingData  4,  0, ArtTile_SS_Plane_5, $10, TRUE,  FALSE
		SSTimingData  4,  0, ArtTile_SS_Plane_5, $12, TRUE,  FALSE
		SSTimingData  8,  2, ArtTile_SS_Plane_5, $24, FALSE, FALSE
		SSTimingData  8,  4, ArtTile_SS_Plane_5, $30, FALSE, FALSE
		SSTimingData  0,  6, ArtTile_SS_Plane_5, $3C, FALSE, FALSE
		SSTimingData  0,  6, ArtTile_SS_Plane_5, $3C, FALSE, FALSE
		SSTimingData  8,  4, ArtTile_SS_Plane_5, $30, FALSE, FALSE
		SSTimingData  8,  2, ArtTile_SS_Plane_5, $24, FALSE, FALSE
		even

; ---------------------------------------------------------------------------

SSBGData:	macro vram,yscroll
		dc.b ((vram)*tile_size)>>10, yscroll
		endm

SS_BG_Modes:
		; FG VRAM, Y scroll direction
		SSBGData ArtTile_SS_Plane_1, 1	;  0 - grid
		SSBGData ArtTile_SS_Plane_2, 0	;  2 - fish morph 1
		SSBGData ArtTile_SS_Plane_2, 1	;  4 - fish morph 2
		SSBGData ArtTile_SS_Plane_3, 0	;  6 - fish
		SSBGData ArtTile_SS_Plane_3, 1	;  8 - bird morph 1
		SSBGData ArtTile_SS_Plane_4, 0	; $A - bird morph 2
		SSBGData ArtTile_SS_Plane_4, 1	; $C - bird
		even

; ===========================================================================

Pal_SSCyc1:	binclude	"palette/Cycle - Special Stage 1.bin"
		even

Pal_SSCyc2:	binclude	"palette/Cycle - Special Stage 2.bin"
		even

; ===========================================================================
; ---------------------------------------------------------------------------
; Special stage background animation subroutine
; ---------------------------------------------------------------------------

SS_BGAnimate:
		move.w	(v_ssbganim).w,d0			; get frame for fish/bird animation
		bne.s	.not_0					; branch if not 0
		move.w	#0,(v_bgscreenposy).w
		move.w	(v_bgscreenposy).w,(v_bgscrposy_vdp).w	; reset vertical scroll for bubble/cloud layer

	.not_0:
		cmpi.w	#8,d0
		bhs.s	SS_BGBirdCloud				; branch if d0 is 8-$C (birds and clouds)
		cmpi.w	#6,d0
		bne.s	.not_6					; branch if d0 isn't 6
		addq.w	#1,(v_bg3screenposx).w
		addq.w	#1,(v_bgscreenposy).w
		move.w	(v_bgscreenposy).w,(v_bgscrposy_vdp).w	; scroll bubble layer

	.not_6:
		moveq	#0,d0
		move.w	(v_bgscreenposx).w,d0
		neg.w	d0
		swap	d0
		lea	(SS_Bubble_WobbleData).l,a1
		lea	(v_ss_scroll_bubbles).w,a3
		moveq	#10-1,d3				; entry count for SS_Bubble_WobbleData

SS_BGWobbleLoop:
		move.w	2(a3),d0				; get next value from buffer
		bsr.w	CalcSine				; convert to sine
		moveq	#0,d2
		move.b	(a1)+,d2				; read 1st byte
		muls.w	d2,d0					; multiply by sine
		asr.l	#8,d0					; divide by $10
		move.w	d0,(a3)+				; write to 1st word of buffer
		move.b	(a1)+,d2				; read 2nd byte
		ext.w	d2
		add.w	d2,(a3)+				; add to 2nd word of buffer
		dbf	d3,SS_BGWobbleLoop

		lea	(v_ss_scroll_bubbles).w,a3
		lea	(SS_Bubble_ScrollBlocks).l,a2
		bra.s	SS_Scroll_CloudsBubbles
; ===========================================================================

SS_BGBirdCloud:
		cmpi.w	#$C,d0
		bne.s	.not_C					; branch if d0 isn't $C
		subq.w	#1,(v_bg3screenposx).w
		lea	(v_ss_scroll_clouds).w,a3
		move.l	#$18000,d2
		moveq	#7-1,d1					; entry count for SS_Cloud_ScrollBlocks

	.loop:
		move.l	(a3),d0
		sub.l	d2,d0
		move.l	d0,(a3)+
		subi.l	#$2000,d2
		dbf	d1,.loop

	.not_C:
		lea	(v_ss_scroll_clouds).w,a3
		lea	(SS_Cloud_ScrollBlocks).l,a2

SS_Scroll_CloudsBubbles:
		lea	(v_hscrolltablebuffer).w,a1
		move.w	(v_bg3screenposx).w,d0
		neg.w	d0
		swap	d0
		moveq	#0,d3
		move.b	(a2)+,d3
		move.w	(v_bgscreenposy).w,d2
		neg.w	d2
		andi.w	#$FF,d2
		lsl.w	#2,d2

	.loop_block:
		move.w	(a3)+,d0
		addq.w	#2,a3
		moveq	#0,d1
		move.b	(a2)+,d1
		subq.w	#1,d1

	.loop_line:
		move.l	d0,(a1,d2.w)
		addq.w	#4,d2
		andi.w	#$3FC,d2
		dbf	d1,.loop_line
		dbf	d3,.loop_block
		rts
; End of function SS_BGAnimate

; ===========================================================================
SS_Bubble_ScrollBlocks:
		dc.b 10-1
		dc.b $28, $18, $10, $28, $18, $10, $30, $18, 8, $10
		even

SS_Cloud_ScrollBlocks:
		dc.b 7-1
		dc.b $30, $30, $30, $28, $18, $18, $18
		even

SS_Bubble_WobbleData:
		dc.b 8, 2
		dc.b 4, -1
		dc.b 2, 3
		dc.b 8, -1
		dc.b 4, 2
		dc.b 2, 3
		dc.b 8, -3
		dc.b 4, 2
		dc.b 2, 3
		dc.b 2, -1
		even
; ===========================================================================
; ===========================================================================
; ---------------------------------------------------------------------------
; Subroutine to show the special stage layout
; ---------------------------------------------------------------------------

SS_ShowLayout:
		bsr.w	SS_AnimateBlocks			; animate walls, rings, and other blocks
		bsr.w	SS_ExecuteAnimationQueue		; animate queued events for touched blocks
; ---------------------------------------------------------------------------

	; --- Calculate the rotated position of the layout grid ---
		move.w	d5,-(sp)				; backup sprites rendered in BuildSprites (which is called before SS_ShowLayout)

		lea	(v_ss_rotationmatrix).w,a1		; set start of rotation buffer (each entry is two words per cell, X/Y axis)
		move.b	(v_ssangle).w,d0			; get current angle of the special stage rotation
		andi.b	#$FC,d0					; snap to nearest multiple of 4 to match stage rotation
		jsr	(CalcSine).l				; get sine and cosine values based on angle
		move.w	d0,d4					; backup sine result
		move.w	d1,d5					; backup cosine result
		muls.w	#ss_blocksize,d4			; d4 = X-rotation delta after each cell
		muls.w	#ss_blocksize,d5			; d5 = Y-rotation delta after each cell

		moveq	#0,d2					; clear d2
		move.w	(v_screenposx).w,d2			; get current camera X-position
		divu.w	#ss_blocksize,d2			; divide camera X-position by block size
		swap	d2					; get remainder (modulo part)
		neg.w	d2					; make remainder negative
		addi.w	#-(ss_matrixsize-1)*ss_blocksize/2,d2	; d2 = base X-offset for all cells (-$B4)

		moveq	#0,d3					; clear d3
		move.w	(v_screenposy).w,d3			; get current camera Y-position
		divu.w	#ss_blocksize,d3			; divide camera X-position by block size
		swap	d3					; get remainder (modulo part)
		neg.w	d3					; make remainder negative
		addi.w	#-(ss_matrixsize-1)*ss_blocksize/2,d3	; d3 = base Y-offset for all cells (-$B4)

		move.w	#ss_matrixsize-1,d7			; calculate rotated positions for all rows
	.rotateRows:
		movem.w	d0-d2,-(sp)				; backup sine, cosine, and X offset per row

		movem.w	d0-d1,-(sp)				; backup sine and cosine
		neg.w	d0					; negate sine for X-rotation term
		muls.w	d2,d1					; X * cos
		muls.w	d3,d0					; Y * -sin
		move.l	d0,d6					; copy
		add.l	d1,d6					; d6 = rotated X-position
		movem.w	(sp)+,d0-d1				; restore sine and cosine
		muls.w	d2,d0					; X * sin
		muls.w	d3,d1					; Y * cos
		add.l	d0,d1					; d1 = rotated Y-position
		move.l	d6,d2					; d2 = rotated X-position

		move.w	#ss_matrixsize-1,d6			; calculate rotated positions for all cells in this row
	.rotateCellsInRow:
		move.l	d2,d0					; get X-position
		asr.l	#8,d0					; shift down a byte
		move.w	d0,(a1)+				; write rotated X-position for cell
		move.l	d1,d0					; get Y-position
		asr.l	#8,d0					; shift down a byte
		move.w	d0,(a1)+				; write rotated Y-position for cell
		add.l	d5,d2					; increase Y-position by cosine Y-delta for next cell
		add.l	d4,d1					; increase Y-position by sine X-delta for next cell
		dbf	d6,.rotateCellsInRow			; loop until all cells for this row have been calculated

		movem.w	(sp)+,d0-d2				; restore sine, cosine, and X offset for next row

		addi.w	#ss_blocksize,d3			; increase base Y-position by block height
		dbf	d7,.rotateRows				; loop until all rows have been calculated

		move.w	(sp)+,d5				; restore number of rendered sprites in BuildSprites

	; --- Insert block types into rotated grid and render them as sprites ---
		lea	(v_sslayout_base).l,a0			; get base pointer for stage layout

		moveq	#0,d0					; clear d0
		move.w	(v_screenposy).w,d0			; get current camera Y-position
		divu.w	#ss_blocksize,d0			; divide camera Y-position by block size
		mulu.w	#ss_layout_rowlength,d0			; multiply by length of rows
		adda.l	d0,a0					; a0 = first row to be rendered

		moveq	#0,d0					; clear d0
		move.w	(v_screenposx).w,d0			; get current camera X-position
		divu.w	#ss_blocksize,d0			; divide camera X-position by block size
		adda.w	d0,a0					; a0 = first row and cell to be rendered

		lea	(v_ss_rotationmatrix).w,a4		; get calculated results in rotation matrix
		move.w	#ss_matrixsize-1,d7			; render 16 rows
	.loopAllRows:
		move.w	#ss_matrixsize-1,d6			; render 16 blocks per row
	.loopRow:
		moveq	#0,d0					; clear d0
		move.b	(a0)+,d0				; get next block ID
		beq.s	.nextBlock				; if it's a blank block, branch
		cmpi.b	#id_SS_Glass_Ani4,d0			; is block ID greater than the last possible one? ($4E)
		bhi.s	.nextBlock				; if yes, render empty block instead

		move.w	(a4),d3					; get rotated X-position for this cell
		addi.w	#128+(320/2),d3				; d3 = sprite X-position
		cmpi.w	#128-16,d3				; is sprite offscreen to the left?
		blo.s	.nextBlock				; if yes, skip drawing
		cmpi.w	#128+320+16,d3				; is sprite offscreen to the right?
		bhs.s	.nextBlock				; if yes, skip drawing

		move.w	2(a4),d2				; get rotated Y-position for this cell
		addi.w	#128+(224/2),d2				; d2 = sprite Y-position
		cmpi.w	#128-16,d2				; is sprite offscreen to the top?
		blo.s	.nextBlock				; if yes, skip drawing
		cmpi.w	#128+224+16,d2				; is sprite offscreen to the bottom?
		bhs.s	.nextBlock				; if yes, skip drawing

		lea	(v_ss_spritesettings).l,a5		; load block definitions array
		lsl.w	#3,d0					; multiply by 8 bytes per entry
		lea	(a5,d0.w),a5				; get data for block ID
		movea.l	(a5)+,a1				; get mappings pointer
		move.w	(a5)+,d1				; get frame ID
		add.w	d1,d1					; double for word-based indexing
		adda.w	(a1,d1.w),a1				; get mappings for current frame
		movea.w	(a5)+,a3				; get art tile / VRAM settings
		moveq	#1-1,d1					; write 1 sprite piece by default
		move.b	(a1)+,d1				; get number of sprite pieces in frame
		subq.b	#1,d1					; subtract 1 for dbf
		bmi.s	.nextBlock				; if result underflowed, this is was blank frame mapping, branch
		jsr	(BuildSpr_Normal).l			; write data from sprite pieces to buffer (never flipped)

	.nextBlock:
		addq.w	#4,a4					; advance to next entry in rotation matrix
		dbf	d6,.loopRow				; loop until all blocks in row have been rendered
		lea	spritelayer_size-ss_matrixsize(a0),a0	; advance to next row ($80 bytes - 16 bytes that were already advanced)
		dbf	d7,.loopAllRows				; loop until all rows were rendered

		move.b	d5,(v_spritecount).w			; write total number of rendered sprites to debug value

		cmpi.b	#sprites_max,d5				; check sprite limit (Mega Drive can only handle 80 at a time)
		beq.s	.spriteLimit				; if all sprite slots are taken up, abort process

		move.l	#0,(a2)					; unlink last sprite
		rts						; return
; ---------------------------------------------------------------------------

	.spriteLimit:
		move.b	#0,-5(a2)				; unlink penultimate sprite
		rts						; return
; End of function SS_ShowLayout


; ===========================================================================
; ---------------------------------------------------------------------------
; Subroutine to animate blocks (walls, rings, etc.) in the Special Stage
; ---------------------------------------------------------------------------

; SS_AniWallsRings:
SS_AnimateBlocks:
	; --- Rotate square walls ---
		lea	(v_ss_spritesettings+8+5-1).l,a1	; load sprite settings array, skip blank and target frame ID (word, +5-1)
		moveq	#0,d0					; clear d0
		move.b	(v_ssangle).w,d0			; get current rotation angle
		lsr.b	#2,d0					; divide by 4 (walls are snapped to multiples of 4 degrees)
		andi.w	#$F,d0					; limit to 16 rotations
		moveq	#id_SS_WallGreen_8-1,d1			; rotate all wall blocks (id_SS_WallGreen_8 = last one = $24)
	.rotateWalls:
		move.w	d0,(a1)					; set new frame ID to rotated one
		addq.w	#8,a1					; advance to next wall sprite setting
		dbf	d1,.rotateWalls				; loop until all walls have been rotated

	; --- Animate rings (8 frames) ---
		lea	(v_ss_spritesettings+5).l,a1		; load sprite settings array, target frame ID (byte, +5)
		subq.b	#1,(v_ani1_time).w			; decrement delay until ring animation needs to update
		bpl.s	.updateRingFrame			; if time remains, branch
		move.b	#8-1,(v_ani1_time).w			; reset delay
		addq.b	#1,(v_ani1_frame).w			; advance frame ID
		andi.b	#3,(v_ani1_frame).w			; wrap around every 8 frames
	.updateRingFrame:
		move.b	(v_ani1_frame).w,8*id_SS_Ring(a1)	; set new ring frame ID

	; --- Animate various other blocks (2 frames) ---
		subq.b	#1,(v_ani2_time).w			; decrement delay until frames need to update
		bpl.s	.updateAlternatingFrames		; if time remains, branch
		move.b	#8-1,(v_ani2_time).w			; reset delay
		addq.b	#1,(v_ani2_frame).w			; advance frame ID
		andi.b	#1,(v_ani2_frame).w			; alternate between only two frames
	.updateAlternatingFrames:
		move.b	(v_ani2_frame).w,d0			; get current alternating frame ID
		move.b	d0,8*id_SS_GOAL(a1)			; animate goal blocks
		move.b	d0,8*id_SS_RedWhite(a1)			; animate red/white blocks
		move.b	d0,8*id_SS_UP(a1)			; animate UP blocks
		move.b	d0,8*id_SS_DOWN(a1)			; animate DOWN blocks
		move.b	d0,8*id_SS_Emerald1_Blue(a1)		; animate emerald 1 (blue)
		move.b	d0,8*id_SS_Emerald2_Yellow(a1)		; animate emerald 2 (yellow)
		move.b	d0,8*id_SS_Emerald3_Pink(a1)		; animate emerald 3 (pink)
		move.b	d0,8*id_SS_Emerald4_Green(a1)		; animate emerald 4 (green)
		move.b	d0,8*id_SS_Emerald5_Red(a1)		; animate emerald 5 (red)
		move.b	d0,8*id_SS_Emerald6_Grey(a1)		; animate emerald 6 (grey)

	; --- Animate glass blocks (8 frames) ---
		subq.b	#1,(v_ani3_time).w			; decrement delay until glass frames needs to update
		bpl.s	.updateGlassFrames			; if time remains, branch
		move.b	#5-1,(v_ani3_time).w			; reset delay
		addq.b	#1,(v_ani3_frame).w			; advance frame ID
		andi.b	#3,(v_ani3_frame).w			; wrap around every 8 frames
	.updateGlassFrames:
		move.b	(v_ani3_frame).w,d0			; get current glass frame ID
		move.b	d0,8*id_SS_Glass1_Blue(a1)		; update glass block 1 (blue)
		move.b	d0,8*id_SS_Glass2_Green(a1)		; update glass block 2 (green)
		move.b	d0,8*id_SS_Glass3_Yellow(a1)		; update glass block 3 (yellow)
		move.b	d0,8*id_SS_Glass4_Pink(a1)		; update glass block 4 (pink)

	; ---  Animate wall palette cycle (unlike the other animations above, this affects VRAM settings instead of frame ID) ---
		subq.b	#1,(v_ani0_time).w			; decrement delay until wall palettes need to update
		bpl.s	.updateWallPalettes			; if time remains, branch
		move.b	#8-1,(v_ani0_time).w			; reset delay
		subq.b	#1,(v_ani0_frame).w			; advance frame ID (backwards)
		andi.b	#7,(v_ani0_frame).w			; wrap around every 8 frames
	.updateWallPalettes:
		lea	(v_ss_spritesettings+8+8+6).l,a1	; load sprite settings array, skip blank, first wall, and target VRAM settings (word, +6)
		lea	(SS_Wall_Palettes_VRAM).l,a0		; load wall VRAM settings, containing the palette line bits
		moveq	#0,d0					; clear d0
		move.b	(v_ani0_frame).w,d0			; get current frame
		add.w	d0,d0					; double for word-based indexing
		lea	(a0,d0.w),a0				; jump to current start in VRAM settings array

	rept 4	; Repeated four times to account for the four sets of walls (blue, yellow, green, pink)
		move.w	$0(a0),8*0(a1)				; update wall 1
		move.w	$2(a0),8*1(a1)				; update wall 2
		move.w	$4(a0),8*2(a1)				; update wall 3
		move.w	$6(a0),8*3(a1)				; update wall 4
		move.w	$8(a0),8*4(a1)				; update wall 5
		move.w	$A(a0),8*5(a1)				; update wall 6
		move.w	$C(a0),8*6(a1)				; update wall 7
		move.w	$E(a0),8*7(a1)				; update wall 8

		adda.w	#2*$10,a0				; advance to next set of VRAM settings for next wall set
		adda.w	#8*9,a1					; advance to next set of walls (0 walls are skipped, thus never changing palette)
	endr

		rts						; return

; ---------------------------------------------------------------------------
; Palette cycle data for square blocks in Special Stages.
; - Four sets for the four wall types. Each set has same blinking pattern:
;   nBnnnnnB twice (n = normal palette, B = blinking palette)
; - Blinking palette line is the one before the normal one (i.e. -1)
; - All values are technically complete VRAM settings with the art tile,
;   but the only difference between each value is the palette line
; ---------------------------------------------------------------------------

sswallpal: macro paletteline
	set normal, ArtTile_SS_Wall|(paletteline<<13)
	set blink,  ArtTile_SS_Wall|(((paletteline-1)&3)<<13)
	rept 2
		dc.w normal,  blink, normal, normal
		dc.w normal, normal, normal,  blink
	endr
	endm

; SS_WaRiVramSet:
SS_Wall_Palettes_VRAM:
		sswallpal 0	; blue walls
		sswallpal 1	; yellow walls
		sswallpal 2	; green walls
		sswallpal 3	; pink walls
		even
; End of function SS_AnimateBlocks


; ===========================================================================
; ---------------------------------------------------------------------------
; Subroutine to	find a free slot in the Special Stage sprite update list,
; used to animate blocks collected/touched by Sonic.
; ---------------------------------------------------------------------------

; SS_RemoveCollectedItem: <-- old misnomer
SS_FindFreeAnimationSlot:
		lea	(v_ss_animations).l,a2			; address of sprite update list
		move.w	#(v_ss_animations_end-v_ss_animations)/8-1,d0 ; up to $20 slots

	.loop:
		tst.b	(a2)					; is slot free?
		beq.s	.return					; if yes, exit with it
		addq.w	#8,a2					; go to next slot
		dbf	d0,.loop				; try again

	.return:
		rts						; return with slot in a2
; End of function SS_FindFreeAnimationSlot


; ===========================================================================
; ---------------------------------------------------------------------------
; Subroutine to animate special stage items when you touch them.
; This system uses a buffer of animation events that are added through the
; above SS_FindFreeAnimationSlot subroutine.
;
; Each slot is 8 bytes in size, broken down like so:
;	0   - animation ID (1-based; zero implies empty slot)
;	1   - (unused)
;	2   - frame delay between animation advancements
;	3   - current index ID in animation script
;	4-7 - RAM location of target block in stage layout
; ---------------------------------------------------------------------------
ss_ani_id:	equ 0
ss_ani_delay:	equ 2
ss_ani_frame:	equ 3
ss_ani_block:	equ 4
; ---------------------------------------------------------------------------

; SS_AniItems:
SS_ExecuteAnimationQueue:
		lea	(v_ss_animations).l,a0			; load start address of animation event buffer
		move.w	#(v_ss_animations_end-v_ss_animations)/8-1,d7 ; set to iterate through all slots

	.loop:
		moveq	#0,d0					; clear d0
		move.b	ss_ani_id(a0),d0			; get potential animation event
		beq.s	.nextslot				; if slot has none, branch
		lsl.w	#2,d0					; multiply ID by 4 for long-based indexing
		movea.l	SS_AniIndex-4(pc,d0.w),a1		; get animation entry in jump table (-4 because these IDs are 1-based)
		jsr	(a1)					; execute animation and return

	.nextslot:
		addq.w	#8,a0					; go to next animation event slot
		dbf	d7,.loop				; loop until all event slots were checked
		rts						; return

; ===========================================================================
SS_AniIndex:	dc.l SS_AniRingSparks				; animation ID 1
		dc.l SS_AniBumper				; animation ID 2
		dc.l SS_Ani1Up					; animation ID 3
		dc.l SS_AniReverse				; animation ID 4
		dc.l SS_AniEmeraldSparks			; animation ID 5
		dc.l SS_AniGlassBlock				; animation ID 6
; ===========================================================================

SS_AniRingSparks:
		subq.b	#1,ss_ani_delay(a0)			; decrement delay until next animation advancement
		bpl.s	.return					; if time remains, branch
		move.b	#5,ss_ani_delay(a0)			; reset delay

		moveq	#0,d0					; clear d0
		move.b	ss_ani_frame(a0),d0			; get current frame index in animation script
		addq.b	#1,ss_ani_frame(a0)			; advance to next frame index
		movea.l	ss_ani_block(a0),a1			; get location of block in RAM
		move.b	SS_AniRingData(pc,d0.w),d0		; retrieve new block ID from animation script
		move.b	d0,(a1)					; update block in layout
		bne.s	.return					; if animation isn't finished, branch

		clr.l	(a0)					; clear animation event slot
		clr.l	ss_ani_block(a0)			; ''

	.return:
		rts						; return
; ===========================================================================
SS_AniRingData:	dc.b id_SS_Ring_Ani1, id_SS_Ring_Ani2, id_SS_Ring_Ani3, id_SS_Ring_Ani4, 0
		even
; ===========================================================================

SS_AniBumper:
		subq.b	#1,ss_ani_delay(a0)			; decrement delay until next animation advancement
		bpl.s	.return					; if time remains, branch
		move.b	#7,ss_ani_delay(a0)			; reset delay

		moveq	#0,d0					; clear d0
		move.b	ss_ani_frame(a0),d0			; get current frame index in animation script
		addq.b	#1,ss_ani_frame(a0)			; advance to next frame index
		movea.l	ss_ani_block(a0),a1			; get location of block in RAM
		move.b	SS_AniBumpData(pc,d0.w),d0		; retrieve new block ID from animation script
		bne.s	.animating				; if animation isn't finished, branch

		clr.l	(a0)					; clear animation event slot
		clr.l	ss_ani_block(a0)			; ''

		move.b	#id_SS_Bumper,(a1)			; reset bumper block to default idle one
		rts						; return
; ---------------------------------------------------------------------------

	.animating:
		move.b	d0,(a1)					; update block in layout

	.return:
		rts						; return
; ===========================================================================
SS_AniBumpData:	dc.b id_SS_Bumper_Ani1, id_SS_Bumper_Ani2, id_SS_Bumper_Ani1, id_SS_Bumper_Ani2, 0
		even
; ===========================================================================

SS_Ani1Up:
		subq.b	#1,ss_ani_delay(a0)			; decrement delay until next animation advancement
		bpl.s	.return					; if time remains, branch
		move.b	#5,ss_ani_delay(a0)			; reset delay

		moveq	#0,d0					; clear d0
		move.b	ss_ani_frame(a0),d0			; get current frame index in animation script
		addq.b	#1,ss_ani_frame(a0)			; advance to next frame index
		movea.l	ss_ani_block(a0),a1			; get location of block in RAM
		move.b	SS_Ani1UpData(pc,d0.w),d0		; retrieve new block ID from animation script
		move.b	d0,(a1)					; update block in layout
		bne.s	.return					; if animation isn't finished, branch

		clr.l	(a0)					; clear animation event slot
		clr.l	ss_ani_block(a0)			; ''

	.return:
		rts						; return
; ===========================================================================
SS_Ani1UpData:	dc.b id_SS_Emerald_Ani1, id_SS_Emerald_Ani2, id_SS_Emerald_Ani3, id_SS_Emerald_Ani4, 0
		even
; ===========================================================================

SS_AniReverse:
		subq.b	#1,ss_ani_delay(a0)			; decrement delay until next animation advancement
		bpl.s	.return					; if time remains, branch
		move.b	#7,ss_ani_delay(a0)			; reset delay

		moveq	#0,d0					; clear d0
		move.b	ss_ani_frame(a0),d0			; get current frame index in animation script
		addq.b	#1,ss_ani_frame(a0)			; advance to next frame index
		movea.l	ss_ani_block(a0),a1			; get location of block in RAM
		move.b	SS_AniRevData(pc,d0.w),d0		; retrieve new block ID from animation script
		bne.s	.animating				; if animation isn't finished, branch

		clr.l	(a0)					; clear animation event slot
		clr.l	ss_ani_block(a0)			; ''

		move.b	#id_SS_R,(a1)				; reset R block to default idle one
		rts
; ---------------------------------------------------------------------------

	.animating:
		move.b	d0,(a1)					; update block in layout

	.return:
		rts						; return
; ===========================================================================
SS_AniRevData:	dc.b id_SS_R, id_SS_R_Ani, id_SS_R, id_SS_R_Ani, 0
		even
; ===========================================================================

SS_AniEmeraldSparks:
		subq.b	#1,ss_ani_delay(a0)			; decrement delay until next animation advancement
		bpl.s	.return					; if time remains, branch
		move.b	#5,ss_ani_delay(a0)			; reset delay

		moveq	#0,d0					; clear d0
		move.b	ss_ani_frame(a0),d0			; get current frame index in animation script
		addq.b	#1,ss_ani_frame(a0)			; advance to next frame index
		movea.l	ss_ani_block(a0),a1			; get location of block in RAM
		move.b	SS_AniEmerData(pc,d0.w),d0		; retrieve new block ID from animation script
		move.b	d0,(a1)					; update block in layout
		bne.s	.return					; if animation isn't finished, branch

		clr.l	(a0)					; clear animation event slot
		clr.l	ss_ani_block(a0)			; ''

		move.b	#4,(v_player+obRoutine).w		; set object 09 to SonicSS_ExitStage (this triggers the actual exit)
		move.w	#sfx_SSGoal,d0				; set special stage GOAL sound
		jsr	(QueueSound2).l				; play it

	.return:
		rts						; return
; ===========================================================================
SS_AniEmerData:	dc.b id_SS_Emerald_Ani1, id_SS_Emerald_Ani2, id_SS_Emerald_Ani3, id_SS_Emerald_Ani4, 0
		even
; ===========================================================================

SS_AniGlassBlock:
		subq.b	#1,ss_ani_delay(a0)			; decrement delay until next animation advancement
		bpl.s	.return					; if time remains, branch
		move.b	#1,ss_ani_delay(a0)			; reset delay

		moveq	#0,d0					; clear d0
		move.b	ss_ani_frame(a0),d0			; get current frame index in animation script
		addq.b	#1,ss_ani_frame(a0)			; advance to next frame index
		movea.l	ss_ani_block(a0),a1			; get location of block in RAM
		move.b	SS_AniGlassData(pc,d0.w),d0		; retrieve new block ID from animation script
		move.b	d0,(a1)					; update block in layout
		bne.s	.return					; if animation isn't finished, branch

		move.b	ss_ani_block(a0),(a1)			; update glass with weaker version (see SonicSS_GlassUpdate)

		clr.l	(a0)					; clear animation event slot
		clr.l	ss_ani_block(a0)			; ''

	.return:
		rts						; return
; ===========================================================================
SS_AniGlassData:dc.b id_SS_Glass_Ani1, id_SS_Glass_Ani2, id_SS_Glass_Ani3, id_SS_Glass_Ani4
		dc.b id_SS_Glass_Ani1, id_SS_Glass_Ani2, id_SS_Glass_Ani3, id_SS_Glass_Ani4, 0
		even
; ===========================================================================

; End of function SS_AniItems


; ===========================================================================
; ---------------------------------------------------------------------------
; Special stage layout pointers
; ---------------------------------------------------------------------------
SS_LayoutIndex:
		dc.l SS_1
		dc.l SS_2
		dc.l SS_3
		dc.l SS_4
		dc.l SS_5
		dc.l SS_6

	if ((*-SS_LayoutIndex)/4<>ss_emeralds_num)&(MOMPASS=1)
		warning "SS_LayoutIndex does not match expected emerald count!"
	endif

		even

; ---------------------------------------------------------------------------
; Special stage start locations
; (Previously separated into "_inc/Start Location Array - Special Stages.asm")
; ---------------------------------------------------------------------------

SS_StartLoc:
		binclude	"startpos/Special Stages/ss1.bin"
		binclude	"startpos/Special Stages/ss2.bin"
		binclude	"startpos/Special Stages/ss3.bin"
		binclude	"startpos/Special Stages/ss4.bin"
		binclude	"startpos/Special Stages/ss5.bin"
		binclude	"startpos/Special Stages/ss6.bin"

	if ((*-SS_StartLoc)/4<>ss_emeralds_num)&(MOMPASS=1)
		warning "SS_StartLoc does not match expected emerald count!"
	endif

		even

; ---------------------------------------------------------------------------
; Subroutine to load special stage layout
; ---------------------------------------------------------------------------

SS_Load:
		moveq	#0,d0					; clear d0
		move.b	(v_lastspecial).w,d0			; load number of last special stage entered (0-5)
		addq.b	#1,(v_lastspecial).w			; remember new last-visited special stage number
		cmpi.b	#ss_emeralds_num,(v_lastspecial).w	; has it wrapped over the maximum?
		blo.s	SS_FindUnbeatenStage			; if not, branch
		move.b	#0,(v_lastspecial).w			; reset if higher than 6

SS_FindUnbeatenStage:
		; Skip special stages whose emerald has already been collected. The game cycles through stage IDs
		; using v_lastspecial: If the selected stage matches an entry in v_emldlist, execution branches
		; back to SS_Load and the next stage is selected. This repeats until an unbeaten stage is found.
		cmpi.b	#ss_emeralds_num,(v_emeralds).w		; do you already have all emeralds?
		beq.s	SS_LoadData				; if yes, load stage anyway (should not happen, probably a failsafe)
		moveq	#0,d1					; clear d1
		move.b	(v_emeralds).w,d1			; get number of already collected emeralds
		subq.b	#1,d1					; subtract 1 for dbf
		blo.s	SS_LoadData				; if it underflowed, no emeralds have been collected so far
		lea	(v_emldlist).w,a3			; load array of already collected emeralds
	.chkEmldLoop:
		cmp.b	(a3,d1.w),d0				; does this collected emerald belong to the selected stage?
		bne.s	.chkNext				; if not, check the next entry
		bra.s	SS_Load					; repeat SS_Load, increase v_lastspecial, try another stage
	.chkNext:
		dbf	d1,.chkEmldLoop				; check all collected emeralds

; ---------------------------------------------------------------------------

; d0 = special stage to load (0-5)
SS_LoadData:
	; --- Load start positions for Sonic ---
		lsl.w	#2,d0					; multiply by 4 for long-based indexing
		lea	SS_StartLoc(pc,d0.w),a1			; load Sonic's start location for this stage
		move.w	(a1)+,(v_player+obX).w			; set start X-position
		move.w	(a1)+,(v_player+obY).w			; set start Y-position

	; --- Decompress Enigma-compressed special stage layout to a temporary buffer ---
		movea.l	SS_LayoutIndex(pc,d0.w),a0		; load compressed special stage layout
		lea	(v_sslayout_decompress).l,a1		; set decompression buffer for layout
		move.w	#0,d0					; no added art tile settings
		jsr	(EniDec).l				; decompress special stage layout to buffer

	; --- Fully clear target layout buffer ---
		lea	(v_sslayout_base).l,a1			; set start address of layout RAM
		move.w	#(v_sslayout_decompress-v_sslayout_base)/4-1,d0 ; clear the entire layout buffer
	.clearLayoutBuffer:
		clr.l	(a1)+					; clear four bytes
		dbf	d0,.clearLayoutBuffer			; loop until buffer has been cleared

	; --- Copy decompressed layout to the final buffer, inserting $40 bytes of padding per row ---
		lea	(v_sslayout_actual).l,a1		; set target layout destination after padding
		lea	(v_sslayout_decompress).l,a0		; load decompressed layout data
		moveq	#(v_sslayout_end-v_sslayout_actual)/ss_layout_rowlength-1,d1 ; transfer the full layout
	.copyAllRows:
		moveq	#(ss_layout_rowlength/2)-1,d2		; set to transfer one row ($40 bytes of actual data)
	.copyRow:
		move.b	(a0)+,(a1)+				; transfer one cell to final layout buffer
		dbf	d2,.copyRow				; loop until row has been transferred
		lea	ss_layout_rowlength/2(a1),a1		; advance to next row ($40 bytes of padding)
		dbf	d1,.copyAllRows				; loop until all rows have been transferred

	; --- Load all sprite settings from SS_MapIndex into v_ss_spritesettings ---
		lea	(v_ss_spritesettings+8).l,a1		; skip first entry (for block $00 / blank)
		lea	(SS_MapIndex).l,a0			; load block sprite info definitions
		moveq	#(SS_MapIndex_End-SS_MapIndex)/6-1,d1	; load all entries in definitions list
	.loadSpriteSettings:
		move.l	(a0)+,(a1)+				; copy frame ID and mappings pointer
		move.w	#0,(a1)+				; prepare two empty bytes (upper one remains unused)
		move.b	-4(a0),-1(a1)				; copy frame ID to lower byte
		move.w	(a0)+,(a1)+				; load VRAM settings (palette and art tile)
		dbf	d1,.loadSpriteSettings			; loop until all sprite settings have been loaded

	; --- Fully clear animations processing queue ---
		lea	(v_ss_animations).l,a1			; set start address of animations queue
		move.w	#(v_ss_animations_end-v_ss_animations)/4-1,d1 ; clear the entire queue
	.clearAnimationQueue:
		clr.l	(a1)+					; clear four bytes
		dbf	d1,.clearAnimationQueue			; loop until queue has been cleared

		rts						; return
; End of function SS_Load
; ===========================================================================
; ---------------------------------------------------------------------------
; Special stage mappings and VRAM pointers (loaded into v_ss_spritesettings)
; ---------------------------------------------------------------------------

SS_MapIndex:

specialStageData: macro frame,mappings,palette,vram,{INTLABEL}
__LABEL__:	label	(*-SS_MapIndex)/(4+2)+1
		dc.l	(frame<<24)|mappings
		dc.w	palette|vram
		endm

; Blank block is implicitly added to v_ss_spritesettings by skipping over the first 8 bytes
; id_SS_Blank:		specialStageData	0, 0		  0,	     0				; $00 - blank block

; Square wall blocks (0th blocks per color are static and don't animate)
id_SS_WallBlue_0:	specialStageData	0, Map_SSWalls,   Tile_Pal1, ArtTile_SS_Wall		; $01 - wall block (blue)
id_SS_WallBlue_1:	specialStageData	0, Map_SSWalls,   Tile_Pal1, ArtTile_SS_Wall		; $02 - ''
id_SS_WallBlue_2:	specialStageData	0, Map_SSWalls,   Tile_Pal1, ArtTile_SS_Wall		; $03 - ''
id_SS_WallBlue_3:	specialStageData	0, Map_SSWalls,   Tile_Pal1, ArtTile_SS_Wall		; $04 - ''
id_SS_WallBlue_4:	specialStageData	0, Map_SSWalls,   Tile_Pal1, ArtTile_SS_Wall		; $05 - ''
id_SS_WallBlue_5:	specialStageData	0, Map_SSWalls,   Tile_Pal1, ArtTile_SS_Wall		; $06 - ''
id_SS_WallBlue_6:	specialStageData	0, Map_SSWalls,   Tile_Pal1, ArtTile_SS_Wall		; $07 - ''
id_SS_WallBlue_7:	specialStageData	0, Map_SSWalls,   Tile_Pal1, ArtTile_SS_Wall		; $08 - ''
id_SS_WallBlue_8:	specialStageData	0, Map_SSWalls,   Tile_Pal1, ArtTile_SS_Wall		; $09 - ''

id_SS_WallYellow_0:	specialStageData	0, Map_SSWalls,   Tile_Pal2, ArtTile_SS_Wall		; $0A - wall block (yellow)
id_SS_WallYellow_1:	specialStageData	0, Map_SSWalls,   Tile_Pal2, ArtTile_SS_Wall		; $0B - ''
id_SS_WallYellow_2:	specialStageData	0, Map_SSWalls,   Tile_Pal2, ArtTile_SS_Wall		; $0C - ''
id_SS_WallYellow_3:	specialStageData	0, Map_SSWalls,   Tile_Pal2, ArtTile_SS_Wall		; $0D - ''
id_SS_WallYellow_4:	specialStageData	0, Map_SSWalls,   Tile_Pal2, ArtTile_SS_Wall		; $0E - ''
id_SS_WallYellow_5:	specialStageData	0, Map_SSWalls,   Tile_Pal2, ArtTile_SS_Wall		; $0F - ''
id_SS_WallYellow_6:	specialStageData	0, Map_SSWalls,   Tile_Pal2, ArtTile_SS_Wall		; $10 - ''
id_SS_WallYellow_7:	specialStageData	0, Map_SSWalls,   Tile_Pal2, ArtTile_SS_Wall		; $11 - ''
id_SS_WallYellow_8:	specialStageData	0, Map_SSWalls,   Tile_Pal2, ArtTile_SS_Wall		; $12 - ''

id_SS_WallPink_0:	specialStageData	0, Map_SSWalls,   Tile_Pal3, ArtTile_SS_Wall		; $13 - wall block (pink)
id_SS_WallPink_1:	specialStageData	0, Map_SSWalls,   Tile_Pal3, ArtTile_SS_Wall		; $14 - ''
id_SS_WallPink_2:	specialStageData	0, Map_SSWalls,   Tile_Pal3, ArtTile_SS_Wall		; $15 - ''
id_SS_WallPink_3:	specialStageData	0, Map_SSWalls,   Tile_Pal3, ArtTile_SS_Wall		; $16 - ''
id_SS_WallPink_4:	specialStageData	0, Map_SSWalls,   Tile_Pal3, ArtTile_SS_Wall		; $17 - ''
id_SS_WallPink_5:	specialStageData	0, Map_SSWalls,   Tile_Pal3, ArtTile_SS_Wall		; $18 - ''
id_SS_WallPink_6:	specialStageData	0, Map_SSWalls,   Tile_Pal3, ArtTile_SS_Wall		; $19 - ''
id_SS_WallPink_7:	specialStageData	0, Map_SSWalls,   Tile_Pal3, ArtTile_SS_Wall		; $1A - ''
id_SS_WallPink_8:	specialStageData	0, Map_SSWalls,   Tile_Pal3, ArtTile_SS_Wall		; $1B - ''

id_SS_WallGreen_0:	specialStageData	0, Map_SSWalls,   Tile_Pal4, ArtTile_SS_Wall		; $1C - wall block (green)
id_SS_WallGreen_1:	specialStageData	0, Map_SSWalls,   Tile_Pal4, ArtTile_SS_Wall		; $1D - ''
id_SS_WallGreen_2:	specialStageData	0, Map_SSWalls,   Tile_Pal4, ArtTile_SS_Wall		; $1E - ''
id_SS_WallGreen_3:	specialStageData	0, Map_SSWalls,   Tile_Pal4, ArtTile_SS_Wall		; $1F - ''
id_SS_WallGreen_4:	specialStageData	0, Map_SSWalls,   Tile_Pal4, ArtTile_SS_Wall		; $20 - ''
id_SS_WallGreen_5:	specialStageData	0, Map_SSWalls,   Tile_Pal4, ArtTile_SS_Wall		; $21 - ''
id_SS_WallGreen_6:	specialStageData	0, Map_SSWalls,   Tile_Pal4, ArtTile_SS_Wall		; $22 - ''
id_SS_WallGreen_7:	specialStageData	0, Map_SSWalls,   Tile_Pal4, ArtTile_SS_Wall		; $23 - ''
id_SS_WallGreen_8:	specialStageData	0, Map_SSWalls,   Tile_Pal4, ArtTile_SS_Wall		; $24 - ''

; Solid action blocks
id_SS_Bumper:		specialStageData	0, Map_Bump,      Tile_Pal1, ArtTile_SS_Bumper		; $25 - bumper (idle)
id_SS_W:		specialStageData	0, Map_SS_Shared, Tile_Pal1, ArtTile_SS_W_Block		; $26 - W block (unused)
id_SS_GOAL:		specialStageData	0, Map_SS_Shared, Tile_Pal1, ArtTile_SS_Goal		; $27 - GOAL block
id_SS_1Up:		specialStageData	0, Map_SS_Shared, Tile_Pal1, ArtTile_SS_Extra_Life	; $28 - 1-Up block (hardcoded to be non-solid)
id_SS_UP:		specialStageData	0, Map_SS_Up,     Tile_Pal1, ArtTile_SS_Up_Down		; $29 - UP block
id_SS_DOWN:		specialStageData	0, Map_SS_Down,   Tile_Pal1, ArtTile_SS_Up_Down		; $2A - DOWN block
id_SS_R:		specialStageData	0, Map_SS_Shared, Tile_Pal2, ArtTile_SS_R_Block		; $2B - R block (idle)
id_SS_RedWhite:		specialStageData	0, Map_SS_Glass,  Tile_Pal1, ArtTile_SS_Red_White_Block	; $2C - red/white block
id_SS_Glass1_Blue:	specialStageData	0, Map_SS_Glass,  Tile_Pal1, ArtTile_SS_Glass		; $2D - glass block (blue)
id_SS_Glass2_Green:	specialStageData	0, Map_SS_Glass,  Tile_Pal4, ArtTile_SS_Glass		; $2E - ''          (green)
id_SS_Glass3_Yellow:	specialStageData	0, Map_SS_Glass,  Tile_Pal2, ArtTile_SS_Glass		; $2F - ''          (yellow)
id_SS_Glass4_Pink:	specialStageData	0, Map_SS_Glass,  Tile_Pal3, ArtTile_SS_Glass		; $30 - ''          (pink)
id_SS_R_Ani:		specialStageData	0, Map_SS_Shared, Tile_Pal1, ArtTile_SS_R_Block		; $31 - R block (touched)
id_SS_Bumper_Ani1:	specialStageData	1, Map_Bump,      Tile_Pal1, ArtTile_SS_Bumper		; $32 - bumper (touched 1)
id_SS_Bumper_Ani2:	specialStageData	2, Map_Bump,      Tile_Pal1, ArtTile_SS_Bumper		; $33 - ''     (touched 2)
id_SS_ZONE1:		specialStageData	0, Map_SS_Shared, Tile_Pal1, ArtTile_SS_Zone_1		; $34 - ZONE 1 block (unused)
id_SS_ZONE2:		specialStageData	0, Map_SS_Shared, Tile_Pal1, ArtTile_SS_Zone_2		; $35 - ''   2 block (unused)
id_SS_ZONE3:		specialStageData	0, Map_SS_Shared, Tile_Pal1, ArtTile_SS_Zone_3		; $36 - ''   3 block (unused)
id_SS_ZONE4:		specialStageData	0, Map_SS_Shared, Tile_Pal1, ArtTile_SS_Zone_4		; $37 - ''   4 block (unused)
id_SS_ZONE5:		specialStageData	0, Map_SS_Shared, Tile_Pal1, ArtTile_SS_Zone_5		; $38 - ''   5 block (unused)
id_SS_ZONE6:		specialStageData	0, Map_SS_Shared, Tile_Pal1, ArtTile_SS_Zone_6		; $39 - ''   6 block (unused)

; Non-solid action blocks
id_SS_Ring:		specialStageData	0, Map_Ring,      Tile_Pal2, ArtTile_Ring		; $3A - ring
id_SS_Emerald1_Blue:	specialStageData	0, Map_SS_Chaos3, Tile_Pal1, ArtTile_SS_Emerald		; $3B - emerald (blue)
id_SS_Emerald2_Yellow:	specialStageData	0, Map_SS_Chaos3, Tile_Pal2, ArtTile_SS_Emerald		; $3C - ''      (yellow)
id_SS_Emerald3_Pink:	specialStageData	0, Map_SS_Chaos3, Tile_Pal3, ArtTile_SS_Emerald		; $3D - ''      (pink)
id_SS_Emerald4_Green:	specialStageData	0, Map_SS_Chaos3, Tile_Pal4, ArtTile_SS_Emerald		; $3E - ''      (green)
id_SS_Emerald5_Red:	specialStageData	0, Map_SS_Chaos1, Tile_Pal1, ArtTile_SS_Emerald		; $3F - ''      (red)
id_SS_Emerald6_Grey:	specialStageData	0, Map_SS_Chaos2, Tile_Pal1, ArtTile_SS_Emerald		; $40 - ''      (grey)
id_SS_Ghost:		specialStageData	0, Map_SS_Shared, Tile_Pal1, ArtTile_SS_Ghost_Block	; $41 - ghost block
id_SS_Ring_Ani1:	specialStageData	4, Map_Ring,      Tile_Pal2, ArtTile_Ring		; $42 - ring (sparkle when collecting)
id_SS_Ring_Ani2:	specialStageData	5, Map_Ring,      Tile_Pal2, ArtTile_Ring		; $43 - ''
id_SS_Ring_Ani3:	specialStageData	6, Map_Ring,      Tile_Pal2, ArtTile_Ring		; $44 - ''
id_SS_Ring_Ani4:	specialStageData	7, Map_Ring,      Tile_Pal2, ArtTile_Ring		; $45 - ''
id_SS_Emerald_Ani1:	specialStageData	0, Map_SS_Glass,  Tile_Pal2, ArtTile_SS_Emerald_Sparkle	; $46 - emerald (sparkle when collecting)
id_SS_Emerald_Ani2:	specialStageData	1, Map_SS_Glass,  Tile_Pal2, ArtTile_SS_Emerald_Sparkle	; $47 - ''
id_SS_Emerald_Ani3:	specialStageData	2, Map_SS_Glass,  Tile_Pal2, ArtTile_SS_Emerald_Sparkle	; $48 - ''
id_SS_Emerald_Ani4:	specialStageData	3, Map_SS_Glass,  Tile_Pal2, ArtTile_SS_Emerald_Sparkle	; $49 - ''
id_SS_InvGhostTrigger:	specialStageData	2, Map_SS_Shared, Tile_Pal1, ArtTile_SS_Ghost_Block	; $4A - invisible ghost block trigger

; Solid misc blocks
id_SS_Glass_Ani1:	specialStageData	0, Map_SS_Glass,  Tile_Pal1, ArtTile_SS_Glass		; $4B - glass block (blinking while touched)
id_SS_Glass_Ani2:	specialStageData	0, Map_SS_Glass,  Tile_Pal4, ArtTile_SS_Glass		; $4C - ''
id_SS_Glass_Ani3:	specialStageData	0, Map_SS_Glass,  Tile_Pal2, ArtTile_SS_Glass		; $4D - ''
id_SS_Glass_Ani4:	specialStageData	0, Map_SS_Glass,  Tile_Pal3, ArtTile_SS_Glass		; $4E - ''

SS_MapIndex_End:
