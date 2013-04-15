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
// Interface to method-cache implementations.
//

#ifndef PATMOS_METHOD_CACHE_H
#define PATMOS_METHOD_CACHE_H

#include "basic-types.h"
#include "endian-conversion.h"
#include "simulation-core.h"
#include "symbol.h"

#include <cassert>
#include <cmath>
#include <map>
#include <ostream>

#include <boost/format.hpp>
#include "exception.h"

namespace patmos
{
  /// Basic interface for method-caches implementations.
  class method_cache_t
  {
  private:
  public:
    virtual ~method_cache_t() {}
    
    /// Initialize the cache before executing the first instruction.
    /// @param address Address to fetch initial instructions.
    virtual void initialize(uword_t address) = 0;

    /// A simulated instruction fetch from the method cache.
    /// @param address The memory address to fetch from.
    /// @param iw A pointer to store the fetched instruction word.
    /// @return True when the instruction word is available from the read port.
    virtual bool fetch(uword_t address, word_t iw[2]) = 0;


    /// Check whether a method is in the method cache, if it is not available
    /// yet initiate a transfer, evicting other methods if needed.
    /// @param address The base address of the method.
    /// @return True when the method is available in the cache, false otherwise.
    virtual bool is_available(word_t address) = 0;

    /// Assert that the method is in the method cache.
    /// @param address The base address of the method.
    /// @return True when the method is available in the cache, false otherwise.
    virtual bool assert_availability(word_t address) = 0;

    /// Notify the cache that a cycle passed.
    virtual void tick() = 0;

    /// Print debug information to an output stream.
    /// @param os The output stream to print to.
    virtual void print(std::ostream &os) = 0;

    /// Print statistics to an output stream.
    /// @param os The output stream to print to.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print_stats(std::ostream &os, const symbol_map_t &symbols) = 0;
  };

  /// An ideal method cache, i.e., all methods are always in the cache --
  /// magically.
  class ideal_method_cache_t : public method_cache_t
  {
  private:
    /// The backing memory to fetch instructions from.
    memory_t &Memory;
  public:
    /// Construct an ideal method cache that always hits.
    /// @param memory The memory to fetch instructions from.
    ideal_method_cache_t(memory_t &memory) : Memory(memory)
    {
    }

    /// Initialize the cache before executing the first instruction.
    /// @param address Address to fetch initial instructions.
    virtual void initialize(uword_t address)
    {
      // nothing to be done here
    }

    /// A simulated instruction fetch from the method cache.
    /// @param address The memory address to fetch from.
    /// @param iw A pointer to store the fetched instruction word.
    /// @return True when the instruction word is available from the read port.
    virtual bool fetch(uword_t address, word_t iw[2])
    {
      Memory.read_peek(address, reinterpret_cast<byte_t*>(&iw[0]),
                       sizeof(word_t)*2);
      return true;
    }

    /// Check whether a method is in the method cache, if it is not available
    /// yet initiate a transfer, evicting other methods if needed.
    /// @param address The base address of the method.
    /// @return True when the method is available in the cache, false otherwise.
    virtual bool is_available(word_t address)
    {
      return true;
    }

    /// Assert that the method is in the method cache.
    /// @param address The base address of the method.
    /// @return True when the method is available in the cache, false otherwise.
    virtual bool assert_availability(word_t address)
    {
      return true;
    }

    /// Notify the cache that a cycle passed.
    virtual void tick()
    {
      // do nothing here
    }

    /// Print debug information to an output stream.
    /// @param os The output stream to print to.
    virtual void print(std::ostream &os)
    {
      // nothing to do here either, since the cache has no internal state.
    }

    /// Print statistics to an output stream.
    /// @param os The output stream to print to.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print_stats(std::ostream &os, const symbol_map_t &symbols)
    {
      // nothing to do here either, since the cache has no internal state.
    }
  };

  /// Cache statistics of a particular method.
  class method_stats_info_t
  {
  public:
    /// Number of cache hits for the method.
    unsigned int Num_hits;

    /// Number of cache misses for the method.
    unsigned int Num_misses;

    /// Initialize the method statistics.
    method_stats_info_t() : Num_hits(0), Num_misses(0)
    {
    }
  };

  /// A direct-mapped method cache using LRU replacement on methods.
  /// The cache is organized in blocks (Num_blocks) each of a fixed size
  /// (NUM_BLOCK_BYTES) in bytes. On start-up the cache fetches a given number
  /// of blocks from address 0 of its memory (NUM_INIT_BLOCKS).
  template<unsigned int NUM_BLOCK_BYTES = NUM_METHOD_CACHE_BLOCK_BYTES,
           unsigned int NUM_INIT_BLOCKS = 4>
  class lru_method_cache_t : public method_cache_t
  {
  protected:
    /// Phases of fetching a method from memory.
    enum phase_e
    {
      /// The method cache is idle and available to handle requests.
      IDLE,
      /// The method cache is on the way of fetching the size of the method
      /// from memory.
      SIZE,
      /// The instructions of the method are being transferred from memory.
      TRANSFER
    };

    /// Bookkeeping information on methods in the cache.
    class method_info_t
    {
    public:
      /// Pointer to the instructions of the method.
      byte_t *Instructions;

      /// The address of the method.
      uword_t Address;

      /// The number of blocks occupied by the method.
      uword_t Num_blocks;

      /// The size of the method in bytes.
      uword_t Num_bytes;

      /// Construct a method lru info object. All data is initialized to zero.
      /// @param instructions Pointer to the method's instructions.
      method_info_t(byte_t *instructions = NULL): Instructions(instructions),
          Address(0), Num_blocks(0), Num_bytes(0)
      {
      }

      /// Update the internal data of the method lru info entry.
      /// @param instructions Pointer to the method's instructions.
      /// @param address The new address of the entry.
      /// @param num_blocks The number of blocks occupied in the method cache.
      /// @param num_bytes The number of valid instruction bytes of the method.
      void update(byte_t *instructions, uword_t address, uword_t num_blocks,
                  uword_t num_bytes)
      {
        Instructions = instructions;
        Address = address;
        Num_blocks = num_blocks;
        Num_bytes = num_bytes;
      }
    };

    /// Map addresses to cache statistics of individual methods.
    typedef std::map<word_t, method_stats_info_t> method_stats_t;

    /// The backing memory to fetch instructions from.
    memory_t &Memory;

    /// Number of blocks in the method cache.
    unsigned int Num_blocks;

    /// Currently active phase to fetch a method from memory.
    phase_e Phase;

    /// Number of blocks of the currently pending transfer, if any.
    uword_t Num_transfer_blocks;

    /// Number of bytes of the currently pending transfer, if any.
    uword_t Num_transfer_bytes;

    /// The methods in the cache sorted by age.
    method_info_t *Methods;

    /// The methods' instructions sorted by ID.
    byte_t *Instructions;

    /// The number of methods currently in the cache.
    unsigned int Num_active_methods;

    /// The sum of sizes of all method entries currently active in the cache.
    unsigned int Num_active_blocks;

    /// Number of blocks transferred from the main memory.
    unsigned int Num_blocks_transferred;

    /// Largest number of blocks transferred from the main memory for a single
    /// method.
    unsigned int Num_max_blocks_transferred;

    /// Number of bytes transferred from the main memory.
    unsigned int Num_bytes_transferred;

    /// Largest number of bytes transferred from the main memory for a single
    /// method.
    unsigned int Num_max_bytes_transferred;

    /// Number of cache hits.
    unsigned int Num_hits;

    /// Number of cache misses.
    unsigned int Num_misses;

    /// Number of stall cycles caused by method cache misses.
    unsigned int Num_stall_cycles;

    /// Cache statistics of individual method.
    method_stats_t Method_stats;

    /// A simulated instruction fetch from the method cache.
    /// @param current_method The currently active method.
    /// @param address The memory address to fetch from.
    /// @param iw A pointer to store the fetched instruction word.
    /// @return True when the instruction word is available from the read port.
    bool do_fetch(const method_info_t &current_method, uword_t address,
                  word_t iw[2])
    {
      if(address < current_method.Address ||
         current_method.Address + current_method.Num_bytes <= address)
      {
        simulation_exception_t::illegal_pc(current_method.Address);
      }

      // get instruction word from the method's instructions
      byte_t *iwp = reinterpret_cast<byte_t*>(&iw[0]);
      for(unsigned int i = 0; i != sizeof(word_t)*2; i++, iwp++)
      {
        *iwp = current_method.Instructions[address + i -
                                           current_method.Address];
      }
      return true;
    }

