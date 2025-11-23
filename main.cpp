#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <bitset>
#include <iomanip>
#include <sstream>
#include <cstdint>
#include <algorithm>

using namespace std;

class IF_ID
{
public:
    uint32_t IR;
    uint32_t NPC;
    bool valid;

    IF_ID() : IR(0), NPC(0), valid(false) {}
};

class ID_EX
{
public:
    uint32_t IR;
    uint32_t NPC;
    int32_t A;
    int32_t B;
    int32_t Imm;
    bool valid;

    ID_EX() : IR(0), NPC(0), A(0), B(0), Imm(0), valid(false) {}
};

class EX_MEM
{
public:
    uint32_t IR;
    int32_t B;
    int32_t ALUOutput;
    bool cond;
    bool valid;

    EX_MEM() : IR(0), B(0), ALUOutput(0), cond(false), valid(false) {}
};

class MEM_WB
{
public:
    uint32_t IR;
    int32_t ALUOutput;
    int32_t LMD;
    bool valid;

    MEM_WB() : IR(0), ALUOutput(0), LMD(0), valid(false) {}
};

class RISCVSimulator
{
private:
    vector<uint32_t> instructionMemory;
    vector<int32_t> dataMemory;

    int32_t registers[32];
    uint32_t PC;

    IF_ID if_id, if_id_next;
    ID_EX id_ex, id_ex_next;
    EX_MEM ex_mem, ex_mem_next;
    MEM_WB mem_wb, mem_wb_next;

    int totalCycles;
    int if_utilization, id_utilization, ex_utilization, mem_utilization, wb_utilization;
    bool stall;
    bool branch_taken;
    bool squash_if_id;
    uint32_t branch_target;
    int instructionsCompleted;

    uint32_t getOpcode(uint32_t instruction);
    uint32_t getRd(uint32_t instruction);
    uint32_t getRs1(uint32_t instruction);
    uint32_t getRs2(uint32_t instruction);
    uint32_t getFunct3(uint32_t instruction);
    uint32_t getFunct7(uint32_t instruction);
    int32_t getImmI(uint32_t instruction);
    int32_t getImmS(uint32_t instruction);
    int32_t getImmB(uint32_t instruction);
    int32_t getImmU(uint32_t instruction);
    int32_t getImmJ(uint32_t instruction);

    bool checkDataHazard();
    void insertBubble();

public:
    RISCVSimulator();
    void loadProgram(const string &filename);
    void reset();
    void runCycle();
    void runInstruction();
    void displayState();
    void displayStatistics();

    void IF_stage();
    void ID_stage();
    void EX_stage();
    void MEM_stage();
    void WB_stage();

    bool isProgramComplete();
    int getTotalCycles() { return totalCycles; }
    int getInstructionsCompleted() { return instructionsCompleted; }
    void displayMemory(int start, int count, bool isData);
    void displayPipelineVisualization();
    string getRegisterName(int reg);
};

RISCVSimulator::RISCVSimulator()
{
    instructionMemory.resize(512, 0); // 2KB / 4 bytes = 512 words
    dataMemory.resize(512, 0);
    reset();
}

void RISCVSimulator::reset()
{
    for (int i = 0; i < 32; i++)
    {
        registers[i] = 0;
    }
    PC = 0;
    totalCycles = 0;
    if_utilization = id_utilization = ex_utilization = mem_utilization = wb_utilization = 0;
    stall = false;
    branch_taken = false;
    squash_if_id = false;
    instructionsCompleted = 0;

    if_id = IF_ID();
    id_ex = ID_EX();
    ex_mem = EX_MEM();
    mem_wb = MEM_WB();
}

void RISCVSimulator::loadProgram(const string &filename)
{
    ifstream file(filename);
    if (!file.is_open())
    {
        cerr << "Error: Could not open file " << filename << endl;
        exit(1);
    }

    string line;
    int index = 0;
    while (getline(file, line) && index < 512)
    {
        if (line.empty() || line[0] == '#')
            continue;

        line.erase(remove(line.begin(), line.end(), ' '), line.end());
        if (line.length() > 0)
        {
            instructionMemory[index++] = stoul(line, nullptr, 16);
        }
    }
    file.close();
}

