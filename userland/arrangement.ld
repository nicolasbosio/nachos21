OUTPUT_FORMAT("ecoff-littlemips")
ENTRY(__start)
SECTIONS
{
    .text 0 : ALIGN(128) {
        *(.init)
        *(.text)
        *(.fini)
    }
    .data . : ALIGN(128) {
        /* `coff2noff` cannot output more than one initialized data section,
           so put the contents of all of them inside `.data`. */
        *(.rdata)
        *(.rodata)
        *(.data)
        CONSTRUCTORS
    }
    .sbss . : {
        *(.sbss)
        *(.scommon)
    }
    .bss . : {
        *(.bss)
        *(COMMON)
    }
    /* Sections to be discarded, if present. */
    /DISCARD/ : {
        *(.comment)
        *(.pdr)
        *(.reginfo)
        *(.gnu.att*)
        *(.MIPS.ab*)
    }
}
