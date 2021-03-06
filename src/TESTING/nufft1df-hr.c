#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <complex.h>
#include <float.h>
#include "nufft.h"
#include "0618/elapsed.c"
#include "0618/raw_trace.c"

#ifndef	M_PI
# define M_PI       3.14159265358979323846  /* pi */
#endif
#ifndef M_PI_2
# define M_PI_2     1.57079632679489661923  /* pi/2 */
#endif
#ifndef M_PI_4
# define M_PI_4     0.78539816339744830962  /* pi/4 */
#endif

double cabs_sq(COMPLEX16 c)
{
	return c.r * c.r + c.i * c. i;
}

float cabsf_sq(COMPLEX8 c)
{
	return c.r * c.r + c.i * c. i;
}

double cabs_(COMPLEX16 c)
{
	return sqrt(c.r * c.r + c.i * c. i);
}

float cabsf_(COMPLEX8 c)
{
	return sqrtf(c.r * c.r + c.i * c. i);
}

COMPLEX8 cmplxf(float r, float i)
{
	COMPLEX8 ctemp;
	ctemp.r=r;
	ctemp.i=i;
	return ctemp;
}

COMPLEX16 cmplx(double r, double i)
{
	COMPLEX16 ctemp;
	ctemp.r=r;
	ctemp.i=i;
	return ctemp;
}

void errcompf(PCOMPLEX8 fk0, PCOMPLEX8 fk1, int n, double *err)
{
	double salg=0.0f,ealg=0.0f;

	for(int k = 0 ; k < n; k ++){
		COMPLEX16 ctemp;
		ctemp.r = fk1[k].r-fk0[k].r;
		ctemp.i = fk1[k].i-fk0[k].i;
		ealg += cabs_sq(ctemp);
		ctemp.r=(double)fk0[k].r;
		ctemp.i=(double)fk0[k].i;
		salg += cabs_sq(ctemp);
		//ealg += ( (fk1[k].r-fk0[k].r)*(fk1[k].r-fk0[k].r) +
		//		(fk1[k].i-fk0[k].i)*(fk1[k].i-fk0[k].i) );
		//salg +=  (fk0[k].r*fk0[k].r + fk0[k].i*fk0[k].i);
		printf("%d:fk0=(%f,%f) fk1=(%f,%f)\n",k, fk0[k].r, fk0[k].i, fk1[k].r,fk1[k].i);
	}
	*err =sqrt(ealg/salg);
	printf("ealg/salg:%g/%g err=%e\n",ealg, salg, *err);
}

#define	MX	(512)

void nufft_hr(void)
{
	int nj = 51;
	int ier=0, iflag=-1;
	int ms= 32;
	float eps=1e-5;
	double err=0.0f;
	double observingT=0.0f;
	double deltaF=0.0f,deltaBPM=0.0f;
	COMPLEX8 cj[MX];//,cj0[MX],cj1[MX];
    COMPLEX8 fk0[MX],fk1[MX];
	float *xj;
	float *elapsed[]={elapsed4, elapsed5, elapsed10};
	float *raw_trace[]={raw_trace4, raw_trace5, raw_trace10};
/*
 * xj : non-equspaced x position
 * cj : y position
 */

	//int size_x=sizeof(elapsed[0])/sizeof(float);
	//int size_raw=sizeof(raw_trace[0])/sizeof(float);
	int size_x=sizeof(elapsed5)/sizeof(float);
	int size_raw=sizeof(raw_trace5)/sizeof(float);
	printf("size_x=%d,size_raw=%d\n", size_x, size_raw);
	nj=size_x;
	xj=elapsed[1];	//time, spatial domain, x position, non-uniform
	observingT = elapsed[1][size_x-1];//100ms->1second
	/* xj[] is normalized to [-PI,+PI]
	 * (2*PI / T) * (t - T/2)
	 */
	for(int i = 0; i < size_x; i ++){
		xj[i]= (elapsed[1][i]- observingT/2.0f) * (2.0f * M_PI / observingT);
		printf("%f\n", xj[i]);
	}

	/* nufft does not change the observing period, but the sampling number
	 * from N->2M, so only the highest working frequency changes.
	 */
	observingT /= 10.0f;//100ms->1second
	deltaF=1.0f/observingT;
	deltaBPM=60.0f * deltaF;

	/* ms points in frequency domain, so at least
	 * 180.0bpm/deltaBPM are needed.
	 * ms is oversampling 2*ms in time domain, but
	 * ms points in frequency domain.
	 * half of ms, ms/2, are mirrored, so 2*ms are needed.
	 */
	double dms=ceil(180.0f/deltaBPM)*2.0f;
	ms = next235_(&dms);

	printf("nj=%d, ms=%d\n", nj,ms);
	printf("observingT=%f, dF=%f, dBPM=%f\n", observingT, deltaF, deltaBPM);
	for(int i = 0; i < size_raw; i ++){
		cj[i].r= raw_trace[1][i];
		cj[i].i=0.0f;
	}

	iflag=-1;//forward
	dirft1d1f_(&nj, xj, cj, &iflag, &ms, fk0);
	iflag=-1;	//forward
	nufft1d1ff90_ffte_(&nj, xj, cj, &iflag, &eps, &ms, fk1, &ier);
	errcompf(fk0, fk1, ms, &err);
	printf("ier=%d, err=%e\n", ier, err);

	int mI=0;
	double dMax=0.0f;
	printf("nj=%d, ms=%d\n", nj,ms);
	printf("observingT=%f, dF=%f, dBPM=%f\n", observingT, deltaF, deltaBPM);
	//for(int i = 0; i < ms; i ++){
	for(int i = ms/2; i < ms; i ++){
		float bpm;
		if(i < ms/2){
			bpm=(ms/2-i)*deltaBPM;
			printf("\n%d:%f,%fbpm\n", i , (ms/2-i)*deltaF, bpm);
		}else{
			bpm=(i-ms/2)*deltaBPM;
			printf("\n%d:%f,%fbpm\n", i , (i-ms/2)*deltaF, bpm);
		}
		printf("fk0=(%f,%f),fk1=(%f,%f)\n", fk0[i].r,fk0[i].i,
			fk1[i].r,fk1[i].i);
		printf("(%f,%f, %f)\n",cabsf_sq(fk0[i]), cabsf_sq(fk1[i]), dMax);

		if( (bpm < 180.0f) && (bpm > 50.0f) && (cabsf_sq(fk1[i]) > dMax) ){
			dMax =cabsf_sq(fk1[i]);
			mI = i;
		}
	}
	if(mI < ms/2)
		printf("mI=%d,bpm=%f, max=%f\n", mI, (ms/2-mI)*deltaBPM, dMax);
	else
		printf("mI=%d,bpm=%f, max=%f\n", mI, (mI - ms/2)*deltaBPM, dMax);
}

//http://forum.cvapp.org/viewtopic.php?f=21&t=431&sid=4e8c86381ce6b3a43a53d7d8f8b22058
void nufft_sine(void)
{
	int ier,iflag,j,k1,ms,nj;
	float xj[MX];
	float eps=1e-5;
	double err;
	COMPLEX8 cj[MX];
	COMPLEX8 fk0[MX],fk1[MX];
	double observingT=0.0f;
//
//     --------------------------------------------------
//     create some test data
//     --------------------------------------------------
	ms = 64;	//oversampling 2*ms in time domain
	nj = 77;
	//fortran array index from "1", but c is from "0"
	printf("input xj, nj and cj\n");
	printf("nj bin=%f\n", M_PI*2.0/(nj));
	for(k1 = -nj/2; k1 <= (nj-1)/2;k1++){
		j = k1+nj/2;
		//xj[j] = M_PI * cos(-M_PI*(j+1)/nj);
		//float r_s=M_PI*2.0/(nj*2) * ((rand()%10 - 5 )/5.0f);
		float r_s=M_PI*2.0/(nj) * ((rand()%10 - 5.0 )/5.0f);
		xj[j] = M_PI * k1/(nj/2) + r_s;//generate a random non-equispaced
		//cj[j] = cmplxf(cos(5.0*xj[j])+cos(1.0*xj[j]),
		//			   sin(2.0*xj[j]));
		cj[j].r = cos(5.0*xj[j])+cos(1.0*xj[j]);
		cj[j].i = sin(2.0*xj[j]);

		printf("%d,%d:%f, %f,(%f,%f)\n", k1, j, r_s, xj[j], cj[j].r, cj[j].i);
	}
	printf("dirft1d1f_\n");
	iflag=-1;	//forward
	dirft1d1f_(&nj, xj, cj, &iflag, &ms, fk0);
	printf("nufft1d1ff90_ffte_\n");
	iflag=-1;	//forward
	nufft1d1ff90_ffte_(&nj, xj, cj, &iflag, &eps, &ms, fk1, &ier);
	errcompf(fk0, fk1, ms, &err);
	printf("ier=%d, err=%e\n\n", ier, err);
	printf("nj=%d, ms=%d\n",nj,ms);
	for(int i = 0; i < ms; i ++){
		printf("%d:fk0=(%f,%f),fk1=(%f,%f)\n", i, fk0[i].r,fk0[i].i,
			fk1[i].r,fk1[i].i);
		printf("(%f,%f)\n",cabsf_sq(fk0[i]), cabsf_sq(fk1[i]));
	}
}

int main(int argc, char **argv)
{
	srand(time(NULL));
	//nufft_sine();
	nufft_hr();
}