/**TDB2TT Convert from  barycentric dynamical time (TDB) to terrestrial
 *        time (TT) to an accuracy of nanoseconds (if deltaT is accurate)
 *        using the routines from the International Astronomical Union's
 *        library that do not require external ephemeris data.
 *
 *INPUTS: Jul1,Jul2 Two parts of a Julian date given in TT. The units of
 *                  the date are days. The full date is the sum of both
 *                  terms. The date is broken into two parts to provide
 *                  more bits of precision. It does not matter how the date
 *                  is split.
 *       deltaTTUT1 An optional parameter specifying the offset between TT
 *                  and UT1 in seconds. If this parameter is omitted or an
 *                  empty matrix is passed, then the value of the function
 *                  getEOP will be used. Since getEOP takes UTC (which
 *                  comes from UT1) and we have only TDB (which is close),
 *                  a few iterations are performed to try to get the
 *                  correct result.
 *         clockLoc An optional 3X1 vector specifying the location of the
 *                  clock in the Terrestrial Intermediate Reference System
 *                  (TIRS), though it would not make much of a difference
 *                  if the International Terrestrial Reference System
 *                  (ITRS) were used. The units are meters. Due to
 *                  relativistic effects, clocks that are synchronized with
 *                  respect to TT are not synchronized with respect to TDB.
 *                  If this parameter is omitted, then a clock at the
 *                  center of the Earth is used.
 *        
 *OUTPUTS:Jul1,Jul2 Two parts of a Julian date given in TDB.
 *
 *This function relies on a number of functions in the International
 *Astronomical Union's Standards of Fundamental Astronomy library.
 * 
 *The main implementation issue is that if deltaT is not provided (and
 *generally, one probably would not be expected to know it as it is not
 *tabulated in TDB), one has to iterate using the getEOP function to get
 *the correct offset to use. However, even if the deltaT parameter is
 *given, one still has to iterate, because values in UT1 are required and
 *the iauTtut1 function requires UT1.
 *
 *The algorithm can be compiled for use in Matlab  using the 
 *CompileCLibraries function.
 *
 *The algorithm is run in Matlab using the command format
 *[Jul1,Jul2]=TDB2TT(Jul1,Jul2,deltaTTUT1,clockLoc);
 *or
 *[Jul1,Jul2]=TDB2TT(Jul1,Jul2);
 *
 *Many temporal coordinate systems standards are compared in [1].
 *
 *REFERENCES:
 *[1] D. F. Crouse, "An Overview of Major Terrestrial, Celestial, and
 *    Temporal Coordinate Systems for Target Tracking," Formal Report,
 *    Naval Research Laboratory, no. NRL/FR/5344--16-10,279, 10 Aug. 2016,
 *    173 pages.
 *
 *April 2015 David F. Crouse, Naval Research Laboratory, Washington D.C.
 */
/*(UNCLASSIFIED) DISTRIBUTION STATEMENT A. Approved for public release.*/

 /*This header is required by Matlab.*/
#include "mex.h"
/*This header is for the SOFA library.*/
#include "sofa.h"
#include "MexValidation.h"

