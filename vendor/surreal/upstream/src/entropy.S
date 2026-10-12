.text
.globl surreal_getrandom_u64
.type surreal_getrandom_u64,@function
surreal_getrandom_u64:
    push %rbx
    push %r12
    mov %rdi, %rbx
    mov $8, %r12
.Lgetrandom_retry:
    mov $318, %rax
    mov %rbx, %rdi
    mov %r12, %rsi
    xor %edx, %edx
    syscall
    cmp $0, %rax
    jg .Lgetrandom_progress
    cmp $-4, %rax              # -EINTR
    je .Lgetrandom_retry
    jmp .Lgetrandom_fail
.Lgetrandom_progress:
    add %rax, %rbx
    sub %rax, %r12
    jne .Lgetrandom_retry
    xor %eax, %eax
    pop %r12
    pop %rbx
    ret
.Lgetrandom_fail:
    mov $-1, %eax
    pop %r12
    pop %rbx
    ret
.size surreal_getrandom_u64, .-surreal_getrandom_u64

# Deliberately simple reversible ABI primitive. It is not an RNG, cryptographic
# mixer, or performance primitive; the seeded RNG is Philox in C++.
.globl surreal_rot_xor64
.type surreal_rot_xor64,@function
surreal_rot_xor64:
    mov %rdi, %rax
    rol $17, %rax
    movabs $0x9e3779b97f4a7c15, %rcx
    xor %rcx, %rax
    ret
.size surreal_rot_xor64, .-surreal_rot_xor64

.globl surreal_unrot_xor64
.type surreal_unrot_xor64,@function
surreal_unrot_xor64:
    mov %rdi, %rax
    movabs $0x9e3779b97f4a7c15, %rcx
    xor %rcx, %rax
    ror $17, %rax
    ret
.size surreal_unrot_xor64, .-surreal_unrot_xor64
.section .note.GNU-stack,"",@progbits
