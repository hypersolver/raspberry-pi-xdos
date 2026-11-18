.syntax unified
.cpu cortex-a53
.arm
.section .text.boot, "ax", %progbits
.global _start
.type _start, %function
_start:
    cpsid if
    mrc p15, 0, r0, c0, c0, 5
    and r0, r0, #3
    cmp r0, #0
    bne .Lpark
    ldr sp, =0x00200000
    ldr r0, =__bss_start__
    ldr r1, =__bss_end__
    mov r2, #0
.Lclear:
    cmp r0, r1
    bhs .Lrun
    str r2, [r0], #4
    b .Lclear
.Lrun:
    bl kernel_main
.Lpark:
    wfe
    b .Lpark