uint32_t RISCVSimulator::getOpcode(uint32_t instruction)
{
    return instruction & 0x7F;
}

uint32_t RISCVSimulator::getRd(uint32_t instruction)
{
    return (instruction >> 7) & 0x1F;
}

uint32_t RISCVSimulator::getRs1(uint32_t instruction)
{
    return (instruction >> 15) & 0x1F;
}

uint32_t RISCVSimulator::getRs2(uint32_t instruction)
{
    return (instruction >> 20) & 0x1F;
}

uint32_t RISCVSimulator::getFunct3(uint32_t instruction)
{
    return (instruction >> 12) & 0x7;
}

uint32_t RISCVSimulator::getFunct7(uint32_t instruction)
{
    return (instruction >> 25) & 0x7F;
}

int32_t RISCVSimulator::getImmI(uint32_t instruction)
{
    int32_t imm = (instruction >> 20);
    if (imm & 0x800)
        imm |= 0xFFFFF000;
    return imm;
}

int32_t RISCVSimulator::getImmS(uint32_t instruction)
{
    int32_t imm = ((instruction >> 7) & 0x1F) | ((instruction >> 20) & 0xFE0);
    if (imm & 0x800)
        imm |= 0xFFFFF000;
    return imm;
}

int32_t RISCVSimulator::getImmB(uint32_t instruction)
{
    int32_t imm = ((instruction >> 7) & 0x1E) | ((instruction >> 20) & 0x7E0) |
                  ((instruction << 4) & 0x800) | ((instruction >> 19) & 0x1000);
    if (imm & 0x1000)
        imm |= 0xFFFFE000;
    return imm;
}

int32_t RISCVSimulator::getImmU(uint32_t instruction)
{
    return instruction & 0xFFFFF000;
}

int32_t RISCVSimulator::getImmJ(uint32_t instruction)
{
    int32_t imm = ((instruction >> 20) & 0x7FE) | ((instruction >> 9) & 0x800) |
                  (instruction & 0xFF000) | ((instruction >> 11) & 0x100000);
    if (imm & 0x100000)
        imm |= 0xFFE00000;
    return imm;
}

bool RISCVSimulator::checkDataHazard()
{
    if (!if_id.valid)
        return false;

    uint32_t rs1 = getRs1(if_id.IR);
    uint32_t rs2 = getRs2(if_id.IR);
    uint32_t opcode = getOpcode(if_id.IR);

    bool uses_rs1 = (opcode != 0x37 && opcode != 0x6F);
    bool uses_rs2 = (opcode == 0x33 || opcode == 0x23 || opcode == 0x63);

    if (id_ex.valid)
    {
        uint32_t rd_ex = getRd(id_ex.IR);
        uint32_t opcode_ex = getOpcode(id_ex.IR);

        if (rd_ex != 0 && (opcode_ex == 0x33 || opcode_ex == 0x13 ||
                           opcode_ex == 0x03 || opcode_ex == 0x37 ||
                           opcode_ex == 0x6F || opcode_ex == 0x67))
        {
            if (uses_rs1 && rd_ex == rs1)
                return true;
            if (uses_rs2 && rd_ex == rs2)
                return true;
        }
    }

    if (ex_mem.valid)
    {
        uint32_t rd_mem = getRd(ex_mem.IR);
        uint32_t opcode_mem = getOpcode(ex_mem.IR);

        if (rd_mem != 0 && (opcode_mem == 0x33 || opcode_mem == 0x13 ||
                            opcode_mem == 0x03 || opcode_mem == 0x37 ||
                            opcode_mem == 0x6F || opcode_mem == 0x67))
        {
            if (uses_rs1 && rd_mem == rs1)
                return true;
            if (uses_rs2 && rd_mem == rs2)
                return true;
        }
    }

    return false;
}

void RISCVSimulator::IF_stage()
{
    if (branch_taken)
    {
        if_id_next = IF_ID();
        return;
    }

    if (PC / 4 < instructionMemory.size() && instructionMemory[PC / 4] != 0)
    {
        if_id_next.IR = instructionMemory[PC / 4];
        if_id_next.NPC = PC + 4;
        if_id_next.valid = true;
        if_utilization++;
    }
    else
    {
        if_id_next = IF_ID();
    }
}

