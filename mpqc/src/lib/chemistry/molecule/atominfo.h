//
// atominfo.h
//
// Copyright (C) 1996 Limit Point Systems, Inc.
//
// Author: Curtis Janssen <cljanss@limitpt.com>
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

#ifndef _chemistry_molecule_atominfo_h
#define _chemistry_molecule_atominfo_h

#include <string>
#include <map>
#include <vector>

#include <util/class/class.h>
#include <util/keyval/keyval.h>

namespace sc {

class Units;

/** The AtomInfo class provides information about atoms.  The information
    is kept in a file named atominfo.kv in the SC library directory.  That
    information can be overridden by the user. */
class AtomInfo: public SavableState {
  private:
    enum { Nelement = 118, DefaultZ = 0 };

    struct atom
    {
      int Z;
      char *name;
      char *symbol;
    };

    static struct atom elements_[Nelement];

    std::map<std::string,int> name_to_Z_;
    std::map<std::string,int> symbol_to_Z_;
    std::map<int,std::string> Z_to_names_;
    std::map<int,std::string> Z_to_symbols_;
    std::map<int,double> Z_to_mass_;
    std::map<int,double> Z_to_atomic_radius_;
    std::map<int,double> Z_to_vdw_radius_;
    std::map<int,double> Z_to_bragg_radius_;
    std::map<int,double> Z_to_maxprob_radius_;
    std::map<int,std::vector<double> > Z_to_rgb_;
    std::map<int,double> Z_to_ip_;
    double  atomic_radius_scale_;
    double  vdw_radius_scale_;
    double  bragg_radius_scale_;
    double  maxprob_radius_scale_;

    char *overridden_values_;

    void load_library_values();
    void override_library_values(const Ref<KeyVal> &keyval);
    void load_values(const Ref<KeyVal>& keyval, int override);
    void load_values(std::map<int,double>&,
                     double *scale, const char *keyword,
                     const Ref<KeyVal> &keyval, int override,
                     const Ref<Units> &);
    void load_values(std::map<int,std::vector<double> >&,
                     const char *keyword,
                     const Ref<KeyVal> &keyval, int override);
    void add_overridden_value(const char *assignment);
    void initialize_names();
    double lookup_value(const std::map<int,double>& values, int Z) const;
    double lookup_array_value(const std::map<int,std::vector<double> >& values,
                              int Z, int i) const;
  public:
    AtomInfo();
    AtomInfo(const Ref<KeyVal>&);
    AtomInfo(StateIn&);
    ~AtomInfo();
    void save_data_state(StateOut& s);

    /// These return various measures of the atom's radius.
    double vdw_radius(int Z) const;
    double bragg_radius(int Z) const;
    double atomic_radius(int Z) const;
    double maxprob_radius(int Z) const;

    /// Returns the atomization potential for atomic number Z.
    double ip(int Z) const;

    /// Return the scale factor for the VdW radii.
    double vdw_radius_scale() const { return vdw_radius_scale_; }
    /// Return the scale factor for the Bragg radii.
    double bragg_radius_scale() const { return bragg_radius_scale_; }
    /// Return the scale factor for the atomic radii.
    double atomic_radius_scale() const { return atomic_radius_scale_; }
    /// Return the scale factor for the maximum probability radii.
    double maxprob_radius_scale() const { return maxprob_radius_scale_; }

    /** These return information about the color of the atom
        for visualization programs. */
    double rgb(int Z, int color) const;
    double red(int Z) const;
    double green(int Z) const;
    double blue(int Z) const;

    /// This returns the mass of the most abundant isotope.
    double mass(int Z) const;

    /// This returns the full name of the element.
    std::string name(int Z);
    /// This returns the symbol for the element.
    std::string symbol(int Z);

    /// This converts a name or symbol to the atomic number.
    int string_to_Z(const std::string &, int allow_exceptions = 1);
};

}

#endif

// Local Variables:
// mode: c++
// c-file-style: "CLJ"
// End:
