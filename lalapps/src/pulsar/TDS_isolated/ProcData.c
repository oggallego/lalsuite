/*********************************************************************************/
/*   This intermediate code reads in the Bk's generated by the TargetedPulsars.c */
/*   code.  This code proceeds to keep only the data that is in 30 minute chunks.*/
/*   It output three files (1) same data as input except that only 30 min chucks */
/*   are kept (2) an ascii file with the Bk's average for 30 minutes and the     */
/*   corresponding variance (for gaussian likelihood) and, (3) a file with the   */
/*   reduced chi square and its error.                                           */
/*                                                                               */
/*			               R�jean Dupuis                             */
/*                                                                               */
/*                       University of Glasgow - last modified 26/03/2004        */
/*********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

FILE* tryopen(char *name, char *mode){
 int count=0;
 FILE *fp;
 while (!(fp=fopen(name, mode))){
   fprintf(stderr,"Unable to open file %s in mode %s\n", name, mode);
   fflush(stderr);
   if (count++<10)
     sleep(10);
   else
     exit(3);
  }
   return fp;
 } 

typedef struct
tagComplex
{
  double re;            
  double im;
} Complex;

#define MAXLEN 100000

int main(int argc, char **argv)
{
   /* declare variables */
  FILE *fpin, *fpin2, *fpout1, *fpout2, *fpout3;
  Complex B[MAXLEN], C[MAXLEN], var, avg, sum;
  double t[MAXLEN],  Ct[MAXLEN], chisquare1 = 0.0, df;
  float psr_freq[100];
  int i=0, j=0, cntr=0, k=0, itmp, h=0, ii,i0, SN;
  int num_psr,  cnt=0, jj = 0, irun;
  char outfile1[128]="", outfile2[128]="", outfile3[128]="";
  char infile[64] = "pulsarlist.txt";
  char infile2[128]="";
  char psr_name[100][64];
  
  if (argc !=6){
   fprintf(stderr, "inputdir SN detector outdir irun\n");  
   fprintf(stderr,"There were argc= %d arguments:\n",argc -1);
   fflush(stderr);
   return 2;
  }

  irun = atoi(argv[5]);
  if (irun != 2 && irun != 3)
  {
     fprintf(stderr, "irun must be 2 (S2) or 3 (S3)!\n");
     return(2);
  }
  
  /* read file with list of pulsars (pulsarlist.txt) */
  fpin2=tryopen(infile,"r"); 
  fscanf(fpin2,"%d",&itmp);
  for (i=0;i<itmp;i++)  fscanf(fpin2,"%d\t%s\t%f",&num_psr, &psr_name[i][0], &psr_freq[i]); 
 
  fclose(fpin2);
 
  SN = atoi(argv[2]);  
  if (SN < 0 || SN > 60)
  {
    fprintf(stderr, "Unreasonable value for SN!\n");
    return(0);
  }
  /* begin loop over pulsars */ 
  for (k=0;k<num_psr;k++)
  {
    cnt=0;
    /* create output/input filenames */
    sprintf(outfile1,"%s/outfine.%s_%s.S%d_chi_%d", argv[4],psr_name[k],argv[3],irun,atoi(argv[2])); 
    fpout1=tryopen(outfile1,"w");  
    sprintf(outfile2,"%s/hist_%s.S%d_%d", argv[4],argv[3],irun,atoi(argv[2])); 
    fpout2=tryopen(outfile2,"a");  
    sprintf(outfile3,"%s/outfine.%s_%s.S%d_%d", argv[4],psr_name[k],argv[3],irun, atoi(argv[2])); 
    fpout3=tryopen(outfile3,"w");  
    sprintf(infile2,"%s/outfine.%s_%s.S%d", argv[1],psr_name[k],argv[3],irun); 
    fpin=tryopen(infile2,"r"); 
    i=0;
    cntr = 0;

  while (!feof(fpin))
  {
    h=2; 
    i0 = i;
    fscanf(fpin,"%lf\t%lf\t%lf",&t[i],&B[i].re, &B[i].im); 
    i++;
    fscanf(fpin,"%lf\t%lf\t%lf",&t[i],&B[i].re, &B[i].im); 
    i++;
    jj = 0;
    /* read locked stretch of data 
       if detector out of appears out of lock for less than 
       3 minutes then consider data still part of the 
       same lock stretch (because this could be due to data 
       that was thrown away due to restarting the IIR filters)
       however if there are more that 5 such gaps then also 
       end lock stretch */
    
    while ((t[i-1]-t[i-2] < 180) && !feof(fpin) && jj<5) {
      fscanf(fpin,"%lf\t%lf\t%lf",&t[i],&B[i].re, &B[i].im); 
      if (!feof(fpin)) {i++;  h++;}
      if (t[i-1]-t[i-2] != 60) jj++;   
    }
   /* if locked stretch less than SN, say 30 minutes, then read next stretch */
    if (h<SN) 
         continue;
    
    j=0;
    /* otherwise for each SN chunk calculate avg and variance and write out the Bk's */
    while(j+SN<h)
    {
      avg.re = 0.0; avg.im = 0.0;
      var.re = 0.0; var.im = 0.0;
      sum.re =0.0; sum.im = 0.0;

      for (ii=(i0+j);ii<(i0+SN+j); ii++)
      { 
         sum.re += B[ii].re;
         sum.im += B[ii].im; 
      }
      
      avg.re = sum.re / (double)SN;
      avg.im = sum.re / (double)SN;
      for (ii=i0+j;ii<i0+SN+j; ii++)
      {
        var.re += (B[ii].re - avg.re)*(B[ii].re - avg.re);
	var.im += (B[ii].im - avg.im)*(B[ii].im - avg.im);
      }
      var.re /= (double) SN; var.im /= (double)SN;
      var.re /= (double) (SN - 1); var.im /= (double)(SN - 1);
      for (ii=i0+j;ii<i0+SN+j; ii++)
      {  	 
        C[cnt].re = B[ii].re;
        C[cnt].im = B[ii].im;
	Ct[cnt] = t[ii];
	cnt++;
      }
    
    j+=SN;
    }   
     i = i - j - SN + h;
 }
                
  fclose(fpin);
  avg.re = 0.0;
  avg.im = 0.0;
 
  /* degrees of freedom */  
  df = 2.0*(double)cnt/SN;
  
  chisquare1=0.0;
  
/* calculate mean and sample variance of each 30 minute chunk */  
  for (i=0;i<cnt;i+=SN)
  {  
    for (j=0;j<SN;j++)
    {
      avg.re += C[i+j].re;
      avg.im += C[i+j].im;
    }    
      avg.re /= SN;
      avg.im /= SN;     
      var.re = 0.0;
      var.im = 0.0;
    for (j=0;j<SN;j++)
    {
      var.re += (C[i+j].re - avg.re)*(C[i+j].re - avg.re);
      var.im += (C[i+j].im - avg.im)*(C[i+j].im - avg.im);
    }   
    
    var.re /=(SN - 1.0);
    var.im /=(SN - 1.0);
   
   /* divide by SN to get variance of average */  
    var.re /= SN;
    var.im /= SN;;     
   
    chisquare1 += (avg.re*avg.re)/var.re + (avg.im*avg.im)/var.im;	     
    fprintf(fpout1, "%f\t%e\t%e\t%e\t%e\n", Ct[i], avg.re, avg.im, var.re, var.im);
  }
  fprintf(fpout2, "%s\t%f\t%f\t%f\t%f\t%d\n", psr_name[k], psr_freq[k],chisquare1/df, sqrt(2.0*df)/df, df, SN);
   
  for (i=0;i<cnt;i++)
    fprintf(fpout3, "%f\t%e\t%e\n", Ct[i], C[i].re, C[i].im);
	
  fclose(fpout1); 
  fclose(fpout2);
  fclose(fpout3);
  }   /* end loop over pulsars */ 

 return(0);
}
