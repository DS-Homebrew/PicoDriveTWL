.extern Pico
.extern PicoCpu
.extern PicoPad
.extern PicoOpt
.extern SRam

@-----------------------------------------------------------------------------------------------------
@	ALL FUNCTIONS RELATED TO MEMORY.C
@-----------------------------------------------------------------------------------------------------
.global PicoCheckPc				@ pc (r0)
PicoCheckPc:
	ldr		r3, =(PicoCpu+0x60)	@r3 = &PicoCpu.membase
	ldr		r1, [r3]			@r1 = PicoCpu.membase (value)
	sub		r0, r0, r1
	bic     r0, r0, #-16777216			
	and     r1, r0, #14680064
    cmp     r1, #14680064
	bne		.endif1pcp
	ldr		r2, =(Pico)			@pico.ram
	and		r1, r0, #16711680
	sub		r2, r2, r1
	str		r2, [r3]			@stored PicoCpu.membase
	add		r0, r0, r2			@return value = PicoCpu.membase + pc
    bx      lr
.endif1pcp:
	ldr		r2, =(Pico+0x22200)	@pico.rom
	ldr		r2, [r2]			
	str		r2, [r3]			@stored PicoCpu.membase
	add		r0, r0, r2			@return value = PicoCpu.membase + pc
    bx      lr
	
