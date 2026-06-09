ENTRY(_start)

SECTIONS
{
    . = 1M;
    
    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
    }
    
    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
    }
    
    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
    }
    
    /* Встраиваем terminal.mps как данные */
    .terminal_data BLOCK(4K) : ALIGN(4K)
    {
        terminal_code_start = .;
        *(.terminal_code)
        terminal_code_end = .;
    }
    
    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }
}
