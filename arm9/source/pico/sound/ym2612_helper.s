@ some attempts to improve speed of YM2612UpdateOne() function from ym2612.c
@ by using multiple register loads

@ (c) Copyright 2006, notaz
@ All Rights Reserved


@ sum_outputs* assume r11 points to output buffer

.global sum_outputs_mono @ UINT32 *out_fm
sum_outputs_mono:
    stmfd   sp!, {lr}
    ldmia   r0, {r0-r3,r12,lr} @ checked the ARM manual, this should be safe
    add     r0, r0, r1
    b       .sum_outputs_mono_common



.global sum_outputs_mono_mix @ UINT32 *out_fm
sum_outputs_mono_mix:
    stmfd   sp!, {lr}
    ldmia   r0, {r0-r3,r12,lr}
    add     r0, r0, r1

    ldrsh   r1, [r11]
    add     r0, r0, r1

.sum_outputs_mono_common:
    add     r0, r0, r2
    add     r0, r0, r3
    add     r0, r0, r12
    add     r0, r0, lr

    @ Limit
    mov     r1, #0x8000
    sub     r1, r1, #1
    cmp     r0, r1
    movgt   r0, r1
    bgt     .done1

    cmn     r0, #0x8000
    movlt   r0, #0x8000

.done1:
    strh    r0, [r11], #2

    ldmfd   sp!, {lr}
    bx      lr



.global sum_outputs_stereo @ UINT32 *out_fm, UINT32 pan
sum_outputs_stereo:
    stmfd   sp!, {r4-r6,lr}
    mov     r5, #0      @ lt
    mov     r6, #0      @ rt
    b       .sum_outputs_stereo_common



.global sum_outputs_stereo_mix @ UINT32 *out_fm, UINT32 pan
sum_outputs_stereo_mix:
    stmfd   sp!, {r4-r6,lr}
    ldrsh   r5, [r11]   @ lt
    mov     r6, r5      @ rt

.sum_outputs_stereo_common:
    ldmia   r0, {r0,r2-r4,r12,lr}
    movs    r1, r1, lsr #1
    addcs   r6, r6, r0
    movs    r1, r1, lsr #1
    addcs   r5, r5, r0
    movs    r1, r1, lsr #1
    addcs   r6, r6, r2
    movs    r1, r1, lsr #1
    addcs   r5, r5, r2
    movs    r1, r1, lsr #1
    addcs   r6, r6, r3
    movs    r1, r1, lsr #1
    addcs   r5, r5, r3
    movs    r1, r1, lsr #1
    addcs   r6, r6, r4
    movs    r1, r1, lsr #1
    addcs   r5, r5, r4
    movs    r1, r1, lsr #1
    addcs   r6, r6, r12
    movs    r1, r1, lsr #1
    addcs   r5, r5, r12
    movs    r1, r1, lsr #1
    addcs   r6, r6, lr
    movs    r1, r1, lsr #1
    addcs   r5, r5, lr

    @ Limit
    mov     r1, #0x8000
    sub     r1, r1, #1
    cmp     r5, r1
    movgt   r5, r1
    cmp     r6, r1
    movgt   r6, r1

    cmn     r5, #0x8000
    movlt   r5, #0x8000
    cmn     r6, #0x8000
    movlt   r6, #0x8000

    mov     r5, r5, lsl #16
    mov     r5, r5, lsr #16
    orr     r5, r5, r6, lsl #16
    stmia   r11!, {r5}

    ldmfd   sp!, {r4-r6,lr}
    bx      lr


