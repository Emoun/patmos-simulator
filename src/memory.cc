/*
   Copyright 2012 Technical University of Denmark, DTU Compute.
   All rights reserved.

   This file is part of the Patmos simulator.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

      1. Redistributions of source code must retain the above copyright notice,
         this list of conditions and the following disclaimer.

      2. Redistributions in binary form must reproduce the above copyright
         notice, this list of conditions and the following disclaimer in the
         documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER ``AS IS'' AND ANY EXPRESS
   OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
   NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
   THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   The views and conclusions contained in the software and documentation are
   those of the authors and should not be interpreted as representing official
   policies, either expressed or implied, of the copyright holder.
 */

//
// Implementation of main memory for Patmos.
//
#include "memory.h"

#include "excunit.h"
#include "exception.h"
#include "simulation-core.h"

#include <boost/format.hpp>

#include <cassert>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <iostream>

using namespace patmos;

void ideal_memory_t::check_initialize_content(simulator_t &s, uword_t address, uword_t size, 
                                              bool is_read, bool ignore_errors)
{
  // check if the access exceeds the memory size
  if((address > Memory_size) || (size > Memory_size - address))
  {
    // We cannot quite ignore this one .. 
    simulation_exception_t::unmapped(address);
  }

  // initialize memory content
  uword_t init_size = std::min(address + std::max(1024u, size), Memory_size);
  for(; Initialized_offset < init_size; Initialized_offset++)
  {
    Content[Initialized_offset] = Randomize ? rand() % 256 : 0;
    if (Init_vector) {
      Init_vector[Initialized_offset] = 0;
    }
  }
  
  if (Init_vector && is_read) {
    // Read, check for uninitialized access
    if (!ignore_errors) {
      uword_t top_addr = std::max(address, 
                                  std::min(Initialized_offset, address + size));
      uword_t cnt = Initialized_offset < address + size ? 
                                         address + size - top_addr : 0;
      for (uword_t i = address; i < top_addr; i++) {
        if (!Init_vector[i]) {
          cnt++;
        }
      }

      bool chkaddr = (Mem_check == patmos::MCK_ERROR_ADDR || 
                      Mem_check == patmos::MCK_WARN_ADDR);
      bool warn = (Mem_check == patmos::MCK_WARN ||
                   Mem_check == patmos::MCK_WARN_ADDR);
      if ((chkaddr && cnt == size) || (!chkaddr && cnt > 0)) {
        std::stringstream ss;
        ss << "Read of address 0x" << std::hex << address << std::dec
           << " of size " << size << " reads " << cnt << " uninitialized bytes";
        if (warn) {
          ss << " at PC: " << std::hex << s.PC << std::dec << ", Cycle: " << s.Cycle;
          std::cerr << "\n*** Warning: " << ss.str() << "\n";
          s.Dbg_stack.print(std::cerr);
        } else {
          simulation_exception_t::illegal_access(ss.str());
        }
      }
    }
  } else if (Init_vector) {
    // A write.. mark as used
    for (uword_t i = address; i < address + size; i++) {
      Init_vector[i] = 1;
    }
  }
}

bool ideal_memory_t::read(simulator_t &s, uword_t address, byte_t *value, uword_t size, bool is_fetch)
{
  if (Mmu) {
    address = Mmu->xlate(address, is_fetch ? MMU_EX : MMU_RD);
  }

  // check if the access exceeds the memory size and lazily initialize
  // memory content
  check_initialize_content(s, address, size, true);

  // read the data from the memory
  for(unsigned int i = 0; i < size; i++)
  {
    *value++ = Content[address++];
  }

  return true;
}

bool ideal_memory_t::write(simulator_t &s, uword_t address, byte_t *value, uword_t size)
{
  if (Mmu) {
    address = Mmu->xlate(address, MMU_WR);
  }

  // check if the access exceeds the memory size and lazily initialize
  // memory content
  check_initialize_content(s, address, size, false);

  // write the data to the memory
  for(unsigned int i = 0; i < size; i++)
  {
    Content[address++] = *value++;
  }

  return true;
}

void ideal_memory_t::read_peek(simulator_t &s, uword_t address, byte_t *value, uword_t size, bool is_fetch)
{
  if (Mmu) {
    address = Mmu->xlate(address, is_fetch ? MMU_EX : MMU_RD);
  }

  // Check, but ignore errors.
  check_initialize_content(s, address, size, true, true);

  // read the data from the memory
  for(unsigned int i = 0; i < size; i++)
  {
    *value++ = Content[address++];
  }
}

void ideal_memory_t::write_peek(simulator_t &s, uword_t address, byte_t *value, uword_t size)
{
  if (Mmu) {
    address = Mmu->xlate(address, MMU_WR);
  }

  check_initialize_content(s, address, size, false, true);

  // write the data to the memory
  for(unsigned int i = 0; i < size; i++)
  {
    Content[address++] = *value++;
  }
}


uword_t fixed_delay_memory_t::get_aligned_size(uword_t address, uword_t size,
                                               uword_t &aligned_address)
{
  uword_t start = (address/Num_bytes_per_burst) * Num_bytes_per_burst;
  uword_t end   = (((address + size - 1)/Num_bytes_per_burst) + 1) *
                  Num_bytes_per_burst;
  aligned_address = start;
  return end - start;
}

unsigned int fixed_delay_memory_t::get_transfer_ticks(uword_t aligned_address,
                                                      uword_t aligned_size, 
                                                      bool is_load, 
                                                      bool is_posted) 
{
  unsigned int num_blocks = (((aligned_size-1) / Num_bytes_per_burst) + 1); 
  unsigned int num_ticks = Num_ticks_per_burst * num_blocks;
  
  if (is_load || !is_posted) {
    num_ticks += Num_read_delay_ticks;
  }
  return num_ticks;
}

void fixed_delay_memory_t::tick_request(request_info_t &req)
{
  req.Num_ticks_remaining--;
}

const request_info_t &fixed_delay_memory_t::find_or_create_request(simulator_t &s,
                                              uword_t address, uword_t size,
                                              bool is_load, bool is_fetch, bool is_posted)
{
  if (Mmu) {
    address = Mmu->xlate(address, is_load ? (is_fetch ? MMU_EX : MMU_RD) : MMU_WR);
  }

  // check if the access exceeds the memory size and lazily initialize
  // memory content
  check_initialize_content(s, address, size, is_load);

  // see if the request already exists
  for(requests_t::const_iterator i(Requests.begin()), ie(Requests.end());
      i != ie; i++)
  {
    // found matching request?
    if (i->Address == address && i->Size == size && i->Is_load == is_load)
      return *i;
  }

  // no matching request found, create a new one
  uword_t aligned_address;
  uword_t aligned_size = get_aligned_size(address, size, aligned_address);
  unsigned int num_ticks = get_transfer_ticks(aligned_address, aligned_size, 
                                              is_load, is_posted);
  
  request_info_t tmp = {address, size, is_load, is_posted, num_ticks};
  Requests.push_back(tmp);

  // Update statistics
  Num_max_queue_size = std::max(Num_max_queue_size, (unsigned)Requests.size());
  Num_busy_cycles += num_ticks;
  if (is_load == Last_is_load && address == Last_address + 1) {
    Num_consecutive_requests++;
  }
  if (is_load) {
    Num_reads++;
    Num_bytes_read += size;
    Num_bytes_read_transferred += aligned_size;
  } else {
    Num_writes++;
    Num_bytes_written += size;
    Num_bytes_write_transferred += aligned_size;
  }
  Last_address = address + size;
  Last_is_load = is_load;
  
  // calculate bucket for request size histogram
  uword_t hist_size = (((size - 1) / 4) + 1) * 4;
  request_size_map_t::iterator it = Num_requests_per_size.find(hist_size);
  if (it == Num_requests_per_size.end()) {
    Num_requests_per_size.insert(std::make_pair(hist_size, (uint64_t)1));
  } else {
    it->second++;
  }
  
  
  // return the newly created request
  return Requests.back();
}

bool fixed_delay_memory_t::read(simulator_t &s, uword_t address, byte_t *value, uword_t size, bool is_fetch)
{
  // get the request info
  const request_info_t &req(find_or_create_request(s, address, size, true, is_fetch));

  // check if the request has finished
  if(req.Num_ticks_remaining == 0)
  {
#ifndef NDEBUG
    // check request
    request_info_t &tmp(Requests.front());
    assert(tmp.Address == req.Address && tmp.Size == req.Size &&
            tmp.Is_load == req.Is_load);
#endif

    // clean-up the request
    Requests.erase(Requests.begin());

    // read the data
    return ideal_memory_t::read(s, address, value, size, is_fetch);
  }
  else
  {
    // not yet finished
    return false;
  }
}

bool fixed_delay_memory_t::write(simulator_t &s, uword_t address, byte_t *value, uword_t size)
{
  // To avoid delaying reads until the write has been stored to the queue,
  // we just add it to the queue and delay later until the queue is small 
  // enough.
  bool posted = (Num_posted_writes > 0);
  
  // get the request info
  const request_info_t &req(find_or_create_request(s, address, size, false, 
                                                    posted));

  // check if the request has finished
  if(req.Num_ticks_remaining == 0)
  {
#ifndef NDEBUG
    // check request
    request_info_t &tmp(Requests.front());
    assert(tmp.Address == req.Address && tmp.Size == req.Size &&
            tmp.Is_load == req.Is_load);
#endif

    // clean-up the request
    Requests.erase(Requests.begin());

    // write the data
    return ideal_memory_t::write(s, address, value, size);
  }
  else if (posted) {
    // delay only until the request queue size is small enough
    return Requests.size() <= Num_posted_writes;
  }
  else
  {
    // not yet finished
    return false;
  }
}

