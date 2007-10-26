/*
 *  Copyright (C) 2007 Badri Krishnan  
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



#include "./radiometer.h"
/* lalapps includes */
#include <lalapps.h>
#include <lal/DopplerScan.h>
#include <gsl/gsl_permutation.h>

RCSID( "$Id$");



/* globals, constants and defaults */


extern int lalDebugLevel;


#define EARTHEPHEMERIS "/home/badkri/lscsoft/share/lal/earth05-09.dat"
#define SUNEPHEMERIS "/home/badkri/lscsoft/share/lal/sun05-09.dat"

#define F0 100
#define FBAND 1

#define BLOCKSRNGMED 51
#define MAXFILENAMELENGTH 512 /* maximum # of characters  of a filename */

#define DIROUT "./out"   /* output directory */
#define BASENAMEOUT "radio"    /* prefix file output */

#define SKYFILE "./skypatchfile"      
#define SKYREGION "allsky" 

#define TRUE (1==1)
#define FALSE (1==0)


void CorrelateSingleSFTPair(LALStatus           *status,
			    SingleSFTpair       *out,
			    PulsarDopplerParams *par);


int main(int argc, char *argv[]){

  /* LALStatus pointer */
  static LALStatus  status;  
  
  /* sft related variables */ 
  MultiSFTVector *inputSFTs = NULL;
  REAL8 deltaF, timeBase; 
  INT8  fLastBin, f0Bin;
  UINT4 binsSFT, numSearchBins, numsft;

  SFTPairVec sftPairs;
  SFTPairParams pairParams;
  
  /* information about all the ifos */
  MultiDetectorStateSeries *mdetStates = NULL;
  UINT4 numifo;

  LIGOTimeGPS firstTimeStamp, lastTimeStamp;
  REAL8 tObs;
  REAL8 alpha, delta, patchSizeX, patchSizeY;

  /* ephemeris */
  EphemerisData    *edat=NULL;

  /* skypatch info */
  REAL8  *skyAlpha=NULL, *skyDelta=NULL, *skySizeAlpha=NULL, *skySizeDelta=NULL; 
  INT4   nSkyPatches, skyCounter=0; 
  SkyPatchesInfo skyInfo;


  /* sft constraint variables */
  LIGOTimeGPS startTimeGPS, endTimeGPS;
  LIGOTimeGPSVector *inputTimeStampsVector=NULL;

  /* user input variables */
  BOOLEAN  uvar_help;
  INT4     uvar_blocksRngMed; 
  REAL8    uvar_startTime, uvar_endTime;
  REAL8    uvar_f0, uvar_fBand;
  REAL8    uvar_dAlpha, uvar_dDelta; /* resolution for isotropic sky-grid */
  REAL8    uvar_maxlag;
  CHAR     *uvar_earthEphemeris=NULL;
  CHAR     *uvar_sunEphemeris=NULL;
  CHAR     *uvar_sftDir=NULL;
  CHAR     *uvar_dirnameOut=NULL;
  CHAR     *uvar_fbasenameOut=NULL;
  CHAR     *uvar_skyfile=NULL;
  CHAR     *uvar_timeStampsFile=NULL;
  CHAR     *uvar_skyRegion=NULL;


  /* LAL error-handler */
  lal_errhandler = LAL_ERR_EXIT;
  
  lalDebugLevel = 0;  /* LALDebugLevel must be called before anything else */
  LAL_CALL( LALGetDebugLevel( &status, argc, argv, 'd'), &status);
  
  uvar_maxlag = 0;

  uvar_help = FALSE;
  uvar_blocksRngMed = BLOCKSRNGMED;
  uvar_f0 = F0;
  uvar_fBand = FBAND;
  uvar_dAlpha = 0.2;
  uvar_dDelta = 0.2;

  uvar_earthEphemeris = (CHAR *)LALCalloc( MAXFILENAMELENGTH , sizeof(CHAR));
  strcpy(uvar_earthEphemeris,EARTHEPHEMERIS);

  uvar_sunEphemeris = (CHAR *)LALCalloc( MAXFILENAMELENGTH , sizeof(CHAR));
  strcpy(uvar_sunEphemeris,SUNEPHEMERIS);

  uvar_dirnameOut = (CHAR *)LALCalloc( MAXFILENAMELENGTH , sizeof(CHAR));
  strcpy(uvar_dirnameOut,DIROUT);

  uvar_fbasenameOut = (CHAR *)LALCalloc( MAXFILENAMELENGTH , sizeof(CHAR));
  strcpy(uvar_fbasenameOut,BASENAMEOUT);

  uvar_skyfile = (CHAR *)LALCalloc( MAXFILENAMELENGTH , sizeof(CHAR));
  strcpy(uvar_skyfile,SKYFILE);


  /* register user input variables */
  LAL_CALL( LALRegisterBOOLUserVar( &status, "help",             'h',  UVAR_HELP,     "Print this message", &uvar_help), &status);  
  LAL_CALL( LALRegisterREALUserVar( &status, "f0",               'f',  UVAR_OPTIONAL, "Start search frequency", &uvar_f0), &status);
  LAL_CALL( LALRegisterREALUserVar( &status, "fBand",            'b',  UVAR_OPTIONAL, "Search frequency band", &uvar_fBand), &status);
  LAL_CALL( LALRegisterREALUserVar( &status, "startTime",         0,  UVAR_OPTIONAL, "GPS start time of observation", &uvar_startTime), &status);
  LAL_CALL( LALRegisterREALUserVar(   &status, "endTime",         0,  UVAR_OPTIONAL, "GPS end time of observation", &uvar_endTime), &status);
  LAL_CALL( LALRegisterSTRINGUserVar( &status, "timeStampsFile",  0,  UVAR_OPTIONAL, "Input time-stamps file", &uvar_timeStampsFile),   &status);
  LAL_CALL( LALRegisterSTRINGUserVar( &status, "skyRegion",       0,  UVAR_OPTIONAL, "sky-region polygon (or 'allsky')", &uvar_skyRegion), &status);
  LAL_CALL( LALRegisterREALUserVar(   &status, "dAlpha",          0,  UVAR_OPTIONAL, "Resolution for flat or isotropic coarse grid (rad)", &uvar_dAlpha), &status);
  LAL_CALL( LALRegisterREALUserVar(   &status, "dDelta",          0,  UVAR_OPTIONAL, "Resolution for flat or isotropic coarse grid (rad)", &uvar_dDelta), &status);
  LAL_CALL( LALRegisterSTRINGUserVar( &status, "skyfile",         0,  UVAR_OPTIONAL, "Alternative: input skypatch file", &uvar_skyfile),     &status);
  LAL_CALL( LALRegisterSTRINGUserVar( &status, "earthEphemeris", 'E', UVAR_OPTIONAL, "Earth Ephemeris file",  &uvar_earthEphemeris),  &status);
  LAL_CALL( LALRegisterSTRINGUserVar( &status, "sunEphemeris",   'S', UVAR_OPTIONAL, "Sun Ephemeris file", &uvar_sunEphemeris), &status);
  LAL_CALL( LALRegisterSTRINGUserVar( &status, "sftDir",         'D', UVAR_REQUIRED, "SFT filename pattern", &uvar_sftDir), &status);

  LAL_CALL( LALRegisterREALUserVar(   &status, "maxlag",         0,  UVAR_OPTIONAL, "Maximum time lag for correlating sfts", &uvar_maxlag), &status);


  LAL_CALL( LALRegisterSTRINGUserVar( &status, "dirnameOut",     'o', UVAR_OPTIONAL, "Output directory", &uvar_dirnameOut), &status);
  LAL_CALL( LALRegisterSTRINGUserVar( &status, "fbasenameOut",    0,  UVAR_OPTIONAL, "Output file basename", &uvar_fbasenameOut), &status);
  LAL_CALL( LALRegisterINTUserVar(    &status, "blocksRngMed",    0, UVAR_OPTIONAL, "Running Median block size", &uvar_blocksRngMed), &status);



  /* read all command line variables */
  LAL_CALL( LALUserVarReadAllInput(&status, argc, argv), &status);

  /* exit if help was required */
  if (uvar_help)
    exit(0); 

  /* very basic consistency checks on user input */
  if ( uvar_f0 < 0 ) {
    fprintf(stderr, "start frequency must be positive\n");
    exit(1);
  }

  if ( uvar_fBand < 0 ) {
    fprintf(stderr, "search frequency band must be positive\n");
    exit(1);
  }
 
  /* set up skypatches */
  LAL_CALL( SetUpRadiometerSkyPatches( &status, &skyInfo, uvar_skyfile, uvar_skyRegion, uvar_dAlpha, uvar_dDelta), &status);
  nSkyPatches = skyInfo.numSkyPatches;
  skyAlpha = skyInfo.alpha;
  skyDelta = skyInfo.delta;
  skySizeAlpha = skyInfo.alphaSize;
  skySizeDelta = skyInfo.deltaSize;
    
    

  /* read sfts */
  {
    /* new SFT I/O data types */
    SFTCatalog *catalog = NULL;
    static SFTConstraints constraints;

    REAL8 doppWings, fmin, fmax;
    INT4 k;

    /* set detector constraint */
    constraints.detector = NULL;

    if ( LALUserVarWasSet( &uvar_startTime ) ) {
      LAL_CALL ( LALFloatToGPS( &status, &startTimeGPS, &uvar_startTime), &status);
      constraints.startTime = &startTimeGPS;
    }

    if ( LALUserVarWasSet( &uvar_endTime ) ) {
      LAL_CALL ( LALFloatToGPS( &status, &endTimeGPS, &uvar_endTime), &status);
      constraints.endTime = &endTimeGPS;
    }

    if ( LALUserVarWasSet( &uvar_timeStampsFile ) ) {
      LAL_CALL ( LALReadTimestampsFile ( &status, &inputTimeStampsVector, uvar_timeStampsFile), &status);
      constraints.timestamps = inputTimeStampsVector;
    }
    
    /* get sft catalog */
    LAL_CALL( LALSFTdataFind( &status, &catalog, uvar_sftDir, &constraints), &status);
    if ( (catalog == NULL) || (catalog->length == 0) ) {
      fprintf (stderr,"Unable to match any SFTs with pattern '%s'\n", uvar_sftDir );
      exit(1);
    }

    /* now we can free the inputTimeStampsVector */
    if ( LALUserVarWasSet( &uvar_timeStampsFile ) ) {
      LALFree( inputTimeStampsVector->data );
    }


    /* first some sft parameters */
    deltaF = catalog->data[0].header.deltaF;  /* frequency resolution */
    timeBase= 1.0/deltaF; /* coherent integration time */
    f0Bin = floor( uvar_f0 * timeBase + 0.5); /* initial search frequency */
    numSearchBins =  uvar_fBand * timeBase; /* total number of search bins - 1 */
    fLastBin = f0Bin + numSearchBins;   /* final frequency bin to be analyzed */
    

    /* read sft files making sure to add extra bins for running median */
    /* add wings for Doppler modulation and running median block size*/
    doppWings = (uvar_f0 + uvar_fBand) * VTOT;    
    fmin = uvar_f0 - doppWings - uvar_blocksRngMed * deltaF;
    fmax = uvar_f0 + uvar_fBand + doppWings + uvar_blocksRngMed * deltaF;

    /* read the sfts */
    LAL_CALL( LALLoadMultiSFTs ( &status, &inputSFTs, catalog, fmin, fmax), &status);
    numifo = inputSFTs->length;    

    /* find number of sfts */
    /* loop over ifos and calculate number of sfts */
    /* note that we can't use the catalog to determine the number of SFTs
       because SFTs might be segmented in frequency */
    numsft = 0; /* initialization */
    for (k = 0; k < (INT4)numifo; k++ ) {
      numsft += inputSFTs->data[k]->length;
    } 
    
    /* catalog is ordered in time so we can get start, end time and tObs*/
    firstTimeStamp = catalog->data[0].header.epoch;
    lastTimeStamp = catalog->data[catalog->length - 1].header.epoch;
    tObs = XLALGPSDiff( &lastTimeStamp, &firstTimeStamp ) + timeBase;

    /* SFT info -- assume all SFTs have same length */
    binsSFT = inputSFTs->data[0]->data->data->length;

    LAL_CALL( LALDestroySFTCatalog( &status, &catalog ), &status);  	

  } /* end of sft reading block */

 
  
  /* set up ephemeris, get detector velocities, position, and timestamps */
  { 

    MultiPSDVector *multPSD = NULL;  
    INT4 tmpLeap;

    /*     LALLeapSecFormatAndAcc lsfas = {LALLEAPSEC_GPSUTC, LALLEAPSEC_STRICT}; */
    LALLeapSecFormatAndAcc lsfas = {LALLEAPSEC_GPSUTC, LALLEAPSEC_LOOSE};

    /*  get ephemeris  */
    edat = (EphemerisData *)LALCalloc(1, sizeof(EphemerisData));
    (*edat).ephiles.earthEphemeris = uvar_earthEphemeris;
    (*edat).ephiles.sunEphemeris = uvar_sunEphemeris;
    LAL_CALL( LALLeapSecs(&status, &tmpLeap, &firstTimeStamp, &lsfas), &status);
    (*edat).leap = (INT2)tmpLeap;
    LAL_CALL( LALInitBarycenter( &status, edat), &status);


    /* normalize sfts */
    LAL_CALL( LALNormalizeMultiSFTVect (&status, &multPSD, inputSFTs, uvar_blocksRngMed), &status);
       
    /* we are now done with the psd */
    LAL_CALL ( LALDestroyMultiPSDVector  ( &status, &multPSD), &status);

    /* get information about all detectors including velocity and timestamps */
    /* note that this function returns the velocity at the 
       mid-time of the SFTs -- should not make any difference */
    LAL_CALL ( LALGetMultiDetectorStates ( &status, &mdetStates, inputSFTs, edat), &status);

  } /* end block for position, velocity and time */
  


  /* loop over sky patches -- main Hough calculations */
  for (skyCounter = 0; skyCounter < nSkyPatches; skyCounter++)
    {
      /* set sky positions and skypatch sizes */
      alpha = skyAlpha[skyCounter];
      delta = skyDelta[skyCounter];
      patchSizeX = skySizeDelta[skyCounter];
      patchSizeY = skySizeAlpha[skyCounter];

      /* get sft pairs */
      pairParams.lag = uvar_maxlag;
      LAL_CALL( CreateSFTPairs( &status, &sftPairs, inputSFTs, mdetStates, &pairParams), &status);
      
      /* correlate sft pairs */
      


      /* select candidates */



    } /* finish loop over skypatches */



  /* free memory */
  LAL_CALL (LALDestroyMultiSFTVector(&status, &inputSFTs), &status );
  LALFree(sftPairs.data);

  XLALDestroyMultiDetectorStateSeries ( mdetStates );

  LALFree(edat->ephemE);
  LALFree(edat->ephemS);
  LALFree(edat);

  LALFree(skyAlpha);
  LALFree(skyDelta);
  LALFree(skySizeAlpha);
  LALFree(skySizeDelta);

  LAL_CALL (LALDestroyUserVars(&status), &status);

  LALCheckMemoryLeaks();

  if ( lalDebugLevel )
    REPORTSTATUS ( &status);

  return status.statusCode;
}




