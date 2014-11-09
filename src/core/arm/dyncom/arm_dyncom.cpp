// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#include "core/arm/skyeye_common/armcpu.h"
#include "core/arm/skyeye_common/armemu.h"
#include "core/arm/skyeye_common/vfp/vfp.h"

#include "core/arm/dyncom/arm_dyncom.h"
#include "core/arm/dyncom/arm_dyncom_interpreter.h"

const static cpu_config_t s_arm11_cpu_info = {
    "armv6", "arm11", 0x0007b000, 0x0007f000, NONCACHE
};

ARM_DynCom::ARM_DynCom() : ticks(0) {
    state = std::unique_ptr<ARMul_State>(new ARMul_State);

    ARMul_EmulateInit();
    memset(state.get(), 0, sizeof(ARMul_State));

    ARMul_NewState((ARMul_State*)state.get());

    state->abort_model = 0;
    state->cpu = (cpu_config_t*)&s_arm11_cpu_info;
    state->bigendSig = LOW;

    ARMul_SelectProcessor(state.get(), ARM_v6_Prop | ARM_v5_Prop | ARM_v5e_Prop);
    state->lateabtSig = LOW;

    // Reset the core to initial state
    ARMul_CoProInit(state.get());
    ARMul_Reset(state.get());
    state->NextInstr = RESUME; // NOTE: This will be overwritten by LoadContext
    state->Emulate = 3;

    state->pc = state->Reg[15] = 0x00000000;
    state->Reg[13] = 0x10000000; // Set stack pointer to the top of the stack
    state->servaddr = 0xFFFF0000;
    state->NirqSig = HIGH;

    VFPInit(state.get()); // Initialize the VFP

    ARMul_EmulateInit();
}

ARM_DynCom::~ARM_DynCom() {
}

/**
 * Set the Program Counter to an address
 * @param addr Address to set PC to
 */
void ARM_DynCom::SetPC(u32 pc) {
    state->pc = state->Reg[15] = pc;
}

/*
 * Get the current Program Counter
 * @return Returns current PC
 */
u32 ARM_DynCom::GetPC() const {
    return state->Reg[15];
}

/**
 * Get an ARM register
 * @param index Register index (0-15)
 * @return Returns the value in the register
 */
u32 ARM_DynCom::GetReg(int index) const {
    return state->Reg[index];
}

/**
 * Set an ARM register
 * @param index Register index (0-15)
 * @param value Value to set register to
 */
void ARM_DynCom::SetReg(int index, u32 value) {
    state->Reg[index] = value;
}

/**
 * Get the current CPSR register
 * @return Returns the value of the CPSR register
 */
u32 ARM_DynCom::GetCPSR() const {
    return state->Cpsr;
}

/**
 * Set the current CPSR register
 * @param cpsr Value to set CPSR to
 */
void ARM_DynCom::SetCPSR(u32 cpsr) {
    state->Cpsr = cpsr;
}

/**
 * Returns the number of clock ticks since the last reset
 * @return Returns number of clock ticks
 */
u64 ARM_DynCom::GetTicks() const {
    return ticks;
}

/**
 * Executes the given number of instructions
 * @param num_instructions Number of instructions to executes
 */
void ARM_DynCom::ExecuteInstructions(int num_instructions) {
    state->NumInstrsToExecute = num_instructions;

    // Dyncom only breaks on instruction dispatch. This only happens on every instruction when
    // executing one instruction at a time. Otherwise, if a block is being executed, more 
    // instructions may actually be executed than specified.
    ticks += InterpreterMainLoop(state.get());
}

/**
 * Saves the current CPU context
 * @param ctx Thread context to save
 * @todo Do we need to save Reg[15] and NextInstr?
 */
void ARM_DynCom::SaveContext(ThreadContext& ctx) {
    memcpy(ctx.cpu_registers, state->Reg, sizeof(ctx.cpu_registers));
    memcpy(ctx.fpu_registers, state->ExtReg, sizeof(ctx.fpu_registers));

    ctx.sp = state->Reg[13];
    ctx.lr = state->Reg[14];
    ctx.pc = state->Reg[15];
    ctx.cpsr = state->Cpsr;

    ctx.fpscr = state->VFP[1];
    ctx.fpexc = state->VFP[2];

    ctx.reg_15 = state->Reg[15];
    ctx.mode = state->NextInstr;
}

/**
 * Loads a CPU context
 * @param ctx Thread context to load
 * @param Do we need to load Reg[15] and NextInstr?
 */
void ARM_DynCom::LoadContext(const ThreadContext& ctx) {
    memcpy(state->Reg, ctx.cpu_registers, sizeof(ctx.cpu_registers));
    memcpy(state->ExtReg, ctx.fpu_registers, sizeof(ctx.fpu_registers));

    state->Reg[13] = ctx.sp;
    state->Reg[14] = ctx.lr;
    state->pc = ctx.pc;
    state->Cpsr = ctx.cpsr;

    state->VFP[1] = ctx.fpscr;
    state->VFP[2] = ctx.fpexc;

    state->Reg[15] = ctx.reg_15;
    state->NextInstr = ctx.mode;
}

/// Prepare core for thread reschedule (if needed to correctly handle state)
void ARM_DynCom::PrepareReschedule() {
    state->NumInstrsToExecute = 0;
}
