#include <cpu.h>
#include <bus.h>
#include <emu.h>
#include <instructions.h>

cpu_context ctx = {0};

void cpu_init() {

}

static void fetch_instruction() {
    ctx.curr_opcode = bus_read(ctx.regs.pc++);
    ctx.curr_inst = instruction_by_opcode(ctx.curr_opcode);
}

static void execute() {
    IN_PROC proc = inst_get_processor(ctx.curr_inst->type);

    if (!proc) {
        printf("No processor for instruction! %02X\n", ctx.curr_opcode);
        NO_IMPL
    }

    proc(&ctx);
}

bool cpu_step() {
    if (!ctx.halted) {
        u16 pc = ctx.regs.pc;

        fetch_instruction();
        fetch_data();
        printf("%04X: %-7s (%02X %02X %02X) A: %02X B: %02X C: %02X\n", 
            pc, inst_name(ctx.curr_inst->type), ctx.curr_opcode,
            bus_read(pc + 1), bus_read(pc + 2), ctx.regs.a, ctx.regs.b, ctx.regs.c);

        if (ctx.curr_inst == NULL) {
            printf("Unknown Instruction! %02X\n", ctx.curr_opcode);
            exit(-7);
        }

        execute();
    }
    return true;
}