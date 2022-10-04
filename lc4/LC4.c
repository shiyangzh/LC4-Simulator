/*
 * LC4.c: Defines simulator functions for executing instructions
 */

#include "LC4.h"
#include <stdio.h>

/*
 * Reset the machine state as Pennsim would do
 */
void Reset(MachineState* CPU) 
{
    int i;

    CPU->PC = 0x8200;

    CPU->PSR = 1;
    CPU->PSR = CPU->PSR << 15;

    for (i = 0; i < 8; i++) {
        CPU->R[i] = 0;
    }

    CPU->rsMux_CTL = '\0';
    CPU->rtMux_CTL = '\0';
    CPU->rdMux_CTL = '\0';
    
    CPU->regFile_WE = '\0';
    CPU->NZP_WE = '\0';
    CPU->DATA_WE = '\0';

    CPU->regInputVal = 0;
    CPU->NZPVal = 0;
    CPU->dmemAddr = 0;
    CPU->dmemValue = 0;

    for (i = 0; i < 65536; i++) {
        CPU->memory[i] = 0;
    }
}


/*
 * Clear all of the control signals (set to 0)
 */
void ClearSignals(MachineState* CPU)
{
    CPU->rsMux_CTL = '\0';
    CPU->rtMux_CTL = '\0';
    CPU->rdMux_CTL = '\0';
    
    CPU->regFile_WE = '\0';
    CPU->NZP_WE = '\0';
    CPU->DATA_WE = '\0';
}


/*
 * This function should write out the current state of the CPU to the file output.
 */
void WriteOut(MachineState* CPU, FILE* output)
{

    int bits[16];
    int i;
    int d; // Rd

    unsigned short int n = CPU->memory[CPU->PC];
    for (i = 15; i >= 0; i--) {
        bits[i] = n % 2;
        n = n / 2;
    }
    d = bits[4] * 4 + bits[5] * 2 + bits[6];

    fprintf(output, "%04X ", CPU->PC);

    for (i = 0; i < 16; i++) {
        fprintf(output, "%d", bits[i]);
    }

    if (CPU->regFile_WE == '1') {
        if ((bits[0] == 1 && bits[1] == 1 && bits[2] == 1 && bits[3] == 1) || (bits[0] == 0 && bits[1] == 1 && bits[2] == 0 && bits[3] == 0)) { // if TRAP or JSR
            fprintf(output, " %d %d %04X ", CPU->regFile_WE - '0', 7, CPU->regInputVal);
        } else {
            fprintf(output, " %d %d %04X ", CPU->regFile_WE - '0', d, CPU->regInputVal);
        }
    } else {
        fprintf(output, " 0 0 0000 ");
    }

    if (CPU->NZP_WE == '1') {
        fprintf(output, "%d %d ", CPU->NZP_WE - '0', CPU->NZPVal);
    } else {
        fprintf(output, "0 0 ");
    }

    if ((bits[0] == 0 && bits[1] == 1 && bits[2] == 1 && bits[3] == 0) || (bits[0] == 0 && bits[1] == 1 && bits[2] == 1 && bits[3] == 1)) { // if LDR or STR
        fprintf(output, "%d %04X %04X", CPU->DATA_WE - '0', CPU->dmemAddr, CPU->dmemValue);
    } else {
        fprintf(output, "0 0000 0000");
    }

    fprintf(output, "\n");
}


/*
 * This function should execute one LC4 datapath cycle.
 */
