ENTRY(_start)

SECTIONS
{
    /* QEMU virt ARM64 RAM starts at 0x40000000 */
    . = 0x40080000;

    .text : {
        KEEP(*(.text.boot))
        *(.text .text.*)
    }

    .rodata : {
        *(.rodata .rodata.*)
    }

    .data : {
        *(.data .data.*)
    }

    . = ALIGN(16);
    __bss_start = .;
    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    }
    __bss_end = .;

    . = ALIGN(4096);
    __stack_bottom = .;
    . = . + 0x10000; /* 64 KB Initial Kernel Stack */
    __stack_top = .;

    __kernel_end = .;
}
