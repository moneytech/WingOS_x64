#include <arch/lock.h>
#include <arch/mem/liballoc.h>
#include <arch/sse.h>
#include <device/local_data.h>
#include <loggging.h>
#pragma optimize("-O0")
extern "C" void sse_init(void);
extern "C" void avx_init(void);
uint64_t *fpu_reg;
uint64_t fpu_data[128] __attribute__((aligned(16)));
lock_type sse_lock = {0};
extern "C" void asm_sse_save(uint64_t addr);
extern "C" void asm_sse_load(uint64_t addr);
void init_sse()
{
    // uint64_t fpu_data = (uint64_t)malloc(128 * sizeof(uint64_t));

    fpu_reg = (uint64_t *)fpu_data;
    sse_init();
    asm volatile("fninit");

    //  asm volatile("fxsave (%0)" ::"r"((uint64_t)fpu_reg));
    asm_sse_save((uint64_t)fpu_reg);
}

__attribute__((optimize("O0"))) void save_sse_context(uint64_t *context)
{
    lock((&sse_lock));
    //asm volatile("fxsave (%0)" ::"r"((uint64_t)fpu_reg));
    asm_sse_save((uint64_t)fpu_reg);
    for (int i = 0; i < 128; i++)
    {
        context[i] = fpu_reg[i];
    }
    unlock((&sse_lock));
}
__attribute__((optimize("O0"))) void load_sse_context(uint64_t *context)
{

    lock((&sse_lock));
    for (int i = 0; i < 128; i++)
    {
        fpu_reg[i] = context[i];
    }
    asm_sse_load((uint64_t)fpu_reg);

    unlock((&sse_lock));
    //asm volatile("fxrstor (%0)" ::"r"((uint64_t)fpu_reg));
}
