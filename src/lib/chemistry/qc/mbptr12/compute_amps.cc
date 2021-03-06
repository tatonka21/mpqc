//
// compute_amps.cc
//
// Copyright (C) 2004 Edward Valeev
//
// Author: Edward Valeev <evaleev@vt.edu>
// Maintainer: EV
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

#include <stdexcept>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include <mpqc_config.h>
#include <util/misc/formio.h>
#include <util/misc/regtime.h>
#include <util/class/class.h>
#include <util/state/state.h>
#include <util/state/state_text.h>
#include <util/state/state_bin.h>
#include <math/scmat/matrix.h>
#include <math/scmat/local.h>
#include <chemistry/molecule/molecule.h>
#include <chemistry/qc/basis/integral.h>
#include <math/distarray4/distarray4.h>
#include <chemistry/qc/mbptr12/r12wfnworld.h>
#include <math/mmisc/pairiter.h>
#include <chemistry/qc/mbptr12/r12int_eval.h>
#include <chemistry/qc/mbptr12/compute_tbint_tensor.h>
#include <chemistry/qc/mbptr12/creator.h>
#include <chemistry/qc/mbptr12/container.h>

using namespace std;
using namespace sc;

void
R12IntEval::compute_T2_(RefSCMatrix& T2,
                        const Ref<OrbitalSpace>& space1,
                        const Ref<OrbitalSpace>& space2,
                        const Ref<OrbitalSpace>& space3,
                        const Ref<OrbitalSpace>& space4,
                        bool antisymmetrize,
                        const std::string& transform_key)
{
  Timer tim("T2 amplitudes");
  std::ostringstream oss;
  oss << "<" << space1->id() << " " << space3->id() << "|T2|"
      << space2->id() << " " << space4->id() << ">";
  const std::string label = oss.str();
  ExEnv::out0() << endl << indent
	       << "Entered MP2 T2 amplitude (" << label << ") evaluator" << endl;
  ExEnv::out0() << incindent;

  std::vector<std::string> tform_keys;  tform_keys.push_back(transform_key);
  compute_tbint_tensor<ManyBodyTensors::ERI_to_T2,false,false>(
    T2, corrfactor()->tbint_type_eri(),
    space1, space2,
    space3, space4,
    antisymmetrize,
    tform_keys
  );

  ExEnv::out0() << decindent << indent << "Exited MP2 T2 amplitude (" << label << ") evaluator" << endl;
}


void
R12IntEval::compute_F12_(RefSCMatrix& F12,
                        const Ref<OrbitalSpace>& space1,
                        const Ref<OrbitalSpace>& space2,
                        const Ref<OrbitalSpace>& space3,
                        const Ref<OrbitalSpace>& space4,
                        bool antisymmetrize,
                        const std::vector<std::string>& transform_keys)
{
  Timer tim("F12 integrals");
  std::ostringstream oss;
  oss << "<" << space1->id() << " " << space3->id() << "|F12|"
      << space2->id() << " " << space4->id() << ">";
  const std::string label = oss.str();
  ExEnv::out0() << endl << indent
                << "Entered F12 integrals (" << label << ") evaluator" << endl
                << incindent;

  compute_tbint_tensor<ManyBodyTensors::I_to_T,true,false>(
    F12, corrfactor()->tbint_type_f12(),
    space1, space2,
    space3, space4,
    antisymmetrize,
    transform_keys
  );

  ExEnv::out0() << decindent << indent << "Exited F12 integrals (" << label << ") evaluator" << endl;
}

////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// c-file-style: "CLJ-CONDENSED"
// End:
