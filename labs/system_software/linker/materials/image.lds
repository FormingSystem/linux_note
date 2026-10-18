OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY
{
    FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 64K
    RAM (rw)   : ORIGIN = 0x20000000, LENGTH = 16K
}

SECTIONS
{
    .text : {
        KEEP(*(.text.entry))
        *(.text .text.*)
    } > FLASH
    .rodata : { *(.rodata .rodata.*) } > FLASH
    .registry : {
        . = ALIGN(4);
        __registry_start = .;
        KEEP(*(SORT_BY_NAME(.registry.*)))
        __registry_end = .;
    } > FLASH

    .data : {
        . = ALIGN(4);
        __data_start = .;
        *(.data .data.*)
        __data_end = .;
    } > RAM AT> FLASH
    __data_load = LOADADDR(.data);

    .bss (NOLOAD) : {
        . = ALIGN(4);
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > RAM
    .stack (NOLOAD) : {
        . = ALIGN(8);
        __stack_bottom = .;
        . += 0x400;
        __stack_top = .;
    } > RAM
    /DISCARD/ : { *(.comment) *(.note.GNU-stack) }
}
ASSERT(__stack_top <= ORIGIN(RAM) + LENGTH(RAM), "RAM layout overflow")
