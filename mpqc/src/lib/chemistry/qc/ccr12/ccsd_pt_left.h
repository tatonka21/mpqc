//
// ccsd_pt_left.h -- a numerator of singles in the (T) correction to CCSD
//
// Copyright (C) 2009 Toru Shiozaki
//
// Author: Toru Shiozaki <shiozaki@qtp.ufl.edu>
// Maintainer: TS
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

#ifndef _chemistry_qc_ccr12_ccsd_pt_left_h
#define _chemistry_qc_ccr12_ccsd_pt_left_h

#include <chemistry/qc/ccr12/ccr12_info.h>
#include <chemistry/qc/ccr12/ptnum.h>

namespace sc {

class CCSD_PT_LEFT : public PTNum {

  protected:
   void smith_0_1(double**, const long,const long,const long,const long,const long,const long);

  public:
   CCSD_PT_LEFT(CCR12_Info* info) : PTNum(info) {};
   ~CCSD_PT_LEFT() {};
    
   void compute_amp(double**,const long,const long,const long,const long,const long,const long,const long);

};

}

#endif