int UpdateMachineState(MachineState* CPU, FILE* output)
{
    char binary[16];
    int bits[16];
    int i;
    int d; // Rd
    int s; // Rs
    int t; // Rt
    unsigned short int u; // UIMM6/8/9
    short int v;
    unsigned short int n = CPU->memory[CPU->PC];
    for (i = 15; i >= 0; i--) {
        binary[i] = n % 2 + '0';
        bits[i] = n % 2;
        n = n / 2;
    }

    if ((CPU->PSR < 32768 && CPU->PC >= 0x8000) || (CPU->PC >= 0x2000 && CPU->PC < 0x8000) || (CPU->PC >= 0xA000 && CPU->PC <= 0xFFFF)) {
        printf("error occurred\n");
        return 1;
    }
    
    if (binary[0] == '0' && binary[1] == '0' && binary[2] == '0' && binary[3] == '0') {
        BranchOp(CPU, output); 
    } else if (binary[0] == '0' && binary[1] == '0' && binary[2] == '0' && binary[3] == '1') {
        ArithmeticOp(CPU, output);
    } else if (binary[0] == '0' && binary[1] == '0' && binary[2] == '1' && binary[3] == '0') {
        ComparativeOp(CPU, output);
    } else if (binary[0] == '0' && binary[1] == '1' && binary[2] == '0' && binary[3] == '1') {
        LogicalOp(CPU, output);
    } else if (binary[0] == '1' && binary[1] == '1' && binary[2] == '0' && binary[3] == '0') {
        JumpOp(CPU, output);
    } else if (binary[0] == '0' && binary[1] == '1' && binary[2] == '0' && binary[3] == '0') {
        JSROp(CPU, output);
    } else if (binary[0] == '1' && binary[1] == '0' && binary[2] == '1' && binary[3] == '0') {
        ShiftModOp(CPU, output);
    } else if (binary[0] == '1' && binary[1] == '0' && binary[2] == '0' && binary[3] == '0') { // RTI
        CPU->rsMux_CTL = '1';
        CPU->rtMux_CTL = '0';
        CPU->rdMux_CTL = '0';

        CPU->regFile_WE = '0';
        CPU->NZP_WE = '0';
        CPU->DATA_WE = '0';

        WriteOut(CPU, output);
        CPU->PC = CPU->R[7];
        CPU->PSR = CPU->PSR << 1;
        CPU->PSR = CPU->PSR >> 1;
    } else if (binary[0] == '0' && binary[1] == '1' && binary[2] == '1' && binary[3] == '0') { // LDR
        d = bits[4] * 4 + bits[5] * 2 + bits[6]; // check error handling
        s = bits[7] * 4 + bits[8] * 2 + bits[9];
        u = bits[10] * 32 + bits[11] * 16 + bits[12] * 8 + bits[13] * 4 + bits[14] * 2 + bits[15];
        v = 1U << (5);
        u = (u ^ v) - v;

        CPU->rsMux_CTL = '0';
        CPU->rtMux_CTL = '0';
        CPU->rdMux_CTL = '0';

        CPU->regFile_WE = '1';
        CPU->NZP_WE = '1';
        CPU->DATA_WE = '0';

        

        CPU->dmemAddr = CPU->R[s] + u;
        if (CPU->PSR < 32768 && CPU->dmemAddr >= 0x8000) {
            printf("error occurred\n");
            return 1;
        }
        CPU->R[d] = CPU->memory[CPU->dmemAddr];
        SetNZP(CPU, CPU->R[d]);
        WriteOut(CPU, output);
        CPU->PC += 1; 
    } else if (binary[0] == '0' && binary[1] == '1' && binary[2] == '1' && binary[3] == '1') { // STR
        t = bits[4] * 4 + bits[5] * 2 + bits[6];
        s = bits[7] * 4 + bits[8] * 2 + bits[9];
        u = bits[10] * 32 + bits[11] * 16 + bits[12] * 8 + bits[13] * 4 + bits[14] * 2 + bits[15];
        v = 1U << (5);
        u = (u ^ v) - v;

        CPU->rsMux_CTL = '0';
        CPU->rtMux_CTL = '1';
        CPU->rdMux_CTL = '0';

        CPU->regFile_WE = '0';
        CPU->NZP_WE = '0';
        CPU->DATA_WE = '1';

        CPU->dmemAddr = CPU->R[s] + u;
        if (CPU->PSR < 32768 && CPU->dmemAddr >= 0x8000) {
            printf("error occurred\n");
            return 1;
        }
        CPU->memory[CPU->dmemAddr] = CPU->R[t];
        CPU->dmemValue = CPU->R[t];
        WriteOut(CPU, output);
        CPU->PC += 1; 
    } else if (binary[0] == '1' && binary[1] == '0' && binary[2] == '0' && binary[3] == '1') { // CONST
        d = bits[4] * 4 + bits[5] * 2 + bits[6];
        u = bits[7] * 256 + bits[8] * 128 + bits[9] * 64 + bits[10] * 32 + 
            bits[11] * 16 + bits[12] * 8 + bits[13] * 4 + bits[14] * 2 + bits[15];
        v = 1U << (8);
        u = (u ^ v) - v;

        CPU->rsMux_CTL = '0';
        CPU->rtMux_CTL = '0';
        CPU->rdMux_CTL = '0';

        CPU->regFile_WE = '1';
        CPU->NZP_WE = '1';
        CPU->DATA_WE = '0';

        CPU->R[d] = u;
        SetNZP(CPU, CPU->R[d]);
        CPU->regInputVal = CPU->R[d];
        WriteOut(CPU, output);
        CPU->PC += 1; 
    } else if (binary[0] == '1' && binary[1] == '1' && binary[2] == '0' && binary[3] == '1' && binary[7] == '1') { // HICONST
        d = bits[4] * 4 + bits[5] * 2 + bits[6];
        u = bits[8] * 128 + bits[9] * 64 + bits[10] * 32 + bits[11] * 16 + 
            bits[12] * 8 + bits[13] * 4 + bits[14] * 2 + bits[15];

        CPU->rsMux_CTL = '0';
        CPU->rtMux_CTL = '0';
        CPU->rdMux_CTL = '0';

        CPU->regFile_WE = '1';
        CPU->NZP_WE = '1';
        CPU->DATA_WE = '0';

        CPU->R[d] = (CPU->R[d] & 0xFF) | (u << 8);
        SetNZP(CPU, CPU->R[d]);
        CPU->regInputVal = CPU->R[d];
        WriteOut(CPU, output);
        CPU->PC += 1; 
    } else if (binary[0] == '1' && binary[1] == '1' && binary[2] == '1' && binary[3] == '1') { // TRAP
        // check error handling PSR
        u = bits[8] * 128 + bits[9] * 64 + bits[10] * 32 + bits[11] * 16 + 
            bits[12] * 8 + bits[13] * 4 + bits[14] * 2 + bits[15];

        CPU->rsMux_CTL = '0';
        CPU->rtMux_CTL = '0';
        CPU->rdMux_CTL = '1';

        CPU->regFile_WE = '1';
        CPU->NZP_WE = '1';
        CPU->DATA_WE = '0';

        CPU->PSR = CPU->PSR | 0x8000;
        CPU->R[7] = CPU->PC + 1;
        SetNZP(CPU, CPU->R[7]);
        CPU->regInputVal = CPU->R[7];
        WriteOut(CPU, output);
        CPU->PC = (0x8000 | u);
    }

    return 0;
}