    /// Check whether the method at the given address is in the method cache.
    /// @param address The method address.
    /// @return True in case the method is in the cache, false otherwise.
    virtual bool lookup(uword_t address)
    {
      // check if the address is in the cache
      for(unsigned int i = Num_blocks - 1; i >= Num_blocks - Num_active_methods;
          i--)
      {
        if (Methods[i].Address == address)
        {
          // update the ordering of the methods to match LRU.

          // store the currently accessed entry
          method_info_t tmp = Methods[i];

          // shift all methods between the location of the currently accessed
          // entry and the previously most recently used entry.
          for(unsigned int j = i; j < Num_blocks - 1; j++)
          {
            Methods[j] = Methods[j + 1];
          }

          // reinsert the current entry at the head of the table
          Methods[Num_blocks - 1] = tmp;

          return true;
        }
      }

      // No entry matches the given address.
      return false;
    }
  public:
    /// Construct an LRU-based method cache.
    /// @param memory The memory to fetch instructions from on a cache miss.
    /// @param num_blocks The size of the cache in blocks.
    lru_method_cache_t(memory_t &memory, unsigned int num_blocks) :
        Memory(memory), Num_blocks(num_blocks), Phase(IDLE),
        Num_transfer_blocks(0), Num_transfer_bytes(0), Num_active_methods(0),
        Num_active_blocks(0), Num_blocks_transferred(0),
        Num_max_blocks_transferred(0), Num_bytes_transferred(0),
        Num_max_bytes_transferred(0), Num_hits(0), Num_misses(0),
        Num_stall_cycles(0)
    {
      Methods = new method_info_t[Num_blocks];
      for(unsigned int i = 0; i < Num_blocks; i++)
        Methods[i] = method_info_t(new byte_t[NUM_BLOCK_BYTES * Num_blocks]);
    }

    /// Initialize the cache before executing the first instruction.
    /// @param address Address to fetch initial instructions.
    virtual void initialize(uword_t address)
    {
      assert(Num_active_blocks == 0 && Num_active_methods == 0);

      // get 'most-recent' method of the cache
      method_info_t &current_method = Methods[Num_blocks - 1];

      // initialize the method cache with some dummy method entry.
      Memory.read_peek(address, current_method.Instructions, NUM_INIT_BLOCKS *
                                                             NUM_BLOCK_BYTES);
      current_method.update(current_method.Instructions, address,
                            NUM_INIT_BLOCKS, NUM_INIT_BLOCKS * NUM_BLOCK_BYTES);
      Num_active_blocks = NUM_INIT_BLOCKS;
      Num_active_methods = 1;
    }

    /// A simulated instruction fetch from the method cache.
    /// @param address The memory address to fetch from.
    /// @param iw A pointer to store the fetched instruction word.
    /// @return True when the instruction word is available from the read port.
    virtual bool fetch(uword_t address, word_t iw[2])
    {
      // fetch from 'most-recent' method of the cache
      return do_fetch(Methods[Num_blocks - 1], address, iw);
    }