/** Set up location of skypatch centers and sizes 
    If user specified skyRegion then use DopplerScan function
    to construct an isotropic grid. Otherwise use skypatch file. */
void SetUpRadiometerSkyPatches(LALStatus           *status,
		     SkyPatchesInfo      *out,   /**< output skypatches info */
		     CHAR                *skyFileName, /**< name of skypatch file */
		     CHAR                *skyRegion,  /**< skyregion (if isotropic grid is to be constructed) */
		     REAL8               dAlpha,      /**< alpha resolution (if isotropic grid is to be constructed) */
		     REAL8               dDelta)  /**< delta resolution (if isotropic grid is to be constructed) */
{

  DopplerSkyScanInit scanInit = empty_DopplerSkyScanInit;   /* init-structure for DopperScanner */
  DopplerSkyScanState thisScan = empty_DopplerSkyScanState; /* current state of the Doppler-scan */
  UINT4 nSkyPatches, skyCounter;
  PulsarDopplerParams dopplerpos;	  
  
  INITSTATUS (status, "SetRadiometerUpSkyPatches", rcsid);
  ATTATCHSTATUSPTR (status);

  ASSERT (out, status, RADIOMETER_ENULL, RADIOMETER_MSGENULL);
  ASSERT (dAlpha > 0, status, RADIOMETER_EBAD, RADIOMETER_MSGEBAD);
  ASSERT (dDelta > 0, status, RADIOMETER_EBAD, RADIOMETER_MSGEBAD);

  if (skyRegion ) {
    
    scanInit.dAlpha = dAlpha;
    scanInit.dDelta = dDelta;
    scanInit.gridType = GRID_ISOTROPIC;
    scanInit.metricType =  LAL_PMETRIC_NONE;
    scanInit.skyRegionString = (CHAR*)LALCalloc(1, strlen(skyRegion)+1);
    strcpy (scanInit.skyRegionString, skyRegion);
    /*   scanInit.Freq = usefulParams.spinRange_midTime.fkdot[0] +  usefulParams.spinRange_midTime.fkdotBand[0]; */
    
    /* set up the grid */
    TRY ( InitDopplerSkyScan ( status->statusPtr, &thisScan, &scanInit), status); 
    
    nSkyPatches = out->numSkyPatches = thisScan.numSkyGridPoints;
    
    out->alpha = (REAL8 *)LALCalloc(1, nSkyPatches*sizeof(REAL8));
    out->delta = (REAL8 *)LALCalloc(1, nSkyPatches*sizeof(REAL8));     
    out->alphaSize = (REAL8 *)LALCalloc(1, nSkyPatches*sizeof(REAL8));
    out->deltaSize = (REAL8 *)LALCalloc(1, nSkyPatches*sizeof(REAL8));     
        
    /* loop over skygrid points */  
    XLALNextDopplerSkyPos(&dopplerpos, &thisScan);
    
    skyCounter = 0; 
    while(thisScan.state != STATE_FINISHED) {
      
      out->alpha[skyCounter] = dopplerpos.Alpha;
      out->delta[skyCounter] = dopplerpos.Delta;
      out->alphaSize[skyCounter] = dAlpha;
      out->deltaSize[skyCounter] = dDelta;
      
      if ((dopplerpos.Delta>0) && (dopplerpos.Delta < atan(4*LAL_PI/dAlpha/dDelta) ))
        out->alphaSize[skyCounter] = dAlpha*cos(dopplerpos.Delta -0.5*dDelta)/cos(dopplerpos.Delta);

      if ((dopplerpos.Delta<0) && (dopplerpos.Delta > -atan(4*LAL_PI/dAlpha/dDelta) ))
        out->alphaSize[skyCounter] = dAlpha*cos(dopplerpos.Delta +0.5*dDelta)/cos(dopplerpos.Delta);
      
      XLALNextDopplerSkyPos(&dopplerpos, &thisScan);
      skyCounter++;
      
    } /* end while loop over skygrid */
      
    /* free dopplerscan stuff */
    TRY ( FreeDopplerSkyScan( status->statusPtr, &thisScan), status);
    if ( scanInit.skyRegionString )
      LALFree ( scanInit.skyRegionString );

  } else {

    /* read skypatch info */
    {
      FILE   *fpsky = NULL; 
      INT4   r;
      REAL8  temp1, temp2, temp3, temp4;

      ASSERT (skyFileName, status, RADIOMETER_ENULL, RADIOMETER_MSGENULL);
      
      if ( (fpsky = fopen(skyFileName, "r")) == NULL)
	{
	  ABORT ( status, RADIOMETER_EFILE, RADIOMETER_MSGEFILE );
	}
      
      nSkyPatches = 0;
      do 
	{
	  r = fscanf(fpsky,"%lf%lf%lf%lf\n", &temp1, &temp2, &temp3, &temp4);
	  /* make sure the line has the right number of entries or is EOF */
	  if (r==4) nSkyPatches++;
	} while ( r != EOF);
      rewind(fpsky);

      out->numSkyPatches = nSkyPatches;      
      out->alpha = (REAL8 *)LALCalloc(nSkyPatches, sizeof(REAL8));
      out->delta = (REAL8 *)LALCalloc(nSkyPatches, sizeof(REAL8));     
      out->alphaSize = (REAL8 *)LALCalloc(nSkyPatches, sizeof(REAL8));
      out->deltaSize = (REAL8 *)LALCalloc(nSkyPatches, sizeof(REAL8));     
      
      for (skyCounter = 0; skyCounter < nSkyPatches; skyCounter++)
	{
	  r = fscanf(fpsky,"%lf%lf%lf%lf\n", out->alpha + skyCounter, out->delta + skyCounter, 
		     out->alphaSize + skyCounter,  out->deltaSize + skyCounter);
	}
      
      fclose(fpsky);     
    } /* end skypatchfile reading block */    
  } /* end setting up of skypatches */


  DETATCHSTATUSPTR (status);
	
  /* normal exit */	
  RETURN (status);

}

