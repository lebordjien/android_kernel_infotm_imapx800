#ifndef __ASMARM_SMP_GTIMER_H
#define __ASMARM_SMP_GTIMER_H

#define G_TIMER_COUNTER_L	0x00
#define G_TIMER_COUNTER_H	0x04
#define G_TIMER_CONTROL		0x08
#define G_TIMER_INTSTAT		0x0c
#define G_TIMER_COMPARE_L	0x10
#define G_TIMER_COMPARE_H	0x14
#define G_TIMER_AUTOINC		0x18

#endif

extern int ca5_gtimer_init(void __iomem *reg, const char *name);

