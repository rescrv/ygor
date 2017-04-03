.section .data

.align 16
sse41_A:
    .long 0
    .long 0
    .long 0
    .long 0

.align 16
sse41_B:
    .long 0
    .long 0
    .long 0
    .long 0

.align 16
sse41_C:
    .long 0
    .long 0
    .long 0
    .long 0

.align 16
sse41_D:
    .long 1634760805
    .long 857760878
    .long 2036477234
    .long 1797285236

.section .text
.global guacamole_mash_sse41
.type   guacamole_mash_sse41, @function
guacamole_mash_sse41:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $0x60, %rsp
    andq    $-0x40, %rsp
    # number & 0xffffffff at 64 off the stack
    # number >> 32 at 68 off the stack
    movl    %edi, 0x40(%rsp)
    movq    %rdi, %rax
    shrq    $0x20, %rax
    movl    %eax, 0x44(%rsp)
    # initialize registers
    movdqa  sse41_B(%rip), %xmm1
    movdqa  sse41_C(%rip), %xmm2
    pinsrd  $2, 0x40(%rsp), %xmm2
    pinsrd  $3, 0x44(%rsp), %xmm1
    movdqa  sse41_A(%rip), %xmm0
    movdqa  sse41_D(%rip), %xmm3
    movdqa  %xmm1, 0x10(%rsp)
    movdqa  %xmm2, 0x20(%rsp)

    # loop
    movq $4, %rax

.align 64
sse41_loop_top:
    # A ^= rotate(B + C, 7)
    movdqa  %xmm1, %xmm4
    paddd   %xmm2, %xmm4
    movdqa  %xmm4, %xmm5
    pslld   $7, %xmm4
    psrld   $0x19, %xmm5
    pxor    %xmm4, %xmm0
    pxor    %xmm5, %xmm0

    # B ^= rotate(C + D, 9)
    movdqa  %xmm2, %xmm6
    paddd   %xmm3, %xmm6
    movdqa  %xmm6, %xmm7
    pslld   $9, %xmm6
    psrld   $0x17, %xmm7
    pxor    %xmm6, %xmm1
    pxor    %xmm7, %xmm1

    # C ^= rotate(D + A, 13)
    movdqa  %xmm3, %xmm4
    paddd   %xmm0, %xmm4
    movdqa  %xmm4, %xmm5
    pslld   $0xd, %xmm4
    psrld   $0x13, %xmm5
    pxor    %xmm4, %xmm2
    pxor    %xmm5, %xmm2

    # D ^= rotate(A + B, 18)
    movdqa  %xmm0, %xmm6
    paddd   %xmm1, %xmm6
    movdqa  %xmm6, %xmm7
    pslld   $0x12, %xmm6
    psrld   $0xe, %xmm7
    pxor    %xmm6, %xmm3
    pxor    %xmm7, %xmm3

    # swap A to G
    pshufd $147, %xmm0, %xmm0
    # swap B to F
    pshufd $78, %xmm1, %xmm1
    # swap C to E
    pshufd $57, %xmm2, %xmm2

    # E ^= rotate(F + G, 7);
    movdqa  %xmm0, %xmm4
    paddd   %xmm1, %xmm4
    movdqa  %xmm4, %xmm5
    pslld   $7, %xmm4
    psrld   $0x19, %xmm5
    pxor    %xmm4, %xmm2
    pxor    %xmm5, %xmm2

    # F ^= rotate(G + D, 9)
    movdqa  %xmm3, %xmm6
    paddd   %xmm0, %xmm6
    movdqa  %xmm6, %xmm7
    pslld   $9, %xmm6
    psrld   $0x17, %xmm7
    pxor    %xmm6, %xmm1
    pxor    %xmm7, %xmm1

    # G ^= rotate(D + E, 13)
    movdqa  %xmm2, %xmm4
    paddd   %xmm3, %xmm4
    movdqa  %xmm4, %xmm5
    pslld   $0xd, %xmm4
    psrld   $0x13, %xmm5
    pxor    %xmm4, %xmm0
    pxor    %xmm5, %xmm0

    # D ^= rotate(E + F, 18)
    movdqa  %xmm1, %xmm6
    paddd   %xmm2, %xmm6
    movdqa  %xmm6, %xmm7
    pslld   $0x12, %xmm6
    psrld   $0xe, %xmm7
    pxor    %xmm6, %xmm3
    pxor    %xmm7, %xmm3

    # swap G to A
    pshufd $57, %xmm0, %xmm0
    # swap F to B
    pshufd $78, %xmm1, %xmm1
    # swap E to C
    pshufd $147, %xmm2, %xmm2

    dec %rax
    jnz sse41_loop_top

    paddd   0x10(%rsp), %xmm1
    paddd   0x20(%rsp), %xmm2
    paddd   sse41_D(%rip), %xmm3

    movaps  %xmm0, (%rsi)
    movaps  %xmm1, 0x10(%rsi)
    movaps  %xmm2, 0x20(%rsi)
    movaps  %xmm3, 0x30(%rsi)

    leave
    ret
