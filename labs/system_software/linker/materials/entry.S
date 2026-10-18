.syntax unified
.cpu cortex-a9
.arm
.section .text.entry, "ax", %progbits
.global _start
.type _start, %function
_start:
    ldr sp, =__stack_top /* 在调用 C 代码前建立教学栈。 */
    bl runtime_start
1:  b 1b
.size _start, . - _start
