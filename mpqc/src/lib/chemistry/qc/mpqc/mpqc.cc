
#include "mpqc.h"

extern "C" {

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <tmpl.h>
#include <util/bio/libbio.h>
#include <comm/picl/picl.h>
#include <comm/picl/ext/piclext.h>
#include <util/misc/libmisc.h>

#if defined(I860) || defined(SGI)
int led(int);
void bzero(void*,int);
#endif
};

#include <util/keyval/keyval.h>
#include <util/sgen/sgen.h>
#include <util/misc/libmisc.h>
#include <math/scmat/matrix.h>
#include <chemistry/molecule/molecule.h>
#include <chemistry/qc/force/libforce.h>
#include <chemistry/qc/dmtscf/scf_dmt.h>

#define ioff(i) ((i)*((i)+1)/2)
#define IOFF(i,j) ((i)>(j)) ? ioff((i))+(j) : ioff((j))+(i)

static void clean_and_exit(int);
static void mkcostvec(centers_t*, sym_struct_t*, dmt_cost_t*);

int host;
char* argv0;
int MPSCF::active = 0;

#define CLASSNAME MPSCF
#define PARENTS public OneBodyWavefunction
#define HAVE_KEYVAL_CTOR
#define HAVE_STATEIN_CTOR
#include <util/state/statei.h>
#include <util/class/classi.h>
void *
MPSCF::_castdown(const ClassDesc*cd)
{
  void* casts[1];
  casts[0] = OneBodyWavefunction::_castdown(cd);
  return do_castdowns(casts,cd);
}

// This grabs the molecule, computes the symmetry redundant centers,
// sticks it back into the keyval input, and gives it to the base
// class initializer.  The temporary keyvals created must be free'ed
// by assigned nil to MPSCF_CTOR_hack_keyval.
static RefKeyVal MPSCF_CTOR_hack_keyval;
static KeyVal&
MPSCF_CTOR_hack(KeyVal&keyval,
                scf_struct_t *_scf_info,
                sym_struct_t *_sym_info,
                scf_irreps_t *_irreps,
                centers_t *_centers,
                FILE *outfile)
{
  RefMolecule uniq_molecule= keyval.describedclassvalue("molecule");
  RefGaussianBasisSet gbs = keyval.describedclassvalue("basis");
  centers_t *uniq_centers = gbs->convert_to_centers_t(uniq_molecule);
  int errcod = scf_get_keyval_input(keyval,uniq_centers,
                                    _scf_info,
                                    _sym_info,
                                    _irreps,
                                    _centers,
                                    outfile);
  if(errcod != 0) {
      fprintf(outfile,"trouble reading input\n");
      clean_and_exit(host);
    }
  free_centers(uniq_centers);

  // if the symmetry routines added atoms i've now got a huge mess
  // to take care of.
  if (uniq_molecule->natom() != _centers->n) {
      RefMolecule molecule = new Molecule;
      for (int i=0; i<_centers->n; i++) {
          AtomicCenter ac(_centers->center[i].atom,
                          _centers->center[i].r[0],
                          _centers->center[i].r[1],
                          _centers->center[i].r[2]
              );
          molecule->add_atom(i,ac);
        }

      // put the correct molecule in the input
      AssignedKeyVal * assignedkeyval = new AssignedKeyVal;
      assignedkeyval->assign("molecule",molecule);
      MPSCF_CTOR_hack_keyval = new AggregateKeyVal(*assignedkeyval,keyval);

      // the basis's molecule is also wrong, so fix it too
      assignedkeyval->assign("basis:molecule",molecule);
      // read in the bad basis so i can get its class descriptor
      RefDescribedClass dc = keyval.describedclassvalue("basis");
      // now create a good basis with a keyval that has the correct molecule
      RefKeyVal pkv = new PrefixKeyVal("basis",*MPSCF_CTOR_hack_keyval);
      dc = dc->class_desc()->create(*pkv);
      pkv = 0;
      assignedkeyval->assign("basis",dc);

      // also fix coor's molecule (if coor exists)
      if (keyval.exists("coor")) {
          assignedkeyval->assign("coor:molecule",molecule);
          // read in the bad coor so i can get its class descriptor
          RefDescribedClass dc = keyval.describedclassvalue("coor");
          // now create a good coor with a keyval that has the correct molecule
          RefKeyVal pkv = new PrefixKeyVal("coor",*MPSCF_CTOR_hack_keyval);
          dc = dc->class_desc()->create(*pkv);
          pkv = 0;
          assignedkeyval->assign("coor",dc);
        }
      return *MPSCF_CTOR_hack_keyval;
    }
  else {
      return keyval;
    }
}