void RISCVSimulator::ID_stage()
{
    if (squash_if_id)
    {
        id_ex_next = ID_EX();
        squash_if_id = false;
        return;
    }

    if (!if_id.valid)
    {
        id_ex_next = ID_EX();
        return;
    }

    uint32_t opcode = getOpcode(if_id.IR);
    uint32_t rs1 = getRs1(if_id.IR);
    uint32_t rs2 = getRs2(if_id.IR);

    if (checkDataHazard() && opcode != 0x63 && opcode != 0x6F && opcode != 0x67)
    {
        id_ex_next = ID_EX();
        if_id_next = if_id;
        stall = true;
        return;
    }

    id_ex_next.IR = if_id.IR;
    id_ex_next.NPC = if_id.NPC;
    id_ex_next.A = registers[rs1];
    id_ex_next.B = registers[rs2];
    id_ex_next.valid = true;
    id_utilization++;

    if (opcode == 0x13 || opcode == 0x03 || opcode == 0x67)
    {
        id_ex_next.Imm = getImmI(if_id.IR);
    }
    else if (opcode == 0x23)
    {
        id_ex_next.Imm = getImmS(if_id.IR);
    }
    else if (opcode == 0x63)
    {
        id_ex_next.Imm = getImmB(if_id.IR);
    }
    else if (opcode == 0x37)
    {
        id_ex_next.Imm = getImmU(if_id.IR);
    }
    else if (opcode == 0x6F)
    {
        id_ex_next.Imm = getImmJ(if_id.IR);
    }
}

void RISCVSimulator::EX_stage()
{
    if (!id_ex.valid)
    {
        ex_mem_next = EX_MEM();
        return;
    }

    uint32_t opcode = getOpcode(id_ex.IR);
    uint32_t funct3 = getFunct3(id_ex.IR);
    uint32_t funct7 = getFunct7(id_ex.IR);

    ex_mem_next.IR = id_ex.IR;
    ex_mem_next.B = id_ex.B;
    ex_mem_next.valid = true;
    ex_mem_next.cond = false;
    ex_utilization++;

    if (opcode == 0x33)
    {
        if (funct3 == 0x0 && funct7 == 0x00)
        {
            ex_mem_next.ALUOutput = id_ex.A + id_ex.B;
        }
        else if (funct3 == 0x0 && funct7 == 0x20)
        {
            ex_mem_next.ALUOutput = id_ex.A - id_ex.B;
        }
        else if (funct3 == 0x0 && funct7 == 0x01)
        {
            int64_t result = (int64_t)id_ex.A * (int64_t)id_ex.B;
            ex_mem_next.ALUOutput = (int32_t)(result & 0xFFFFFFFF);
        }
        else if (funct3 == 0x4 && funct7 == 0x01)
        {
            ex_mem_next.ALUOutput = (id_ex.B != 0) ? id_ex.A / id_ex.B : -1;
        }
        else if (funct3 == 0x6 && funct7 == 0x01)
        {
            ex_mem_next.ALUOutput = (id_ex.B != 0) ? id_ex.A % id_ex.B : id_ex.A;
        }
        else if (funct3 == 0x7 && funct7 == 0x00)
        {
            ex_mem_next.ALUOutput = id_ex.A & id_ex.B;
        }
        else if (funct3 == 0x6 && funct7 == 0x00)
        {
            ex_mem_next.ALUOutput = id_ex.A | id_ex.B;
        }
        else if (funct3 == 0x1 && funct7 == 0x00)
        {
            ex_mem_next.ALUOutput = id_ex.A << (id_ex.B & 0x1F);
        }
        else if (funct3 == 0x5 && funct7 == 0x00)
        {
            ex_mem_next.ALUOutput = (uint32_t)id_ex.A >> (id_ex.B & 0x1F);
        }
        else if (funct3 == 0x2 && funct7 == 0x00)
        {
            ex_mem_next.ALUOutput = (id_ex.A < id_ex.B) ? 1 : 0;
        }
        else if (funct3 == 0x3 && funct7 == 0x00)
        {
            ex_mem_next.ALUOutput = ((uint32_t)id_ex.A < (uint32_t)id_ex.B) ? 1 : 0;
        }
    }
    else if (opcode == 0x13)
    {
        if (funct3 == 0x0)
        {
            uint32_t bit30 = (id_ex.IR >> 30) & 0x1;
            if (bit30 == 1)
            {
                ex_mem_next.ALUOutput = id_ex.A - id_ex.Imm;
            }
            else
            {
                ex_mem_next.ALUOutput = id_ex.A + id_ex.Imm;
            }
        }
        else if (funct3 == 0x7)
        {
            ex_mem_next.ALUOutput = id_ex.A & id_ex.Imm;
        }
        else if (funct3 == 0x6)
        {
            ex_mem_next.ALUOutput = id_ex.A | id_ex.Imm;
        }
        else if (funct3 == 0x1)
        {
            ex_mem_next.ALUOutput = id_ex.A << (id_ex.Imm & 0x1F);
        }
        else if (funct3 == 0x5)
        {
            ex_mem_next.ALUOutput = (uint32_t)id_ex.A >> (id_ex.Imm & 0x1F);
        }
        else if (funct3 == 0x2)
        {
            ex_mem_next.ALUOutput = (id_ex.A < id_ex.Imm) ? 1 : 0;
        }
        else if (funct3 == 0x3)
        {
            ex_mem_next.ALUOutput = ((uint32_t)id_ex.A < (uint32_t)id_ex.Imm) ? 1 : 0;
        }
    }
    else if (opcode == 0x03)
    {
        ex_mem_next.ALUOutput = id_ex.A + id_ex.Imm;
    }
    else if (opcode == 0x23)
    {
        ex_mem_next.ALUOutput = id_ex.A + id_ex.Imm;
    }
    else if (opcode == 0x63)
    {
        if (funct3 == 0x0)
        {
            ex_mem_next.cond = (id_ex.A == id_ex.B);
        }
        branch_target = (id_ex.NPC - 4) + id_ex.Imm;

        if (ex_mem_next.cond)
        {
            PC = branch_target;
        }
        else
        {
            PC = id_ex.NPC;
        }
        branch_taken = true;
        squash_if_id = true;
    }
    else if (opcode == 0x37)
    {
        ex_mem_next.ALUOutput = id_ex.Imm;
    }
    else if (opcode == 0x6F)
    {
        ex_mem_next.ALUOutput = id_ex.NPC;
        PC = (id_ex.NPC - 4) + id_ex.Imm;
        branch_taken = true;
        squash_if_id = true;
    }
    else if (opcode == 0x67)
    { // jalr
        ex_mem_next.ALUOutput = id_ex.NPC;
        PC = (id_ex.A + id_ex.Imm) & ~1;
        branch_taken = true;
        squash_if_id = true;
    }
}

