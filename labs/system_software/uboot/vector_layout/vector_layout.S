/* 教学布局：只验证向量槽位，不是可安装的异常处理程序。 */
.section .vectors, "ax"
.balign 2048
.global vector_table
vector_table:
.macro vector_slot name, offset
    .org vector_table + \offset
    .global \name
\name:
    b halted
.endm

vector_slot current_sp0_sync,   0x000
vector_slot current_sp0_irq,    0x080
vector_slot current_sp0_fiq,    0x100
vector_slot current_sp0_serror, 0x180
vector_slot current_spx_sync,   0x200
vector_slot current_spx_irq,    0x280
vector_slot current_spx_fiq,    0x300
vector_slot current_spx_serror, 0x380
vector_slot lower_a64_sync,     0x400
vector_slot lower_a64_irq,      0x480
vector_slot lower_a64_fiq,      0x500
vector_slot lower_a64_serror,   0x580
vector_slot lower_a32_sync,     0x600
vector_slot lower_a32_irq,      0x680
vector_slot lower_a32_fiq,      0x700
vector_slot lower_a32_serror,   0x780
.org vector_table + 0x800
.global vector_table_end
vector_table_end:

.text
halted:
    /* 无栈、无寄存器保存；实验不执行此处。 */
    b halted
