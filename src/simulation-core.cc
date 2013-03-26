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

#include <iostream>

namespace patmos
{
  simulator_t::simulator_t(memory_t &memory, memory_t &local_memory,
                           data_cache_t &data_cache,
                           method_cache_t &method_cache,
                           stack_cache_t &stack_cache, symbol_map_t &symbols) :
      Cycle(0), Memory(memory), Local_memory(local_memory),
      Data_cache(data_cache), Method_cache(method_cache),
      Stack_cache(stack_cache), Symbols(symbols), BASE(0), PC(0), nPC(0),
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
  
  void simulator_t::update_dbg_stack_state(dbg_stack_frame_t &frame, 
                                           word_t base, word_t pc) const
  {
    frame.Curr_base = base;
    frame.Curr_offset = pc - base;
    // TODO Does not work that way..
    //frame.GPR = GPR;
  }
  
  void simulator_t::print_stackframe(std::ostream &os, unsigned depth, 
                                     const dbg_stack_frame_t &frame,
                                     const dbg_stack_frame_t *callframe) const
  {
    os << boost::format("#%d 0x%x ") % depth % frame.Function;
    Symbols.print(os, frame.Function, true);
    
    // TODO print registers r3-r8 from callframe, if available, as well as varargs
    os << "()\n";
    
    os << boost::format("   at 0x%x (base: 0x%x ")  
                     % (frame.Curr_base + frame.Curr_offset) % frame.Curr_base;
    Symbols.print(os, frame.Curr_base);
    os << boost::format(", offset: 0x%x ") % frame.Curr_offset;
    Symbols.print(os, frame.Curr_base + frame.Curr_offset);
    os << ")\n";
    
    // TODO print frame state (if debug/verbose is enabled?)    
  }
  
  void simulator_t::push_dbg_stackframe(word_t target) 
  {
    if (!Dbg_stack.empty()) {
      update_dbg_stack_state(*Dbg_stack.back(), BASE, nPC);
    }
    
    // Create a new stack frame and initialize it
    dbg_stack_frame_t *Frame = new dbg_stack_frame_t(target);
    
    Dbg_stack.push_back(Frame);    
  }

  void simulator_t::pop_dbg_stackframe(word_t return_base, word_t return_offset) 
  {
    if (Dbg_stack.empty()) return;
    
    if (Dbg_stack.size() == 1) {
      // If there is only one stack frame, just pop it
      delete Dbg_stack.back();
      Dbg_stack.pop_back();
      return;
    }
    
    // Check if we are truly returning, otherwise do not pop (if this is a longjmp)
    dbg_stack_frame_t *CallFrame = *(Dbg_stack.end() - 2);
    if (CallFrame->Curr_base == return_base && 
        CallFrame->Curr_offset == return_offset) 
    {
      delete Dbg_stack.back();
      Dbg_stack.pop_back();
    }
  }
  
  void simulator_t::run(word_t entry, uint64_t debug_cycle,
                        debug_format_e debug_fmt, std::ostream &debug_out,
                        uint64_t max_cycles, bool profiling)
  {
    // do some initializations before executing the first instruction.
    if (Cycle == 0)
    {
      BASE = PC = entry;
      Method_cache.initialize(entry);

      if (profiling)
        Profiling.initialize(entry);
      
      push_dbg_stackframe(entry);
    }

    try
    {
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
        pipeline_invoke(SIF, &instruction_data_t::IF, debug_pipline);

        // commit results
        pipeline_invoke(SMW, &instruction_data_t::MW_commit);
        pipeline_invoke(SEX, &instruction_data_t::EX_commit);
        pipeline_invoke(SDR, &instruction_data_t::DR_commit);
        pipeline_invoke(SIF, &instruction_data_t::IF_commit);

        // write cycle counter-value into special registers
        SPR.set(scl, Cycle & 0xffffffff);
        SPR.set(sch, Cycle >> 32);

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

        if (debug && debug_fmt == DF_INSTRUCTIONS)
        {
          print_instructions(debug_out, SMW);
        }
        
        // move pipeline stages
        for (int i = SEX; i >= Stall; i--)
        {
          for (unsigned int j = 0; j < NUM_SLOTS; j++)
          {
            Pipeline[i + 1][j] = Pipeline[i][j];
          }
        }

        // prevent forwarding
        if (Stall > SEX)
        {
          for(unsigned int i = 0; i < NUM_SLOTS; i++)
          {
            Pipeline[SEX][i].GPR_EX_Rd.reset();
          }
        }

        // decode the next instruction, only if we are not stalling.
        if (Stall == SIF)
        {
          unsigned int iw_size;

          // fetch the instruction word from the method cache.
          word_t iw[2];
          Method_cache.fetch(PC, iw);

          // decode the instruction word.
          iw_size = Decoder.decode(iw,  Pipeline[0]);

          // provide next program counter value
          nPC = PC + iw_size*4;

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
              if (Pipeline[0][j].I)
                Instruction_stats[j][Pipeline[0][j].I->ID].Num_fetched++;
            }
         }
        }
        else if (Stall != NUM_STAGES- 1)
        {
          for(unsigned int i = 0; i < NUM_SLOTS; i++)
          {
            Pipeline[Stall + 1][i] = instruction_data_t();
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


        if (profiling)
        {
          instruction_data_t *Inst = &Pipeline[NUM_STAGES-1][0];

          // check for executed call/return
          if (Inst && Inst->DR_Pred)
          {
            const char *name = Inst->I->Name;

            if (strncmp(name, "call", 4) == 0)
              Profiling.enter(PC, Cycle);

            if (strncmp(name, "ret", 3) == 0)
              Profiling.leave(Cycle);
          }
        }

      }
    }
    catch (simulation_exception_t e)
    {
      if (profiling)
        Profiling.finalize(Cycle);

      // pass on to caller
      throw simulation_exception_t(e.get_kind(), e.get_info(), PC, Cycle);
    }

    if (profiling)
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
        if (i != 0) oss << " || ";
        Pipeline[stage][i].print(oss, emptymap);
      }

      os << boost::format("%1$08x %2$9d %3% %|70t|") % PC % Cycle % oss.str();

      for(unsigned int i = 0; i < NUM_SLOTS; i++) {
        if (i != 0) os << " ";
        Pipeline[stage][i].printOperands(*this, os, Symbols);
      }

      os << "\n";      
  }
  
  void simulator_t::print(std::ostream &os, debug_format_e debug_fmt) const
  {
    if (debug_fmt == DF_TRACE)
    {
      os << boost::format("%1$08x %2%\n") % PC % Cycle;
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
    else if (debug_fmt == DF_TRACE_STACK)
    {
      Stack_cache.trace(os, Cycle);
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

  void simulator_t::print_stats(std::ostream &os, bool slot_stats) const
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
    if (!Profiling.empty())
    {
      Profiling.print(os, Symbols);
    }

    os << "\n";
  }

  void simulator_t::print_stacktrace(std::ostream &os) const {
    // TODO Obviously, it would be nicer to print the actual stack
    // frame, but since we have up to three different stacks and no
    // easy way to determine the stack frame size, it is much easier 
    // to just keep track of the stack frames separately. This has the
    // advantage that we can store more debug info in the frames.
    // This could be removed when we get GDB support into the simulator ;)
    
    os << "Stacktrace:\n";
    
    if (Dbg_stack.empty()) {
      dbg_stack_frame_t Frame(BASE);
      update_dbg_stack_state(Frame, BASE, PC);
      print_stackframe(os, 0, Frame, 0);
      return;
    }
    
    dbg_stack_frame_t *CallFrame = Dbg_stack.size() > 1 ? *(Dbg_stack.end() - 2) : 0;
    
    // Need to update the active stack frame with the current state
    // Make a copy and update it.
    dbg_stack_frame_t TopFrame = *Dbg_stack.back();
    update_dbg_stack_state(TopFrame, BASE, PC);
    print_stackframe(os, 0, TopFrame, CallFrame);
    
    for (int i = Dbg_stack.size() - 2; i >= 0; i--) {
      dbg_stack_frame_t *Frame = *(Dbg_stack.begin() + i);
      CallFrame = i > 0 ? *(Dbg_stack.begin() + i - 1) : 0;
      print_stackframe(os, Dbg_stack.size() - i - 1, *Frame, CallFrame);
    }
  }
  
  std::ostream &operator<<(std::ostream &os, Pipeline_t p)
  {
    const static char* names[NUM_STAGES] = {"IF", "DR", "EX", "MW"};
    assert(names[p] != NULL);

    os << names[p];

    return os;
  }
}
