//
//  This file is part of the Patmos Simulator.
//  The Patmos Simulator is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  The Patmos Simulator is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with the Patmos Simulator. If not, see <http://www.gnu.org/licenses/>.
//
//
// Definition of simulation functions for every Patmos instruction.
//

#ifndef PATMOS_INSTRUCTIONS_H
#define PATMOS_INSTRUCTIONS_H

#include "endian-conversion.h"
#include "data-cache.h"
#include "instruction.h"
#include "memory.h"
#include "method-cache.h"
#include "simulation-core.h"
#include "stack-cache.h"
#include "symbol.h"

#include <ostream>
#include <boost/format.hpp>

namespace patmos
{
  template<typename T>
  T &i_mk()
  {
    static T t;
    return t;
  }

  /// A NOP instruction, which does really nothing, except incrementing the PC.
  class i_nop_t : public instruction_t
  {
  public:
    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print(std::ostream &os, const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      os << "nop";
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the IF pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void IF(simulator_t &s, instruction_data_t &ops) const
    {
      s.PC = s.nPC;
    }

    /// Commit function to commit the shadow state of the instruction in
    /// the IF pipeline stage to the global state.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void IF_commit(simulator_t &s, instruction_data_t &ops) const
    {
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
    }

    /// Commit function to commit the shadow state of the instruction in
    /// the DR pipeline stage to the global state.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR_commit(simulator_t &s, instruction_data_t &ops) const
    {
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the EX pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
    }

    /// Commit function to commit the shadow state of the instruction in
    /// the EX pipeline stage to the global state.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void EX_commit(simulator_t &s, instruction_data_t &ops) const
    {
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the MW pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void MW(simulator_t &s, instruction_data_t &ops) const
    {
    }

    /// Commit function to commit the shadow state of the instruction in
    /// the MW pipeline stage to the global state.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void MW_commit(simulator_t &s, instruction_data_t &ops) const
    {
    }

    /// Pipeline function to simulate the behavior of a decoupled load
    /// instruction running in parallel to the pipeline.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void dMW(simulator_t &s, instruction_data_t &ops) const
    {
    }
  };

  /// Abstract base class of predicated instructions, i.e., all instructions
  /// that actually do something.
  class i_pred_t : public i_nop_t
  {
  protected:
    /// Read a GPR register at the EX stage.
    /// @param s The parent simulator.
    /// @param op The register operand.
    /// @return The register value, considering by-passing from the EX, and MW
    /// stages.
    static inline word_t read_GPR_EX(simulator_t &s, GPR_op_t op)
    {
      return s.Pipeline[SEX][0].GPR_EX_Rd.get(
             s.Pipeline[SEX][1].GPR_EX_Rd.get(
              s.Pipeline[SMW][0].GPR_MW_Rd.get(
              s.Pipeline[SMW][1].GPR_MW_Rd.get(
               op)))).get();
    }

  public:
    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print(std::ostream &os, const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      assert(false);
    }
  };

  /// Base class for ALUi or ALUl instructions.
  class i_aluil_t : public i_pred_t
  {
  public:
    /// Compute the result of an ALUi or ALUl instruction.
    /// @param value1 The value of the first operand.
    /// @param value2 The value of the second operand.
    virtual word_t compute(word_t value1, word_t value2) const = 0;

    // IF inherited from NOP

    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.DR_Rs1 = s.GPR.get(ops.OPS.ALUil.Rs1);
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the EX pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      // compute the result of the ALU instruction
      ops.EX_result = compute(read_GPR_EX(s, ops.DR_Rs1), ops.OPS.ALUil.Imm2);
    }

    /// Commit function to commit the shadow state of the instruction in
    /// the EX pipeline stage to the global state.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void EX_commit(simulator_t &s, instruction_data_t &ops) const
    {
      if (ops.DR_Pred)
      {
        // store the result by writing it into a by-pass
        ops.GPR_EX_Rd.set(ops.OPS.ALUil.Rd, ops.EX_result);
      }
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the MW pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void MW(simulator_t &s, instruction_data_t &ops) const
    {
      if (ops.DR_Pred)
      {
        s.GPR.set(ops.GPR_EX_Rd.get());
        ops.GPR_MW_Rd.set(ops.GPR_EX_Rd.get());
        ops.GPR_EX_Rd.reset();
      }
    }

    /// Commit function to commit the shadow state of the instruction in
    /// the MW pipeline stage to the global state.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void MW_commit(simulator_t &s, instruction_data_t &ops) const
    {
      if (ops.DR_Pred)
      {
        ops.GPR_MW_Rd.reset();
      }
    }
  };

#define ALUil_INSTR(name, expr) \
  class i_ ## name ## _t : public i_aluil_t \
  { \
  public:\
    virtual void print(std::ostream &os, const instruction_data_t &ops, \
                       const symbol_map_t &symbols) const \
    { \
      os << boost::format("(p%2%) %1% r%3% = r%4%, %5%") % #name \
          % ops.Pred % ops.OPS.ALUil.Rd % ops.OPS.ALUil.Rs1 \
          % ops.OPS.ALUil.Imm2; \
      symbols.print(os, ops.OPS.ALUil.Imm2); \
    } \
    virtual word_t compute(word_t value1, word_t value2) const \
    { \
      return expr; \
    } \
  };

  ALUil_INSTR(addil , value1          +  value2                  )
  ALUil_INSTR(subil , value1          -  value2                  )
  ALUil_INSTR(rsubil, value2          -  value1                  )
  ALUil_INSTR(slil  , value1          << (value2 & 0x1F)         )
  ALUil_INSTR(sril  , (uword_t)value1 >> (uword_t)(value2 & 0x1F))
  ALUil_INSTR(srail , value1          >> (value2 & 0x1F)         )
  ALUil_INSTR(oril  , value1          |  value2                  )
  ALUil_INSTR(andil , value1          &  value2                  )

  ALUil_INSTR(rll    , value1 << (value2 & 0x1F)        | ((uword_t)value1 >> (32 - (value2 & 0x1F))) )
  ALUil_INSTR(rrl    , value1 << (32 - (value2 & 0x1F)) | ((uword_t)value1 >> (value2 & 0x1F))        )
  ALUil_INSTR(xorl   , value1          ^  value2                  )
  ALUil_INSTR(norl   , ~(value1        |  value2)                 )
  ALUil_INSTR(shaddl , (value1 << 1)   +  value2                  )
  ALUil_INSTR(shadd2l, (value1 << 2)   +  value2                  )

  /// Base class for ALUr instructions.
  class i_alur_t : public i_pred_t
  {
  public:
    /// Compute the result of an ALUr instruction.
    /// @param value1 The value of the first operand.
    /// @param value2 The value of the second operand.
    virtual word_t compute(word_t value1, word_t value2) const = 0;

    // IF inherited from NOP

    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.DR_Rs1 = s.GPR.get(ops.OPS.ALUr.Rs1);
      ops.DR_Rs2 = s.GPR.get(ops.OPS.ALUr.Rs2);
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the EX pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      // compute the result of the ALU instruction
      ops.EX_result = compute(read_GPR_EX(s, ops.DR_Rs1),
                              read_GPR_EX(s, ops.DR_Rs2));
    }

    /// Commit function to commit the shadow state of the instruction in
    /// the EX pipeline stage to the global state.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void EX_commit(simulator_t &s, instruction_data_t &ops) const
    {
      if (ops.DR_Pred)
      {
        // store the result by writing it into a by-pass.
        ops.GPR_EX_Rd.set(ops.OPS.ALUr.Rd, ops.EX_result);
      }
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the MW pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void MW(simulator_t &s, instruction_data_t &ops) const
    {
      if (ops.DR_Pred)
      {
        s.GPR.set(ops.GPR_EX_Rd.get());
        ops.GPR_MW_Rd.set(ops.GPR_EX_Rd.get());
        ops.GPR_EX_Rd.reset();
      }
    }

    /// Commit function to commit the shadow state of the instruction in
    /// the MW pipeline stage to the global state.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void MW_commit(simulator_t &s, instruction_data_t &ops) const
    {
      if (ops.DR_Pred)
      {
        ops.GPR_MW_Rd.reset();
      }
    }
  };

#define ALUr_INSTR(name, expr) \
  class i_ ## name ## _t : public i_alur_t \
  { \
  public:\
    virtual void print(std::ostream &os, const instruction_data_t &ops, \
                       const symbol_map_t &symbols) const \
    { \
      os << boost::format("(p%2%) %1% r%3% = r%4%, r%5%") % #name \
          % ops.Pred % ops.OPS.ALUr.Rd % ops.OPS.ALUr.Rs1 % ops.OPS.ALUr.Rs2; \
    } \
    virtual word_t compute(word_t value1, word_t value2) const \
    { \
      return expr; \
    } \
  };

  ALUr_INSTR(add   , value1          +  value2                  )
  ALUr_INSTR(sub   , value1          -  value2                  )
  ALUr_INSTR(rsub  , value2          -  value1                  )
  ALUr_INSTR(sl    , value1          << (value2 & 0x1F)         )
  ALUr_INSTR(sr    , (uword_t)value1 >> (uword_t)(value2 & 0x1F))
  ALUr_INSTR(sra   , value1          >> (value2 & 0x1F)         )
  ALUr_INSTR(or    , value1          |  value2                  )
  ALUr_INSTR(and   , value1          &  value2                  )

  ALUr_INSTR(rl    , value1 << (value2 & 0x1F)        | ((uword_t)value1 >> (32 - (value2 & 0x1F))) )
  ALUr_INSTR(rr    , value1 << (32 - (value2 & 0x1F)) | ((uword_t)value1 >> (value2 & 0x1F))        )
  ALUr_INSTR(xor   , value1          ^  value2                  )
  ALUr_INSTR(nor   , ~(value1        |  value2)                 )
  ALUr_INSTR(shadd , (value1 << 1)   +  value2                  )
  ALUr_INSTR(shadd2, (value1 << 2)   +  value2                  )

  /// Base class for ALUu instructions.
  class i_aluu_t : public i_pred_t
  {
  public:
    /// Compute the result of an ALUu instruction.
    /// @param value The value of the operand.
    virtual word_t compute(word_t value) const = 0;

    // IF inherited from NOP

    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.DR_Rs1 = s.GPR.get(ops.OPS.ALUu.Rs1);
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the EX pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      ops.EX_result = compute(read_GPR_EX(s, ops.DR_Rs1));
    }

    /// Commit function to commit the shadow state of the instruction in
    /// the EX pipeline stage to the global state.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void EX_commit(simulator_t &s, instruction_data_t &ops) const
    {
      if (ops.DR_Pred)
      {
        // store the result by writing it into a by-pass.
        ops.GPR_EX_Rd.set(ops.OPS.ALUu.Rd, ops.EX_result);
      }
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the MW pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void MW(simulator_t &s, instruction_data_t &ops) const
    {
      if (ops.DR_Pred)
      {
        s.GPR.set(ops.GPR_EX_Rd.get());
        ops.GPR_MW_Rd.set(ops.GPR_EX_Rd.get());
        ops.GPR_EX_Rd.reset();
      }
    }

    /// Commit function to commit the shadow state of the instruction in
    /// the MW pipeline stage to the global state.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void MW_commit(simulator_t &s, instruction_data_t &ops) const
    {
      if (ops.DR_Pred)
      {
        ops.GPR_MW_Rd.reset();
      }
    }
  };

#define ALUu_INSTR(name, operator) \
  class i_ ## name ## _t : public i_aluu_t \
  { \
  public:\
    virtual void print(std::ostream &os, const instruction_data_t &ops, \
                       const symbol_map_t &symbols) const \
    { \
      os << boost::format("(p%2%) %1% r%3% = r%4%") % #name \
          % ops.Pred % ops.OPS.ALUu.Rd % ops.OPS.ALUu.Rs1; \
    } \
    virtual word_t compute(word_t value1) const \
    { \
      return operator(value1); \
    } \
  };

  ALUu_INSTR(sext8 , (word_t)(int8_t))
  ALUu_INSTR(sext16, (word_t)(int16_t))
  ALUu_INSTR(zext16, (word_t)(uint16_t))
  ALUu_INSTR(abs   , std::abs)

  /// Base class for ALUr instructions.
  class i_alum_t : public i_pred_t
  {
  public:
    /// Compute the result of an ALUr instruction.
    /// @param value1 The value of the first operand.
    /// @param value2 The value of the second operand.
    virtual dword_t compute(word_t value1, word_t value2) const = 0;

    // IF inherited from NOP

    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.DR_Rs1 = s.GPR.get(ops.OPS.ALUm.Rs1);
      ops.DR_Rs2 = s.GPR.get(ops.OPS.ALUm.Rs2);
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the EX pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      // compute the result of the ALU instruction
      dword_t result = compute(read_GPR_EX(s, ops.DR_Rs1),
                               read_GPR_EX(s, ops.DR_Rs2));

      ops.EX_mull = (word_t)result;
      ops.EX_mulh = result >> sizeof(word_t)*8;
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the MW pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void MW(simulator_t &s, instruction_data_t &ops) const
    {
      if (ops.DR_Pred)
      {
        s.SPR.set(sl, ops.EX_mull);
        s.SPR.set(sh, ops.EX_mulh);
      }
    }
  };

#define ALUm_INSTR(name, type, stype) \
  class i_ ## name ## _t : public i_alum_t \
  { \
  public:\
    virtual void print(std::ostream &os, const instruction_data_t &ops, \
                       const symbol_map_t &symbols) const \
    { \
      os << boost::format("(p%2%) %1% r%3%, r%4%") % #name \
          % ops.Pred % ops.OPS.ALUm.Rs1 % ops.OPS.ALUm.Rs2; \
    } \
    virtual dword_t compute(word_t value1, word_t value2) const \
    { \
      return ((type)(stype)value1) * ((type)(stype)value2); \
    } \
  };

  ALUm_INSTR(mul , dword_t, word_t)
  ALUm_INSTR(mulu, udword_t, uword_t)

  /// Base class for ALUc instructions.
  class i_aluc_t : public i_pred_t
  {
  public:
    /// Compute the result of an ALUc instruction.
    /// @param value1 The value of the first operand.
    /// @param value2 The value of the second operand.
    virtual bit_t compute(word_t value1, word_t value2) const = 0;

    // IF inherited from NOP

    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.DR_Rs1 = s.GPR.get(ops.OPS.ALUc.Rs1);
      ops.DR_Rs2 = s.GPR.get(ops.OPS.ALUc.Rs2);
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the EX pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      if (ops.DR_Pred)
      {
        // compute the result of the comparison instruction
        bit_t result = compute(read_GPR_EX(s, ops.DR_Rs1),
                               read_GPR_EX(s, ops.DR_Rs2));

        // store the result by writing it into the register file.
        s.PRR.set(ops.OPS.ALUc.Pd, result);
      }
    }

    // MW inherited from NOP
  };

#define ALUc_INSTR(name, operator) \
  class i_ ## name ## _t : public i_aluc_t \
  { \
  public:\
    virtual void print(std::ostream &os, const instruction_data_t &ops, \
                       const symbol_map_t &symbols) const \
    { \
      os << boost::format("(p%2%) %1% p%3% = r%4%, r%5%") % #name \
          % ops.Pred % ops.OPS.ALUc.Pd % ops.OPS.ALUc.Rs1 % ops.OPS.ALUc.Rs2; \
    } \
    virtual bit_t compute(word_t value1, word_t value2) const \
    { \
      return value1 operator value2; \
    } \
  };

  ALUc_INSTR(cmpeq , ==)
  ALUc_INSTR(cmpneq, !=)
  ALUc_INSTR(cmplt , <)
  ALUc_INSTR(cmple , <=)

  #define ALUcu_INSTR(name, operator) \
  class i_ ## name ## _t : public i_aluc_t \
  { \
  public:\
    virtual void print(std::ostream &os, const instruction_data_t &ops, \
                       const symbol_map_t &symbols) const \
    { \
      os << boost::format("(p%2%) %1% p%3% = r%4%, r%5%") % #name \
          % ops.Pred % ops.OPS.ALUc.Pd % ops.OPS.ALUc.Rs1 % ops.OPS.ALUc.Rs2; \
    } \
    virtual bit_t compute(word_t value1, word_t value2) const \
    { \
      return ((uword_t)value1) operator ((uword_t)value2); \
    } \
  };

  ALUcu_INSTR(cmpult, <)
  ALUcu_INSTR(cmpule, <=)

  class i_btest_t : public i_aluc_t
  {
  public:
    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print(std::ostream &os, const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      os << boost::format("(p%1%) btest p%2% = r%3%, r%4%")
          % ops.Pred % ops.OPS.ALUc.Pd % ops.OPS.ALUc.Rs1 % ops.OPS.ALUc.Rs2;
    }

    virtual bit_t compute(word_t value1, word_t value2) const
    {
      return (((uword_t)value1) & (1 << ((uword_t)value2))) != 0;
    }
  };

  /// Base class for ALUp instructions.
  class i_alup_t : public i_pred_t
  {
  public:
    /// Compute the result of an ALUp instruction.
    /// @param value1 The value of the first operand.
    /// @param value2 The value of the second operand.
    virtual bit_t compute(word_t value1, word_t value2) const = 0;

    // IF inherited from NOP

    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.DR_Ps1 = s.PRR.get(ops.OPS.ALUp.Ps1).get();
      ops.DR_Ps2 = s.PRR.get(ops.OPS.ALUp.Ps2).get();
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the EX pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      if (ops.DR_Pred)
      {
        // compute the result of the ALU instruction
        bit_t result = compute(ops.DR_Ps1, ops.DR_Ps2);

        // store the result by writing it into the register file.
        s.PRR.set(ops.OPS.ALUp.Pd, result);
      }
    }

    // MW inherited from NOP.
  };

#define ALUp_INSTR(name, lval, operator) \
  class i_ ## name ## _t : public i_alup_t \
  { \
  public:\
    virtual void print(std::ostream &os, const instruction_data_t &ops, \
                       const symbol_map_t &symbols) const \
    { \
      os << boost::format("(p%2%) %1% p%3% = p%4%, p%5%") % #name \
          % ops.Pred % ops.OPS.ALUp.Pd % ops.OPS.ALUp.Ps1 % ops.OPS.ALUp.Ps2; \
    } \
    virtual bit_t compute(word_t value1, word_t value2) const \
    { \
      return lval == ((value1 operator value2) & 0x1); \
    } \
  };

  ALUp_INSTR(por , 1, |)
  ALUp_INSTR(pand, 1, &)
  ALUp_INSTR(pxor, 1, ^)
  ALUp_INSTR(pnor, 0, |)

  /// A multi-cycle NOP operation.
  class i_spcn_t : public i_pred_t
  {
  public:
    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print(std::ostream &os, const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      os << boost::format("(p%1%) nop %2%") % ops.Pred % ops.OPS.SPCn.Imm;
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the IF pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void IF(simulator_t &s, instruction_data_t &ops) const
    {
      i_pred_t::IF(s, ops);
      ops.DR_Imm = 0;
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      bit_t Pred = ops.DR_Pred = s.PRR.get(ops.Pred).get();

      if (Pred)
      {
        if (ops.DR_Imm != ops.OPS.SPCn.Imm)
        {
          // increment NOP cycle counter
          ops.DR_Imm++;

          // stall the pipeline
          s.pipeline_stall(SDR);
        }
      }
    }

    // EX inherited from NOP

    // MW inherited from NOP
  };

  /// Wait for memory operations to complete.
  class i_spcw_t : public i_pred_t
  {
  public:
    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print(std::ostream &os, const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      os << boost::format("(p%1%) waitm") % ops.Pred;
    }

    // IF inherited from NOP

    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      bit_t Pred = s.PRR.get(ops.Pred).get();

      if (Pred)
      {
        if (s.Is_decoupled_load_active)
        {
          // stall the pipeline
          s.pipeline_stall(SDR);
        }
      }
    }

    // EX inherited from NOP

    // MW inherited from NOP
  };

  /// Move a value from a general purpose register to a special purpose
  /// register.
  class i_spct_t : public i_pred_t
  {
  public:
    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print(std::ostream &os, const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      os << boost::format("(p%1%) mts s%2% = r%3%")
         % ops.Pred % ops.OPS.SPCt.Sd % ops.OPS.SPCt.Rs1;
    }

    // IF inherited from NOP

    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.DR_Rs1 = s.GPR.get(ops.OPS.SPCt.Rs1);
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the EX pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      if (ops.DR_Pred)
      {
        uword_t result = read_GPR_EX(s, ops.DR_Rs1);

        // store the result by writing it into the special purpose register file
        // or resetting the predicate registers
        if (ops.OPS.SPCt.Sd == 0)
        {
          // p0 is always 1, so skip it
          for(unsigned int i = 1; i < NUM_PRR; i++)
            s.PRR.set((PRR_e)i, ((result >> i) & 1) == 1);
        }
        else
          s.SPR.set(ops.OPS.SPCt.Sd, result);
      }
    }

    // MW inherited from NOP
  };

  /// Move a value from a special purpose register to a general purpose
  /// register.
  class i_spcf_t : public i_pred_t
  {
  public:
    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print(std::ostream &os, const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      os << boost::format("(p%1%) mfs r%2% = s%3%")
         % ops.Pred % ops.OPS.SPCf.Rd % ops.OPS.SPCf.Ss;
    }

    // IF inherited from NOP

    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      // read a value from the special register file or all predicate registers
      if (ops.OPS.SPCf.Ss == 0)
      {
        ops.DR_Ss = 0;
        for(unsigned int i = 0; i < NUM_PRR; i++)
          ops.DR_Ss |= (s.PRR.get((PRR_e)i).get() << i);
      }
      else
        ops.DR_Ss = s.SPR.get(ops.OPS.SPCf.Ss).get();
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the EX pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      // read the special purpose register, without forwarding
      ops.EX_result = ops.DR_Ss;
    }

    /// Commit function to commit the shadow state of the instruction in
    /// the EX pipeline stage to the global state.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void EX_commit(simulator_t &s, instruction_data_t &ops) const
    {
      if (ops.DR_Pred)
      {
        // store the result by writing it into a by-pass.
        ops.GPR_EX_Rd.set(ops.OPS.SPCf.Rd, ops.EX_result);
      }
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the MW pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void MW(simulator_t &s, instruction_data_t &ops) const
    {
      if (ops.DR_Pred)
      {
        s.GPR.set(ops.GPR_EX_Rd.get());
        ops.GPR_MW_Rd.set(ops.GPR_EX_Rd.get());
        ops.GPR_EX_Rd.reset();
      }
    }

    /// Commit function to commit the shadow state of the instruction in
    /// the MW pipeline stage to the global state.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void MW_commit(simulator_t &s, instruction_data_t &ops) const
    {
      if (ops.DR_Pred)
      {
        ops.GPR_MW_Rd.reset();
      }
    }
  };

  /// Base class for memory load instructions.
  class i_ldt_t : public i_pred_t
  {
  public:
    /// load the value from memory.
    /// @param s The Patmos simulator executing the instruction.
    /// @param address The address of the memory access.
    /// @param value If the function returns true, the loaded value is returned
    /// here.
    /// @return True if the value was loaded, false if the value is not yet
    /// available and stalling is needed.
    virtual bool load(simulator_t &s, word_t address, word_t &value) const = 0;

    // IF inherited from NOP

    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.DR_Rs1 = s.GPR.get(ops.OPS.LDT.Ra);
    }

    // EX implemented by base class

    /// Pipeline function to simulate the behavior of the instruction in
    /// the MW pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void MW(simulator_t &s, instruction_data_t &ops) const
    {
      if (ops.DR_Pred)
      {
        // load from memory
        word_t result;
        bool is_available = load(s, ops.EX_Address, result);

        // the value is already available?
        if (is_available)
        {
          // store the loaded value by writing it into a by-pass
          s.GPR.set(ops.OPS.LDT.Rd, result);
          ops.GPR_MW_Rd.set(ops.OPS.LDT.Rd, result);
        }
        else
        {
          // stall and wait for the memory/cache
          s.pipeline_stall(SMW);
        }
      }
    }

    /// Commit function to commit the shadow state of the instruction in
    /// the MW pipeline stage to the global state.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void MW_commit(simulator_t &s, instruction_data_t &ops) const
    {
      if (ops.DR_Pred)
      {
        ops.GPR_MW_Rd.reset();
      }
    }
  };

#define LD_INSTR(name, base, atype, ctype) \
  class i_ ## name ## _t : public i_ldt_t \
  { \
  public:\
    virtual void print(std::ostream &os, const instruction_data_t &ops, \
                       const symbol_map_t &symbols) const \
    { \
      os << boost::format("(p%2%) %1% r%3% = [r%4% + %5%]") % #name \
          % ops.Pred % ops.OPS.LDT.Rd % ops.OPS.LDT.Ra % ops.OPS.LDT.Imm; \
      symbols.print(os, ops.EX_Address); \
    } \
    virtual void EX(simulator_t &s, instruction_data_t &ops) const \
    { \
      ops.EX_Address = read_GPR_EX(s, ops.DR_Rs1) + ops.OPS.LDT.Imm*sizeof(atype); \
    } \
    virtual bool load(simulator_t &s, word_t address, word_t &value) const \
    { \
      atype tmp=0; \
      if ((address & (sizeof(atype) - 1)) != 0) \
        simulation_exception_t::unaligned(address); \
      bool is_available = base.read_fixed(address, tmp); \
      value = (ctype)from_big_endian<big_ ## atype>(tmp); \
      return is_available; \
    } \
  };

  LD_INSTR(lws , s.Stack_cache, word_t, word_t)
  LD_INSTR(lhs , s.Stack_cache, hword_t, word_t)
  LD_INSTR(lbs , s.Stack_cache, byte_t, word_t)
  LD_INSTR(lwus, s.Stack_cache, uword_t, uword_t)
  LD_INSTR(lhus, s.Stack_cache, uhword_t, uword_t)
  LD_INSTR(lbus, s.Stack_cache, ubyte_t, uword_t)

  LD_INSTR(lwl , s.Local_memory, word_t, word_t)
  LD_INSTR(lhl , s.Local_memory, hword_t, word_t)
  LD_INSTR(lbl , s.Local_memory, byte_t, word_t)
  LD_INSTR(lwul, s.Local_memory, uword_t, uword_t)
  LD_INSTR(lhul, s.Local_memory, uhword_t, uword_t)
  LD_INSTR(lbul, s.Local_memory, ubyte_t, uword_t)

  LD_INSTR(lwc , s.Data_cache, word_t, word_t)
  LD_INSTR(lhc , s.Data_cache, hword_t, word_t)
  LD_INSTR(lbc , s.Data_cache, byte_t, word_t)
  LD_INSTR(lwuc, s.Data_cache, uword_t, uword_t)
  LD_INSTR(lhuc, s.Data_cache, uhword_t, uword_t)
  LD_INSTR(lbuc, s.Data_cache, ubyte_t, uword_t)

  LD_INSTR(lwm , s.Memory, word_t, word_t)
  LD_INSTR(lhm , s.Memory, hword_t, word_t)
  LD_INSTR(lbm , s.Memory, byte_t, word_t)
  LD_INSTR(lwum, s.Memory, uword_t, uword_t)
  LD_INSTR(lhum, s.Memory, uhword_t, uword_t)
  LD_INSTR(lbum, s.Memory, ubyte_t, uword_t)


  /// Base class for memory load instructions.
  class i_dldt_t : public i_pred_t
  {
  public:
    /// load the value from memory.
    /// @param s The Patmos simulator executing the instruction.
    /// @param address The address of the memory access.
    /// @param value If the function returns true, the loaded value is returned
    /// here.
    /// @return True if the value was loaded, false if the value is not yet
    /// available and stalling is needed.
    virtual bool load(simulator_t &s, word_t address, word_t &value) const = 0;

    // IF inherited from NOP

    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.DR_Rs1 = s.GPR.get(ops.OPS.LDT.Ra);

      if (ops.DR_Pred && s.Is_decoupled_load_active)
      {
        // stall the pipeline
        s.pipeline_stall(SDR);
      }
    }

    // EX implemented by base class

    // MW inherited from NOP

    /// Pipeline function to simulate the behavior of a decoupled load
    /// instruction running in parallel to the pipeline.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void dMW(simulator_t &s, instruction_data_t &ops) const
    {
      assert(s.Is_decoupled_load_active);

      // load from memory
      word_t result;
      bool is_available = load(s, ops.EX_Address, result);

      // the value is already available?
      if (is_available)
      {
        // store the loaded value by writing it into a by-pass
        s.SPR.set(sm, result);
        s.Decoupled_load = instruction_data_t();
        s.Is_decoupled_load_active = false;
      }
    }
  };

#define DLD_INSTR(name, base, atype, ctype) \
  class i_ ## name ## _t : public i_dldt_t \
  { \
  public:\
    virtual void print(std::ostream &os, const instruction_data_t &ops, \
                       const symbol_map_t &symbols) const \
    { \
      os << boost::format("(p%2%) %1% sm = [r%3% + %4%]") % #name \
          % ops.Pred % ops.OPS.LDT.Ra % ops.OPS.LDT.Imm; \
    } \
    virtual void EX(simulator_t &s, instruction_data_t &ops) const \
    { \
      ops.EX_Address = read_GPR_EX(s, ops.DR_Rs1) + ops.OPS.LDT.Imm*sizeof(atype); \
      if (ops.DR_Pred) \
      { \
        assert(!s.Is_decoupled_load_active); \
        s.Decoupled_load = ops; \
        s.Is_decoupled_load_active = true; \
      } \
    } \
    virtual bool load(simulator_t &s, word_t address, word_t &value) const \
    { \
      atype tmp; \
      if ((address & (sizeof(atype) - 1)) != 0) \
        simulation_exception_t::unaligned(address); \
      bool is_available = base.read_fixed(address, tmp); \
      value = (ctype)from_big_endian<big_ ## atype>(tmp); \
      return is_available; \
    } \
  };

  DLD_INSTR(dlwc , s.Data_cache, word_t, word_t)
  DLD_INSTR(dlhc , s.Data_cache, hword_t, word_t)
  DLD_INSTR(dlbc , s.Data_cache, byte_t, word_t)
  DLD_INSTR(dlwuc, s.Data_cache, uword_t, uword_t)
  DLD_INSTR(dlhuc, s.Data_cache, uhword_t, uword_t)
  DLD_INSTR(dlbuc, s.Data_cache, ubyte_t, uword_t)

  DLD_INSTR(dlwm , s.Memory, word_t, word_t)
  DLD_INSTR(dlhm , s.Memory, hword_t, word_t)
  DLD_INSTR(dlbm , s.Memory, byte_t, word_t)
  DLD_INSTR(dlwum, s.Memory, uword_t, uword_t)
  DLD_INSTR(dlhum, s.Memory, uhword_t, uword_t)
  DLD_INSTR(dlbum, s.Memory, ubyte_t, uword_t)

  /// Base class for memory store instructions.
  class i_stt_t : public i_pred_t
  {
  public:
    /// Store the value to memory.
    /// @param s The Patmos simulator executing the instruction.
    /// @param address The address of the memory access.
    /// @param value The value to be stored.
    /// @return True when the value was finally written to the memory, false
    /// if the instruction has to stall.
    virtual bool store(simulator_t &s, word_t address, word_t value) const = 0;

    // IF inherited from NOP

    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.DR_Rs1 = s.GPR.get(ops.OPS.STT.Ra);
      ops.DR_Rs2 = s.GPR.get(ops.OPS.STT.Rs1);
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the MW pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void MW(simulator_t &s, instruction_data_t &ops) const
    {
      if (ops.DR_Pred)
      {
        // store to memory
        if (!store(s, ops.EX_Address, ops.EX_Rs))
        {
          // we need to stall in order to ensure that the value was actually
          // propagated down to the memory
          s.pipeline_stall(SMW);
        }
      }
    }
  };

