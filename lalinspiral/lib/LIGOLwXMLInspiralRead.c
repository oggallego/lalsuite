/*
 * LIGOLwXMLInspiralRead.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with with program; see the file COPYING. If not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include <stdio.h>
#include <metaio.h>

#include <lal/LIGOMetadataTables.h>
#include <lal/LIGOLwXMLInspiralRead.h>


/*
 *
 * LAL Functions
 *
 */


typedef struct
tagMetaTableDirectory
{
  const CHAR *name;
  INT4   pos;
  INT4   idx;
}
MetaTableDirectory;


#define CLOBBER_EVENTS \
  while ( *eventHead ) \
{ \
  thisEvent = *eventHead; \
  *eventHead = (*eventHead)->next; \
  LALFree( thisEvent ); \
  thisEvent = NULL; \
}



int
LALSnglInspiralTableFromLIGOLw (
    SnglInspiralTable **eventHead,
    const CHAR         *fileName,
    INT4                startEvent,
    INT4                stopEvent
    )

{
  int                                   i, j, nrows;
  int                                   mioStatus;
  SnglInspiralTable                    *thisEvent = NULL;
  struct MetaioParseEnvironment         parseEnv;
  const  MetaioParseEnv                 env = &parseEnv;
  MetaTableDirectory tableDir[] =
  {
    {"ifo",                     -1, 0},
    {"search",                  -1, 1},
    {"channel",                 -1, 2},
    {"end_time",                -1, 3},
    {"end_time_ns",             -1, 4},
    {"end_time_gmst",           -1, 5},
    {"impulse_time",            -1, 6},
    {"impulse_time_ns",         -1, 7},
    {"template_duration",       -1, 8},
    {"event_duration",          -1, 9},
    {"amplitude",               -1, 10},
    {"eff_distance",            -1, 11},
    {"coa_phase",               -1, 12},
    {"mass1",                   -1, 13},
    {"mass2",                   -1, 14},
    {"mchirp",                  -1, 15},
    {"mtotal",                  -1, 16},
    {"eta",                     -1, 17},
    {"tau0",                    -1, 18},
    {"tau2",                    -1, 19},
    {"tau3",                    -1, 20},
    {"tau4",                    -1, 21},
    {"tau5",                    -1, 22},
    {"ttotal",                  -1, 23},
    {"psi0",                    -1, 24},
    {"psi3",                    -1, 25},
    {"alpha",                   -1, 26},
    {"alpha1",                  -1, 27},
    {"alpha2",                  -1, 28},
    {"alpha3",                  -1, 29},
    {"alpha4",                  -1, 30},
    {"alpha5",                  -1, 31},
    {"alpha6",                  -1, 32},
    {"beta",                    -1, 33},
    {"f_final",                 -1, 34},
    {"snr",                     -1, 35},
    {"chisq",                   -1, 36},
    {"chisq_dof",               -1, 37},
    {"bank_chisq",              -1, 38},
    {"bank_chisq_dof",          -1, 39},
    {"cont_chisq",              -1, 40},
    {"cont_chisq_dof",          -1, 41},
    {"sigmasq",                 -1, 42},
    {"rsqveto_duration",        -1, 43},
    {"event_id",                -1, 44},
    {"Gamma0",                  -1, 45},
    {"Gamma1",                  -1, 46},
    {"Gamma2",                  -1, 47},
    {"Gamma3",                  -1, 48},
    {"Gamma4",                  -1, 49},
    {"Gamma5",                  -1, 50},
    {"Gamma6",                  -1, 51},
    {"Gamma7",                  -1, 52},
    {"Gamma8",                  -1, 53},
    {"Gamma9",                  -1, 54},
    {"kappa",                   -1, 55},
    {"chi",                     -1, 56},
    {"spin1x",                  -1, 57},
    {"spin1y",                  -1, 58},
    {"spin1z",                  -1, 59},
    {"spin2x",                  -1, 60},
    {"spin2y",                  -1, 61},
    {"spin2z",                  -1, 62},
    {"process_id",              -1, 63},
    {NULL,                       0, 0}
  };

  /* check that the bank handle and pointer are vaid */
  if ( ! eventHead )
  {
    fprintf( stderr, "null pointer passed as handle to event list" );
    return -1;
  }
  if ( *eventHead )
  {
    fprintf( stderr, "non-null pointer passed as pointer to event list" );
    return -1;
  }

  /* open the sngl_inspiral table template bank file */
  mioStatus = MetaioOpenFile( env, fileName );
  if ( mioStatus )
  {
    MetaioAbort(env);
    MetaioClose(env);
    fprintf( stderr, "unable to open file %s\n", fileName );
    return -1;
  }

  /* open the sngl_inspiral table template bank file */
  mioStatus = MetaioOpenTableOnly( env, "sngl_inspiral" );
  if ( mioStatus )
  {
    MetaioAbort(env);
    MetaioClose(env);
    fprintf( stdout, "no sngl_inspiral table in file %s\n", fileName );
    return 0;
  }

  /* figure out the column positions of the template parameters */
  for ( i = 0; tableDir[i].name; ++i )
  {
    if ( (tableDir[i].pos = MetaioFindColumn( env, tableDir[i].name )) < 0
        &&  tableDir[i].idx != 39 )
    {
      fprintf( stderr, "unable to find column %s\n", tableDir[i].name );

      if ( strstr(tableDir[i].name, "Gamma") )
      {
        fprintf( stderr,
            "The %s column is not populated, continuing anyway\n", tableDir[i].name);
      }
      else
      {
        MetaioClose(env);
        return -1;
      }
    }
  }

  /* loop over the rows in the file */
  i = nrows = 0;
  while ( (mioStatus = MetaioGetRow(env)) == 1 )
  {
    /* count the rows in the file */
    i++;

    /* stop parsing if we have reach the last row requested */
    if ( stopEvent > -1 && i > stopEvent )
    {
      break;
    }

    /* if we have reached the first requested row, parse the row */
    if ( i > startEvent )
    {
      /* allocate memory for the template we are about to read in */
      if ( ! *eventHead )
      {
        thisEvent = *eventHead = (SnglInspiralTable *)
          LALCalloc( 1, sizeof(SnglInspiralTable) );
      }
      else
      {
        thisEvent = thisEvent->next = (SnglInspiralTable *)
          LALCalloc( 1, sizeof(SnglInspiralTable) );
      }
      if ( ! thisEvent )
      {
        fprintf( stderr, "could not allocate inspiral template\n" );
        CLOBBER_EVENTS;
        MetaioClose( env );
        return -1;
      }

      /* parse the contents of the row into the InspiralTemplate structure */
      for ( j = 0; tableDir[j].name; ++j )
      {
        REAL4 r4colData = env->ligo_lw.table.elt[tableDir[j].pos].data.real_4;
        REAL8 r8colData = env->ligo_lw.table.elt[tableDir[j].pos].data.real_8;
        INT4  i4colData = env->ligo_lw.table.elt[tableDir[j].pos].data.int_4s;
        INT8  i8colData = env->ligo_lw.table.elt[tableDir[j].pos].data.int_8s;

        if ( tableDir[j].pos < 0 ) continue;

        if ( tableDir[j].idx == 0 )
        {
          snprintf( thisEvent->ifo, LIGOMETA_IFO_MAX * sizeof(CHAR),
              "%s", env->ligo_lw.table.elt[tableDir[j].pos].data.lstring.data );
        }
        else if ( tableDir[j].idx == 1 )
        {
          snprintf( thisEvent->search, LIGOMETA_SEARCH_MAX * sizeof(CHAR),
              "%s", env->ligo_lw.table.elt[tableDir[j].pos].data.lstring.data );
        }
        else if ( tableDir[j].idx == 2 )
        {
          snprintf( thisEvent->channel, LIGOMETA_CHANNEL_MAX * sizeof(CHAR),
              "%s", env->ligo_lw.table.elt[tableDir[j].pos].data.lstring.data );
        }
        else if ( tableDir[j].idx == 3 )
        {
          thisEvent->end.gpsSeconds = i4colData;
        }
        else if ( tableDir[j].idx == 4 )
        {
          thisEvent->end.gpsNanoSeconds = i4colData;
        }
        else if ( tableDir[j].idx == 5 )
        {
          thisEvent->end_time_gmst = r8colData;
        }
        else if ( tableDir[j].idx == 6 )
        {
          thisEvent->impulse_time.gpsSeconds = i4colData;
        }
        else if ( tableDir[j].idx == 7 )
        {
          thisEvent->impulse_time.gpsNanoSeconds = i4colData;
        }
        else if ( tableDir[j].idx == 8 )
        {
          thisEvent->template_duration = r8colData;
        }
        else if ( tableDir[j].idx == 9 )
        {
          thisEvent->event_duration = r8colData;
        }
        else if ( tableDir[j].idx == 10 )
        {
          thisEvent->amplitude = r4colData;
        }
        else if ( tableDir[j].idx == 11 )
        {
          thisEvent->eff_distance = r4colData;
        }
        else if ( tableDir[j].idx == 12 )
        {
          thisEvent->coa_phase = r4colData;
        }
        else if ( tableDir[j].idx == 13 )
        {
          thisEvent->mass1 = r4colData;
        }
        else if ( tableDir[j].idx == 14 )
        {
          thisEvent->mass2 = r4colData;
        }
        else if ( tableDir[j].idx == 15 )
        {
          thisEvent->mchirp = r4colData;
        }
        else if ( tableDir[j].idx == 16 )
        {
          thisEvent->mtotal = r4colData;
        }
        else if ( tableDir[j].idx == 17 )
        {
          thisEvent->eta = r4colData;
        }
        else if ( tableDir[j].idx == 18 )
        {
          thisEvent->tau0 = r4colData;
        }
        else if ( tableDir[j].idx == 19 )
        {
          thisEvent->tau2 = r4colData;
        }
        else if ( tableDir[j].idx == 20 )
        {
          thisEvent->tau3 = r4colData;
        }
        else if ( tableDir[j].idx == 21 )
        {
          thisEvent->tau4 = r4colData;
        }
        else if ( tableDir[j].idx == 22 )
        {
          thisEvent->tau5 = r4colData;
        }
        else if ( tableDir[j].idx == 23 )
        {
          thisEvent->ttotal = r4colData;
        }
        else if ( tableDir[j].idx == 24 )
        {
          thisEvent->psi0 = r4colData;
        }
        else if ( tableDir[j].idx == 25 )
        {
          thisEvent->psi3 = r4colData;
        }
        else if ( tableDir[j].idx == 26 )
        {
          thisEvent->alpha = r4colData;
        }
        else if ( tableDir[j].idx == 27 )
        {
          thisEvent->alpha1 = r4colData;
        }
        else if ( tableDir[j].idx == 28 )
        {
          thisEvent->alpha2 = r4colData;
        }
        else if ( tableDir[j].idx == 29 )
        {
          thisEvent->alpha3 = r4colData;
        }
        else if ( tableDir[j].idx == 30 )
        {
          thisEvent->alpha4 = r4colData;
        }
        else if ( tableDir[j].idx == 31 )
        {
          thisEvent->alpha5 = r4colData;
        }
        else if ( tableDir[j].idx == 32 )
        {
          thisEvent->alpha6 = r4colData;
        }
        else if ( tableDir[j].idx == 33 )
        {
          thisEvent->beta = r4colData;
        }
        else if ( tableDir[j].idx == 34 )
        {
          thisEvent->f_final = r4colData;
        }
        else if ( tableDir[j].idx == 35 )
        {
          thisEvent->snr = r4colData;
        }
        else if ( tableDir[j].idx == 36 )
        {
          thisEvent->chisq = r4colData;
        }
        else if ( tableDir[j].idx == 37 )
        {
          thisEvent->chisq_dof = i4colData;
        }
        else if ( tableDir[j].idx == 38 )
        {
          thisEvent->bank_chisq = r4colData;
        }
        else if ( tableDir[j].idx == 39 )
        {
          thisEvent->bank_chisq_dof = i4colData;
        }
        else if ( tableDir[j].idx == 40 )
        {
          thisEvent->cont_chisq = r4colData;
        }
        else if ( tableDir[j].idx == 41 )
        {
          thisEvent->cont_chisq_dof = i4colData;
        }
        else if ( tableDir[j].idx == 42 )
        {
          thisEvent->sigmasq = r8colData;
        }
        else if ( tableDir[j].idx == 43 )
        {
          thisEvent->rsqveto_duration = r4colData;
        }
        else if ( tableDir[j].idx == 44 )
        {
          thisEvent->event_id = i8colData;
        }
        else if ( tableDir[j].idx == 45 )
        {
          thisEvent->Gamma[0] = r4colData;
        }
        else if ( tableDir[j].idx == 46 )
        {
          thisEvent->Gamma[1] = r4colData;
        }
        else if ( tableDir[j].idx == 47 )
        {
          thisEvent->Gamma[2] = r4colData;
        }
        else if ( tableDir[j].idx == 48 )
        {
          thisEvent->Gamma[3] = r4colData;
        }
        else if ( tableDir[j].idx == 49 )
        {
          thisEvent->Gamma[4] = r4colData;
        }
        else if ( tableDir[j].idx == 50 )
        {
          thisEvent->Gamma[5] = r4colData;
        }
        else if ( tableDir[j].idx == 51 )
        {
          thisEvent->Gamma[6] = r4colData;
        }
        else if ( tableDir[j].idx == 52 )
        {
          thisEvent->Gamma[7] = r4colData;
        }
        else if ( tableDir[j].idx == 53 )
        {
          thisEvent->Gamma[8] = r4colData;
        }
        else if ( tableDir[j].idx == 54 )
        {
          thisEvent->Gamma[9] = r4colData;
        }
        else if ( tableDir[j].idx == 55 )
        {
          thisEvent->kappa = r4colData;
        }
        else if ( tableDir[j].idx == 56 )
        {
          thisEvent->chi = r4colData;
        }
        else if ( tableDir[j].idx == 57 )
        {
          thisEvent->spin1x = r4colData;
        }
        else if ( tableDir[j].idx == 58 )
        {
          thisEvent->spin1y = r4colData;
        }
        else if ( tableDir[j].idx == 59 )
        {
          thisEvent->spin1z = r4colData;
        }
        else if ( tableDir[j].idx == 60 )
        {
          thisEvent->spin2x = r4colData;
        }
        else if ( tableDir[j].idx == 61 )
        {
          thisEvent->spin2y = r4colData;
        }
        else if ( tableDir[j].idx == 62 )
        {
          thisEvent->spin2z = r4colData;
        }
        else if ( tableDir[j].idx == 63 )
        {
          thisEvent->process_id = i8colData;
        }
        else
        {
          CLOBBER_EVENTS;
          fprintf( stderr, "unknown column while parsing sngl_inspiral\n" );
          return -1;
        }
      }

      /* count the number of template parsed */
      nrows++;
    }
  }

  if ( mioStatus == -1 )
  {
    fprintf( stderr, "error parsing after row %d\n", i );
    CLOBBER_EVENTS;
    MetaioClose( env );
    return -1;
  }

  /* we have sucesfully parsed temples */
  MetaioClose( env );
  return nrows;
}

