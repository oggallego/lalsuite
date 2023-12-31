/*
 *  Copyright (C) 2012 Chris Pankow, Evan Ochsner
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
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301  USA
 */

/**
 * \author Chris Pankow
 *
 * \file
 *
 * \brief Testing constant precession code on IMRPhenomB waveform.
 */


#include <lal/LALSimInspiralPrecess.h>

int main(void){

  FILE* h_ref = fopen("h_ref_PhenomB.txt", "w");
  FILE* h_rot = fopen("h_rot_PhenomB.txt", "w");

  int ret;
  unsigned int i;

  // Waveform parameters
  double m1 = 2.0*LAL_MSUN_SI, m2 = 5.0*LAL_MSUN_SI;
  double s1x = 0.0, s1y = 0.0, s1z = 0.0;
  double s2x = 0.0, s2y = 0.0, s2z = 0.0;
  double f_min = 40.0, f_ref = 0.0, dist = 1e6*LAL_PC_SI, inc = LAL_PI_4;
  double lambda1 = 0.0, lambda2 = 0.0;
  double phi = 0.0, dt = 1/16384.0;
  LALSimInspiralWaveformFlags *waveFlags = NULL;
  LALSimInspiralTestGRParam *nonGRparams = NULL;
  int amplitudeOrder = 0, phaseOrder = 7;
  Approximant approximant = IMRPhenomB;

  double view_th = 0.0, view_ph = 0.0;
  double Y_2_m2 = XLALSpinWeightedSphericalHarmonic( view_th + LAL_PI, view_ph, -2, 2, -2), Y_22 = XLALSpinWeightedSphericalHarmonic( view_th, view_ph, -2, 2, 2);

  REAL8TimeSeries *hp, *hx;
  hp = NULL; hx = NULL;

  ret = XLALSimInspiralChooseTDWaveformOLD(
    &hp, &hx,
    m1, m2,
    s1x, s1y, s1z,
    s2x, s2y, s2z,
    dist, inc,
    phi, 0., 0., 0.,
    dt, f_min, f_ref,
    lambda1, lambda2, 0., 0.,
    waveFlags,
    nonGRparams,
    amplitudeOrder, phaseOrder,
    approximant
    );
  if( ret != XLAL_SUCCESS )
    XLAL_ERROR( XLAL_EFUNC );

  COMPLEX16TimeSeries *h_22, *h_2_2;
  h_22 = NULL; h_2_2 = NULL;
  // Initialize the h_lm REAL8 vectors
  h_2_2 = XLALCreateCOMPLEX16TimeSeries(
    "h_{2-2}",
    &(hp->epoch),
    hp->f0,
    hp->deltaT,
    &(hp->sampleUnits),
    hp->data->length
    );

  // Define h_{2-2}
  for( i=0; i< hp->data->length; i++ ){
    h_2_2->data->data[i] = (hp->data->data[i] + hx->data->data[i] * I) / Y_2_m2;
  }

  XLALDestroyREAL8TimeSeries( hp );
  XLALDestroyREAL8TimeSeries( hx );
  hp = NULL; hx = NULL;

  inc=0;  // +z axis
  ret = XLALSimInspiralChooseTDWaveformOLD(
    &hp, &hx,
    m1, m2,
    s1x, s1y, s1z,
    s2x, s2y, s2z,
    dist, inc,
    phi, 0., 0., 0.,
    dt, f_min, f_ref,
    lambda1, lambda2, 0., 0.,
    waveFlags,
    NULL, // non-GR params
    amplitudeOrder, phaseOrder,
    approximant
    );
  if( ret != XLAL_SUCCESS )
    XLAL_ERROR( XLAL_EFUNC );


  // Initialize the h_lm REAL8 vectors
  h_22 = XLALCreateCOMPLEX16TimeSeries(
    "h_{22}",
    &(hp->epoch),
    hp->f0,
    hp->deltaT,
    &(hp->sampleUnits),
    hp->data->length
    );

  // Define h_{22}
  for( i=0; i< hp->data->length; i++ ){
    h_22->data->data[i] = (hp->data->data[i] + hx->data->data[i] * I) / Y_22;
  }

  // Write out reference waveform
  REAL8 t0 = XLALGPSGetREAL8(&(hp->epoch));
  for(i=0; i<hp->data->length; i++)
    fprintf( h_ref, "%g %g %g\n", t0 + i * hp->deltaT,
             hp->data->data[i], hx->data->data[i] );

  XLALSimInspiralDestroyWaveformFlags( waveFlags );

  SphHarmTimeSeries *ts = NULL;
  ts = XLALSphHarmTimeSeriesAddMode( ts, h_22, 2, 2 );
  ts = XLALSphHarmTimeSeriesAddMode( ts, h_2_2, 2, -2 );

  ret = XLALSimInspiralConstantPrecessionConeWaveform(
    &hp, &hx,
    ts,
    10, LAL_PI/4, 0,
    0, LAL_PI/4 );
  if( ret != XLAL_SUCCESS )
    XLAL_ERROR( XLAL_EFUNC );

  XLALDestroyCOMPLEX16TimeSeries( h_22 );
  XLALDestroyCOMPLEX16TimeSeries( h_2_2 );
  XLALDestroySphHarmTimeSeries( ts );

  // Write out rotated waveform
  for(i=0; i<hp->data->length; i++)
    fprintf( h_rot, "%g %g %g\n", t0 + i * hp->deltaT,
             hp->data->data[i], hx->data->data[i] );

  // We're done.
  XLALDestroyREAL8TimeSeries( hp );
  XLALDestroyREAL8TimeSeries( hx );

  fclose(h_ref);
  fclose(h_rot);

  return 0;
}