//////////////// PARSING HELPER FUNCTIONS ///////////////////////////



/*
 * Parses rest of branch operation and updates state of machine.
 */
void BranchOp(MachineState* CPU, FILE* output)
{
    char binary[16];
    int bits[16];
    unsigned short int PSRbits[16];
    int i;
    unsigned short int val = CPU->PSR;
    short int u; // IMM9
    short int v;
    unsigned short int n = CPU->memory[CPU->PC];
    for (i = 15; i >= 0; i--) {
        binary[i] = n % 2 + '0';
        bits[i] = n % 2;
        n = n / 2;

        PSRbits[i] = val % 2;
        val = val / 2;
    }

    // if (bits[7] == 0) {
    //     u = bits[8] * 128 + bits[9] * 64 + bits[10] * 32 + bits[11] * 16 + bits[12] * 8 + bits[13] * 4 + bits[14] * 2 + bits[15];
    // } else {
    //     for (i = 12; i < 16; i++) {
    //         bits[i] = 1 - bits[i];
    //     }
    //     u = -1 * (bits[8] * 128 + bits[9] * 64 + bits[10] * 32 + bits[11] * 16 + bits[12] * 8 + bits[13] * 4 + bits[14] * 2 + bits[15]);
    // }

    u = bits[7] * 256 + bits[8] * 128 + bits[9] * 64 + bits[10] * 32 + bits[11] * 16 + bits[12] * 8 + bits[13] * 4 + bits[14] * 2 + bits[15];
    v = 1U << (8);
    u = (u ^ v) - v;

    CPU->rsMux_CTL = '0';
    CPU->rtMux_CTL = '0';
    CPU->rdMux_CTL = '0';

    CPU->regFile_WE = '0';
    CPU->NZP_WE = '0';
    CPU->DATA_WE = '0';

    if (binary[4] == '0' && binary[5] == '0' && binary[6] == '0') { // NOP
        WriteOut(CPU, output);
        CPU->PC += 1;
    } else if (binary[4] == '1' && binary[5] == '0' && binary[6] == '0') { // BRn
        WriteOut(CPU, output);
        if (PSRbits[13] == 1) {
            CPU->PC = CPU->PC + 1 + u;
        } else {
            CPU->PC += 1;
        }
    } else if (binary[4] == '1' && binary[5] == '1' && binary[6] == '0') { // BRnz
        WriteOut(CPU, output);
        if (PSRbits[13] == 1 || PSRbits[14] == 1) {
            CPU->PC = CPU->PC + 1 + u;
        } else {
            CPU->PC += 1;
        }
    } else if (binary[4] == '1' && binary[5] == '0' && binary[6] == '1') { // BRnp
        WriteOut(CPU, output);
        if (PSRbits[13] == 1 || PSRbits[15] == 1) {
            CPU->PC = CPU->PC + 1 + u;
        } else {
            CPU->PC += 1;
        }
    } else if (binary[4] == '0' && binary[5] == '1' && binary[6] == '0') { // BRz
        WriteOut(CPU, output);
        if (PSRbits[14] == 1) {
            CPU->PC = CPU->PC + 1 + u;
        } else {
            CPU->PC += 1;
        }
    } else if (binary[4] == '0' && binary[5] == '1' && binary[6] == '1') { // BRzp
        WriteOut(CPU, output);
        if (PSRbits[14] == 1 || PSRbits[15] == 1) {
            CPU->PC = CPU->PC + 1 + u;
        } else {
            CPU->PC += 1;
        }
    } else if (binary[4] == '0' && binary[5] == '0' && binary[6] == '1') { // BRp
        WriteOut(CPU, output);
        if (PSRbits[15] == 1) {
            CPU->PC = CPU->PC + 1 + u;
        } else {
            CPU->PC += 1;
        }
    } else if (binary[4] == '1' && binary[5] == '1' && binary[6] == '1') { // BRnzp
        WriteOut(CPU, output);
        CPU->PC = CPU->PC + 1 + u;
    }
}