//Function prototype
double getDeltaTFromEOP(double TT1,double TT2);

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]) {
    double TDB1,TDB2,TT1,TT2,deltaTTUT1=0;
    double u, v, elon;
    int retVal,curIter;
    bool iterateDeltaT;
    
    if(nrhs<2||nrhs>4){
        mexErrMsgTxt("Wrong number of inputs.");
        return;
    }

    if(nlhs>2){
        mexErrMsgTxt("Wrong number of outputs.");
        return;
    }
    
    TDB1=getDoubleFromMatlab(prhs[0]);
    TDB2=getDoubleFromMatlab(prhs[1]);

    //The initial estimate for TT is TDB. An initial estimate for TT is
    //needed as other values are parameterized by TT.
    TT1=TDB1;
    TT2=TDB2;
    
    //If the deltaT parameter is given and is not just an empty matrix.
    if(nrhs>2&&!mxIsEmpty(prhs[2])) {
        iterateDeltaT=false;
        deltaTTUT1=getDoubleFromMatlab(prhs[2]);
    } else {
        iterateDeltaT=true;
    }
    
    if(nrhs>3&&!mxIsEmpty(prhs[3])) {
        double *xyzClock;
        
        if(mxGetM(prhs[3])!=3||mxGetN(prhs[3])!=1) {
            mexErrMsgTxt("The dimensionality of the clock location is incorrect.");
            return;
        }
        checkRealDoubleArray(prhs[3]);
        
        xyzClock=(double*)mxGetData(prhs[3]);
        //Convert from meters to kilometers.
        xyzClock[0]/=1000;xyzClock[1]/=1000;xyzClock[2]/=1000;
        
        u=sqrt(xyzClock[0]*xyzClock[0] + xyzClock[1]*xyzClock[1]);
        v=xyzClock[2];
        elon=atan2(xyzClock[1],xyzClock[0]);
    } else {
        u=0;
        v=0;
        elon=0;
    }

    //Assume convergence in 2 iterations.
    for(curIter=0;curIter<2;curIter++) {
        double Jul1UT1, Jul2UT1,UT1Frac,deltaT;
        
        if(iterateDeltaT) {
            deltaTTUT1=getDeltaTFromEOP(TT1,TT2);
        }
        
        //Get UT1 using the current estimate of TT.
        retVal=iauTtut1(TT1, TT2, deltaTTUT1, &Jul1UT1, &Jul2UT1);
        switch(retVal) {
            case -1:
                mexErrMsgTxt("Unacceptable date provided.");
                return;
            case 1:
                mexWarnMsgTxt("Dubious year provided\n");
                break;
            default:
                break;
        }
        
        //Find the fraction of a Julian day in UT1
        UT1Frac=(Jul1UT1-floor(Jul1UT1))+(Jul2UT1-floor(Jul2UT1));
        UT1Frac=UT1Frac-floor(UT1Frac);
        
        /*Compute TDB-TT.*/  
        deltaT = iauDtdb(TDB1, TDB2, UT1Frac, elon, u, v);
        //TDB -> TT
        retVal = iauTdbtt(TDB1, TDB2, deltaT, &TT1, &TT2);
        if(retVal!=0) {
           mexErrMsgTxt("An error occured during an intermediate conversion to TDB.\n");
           return;
        }
    }
    
    plhs[0]=doubleMat2Matlab(&TT1,1, 1);
    if(nlhs>1) {
        plhs[1]=doubleMat2Matlab(&TT2,1, 1);
    }
}

double getDeltaTFromEOP(double TT1,double TT2) {
/**GETDELTATFROMEOP Get the deltaTTUT1 parameter given a time in
 *                  terrestrial time using the function getEOP. This will
 *                  call Matlab errors if parameter problems arise.
 */
    
    mxArray *retVals[4];
    mxArray *JulUTCMATLAB[2];
    double deltaT, JulUTC[2];
    int retVal;

    //Get the time in UTC to look up the parameters by going to TAI and
    //then UTC.
    retVal=iauTttai(TT1, TT2, &JulUTC[0], &JulUTC[1]);
    if(retVal!=0) {
        mexErrMsgTxt("An error occurred computing TAI.");
    }
    retVal=iauTaiutc(JulUTC[0], JulUTC[1], &JulUTC[0], &JulUTC[1]);
    switch(retVal){
    case 1:
        mexWarnMsgTxt("Dubious Date entered.");
        break;
    case -1:
        mexErrMsgTxt("Unacceptable date entered");
        break;
    default:
        break;
    }

    JulUTCMATLAB[0]=doubleMat2Matlab(&JulUTC[0],1,1);
    JulUTCMATLAB[1]=doubleMat2Matlab(&JulUTC[1],1,1);

    //Get the Earth orientation parameters for the given date.
    mexCallMATLAB(4,retVals,2,JulUTCMATLAB,"getEOP");
    //This is TT-UT1
    deltaT=getDoubleFromMatlab(retVals[3]);
    //Free the returned arrays.
    mxDestroyArray(retVals[0]);
    mxDestroyArray(retVals[1]);
    mxDestroyArray(retVals[2]);
    mxDestroyArray(retVals[3]);
    mxDestroyArray(JulUTCMATLAB[0]);
    mxDestroyArray(JulUTCMATLAB[1]);
    
    return deltaT;
}

/*LICENSE:
%
%The source code is in the public domain and not licensed or under
%copyright. The information and software may be used freely by the public.
%As required by 17 U.S.C. 403, third parties producing copyrighted works
%consisting predominantly of the material produced by U.S. government
%agencies must provide notice with such work(s) identifying the U.S.
%Government material incorporated and stating that such material is not
%subject to copyright protection.
%
%Derived works shall not identify themselves in a manner that implies an
%endorsement by or an affiliation with the Naval Research Laboratory.
%
%RECIPIENT BEARS ALL RISK RELATING TO QUALITY AND PERFORMANCE OF THE
%SOFTWARE AND ANY RELATED MATERIALS, AND AGREES TO INDEMNIFY THE NAVAL
%RESEARCH LABORATORY FOR ALL THIRD-PARTY CLAIMS RESULTING FROM THE ACTIONS
%OF RECIPIENT IN THE USE OF THE SOFTWARE.*/
