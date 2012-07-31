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

#ifndef PATMOS_SIMULATOR_CORE_H
#define PATMOS_SIMULATOR_CORE_H

#include "decoder.h"
#include "instruction.h"
#include "exception.h"

#include <limits>

namespace patmos
{
  // forward definitions
  class memory_t;
  class data_cache_t;
  class symbol_map_t;
  class stack_cache_t;
  class method_cache_t;
  class binary_format_t;

  /// Define the maximum number of slots in a bundle.
  static const unsigned int NUM_SLOTS = 2;

  /// Define the number of bytes used for the global main memory.
  static const unsigned int NUM_MEMORY_BYTES = 0x4000000;

  /// Define the number of bytes in a block transferred on an access to the 
  /// global main memory.
  static const unsigned int NUM_MEMORY_BLOCK_BYTES = 8;

  /// Define the number of bytes used for the local memory.
  static const unsigned int NUM_LOCAL_MEMORY_BYTES = 0x800;

  /// Define the number of bytes used for the data cache.
  static const unsigned int NUM_DATA_CACHE_BYTES = 0x800;

  /// Define the number of bytes used for the stack cache.
  static const unsigned int NUM_STACK_CACHE_BYTES = 0x800;

  /// Define the number of bytes in a block of the stack cache.
  static const unsigned int NUM_STACK_CACHE_BLOCK_BYTES = 4;

  /// Define the maximum total number of stack data, including spilled data.
  static const unsigned int NUM_STACK_CACHE_TOTAL_BLOCKS =
                                       std::numeric_limits<unsigned int>::max();

  /// Define the number of bytes used for the method cache.
  static const unsigned int NUM_METHOD_CACHE_BYTES = 0x800;

  /// Define the number of bytes in a block of the method cache.
  static const unsigned int NUM_METHOD_CACHE_BLOCK_BYTES = 32;

  /// General-purpose register holding the program's exit code when terminating.
  static const GPR_e GPR_EXIT_CODE_INDEX = r1;

  /// Definition of Pipeline stages.
  enum Pipeline_t
  {
    /// Fetch the bundle from instruction memory.
    SIF = 0,
    /// Decode the instruction's format and read register operands.
    SDR,
    /// Execute arithmetic operations and calculate addresses for memory
    /// accesses.
    SEX,
    /// Perform memory accesses and register write, if any.
    SMW,

    /// Index of last pipeline stage -- used to instantiate arrays etc.
    NUM_STAGES
  };

  /// Main class representing the simulation of a single Patmos core.
  class simulator_t
  {
  private:
    /// Runtime statistics for an instruction class.
    struct instruction_stat_t
    {
      /// Number of times an instruction of the instruction class was fetched.
      unsigned int Num_fetched;

      /// Number of times an instruction of the instruction class was retired
      /// (s.t. the predicate evaluated to true)
      unsigned int Num_retired;

      /// Number of times an instruction of the instruction class was retired
      /// (s.t. the predicate evaluated to false)
      unsigned int Num_discarded;
    };

    /// A vector containing instruction statistics.
    typedef std::vector<instruction_stat_t> instruction_stats_t;
  public:
    /// Cycle counter
    unsigned int Cycle;

    /// The main memory used during the simulation.
    memory_t &Memory;

    /// The local memory used during the simulation.
    memory_t &Local_memory;

    /// The data cache used during the simulation.
    data_cache_t &Data_cache;

    /// The method cache used during the simulation.
    method_cache_t &Method_cache;

    /// The stack cache used during the simulation.
    stack_cache_t &Stack_cache;

    /// A map to retrieve symbol information from addresses.
    symbol_map_t &Symbols;

    /// The decoder of the simulator.
    decoder_t Decoder;

    /// The base address of the current method.
    uword_t BASE;

    /// The program counter register.
    uword_t PC;

    /// The next value for program counter register.
    uword_t nPC;

    /// The general purpose registers.
    GPR_t GPR;

    /// The predicate registers.
    PRR_t PRR;

    /// The special purpose registers.
    SPR_t SPR;

    /// Counter up to which pipeline stage the processor stalls.
    Pipeline_t Stall;

    /// Active instructions in the pipeline stage.
    instruction_data_t Pipeline[NUM_STAGES][NUM_SLOTS];

    /// Active decoupled load running in parallel to the pipeline.
    instruction_data_t Decoupled_load;

    /// Flag indicating whether a decoupled load is active.
    bool Is_decoupled_load_active;

    /// Runtime statistics on all instructions.
    instruction_stats_t Instruction_stats;

    /// Count number of pipeline bubbles retired.
    unsigned int Num_bubbles_retired;

    /// Number of stall cycles per pipeline stage
    unsigned int Num_stall_cycles[NUM_STAGES];

    /// Perform a step of the simulation for a given pipeline.
    /// @param pst The pipeline stage.
    /// @param f The simulation/commit function to invoke.
    /// @param debug Flag indicating whether debug output should be printed.
    void pipeline_invoke(Pipeline_t pst,
                         void (instruction_data_t::*f)(simulator_t &),
                         bool debug = false);

    /// Flush the pipeline up to and including the given pipeline stage.
    /// @param pst The pipeline stage up to which instructions should be
    /// flushed.
    void pipeline_flush(Pipeline_t pst);

    /// Stall the pipeline up to *not* including the given pipeline stage.
    /// @param pst The pipeline stage up to which instructions should be
    /// stalled.
    void pipeline_stall(Pipeline_t pst);
  public:
    /// Construct a new instance of a Patmos-core simulator
    /// The simulator only retains the references of the arguments passed in the
    /// constructor, i.e., it does not clone them, proclaim ownership, etc.
    /// @param memory The main memory to use during the simulation.
    /// @param local_memory The local memory to use during the simulation.
    /// @param data_cache The data cache to use during the simulation.
    /// @param method_cache The method cache to use during the simulation.
    /// @param stack_cache The stack cache to use during the simulation.
    /// @param symbols A mapping from addresses to symbols.
    simulator_t(memory_t &memory, memory_t &local_memory,
                data_cache_t &data_cache, method_cache_t &method_cache,
                stack_cache_t &stack_cache, symbol_map_t &symbols);

    /// Run the simulator.
    /// @param entry Initialize the method cache, PC, etc. to start execution
    /// from this entry address.
    /// @param debug_cycle Print debug trace starting at the given cycle.
    /// @param max_cycles The maximum number of cycles to run the simulation.
    void run(word_t entry = 0,
             uint64_t debug_cycle = std::numeric_limits<uint64_t>::max(),
             uint64_t max_cycles = std::numeric_limits<uint64_t>::max());

    /// Print the internal state of the simulator to an output stream.
    /// @param os An output stream.
    void print(std::ostream &os) const;

    /// Print runtime statistics of the current simulation run to an output 
    /// stream.
    /// @param os An output stream.
    void print_stats(std::ostream &os) const;
  };


  /// Print the name of the pipeline stage to the output stream.
  /// @param os The output stream.
  /// @param p The pipeline stage.
  /// @return The output stream.
  std::ostream &operator<<(std::ostream &os, Pipeline_t p);
}

#endif // PATMOS_SIMULATOR_CORE_H

