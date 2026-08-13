.global Reset_Handler
.extern kernel_main
.extern _vector_table

.equ MODE_IRQ, 0x12
.equ MODE_SYS, 0x1F
.equ I_BIT,    0x80
.equ F_BIT,    0x40

.section .text.startup
Reset_Handler:
    
    mov r0, #0x00000000
    ldr r1, =_vector_table
    ldmia r1!, {r2-r9}
    stmia r0!, {r2-r9}

    
    msr cpsr_c, #(MODE_IRQ | I_BIT | F_BIT)
    ldr sp, =irq_stack_top

   
    msr cpsr_c, #MODE_SYS
    ldr sp, =stack_top

    
    bl kernel_main

hang:
    b hang

.section .bss
.align 3
irq_stack:
    .space 1024
irq_stack_top: