//
// exception.cc
//
// Copyright (C) 1996 Limit Point Systems, Inc.
//
// Author: Joseph Kenny <jpkenny@sandia.gov>
// Maintainer: LPS
//
// This file is part of the SC Toolkit.
//
// The SC Toolkit is free software; you can redistribute it and/or modify
// it under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// The SC Toolkit is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public License
// along with the SC Toolkit; see the file COPYING.LIB.  If not, write to
// the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
// The U.S. Government is granted a limited license as per AL 91-7.
//

#include <string>
#include <sstream>
#include <util/misc/exception.h>
#include <util/misc/exenv.h>

using namespace std;
using namespace sc;

namespace sc {
  namespace detail {
    std::string to_string(int i) {
      ostringstream oss;
      oss << i;
      return oss.str();
    }
  }
}

////////////////////////////////////////////////////////////////////////
// Exception

Exception::Exception(const char *description,
                     const char *file,
                     int line) MPQC__NOEXCEPT:
  std::runtime_error(
    std::string(description ? description :
        (file ? "exception at "
          : "(no description or file information given for Exception)"
        ))
    + std::string(description ? (file ? ", at " : "") : "")
    + std::string(file ? (std::string(file) + ":" + sc::detail::to_string(line)) : "")
  ),
  description_(description),
  file_(file),
  line_(line)
{
}

Exception::Exception(const Exception& ref) MPQC__NOEXCEPT:
    std::runtime_error(ref.what()),
    description_(ref.description_),
    file_(ref.file_),
    line_(ref.line_)
{
}

Exception::~Exception() MPQC__NOEXCEPT
{
  try{ ExEnv::out0().flush(); ExEnv::err0().flush(); }
  catch(...) {}
}

//const char*
//Exception::what() const MPQC__NOEXCEPT
//{
//  try {
//      std::ostringstream oss;
//      if (description_) {
//        oss << "Exception: " << description_ << std::endl;
//      }
//      if (file_) {
//        oss   << "Exception: location = " << file_ << ":" << line_ << std::endl;
//      }
//      if (description_ || file_)
//        return oss.str().c_str();
//      else
//        return "No description or file information given for Exception";
//    }
//  catch (...) {}

//  return "No information available for Exception";
//}