#define ST_INSTR(name, base, type) \
  class i_ ## name ## _t : public i_stt_t \
  { \
  public:\
    virtual void print(std::ostream &os, const instruction_data_t &ops, \
                       const symbol_map_t &symbols) const \
    { \
      os << boost::format("(p%2%) %1% [r%3% + %4%] = r%5%") % #name \
          % ops.Pred % ops.OPS.STT.Ra % ops.OPS.STT.Imm2 % ops.OPS.STT.Rs1; \
      symbols.print(os, ops.EX_Address); \
    } \
    virtual void EX(simulator_t &s, instruction_data_t &ops) const \
    { \
      ops.EX_Address = read_GPR_EX(s, ops.DR_Rs1) + ops.OPS.STT.Imm2*sizeof(type); \
      ops.EX_Rs = read_GPR_EX(s, ops.DR_Rs2); \
    } \
    virtual bool store(simulator_t &s, word_t address, word_t value) const \
    { \
      type big_value = to_big_endian<big_ ## type>((type)value); \
      if ((address & (sizeof(type) - 1)) != 0) \
        simulation_exception_t::unaligned(address); \
      return base.write_fixed(address, big_value); \
    } \
  };

  ST_INSTR(sws, s.Stack_cache, word_t)
  ST_INSTR(shs, s.Stack_cache, hword_t)
  ST_INSTR(sbs, s.Stack_cache, byte_t)

  ST_INSTR(swl, s.Local_memory, word_t)
  ST_INSTR(shl, s.Local_memory, hword_t)
  ST_INSTR(sbl, s.Local_memory, byte_t)

  ST_INSTR(swc, s.Data_cache, word_t)
  ST_INSTR(shc, s.Data_cache, hword_t)
  ST_INSTR(sbc, s.Data_cache, byte_t)

  ST_INSTR(swm, s.Memory, word_t)
  ST_INSTR(shm, s.Memory, hword_t)
  ST_INSTR(sbm, s.Memory, byte_t)


#define STC_INSTR(name, function) \
  class i_ ## name ## _t : public i_pred_t \
  { \
  public:\
    virtual void print(std::ostream &os, const instruction_data_t &ops, \
                       const symbol_map_t &symbols) const \
    { \
      os << boost::format("(p%2%) %1% %3%") % #name % ops.Pred % ops.OPS.STC.Imm; \
      symbols.print(os, ops.EX_Address); \
    } \
    virtual void DR(simulator_t &s, instruction_data_t &ops) const \
    { \
      ops.DR_Pred = s.PRR.get(ops.Pred).get(); \
      ops.DR_Ss = s.SPR.get(st).get(); \
    } \
    virtual void MW(simulator_t &s, instruction_data_t &ops) const \
    { \
      uword_t stack_top = ops.DR_Ss; \
      if(ops.DR_Pred && !s.Stack_cache.function(ops.OPS.STC.Imm * \
                                                NUM_STACK_CACHE_BLOCK_BYTES, \
                                                stack_top)) \
      { \
        s.pipeline_stall(SMW); \
      } \
      s.SPR.set(st, stack_top); \
    } \
  };

  STC_INSTR(sres, reserve)
  STC_INSTR(sens, ensure)
  STC_INSTR(sfree, free)

  /// Base class for branch, call, and return instructions.
  class i_pfl_t : public i_pred_t
  {
  protected:
    /// Store the method base address and offset to the respective special
    /// purpose registers.
    /// @param s The Patmos simulator executing the instruction.
    /// @param pred The predicate under which the instruction is executed.
    /// @param base The base address of the current method.
    /// @param pc The current program counter.
    void no_store_return_address(simulator_t &s, instruction_data_t &ops,
                                 bit_t pred, uword_t base, uword_t pc) const
    {
      assert(base <= pc);
    }

    /// Store the method base address and offset to the respective special
    /// purpose registers.
    /// @param s The Patmos simulator executing the instruction.
    /// @param pred The predicate under which the instruction is executed.
    /// @param base The base address of the current method.
    /// @param pc The current program counter.
    void store_return_address(simulator_t &s, instruction_data_t &ops,
                              bit_t pred, uword_t base, uword_t pc) const
    {
      if (pred && !ops.EX_PFL_Discard)
      {
        assert(base <= pc);

        // store the return address and method base address by writing them into
        // general purpose registers
        s.GPR.set(rfb, base);
        s.GPR.set(rfo, pc - base);
      }
    }

    /// Perform a function branch/call/return.
    /// Fetch the function into the method cache, stall the pipeline, and set
    /// the program counter.
    /// @param s The Patmos simulator executing the instruction.
    /// @param pred The predicate under which the instruction is executed.
    /// @param base The base address of the target method.
    /// @param address The target address.
    void fetch_and_dispatch(simulator_t &s, instruction_data_t &ops,
                            bit_t pred, word_t base, word_t address) const
    {
      if (pred && !ops.EX_PFL_Discard)
      {
        // check if the target method is in the cache, otherwise stall until
        // it is loaded.
        if (!s.Method_cache.is_available(base))
        {
          // stall the pipeline
          s.pipeline_stall(SEX);
        }
        else
        {
          // set the program counter and base
          s.BASE = base;
          s.PC = s.nPC = address;
          ops.EX_PFL_Discard = 1;
        }
      }
    }

    /// Perform a function branch or call.
    /// The function is assumed to be in the method cache, thus simply set the
    /// program counter.
    /// @param s The Patmos simulator executing the instruction.
    /// @param pred The predicate under which the instruction is executed.
    /// @param base The base address of the target method.
    /// @param address The target address.
    void dispatch(simulator_t &s, instruction_data_t &ops, bit_t pred,
                  word_t base, word_t address) const
    {
      if (pred && !ops.EX_PFL_Discard)
      {
        // assure that the target method is in the cache.
        assert(s.Method_cache.assert_availability(base));

        // set the program counter and base
        s.BASE = base;
        s.PC = s.nPC = address;
        ops.EX_PFL_Discard = 1;
      }
    }
  public:
    /// Pipeline function to simulate the behavior of the instruction in
    /// the IF pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void IF(simulator_t &s, instruction_data_t &ops) const
    {
      // store the PC for PC-relative addressing in EX stage, if we are not stalling
      // NB: s.Stall is already set the first time s.PC is updated.
      //     If we checked (s.Stall==SIF), we would have to include i_pred_t::IF,
      //     which is rather ugly.
      if (s.PC != s.nPC) {
        ops.IF_PC = s.PC;
      }
      // call the inherited function (advance PC)
      i_pred_t::IF(s, ops);
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.EX_PFL_Discard = 0;
    }

    // EX implemented by sub-classes

    // MW inherited from NOP
  };

#define PFLB_INSTR(name, store, dispatch, new_base, target) \
  class i_ ## name ## _t : public i_pfl_t \
  { \
  public:\
    virtual void print(std::ostream &os, const instruction_data_t &ops, \
                       const symbol_map_t &symbols) const \
    { \
      os << boost::format("(p%2%) %1% %3%") % #name % ops.Pred % ops.OPS.PFLb.Imm; \
      symbols.print(os, ops.EX_Address); \
    } \
    virtual void EX(simulator_t &s, instruction_data_t &ops) const \
    { \
      ops.EX_Address = target; \
      store(s, ops, ops.DR_Pred, s.BASE, s.nPC); \
      dispatch(s, ops, ops.DR_Pred, new_base, target); \
    } \
  };

  PFLB_INSTR(call, store_return_address, fetch_and_dispatch,
             ops.OPS.PFLb.Imm*sizeof(word_t),
             ops.OPS.PFLb.Imm*sizeof(word_t))
  PFLB_INSTR(b, no_store_return_address, dispatch,
             s.BASE,
             ops.IF_PC + ops.OPS.PFLb.Imm*sizeof(word_t))

  /// Branch and call instructions with a register operand.
  class i_pfli_t : public i_pfl_t
  {
  public:
    // IF inherited from i_pfl_t

    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.DR_Rs1 = s.GPR.get(ops.OPS.PFLi.Rs);
      ops.EX_PFL_Discard = 0;
    }

    // EX implemented by sub-classes

    // MW inherited from NOP
  };

#define PFLI_INSTR(name, store, dispatch, new_base, target) \
  class i_ ## name ## _t : public i_pfli_t \
  { \
  public:\
    virtual void print(std::ostream &os, const instruction_data_t &ops, \
                       const symbol_map_t &symbols) const \
    { \
      os << boost::format("(p%2%) %1% r%3%") % #name % ops.Pred % ops.OPS.PFLi.Rs; \
      symbols.print(os, ops.EX_Address); \
    } \
    virtual void EX(simulator_t &s, instruction_data_t &ops) const \
    { \
      ops.EX_Address = target; \
      store(s, ops, ops.DR_Pred, s.BASE, s.nPC); \
      dispatch(s, ops, ops.DR_Pred, new_base, target); \
    } \
  };

  PFLI_INSTR(callr, store_return_address, fetch_and_dispatch,
             read_GPR_EX(s, ops.DR_Rs1),
             read_GPR_EX(s, ops.DR_Rs1))
  PFLI_INSTR(br, no_store_return_address, dispatch,
             s.BASE,
             ops.IF_PC + read_GPR_EX(s, ops.DR_Rs1))

  /// An instruction for returning from function calls.
  class i_ret_t : public i_pfl_t
  {
  public:
    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print(std::ostream &os, const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      os << boost::format("(p%1%) ret %2%, %3%") % ops.Pred
         % ops.OPS.PFLr.Rb % ops.OPS.PFLr.Ro;
    }

    // IF inherited from NOP

    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.DR_Base   = s.GPR.get(ops.OPS.PFLr.Rb).get();
      ops.DR_Offset = s.GPR.get(ops.OPS.PFLr.Ro).get();
      ops.EX_PFL_Discard = 0;
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the EX pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      // returning to address 0? interpret this as a halt.
      if (ops.DR_Pred && ops.DR_Base == 0)
      {
        // stall the first stage of the pipeline
        s.pipeline_stall(SDR);
      }
      else
        fetch_and_dispatch(s, ops, ops.DR_Pred, ops.DR_Base,
                           ops.DR_Base + ops.DR_Offset);
    }

    /// Signal to the simulator that a "halt" instruction has been executed.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void MW_commit(simulator_t &s, instruction_data_t &ops) const
    {
      // returning to address 0? interpret this as a halt.
      if (ops.DR_Pred && ops.DR_Base == 0)
        simulation_exception_t::halt(s.GPR.get(GPR_EXIT_CODE_INDEX).get());
    }
  };

  /// A (temporary) instruction for pc-relative, conditional branches.
  /// Used only for hardware development.
  class i_bne_t : public i_pfl_t
  {
  public:
    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print(std::ostream &os, const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      os << boost::format("bne %1% != %2%, %3%") % ops.OPS.BNE.Rs1
         % ops.OPS.BNE.Rs2 % ops.OPS.BNE.Imm;
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = 1;
      ops.DR_Rs1 = s.GPR.get(ops.OPS.BNE.Rs1);
      ops.DR_Rs2 = s.GPR.get(ops.OPS.BNE.Rs2);
      ops.EX_PFL_Discard = 0;
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the EX pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      // compute the result of the ALU instruction
      bit_t pred = read_GPR_EX(s, ops.DR_Rs1) != read_GPR_EX(s, ops.DR_Rs2);
      dispatch(s, ops, pred, s.BASE, s.PC + ops.OPS.BNE.Imm*sizeof(word_t));
    }
  };
}

#endif // PATMOS_INSTRUCTIONS_H


