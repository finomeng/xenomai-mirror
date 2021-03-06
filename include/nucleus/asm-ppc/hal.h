/**
 *   @ingroup hal
 *   @file
 *
 *   Real-Time Hardware Abstraction Layer for PowerPC.
 *
 *   Copyright &copy; 2002-2004 Philippe Gerum.
 *
 *   Xenomai is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License as
 *   published by the Free Software Foundation, Inc., 675 Mass Ave,
 *   Cambridge MA 02139, USA; either version 2 of the License, or (at
 *   your option) any later version.
 *
 *   Xenomai is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Xenomai; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *   02111-1307, USA.
 */

#ifndef _XENO_ASM_PPC_HAL_H
#define _XENO_ASM_PPC_HAL_H

#include <nucleus/asm-generic/hal.h>	/* Read the generic bits. */
#include <asm/div64.h>

#ifndef CONFIG_ADEOS_CORE
#if IPIPE_RELEASE_NUMBER < 0x010100
#define rthal_virtualize_irq(dom,irq,isr,ackfn,mode) \
    ipipe_virtualize_irq(dom,irq,isr,ackfn,mode)
#else
#define rthal_virtualize_irq(dom,irq,isr,ackfn,mode) \
    ipipe_virtualize_irq(dom,irq,(ipipe_irq_handler_t)isr,NULL,(ipipe_irq_ackfn_t)ackfn,mode)
#endif
#endif

typedef unsigned long long rthal_time_t;

#define __rthal_u64tou32(ull, h, l) ({                  \
        union { unsigned long long _ull;                \
            struct { u_long _h; u_long _l; } _s; } _u;  \
        _u._ull = (ull);                                \
        (h) = _u._s._h;                                 \
        (l) = _u._s._l;                                 \
        })

#define __rthal_u64fromu32(h, l) ({                     \
        union { unsigned long long _ull;                \
            struct { u_long _h; u_long _l; } _s; } _u;  \
        _u._s._h = (h);                                 \
        _u._s._l = (l);                                 \
        _u._ull;                                        \
        })

static inline unsigned long long rthal_ullmul(const unsigned long m0, 
					      const unsigned long m1)
{
    return (unsigned long long) m0 * m1;
}

static inline unsigned long long rthal_ulldiv (unsigned long long ull,
					       const unsigned long uld,
					       unsigned long *const rp)
{
#if defined(__KERNEL__) && BITS_PER_LONG == 32
    const unsigned long r = __div64_32(&ull, uld);
#else /* !__KERNEL__ || BITS_PER_LONG == 64 */
    const unsigned long r = ull % uld;
    ull /= uld;
#endif /* __KERNEL__ */

    if (rp)
	*rp = r;

    return ull;
}

#define rthal_uldivrem(ull,ul,rp) ((u_long) rthal_ulldiv((ull),(ul),(rp)))

static inline int rthal_imuldiv (int i, int mult, int div) {

    /* Returns (int)i = (unsigned long long)i*(u_long)(mult)/(u_long)div. */
    const unsigned long long ull = rthal_ullmul(i, mult);
    return rthal_uldivrem(ull, div, NULL);
}

static inline __attribute_const__
unsigned long long __rthal_ullimd (const unsigned long long op,
                                   const unsigned long m,
                                   const unsigned long d)
{
    u_long oph, opl, tlh, tll, qh, rh, ql;
    unsigned long long th, tl;

    __rthal_u64tou32(op, oph, opl);
    tl = rthal_ullmul(opl, m);
    __rthal_u64tou32(tl, tlh, tll);
    th = rthal_ullmul(oph, m);
    th += tlh;

    qh = rthal_uldivrem(th, d, &rh);
    th = __rthal_u64fromu32(rh, tll);
    ql = rthal_uldivrem(th, d, NULL);
    return __rthal_u64fromu32(qh, ql);
}

static inline long long rthal_llimd (long long op,
                                     unsigned long m,
                                     unsigned long d)
{

    if(op < 0LL)
        return -__rthal_ullimd(-op, m, d);
    return __rthal_ullimd(op, m, d);
}

static inline __attribute_const__ unsigned long ffnz (unsigned long ul) {

    __asm__ ("cntlzw %0, %1" : "=r" (ul) : "r" (ul & (-ul)));
    return 31 - ul;
}

#if defined(__KERNEL__) && !defined(__cplusplus)
#include <asm/system.h>
#include <asm/time.h>
#include <asm/timex.h>
#include <nucleus/asm/atomic.h>
#include <asm/processor.h>

#ifdef CONFIG_ADEOS_CORE
#define RTHAL_TIMER_IRQ   ADEOS_TIMER_VIRQ
#else /* !CONFIG_ADEOS_CORE */
#define RTHAL_TIMER_IRQ   IPIPE_TIMER_VIRQ
#endif /* CONFIG_ADEOS_CORE */

