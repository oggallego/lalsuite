/*
 *  Copyright (C) 2008, 2009 Jordi Burguet-Castell, Xavier Siemens
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with with program; see the file COPYING. If not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 *  MA  02111-1307  USA
 */

#ifndef _COMPUTE_DATA_QUALITY_VECTOR_H
#define _COMPUTE_DATA_QUALITY_VECTOR_H

#include <lal/LALDatatypes.h>
#include <lal/BandPassTimeSeries.h>

NRCSID (COMPUTE_DATA_QUALITY_VECTORH,"$Id$");

#ifdef  __cplusplus
extern "C" {
#pragma } /** to match the previous brace **/
#endif

int XLALComputeDQ(int* sv_data, int n_sv,
                  float* lax_data, float* lay_data, int n_light,
                  COMPLEX16* gamma_data, int n_gamma,
                  int transient, int missing,
                  int* dq_data, int n_dq);



#ifdef  __cplusplus
#pragma { /** to match the next brace **/
}
#endif

#endif /* _COMPUTE_DATA_QUALITY_VECTOR_H */