#undef CLOBBER_EVENTS

#define CLOBBER_BANK \
  while ( *bankHead ) \
{ \
  thisTmplt = *bankHead; \
  *bankHead = (*bankHead)->next; \
  LALFree( thisTmplt ); \
  thisTmplt = NULL; \
}


int
InspiralTmpltBankFromLIGOLw (
    InspiralTemplate  **bankHead,
    const CHAR         *fileName,
    INT4                startTmplt,
    INT4                stopTmplt
    )

{
  int                                   i, j, nrows;
  int                                   mioStatus;
  InspiralTemplate                     *thisTmplt = NULL;
  struct MetaioParseEnvironment         parseEnv;
  const  MetaioParseEnv                 env = &parseEnv;
  int   pParParam;
  int   pParValue;
  REAL4 minMatch = 0;
  MetaTableDirectory tableDir[] =
  {
    {"mass1",   -1, 0},
    {"mass2",   -1, 1},
    {"mchirp",  -1, 2},
    {"eta",     -1, 3},
    {"tau0",    -1, 4},
    {"tau2",    -1, 5},
    {"tau3",    -1, 6},
    {"tau4",    -1, 7},
    {"tau5",    -1, 8},
    {"ttotal",  -1, 9},
    {"psi0",    -1, 10},
    {"psi3",    -1, 11},
    {"beta",    -1, 12},
    {"f_final", -1, 13},
    {"end_time", -1, 14},
    {"end_time_ns", -1, 15},
    {"event_id", -1, 16},
    {"ifo", -1, 17},
    {"Gamma0", -1, 18},
    {"Gamma1", -1, 19},
    {"Gamma2", -1, 20},
    {"Gamma3", -1, 21},
    {"Gamma4", -1, 22},
    {"Gamma5", -1, 23},
    {"Gamma6", -1, 24},
    {"Gamma7", -1, 25},
    {"Gamma8", -1, 26},
    {"Gamma9", -1, 27},
    {"kappa", -1, 28},
    {"chi", -1, 29},
    {"spin1x", -1, 30},
    {"spin1y", -1, 31},
    {"spin1z", -1, 32},
    {"spin2x", -1, 33},
    {"spin2y", -1, 34},
    {"spin2z", -1, 35},
    {NULL,      0, 0}
  };

  /* check that the bank handle and pointer are vaid */
  if ( ! bankHead )
  {
    fprintf( stderr, "null pointer passed as handle to template bank" );
    return -1;
  }
  if ( *bankHead )
  {
    fprintf( stderr, "non-null pointer passed as pointer to template bank" );
    return -1;
  }


  /* open the procress_params table from the bank file */
  mioStatus = MetaioOpenTable( env, fileName, "process_params" );
  if ( mioStatus )
  {
    fprintf( stderr, "error opening process_params table from file %s\n",
        fileName );
    return -1;
  }

  /* figure out where the param and value columns are */
  if ( (pParParam = MetaioFindColumn( env, "param" )) < 0 )
  {
    fprintf( stderr, "unable to find column param in process_params\n" );
    MetaioClose(env);
    return -1;
  }
  if ( (pParValue = MetaioFindColumn( env, "value" )) < 0 )
  {
    fprintf( stderr, "unable to find column value in process_params\n" );
    MetaioClose(env);
    return -1;
  }

  /* get the minimal match of the bank from the process params */
  while ( (mioStatus = MetaioGetRow(env)) == 1 )
  {
    if ( ! strcmp( env->ligo_lw.table.elt[pParParam].data.lstring.data,
          "--minimal-match" ) )
    {
      minMatch = (REAL4)
        atof( env->ligo_lw.table.elt[pParValue].data.lstring.data );
    }
  }

  MetaioClose( env );

  /* open the sngl_inspiral table template bank file */
  mioStatus = MetaioOpenTable( env, fileName, "sngl_inspiral" );
  if ( mioStatus )
  {
    fprintf( stdout, "no sngl_inspiral table in file %s\n", fileName );
    return 0;
  }

  /* figure out the column positions of the template parameters */
  for ( i = 0; tableDir[i].name; ++i )
  {
    if ( (tableDir[i].pos = MetaioFindColumn( env, tableDir[i].name )) < 0 )
    {
      fprintf( stderr, "unable to find column %s\n", tableDir[i].name );

      if ( strstr(tableDir[i].name, "Gamma") )
      {
        fprintf( stderr,
            "The %s column is not populated, continuing anyway\n", tableDir[i].name);
      }
      else
      {
        MetaioClose(env);
        return -1;
      }
    }
  }

  /* loop over the rows in the file */
  i = nrows = 0;
  while ( (mioStatus = MetaioGetRow(env)) == 1 )
  {
    /* count the rows in the file */
    i++;

    /* stop parsing if we have reach the last row requested */
    if ( stopTmplt > -1 && i > stopTmplt )
    {
      break;
    }

    /* if we have reached the first requested row, parse the row */
    if ( i > startTmplt )
    {
      /* allocate memory for the template we are about to read in */
      if ( ! *bankHead )
      {
        thisTmplt = *bankHead = (InspiralTemplate *)
          LALCalloc( 1, sizeof(InspiralTemplate) );
      }
      else
      {
        thisTmplt = thisTmplt->next = (InspiralTemplate *)
          LALCalloc( 1, sizeof(InspiralTemplate) );
      }
      if ( ! thisTmplt )
      {
        fprintf( stderr, "could not allocate inspiral template\n" );
        CLOBBER_BANK;
        MetaioClose( env );
        return -1;
      }

      /* parse the contents of the row into the InspiralTemplate structure */
      for ( j = 0; tableDir[j].name; ++j )
      {
        enum METAIO_Type column_type = env->ligo_lw.table.col[tableDir[j].pos].data_type;
        REAL4 colData = env->ligo_lw.table.elt[tableDir[j].pos].data.real_4;
        INT4 i4colData = env->ligo_lw.table.elt[tableDir[j].pos].data.int_4s;

        if ( tableDir[j].pos < 0 ) continue;
        if ( tableDir[j].idx == 0 )
        {
          thisTmplt->mass1 = colData;
        }
        else if ( tableDir[j].idx == 1 )
        {
          thisTmplt->mass2 = colData;
        }
        else if ( tableDir[j].idx == 2 )
        {
          thisTmplt->chirpMass = colData;
        }
        else if ( tableDir[j].idx == 3 )
        {
          thisTmplt->eta = colData;
        }
        else if ( tableDir[j].idx == 4 )
        {
          thisTmplt->t0 = colData;
        }
        else if ( tableDir[j].idx == 5 )
        {
          thisTmplt->t2 = colData;
        }
        else if ( tableDir[j].idx == 6 )
        {
          thisTmplt->t3 = colData;
        }
        else if ( tableDir[j].idx == 7 )
        {
          thisTmplt->t4 = colData;
        }
        else if ( tableDir[j].idx == 8 )
        {
          thisTmplt->t5 = colData;
        }
        else if ( tableDir[j].idx == 9 )
        {
          thisTmplt->tC = colData;
        }
        else if ( tableDir[j].idx == 10 )
        {
          thisTmplt->psi0 = colData;
        }
        else if ( tableDir[j].idx == 11 )
        {
          thisTmplt->psi3 = colData;
        }
        else if ( tableDir[j].idx == 12 )
        {
          thisTmplt->beta = colData;
        }
        else if ( tableDir[j].idx == 13 )
        {
          thisTmplt->fFinal = colData;
        }
        else if ( tableDir[j].idx == 14 )
        {
          thisTmplt->end_time.gpsSeconds = i4colData;
        }
        else if ( tableDir[j].idx == 15 )
        {
          thisTmplt->end_time.gpsNanoSeconds = i4colData;
        }
        else if ( tableDir[j].idx == 16 )
        {
          INT8 i8colData;
          if ( column_type == METAIO_TYPE_INT_8S )
            i8colData = env->ligo_lw.table.elt[tableDir[j].pos].data.int_8s;
          else
          {
            i8colData = XLALLIGOLwParseIlwdChar(env, tableDir[j].pos, "sngl_inspiral", "event_id");
            if ( i8colData < 0 )
              return -1;
          }
          thisTmplt->event_id = i8colData;
        }
        else if ( tableDir[j].idx == 17 )
        {
          snprintf( thisTmplt->ifo, LIGOMETA_IFO_MAX * sizeof(CHAR),
              "%s", env->ligo_lw.table.elt[tableDir[j].pos].data.lstring.data );
        }
        else if ( tableDir[j].idx == 18 )
        {
          thisTmplt->Gamma[0] = colData;
        }
        else if ( tableDir[j].idx == 19 )
        {
          thisTmplt->Gamma[1] = colData;
        }
        else if ( tableDir[j].idx == 20 )
        {
          thisTmplt->Gamma[2] = colData;
        }
        else if ( tableDir[j].idx == 21 )
        {
          thisTmplt->Gamma[3] = colData;
        }
        else if ( tableDir[j].idx == 22 )
        {
          thisTmplt->Gamma[4] = colData;
        }
        else if ( tableDir[j].idx == 23 )
        {
          thisTmplt->Gamma[5] = colData;
        }
        else if ( tableDir[j].idx == 24 )
        {
          thisTmplt->Gamma[6] = colData;
        }
        else if ( tableDir[j].idx == 25 )
        {
          thisTmplt->Gamma[7] = colData;
        }
        else if ( tableDir[j].idx == 26 )
        {
          thisTmplt->Gamma[8] = colData;
        }
        else if ( tableDir[j].idx == 27 )
        {
          thisTmplt->Gamma[9] = colData;
        }
        else if ( tableDir[j].idx == 28 )
        {
          thisTmplt->kappa = colData;
        }
        else if ( tableDir[j].idx == 29 )
        {
          thisTmplt->chi = colData;
        }
        else if ( tableDir[j].idx == 30 )
        {
          thisTmplt->spin1[0] = colData;
        }
        else if ( tableDir[j].idx == 31 )
        {
          thisTmplt->spin1[1] = colData;
        }
        else if ( tableDir[j].idx == 32 )
        {
          thisTmplt->spin1[2] = colData;
        }
        else if ( tableDir[j].idx == 33 )
        {
          thisTmplt->spin2[0] = colData;
        }
        else if ( tableDir[j].idx == 34 )
        {
          thisTmplt->spin2[1] = colData;
        }
        else if ( tableDir[j].idx == 35 )
        {
          thisTmplt->spin2[2] = colData;
        }
        else
        {
          CLOBBER_BANK;
          fprintf( stderr, "unknown column while parsing\n" );
          return -1;
        }
      }

      /* compute derived mass parameters */
      thisTmplt->totalMass = thisTmplt->mass1 + thisTmplt->mass2;
      if ( thisTmplt->totalMass > 0 )
      {
        thisTmplt->mu = thisTmplt->mass1 * thisTmplt->mass2 /
          thisTmplt->totalMass;
      }

      /* set the match determined from the bank generation process params */
      thisTmplt->minMatch = minMatch;

      /* count the number of template parsed */
      thisTmplt->number = nrows++;
    }
  }

  if ( mioStatus == -1 )
  {
    fprintf( stderr, "error parsing after row %d\n", i );
    CLOBBER_BANK;
    MetaioClose( env );
    return -1;
  }

  /* we have sucesfully parsed temples */
  MetaioClose( env );
  return nrows;
}

