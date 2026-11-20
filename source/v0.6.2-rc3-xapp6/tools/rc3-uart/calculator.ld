OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(xapp_entry)
SECTIONS
{
    . = 0;
    .xapp_header : { KEEP(*(.xapp_header)) }
    . = ALIGN(4);
    .text : { *(.text*) *(.rodata*) . = ALIGN(4); }
    . = ALIGN(4);
    .data : { *(.data*) }
    xapp_image_end = .;
    .unexpected : { *(.bss*) *(COMMON) *(.got*) }
    ASSERT(SIZEOF(.unexpected) == 0, "XAPP requires relocations or zero-fill state")
    /DISCARD/ : { *(.comment*) *(.note*) *(.ARM.attributes*) *(.ARM.exidx*) *(.ARM.extab*) }
}