void RISCVSimulator::MEM_stage()
{
    if (!ex_mem.valid)
    {
        mem_wb_next = MEM_WB();
        return;
    }

    uint32_t opcode = getOpcode(ex_mem.IR);

    mem_wb_next.IR = ex_mem.IR;
    mem_wb_next.ALUOutput = ex_mem.ALUOutput;
    mem_wb_next.valid = true;
    mem_utilization++;

    if (opcode == 0x03)
    {
        int address = ex_mem.ALUOutput / 4;
        if (address >= 0 && address < dataMemory.size())
        {
            mem_wb_next.LMD = dataMemory[address];
        }
    }
    else if (opcode == 0x23)
    {
        int address = ex_mem.ALUOutput / 4;
        if (address >= 0 && address < dataMemory.size())
        {
            dataMemory[address] = ex_mem.B;
        }
    }
}

void RISCVSimulator::WB_stage()
{
    if (!mem_wb.valid)
    {
        return;
    }

    uint32_t opcode = getOpcode(mem_wb.IR);
    uint32_t rd = getRd(mem_wb.IR);

    wb_utilization++;

    if (rd != 0)
    {
        if (opcode == 0x03)
        { // lw
            registers[rd] = mem_wb.LMD;
        }
        else if (opcode == 0x33 || opcode == 0x13 || opcode == 0x37 ||
                 opcode == 0x6F || opcode == 0x67)
        {
            registers[rd] = mem_wb.ALUOutput;

            if (opcode == 0x33 && getFunct3(mem_wb.IR) == 0x0 &&
                getFunct7(mem_wb.IR) == 0x01 && rd < 31)
            {
                uint32_t rs1 = getRs1(mem_wb.IR);
                uint32_t rs2 = getRs2(mem_wb.IR);
                int64_t result = (int64_t)registers[rs1] * (int64_t)registers[rs2];
                registers[rd + 1] = (int32_t)(result >> 32);
            }
        }
    }

    registers[0] = 0;
    instructionsCompleted++;
}