/** create pairs of sfts */
void CreateSFTPairs(LALStatus                *status,
		    SFTPairVec               *out,
		    MultiSFTVector           *inputSFTs,
		    MultiDetectorStateSeries *mdetStates,
		    SFTPairParams            *par)
{  

  UINT4 numifo;

  INITSTATUS (status, "CreateSFTPairs", rcsid);
  ATTATCHSTATUSPTR (status);

  ASSERT (out, status, RADIOMETER_ENULL, RADIOMETER_MSGENULL);
  ASSERT (inputSFTs, status, RADIOMETER_ENULL, RADIOMETER_MSGENULL);
  ASSERT (mdetStates, status, RADIOMETER_ENULL, RADIOMETER_MSGENULL);
  ASSERT (inputSFTs->length == mdetStates->length, status, RADIOMETER_EBAD, RADIOMETER_MSGEBAD);

  numifo = inputSFTs->length;
  /* for now require exactly 2 ifos -- to be relaxed very soon in the future */
  if ( numifo != 2) {
    ABORT ( status, RADIOMETER_EBAD, RADIOMETER_MSGEBAD );
  }

  TRY( CreateSFTPairsFrom2SFTvectors(status->statusPtr, out, inputSFTs->data[0], 
				     inputSFTs->data[1], mdetStates->data[0], 
				     mdetStates->data[1], par), status);

  DETATCHSTATUSPTR (status);
	
  /* normal exit */	
  RETURN (status);
}