MPSCF::MPSCF(KeyVal&keyval):
  OneBodyWavefunction(MPSCF_CTOR_hack(keyval,
                                      &scf_info,
                                      &sym_info,
                                      &irreps,
                                      &centers,
                                      stdout)),
  _exchange_energy(this),
  _eigenvectors(this),
  _scf(this)
{
  // get rid of the temporary keyval needed to initialize the base classes
  MPSCF_CTOR_hack_keyval = 0;

  // override the default thresholds
  if (!keyval.exists("value_accuracy")) {
      set_value_accuracy(1.0e-10);
    }
  if (!keyval.exists("gradient_accuracy")) {
      set_gradient_accuracy(1.0e-9);
    }
  if (!keyval.exists("hessian_accuracy")) {
      set_hessian_accuracy(1.0e-8);
    }

  // make sure only one MPSCF object exists
  if (active) {
      fprintf(stderr,"only one MPSCF allowed\n");
      abort();
    }
  active = 1;
  
  int i;
  int errcod;
  int nproc,me,top,ord,dir;
  int localp;
  int nshell,nshtr;
  int throttle,geom_code=-1,sync_loop;
  int size,count;
  int *shellmap;
  double energy;
  dmt_cost_t* costvec;
  int mp2;

  char *filename="mpqc";
  char **atom_labels;
  char fockfile[512],fockofile[512],oldvecfile[512];
  FILE *infile;

  int restart;

  struct stat stbuf;

  double_matrix_t gradient;

/* turn out the lights */

#if defined(I860)
  led(0);
#endif

/* some PICL emulators need argv0 */
  argv0 = "bad news: argv0 not available -- MPSCF";

/* get some info from the host program */

  open0(&nproc,&me,&host);
  setarc0(&nproc,&top,&ord,&dir);

/* initialize timing for mpqc */

  tim_enter("mpqcnode");
  tim_enter("input");

/* set up i/o. node 0 will do all the reading and pass what's needed on to
 * the other nodes 
 */

  outfile = stdout;

  if(me==0) {
      if(host0()!=0) {
          fprintf(outfile,"\n  loaded from host program\n");
        }
      else {
          fprintf(outfile,
                  "\n      MPSCF: Massively Parallel Quantum Chemistry\n\n\n");
          fprintf(outfile,"  loaded from workstation or SRM\n");
        }
      fprintf(outfile,"  Running on a %s with %d nodes.\n",
              machine_type(),nproc);
      fflush(outfile);

      /* read input, and initialize various structs */

      localp = 0;
      if (keyval.exists("local_P"))
          localp = keyval.booleanvalue("local_P");

      throttle = 0;
      if (keyval.exists("throttle")) throttle = keyval.intvalue("throttle");

      sync_loop = 1;
      if (keyval.exists("sync_loop")) sync_loop = keyval.intvalue("sync_loop");

      node_timings=0;
      if (keyval.exists("node_timings"))
          node_timings = keyval.booleanvalue("node_timings");

      if (keyval.exists("filename"))
          filename = keyval.pcharvalue("filename");

      save_vector=1;
      if (keyval.exists("save_vector"))
        save_vector = keyval.booleanvalue("save_vector");

      restart=1;
      if (keyval.exists("restart"))
          restart = keyval.booleanvalue("restart");

      fprintf(outfile,"\n  mpqc options:\n");
      fprintf(outfile,"    node_timings       = %s\n",(node_timings)?"YES":"NO");
      fprintf(outfile,"    throttle           = %d\n",throttle);
      fprintf(outfile,"    sync_loop          = %d\n",sync_loop);
      fprintf(outfile,"    restart            = %s\n",(restart)?"YES":"NO");
      fprintf(outfile,"    save_vector        = %s\n",(save_vector)?"YES":"NO");
      sprintf(vecfile,"%s.scfvec",filename);
      sprintf(oldvecfile,"%s.oldvec",filename);
      sprintf(fockfile,"%s.fock",filename);
      sprintf(fockofile,"%s.focko",filename);

//       errcod = scf_get_input(&scf_info,&sym_info,&irreps,&centers,outfile);
//       if(errcod != 0) {
//           fprintf(outfile,"trouble reading input\n");
//           clean_and_exit(host);
//         }
    }

  sgen_reset_bcast0();

  bcast0_scf_struct(&scf_info,0,0);
  bcast0_sym_struct(&sym_info,0,0);
  bcast0_centers(&centers,0,0);
  bcast0_scf_irreps(&irreps,0,0);

  bcast0(&restart,sizeof(int),mtype_get(),0);
  bcast0(&save_vector,sizeof(int),mtype_get(),0);
  bcast0(&throttle,sizeof(int),mtype_get(),0);
  bcast0(&sync_loop,sizeof(int),mtype_get(),0);
  bcast0(&node_timings,sizeof(int),mtype_get(),0);
  bcast0(&localp,sizeof(int),mtype_get(),0);

  if(me==0) size=strlen(vecfile)+1;
  bcast0(&size,sizeof(int),mtype_get(),0);
  bcast0(vecfile,size,mtype_get(),0);

  if(me==0) size=strlen(oldvecfile)+1;
  bcast0(&size,sizeof(int),mtype_get(),0);
  bcast0(oldvecfile,size,mtype_get(),0);

  if(me==0) size=strlen(fockfile)+1;
  bcast0(&size,sizeof(int),mtype_get(),0);
  bcast0(fockfile,size,mtype_get(),0);

  if(me==0) size=strlen(fockofile)+1;
  bcast0(&size,sizeof(int),mtype_get(),0);
  bcast0(fockofile,size,mtype_get(),0);

/* initialize centers */

  int_initialize_1e(0,0,&centers,&centers);
  int_initialize_offsets1(&centers,&centers);

/* initialize force and geometry routines */

  if(me==0) fprintf(outfile,"\n");
  if(scf_info.iopen) dmt_force_osscf_keyval_init(&keyval,outfile);
  else dmt_force_csscf_keyval_init(&keyval,outfile);

/* set up shell map for libdmt */

  nshell=centers.nshell;
  nshtr=nshell*(nshell+1)/2;

  shellmap = (int *) malloc(sizeof(int)*nshell);
  bzero(shellmap,sizeof(int)*nshell);

  for(i=0; i < centers.nshell ; i++) shellmap[i] = INT_SH_NFUNC(&centers,i);

  costvec = (dmt_cost_t*) malloc(sizeof(dmt_cost_t)*nshtr);
  mkcostvec(&centers,&sym_info,costvec);
  dmt_def_map2(scf_info.nbfao,centers.nshell,shellmap,costvec,0);
  free(costvec);

  free(shellmap);
  if(me==0) printf("\n");
  dmt_map_examine();

  int_done_offsets1(&centers,&centers);
  int_done_1e();

/* set the throttle for libdmt loops */

  dmt_set_throttle(throttle);

/* set the sync_loop for libdmt loops */

  dmt_set_sync_loop(sync_loop);

/* allocate memory for vector and fock matrices */

  SCF_VEC = dmt_create("scf vector",scf_info.nbfao,COLUMNS);
  FOCK = dmt_create("fock matrix",scf_info.nbfao,SCATTERED);
  if(scf_info.iopen)
      FOCKO = dmt_create("open fock matrix",scf_info.nbfao,SCATTERED);
  else
      FOCKO = dmt_nil();

  if(mynode0() == 0 && save_vector)
      fprintf(outfile,"  scf vector will be written to file %s\n",vecfile);

/* if restart, then read in old scf vector if it exists */

  FILE* test_vec = fopen(vecfile,"r");
  if(test_vec && restart) {
      fclose(test_vec);
      dmt_read(vecfile,SCF_VEC);
      if(me==0) fprintf(outfile,"\n  read vector from file %s\n\n",vecfile);
      scf_info.restart=1;
    }
  else if (test_vec) {
      fclose(test_vec);
    }

  tim_exit("input");
  
}