    /// Check whether a method is in the method cache, if it is not available
    /// yet initiate a transfer, evicting other methods if needed.
    /// @param address The base address of the method.
    /// @return True when the method is available in the cache, false otherwise.
    virtual bool is_available(word_t address)
    {
      // check status of the method cache
      switch(Phase)
      {
        // a new request has to be started.
        case IDLE:
        {
          assert(Num_transfer_blocks == 0 && Num_transfer_bytes == 0);

          if (lookup(address))
          {
            // method is in the cache ... done!
            Num_hits++;
            Method_stats[address].Num_hits++;
            return true;
          }
          else
          {
            // proceed to next phase ... fetch the size from memory.
            // NOTE: the next phase starts immediately.
            Phase = SIZE;
            Num_misses++;
            Method_stats[address].Num_misses++;
          }
        }

        // the size of the method has to be fetched from memory.
        case SIZE:
        {
          assert(Num_transfer_blocks == 0 && Num_transfer_bytes == 0);

          // get the size of the method that should be loaded
          uword_t num_bytes_big_endian;
          if (Memory.read(address - sizeof(uword_t),
                          reinterpret_cast<byte_t*>(&num_bytes_big_endian),
                          sizeof(uword_t)))
          {
            // convert method size to native endianess and compute size in
            // blocks
            Num_transfer_bytes = from_big_endian<big_uword_t>(
                                                          num_bytes_big_endian);
            Num_transfer_blocks = std::ceil(((float)Num_transfer_bytes) /
                                             NUM_BLOCK_BYTES);

            // check method size against cache size.
            if (Num_transfer_blocks == 0 || Num_transfer_blocks > Num_blocks)
            {
              simulation_exception_t::code_exceeded(address);
            }

            // throw other entries out of the cache if needed
            while (Num_active_blocks + Num_transfer_blocks > Num_blocks)
            {
              assert(Num_active_methods > 0);
              Num_active_blocks -=
                            Methods[Num_blocks - Num_active_methods].Num_blocks;
              Num_active_methods--;
            }

            // update counters
            Num_active_methods++;
            Num_active_blocks += Num_transfer_blocks;
            Num_blocks_transferred += Num_transfer_blocks;
            Num_max_blocks_transferred = std::max(Num_max_blocks_transferred,
                                                  Num_transfer_blocks);
            Num_bytes_transferred += Num_transfer_bytes;
            Num_max_bytes_transferred = std::max(Num_max_bytes_transferred,
                                                  Num_transfer_bytes);

            // shift the remaining blocks
            byte_t *saved_instructions =
                          Methods[Num_blocks - Num_active_methods].Instructions;
            for(unsigned int j = Num_blocks - Num_active_methods;
                j < Num_blocks - 1; j++)
            {
              Methods[j] = Methods[j + 1];
            }

            // insert the new entry at the head of the table
            Methods[Num_blocks - 1].update(saved_instructions, address,
                                           Num_transfer_blocks,
                                           Num_transfer_bytes);

            // proceed to next phase ... the size of the method has been fetched
            // from memory, now transfer the method's instructions.
            // NOTE: the next phase starts immediately.
            Phase = TRANSFER;
          }
          else
          {
            // keep waiting until the size has been loaded.
            return false;
          }
        }

        // begin transfer from main memory to the method cache.
        case TRANSFER:
        {
          assert(Num_transfer_blocks != 0 && Num_transfer_bytes != 0);

          if (Memory.read(address, Methods[Num_blocks - 1].Instructions,
                          Num_transfer_blocks * NUM_BLOCK_BYTES))
          {
            // the transfer is done, go back to IDLE phase
            Num_transfer_blocks = Num_transfer_bytes = 0;
            Phase = IDLE;
            return true;
          }
          else
          {
            // keep waiting until the transfer is completed.
            return false;
          }
        }
      }

      assert(false);
      abort();
    }

    /// Assert that the method is in the method cache.
    /// @param address The base address of the method.
    /// @return True when the method is available in the cache, false otherwise.
    virtual bool assert_availability(word_t address)
    {
      // check if the address is in the cache
      for(unsigned int i = Num_blocks - 1; i >= Num_blocks - Num_active_methods;
          i--)
      {
        if (Methods[i].Address == address)
        {
          return true;
        }
      }

      return false;
    }

    /// Notify the cache that a cycle passed -- i.e., if there is an ongoing
    /// transfer of a method to the cache, advance this transfer by one cycle.
    virtual void tick()
    {
      // update statistics
      if (Phase != IDLE)
        Num_stall_cycles++;
    }

    /// Print debug information to an output stream.
    /// @param os The output stream to print to.
    virtual void print(std::ostream &os)
    {
      os << boost::format(" #M: %1$02d #B: %2$02d\n")
         % Num_active_methods % Num_active_blocks;

      for(unsigned int i = Num_blocks - 1; i >= Num_blocks - Num_active_methods;
          i--)
      {
        os << boost::format("   M%1$02d: 0x%2$08x (%3$8d Blk %4$8d b)\n")
           % (Num_blocks - i) % Methods[i].Address % Methods[i].Num_blocks
           % Methods[i].Num_bytes;
      }

      os << '\n';
    }

