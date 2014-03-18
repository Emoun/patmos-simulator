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
// Basic definitions of interfaces to simulate data caches.
//

#ifndef PATMOS_INSTR_CACHE_H
#define PATMOS_INSTR_CACHE_H

#include "memory.h"
#include "data-cache.h"

#include "simulation-core.h"

namespace patmos
{ 
  class excunit_t;

  /// Basic interface for instruction-caches implementations.
  class instr_cache_t
  {
  private:
  public:
    virtual ~instr_cache_t() {}

    virtual excunit_t &get_exception_handler() = 0;
    
    /// Initialize the cache before executing the first instruction.
    /// @param address Address to fetch initial instructions.
    virtual void initialize(uword_t address) = 0;

    /// A simulated instruction fetch from the instruction cache.
    /// @param base The current method base address.
    /// @param address The memory address to fetch from.
    /// @param iw A pointer to store the fetched instruction word.
    /// @return True when the instruction word is available from the read port.
    virtual bool fetch(uword_t base, uword_t address, word_t iw[2]) = 0;

    /// Ensure that the method is in the method cache.
    /// If it is not available yet, initiate a transfer,
    /// evicting other methods if needed.
    /// Has no effect on other caches.
    /// @param address The base address of the method.
    /// @param offset Offset within the method where execution should continue.
    /// @return True when the method is available in the cache, false otherwise.
    virtual bool load_method(word_t address, word_t offset) = 0;

    /// Check whether a method is in the method cache.
    /// @param address The base address of the method.
    /// @return True when the method is available in the cache, false otherwise.
    virtual bool is_available(word_t address) = 0;

    /// Notify the cache that a cycle passed.
    virtual void tick() = 0;

    /// Print debug information to an output stream.
    /// @param os The output stream to print to.
    virtual void print(std::ostream &os) = 0;

    /// Print statistics to an output stream.
    /// @param os The output stream to print to.
    virtual void print_stats(const simulator_t &s, std::ostream &os, 
                             bool short_stats) = 0;
    
    /// Reset statistics.
    virtual void reset_stats() = 0;
    
    /// Flush the cache.
    virtual void flush_cache() = 0;
  };
  

  class no_instr_cache_t : public instr_cache_t
  {
  private:
    /// The global memory.
    memory_t *Memory;

    /// Number of words fetched so far for the current fetch request.
    word_t Fetched;
    
    /// Words fetched so far for the current fetch request.
    word_t Fetch_cache[NUM_SLOTS];
    
    /// Was the current slot access a miss?
    bool Is_miss[NUM_SLOTS];
    
    /// Number of fetch requests with only misses
    uint64_t Num_all_miss;
    
    /// Number of fetch requests with a single miss in the first slot
    uint64_t Num_first_miss;
    
    /// Number of fetch requests with a single miss not in the first slot
    uint64_t Num_succ_miss;
    
    /// Number of fetch requests without misses
    uint64_t Num_hits;
    
  public:
    /// Construct a new instruction cache instance.
    /// The memory passed to this cache is not owned by this cache and must be
    /// managed externally.
    /// @param memory The memory that is accessed through the cache.
    no_instr_cache_t(memory_t &memory) 
    : Memory(&memory), Fetched(0),
      Num_all_miss(0), Num_first_miss(0), Num_succ_miss(0), Num_hits(0)
    {
      for (int i = 0; i < NUM_SLOTS; i++) {
        Is_miss[i] = false;
      }
    }
    
    virtual excunit_t& get_exception_handler() {
      return Memory->get_exception_handler();
    }
    
    virtual void initialize(uword_t address) {}

    virtual bool fetch(uword_t base, uword_t address, word_t iw[NUM_SLOTS]);

    virtual bool load_method(word_t address, word_t offset);

    virtual bool is_available(word_t address);
    
    virtual void tick() {}

    virtual void print(std::ostream &os) {}

    virtual void print_stats(const simulator_t &s, std::ostream &os,
                             bool short_stats);
    
    virtual void reset_stats();
    
    virtual void flush_cache() {}
  };
  
  
  /// An instuction cache using a backing data cache.
  /// @param owning_cache set to true if this cache should own the given memory.
  template<bool IS_OWNING_CACHE>
  class instr_cache_wrapper_t : public no_instr_cache_t
  {
  private:
    /// The global memory or backing cache.
    data_cache_t *Backing_cache;
    
  public:
    /// Construct a new instruction cache instance.
    /// @param memory The memory that is accessed through the cache.
    instr_cache_wrapper_t(data_cache_t *data_cache) 
    : no_instr_cache_t(*data_cache), Backing_cache(data_cache)
    {
    }
    virtual ~instr_cache_wrapper_t() {
      if (IS_OWNING_CACHE) {
        delete Backing_cache;
      }
    }

    /// Notify the memory that a cycle has passed.
    virtual void tick()
    {
      if (IS_OWNING_CACHE) {
        Backing_cache->tick();
      }
      
      no_instr_cache_t::tick();
    }

    /// Print debug information to an output stream.
    /// @param os The output stream to print to.
    virtual void print(std::ostream &os)
    {
      if (IS_OWNING_CACHE) {
        Backing_cache->print(os);
        os << "\n";
      }
      
      no_instr_cache_t::print(os);;
    }

    /// Print statistics to an output stream.
    /// @param os The output stream to print to.
    virtual void print_stats(const simulator_t &s, std::ostream &os, 
                             bool short_stats)
    {
      if (IS_OWNING_CACHE) {
        Backing_cache->print_stats(s, os, short_stats);
        os << "\n";
      }

      no_instr_cache_t::print_stats(s, os, short_stats);
    }
    
    virtual void reset_stats() {
      if (IS_OWNING_CACHE) {
        Backing_cache->reset_stats();
      }
      no_instr_cache_t::reset_stats();
    }
    
    virtual void flush_cache() {
      Backing_cache->flush_cache();
    }
  };

}

#endif // PATMOS_INSTR_CACHE_H
