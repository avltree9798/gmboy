#include <cpu.h>
#include <bus.h>
#include <emu.h>

extern cpu_context ctx;

void fetch_data() {
    ctx.mem_dest = 0;
    ctx.dest_is_mem = false;
    switch(ctx.curr_inst->mode) {
        case AM_IMP: break;
        case AM_R:
            ctx.fetched_data = cpu_read_reg(ctx.curr_inst->reg_1);
            break;
        case AM_R_R:
            ctx.fetched_data = cpu_read_reg(ctx.curr_inst->reg_2);
            return;
        case AM_R_D8:
            ctx.fetched_data = bus_read(ctx.regs.pc);
            emu_cycles(1);
            ctx.regs.pc++;
            break;
        case AM_R_D16: 
        case AM_D16: {
            u16 lo = bus_read(ctx.regs.pc);
            emu_cycles(1);
            u16 hi = bus_read(ctx.regs.pc + 1);
            emu_cycles(1);
            ctx.fetched_data = lo | (hi << 8);
            ctx.regs.pc += 2;
            break;
        }
        case AM_MR_R: {
            ctx.fetched_data = cpu_read_reg(ctx.curr_inst->reg_2);
            ctx.mem_dest = cpu_read_reg(ctx.curr_inst->reg_1);
            ctx.dest_is_mem = true;
            if (ctx.curr_inst->reg_1 == RT_C) {
                ctx.mem_dest |= 0xFF00;
            }
            break;
        }
        case AM_R_MR: {
            u16 addr = cpu_read_reg(ctx.curr_inst->reg_2);
            if (ctx.curr_inst->reg_2 == RT_C) {
                addr |= 0xFF00;
            }
            ctx.fetched_data = bus_read(addr);
            emu_cycles(1);
            break;
        }
        case AM_R_HLI: {
            ctx.fetched_data = bus_read(cpu_read_reg(ctx.curr_inst->reg_2));
            emu_cycles(1);
            cpu_set_reg(RT_HL, cpu_read_reg(RT_HL) + 1);
            break;
        }
        case AM_R_HLD: {
            ctx.fetched_data = bus_read(cpu_read_reg(ctx.curr_inst->reg_2));
            emu_cycles(1);
            cpu_set_reg(RT_HL, cpu_read_reg(RT_HL) - 1);
            break;
        }
        case AM_HLI_R: {
            ctx.fetched_data = cpu_read_reg(ctx.curr_inst->reg_2);
            ctx.mem_dest = cpu_read_reg(ctx.curr_inst->reg_1);
            ctx.dest_is_mem = true;
            cpu_set_reg(RT_HL, cpu_read_reg(RT_HL) + 1);
            break;
        }
        case AM_HLD_R: {
            ctx.fetched_data = cpu_read_reg(ctx.curr_inst->reg_2);
            ctx.mem_dest = cpu_read_reg(ctx.curr_inst->reg_1);
            ctx.dest_is_mem = true;
            cpu_set_reg(RT_HL, cpu_read_reg(RT_HL) - 1);
            break;
        }
        case AM_R_A8: {
            u8 imm = bus_read(ctx.regs.pc);
            emu_cycles(1);
            ctx.regs.pc++;
            ctx.fetched_data = bus_read(0xFF00 | imm);
            emu_cycles(1);
            break;
        }
        case AM_A8_R: {
            u8 imm = bus_read(ctx.regs.pc);
            emu_cycles(1);
            ctx.regs.pc++;
            ctx.mem_dest = 0xFF00 | imm;
            ctx.dest_is_mem = true;
            ctx.fetched_data = cpu_read_reg(ctx.curr_inst->reg_2);
            break;
        }
        case AM_HL_SPR: {
            ctx.fetched_data = bus_read(ctx.regs.pc);
            emu_cycles(1);
            ctx.regs.pc++;
            break;
        }
        case AM_D8: {
            ctx.fetched_data = bus_read(ctx.regs.pc);
            emu_cycles(1);
            ctx.regs.pc++;
            break;
        }
        case AM_A16_R:
        case AM_D16_R: {
            u16 lo = bus_read(ctx.regs.pc);
            emu_cycles(1);
            u16 hi = bus_read(ctx.regs.pc + 1);
            emu_cycles(1);
            ctx.mem_dest = lo | (hi << 8);
            ctx.dest_is_mem = true;
            ctx.regs.pc += 2;
            ctx.fetched_data = cpu_read_reg(ctx.curr_inst->reg_2);
            break;
        }
        case AM_MR_D8: {
            ctx.fetched_data = bus_read(ctx.regs.pc);
            emu_cycles(1);
            ctx.regs.pc++;
            ctx.mem_dest = cpu_read_reg(ctx.curr_inst->reg_1);
            ctx.dest_is_mem = true;
            break;
        }
        case AM_MR: {
            ctx.mem_dest = cpu_read_reg(ctx.curr_inst->reg_1);
            ctx.dest_is_mem = true;
            ctx.fetched_data = bus_read(cpu_read_reg(ctx.curr_inst->reg_1));
            emu_cycles(1);
            break;
        }
        case AM_R_A16: {
            u16 lo = bus_read(ctx.regs.pc);
            emu_cycles(1);

            u16 hi = bus_read(ctx.regs.pc + 1);
            emu_cycles(1);

            u16 addr = lo | (hi << 8);

            ctx.regs.pc += 2;
            ctx.fetched_data = bus_read(addr);
            emu_cycles(1);

            break;
        }
        default:
            printf("Unknown addressing mode! %d\n", ctx.curr_inst->mode);
            exit(-7);
    }
}