/** create pairs of sfts from a pair of sft vectors*/
void CreateSFTPairsFrom2SFTvectors(LALStatus                 *status,
				   SFTPairVec                *out,
				   const SFTVector           *in1,
				   const SFTVector           *in2,
				   const DetectorStateSeries *det1,
				   const DetectorStateSeries *det2,
				   SFTPairParams             *par)
{
  
  UINT4 i, j, numsft1, numsft2, numPairs, numsftMin;

  COMPLEX8FrequencySeries  *thisSFT1, *thisSFT2;	       
  DetectorState *thisDetState1, *thisDetState2;
  SingleSFTpair *thisPair;

  INITSTATUS (status, "CreateSFTPairsFrom2SFTvectors", rcsid);
  ATTATCHSTATUSPTR (status);

  ASSERT (out, status, RADIOMETER_ENULL, RADIOMETER_MSGENULL);
  ASSERT (in1, status, RADIOMETER_ENULL, RADIOMETER_MSGENULL);
  ASSERT (in2, status, RADIOMETER_ENULL, RADIOMETER_MSGENULL);
  ASSERT (det1, status, RADIOMETER_ENULL, RADIOMETER_MSGENULL);
  ASSERT (det2, status, RADIOMETER_ENULL, RADIOMETER_MSGENULL);
  ASSERT (par, status, RADIOMETER_ENULL, RADIOMETER_MSGENULL);

  /* number of SFTs from the two ifos */
  numsft1 = in1->length;
  numsft2 = in2->length;
  numsftMin = (numsft1 < numsft2)? numsft1 : numsft2;

  numPairs = out->length; /* initialization */
  thisPair = out->data + numPairs - 1;

  /* increase length of sftpair vector */
  out->length += numsftMin;
  out->data = LALRealloc(out->data, out->length * sizeof(out->data[0]));

  /* go over all sfts in first vector */
  for (i=0; i<numsft1; i++) {

    thisSFT1 = in1->data + i;
    thisDetState1 = det1->data + i;
    
    /* go over all sfts in second vector and check if it should be paired with thisSFT1 */
    /* this can be made more efficient if necessary */
    for (j=0; j<numsft2; j++) {

      LIGOTimeGPS t1, t2;
      REAL8 timeDiff;

      thisSFT2 = in2->data + j;
      thisDetState2 = det2->data + i;

      /* calculate time difference */      
      t1 = thisSFT1->epoch;
      t2 = thisSFT2->epoch;
      timeDiff = XLALDeltaFloatGPS( &t1, &t2);

      /* decide whether to add this pair or not */
      if ( fabs(timeDiff) < par->lag ) {
	numPairs++;

	if ( numPairs < out->length) {
	  /* there is enough memory */
	  thisPair++;
	  TRY( FillSFTPair( status->statusPtr, thisPair, thisSFT1, thisSFT2, thisDetState1, thisDetState2), status);
	} 
	else {
	  /* there not enough memory -- allocate memory and add the pair */

	  out->length += numsftMin;
	  out->data = LALRealloc(out->data, out->length * sizeof(out->data[0]));

	  thisPair++;
	  TRY( FillSFTPair( status->statusPtr, thisPair, thisSFT1, thisSFT2, thisDetState1, thisDetState2), status);	  
	}
      }
    } /* end loop over second sft set */
  } /* end loop over first sft set */ 


  /* realloc memory */
  out->length = numPairs;
  out->data = LALRealloc(out->data, out->length * sizeof(out->data[0]));

  DETATCHSTATUSPTR (status);
	
  /* normal exit */	
  RETURN (status);

}


