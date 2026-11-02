// =====================================================================
//  mayuri_pass2.cpp  -  MAYURI'S MODULE : Pass 2
//  ---------------------------------------------------------------------
//  Still NO encoding here: we only store each branch's target address
//  and the displacement (target - end-of-instruction) that Worklet 2
//  will need to write into the rel32 field. Actually producing bytes is
//  Worklet 2's job, not ours.
// =====================================================================
#include "mayuri_pass2.h"

bool pass2Resolve(vector<Instruction>& instructions,
                  const unordered_map<string, Symbol>& table, vector<AsmError>& errors) {
    bool ok = true;

    for (size_t i = 0; i < instructions.size(); i++) {
        Instruction& ins = instructions[i];

        for (size_t k = 0; k < ins.operands.size(); k++) {
            Operand& op = ins.operands[k];
            if (op.kind != "SYM") continue;

            unordered_map<string, Symbol>::const_iterator it = table.find(op.value);
            if (it == table.end()) {
                addError(errors, "Symbol Error", "Undefined symbol '" + op.value + "'",
                         ins.line, op.column);
                ok = false;
                continue;
            }
            op.resolved        = true;
            op.resolvedAddress = it->second.address;
            // distance from the END of this instruction to the target
            ins.displacement = (int)((long long)it->second.address -
                                     (long long)(ins.address + ins.length));
        }
    }
    return ok;
}