#define CLOBBER_SIM \
  while ( *simHead ) \
{ \
  thisSim = *simHead; \
  *simHead = (*simHead)->next; \
  LALFree( thisSim ); \
  thisSim = NULL; \
}


int
SimInspiralTableFromLIGOLw (
    SimInspiralTable   **simHead,
    const CHAR          *fileName,
    INT4                 startTime,
    INT4                 endTime
    )

{
  int                                   i, j, nrows;
  int                                   mioStatus;
  SimInspiralTable                     *thisSim = NULL;
  struct MetaioParseEnvironment         parseEnv;
  const  MetaioParseEnv                 env = &parseEnv;
  MetaTableDirectory tableDir[] =
  {
    {"waveform",            -1, 0},
    {"geocent_end_time",    -1, 1},
    {"geocent_end_time_ns", -1, 2},
    {"h_end_time",          -1, 3},
    {"h_end_time_ns",       -1, 4},
    {"l_end_time",          -1, 5},
    {"l_end_time_ns",       -1, 6},
    {"g_end_time",          -1, 7},
    {"g_end_time_ns",       -1, 8},
    {"t_end_time",          -1, 9},
    {"t_end_time_ns",       -1, 10},
    {"v_end_time",          -1, 11},
    {"v_end_time_ns",       -1, 12},
    {"end_time_gmst",       -1, 13},
    {"source",              -1, 14},
    {"mass1",               -1, 15},
    {"mass2",               -1, 16},
    {"eta",                 -1, 17},
    {"distance",            -1, 18},
    {"longitude",           -1, 19},
    {"latitude",            -1, 20},
    {"inclination",         -1, 21},
    {"coa_phase",           -1, 22},
    {"polarization",        -1, 23},
    {"psi0",                -1, 24},
    {"psi3",                -1, 25},
    {"alpha",               -1, 26},
    {"alpha1",              -1, 27},
    {"alpha2",              -1, 28},
    {"alpha3",              -1, 29},
    {"alpha4",              -1, 30},
    {"alpha5",              -1, 31},
    {"alpha6",              -1, 32},
    {"beta",                -1, 33},
    {"spin1x",              -1, 34},
    {"spin1y",              -1, 35},
    {"spin1z",              -1, 36},
    {"spin2x",              -1, 37},
    {"spin2y",              -1, 38},
    {"spin2z",              -1, 39},
    {"theta0",              -1, 40},
    {"phi0",                -1, 41},
    {"f_lower",             -1, 42},
    {"f_final",             -1, 43},
    {"mchirp",              -1, 44},
    {"eff_dist_h",          -1, 45},
    {"eff_dist_l",          -1, 46},
    {"eff_dist_g",          -1, 47},
    {"eff_dist_t",          -1, 48},
    {"eff_dist_v",          -1, 49},
    {"numrel_mode_min",     -1, 50},
    {"numrel_mode_max",     -1, 51},
    {"numrel_data",         -1, 52},
    {"amp_order",           -1, 53},
    {"taper",               -1, 54},
    {"bandpass",            -1, 55},
    {"process_id",          -1, 56},
    {"simulation_id",       -1, 57},
    {NULL,                   0, 0}
  };

  /* check that the bank handle and pointer are valid */
  if ( ! simHead )
  {
    fprintf( stderr, "null pointer passed as handle to simulation list" );
    return -1;
  }
  if ( *simHead )
  {
    fprintf( stderr, "non-null pointer passed as pointer to simulation list" );
    return -1;
  }

  /* open the sim_inspiral table file */
  mioStatus = MetaioOpenTable( env, fileName, "sim_inspiral" );
  if ( mioStatus )
  {
    fprintf( stderr, "error opening sim_inspiral table from file %s\n",
        fileName );
    return -1;
  }

  /* figure out the column positions of the simulated parameters */
  for ( i = 0; tableDir[i].name; ++i )
  {
    if ( (tableDir[i].pos = MetaioFindColumn( env, tableDir[i].name )) < 0 )
    {
      fprintf( stderr, "unable to find column %s\n", tableDir[i].name );
      MetaioClose(env);
      return -1;
    }
  }

  /* loop over the rows in the file */
  i = nrows = 0;
  while ( (mioStatus = MetaioGetRow(env)) == 1 )
  {
    INT4 geo_time = env->ligo_lw.table.elt[tableDir[1].pos].data.int_4s;

    /* get the injetcion time and check that it is within the time window */
    /* JC: AGAIN... HOPE PARENTHESES ARE RIGHT! */
    if ( ! endTime || ( geo_time > startTime && geo_time < endTime ) )
    {
      /* allocate memory for the template we are about to read in */
      if ( ! *simHead )
      {
        thisSim = *simHead = (SimInspiralTable *)
          LALCalloc( 1, sizeof(SimInspiralTable) );
      }
      else
      {
        thisSim = thisSim->next = (SimInspiralTable *)
          LALCalloc( 1, sizeof(SimInspiralTable) );
      }
      if ( ! thisSim )
      {
        fprintf( stderr, "could not allocate inspiral simulation\n" );
        CLOBBER_SIM;
        MetaioClose( env );
        return -1;
      }

      /* parse the row into the SimInspiralTable structure */
      for ( j = 0; tableDir[j].name; ++j )
      {
        REAL4 r4colData = env->ligo_lw.table.elt[tableDir[j].pos].data.real_4;
        REAL8 r8colData = env->ligo_lw.table.elt[tableDir[j].pos].data.real_8;
        INT4  i4colData = env->ligo_lw.table.elt[tableDir[j].pos].data.int_4s;
        INT8  i8colData = env->ligo_lw.table.elt[tableDir[j].pos].data.int_8s;
        if ( tableDir[j].idx == 0 )
        {
          snprintf(thisSim->waveform, LIGOMETA_WAVEFORM_MAX * sizeof(CHAR),
              "%s", env->ligo_lw.table.elt[tableDir[j].pos].data.lstring.data);
        }
        else if ( tableDir[j].idx == 1 )
        {
          thisSim->geocent_end_time.gpsSeconds = i4colData;
        }
        else if ( tableDir[j].idx == 2 )
        {
          thisSim->geocent_end_time.gpsNanoSeconds = i4colData;
        }
        else if ( tableDir[j].idx == 3 )
        {
          thisSim->h_end_time.gpsSeconds = i4colData;
        }
        else if ( tableDir[j].idx == 4 )
        {
          thisSim->h_end_time.gpsNanoSeconds = i4colData;
        }
        else if ( tableDir[j].idx == 5 )
        {
          thisSim->l_end_time.gpsSeconds = i4colData;
        }
        else if ( tableDir[j].idx == 6 )
        {
          thisSim->l_end_time.gpsNanoSeconds = i4colData;
        }
        else if ( tableDir[j].idx == 7 )
        {
          thisSim->g_end_time.gpsSeconds = i4colData;
        }
        else if ( tableDir[j].idx == 8 )
        {
          thisSim->g_end_time.gpsNanoSeconds = i4colData;
        }
        else if ( tableDir[j].idx == 9 )
        {
          thisSim->t_end_time.gpsSeconds = i4colData;
        }
        else if ( tableDir[j].idx == 10 )
        {
          thisSim->t_end_time.gpsNanoSeconds = i4colData;
        }
        else if ( tableDir[j].idx == 11 )
        {
          thisSim->v_end_time.gpsSeconds = i4colData;
        }
        else if ( tableDir[j].idx == 12 )
        {
          thisSim->v_end_time.gpsNanoSeconds = i4colData;
        }
        else if ( tableDir[j].idx == 13 )
        {
          thisSim->end_time_gmst = r8colData;
        }
        else if ( tableDir[j].idx == 14 )
        {
          snprintf(thisSim->source, LIGOMETA_SOURCE_MAX * sizeof(CHAR),
              "%s", env->ligo_lw.table.elt[tableDir[j].pos].data.lstring.data);
        }
        else if ( tableDir[j].idx == 15 )
        {
          thisSim->mass1 = r4colData;
        }
        else if ( tableDir[j].idx == 16 )
        {
          thisSim->mass2 = r4colData;
        }
        else if ( tableDir[j].idx == 17 )
        {
          thisSim->eta = r4colData;
        }
        else if ( tableDir[j].idx == 18 )
        {
          thisSim->distance = r4colData;
        }
        else if ( tableDir[j].idx == 19 )
        {
          thisSim->longitude = r4colData;
        }
        else if ( tableDir[j].idx == 20 )
        {
          thisSim->latitude = r4colData;
        }
        else if ( tableDir[j].idx == 21 )
        {
          thisSim->inclination = r4colData;
        }
        else if ( tableDir[j].idx == 22 )
        {
          thisSim->coa_phase = r4colData;
        }
        else if ( tableDir[j].idx == 23 )
        {
          thisSim->polarization = r4colData;
        }
        else if ( tableDir[j].idx == 24 )
        {
          thisSim->psi0 = r4colData;
        }
        else if ( tableDir[j].idx == 25 )
        {
          thisSim->psi3 = r4colData;
        }
        else if ( tableDir[j].idx == 26 )
        {
          thisSim->alpha = r4colData;
        }
        else if ( tableDir[j].idx == 27 )
        {
          thisSim->alpha1 = r4colData;
        }
        else if ( tableDir[j].idx == 28 )
        {
          thisSim->alpha2 = r4colData;
        }
        else if ( tableDir[j].idx == 29 )
        {
          thisSim->alpha3 = r4colData;
        }
        else if ( tableDir[j].idx == 30 )
        {
          thisSim->alpha4 = r4colData;
        }
        else if ( tableDir[j].idx == 31 )
        {
          thisSim->alpha5 = r4colData;
        }
        else if ( tableDir[j].idx == 32 )
        {
          thisSim->alpha6 = r4colData;
        }
        else if ( tableDir[j].idx == 33 )
        {
          thisSim->beta = r4colData;
        }
        else if ( tableDir[j].idx == 34 )
        {
          thisSim->spin1x = r4colData;
        }
        else if ( tableDir[j].idx == 35 )
        {
          thisSim->spin1y = r4colData;
        }
        else if ( tableDir[j].idx == 36 )
        {
          thisSim->spin1z = r4colData;
        }
        else if ( tableDir[j].idx == 37 )
        {
          thisSim->spin2x = r4colData;
        }
        else if ( tableDir[j].idx == 38 )
        {
          thisSim->spin2y = r4colData;
        }
        else if ( tableDir[j].idx == 39 )
        {
          thisSim->spin2z = r4colData;
        }
        else if ( tableDir[j].idx == 40 )
        {
          thisSim->theta0 = r4colData;
        }
        else if ( tableDir[j].idx == 41 )
        {
          thisSim->phi0 = r4colData;
        }
        else if ( tableDir[j].idx == 42 )
        {
          thisSim->f_lower = r4colData;
        }
        else if ( tableDir[j].idx == 43 )
        {
          thisSim->f_final = r4colData;
        }
        else if ( tableDir[j].idx == 44 )
        {
          thisSim->mchirp = r4colData;
        }
        else if ( tableDir[j].idx == 45 )
        {
          thisSim->eff_dist_h = r4colData;
        }
        else if ( tableDir[j].idx == 46 )
        {
          thisSim->eff_dist_l = r4colData;
        }
        else if ( tableDir[j].idx == 47 )
        {
          thisSim->eff_dist_g = r4colData;
        }
        else if ( tableDir[j].idx == 48 )
        {
          thisSim->eff_dist_t = r4colData;
        }
        else if ( tableDir[j].idx == 49 )
        {
          thisSim->eff_dist_v = r4colData;
        }
	else if ( tableDir[j].idx == 50 )
	{
	  thisSim->numrel_mode_min = i4colData;
	}
	else if ( tableDir[j].idx == 51 )
	{
	  thisSim->numrel_mode_max = i4colData;
	}
	else if ( tableDir[j].idx == 52 )
	{
          snprintf(thisSim->numrel_data, LIGOMETA_STRING_MAX * sizeof(CHAR),
              "%s", env->ligo_lw.table.elt[tableDir[j].pos].data.lstring.data);
	}
        else if ( tableDir[j].idx == 53 )
        {
          thisSim->amp_order = i4colData;
        }
        else if ( tableDir[j].idx == 54 )
        {
          snprintf(thisSim->taper, LIGOMETA_INSPIRALTAPER_MAX * sizeof(CHAR),
              "%s", env->ligo_lw.table.elt[tableDir[j].pos].data.lstring.data);
        }
        else if ( tableDir[j].idx == 55 )
        {
          thisSim->bandpass = i4colData;
        }
        else if ( tableDir[j].idx == 56 )
        {
          thisSim->process_id = i8colData;
        }
        else if ( tableDir[j].idx == 57 )
        {
          thisSim->simulation_id = i8colData;
        }
        else
        {
            CLOBBER_SIM;
            fprintf( stderr, "unknown column while parsing\n" );
            return -1;
        }
      }

      /* increase the count of rows parsed */
      ++nrows;
    }
  }

  if ( mioStatus == -1 )
  {
    fprintf( stderr, "error parsing after row %d\n", i );
    CLOBBER_SIM;
    MetaioClose( env );
    return -1;
  }

  /* we have sucesfully parsed temples */
  MetaioClose( env );
  return nrows;
}

#undef CLOBBER_SIM


#define CLOBBER_EVENTS \
  while ( *eventHead ) \
{ \
  thisEvent = *eventHead; \
  *eventHead = (*eventHead)->next; \
  LALFree( thisEvent ); \
  thisEvent = NULL; \
}




int
LALExtTriggerTableFromLIGOLw (
    ExtTriggerTable   **eventHead,
    CHAR               *fileName,
    INT4                startEvent,
    INT4                stopEvent
    )

{
  int                                   i, j, nrows;
  int                                   mioStatus;
  ExtTriggerTable                       *thisEvent = NULL;
  struct MetaioParseEnvironment         parseEnv;
  const  MetaioParseEnv                 env = &parseEnv;
  MetaTableDirectory tableDir[] =
  {
    {"det_alts",               -1,  0},
    {"det_band",               -1,  1},
    {"det_fluence",            -1,  2},
    {"det_fluence_int",        -1,  3},
    {"det_name",               -1,  4},
    {"det_peak",               -1,  5},
    {"det_peak_int",           -1,  6},
    {"det_snr",                -1,  7},
    {"email_time",             -1,  8},
    {"event_dec",              -1,  9},
    {"event_dec_err",          -1, 10},
    {"event_epoch",            -1, 11},
    {"event_err_type",         -1, 12},
    {"event_ra",               -1, 13},
    {"event_ra_err",           -1, 14},
    {"start_time",             -1, 15},
    {"start_time_ns",          -1, 16},
    {"event_type",             -1, 17},
    {"event_z",                -1, 18},
    {"event_z_err",            -1, 19},
    {"notice_comments",        -1, 20},
    {"notice_id",              -1, 21},
    {"notice_sequence",        -1, 22},
    {"notice_time",            -1, 23},
    {"notice_type",            -1, 24},
    {"notice_url",             -1, 25},
    {"obs_fov_dec",            -1, 26},
    {"obs_fov_dec_width",      -1, 27},
    {"obs_fov_ra",             -1, 28},
    {"obs_fov_ra_width",       -1, 29},
    {"obs_loc_ele",            -1, 30},
    {"obs_loc_lat",            -1, 31},
    {"obs_loc_long",           -1, 32},
    {"ligo_fave_lho",          -1, 33},
    {"ligo_fave_llo",          -1, 34},
    {"ligo_delay",             -1, 35},
    {"event_number_gcn",       -1, 36},
    {"event_number_grb",       -1, 37},
    {"event_status",           -1, 38},
    {NULL,                      0, 0}
  };


  /* check that the bank handle and pointer are void */
  if ( ! eventHead )
  {
    fprintf( stderr, "null pointer passed as handle to event list" );
    return -1;
  }
  if ( *eventHead )
  {
    fprintf( stderr, "non-null pointer passed as pointer to event list" );
    return -1;
  }

  /* open the sngl_inspiral table template bank file */
  mioStatus = MetaioOpenTable( env, fileName, "external_trigger" );
  if ( mioStatus )
  {
    fprintf( stdout, "no ext_trigger table in file %s\n", fileName );
    return 0;
  }

  /* figure out the column positions of the template parameters */
  for ( i = 0; tableDir[i].name; ++i )
  {
    if ( (tableDir[i].pos = MetaioFindColumn( env, tableDir[i].name )) < 0 )
    {
      fprintf( stderr, "unable to find column %s\n", tableDir[i].name );
      MetaioClose(env);
      return -1;
    }
  }

  /* loop over the rows in the file */
  i = nrows = 0;
  while ( (mioStatus = MetaioGetRow(env)) == 1 )
  {
    /* count the rows in the file */
    i++;

    /* stop parsing if we have reach the last row requested */
    if ( stopEvent > -1 && i > stopEvent )
    {
      break;
    }

    /* if we have reached the first requested row, parse the row */
    if ( i > startEvent )
    {
      /* allocate memory for the template we are about to read in */
      if ( ! *eventHead )
      {
        thisEvent = *eventHead = (ExtTriggerTable*)
          LALCalloc( 1, sizeof(ExtTriggerTable) );
      }
      else
      {
        thisEvent = thisEvent->next = (ExtTriggerTable *)
          LALCalloc( 1, sizeof(ExtTriggerTable) );
      }
      if ( ! thisEvent )
      {
        fprintf( stderr, "could not allocate inspiral template\n" );
        CLOBBER_EVENTS;
        MetaioClose( env );
        return -1;
      }

      /* parse the contents of the row into the InspiralTemplate structure */
      for ( j = 0; tableDir[j].name; ++j )
      {
        REAL4 r4colData = env->ligo_lw.table.elt[tableDir[j].pos].data.real_4;
        /* REAL8 r8colData = env->ligo_lw.table.elt[tableDir[j].pos].data.real_8; */
        INT4  i4colData = env->ligo_lw.table.elt[tableDir[j].pos].data.int_4s;

        if ( tableDir[j].idx == 0 )
        {
          snprintf( thisEvent->det_alts, LIGOMETA_STD * sizeof(CHAR),
              "%s", env->ligo_lw.table.elt[tableDir[j].pos].data.lstring.data );
        }
        else if ( tableDir[j].idx == 1 )
        {
          snprintf( thisEvent->det_band, LIGOMETA_STD * sizeof(CHAR),
              "%s", env->ligo_lw.table.elt[tableDir[j].pos].data.lstring.data );
        }
        else if ( tableDir[j].idx == 2 )
        {
          snprintf( thisEvent->det_fluence, LIGOMETA_STD * sizeof(CHAR),
              "%s", env->ligo_lw.table.elt[tableDir[j].pos].data.lstring.data );
        }
        else if ( tableDir[j].idx == 3 )
        {
          snprintf( thisEvent->det_fluence_int, LIGOMETA_STD * sizeof(CHAR),
              "%s", env->ligo_lw.table.elt[tableDir[j].pos].data.lstring.data );
        }
        else if ( tableDir[j].idx == 4 )
        {
          snprintf( thisEvent->det_name, LIGOMETA_STD * sizeof(CHAR),
              "%s", env->ligo_lw.table.elt[tableDir[j].pos].data.lstring.data );
        }
        else if ( tableDir[j].idx == 5 )
        {
          snprintf( thisEvent->det_peak, LIGOMETA_STD * sizeof(CHAR),
              "%s", env->ligo_lw.table.elt[tableDir[j].pos].data.lstring.data );
        }
        else if ( tableDir[j].idx == 6 )
        {
          snprintf( thisEvent->det_peak_int, LIGOMETA_STD * sizeof(CHAR),
              "%s", env->ligo_lw.table.elt[tableDir[j].pos].data.lstring.data );
        }
        else if ( tableDir[j].idx == 7 )
        {
          snprintf( thisEvent->det_snr, LIGOMETA_STD * sizeof(CHAR),
              "%s", env->ligo_lw.table.elt[tableDir[j].pos].data.lstring.data );
        }
        else if ( tableDir[j].idx == 8 )
        {
          thisEvent->email_time = i4colData;
        }
        else if ( tableDir[j].idx == 9 )
        {
          thisEvent->event_dec = r4colData;
        }
        else if ( tableDir[j].idx == 10 )
        {
          thisEvent->event_dec_err = r4colData;
        }
        else if ( tableDir[j].idx == 11 )
        {
          snprintf( thisEvent->event_epoch, LIGOMETA_STD * sizeof(CHAR),
              "%s", env->ligo_lw.table.elt[tableDir[j].pos].data.lstring.data );
        }
        else if ( tableDir[j].idx == 12 )
        {
          snprintf( thisEvent->event_err_type, LIGOMETA_STD * sizeof(CHAR),
              "%s", env->ligo_lw.table.elt[tableDir[j].pos].data.lstring.data );
        }
        else if ( tableDir[j].idx == 13 )
        {
          thisEvent->event_ra = r4colData;
        }
        else if ( tableDir[j].idx == 14 )
        {
          thisEvent->event_ra_err = r4colData;
        }
        else if ( tableDir[j].idx == 15 )
        {
          thisEvent->start_time = i4colData;
          /*  printf("start time:%d\n",i4colData); */
        }
        else if ( tableDir[j].idx == 16 )
        {
          thisEvent->start_time_ns = i4colData;
        }
        else if ( tableDir[j].idx == 17 )
        {
          snprintf( thisEvent->event_type, LIGOMETA_STD * sizeof(CHAR),
              "%s", env->ligo_lw.table.elt[tableDir[j].pos].data.lstring.data );
        }
        else if ( tableDir[j].idx == 18 )
        {
          thisEvent->event_z = r4colData;
        }
        else if ( tableDir[j].idx == 19 )
        {
          thisEvent->event_z_err = r4colData;
        }
        else if ( tableDir[j].idx == 20 )
        {
          snprintf( thisEvent->notice_comments, LIGOMETA_STD * sizeof(CHAR),
              "%s", env->ligo_lw.table.elt[tableDir[j].pos].data.lstring.data );
        }
        else if ( tableDir[j].idx == 21 )
        {
          snprintf( thisEvent->notice_id, LIGOMETA_STD * sizeof(CHAR),
              "%s", env->ligo_lw.table.elt[tableDir[j].pos].data.lstring.data );
        }
        else if ( tableDir[j].idx == 22 )
        {
          snprintf( thisEvent->notice_sequence, LIGOMETA_STD * sizeof(CHAR),
              "%s", env->ligo_lw.table.elt[tableDir[j].pos].data.lstring.data );
        }
        else if ( tableDir[j].idx == 23 )
        {
          thisEvent->notice_time = i4colData;
        }
        else if ( tableDir[j].idx == 24 )
        {
          snprintf( thisEvent->notice_type, LIGOMETA_STD * sizeof(CHAR),
              "%s", env->ligo_lw.table.elt[tableDir[j].pos].data.lstring.data );
        }
        else if ( tableDir[j].idx == 25 )
        {
          snprintf( thisEvent->notice_url, LIGOMETA_STD * sizeof(CHAR),
              "%s", env->ligo_lw.table.elt[tableDir[j].pos].data.lstring.data );
        }
        else if ( tableDir[j].idx == 26 )
        {
          thisEvent->obs_fov_dec = r4colData;
        }
        else if ( tableDir[j].idx == 27 )
        {
          thisEvent->obs_fov_dec_width = r4colData;
        }
        else if ( tableDir[j].idx == 28 )
        {
          thisEvent->obs_fov_ra = r4colData;
        }
        else if ( tableDir[j].idx == 29 )
        {
          thisEvent->obs_fov_ra_width = i4colData;
        }
        else if ( tableDir[j].idx == 30 )
        {
          thisEvent->obs_loc_ele = r4colData;
        }
        else if ( tableDir[j].idx == 31 )
        {
          thisEvent->obs_loc_lat = r4colData;
        }
        else if ( tableDir[j].idx == 32 )
        {
          thisEvent->obs_loc_long = r4colData;
        }
        else if ( tableDir[j].idx == 33 )
        {
          thisEvent->ligo_fave_lho = r4colData;
        }
        else if ( tableDir[j].idx == 34 )
        {
          thisEvent->ligo_fave_llo = r4colData;
        }
        else if ( tableDir[j].idx == 35 )
        {
          thisEvent->ligo_delay = r4colData;
        }
        else if ( tableDir[j].idx == 36 )
        {
          thisEvent->event_number_gcn= i4colData;
        }
        else if ( tableDir[j].idx == 37 )
        {
          snprintf( thisEvent->event_number_grb, 8 * sizeof(CHAR),
              "%s", env->ligo_lw.table.elt[tableDir[j].pos].data.lstring.data );
        }
        else if ( tableDir[j].idx == 38 )
        {
          thisEvent->event_status = i4colData;
        }
        else
        {
          CLOBBER_EVENTS;
          fprintf( stderr, "unknown column while parsing ext_trigger\n" );
          return -1;
        }
      }

      /* count the number of template parsed */
      nrows++;
    }
  }

  /* must be reduced to avoid stopping psocessing with triggers.xml
     because that file is generated corrupted (by just adding new triggers
     in new lines */
  /*
     if ( mioStatus == -1 )
     {
     fprintf( stderr, "error parsing after row %d\n", i );
     CLOBBER_EVENTS;
     MetaioClose( env );
     return -1;
     }
   */

  /* we have sucesfully parsed temples */
  MetaioClose( env );
  return nrows;
}


#undef CLOBBER_EVENTS
