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
// Core simulation loop of the Patmos Simulator.
//

#include "simulation-core.h"
#include "data-cache.h"
#include "instruction.h"
#include "memory.h"
#include "method-cache.h"
#include "stack-cache.h"
#include "symbol.h"
#include "interrupts.h"
#include "instructions.h"

#include <ios>
#include <iostream>
#include <iomanip>

namespace patmos
{
  simulator_t::simulator_t(memory_t &memory, memory_t &local_memory,
                           data_cache_t &data_cache,
                           method_cache_t &method_cache,
                           stack_cache_t &stack_cache, symbol_map_t &symbols,
                           rtc_t &rtc, interrupt_handler_t &interrupt_handler)
    : Dbg_cnt_delay(0),
      Cycle(0), Memory(memory), Local_memory(local_memory),
      Data_cache(data_cache), Method_cache(method_cache),
      Stack_cache(stack_cache), Symbols(symbols), Dbg_stack(*this),
      Rtc(rtc), Interrupt_handler(interrupt_handler),
      BASE(0), PC(0), nPC(0),
      Stall(SIF), Is_decoupled_load_active(false)
  {
    // initialize one predicate register to be true, otherwise no instruction
    // will ever execute
    PRR.set(p0, true);
    // initialize negated predicates
    for (unsigned int p = pn1; p < NUM_PRRn; p++) {
	PRR.set((PRR_e)p, true);
    }

    // initialize the pipeline
    for(unsigned int i = 0; i < NUM_STAGES; i++)
    {
      Num_stall_cycles[i] = 0;
      for(unsigned int j = 0; j < NUM_SLOTS; j++)
      {
        Pipeline[i][j] = instruction_data_t();
      }
    }

    // Initialize instruction statistics
    for(unsigned int j = 0; j < NUM_SLOTS; j++)
    {
      Instruction_stats[j].resize(Decoder.get_num_instructions());

      Num_bubbles_retired[j] = 0;
    }
  }


  void simulator_t::pipeline_invoke(Pipeline_t pst,
                                   void (instruction_data_t::*f)(simulator_t &),
                                   bool debug, std::ostream &debug_out)
  {
    if (debug)
    {
      debug_out << boost::format("%1% : ") % pst;
    }

    // invoke simulation functions
    for(unsigned int i = 0; i < NUM_SLOTS; i++)
    {
      // debug output
      if (debug)
      {
        if (i != 0)
        {
          debug_out << " || ";
        }
        Pipeline[pst][i].print(debug_out, Symbols);
        debug_out.flush();
      }

      // simulate the respective pipeline stage of the instruction
      (Pipeline[pst][i].*f)(*this);
    }

    if (debug)
    {
      debug_out << "\n";
    }
  }

  void simulator_t::pipeline_stall(Pipeline_t pst)
  {
    Stall = std::max(Stall, pst);
  }


  void simulator_t::run(word_t entry, uint64_t debug_cycle,
                        debug_format_e debug_fmt, std::ostream &debug_out,
                        uint64_t max_cycles,
                        bool collect_instr_stats)
  {

    int branch_counter      = 0;
    int interrupt_handling  = 0;
    instruction_t *intr     = new i_intr_t();
    intr->ID                = patmos::decoder_t::get_num_instructions();
    intr->Name              = "intr";

    // do some initializations before executing the first instruction.
    if (Cycle == 0)
    {
      BASE = nPC = entry;
      Method_cache.initialize(entry);
      Profiling.initialize(entry);
      Dbg_stack.initialize(entry);
    }

    try
    {
      // start of main simulation loop
      for(uint64_t cycle = 0; cycle < max_cycles; cycle++, Cycle++)
      {
        bool debug = (Cycle >= debug_cycle);
        bool debug_pipline = debug && (debug_fmt >= DF_LONG);

        // simulate decoupled load
        Decoupled_load.dMW(*this);

        if (debug_pipline)
        {
          debug_out << "dMW: ";
          Decoupled_load.print(debug_out, Symbols);
          debug_out << "\n";
        }

        // invoke simulation functions
        pipeline_invoke(SMW, &instruction_data_t::MW, debug_pipline);
        pipeline_invoke(SEX, &instruction_data_t::EX, debug_pipline);
        pipeline_invoke(SDR, &instruction_data_t::DR, debug_pipline);

        // print instructions in EX stage
        if (debug && debug_fmt == DF_INSTRUCTIONS)
        {
          print_instructions(debug_out, SEX);
        }

        Rtc.tick();

        // track instructions retired
        if (Stall != NUM_STAGES-1)
        {
          for(unsigned int j = 0; j < NUM_SLOTS; j++)
          {
              if (Pipeline[NUM_STAGES-1][j].I)
              {
                // get instruction statistics
                instruction_stat_t &stat(
                        Instruction_stats[j][Pipeline[NUM_STAGES-1][j].I->ID]);

                // update instruction statistics
                if (Pipeline[NUM_STAGES-1][j].DR_Pred)
                  stat.Num_retired++;
                else
                  stat.Num_discarded++;
              }
              else
                Num_bubbles_retired[j]++;
          }
        }

        // track pipeline stalls
        Num_stall_cycles[Stall]++;

        // move pipeline stages
        for (int i = SEX; i >= Stall; i--)
        {
          for (unsigned int j = 0; j < NUM_SLOTS; j++)
          {
            Pipeline[i + 1][j] = Pipeline[i][j];
          }
        }

        // insert bubbles after stalling stage.
        // Note that it is not possible to stall in IF, this is interpreted as
        // 'not stalling'.
        if (Stall > SIF && Stall != NUM_STAGES- 1)
        {
          for(unsigned int i = 0; i < NUM_SLOTS; i++)
          {
            Pipeline[Stall + 1][i] = instruction_data_t();
          }
        }


        // if we are stalling in MW, reset the bypass in EX so that it can be
        // filled by the stalled EX stage again (needs to be done for all
        // stalled bypasses).
        if (Stall > SEX) {
          for(unsigned int i = 0; i < NUM_SLOTS; i++)
          {
            Pipeline[SEX][i].GPR_EX_Rd.reset();
          }
        }

        // update the Program counter
        PC = nPC;

        unsigned int iw_size;
        word_t iw[2];

        // decode the next instruction, only if we are not stalling.
        if (Stall == SIF)
        {

          // decode the instruction word.
          if (Interrupt_handler.interrupt_pending() && 
              branch_counter == 0 && 
              interrupt_handling == 0) 
          { 
            interrupt_t &interrupt = Interrupt_handler.get_interrupt();

            Pipeline[SIF][0] = instruction_data_t::mk_CFLb(*intr, p0, interrupt.Address, interrupt.Address);
            for(unsigned int i = 1; i < NUM_SLOTS; i++)
            {
              Pipeline[SIF][i] = instruction_data_t();
            }

            // Handling interrupt, next CPU cycle no new instructions have to be decoded
            interrupt_handling = 3;

            // Store return from interrupt address
            SPR.set(s9, PC);
          }
          else if (interrupt_handling > 0)
          {
            // Putting more empty instrutions after an interrupt
            for(unsigned int i = 0; i < NUM_SLOTS; i++)
            {
              Pipeline[SIF][i] = instruction_data_t();
            }
            interrupt_handling--;

          }
          else
          {

            // fetch the instruction word from the method cache.
            Method_cache.fetch(PC, iw);
            iw_size = Decoder.decode(iw, Pipeline[0]);

            // provide next program counter value
            if(Pipeline[SIF][0].I->is_flow_control())
                branch_counter = 2;
            else if (branch_counter)
              branch_counter--;

            nPC = PC + iw_size*4;
          }

          // unknown instruction
          if (iw_size == 0)
          {
            simulation_exception_t::illegal(from_big_endian<big_word_t>(iw[0]));
          }
          else
          {
            // track instructions fetched
            for(unsigned int j = 0; j < NUM_SLOTS; j++)
            {
              if (Pipeline[SIF][j].I)
                Instruction_stats[j][Pipeline[SIF][j].I->ID].Num_fetched++;
            }
          }

          // assign fetch address to new instructions
          for(unsigned int i = 0; i < NUM_SLOTS; i++)
          {
            Pipeline[SIF][i].Address = PC;
          }

        }

        // reset the stall counter.
        Stall = SIF;

        // advance the time for the method cache, stack cache, and memory
        Memory.tick();
        Method_cache.tick();
        Stack_cache.tick();

        if (debug)
        {
          print(debug_out, debug_fmt);
        }

        // Collect stats for instructions
        if (collect_instr_stats) {
          for (unsigned int j = 0; j < NUM_SLOTS; j++)
          {
            if (Pipeline[SMW][j].I) {
              // I am too lazy now to remove all the const's..
              instruction_t &I(Decoder.get_instruction(Pipeline[SMW][j].I->ID));
              I.collect_stats(*this, Pipeline[SMW][j]);
            }
          }
        }
      } // end of simulation loop
    }
    catch (simulation_exception_t e)
    {
      delete intr;

      Profiling.finalize(Cycle);

      // pass on to caller
      throw simulation_exception_t(e.get_kind(), e.get_info(), PC, Cycle);
    }

    delete intr;

    Profiling.finalize(Cycle);
  }

  void simulator_t::print_registers(std::ostream &os,
                                    debug_format_e debug_fmt) const
  {
    if (debug_fmt == DF_SHORT)
    {
      for(unsigned int r = r0; r < NUM_GPR; r++)
      {
        os << boost::format(" r%1$-2d: %2$08x") % r % GPR.get((GPR_e)r).get();
      }
      os << "\n";
    }
    else
    {
      os << boost::format("\nCyc : %1%\n PRR: ") % Cycle;

      // print values of predicate registers
      unsigned int sz_value = 0;
      for(int p = NUM_PRR - 1; p >= 0; p--)
      {
        bit_t pred_value = PRR.get((PRR_e)p).get();
        sz_value |= pred_value << p;
        os << pred_value;
      }

      os << boost::format("  BASE: %1$08x   PC : %2$08x   ")
        % BASE % PC;

      Symbols.print(os, PC);

      os << "\n ";

      // print values of general purpose registers
      for(unsigned int r = r0; r < NUM_GPR; r++)
      {
        os << boost::format("r%1$-2d: %2$08x") % r % GPR.get((GPR_e)r).get();

        if ((r & 0x7) == 7)
        {
          os << "\n ";
        }
        else
        {
          os << "   ";
        }
      }
      os << "\n ";

      // print values of special purpose registers -- special handling of SZ.
      os << boost::format("s0 : %1$08x   ") % sz_value;
      for(unsigned int s = s1; s < NUM_SPR; s++)
      {
        os << boost::format("s%1$-2d: %2$08x") % s % SPR.get((SPR_e)s).get();

        if ((s & 0x7) == 7)
        {
          os << "\n ";
        }
        else
        {
          os << "   ";
        }
      }
      os << "\n";
    }
  }

  void simulator_t::print_instructions(std::ostream &os, Pipeline_t stage) const {
      std::ostringstream oss;
      symbol_map_t emptymap;
      for(unsigned int i = 0; i < NUM_SLOTS; i++) {
        std::ostringstream instr;
        Pipeline[stage][i].print(instr, emptymap);
        
        if (i != 0) oss << " || ";
        oss << boost::format("%1% %|30t|") % instr.str();
      }

      os << boost::format("%1$08x %2$9d %3% %|75t|") 
                   % Pipeline[stage][0].Address % Cycle % oss.str();

      for(unsigned int i = 0; i < NUM_SLOTS; i++) {
        if (i != 0) os << " || ";
        Pipeline[stage][i].print_operands(*this, os, Symbols);
      }

      os << "\n";      
  }
  
  /// Read a GPR register at the EX stage.
  /// @param s The parent simulator.
  /// @param op The register operand.
  /// @return The register value, considering by-passing from the EX, and MW
  /// stages.
  static inline word_t read_GPR_post_EX(const simulator_t &s, GPR_e reg)
  {
    // Note: we are between cycles, so the EX bypass has been updated, 
    // the EX instructions have been moved to MW, but the MW bypass is not yet set.
    return s.Pipeline[SMW][1].GPR_EX_Rd.get(
           s.Pipeline[SMW][0].GPR_EX_Rd.get(
              s.GPR.get(reg))).get();
  }
  
  void simulator_t::print(std::ostream &os, debug_format_e debug_fmt)
  {
    if (debug_fmt == DF_TRACE)
    {
      // Boost::format is unacceptably slow (adpcm.elf):
      //  => no debugging output:  1.6s
      //  => os with custom formatting: 2.4s
      //  => boost::format: 10.6s !!
      os << std::hex << std::setw(8) << std::setfill('0') << PC << ' '
         << std::dec << Cycle << '\n' << std::setfill(' ');
      // os << boost::format("%1$08x %2%\n") % PC % Cycle;
      return;
    }
    else if (debug_fmt == DF_INSTRUCTIONS) {
      // already done before
      return;
    }
    else if (debug_fmt == DF_BLOCKS) {
      if (Symbols.contains(PC)) {
	os << boost::format("%1$08x %2$9d ") % PC % Cycle;
	Symbols.print(os, PC);
	os << "\n";
      }
      return;
    }
    else if (debug_fmt == DF_CALLS) {
      if (Dbg_cnt_delay == 1) {

        if (Dbg_is_call) {
          os << " args: " << boost::format("r3 = %1$08x, r4 = %2$08x, ") 
                % read_GPR_post_EX(*this, r3) % read_GPR_post_EX(*this, r4);
          os << boost::format("r5 = %1$08x, r6 = %2$08x, r7 = %3$08x, r8 = %4$08x") 
                % read_GPR_post_EX(*this, r5) % read_GPR_post_EX(*this, r6)
                % read_GPR_post_EX(*this, r7) % read_GPR_post_EX(*this, r8);
        } else {
          os << " retval: " << boost::format("r1 = %1$08x, r2 = %2$08x") 
                % read_GPR_post_EX(*this, r1) % read_GPR_post_EX(*this, r2);
        }
        os << "\n";
        Dbg_cnt_delay = 0;
      } 
      else if (Dbg_cnt_delay > 1) {
        Dbg_cnt_delay--;
      }
      else if (Pipeline[SMW][0].I && Pipeline[SMW][0].DR_Pred &&
               Pipeline[SMW][0].I->is_flow_control()) {
        std::string name = Pipeline[SMW][0].I->Name;
        Dbg_cnt_delay = 0;
        if (name == "ret") {
          Dbg_cnt_delay = 3;
          Dbg_is_call = false;
        }
        else if (name == "call" || name == "callr") {
          Dbg_cnt_delay = 3;
          Dbg_is_call = true;
        }
        if (Dbg_cnt_delay) {
          os << boost::format("%1$08x %2$9d ") % PC % Cycle;
          os << (Dbg_is_call ? "call from " : "return from ");
          Symbols.print(os, Pipeline[SMW][0].Address, true);
          os << " to ";
          Symbols.print(os, Pipeline[SMW][0].EX_Address, true);
        }
      }
    }
    else
    {
      // print register values
      print_registers(os, debug_fmt);

      if (debug_fmt == DF_ALL)
      {
        // print state of method cache
        os << "Method Cache:\n";
        Method_cache.print(os);

        // print state of data cache
        os << "Data Cache:\n";
        Data_cache.print(os);

        // print state of stack cache
        os << "Stack Cache:\n";
        Stack_cache.print(os);

        // print state of main memory
        os << "Memory:\n";
        Memory.print(os);

        os << "\n";
      }
    }
  }

  void simulator_t::print_stats(std::ostream &os, bool slot_stats, bool instr_stats) const
  {
    // print register values
    print_registers(os, DF_DEFAULT);

    unsigned int num_total_fetched[NUM_SLOTS];
    unsigned int num_total_retired[NUM_SLOTS];
    unsigned int num_total_discarded[NUM_SLOTS];
    unsigned int num_total_bubbles[NUM_SLOTS];
    
    os << boost::format("\n\nInstruction Statistics:\n   %1$15s:") % "instruction";
    for (unsigned int i = 0; i < NUM_SLOTS; i++) {
      num_total_fetched[i] = num_total_retired[i] = num_total_discarded[i] = 0;
      num_total_bubbles[i] = 0;
      num_total_bubbles[slot_stats ? i : 0] += Num_bubbles_retired[i];
    
      if (!i || slot_stats) {
        os << boost::format(" %1$10s %2$10s %3$10s")
           % "#fetched" % "#retired" % "#discarded";
      }
    }
    os << "\n";
          
    for(unsigned int i = 0; i < Instruction_stats[0].size(); i++)
    {
      // get instruction and statistics on it
      const instruction_t &I(Decoder.get_instruction(i));

      os << boost::format("   %1$15s:") % I.Name;
      
      unsigned int num_fetched[NUM_SLOTS];
      unsigned int num_retired[NUM_SLOTS];
      unsigned int num_discarded[NUM_SLOTS];
      
      for (unsigned int j = 0; j < NUM_SLOTS; j++) {
        const instruction_stat_t &S(Instruction_stats[j][i]);

        num_fetched[j] = num_retired[j] = num_discarded[j] = 0;
        
        num_fetched[slot_stats ? j : 0] += S.Num_fetched;
        num_retired[slot_stats ? j : 0] += S.Num_retired;
        num_discarded[slot_stats ? j : 0] += S.Num_discarded;

        assert(S.Num_fetched >= (S.Num_retired + S.Num_discarded));
      }
      
      for (unsigned int j = 0; j < NUM_SLOTS; j++) {
        os << boost::format(" %1$10d %2$10d %3$10d")
          % num_fetched[j] % num_retired[j] % num_discarded[j];

        // collect summary
        num_total_fetched[j] += num_fetched[j];
        num_total_retired[j] += num_retired[j];
        num_total_discarded[j] += num_discarded[j];
        
        if (!slot_stats) break;
      }
      
      if (instr_stats) {
        os << "\t";
        I.print_stats(*this, os, Symbols);
      }
      
      os << "\n";
    }

    // summary over all instructions
    os << boost::format("   %1$15s:") % "all";
    for (unsigned int j = 0; j < NUM_SLOTS; j++) {
      os << boost::format(" %1$10d %2$10d %3$10d")
          % num_total_fetched[j] % num_total_retired[j] % num_total_discarded[j];
      if (!slot_stats) break;
    }          
    os << "\n";
    os << boost::format("   %1$15s:") % "bubbles";
    for (unsigned int j = 0; j < NUM_SLOTS; j++) {
      os << boost::format(" %1$10s %2$10d %3$10s") % "-" % num_total_bubbles[j] % "-";
      if (!slot_stats) break;
    }          
    os << "\n";

    // Cycle statistics
    os << "\nStall Cycles:\n";
    for (int i = SDR; i < NUM_STAGES; i++)
    {
      os << boost::format("   %1%: %2%\n")
         % (Pipeline_t)i % Num_stall_cycles[i];
    }
    // print statistics of method cache
    Method_cache.print_stats(os, Symbols);

    // print statistics of data cache
    Data_cache.print_stats(os);

    // print statistics of stack cache
    Stack_cache.print_stats(os);

    // print statistics of main memory
    Memory.print_stats(os);

    // print profiling information
    Profiling.print(os, Symbols);

    os << "\n";
  }


  std::ostream &operator<<(std::ostream &os, Pipeline_t p)
  {
    const static char* names[NUM_STAGES] = {"IF", "DR", "EX", "MW"};
    assert(names[p] != NULL);

    os << names[p];

    return os;
  }
}