/*
 * Parses rest of arithmetic operation and prints out.
 */
void ArithmeticOp(MachineState* CPU, FILE* output)
{
    char binary[16];
    int bits[16];
    int i;
    int d; // Rd
    int s; // Rs
    int t; // Rt
    unsigned short int u; // IMM5
    short int x;
    unsigned short int n = CPU->memory[CPU->PC];
    for (i = 15; i >= 0; i--) {
        binary[i] = n % 2 + '0';
        bits[i] = n % 2;
        n = n / 2;
    }

    d = bits[4] * 4 + bits[5] * 2 + bits[6];
    s = bits[7] * 4 + bits[8] * 2 + bits[9];
    t = bits[13] * 4 + bits[14] * 2 + bits[15];

    CPU->rsMux_CTL = '0';
    CPU->rtMux_CTL = '0';
    CPU->rdMux_CTL = '0';

    CPU->regFile_WE = '1';
    CPU->NZP_WE = '1';
    CPU->DATA_WE = '0';

    if (binary[10] == '0' && binary[11] == '0' && binary[12] == '0') {
        CPU->R[d] = CPU->R[s] + CPU->R[t];
        SetNZP(CPU, CPU->R[d]);
        CPU->regInputVal = CPU->R[d];
    } else if (binary[10] == '0' && binary[11] == '0' && binary[12] == '1') {
        CPU->R[d] = CPU->R[s] * CPU->R[t];
        SetNZP(CPU, CPU->R[d]);
        CPU->regInputVal = CPU->R[d];
    } else if (binary[10] == '0' && binary[11] == '1' && binary[12] == '0') {
        CPU->R[d] = CPU->R[s] - CPU->R[t];
        SetNZP(CPU, CPU->R[d]);
        CPU->regInputVal = CPU->R[d];
    } else if (binary[10] == '0' && binary[11] == '1' && binary[12] == '1') {
        CPU->R[d] = CPU->R[s] / CPU->R[t];
        SetNZP(CPU, CPU->R[d]);
        CPU->regInputVal = CPU->R[d];
    } else if (binary[10] == '1') {
        if (bits[11] == 0) {
            u = bits[12] * 8 + bits[13] * 4 + bits[14] * 2 + bits[15];
            CPU->R[d] = CPU->R[s] + u;
        } else {
            for (i = 12; i < 16; i++) {
                bits[i] = 1 - bits[i];
            }
            u = bits[12] * 8 + bits[13] * 4 + bits[14] * 2 + bits[15] + 1;
            CPU->R[d] = CPU->R[s] - u;
        }
        SetNZP(CPU, CPU->R[d]);
        CPU->regInputVal = CPU->R[d];
    }
    WriteOut(CPU, output);
    CPU->PC += 1; 
}