#define rthal_irq_descp(irq)  (irq_desc + irq)

static inline unsigned long long rthal_rdtsc (void) {
    unsigned long long t;
    rthal_read_tsc(t);
    return t;
}

#if defined(CONFIG_ADEOS_CORE) && !defined(CONFIG_ADEOS_NOTHREADS)

/* Since real-time interrupt handlers are called on behalf of the
   Xenomai domain stack, we cannot infere the "current" Linux task
   address using %esp. We must use the suspended Linux domain's stack
   pointer instead. */

static inline struct task_struct *rthal_root_host_task (int cpuid) {
    return ((struct thread_info *)(rthal_root_domain->esp[cpuid] & (~8191UL)))->task;
}

static inline struct task_struct *rthal_current_host_task (int cpuid)

{
    register unsigned long esp asm ("r1");
    
    if (esp >= rthal_domain.estackbase[cpuid] && esp < rthal_domain.estackbase[cpuid] + 8192)
	return rthal_root_host_task(cpuid);

    return current;
}

#else /* !CONFIG_ADEOS_CORE || CONFIG_ADEOS_NOTHREADS */

static inline struct task_struct *rthal_root_host_task (int cpuid) {
    return current;
}

static inline struct task_struct *rthal_current_host_task (int cpuid) {
    return current;
}

#endif /* CONFIG_ADEOS_CORE && !CONFIG_ADEOS_NOTHREADS */

static inline void rthal_timer_program_shot (unsigned long delay)
{
    if(!delay) delay = 1;
#ifdef CONFIG_40x
    mtspr(SPRN_PIT,delay);
#else /* !CONFIG_40x */
    set_dec(delay);
#endif /* CONFIG_40x */
}

    /* Private interface -- Internal use only */

#define RTHAL_SWITCH_FRAME_SIZE  (STACK_FRAME_OVERHEAD + sizeof(struct pt_regs))

asmlinkage void rthal_thread_switch(struct thread_struct *prev,
				    struct thread_struct *next);

asmlinkage void rthal_thread_trampoline(void);

#ifdef CONFIG_XENO_HW_FPU

typedef struct rthal_fpenv {
    
    /* This layout must follow exactely the definition of the FPU
       backup area in a PPC thread struct available from
       <asm-ppc/processor.h>. Specifically, fpr[] an fpscr words must
       be contiguous in memory (see arch/ppc/hal/fpu.S). */

    double fpr[32];
    unsigned long fpscr_pad;	/* <= Hi-word of the FPR used to */
    unsigned long fpscr;	/* retrieve the FPSCR. */

} rthal_fpenv_t;

void rthal_init_fpu(rthal_fpenv_t *fpuenv);

void rthal_save_fpu(rthal_fpenv_t *fpuenv);

void rthal_restore_fpu(rthal_fpenv_t *fpuenv);

#ifndef CONFIG_SMP
#define rthal_get_fpu_owner(cur) last_task_used_math
#else /* CONFIG_SMP */
#define rthal_get_fpu_owner(cur) ({                             \
    struct task_struct * _cur = (cur);                          \
    ((_cur->thread.regs && (_cur->thread.regs->msr & MSR_FP))   \
     ? _cur : NULL);                                            \
})
#endif /* CONFIG_SMP */
    
#define rthal_disable_fpu() ({                          \
    register long _msr;                                 \
    __asm__ __volatile__ ( "mfmsr %0" : "=r"(_msr) );   \
    __asm__ __volatile__ ( "mtmsr %0"                   \
                           : /* no output */            \
                           : "r"(_msr & ~(MSR_FP))      \
                           : "memory" );                \
})

#define rthal_enable_fpu() ({                           \
    register long _msr;                                 \
    __asm__ __volatile__ ( "mfmsr %0" : "=r"(_msr) );   \
    __asm__ __volatile__ ( "mtmsr %0"                   \
                           : /* no output */            \
                           : "r"(_msr | MSR_FP)         \
                           : "memory" );                \
})

#endif /* CONFIG_XENO_HW_FPU */

static const char *const rthal_fault_labels[] = {
    [0] = "Data or instruction access",
    [1] = "Alignment",
    [2] = "Altivec unavailable",
    [3] = "Program check exception",
    [4] = "Machine check exception",
    [5] = "Unknown",
    [6] = "Instruction breakpoint",
    [7] = "Run mode exception",
    [8] = "Single-step exception",
    [9] = "Non-recoverable exception",
    [10] = "Software emulation",
    [11] = "Debug",
    [12] = "SPE",
    [13] = "Altivec assist",
    [14] = NULL
};

#endif /* __KERNEL__ && !__cplusplus */

#endif /* !_XENO_ASM_PPC_HAL_H */