/** little helper function for filling up sft pair */
void FillSFTPair(LALStatus                 *status,
		 SingleSFTpair             *out,
		 COMPLEX8FrequencySeries   *sft1, 
		 COMPLEX8FrequencySeries   *sft2, 
		 DetectorState             *det1,
		 DetectorState             *det2)
{  
  INITSTATUS (status, "FillSFTPairs", rcsid);
  ATTATCHSTATUSPTR (status);
    
  out->sft1 = sft1;
  out->sft2 = sft2;
  
  out->vel1[0] = det1->vDetector[0];
  out->vel1[1] = det1->vDetector[1];
  out->vel1[2] = det1->vDetector[2];
  
  out->vel2[0] = det2->vDetector[0];
  out->vel2[1] = det2->vDetector[1];
  out->vel2[2] = det2->vDetector[2];
  
  out->pos1[0] = det1->rDetector[0];
  out->pos1[1] = det1->rDetector[1];
  out->pos1[2] = det1->rDetector[2];
  
  out->pos2[0] = det2->rDetector[0];
  out->pos2[1] = det2->rDetector[1];
  out->pos2[2] = det2->rDetector[2];
  
  DETATCHSTATUSPTR (status);
	
  /* normal exit */	
  RETURN (status);

}


/** Correlate a single pair of SFT at a parameter space point*/
void CorrelateSingleSFTPair(LALStatus           *status,
			    SingleSFTpair       *in,
			    PulsarDopplerParams *par)
{  
  UINT4 k;
  REAL8 fhat1, fhat2, freq1, freq2;
  REAL8 *vel1, *vel2, *pos1, *pos2;
  REAL8 alpha, delta;
  REAL8 timeDiff1, timeDiff2, factor1, factor2;
  REAL8 vdotn1, vdotn2;

  REAL8 deltaF;
  UINT8 freqBin1, freqBin2;

  COMPLEX8FrequencySeries  *sft1, *sft2; 

  INITSTATUS (status, "CorrelateSingleSFTPair", rcsid);
  ATTATCHSTATUSPTR (status);


  vel1 = in->vel1;
  vel2 = in->vel2;
  pos1 = in->pos1;
  pos2 = in->pos2;
  sft1 = in->sft1;
  sft2 = in->sft2;

  /* assume both sfts have the same freq. resolution */
  deltaF = sft1->deltaF;

  alpha = par->Alpha;
  delta = par->Delta;


  /* calculate the frequencies of the signal at the two IFOs */
  vdotn1 = vel1[0]*cos(delta)*cos(alpha) + vel1[1]*cos(delta)*sin(alpha) + vel1[2]*sin(delta);
  vdotn2 = vel2[0]*cos(delta)*cos(alpha) + vel2[1]*cos(delta)*sin(alpha) + vel2[2]*sin(delta);

  timeDiff1 = XLALGPSDiff(&(sft1->epoch), &(par->refTime));
  timeDiff2 = XLALGPSDiff(&(sft2->epoch), &(par->refTime));

  fhat1 = fhat2 = par->fkdot[0];
  factor1 = factor2 = 1.0;
  for (k = 1;  k < PULSAR_MAX_SPINS; k++) {

    factor1 *= timeDiff1 / k;
    factor2 *= timeDiff2 / k;

    fhat1 += par->fkdot[k] * factor1;
    fhat2 += par->fkdot[k] * factor2;
  }

  freq1 = fhat1 * (1 + vdotn1);
  freq2 = fhat2 * (1 + vdotn2);

  freqBin1 = (UINT8)(freq1/deltaF);
  freqBin2 = (UINT8)(freq2/deltaF);

  DETATCHSTATUSPTR (status);
	
  /* normal exit */	
  RETURN (status);

}
