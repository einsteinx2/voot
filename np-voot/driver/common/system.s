!   system.s
!
!   $Id: system.s,v 1.7 2002/11/14 20:56:03 quad Exp $
!
! DESCRIPTION
!
!   Various processor level functions.
!

    .section .text

    .global _dbr
    .global _dbr_set
    .global _sgr
    .global _flush_cache
    .global _ubc_wait

_dbr:
    stc     DBR, r0
    rts
    nop

_dbr_set:
    ldc     r4, DBR
    rts
    nop

_sgr:
    stc     SGR, r0
    rts
    nop

_flush_cache:
    mov     #-96, r3
    mov     #-1, r2
    shll8   r3
    shlr2   r2
    shll16  r3
    shlr    r2
    mova    skip1, r0
    and     r2, r0
    or      r3, r0
    jmp     @r0
    nop

    .align  4

skip1:
    stc.l   sr, @-r15
    stc     sr, r0
    or      #240, r0
    ldc     r0, sr
    mov     #-12, r5
    shll16  r5
    mov     r5, r6
    add     #32, r6
    shll8   r5
    shll8   r6
    mov     #-1, r3
    shll8   r3
    shll16  r3
    add     #28, r3
    mov.l   @r3, r0
    tst     #32, r0
    bt/s    skip2
    mov     #2, r2
    mov     #1, r2
skip2:
    shll8   r2
    shll2   r2
    shll2   r2
    mov     #0, r1
loop1:
    add     #-32, r2
    mov     r2, r0
    shlr2   r0
    shll2   r0
    tst     r2, r2
    mov.l   r1, @(r0, r6)
    mov.l   r1, @(r0, r5)
    bf      loop1
    nop

    nop
    nop
    nop
    nop
    nop
    ldc.l   @r15+, sr
    rts
    nop

!
! Wait enough instructions for the UBC to be refreshed
!

_ubc_wait:
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    rts
    nop
