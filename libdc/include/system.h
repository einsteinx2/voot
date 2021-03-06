#ifndef __SYSTEM_H__
#define __SYSTEM_H__

#include "vars.h"

#define REG_EXPEVT      (REGISTER(uint32) (0xFF000024))
#define REG_INTEVT      (REGISTER(uint32) (0xFF000028))

typedef struct
{
    uint32 fpscr;
    uint32 fpul;
    uint32 pr;
    uint32 mach;
    uint32 macl;

    uint32 vbr;
    uint32 gbr;
    uint32 sr;
    uint32 dbr;

    uint32 r7_bank;
    uint32 r6_bank;
    uint32 r5_bank;
    uint32 r4_bank;
    uint32 r3_bank;
    uint32 r2_bank;
    uint32 r1_bank;
    uint32 r0_bank;

    uint32 r14;
    uint32 r13;
    uint32 r12;
    uint32 r11;
    uint32 r10;
    uint32 r9;
    uint32 r8;
    uint32 r7;
    uint32 r6;
    uint32 r5;
    uint32 r4;
    uint32 r3;
    uint32 r2;
    uint32 r1;

    uint32 exception_type;
    uint32 r0;
} register_stack;

extern uint32 dbr(void);
extern void dbr_set(void *set);

extern uint32 vbr(void);
extern void vbr_set(void *set);

extern uint32 r15(void);
extern uint32 spc(void);

extern void disable_cache();
extern void flush_cache();

#endif
