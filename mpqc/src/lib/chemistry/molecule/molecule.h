//
// molecule.h
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

#ifndef _chemistry_molecule_molecule_h
#define _chemistry_molecule_molecule_h

#ifdef __GNUC__
#pragma interface
#endif

#include <stdio.h>
#include <iostream>
#include <util/class/class.h>
#include <util/state/state.h>
#include <util/keyval/keyval.h>
#include <util/misc/units.h>
#include <math/symmetry/pointgrp.h>
#include <math/scmat/vector3.h>
#include <math/scmat/matrix.h>
#include <chemistry/molecule/atominfo.h>

namespace sc {

/**
The Molecule class contains information about molecules.  It has a
KeyVal constructor that can create a new molecule from either a
PDB file or from a list of Cartesian coordinates.

The following ParsedKeyVal input reads from the PDB
file <tt>h2o.pdb</tt>:
<pre>
molecule<Molecule>: (
   pdb_file = "h2o.pdb"
 )
</pre>

The following input explicitly gives the atom coordinates, using the
ParsedKeyVal table notation:
<pre>
molecule<Molecule>: (
    unit=angstrom
    { atom_labels atoms           geometry            } = {
          O1         O   [ 0.000000000 0  0.369372944 ]
          H1         H   [ 0.783975899 0 -0.184686472 ]
          H2         H   [-0.783975899 0 -0.184686472 ]
     }
    )
  )
</pre>
The default units are Bohr with can be overridden with
#unit=angstrom#.  The #atom_labels# array can be
omitted.  The #atoms# and #geometry# arrays
are required.

The Molecule class has a PointGroup
member object, which also has a KeyVal constructor
that is called when a Molecule is made.  The
following example constructs a molecule with \f$C_{2v}\f$ symmetry:
<pre>
molecule<Molecule>: (
    symmetry=c2v
    unit=angstrom
    { atoms         geometry            } = {
        O   [0.000000000 0  0.369372944 ]
        H   [0.783975899 0 -0.184686472 ]
     }
    )
  )
</pre>
Only the symmetry unique atoms need to be specified.  Nonunique
atoms can be given too, however, numerical errors in the
geometry specification can result in the generation of extra
atoms so be careful.
*/
class Molecule: public SavableState
{
  protected:
    int natoms_;
    Ref<AtomInfo> atominfo_;
    Ref<PointGroup> pg_;
    Ref<Units> geometry_units_;
    double **r_;
    int *Z_;
    double *charges_;

    // symmetry equiv info
    int nuniq_;
    int *nequiv_;
    int **equiv_;
    int *atom_to_uniq_;
    void init_symmetry_info(double tol=0.5);
    void clear_symmetry_info();

    // these are optional
    double *mass_;
    char **labels_;

    void clear();
  public:
    Molecule();
    Molecule(const Molecule&);
    Molecule(StateIn&);
    /// The KeyVal constructor.
    Molecule(const Ref<KeyVal>&input);

    virtual ~Molecule();

    Molecule& operator=(const Molecule&);

    /// Add an AtomicCenter to the Molecule.
    void add_atom(int Z,double x,double y,double z,
                  const char * = 0, double mass = 0.0,
                  int have_charge = 0, double charge = 0.0);

    /// Print information about the molecule.
    virtual void print(std::ostream& =ExEnv::out0()) const;
    virtual void print_parsedkeyval(std::ostream& =ExEnv::out0(),
                                    int print_pg = 1,
                                    int print_unit = 1,
                                    int number_atoms = 1) const;

    /// Returns the number of atoms in the molcule.
    int natom() const { return natoms_; }

    int Z(int atom) const { return Z_[atom]; }
    double &r(int atom, int xyz) { return r_[atom][xyz]; }
    const double &r(int atom, int xyz) const { return r_[atom][xyz]; }
    double *r(int atom) { return r_[atom]; }
    const double *r(int atom) const { return r_[atom]; }
    double mass(int atom) const;
    /** Returns the label explicitly assigned to atom.  If
        no label has been assigned, then null is returned. */
    const char *label(int atom) const;

    /** Takes an (x, y, z) postion and finds an atom within the
        given tolerance distance.  If no atom is found -1 is returned. */
    int atom_at_position(double *, double tol = 0.05) const;

    /** Returns the index of the atom with the given label.
        If the label cannot be found -1 is returned. */
    int atom_label_to_index(const char *label) const;

    /** Returns a double* containing the nuclear
        charges of the atoms.  The caller is responsible for
        freeing the return value. */
    double *charges() const;

    /// Return the charge of the atom.
    double charge(int iatom) const;

    /// Returns the total nuclear charge.
    double nuclear_charge() const;

    /// Sets the PointGroup of the molecule.
    void set_point_group(const Ref<PointGroup>&, double tol=1.0e-7);
    /// Returns the PointGroup of the molecule.
    Ref<PointGroup> point_group() const;

    /** Find this molecules true point group (limited to abelian groups).
        If the point group of this molecule is set to the highest point
        group, then the origin must first be set to the center of mass. */
    Ref<PointGroup> highest_point_group(double tol = 1.0e-8) const;

    /** Return 1 if this given axis is a symmetry element for the molecule.
        The direction vector must be a unit vector. */
    int is_axis(SCVector3 &origin,
                SCVector3 &udirection, int order, double tol=1.0e-8) const;

    /** Return 1 if the given plane is a symmetry element for the molecule.
        The perpendicular vector must be a unit vector. */
    int is_plane(SCVector3 &origin, SCVector3 &uperp, double tol=1.0e-8) const;

    /// Return 1 if the molecule has an inversion center.
    int has_inversion(SCVector3 &origin, double tol = 1.0e-8) const;

    /// Returns 1 if the molecule is linear, 0 otherwise.
    int is_linear(double tolerance = 1.0e-5) const;
    /// Returns 1 if the molecule is planar, 0 otherwise.
    int is_planar(double tolerance = 1.0e-5) const;
    /** Sets linear to 1 if the molecular is linear, 0 otherwise.
        Sets planar to 1 if the molecular is planar, 0 otherwise. */
    void is_linear_planar(int&linear,int&planar,double tol = 1.0e-5) const;

    /** Returns a SCVector3 containing the cartesian coordinates of
        the center of mass for the molecule. */
    SCVector3 center_of_mass() const;

    /// Returns the nuclear repulsion energy for the molecule
    double nuclear_repulsion_energy();
    
    /** Compute the nuclear repulsion energy first derivative with respect
        to the given center. */
    void nuclear_repulsion_1der(int center, double xyz[3]);

    /// Compute the electric field due to the nuclei at the given point.
    void nuclear_efield(const double *position, double* efield);

    /** Compute the electric field due to the given charges at the
        positions of the nuclei at the given point. */
    void nuclear_charge_efield(const double *charges,
                               const double *position, double* efield);
    
    /** If the molecule contains only symmetry unique atoms, this function
        will generate the other, redundant atoms.  The redundant atom
        will only be generated if there is no other atoms within a distance
        of tol.  If the is another atom and it is not identical, then
        abort will be called. */
    void symmetrize(double tol = 0.5);

    /// Set the point group and then symmetrize.
    void symmetrize(const Ref<PointGroup> &pg, double tol = 0.5);

    /** This will try to carefully correct symmetry errors
        in molecules.  If any atom is out of place by more then
        tol, abort will be called. */
    void cleanup_molecule(double tol = 0.1);

    void translate(const double *r);
    void move_to_com();
    void transform_to_principal_axes(int trans_frame=1);
    void transform_to_symmetry_frame();
    void print_pdb(std::ostream& =ExEnv::out0(), char *title =0) const;

    void read_pdb(const char *filename);

    /** Compute the principal moments of inertia and, possibly, the
        principal axes. */
    void principal_moments_of_inertia(double *evals, double **evecs=0) const;

    /// Return information about symmetry unique and equivalent atoms.
    int nunique() const { return nuniq_; }
    /// Returns the overall number of the iuniq'th unique atom.
    int unique(int iuniq) const { return equiv_[iuniq][0]; }
    /// Returns the number of atoms equivalent to iuniq.
    int nequivalent(int iuniq) const { return nequiv_[iuniq]; }
    /// Returns the j'th atom equivalent to iuniq.
    int equivalent(int iuniq, int j) const { return equiv_[iuniq][j]; }
    /** Converts an atom number to the number of its generating unique atom.
        The return value is in [0, nunique). */
    int atom_to_unique(int iatom) const { return atom_to_uniq_[iatom]; }
    /** Converts an atom number to the offset of this atom in the list of
        generated atoms. The unique atom itself is allows offset 0.  */
    int atom_to_unique_offset(int iatom) const;

    /// Return the number of core electrons.
    int n_core_electrons();

    /// Return the maximum atomic number.
    int max_z();

    /// Return the molecules AtomInfo object.
    Ref<AtomInfo> atominfo() const { return atominfo_; }

    /// Returns the element name of the atom.
    std::string atom_name(int iatom) const;

    /// Returns the element symbol of the atom.
    std::string atom_symbol(int iatom) const;

    void save_data_state(StateOut&);
};

}

#endif

// Local Variables:
// mode: c++
// c-file-style: "CLJ"
// End:
