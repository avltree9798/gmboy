#pragma once

#include <common.h>
#include <instructions.h>

typedef struct {
    u8 a, f;
    u8 b, c;
    u8 d, e;
    u8 h, l;
    u16 pc;
    u16 sp;
} cpu_registers;

typedef struct {
    cpu_registers regs;
    u16 fetched_data;
    u16 mem_dest;
    bool dest_is_mem;
    u8 curr_opcode;
    instruction* curr_inst;
    bool halted;
    bool stepping;
    bool int_master_enabled;
    u8 ie_register;
} cpu_context;

cpu_registers* cpu_get_regs();

void cpu_init();
bool cpu_step();
void fetch_data();
u16 cpu_read_reg(reg_type rt);
void cpu_set_reg(reg_type rt, u16 val);
void cpu_set_flags(cpu_context *ctx, char z, char n, char h, char c);

typedef void (*IN_PROC)(cpu_context *);

IN_PROC inst_get_processor(in_type type);

#define CPU_FLAG_Z BIT(ctx->regs.f, 7)
#define CPU_FLAG_C BIT(ctx->regs.f, 4)