/*
 * Parses rest of comparative operation and prints out.
 */
void ComparativeOp(MachineState* CPU, FILE* output)
{
    char binary[16];
    int bits[16];
    int i;
    int s; // Rs
    int t; // Rt
    short signedS;
    short signedT;
    unsigned short int u; // UIMM7
    unsigned short int w;
    short int v;
    unsigned short int n = CPU->memory[CPU->PC];
    for (i = 15; i >= 0; i--) {
        binary[i] = n % 2 + '0';
        bits[i] = n % 2;
        n = n / 2;
    }

    s = bits[4] * 4 + bits[5] * 2 + bits[6];
    t = bits[13] * 4 + bits[14] * 2 + bits[15];

    CPU->rsMux_CTL = '1';
    CPU->rtMux_CTL = '0';
    CPU->rdMux_CTL = '0';

    CPU->regFile_WE = '0';
    CPU->NZP_WE = '1';
    CPU->DATA_WE = '0';

    if (binary[7] == '0' && binary[8] == '0') {
        signedS = (signed short) CPU->R[s]; 
        signedT = (signed short) CPU->R[t]; 
        SetNZP(CPU, signedS - signedT);
    } else if (binary[7] == '0' && binary[8] == '1') {
        if ((unsigned short)CPU->R[s] > (unsigned short)CPU->R[t]) {
            SetNZP(CPU, 1);
        } else if ((unsigned short)CPU->R[s] < (unsigned short)CPU->R[t]) {
            SetNZP(CPU, -1);
        } else {
            SetNZP(CPU, 0);
        }
        
    } else if (binary[7] == '1' && binary[8] == '0') {
        u = bits[9] * 64 + bits[10] * 32 + bits[11] * 16 + bits[12] * 8 + bits[13] * 4 + bits[14] * 2 + bits[15];
        v = 1U << (6);
        u = (u ^ v) - v;
        SetNZP(CPU, CPU->R[s] - u);
    } else if (binary[7] == '1' && binary[8] == '1') {
        u = bits[9] * 64 + bits[10] * 32 + bits[11] * 16 + bits[12] * 8 + bits[13] * 4 + bits[14] * 2 + bits[15];
        signedS = (signed short) (CPU->R[s] - u); 
        SetNZP(CPU, signedS);
        if ((unsigned short)CPU->R[s] > (unsigned short)u) {
            SetNZP(CPU, 1);
        } else if ((unsigned short)CPU->R[s] < (unsigned short)u) {
            SetNZP(CPU, -1);
        } else {
            SetNZP(CPU, 0);
        }
    }
    WriteOut(CPU, output);
    CPU->PC += 1; 
}

/*
 * Parses rest of logical operation and prints out.
 */
