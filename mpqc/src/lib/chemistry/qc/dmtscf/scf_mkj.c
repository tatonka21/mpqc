
#include <stdio.h>
#include <stdlib.h>
#include <tmpl.h>
#include <comm/picl/picl.h>
#include <math/array/math_lib.h>
#include <util/misc/libmisc.h>
#include <math.h>
#include <chemistry/qc/intv2/int_libv2.h>
#include <chemistry/qc/dmtsym/sym_dmt.h>
#include "scf.h"
#include "scf_bnd.gbl"

#ifdef INT_CE_SH_AM
#undef INT_CE_SH_AM
#endif
#define INT_CE_SH_AM(c,a,s) ((c)->center[(a)].basis.shell[(s)].type[0].am)

#define ioff(i) (((i)*((i)+1))>>1)
#define IOFF(a,b) (((a)>(b))?(ioff(a)+(b)):(ioff(b)+(a)))

extern signed char *Qvec;

#include "scf_mkj.gbl"
#include "scf_mkj.lcl"

GLOBAL_FUNCTION int
scf_make_j_d(_centers,_irreps,_scf_info,_sym_info,
                 _gmat,_dpmat,maxp,_mgdbuff,iter,_outfile)
centers_t *_centers;
scf_irreps_t *_irreps;
scf_struct_t *_scf_info;
sym_struct_t *_sym_info;
double_vector_t *_gmat;
double_vector_t *_dpmat;
signed char *maxp;
double *_mgdbuff;
int iter;
FILE *_outfile;
{
  register int tmax,imax,cpmax,pmaxijk;
  int pmaxik,pmaxjk,pmaxij,Qvecij;
  int i,j,k,l;
  int ij,kl,ijkl;
  int g,gij,gkl,gijkl;
  int n1,n2,n3,n4;
  int e12,e34,e13e24,e_any;
  int bf1,bf2,bf3,bf4;
  int i1,j1,k1,l1;
  int i2,j2,k2,l2;
  int ii,jj,kk,ll;
  int ij1;
  int lij,lkl;
  int index;
  int nijkl,leavel,qijkl=1;
  int int_index,kindex;
  int nproc=numnodes0();
  int me=mynode0();
  int s1,s2,s3,s4;
  double tnint=0.0;
  double pki_int,value;
  double *gtmp,*ptmp,*ptmpo,*gtmpo;
  int inttol = (int) ((double) -(_scf_info->intcut)/log10(2.0));
  int use_symmetry=(_sym_info->g > 1)?1:0;
  char *shnfunc;

  tim_enter("scf_mkgj");

  gtmp = _gmat->d;
  ptmp = _dpmat->d;

 /* form shnfunc array */

  shnfunc = (char *) malloc(_centers->nshell);
  if(shnfunc==NULL) {
    fprintf(_outfile,"could not malloc shnfunc\n");
    return(-1);
    }
  for (i=0; i<_centers->nshell; i++) shnfunc[i]=INT_SH_NFUNC((_centers),i);

 /* loop over all shells, calculate a bunch of integrals from each shell
  * quartet, and stick those integrals where they belong
  */

  kindex=int_index=0;
  for (i=0; i<_centers->nshell; i++) {
    if(use_symmetry) if(!_sym_info->p1[i]) continue;

    for (j=0; j<=i; j++) {
      leavel=0;
      ij = ioff(i)+j;
      if(use_symmetry) if(!_sym_info->lamij[ij]) continue;
      Qvecij=(int)Qvec[ij];
      if(_scf_info->eliminate) pmaxij=maxp[ij];

      for (k=0; k<=i; k++,kindex++) {
        if(kindex%nproc!=me) {
          continue;
          }

        kl=ioff(k);

        tim_enter("l loop");
        for (l=0; l<=(k==i?j:k); l++) {
          if(use_symmetry) {
            ijkl=ioff(ij)+kl;
            nijkl=leavel=0;
            for(g=0; g < _sym_info->g ; g++) {
              gij=IOFF(_sym_info->shell_map[i][g],_sym_info->shell_map[j][g]);
              gkl=IOFF(_sym_info->shell_map[k][g],_sym_info->shell_map[l][g]);
              gijkl = IOFF(gij,gkl);
              if(gijkl > ijkl) leavel=1;
              if(gijkl == ijkl) nijkl++;
              }
            if(leavel) {
              kl++; continue;
              }
            qijkl = _sym_info->g/nijkl;
            }

          imax = (int) Qvec[kl]+Qvecij;

          if(_scf_info->eliminate) {
            cpmax = (maxp[kl]>pmaxij) ? maxp[kl] : pmaxij;

            if(cpmax+imax < inttol) {
              /* If we are trying to save integrals on this node, then
               * int_index must be incremented now. */
              if (_scf_info->int_store) int_index++;
              kl++;
              continue;
              }
            }

          tim_enter("ints");
          s1 = i; s2 = j; s3 = k; s4 = l;

          tim_enter("gd_erep");
          int_erep(INT_EREP|INT_NOBCHK|INT_NOPERM,&s1,&s2,&s3,&s4);
          tim_exit("gd_erep");

          n1 = shnfunc[s1];
          n2 = shnfunc[s2];
          n3 = shnfunc[s3];
          n4 = shnfunc[s4];

         /* Shell equivalency information. */
          e12    = (s2==s1);
          e13e24 = (s3==s1) && (s4==s2);
          e34    = (s4==s3);

          index = 0;

          for (bf1=0; bf1<=INT_MAX1(n1) ; bf1++) {
            i2 = _centers->func_num[s1] + bf1;

            for (bf2=0; bf2<=INT_MAX2(e12,bf1,n2) ; bf2++) {
              j2 = _centers->func_num[s2] + bf2;
              if(i2>=j2) { i1=i2; j1=j2; }
              else { i1=j2; j1=i2; }
              ij1=ioff(i1)+j1;

              for (bf3=0; bf3<=INT_MAX3(e13e24,bf1,n3) ; bf3++) {
                k2 = _centers->func_num[s3] + bf3;

                for (bf4=0;bf4<=INT_MAX4(e13e24,e34,bf1,bf2,bf3,n4);bf4++){
                  if (INT_NONZERO(_mgdbuff[index])) {
                    l2 = _centers->func_num[s4] + bf4;

                    if(k2>=l2) { k1=k2; l1=l2; }
                    else { k1=l2; l1=k2; }

                    if(ij1 >= ioff(k1)+l1) {
                      ii = i1; jj = j1; kk = k1; ll = l1;
                      }
                    else {
                      ii = k1; jj = l1; kk = i1; ll = j1;
                      }

                    pki_int = (double) qijkl*_mgdbuff[index];

                    lij=ioff(ii)+jj;
                    lkl=ioff(kk)+ll;
                    value=(lij==lkl)? 0.5*pki_int: pki_int;
                    gtmp[lij] += ptmp[lkl]*value;
                    gtmp[lkl] += ptmp[lij]*value;
                    }

                  index++;
                  }
                }
              }
            }
          tnint += (double) (n1*n2*n3*n4);
          tim_exit("ints");

          kl++;
          int_index++;
          }
        tim_exit("l loop");
        }
      }
    }

  if(_scf_info->print_flg & 4) {
    gsum0(&tnint,1,5,mtype_get(),0);
    if(mynode0()==0)
      fprintf(_outfile,"  %8.0f integrals in scf_make_j_d\n",tnint);
    }

  free(shnfunc);

  tim_exit("scf_mkgj");

  return 0;
  }
