#ifndef _PROB_H_
#define _PROB_H_

#include "AMReX_REAL.H"

struct ProbParm {

  amrex::Real u_0     = 0.0;
  amrex::Real v_0     = 0.0;

}; // namespace ProbParm

extern ProbParm parms;

#endif