string RISCVSimulator::getRegisterName(int reg)
{
    const char *names[] = {
        "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
        "s0/fp", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
        "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
        "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"};
    return names[reg];
}

void RISCVSimulator::runCycle()
{
    bool was_stalled = stall;
    stall = false;

    WB_stage();
    MEM_stage();
    EX_stage();
    ID_stage();

    if (!was_stalled || !stall)
    {
        IF_stage();
    }
    else
    {
        if_id_next = if_id;
    }

    mem_wb = mem_wb_next;
    ex_mem = ex_mem_next;
    id_ex = id_ex_next;

    if (!stall)
    {
        if_id = if_id_next;
    }

    if (!stall && !branch_taken)
    {
        PC += 4;
    }

    branch_taken = false;

    totalCycles++;
}

void RISCVSimulator::displayState()
{
    cout << "\n========== Cycle " << totalCycles << " ==========\n";

    cout << "\n--- Pipeline Registers ---\n";
    cout << "IF/ID:  Valid=" << if_id.valid << " IR=0x" << hex << setw(8) << setfill('0') << if_id.IR
         << " NPC=" << dec << if_id.NPC << "\n";
    cout << "ID/EX:  Valid=" << id_ex.valid << " IR=0x" << hex << setw(8) << setfill('0') << id_ex.IR
         << " A=" << dec << id_ex.A << " B=" << id_ex.B << " Imm=" << id_ex.Imm << "\n";
    cout << "EX/MEM: Valid=" << ex_mem.valid << " IR=0x" << hex << setw(8) << setfill('0') << ex_mem.IR
         << " ALUOutput=" << dec << ex_mem.ALUOutput << " B=" << ex_mem.B << " cond=" << ex_mem.cond << "\n";
    cout << "MEM/WB: Valid=" << mem_wb.valid << " IR=0x" << hex << setw(8) << setfill('0') << mem_wb.IR
         << " ALUOutput=" << dec << mem_wb.ALUOutput << " LMD=" << mem_wb.LMD << "\n";

    cout << "\n--- Registers ---\n";
    for (int i = 0; i < 32; i += 4)
    {
        for (int j = 0; j < 4; j++)
        {
            int reg = i + j;
            cout << "x" << setw(2) << setfill('0') << reg << "(" << setw(5) << setfill(' ') << left
                 << getRegisterName(reg) << ")" << right << "=" << registers[reg] << setw(10);
            if (j < 3)
                cout << " ";
        }
        cout << "\n";
    }

    cout << "\nPC = " << PC << " (0x" << hex << PC << dec << ")\n";
    cout << "Stall = " << (stall ? "YES" : "NO") << "\n";
}

bool RISCVSimulator::isProgramComplete()
{
    return !if_id.valid && !id_ex.valid && !ex_mem.valid && !mem_wb.valid &&
           (PC / 4 >= instructionMemory.size() || instructionMemory[PC / 4] == 0);
}

void RISCVSimulator::displayMemory(int start, int count, bool isData)
{
    cout << "\n========== " << (isData ? "Data" : "Instruction") << " Memory ==========\n";
    cout << "Showing " << count << " words starting from address " << start << " (0x" << hex << start << dec << ")\n\n";

    for (int i = 0; i < count; i++)
    {
        int addr = start + (i * 4);
        int index = addr / 4;

        if (isData && index < dataMemory.size())
        {
            cout << "Address 0x" << hex << setw(4) << setfill('0') << addr << dec
                 << " [" << setw(4) << index << "]: "
                 << setw(10) << dataMemory[index] << " (0x" << hex << setw(8) << setfill('0')
                 << (uint32_t)dataMemory[index] << dec << ")\n";
        }
        else if (!isData && index < instructionMemory.size())
        {
            cout << "Address 0x" << hex << setw(4) << setfill('0') << addr << dec
                 << " [" << setw(4) << index << "]: "
                 << "0x" << hex << setw(8) << setfill('0') << instructionMemory[index] << dec << "\n";
        }
    }
    cout << "\n";
}

void RISCVSimulator::displayPipelineVisualization()
{
    cout << "\n======================================================================\n";
    cout << "|                    PIPELINE VISUALIZATION                            |\n";
    cout << "======================================================================\n\n";

    cout << "   ---------      ---------      ---------      ---------      ---------\n";
    cout << "  |   IF    |--->|   ID    |--->|   EX    |--->|   MEM   |--->|   WB    |\n";
    cout << "  |  Fetch  |    | Decode  |    | Execute |    | Memory  |    |  Write  |\n";
    cout << "   ---------      ---------      ---------      ---------      --------\n\n";

    cout << "Current Pipeline State (Cycle " << totalCycles << "):\n\n";

    cout << "+- IF Stage ----------------------------------------------------+\n";
    if (PC / 4 < instructionMemory.size() && instructionMemory[PC / 4] != 0)
    {
        cout << "|  Fetching from PC=" << PC << " (0x" << hex << PC << dec << ")\n";
        cout << "|  Instruction: 0x" << hex << setw(8) << setfill('0')
             << instructionMemory[PC / 4] << dec << "\n";
    }
    else
    {
        cout << "|  [EMPTY - No instruction to fetch]\n";
    }
    cout << "+---------------------------------------------------------------+\n\n";

    cout << "+- ID Stage (IF/ID Latch) -------------------------------------+\n";
    if (if_id.valid)
    {
        cout << "|  IR:  0x" << hex << setw(8) << setfill('0') << if_id.IR << dec << "\n";
        cout << "|  NPC: " << if_id.NPC << "\n";
        cout << "|  Status: Decoding instruction\n";
    }
    else
    {
        cout << "|  [BUBBLE - No valid instruction]\n";
    }
    cout << "+---------------------------------------------------------------+\n\n";

    cout << "+- EX Stage (ID/EX Latch) -------------------------------------+\n";
    if (id_ex.valid)
    {
        cout << "|  IR:  0x" << hex << setw(8) << setfill('0') << id_ex.IR << dec << "\n";
        cout << "|  A:   " << id_ex.A << "\n";
        cout << "|  B:   " << id_ex.B << "\n";
        cout << "|  Imm: " << id_ex.Imm << "\n";
        cout << "|  Status: Executing ALU operation\n";
    }
    else
    {
        cout << "|  [BUBBLE - No valid instruction]\n";
    }
    cout << "+---------------------------------------------------------------+\n\n";

    cout << "+- MEM Stage (EX/MEM Latch) -----------------------------------+\n";
    if (ex_mem.valid)
    {
        cout << "|  IR:        0x" << hex << setw(8) << setfill('0') << ex_mem.IR << dec << "\n";
        cout << "|  ALUOutput: " << ex_mem.ALUOutput << "\n";
        cout << "|  B:         " << ex_mem.B << "\n";
        cout << "|  Cond:      " << (ex_mem.cond ? "TRUE" : "FALSE") << "\n";
        cout << "|  Status: Accessing memory (if needed)\n";
    }
    else
    {
        cout << "|  [BUBBLE - No valid instruction]\n";
    }
    cout << "+---------------------------------------------------------------+\n\n";

    cout << "+- WB Stage (MEM/WB Latch) ------------------------------------+\n";
    if (mem_wb.valid)
    {
        cout << "|  IR:        0x" << hex << setw(8) << setfill('0') << mem_wb.IR << dec << "\n";
        cout << "|  ALUOutput: " << mem_wb.ALUOutput << "\n";
        cout << "|  LMD:       " << mem_wb.LMD << "\n";
        uint32_t rd = getRd(mem_wb.IR);
        cout << "|  Writing to: x" << rd;
        if (rd > 0)
            cout << " (" << getRegisterName(rd) << ")";
        cout << "\n";
        cout << "|  Status: Writing back to register\n";
    }
    else
    {
        cout << "|  [BUBBLE - No valid instruction]\n";
    }
    cout << "+---------------------------------------------------------------+\n\n";

    // Show hazards
    if (stall)
    {
        cout << "*** HAZARD DETECTED: Pipeline stalled due to data hazard ***\n";
    }
    if (squash_if_id)
    {
        cout << "*** CONTROL HAZARD: Branch/Jump detected, flushing pipeline ***\n";
    }

    cout << "\n";
}

void RISCVSimulator::displayStatistics()
{
    cout << "\n========== Execution Statistics ==========\n";
    cout << "Total Cycles: " << totalCycles << "\n";
    cout << "Instructions Completed: " << instructionsCompleted << "\n";

    cout << "\nStage Utilization:\n";
    cout << "  IF:  " << if_utilization << " / " << totalCycles
         << " = " << fixed << setprecision(2) << (100.0 * if_utilization / totalCycles) << "%\n";
    cout << "  ID:  " << id_utilization << " / " << totalCycles
         << " = " << (100.0 * id_utilization / totalCycles) << "%\n";
    cout << "  EX:  " << ex_utilization << " / " << totalCycles
         << " = " << (100.0 * ex_utilization / totalCycles) << "%\n";
    cout << "  MEM: " << mem_utilization << " / " << totalCycles
         << " = " << (100.0 * mem_utilization / totalCycles) << "%\n";
    cout << "  WB:  " << wb_utilization << " / " << totalCycles
         << " = " << (100.0 * wb_utilization / totalCycles) << "%\n";
}

int main()
{
    RISCVSimulator simulator;

    cout << "========================================\n";
    cout << "   RISC-V 5-Stage Pipeline Simulator\n";
    cout << "========================================\n\n";

    // Load program
    string filename;
    cout << "Enter the machine code file name: ";
    cin >> filename;

    simulator.loadProgram(filename);
    cout << "Program loaded successfully!\n\n";

    // Select mode
    int mode;
    cout << "Select execution mode:\n";
    cout << "1. Instruction Mode (step through instructions)\n";
    cout << "2. Cycle Mode (step through cycles)\n";
    cout << "Enter choice (1 or 2): ";
    cin >> mode;

    bool continueExecution = true;

    while (continueExecution && !simulator.isProgramComplete())
    {
        int steps;
        cout << "\nEnter number of " << (mode == 1 ? "instructions" : "cycles") << " to execute: ";
        cin >> steps;

        for (int i = 0; i < steps && !simulator.isProgramComplete(); i++)
        {
            if (mode == 1)
            {
                // Instruction mode - run cycles until one more instruction completes
                int instructionsBefore = simulator.getInstructionsCompleted();
                while (simulator.getInstructionsCompleted() == instructionsBefore && !simulator.isProgramComplete())
                {
                    simulator.runCycle();
                }
                simulator.displayState();
            }
            else
            {
                // Cycle mode
                simulator.runCycle();
                simulator.displayState();
            }
        }

        if (!simulator.isProgramComplete())
        {
            // Interactive menu
            char choice;
            cout << "\n+=======================================================+\n";
            cout << "|                    OPTIONS MENU                       |\n";
            cout << "+=======================================================+\n";
            cout << "  c - Continue execution\n";
            cout << "  v - View pipeline visualization\n";
            cout << "  m - View memory contents\n";
            cout << "  s - View statistics\n";
            cout << "  q - Quit and show final statistics\n";
            cout << "\nEnter your choice: ";
            cin >> choice;

            switch (choice)
            {
            case 'c':
            case 'C':
                continueExecution = true;
                break;

            case 'v':
            case 'V':
                simulator.displayPipelineVisualization();
                continueExecution = true;
                break;

            case 'm':
            case 'M':
            {
                char memType;
                int start, count;
                cout << "\nMemory type (i=instruction, d=data): ";
                cin >> memType;
                cout << "Start address (in bytes): ";
                cin >> start;
                cout << "Number of words to display: ";
                cin >> count;
                simulator.displayMemory(start, count, (memType == 'd' || memType == 'D'));
                continueExecution = true;
                break;
            }

            case 's':
            case 'S':
                simulator.displayStatistics();
                continueExecution = true;
                break;

            case 'q':
            case 'Q':
                continueExecution = false;
                break;

            default:
                cout << "Invalid choice. Continuing execution.\n";
                continueExecution = true;
            }
        }
    }

    cout << "\n\nProgram execution completed!\n";
    simulator.displayStatistics();

    return 0;
}