bool fixed_delay_memory_t::is_ready()
{
  return Requests.empty();
}

void fixed_delay_memory_t::tick(simulator_t &s)
{
  // check if there are only posted writes in the queue, then there is
  // no one waiting on any result and we are actually not stalling in this
  // cycle
  if (!Requests.empty() && Requests.size() <= Num_posted_writes) {
    bool posted = true;
    for (requests_t::iterator it = Requests.begin(), ie = Requests.end();
          it != ie; it++)
    {
      if (!it->Is_posted) {
        posted = false;
        break;
      }
    }
    if (posted) {
      Num_posted_write_cycles++;
    }
  }

  // update the request queue
  if (!Requests.empty() && Requests.front().Num_ticks_remaining)
  {
    request_info_t &req = Requests.front();
    tick_request(req);
    
    if (req.Num_ticks_remaining == 0 && req.Is_posted) {
      Requests.erase(Requests.begin());
    }
  }
}

/// Print the internal state of the memory to an output stream.
/// @param os The output stream to print to.
void fixed_delay_memory_t::print(const simulator_t &s, std::ostream &os) const
{
  if (Requests.empty())
  {
    os << " IDLE\n";
  }
  else
  {
    for(requests_t::const_iterator i(Requests.begin()), ie(Requests.end());
        i != ie; i++)
    {
      os << boost::format(" %1%: %2% (0x%3$08x %4%)\n")
        % (i->Is_load ? "LOAD " : "STORE") % i->Num_ticks_remaining
        % i->Address % i->Size;
    }
  }
}

void fixed_delay_memory_t::print_stats(const simulator_t &s, std::ostream &os, 
                                       const stats_options_t& options)
{
  uint64_t stall_cycles = Num_busy_cycles - Num_posted_write_cycles;
  
  float cycles = s.Cycle;
  float stalls = (float)stall_cycles/cycles;
  float hidden = (float)Num_posted_write_cycles/cycles;
  uint64_t total_bytes = Num_bytes_read_transferred + 
                          Num_bytes_write_transferred;
  
  os << boost::format("                                total  %% of cycles\n"
                      "   Max Queue Size        : %1$10d\n"
                      "   Consecutive Transfers : %2$10d\n"
                      "   Requests              : %3$10d\n"
                      "   Bursts transferred    : %4$10d\n"
                      "   Bytes transferred     : %5$10d\n"
                      "   Stall Cycles          : %6$10d %7$10.2f%%\n"
                      "   Hidden Write Cycles   : %8$10d %9$10.2f%%\n\n")
    % Num_max_queue_size 
    % Num_consecutive_requests
    % (Num_reads + Num_writes) 
    % (total_bytes / Num_bytes_per_burst)
    % total_bytes
    % stall_cycles % (stalls * 100.0)
    % Num_posted_write_cycles % (hidden * 100.0);
  
  float read_pct = (float)Num_bytes_read / (float)total_bytes;
  float write_pct = (float)Num_bytes_written / (float)total_bytes;
  float read_trans_pct = (float)Num_bytes_read_transferred /
                          (float)total_bytes;
  float write_trans_pct = (float)Num_bytes_write_transferred / 
                          (float)total_bytes;
  
  os << boost::format("                                 Read                  Write\n"
                      "   Requests              : %1$10d             %2$10d\n"
                      "   Bytes Requested       : %3$10d %4$10.2f%% %5$10d %6$10.2f%%\n"
                      "   Bytes Transferred     : %7$10d %8$10.2f%% %9$10d %10$10.2f%%\n\n")
    % Num_reads % Num_writes 
    % Num_bytes_read % (read_pct * 100.0) 
    % Num_bytes_written % (write_pct * 100.0)
    % Num_bytes_read_transferred % (read_trans_pct * 100.0)
    % Num_bytes_write_transferred % (write_trans_pct * 100.0);
    
  if (options.short_stats) 
    return;
  
  os << "Request size    #requests\n";
  for (request_size_map_t::iterator it = Num_requests_per_size.begin(),
        ie = Num_requests_per_size.end(); it != ie; it++)
  {
    os << boost::format("  %1$10d : %2$12d\n") % it->first % it->second;
  }
}

