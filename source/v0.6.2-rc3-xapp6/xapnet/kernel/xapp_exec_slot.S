/* XD OS trusted XAPP execution slot for Pi 3 AArch32.
 * Circle lib/pagetable.cpp maps only kernel text (_etext) as executable.
 * Normal heap is NORMAL_XN. Do NOT execute copied instructions from heap.
 * This slot is reserved in a dedicated NOLOAD section and mapped at boot.
 * Privileged developer-only module; no memory isolation or signature check. */
.syntax unified
.arm
.section .xdos_xapp_slot, "awx", %nobits
.balign 64
.global xdos_xapp_exec_slot
.type xdos_xapp_exec_slot, %object
xdos_xapp_exec_slot:
    .space 327680, 0
.global xdos_xapp_exec_slot_end
xdos_xapp_exec_slot_end:
.size xdos_xapp_exec_slot, xdos_xapp_exec_slot_end - xdos_xapp_exec_slot

/* XAPP V2: three 320 KiB slots and one 768 KiB slot, linked at 0x400000 (circle.ld). */
.section .xdos_xapp_v2_slots, "awx", %nobits
.balign 64
.global xdos_xapp_v2_slots
.type xdos_xapp_v2_slots, %object
xdos_xapp_v2_slots:
    .space (3 * 320 + 768) * 1024, 0
.global xdos_xapp_v2_slots_end
xdos_xapp_v2_slots_end:
.size xdos_xapp_v2_slots, xdos_xapp_v2_slots_end - xdos_xapp_v2_slots