@---------------------------------------------------------------------------
.global PadRead			@int i (r0)
PadRead:
	stmfd   sp!, {r4,lr}
	mov		r4, r0						@backup of r0
	ldr		r1, =PicoPad
	ldr     r1, [r1, r0, lsl #2]
    mvn     r1, r1						@r1 = pad
	ldr		r2, =(Pico+0x22000)			@pico.ioports
	add		r3, r0, #1 
	ldrb	r2, [r2, r3]
	and		r2, r2, #0x40				@r2 = TH
	ldr		r3, =PicoOpt
	ldr		r3, [r3]
	ands	r3, r3, #0x20
	beq		.endif1pr
	ldr		r3, =(Pico+0x2220A)			@pico.m.padTHPhase
	ldr		r3, [r3, r0]				@r3 = phase    [r0 is free]
	cmp		r3, #2 
	bne		.else1pr
	cmp		r2, #0 
	bne		.else1pr
	and		r0, r1, #0xC0 				@ [r3 is free]
	asr		r0, r0, #2 
	b		.endpr
.else1pr:
	cmp		r3, #3 
	bne		.endif1pr
	cmp		r2, #0 
	beq		.else2pr
	and		r0, r1, #0x30 				@ [r3 is free]
	asr		r3, r1, #8 
	and		r3, r3, #0xF 
	orr		r0, r0, r3
	b		.endpr
.else2pr:
	and		r0, r1, #0xC0 
	asr		r0, r0, #2 
	orr		r0, r0, #0x0F
	b 		.endpr
.endif1pr:
	cmp		r2, #0
	beq  	.else3pr
	and		r0, r1, #0x3F
	b		.endpr
.else3pr:
	and		r0, r1, #0xC0 
	asr		r0, r0, #2
	and		r3, r1, #3
	orr		r0, r0, r3
.endpr:									@ [r2 is free]
	ldr		r3, =(Pico+0x22000)			@pico.ioports
	add		r2, r3, r4
	add		r3, r3, r4
	ldrb 	r3, [r3, #1]				@pico.ioports[i+1]
	ldrb	r2, [r2, #4] 				@pico.ioports[i+4]
	and		r2, r3, r2 
	orr		r0, r0, r2
	ldmfd   sp!, {r4,pc}

@---------------------------------------------------------------------------
.global OtherRead16						@ unsigned int a (r0)
OtherRead16:
	stmfd   sp!, {r1-r4,lr}
	ldr		r3, =0xffc000
	and		r3, r3, r0
	cmp		r3, #0xa00000
	bne		.endif1or16
	lsl		r0, r0, #19
	lsr		r0, r0, #19
	ldr		r3, =PicoOpt
	ldr		r3, [r3]
	ands	r3, r3, #4
	bne		.endif2or16
	ldr		r3, =(Pico+0x22214)			@r3 = &pico.m.z80_lastaddr
	ldrh	r2, [r3]					@r2 = pico.m.z80_lastaddr
	cmp		r0, r2
	bne		.endif3or16
	ldr		r1, =(Pico+0x22216)			@r1 = &Pico.m.z80_fakeval
	ldrh	r0, [r1]					@r0 = Pico.m.z80_fakeval
	add		r0, r0, #1
	strh	r0, [r1]
	sub		r0, r0, #1
	b		.endor16
.endif3or16:
	ldr		r1, =(Pico+0x22216)			@r1 = &Pico.m.z80_fakeval
	mov		r4, #0
	strh	r4, [r1]
	strh 	r0, [r3]
.endif2or16:
	ldr		r1, =(Pico+0x20000)			@r1 = Pico.zram
	add		r2, r1, r0
	ldrb	r3, [r2]
	add		r2, r2, #1
	ldrb	r2, [r2]
	orr		r0, r2, r3, lsl #8			@r0 = (Pico.zram[a]<<8)|Pico.zram[a+1]
	b		.endor16
.endif1or16:
	bic     r3, r0, #-16777216
    bic     r3, r3, #3
	mov		r1, #0xA0000
	orr		r1, r1, #0x400
	cmp		r3, r1
	bne 	.endif4or16
	ldr		r1, =(Pico+0x22208)
	ldrb	r2, [r1]
	and		r0, r2, #3 					@r0 = Pico.m.rotate&3
	add		r2, r2, #1
	strb 	r2, [r1]
	b 		.endor16
.endif4or16:
	bic     r1, r0, #-16777216
    bic     r1, r1, #31
	cmp		r1, #0xA10000
	bne		.endif5or16
	lsr     r0, r0, #1
    and     r0, r0, #0xF
	cmp		r0, #0
	beq		.case0or16
	cmp 	r0, #1
	beq		.case1or16
	cmp		r0, #2
	beq		.case2or16
	ldr		r2, =(Pico+0x22000)
	add		r0, r2, r0
	ldrb	r0, [r0]					@r0 = Pico.ioports[aa]
	orr		r0, r0, lsr #8
	b 		.endor16
.case0or16:
	ldr		r2, =(Pico+0x2220F)			
	ldrb	r0, [r2]					@r0 = Pico.m.hardware
	orr		r0, r0, lsr #8
	b		.endor16
.case1or16:
	mov		r0, #0
	bl		PadRead
	ldr		r1, =(Pico+0x22000)
	ldrb	r1, [r1, #1]
	and		r1, r1, #0x80
	orr		r0, r0, r1 					@r0 = PadRead(0)|Pico.ioports[1]&0x80
	orr		r0, r0, lsr #8
	b		.endor16
.case2or16:
	mov		r0, #1
	bl		PadRead
	ldr		r1, =(Pico+0x22000)
	ldrb	r1, [r1, #2]
	and		r1, r1, #0x80
	orr		r0, r0, r1 					@r0 = PadRead(1)|Pico.ioports[2]&0x80
	orr		r0, r0, lsr #8
	b		.endor16
.endif5or16:
	ldr		r2, =0xA11100				@Needs to be improved
	cmp		r0, r2
	bne		.endif6or16
	ldr		r1, =(Pico+0x22209)
	ldrb	r0, [r1]
	lsl		r0, r0, #8					@r0 = Pico.m.z80Run << 8
	b		.endor16
.endif6or16:
	bic     r1, r0, #-16777216
    bic     r1, r1, #31
	cmp 	r1, #0xC00000
	bne		.endif7or16
	bl		PicoVideoRead				@r0 = PicoVideoRead(a)
	orr		r0, r0, lsr #8
	b		.endor16
.endif7or16:
	mov		r0, #0						@default return value (r0 = 0)
.endor16:
	ldmfd   sp!, {r1-r4,pc}

@---------------------------------------------------------------------------
.global PicoRead8			@unsigned int a (r0)
PicoRead8:
	stmfd   sp!, {r4,lr}
	bic     r0, r0, #-16777216
	and		r1, r0, #0xE00000
	cmp		r1, #0xE00000
	bne		.endif1pr8
	eor		r1, r0, #1
	lsl		r1, r1, #16
	lsr		r1, r1, #16
	ldr		r2, =Pico
	add		r0, r2, r1					@r0 = Pico.ram+((a^1)&0xffff)
	ldrb	r0, [r0]
	b 		.endpr8
.endif1pr8:
	ldr 	r2, =(SRam)
	ldr		r1, [r2, #0x4]				@r1 = Sram.start
	ldr 	r3, [r2, #0x8]				@r3 = Sram.end
	cmp		r0, r1
	bcc		.endif2pr8
	cmp		r0, r3
	bhi		.endif2pr8
	ldr		r3, =(Pico+0x22211)
	ldrb 	r4, [r3]
	ands	r4, r4, #1 
	beq		.endif2pr8
	ldr		r2, [r2]					@ r2 = Sram.data
	sub		r2, r2, r1
	add		r0, r2, r0					@ r0 = SRam.data-SRam.start+a
	ldrb	r0, [r0]
	b		.endpr8
.endif2pr8:
	ldr		r2, =(Pico+0x22200)
	ldr		r1, [r2, #4]				@r1 = Pico.romsize
	cmp 	r0, r1 
	bcs		.endif3pr8
	eor		r0, r0, #1
	ldr		r2, [r2]					@r2 = Pico.rom
	add		r0, r2, r0					
	ldrb	r0, [r0]					@r0 = Pico.rom+(a^1)
	b 		.endpr8
.endif3pr8:
	mov		r4, r0						@backup of variable a
	bic     r0, r0, #1					@Now a is temporarly changed
	bl		OtherRead16	
	ands	r1, r4, #1
	lsreq 	r0, r0, #8
.endpr8:
	and		r0, r0, #0xFF
	ldmfd   sp!, {r4,pc}

@---------------------------------------------------------------------------
.global PicoRead16			@unsigned int a (r0)
PicoRead16:
	stmfd   sp!, {r4,lr}
	bic     r0, r0, #-16777216
    bic     r0, r0, #1
	and		r1, r0, #0xE00000
	cmp		r1, #0xE00000
	bne		.endif1pr16
	lsl		r1, r0, #16
	lsr		r1, r1, #16
	ldr		r2, =Pico
	add		r0, r2, r1					@r0 = Pico.ram+(a^&0xffff)
	ldrh	r0, [r0]
	b 		.endpr16
.endif1pr16:
	ldr 	r2, =(SRam)
	ldr		r1, [r2, #0x4]				@r1 = Sram.start
	ldr 	r3, [r2, #0x8]				@r3 = Sram.end
	cmp		r0, r1
	bcc		.endif2pr16
	cmp		r0, r3
	bhi		.endif2pr16
	ldr		r3, =(Pico+0x22211)
	ldrb 	r4, [r3]
	ands	r4, r4, #1 
	beq		.endif2pr16
	ldr		r2, [r2]					@ r2 = Sram.data
	sub		r2, r2, r1
	add		r0, r2, r0					@ r0 = SRam.data-SRam.start+a
	ldrh	r0, [r0]
	b		.endpr16
.endif2pr16:
	ldr		r2, =(Pico+0x22200)
	ldr		r1, [r2, #4]				@r1 = Pico.romsize
	cmp 	r0, r1 
	bcs		.endif3pr16
	ldr		r2, [r2]					@r2 = Pico.rom
	add		r0, r2, r0					
	ldrh	r0, [r0]					@r0 = Pico.rom+a
	b 		.endpr16
.endif3pr16:
	bl 		OtherRead16
.endpr16:
	lsl		r0, r0, #16
	lsr		r0, r0, #16
	ldmfd   sp!, {r4,pc}
	
@---------------------------------------------------------------------------
.global PicoRead32			@unsigned int a (r0)
PicoRead32:
	stmfd   sp!, {r4,lr}
	bic     r0, r0, #-16777216
    bic     r0, r0, #1
	and		r1, r0, #0xE00000
	cmp		r1, #0xE00000
	bne		.endif1pr32
	lsl		r1, r0, #16
	lsr		r1, r1, #16
	ldr		r2, =Pico
	add		r0, r2, r1					@r0 = pm = Pico.ram+(a^&0xffff)
	ldrh	r1, [r0]
	ldrh	r2, [r0, #2]
	orr		r0, r2, r1, lsl #16			@r0 = (pm[0]<<16)|pm[1];
	b 		.endpr32
.endif1pr32:
	ldr 	r2, =(SRam)
	ldr		r1, [r2, #0x4]				@r1 = Sram.start
	ldr 	r3, [r2, #0x8]				@r3 = Sram.end
	cmp		r0, r1
	bcc		.endif2pr32
	cmp		r0, r3
	bhi		.endif2pr32
	ldr		r3, =(Pico+0x22211)
	ldrb 	r4, [r3]
	ands	r4, r4, #1 
	beq		.endif2pr32
	ldr		r2, [r2]					@ r2 = Sram.data
	sub		r2, r2, r1
	add		r0, r2, r0					
	ldrh	r1, [r0]					@ SRam.data-SRam.start+a
	ldrh	r2, [r0, #2]				@ SRam.data-SRam.start+a+2
	orr		r0, r2, r1, lsl #16			@ r0 = *(u8 *)(SRam.data-SRam.start+a)<<16 | *(u8 *)(SRam.data-SRam.start+a+2)
	b		.endpr32
.endif2pr32:
	ldr		r2, =(Pico+0x22200)
	ldr		r1, [r2, #4]				@r1 = Pico.romsize
	cmp 	r0, r1 
	bcs		.endif3pr32
	ldr		r2, [r2]					@r2 = Pico.rom
	add		r0, r2, r0					
	ldrh	r1, [r0]					@Pico.rom+a
	ldrh	r2, [r0, #2]				@Pico.rom+a+2
	orr		r0, r2, r1, lsl #16			@ r0 = (pm[0]<<16)|pm[1]	
	b 		.endpr32
.endif3pr32:
	add		r4, r0, #2
	bl		OtherRead16
	mov		r1, r0
	mov		r0, r4
	bl		OtherRead16
	orr		r0, r0, r1, lsl #16			@ r0 = (OtherRead16(a)<<16) | OtherRead16(a+2)
.endpr32:
	ldmfd   sp!, {r4,pc}
