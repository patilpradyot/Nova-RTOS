.global _vector_table
.global IRQ_Handler
.global start_first_task
.global context_restore

.extern current_tcb
.extern schedule_next
.extern timer_irq_clear
.extern Reset_Handler

.equ MODE_IRQ, 0x12
.equ MODE_SYS, 0x1F
.equ I_BIT,    0x80

.section .vectors, "ax"
_vector_table:
    LDR pc, =Reset_Handler
    LDR pc, =Undefined_Handler
    LDR pc, =SWI_Handler
    LDR pc, =Prefetch_Handler
    LDR pc, =Abort_Handler
    NOP                       
    LDR pc, =IRQ_Handler
    LDR pc, =FIQ_Handler

Undefined_Handler:  B Undefined_Handler
SWI_Handler:        B SWI_Handler
Prefetch_Handler:   B Prefetch_Handler
Abort_Handler:      B Abort_Handler
FIQ_Handler:        B FIQ_Handler

.text

IRQ_Handler:
    SUB     lr, lr, #4            
    STMFD   sp!, {lr}             
    MRS     lr, spsr
    STMFD   sp!, {lr}             

    MSR     cpsr_c, #(MODE_SYS | I_BIT)
    STMFD   sp!, {r0-r12, lr}     

    MSR     cpsr_c, #(MODE_IRQ | I_BIT)
    LDMFD   sp!, {r0}             
    LDMFD   sp!, {r1}             

    MSR     cpsr_c, #(MODE_SYS | I_BIT)
    STMFD   sp!, {r0-r1}          

    LDR     r2, =current_tcb
    LDR     r3, [r2]
    STR     sp, [r3]              

    MSR     cpsr_c, #(MODE_IRQ | I_BIT)
    BL      timer_irq_clear
    BL      schedule_next         

context_restore:
    MSR     cpsr_c, #(MODE_SYS | I_BIT)
    LDR     r2, =current_tcb
    LDR     r3, [r2]
    LDR     sp, [r3]              

    LDMFD   sp!, {r0, r1}         

    MSR     cpsr_c, #(MODE_IRQ | I_BIT)
    MSR     spsr_cxsf, r0
    MOV     lr, r1

    MSR     cpsr_c, #(MODE_SYS | I_BIT)
    LDMFD   sp!, {r0-r12, lr}     

    MSR     cpsr_c, #(MODE_IRQ | I_BIT)
    MOVS    pc, lr                

start_first_task:
    B       context_restore