void fixed_delay_memory_t::reset_stats() 
{
  Num_max_queue_size = 0;
  Num_consecutive_requests = 0;
  Num_busy_cycles = 0;
  Num_posted_write_cycles = 0;
  Num_reads = 0;
  Num_writes = 0;
  Num_bytes_read = 0;
  Num_bytes_written = 0;
  Num_bytes_read_transferred = 0;
  Num_bytes_write_transferred = 0;
  Num_requests_per_size.clear();
}


unsigned int variable_burst_memory_t::get_transfer_ticks(uword_t aligned_address,
                                                         uword_t aligned_size, 
                                                         bool is_load, 
                                                         bool is_posted)
{
  unsigned int start_page = aligned_address / Num_bytes_per_page;
  unsigned int end_page = (aligned_address + aligned_size - 1) / Num_bytes_per_page;
  unsigned int num_pages = end_page - start_page + 1;
  
  // We assume that even variable sized requests are min_burst_length aligned, 
  // simplifying the hardware (note that the hardware does not actually
  // need to align anything though, it just needs to obey the timing requirements
  // and potentially not overlap read/write burst and precharge/activate).
  //
  // We could also just word-align in get_aligned_size(), then we need
  // the following to calculate the correct request length that accounts for 
  // non-overlapping PRE and ACT:
  /*
  // We start at least min_burst bytes before the end of the first page
  unsigned int start_address = std::min(aligned_address,
                                        (start_page + 1) * Num_bytes_per_page -
                                        Num_bytes_per_burst);
  // We end at least min_burst bytes after the start of the last page
  unsigned int end_address = std::max(aligned_address + aligned_size, 
                                      end_page * Num_bytes_per_page +
                                      Num_bytes_per_burst);
  
  // We transfer at least min_burst bytes if we do not span over multiple pages.
  unsigned int length = std::min(end_address - start_address, 
                                 Num_bytes_per_burst);
  */
  
  unsigned int length = aligned_size;
  
  // Now, in every page, we transfer at least min_burst bytes, and have
  // exactly once the cost for min_burst transfer.
  // Note that if burst_size == page_size, this is the same as the fixed-delay
  // memory.
  unsigned num_ticks = num_pages * Num_ticks_per_burst;
  length -= num_pages * Num_bytes_per_burst;
  
  // The rest of the bytes are transferred with one cycle per word.
  num_ticks += length / 4;
  
  if (is_load || !is_posted) {
    num_ticks += Num_read_delay_ticks;
  }

  return num_ticks;    
}



tdm_memory_t::tdm_memory_t(unsigned int memory_size, 
                           unsigned int num_bytes_per_burst,
                           unsigned int num_posted_writes,
                           unsigned int num_cores,
                           unsigned int cpu_id,
                           unsigned int num_ticks_per_burst,
                           unsigned int num_read_delay_ticks,
                           unsigned int num_refresh_ticks_per_round, 
                           bool randomize, mem_check_e memchk)
: fixed_delay_memory_t(memory_size, num_bytes_per_burst, num_posted_writes,
  num_ticks_per_burst, num_read_delay_ticks, randomize, memchk),
  Round_counter(0), Is_Transferring(false)
{
  Round_length = num_cores * num_ticks_per_burst + num_refresh_ticks_per_round;
  Round_start  = cpu_id * num_ticks_per_burst;
  
  if (num_ticks_per_burst + num_read_delay_ticks >= Round_length) {
    std::cerr << 
           "Read delay too long; overlapping TDM requests are not supported.\n";
    abort();
  }
}

unsigned int tdm_memory_t::get_transfer_ticks(uword_t aligned_address,
                                              uword_t aligned_size, 
                                              bool is_load, bool is_posted)
{
  unsigned int num_blocks = ((aligned_size - 1) / Num_bytes_per_burst) + 1;
  
  // We are counting down TDM slots at round end instead of actual ticks.
  return num_blocks;
}

void tdm_memory_t::tick_request(request_info_t &req)
{
  int round_end = Round_start + Num_ticks_per_burst;
  if (!req.Is_posted) {
    round_end += Num_read_delay_ticks;
  }
  if (round_end >= Round_length) {
    round_end -= Round_length;
  }
  
  // We are counting down TDM rounds
  if (round_end == Round_counter) {
    req.Num_ticks_remaining--;
    Is_Transferring = false;
  }
}

void tdm_memory_t::tick(simulator_t &s)
{
  // TODO can we start a transfer if it is requested in the same cycle the 
  // TDM slot starts? If so, move the counter update at the end.
  Round_counter = (Round_counter + 1) % Round_length;
  
  // Check if we have outstanding requests at the beginning of a round
  if (Round_counter == Round_start) {
    // should have been sanity checked in constructor.
    assert(!Is_Transferring && "Overlapping transfers are not supported");
    
    Is_Transferring = !Requests.empty();
  }
  
  fixed_delay_memory_t::tick(s);
}