    /// Print statistics to an output stream.
    /// @param os The output stream to print to.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print_stats(std::ostream &os, const symbol_map_t &symbols)
    {
      // instruction statistics
      os << boost::format("\n\nMethod Cache Statistics:\n"
                          "                            total        max.\n"
                          "   Blocks Transferred: %1$10d  %2$10d\n"
                          "   Bytes Transferred : %3$10d  %4$10d\n"
                          "   Cache Hits        : %5$10d\n"
                          "   Cache Misses      : %6$10d\n"
                          "   Miss Stall Cycles : %7$10d\n\n")
        % Num_blocks_transferred % Num_max_blocks_transferred 
        % Num_bytes_transferred % Num_max_bytes_transferred
        % Num_hits % Num_misses % Num_stall_cycles;

      // print stats per method
      os << "       Method:      #hits     #misses\n";
      for(method_stats_t::iterator i(Method_stats.begin()),
          ie(Method_stats.end()); i != ie; i++)
      {
        os << boost::format("   0x%1$08x: %2$10d  %3$10d    %4%\n")
           % i->first % i->second.Num_hits % i->second.Num_misses
           % symbols.find(i->first);
      }
    }

    /// free dynamically allocated cache memory.
    virtual ~lru_method_cache_t()
    {
      for(unsigned int i = 0; i < Num_blocks; i++)
        delete[] Methods[i].Instructions;

      delete [] Methods;
    }
  };

  /// A direct-mapped method cache using FIFO replacement on methods.
  /// \see lru_method_cache_t
  template<unsigned int NUM_BLOCK_BYTES = NUM_METHOD_CACHE_BLOCK_BYTES,
           unsigned int NUM_INIT_BLOCKS = 4>
  class fifo_method_cache_t : public lru_method_cache_t<NUM_BLOCK_BYTES,
                                                        NUM_INIT_BLOCKS>
  {
  private:
    typedef lru_method_cache_t<NUM_BLOCK_BYTES, NUM_INIT_BLOCKS> base_t;

    size_t active_method;

    /// Check whether the method at the given address is in the method cache.
    /// @param address The method address.
    /// @return True in case the method is in the cache, false otherwise.
    virtual bool lookup(uword_t address)
    {
      return base_t::assert_availability(address);
    }
  public:
    /// Construct an FIFO-based method cache.
    /// @param memory The memory to fetch instructions from on a cache miss.
    /// @param num_blocks The size of the cache in blocks.
    fifo_method_cache_t(memory_t &memory, unsigned int num_blocks) :
        lru_method_cache_t<NUM_BLOCK_BYTES, NUM_INIT_BLOCKS>(memory, num_blocks)
    {
	active_method = base_t::Num_blocks - 1;
    }

    /// Check whether a method is in the method cache, if it is not available
    /// yet initiate a transfer, evicting other methods if needed.
    /// @param address The base address of the method.
    /// @return True when the method is available in the cache, false otherwise.
    virtual bool is_available(word_t address)
    {
      // check if the address is in the cache
      bool avail = base_t::is_available(address);

      if (avail) {
	// update the active method pointer
	for(unsigned int i = base_t::Num_blocks - 1; 
	    i >= base_t::Num_blocks - base_t::Num_active_methods; i--)
	{
	  if (base_t::Methods[i].Address == address)
	  {
	    active_method = i;
	  }
	}
      }
      
      return avail;
    }

    /// A simulated instruction fetch from the method cache.
    /// @param address The memory address to fetch from.
    /// @param iw A pointer to store the fetched instruction word.
    /// @return True when the instruction word is available from the read port.
    virtual bool fetch(uword_t address, word_t iw[2])
    {
      // fetch from the currently active method
      return base_t::do_fetch(base_t::Methods[active_method], address, iw);
    }
  };
}

#endif // PATMOS_METHOD_CACHE_H