MPSCF::~MPSCF()
{
  if(scf_info.iopen) dmt_force_osscf_done();
  else dmt_force_csscf_done();
  tim_print(node_timings);
  clean_and_exit(host0());
  active = 0;
}

MPSCF::MPSCF(StateIn&s):
  SavableState(s,class_desc_),
  OneBodyWavefunction(s),
  _exchange_energy(this),
  _eigenvectors(this),
  _scf(this)
{
  // make sure only one MPSCF object exists
  if (active) {
      fprintf(stderr,"only one MPSCF allowed\n");
      abort();
    }
  active = 1;

  abort();
}

void
MPSCF::save_data_state(StateOut&s)
{
  OneBodyWavefunction::save_data_state(s);
  abort();
}


int
MPSCF::do_exchange_energy(int f)
{
  int old = _exchange_energy.compute();
  _exchange_energy.compute() = f;
  return old;
}

int
MPSCF::do_eigenvectors(int f)
{
  int old = _eigenvectors.compute();
  _eigenvectors.compute() = f;
  return old;
}

void
MPSCF::compute()
{
  int i,j;
  int errcod;

  // Adjust the value accuracy if gradients are needed and set up
  // minimal accuracies.
  if (value_accuracy() > 1.0e-4) set_value_accuracy(1.0e-4);
  if (_gradient.compute()) {
      if (gradient_accuracy() > 1.0e-4) set_gradient_accuracy(1.0e-4);
      if (value_accuracy() > 0.1 * gradient_accuracy()) {
          set_value_accuracy(0.1 * gradient_accuracy());
        }
    }

  /* copy geometry from Molecule to centers */
  for (i=0; i<centers.n; i++) {
      for (j=0; j<3; j++) {
          centers.center[i].r[j] = _mol->operator[](i)[j];
        }
    }

  /* broadcast new geometry information */
  for (i=0; i<centers.n; i++) {
      bcast0(centers.center[i].r,sizeof(double)*3,mtype_get(),0);
    }

  // make sure everybody knows if we're to compute a vector
  bcast0(&_scf.compute(),sizeof(int),mtype_get(),0);
  bcast0(&_scf.computed(),sizeof(int),mtype_get(),0);
  bcast0(&_energy.compute(),sizeof(int),mtype_get(),0);
  bcast0(&_energy.computed(),sizeof(int),mtype_get(),0);

  /* calculate new scf_vector */
  if (_scf.needed() || _energy.needed()) {
      tim_enter("scf_vect");

      scf_info.convergence = ((int) -log10(value_accuracy())) + 1;
      scf_info.intcut = scf_info.convergence + 1;
      fprintf(outfile,"MPSCF: computing energy with integral cutoff 10^-%d"
              " and convergence 10^-%d\n",
              scf_info.intcut,scf_info.convergence);
      errcod = scf_vector(&scf_info,&sym_info,&irreps,&centers,
                          FOCK,FOCKO,SCF_VEC,outfile);
      if(errcod != 0) {
          fprintf(outfile,"trouble forming scf vector\n");
          clean_and_exit(host);
        }

      if(save_vector) dmt_write(vecfile,SCF_VEC);

      scf_info.restart=1;
      _scf.computed() = 1;
      set_energy(scf_info.nuc_rep+scf_info.e_elec);

      tim_exit("scf_vect");
    }

  // make sure that everybody knows whether or not a gradient is needed.
  bcast0(&_gradient.compute(),sizeof(int),mtype_get(),0);
  bcast0(&_gradient.computed(),sizeof(int),mtype_get(),0);

  // compute the gradient if needed
  double_matrix_t grad;
  allocbn_double_matrix(&grad,"n1 n2",3,centers.n);
  if(_gradient.needed()) {
      int cutoff = ((int) -log10(gradient_accuracy())) + 1;
      fprintf(outfile,"MPSCF: computing gradient with cutoff 10^-%d\n",cutoff);
      if(!scf_info.iopen) {
          dmt_force_csscf_threshold_10(cutoff);
          dmt_force_csscf(outfile,FOCK,SCF_VEC,
                          &centers,&sym_info,irreps.ir[0].nclosed,&grad);
        }
      else {
          int ndoc=irreps.ir[0].nclosed;
          int nsoc=irreps.ir[0].nopen;
          dmt_force_osscf(outfile,FOCK,FOCKO,SCF_VEC,
                          &centers,&sym_info,ndoc,nsoc,&grad);
        }
      // convert the gradient to a SCVector
      RefSCVector g(_moldim);
      for (int ii=0,i=0; i<centers.n; i++) {
          for (int j=0; j<3; j++,ii++) {
              g(ii) = grad.d[j][i];
            }
        }
      // update the gradient, converting to internal coordinates if needed
      set_gradient(g);
    }

  // compute the hessian if needed
  if (_hessian.needed()) {
      if (mynode0()==0) {
          printf("A hessian was requested, but cannot be computed\n");
          abort();
        }
    }
  
}