void LogicalOp(MachineState* CPU, FILE* output)
{
    char binary[16];
    int bits[16];
    int i;
    int d; // Rd
    int s; // Rs
    int t; // Rt
    short int u; // IMM5
    unsigned short int n = CPU->memory[CPU->PC];
    for (i = 15; i >= 0; i--) {
        binary[i] = n % 2 + '0';
        bits[i] = n % 2;
        n = n / 2;
    }

    d = bits[4] * 4 + bits[5] * 2 + bits[6];
    s = bits[7] * 4 + bits[8] * 2 + bits[9];
    t = bits[13] * 4 + bits[14] * 2 + bits[15];
    u = bits[11] * 16 + bits[12] * 8 + bits[13] * 4 + bits[14] * 2 + bits[15];

    CPU->rsMux_CTL = '0';
    CPU->rtMux_CTL = '0';
    CPU->rdMux_CTL = '0';

    CPU->regFile_WE = '1';
    CPU->NZP_WE = '1';
    CPU->DATA_WE = '0';

    if (binary[10] == '0' && binary[11] == '0' && binary[12] == '0') {
        CPU->R[d] = CPU->R[s] & CPU->R[t];
        SetNZP(CPU, CPU->R[d]);
        CPU->regInputVal = CPU->R[d];
    } else if (binary[10] == '0' && binary[11] == '0' && binary[12] == '1') {
        CPU->R[d] = ~CPU->R[s];
        SetNZP(CPU, CPU->R[d]);
        CPU->regInputVal = CPU->R[d];
    } else if (binary[10] == '0' && binary[11] == '1' && binary[12] == '0') {
        CPU->R[d] = CPU->R[s] | CPU->R[t];
        SetNZP(CPU, CPU->R[d]);
        CPU->regInputVal = CPU->R[d];
    } else if (binary[10] == '0' && binary[11] == '1' && binary[12] == '1') {
        CPU->R[d] = CPU->R[s] ^ CPU->R[t];
        SetNZP(CPU, CPU->R[d]);
        CPU->regInputVal = CPU->R[d];
    } else if (binary[10] == '1') {
        CPU->R[d] = CPU->R[s] & u;
        SetNZP(CPU, CPU->R[d]);
        CPU->regInputVal = CPU->R[d];
    }
    WriteOut(CPU, output);
    CPU->PC += 1; 
}

/*
 * Parses rest of jump operation and prints out.
 */
void JumpOp(MachineState* CPU, FILE* output)
{
    char binary[16];
    int bits[16];
    int i;
    int s; // Rs
    short int u; // IMM11
    short int v;
    unsigned short int n = CPU->memory[CPU->PC];
    for (i = 15; i >= 0; i--) {
        binary[i] = n % 2 + '0';
        bits[i] = n % 2;
        n = n / 2;
    }

    s = bits[7] * 4 + bits[8] * 2 + bits[9];
    u = bits[5] * 1024 + bits[6] * 512 + bits[7] * 256 + bits[8] * 128 + bits[9] * 64 + 
        bits[10] * 32 + bits[11] * 16 + bits[12] * 8 + bits[13] * 4 + bits[14] * 2 + bits[15];

    v = 1U << (10);
    u = (u ^ v) - v;

    CPU->rsMux_CTL = '0';
    CPU->rtMux_CTL = '0';
    CPU->rdMux_CTL = '0';

    CPU->regFile_WE = '0';
    CPU->NZP_WE = '0';
    CPU->DATA_WE = '0';

    if (binary[4] == '0') {
        WriteOut(CPU, output);
        CPU->PC = CPU->R[s];
    } else if (binary[4] == '1') {
        WriteOut(CPU, output);
        CPU->PC = CPU->PC + 1 + u;
    }
}

/*
 * Parses rest of JSR operation and prints out.
 */
void JSROp(MachineState* CPU, FILE* output)
{
    char binary[16];
    int bits[16];
    int i;
    int s; // Rs
    short int u; // IMM11
    short int v;
    unsigned short int n = CPU->memory[CPU->PC];
    for (i = 15; i >= 0; i--) {
        binary[i] = n % 2 + '0';
        bits[i] = n % 2;
        n = n / 2;
    }

    s = bits[7] * 4 + bits[8] * 2 + bits[9];
    u = bits[5] * 1024 + bits[6] * 512 + bits[7] * 256 + bits[8] * 128 + bits[9] * 64 + 
        bits[10] * 32 + bits[11] * 16 + bits[12] * 8 + bits[13] * 4 + bits[14] * 2 + bits[15];
    v = 1U << (10);
    u = (u ^ v) - v;

    CPU->rsMux_CTL = '0';
    CPU->rtMux_CTL = '0';
    CPU->rdMux_CTL = '1';

    CPU->regFile_WE = '1';
    CPU->NZP_WE = '1';
    CPU->DATA_WE = '0';

    if (binary[4] == '0') {
        CPU->R[7] = CPU->PC + 1;
        SetNZP(CPU, CPU->R[7]);
        CPU->regInputVal = CPU->R[7];
        WriteOut(CPU, output);
        CPU->PC = CPU->R[s];
    } else if (binary[4] == '1') {
        CPU->R[7] = CPU->PC + 1;
        SetNZP(CPU, CPU->R[7]);
        CPU->regInputVal = CPU->R[7];
        WriteOut(CPU, output);
        CPU->PC = (CPU->PC & 0x8000) | (u << 4);
    }
}

/*
 * Parses rest of shift/mod operations and prints out.
 */
void ShiftModOp(MachineState* CPU, FILE* output)
{
    char binary[16];
    int bits[16];
    int i;
    int d; // Rd
    int s; // Rs
    int t; // Rt
    unsigned short int u; // UIMM4
    unsigned short int n = CPU->memory[CPU->PC];
    signed short int sra;
    for (i = 15; i >= 0; i--) {
        binary[i] = n % 2 + '0';
        bits[i] = n % 2;
        n = n / 2;
    }

    d = bits[4] * 4 + bits[5] * 2 + bits[6];
    s = bits[7] * 4 + bits[8] * 2 + bits[9];
    t = bits[13] * 4 + bits[14] * 2 + bits[15];
    u = bits[12] * 8 + bits[13] * 4 + bits[14] * 2 + bits[15];

    CPU->rsMux_CTL = '0';
    CPU->rtMux_CTL = '0';
    CPU->rdMux_CTL = '0';

    CPU->regFile_WE = '1';
    CPU->NZP_WE = '1';
    CPU->DATA_WE = '0';

    if (binary[10] == '0' && binary[11] == '0') {
        CPU->R[d] = CPU->R[s] << u;
        SetNZP(CPU, CPU->R[d]);
        CPU->regInputVal = CPU->R[d];
    } else if (binary[10] == '0' && binary[11] == '1') {
        sra = (signed int)CPU->R[s];
        sra = sra >> u;
        CPU->R[d] = sra;
        SetNZP(CPU, CPU->R[d]);
        CPU->regInputVal = CPU->R[d];
    } else if (binary[10] == '1' && binary[11] == '0') {
        CPU->R[d] = CPU->R[s] >> u;
        SetNZP(CPU, CPU->R[d]);
        CPU->regInputVal = CPU->R[d];
    } else if (binary[10] == '1' && binary[11] == '1') {
        CPU->R[d] = CPU->R[s] % CPU->R[t];
        SetNZP(CPU, CPU->R[d]);
        CPU->regInputVal = CPU->R[d];
    }
    WriteOut(CPU, output);
    CPU->PC += 1; 
}

/*
 * Set the NZP bits in the PSR.
 */
void SetNZP(MachineState* CPU, short result)
{
    CPU->PSR = CPU->PSR >> 3;
    CPU->PSR = CPU->PSR << 3;
    if (result > 0) {
        CPU->PSR += 1;
        CPU->NZPVal = 1;
    } else if (result == 0) {
        CPU->PSR += 2;
        CPU->NZPVal = 2;
    } else if (result < 0) {
        CPU->PSR += 4;
        CPU->NZPVal = 4;
    }
}
