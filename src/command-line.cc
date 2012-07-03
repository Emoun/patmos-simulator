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
// Helper to parse and print command-line options, e.g., for memory/cache sizes
// using unit prefixes.
//

#include "command-line.h"

#include <algorithm>
#include <cctype>

#include <boost/program_options.hpp>

namespace patmos
{
  std::istream &operator >>(std::istream &in, method_cache_e &mck)
  {
    std::string tmp, kind;
    in >> tmp;

    kind.resize(tmp.size());
    std::transform(tmp.begin(), tmp.end(), kind.begin(), ::tolower);

    if(kind == "ideal")
      mck = MC_IDEAL;
    else if(kind == "lru")
      mck = MC_LRU;
    else throw boost::program_options::validation_error(
                  boost::program_options::validation_error::invalid_option_value,
                  "Unknown method cache kind: " + tmp);

    return in;
  }

  std::ostream &operator <<(std::ostream &os, method_cache_e mck)
  {
    switch(mck)
    {
      case MC_IDEAL:
        os << "ideal"; break;
      case MC_LRU:
        os << "lru"; break;
    }

    return os;
  }

  std::istream &operator >>(std::istream &in, stack_cache_e &sck)
  {
    std::string tmp, kind;
    in >> tmp;

    kind.resize(tmp.size());
    std::transform(tmp.begin(), tmp.end(), kind.begin(), ::tolower);

    if(kind == "ideal")
      sck = SC_IDEAL;
    else if(kind == "block")
      sck = SC_BLOCK;
    else throw boost::program_options::validation_error(
                  boost::program_options::validation_error::invalid_option_value,
                  "Unknown stack cache kind: " + tmp);

    return in;
  }

  std::ostream &operator <<(std::ostream &os, stack_cache_e sck)
  {
    switch(sck)
    {
      case SC_IDEAL:
        os << "ideal"; break;
      case SC_BLOCK:
        os << "block"; break;
    }

    return os;
  }

  std::istream &operator >>(std::istream &in, byte_size_t &bs)
  {
    unsigned int v;
    in >> v;

    std::string tmp, unit;
    in >> tmp;

    unit.resize(tmp.size());
    std::transform(tmp.begin(), tmp.end(), unit.begin(), ::tolower);

    if (unit.empty())
      bs = v;
    else if (unit == "k" || unit == "kb")
      bs = v << 10;
    else if (unit == "m" || unit == "mb")
      bs = v << 20;
    else if (unit == "g" || unit == "gb")
      bs = v << 30;
    else throw boost::program_options::validation_error(
                  boost::program_options::validation_error::invalid_option_value,
                  "Unknown unit: " + tmp);
    return in;
  }

  std::ostream &operator <<(std::ostream &os, const byte_size_t &bs)
  {
    unsigned int v = bs.value();

    if ((v & 0x3fffffff) == 0)
      os << (v >> 10) << "g";
    else if ((v & 0xfffff) == 0)
      os << (v >> 20) << "m";
    else if ((v & 0x3ff) == 0)
      os << (v >> 10) << "k";

    return os;
  }
}
