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

#include <limits>

namespace patmos
{
  // forward definitions
  class memory_t;
  class data_cache_t;
  class stack_cache_t;
  class method_cache_t;
  class binary_format_t;

  /// Define the maximum number of slots in a bundle.
  static const unsigned int NUM_SLOTS = 2;

  /// Define the number of bytes used for the global main memory.
  static const unsigned int NUM_MEMORY_BYTES = 0x4000000;

  /// Define the number of bytes used for the local memory.
  static const unsigned int NUM_LOCAL_MEMORY_BYTES = 0x800;

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

  /// Signal exceptions during simulation
  enum simulation_exception_t
  {
    /// A halt instruction was encountered.
    HALT,

    /// Illegal instruction.
    ILLEGAL
  };

  /// Main class representing the simulation of a single Patmos core.
  class simulator_t
  {
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

    /// Store information about the most recent exception.
    word_t Exception_status;

    /// Active instructions in the pipeline stage.
    instruction_data_t Pipeline[NUM_STAGES][NUM_SLOTS];


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
    simulator_t(memory_t &memory, memory_t &local_memory,
                data_cache_t &data_cache, method_cache_t &method_cache,
                stack_cache_t &stack_cache);

    /// Run the simulator.
    /// @param max_cycles The maximum number of cycles to run the simulation.
    /// @param debug Flag indicating whether debug output should be printed.
    void run(uint64_t max_cycles = std::numeric_limits<uint64_t>::max(),
             bool debug = false);

    /// Print the internal state of the simulator to an output stream.
    /// @param os An output stream.
    void print(std::ostream &os) const;
  };
}

#endif // PATMOS_SIMULATOR_CORE_H