double
MPSCF::exchange_energy()
{
  return _exchange_energy;
}

RefSCMatrix
MPSCF::eigenvectors()
{
  return _eigenvectors;
}

double
MPSCF::occupation(int i)
{
  if (i<_nocc) return 2.0;
  else return 0.0;
}

void
MPSCF::print(SCostream&o)
{
  if (outfile) fflush(outfile);
  o.flush();
  OneBodyWavefunction::print(o);
  o.flush();
}

static void
clean_and_exit(int host)
{
  int junk;

#if !defined(I860)
  picl_prober();

  /* tell host that we're done */
  if(mynode0()==0 && host0()) send0(&junk,sizeof(int),1,host);
#endif

 /* this one too */
  close0(0);
  exit(0);
}

static void
mkcostvec(centers_t *centers,sym_struct_t *sym_info,dmt_cost_t *costvec)
{
  int flags;
  int i,j,k,l;
  int ij,kl,ijkl;
  int ioffi,ioffij;
  int g,gi,gj,gk,gl,gij,gkl,gijkl;
  int Qvecij,bound,cost;
  int nb;
  int leavel;
  int use_symmetry=(sym_info->g>1);
  int nproc=numnodes0();
  int me=mynode0();
  double *intbuf;
  extern signed char *Qvec;

 /* free these up for now */
  int_done_offsets1(centers,centers);
  int_done_1e();

  int_initialize_offsets2(centers,centers,centers,centers);

  flags = INT_EREP|INT_NOSTRB|INT_NOSTR1|INT_NOSTR2;

  intbuf =
    int_initialize_erep(flags,0,centers,centers,centers,centers);

  scf_init_bounds(centers,intbuf);

  for (i=ij=0; i<centers->nshell; i++) {
    int nconi = centers->center[centers->center_num[i]].
                       basis.shell[centers->shell_num[i]].ncon;
    int nprimi = centers->center[centers->center_num[i]].
                       basis.shell[centers->shell_num[i]].nprim;
    int ami = centers->center[centers->center_num[i]].
                       basis.shell[centers->shell_num[i]].type[0].am;
    for (j=0; j<=i; j++,ij++) {
      int nconj = centers->center[centers->center_num[j]].
                         basis.shell[centers->shell_num[j]].ncon;
      int nprimj = centers->center[centers->center_num[j]].
                         basis.shell[centers->shell_num[j]].nprim;
      int amj = centers->center[centers->center_num[j]].
                         basis.shell[centers->shell_num[j]].type[0].am;

      costvec[ij].i = i;
      costvec[ij].j = j;
      costvec[ij].magnitude = (int) Qvec[ij];
      costvec[ij].ami = ami;
      costvec[ij].amj = amj;
      costvec[ij].nconi = nconi;
      costvec[ij].nconj = nconj;
      costvec[ij].nprimi = nprimi;
      costvec[ij].nprimj = nprimj;
      costvec[ij].dimi = INT_SH_NFUNC(centers,i);;
      costvec[ij].dimj = INT_SH_NFUNC(centers,j);;
      }
    }

  int_done_erep();
  int_done_offsets2(centers,centers,centers,centers);
  int_initialize_1e(0,0,centers,centers);
  int_initialize_offsets1(centers,centers);
  scf_done_bounds();
  }
