ENTRY(Reset_Handler)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x00000000, LENGTH = 128M
}

SECTIONS
{
    . = 0x00000000;

    .vectors : {
        KEEP(*(.vectors))
    } > RAM

    .text : {
        *(.text.startup)
        *(.text)
        *(.text.*)
    } > RAM

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    } > RAM

    .data : {
        *(.data)
        *(.data.*)
    } > RAM

    .bss : {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    } > RAM

    stack_top = ORIGIN(RAM) + LENGTH(RAM) - 0x1000;
}