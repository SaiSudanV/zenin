ENTRY(_start)

PHDRS
{
    kernel_rx PT_LOAD FLAGS(5); /* Read + execute */
    kernel_rw PT_LOAD FLAGS(6); /* Read + write */
}

SECTIONS
{
    /* QEMU virt ARM64 RAM starts at 0x40000000 */
    . = 0x40080000;
    __kernel_start = .;

    .text : {
        KEEP(*(.text.boot))
        *(.text .text.*)
        KEEP(*(.vectors))
    } :kernel_rx
    . = ALIGN(4096);
    __text_end = .;

    .rodata : {
        *(.rodata .rodata.*)
    } :kernel_rx
    . = ALIGN(4096);
    __rodata_end = .;

    .data : {
        *(.data .data.*)
    } :kernel_rw
    . = ALIGN(4096);
    __data_end = .;

    /* Image metadata footer: read by the boot stub to apply the
     * boot-environment memory-clear contract (D012). */
    .imgmeta : {
        QUAD(0x5A454E494D455441); /* "ZENIMETA" */
        QUAD(__kernel_end - __kernel_start);
        QUAD(__kernel_start);
    } :kernel_rw
    . = ALIGN(4096);

    __bss_start = .;
    .bss : {
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(8);
    } :kernel_rw
    __bss_end = .;

    . = ALIGN(4096);
    __stack_bottom = .;
    . = . + 0x10000; /* 64 KB Initial Kernel Stack */
    __stack_top = .;

    __kernel_end = .;
}
