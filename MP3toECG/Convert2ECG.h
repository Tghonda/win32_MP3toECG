#pragma once
#include <atltime.h>
#include "Arguments.h"

static const char *tblFilePath441 = "GFactorTable441.dat";
static const char *tblFilePath480 = "GFactorTable480.dat";
static const int tbl_minf = 1000;
static const int tbl_maxf = 2400;
static const int tbl_size = (tbl_maxf-tbl_minf);
const int SamplingRate441 = 44100;
const int SamplingRate480 = 48000;
//const int DataRate = 450;
const int DataRate = 2000;
const double k1mSecond = 1.0/1000.0;
const float	thresholdLevel = 4.0;


const int MaxECGTable = 150000;
const int MaxECG = 7200;

#pragma warning(disable : 4200)
struct gaborFactorTbl {
    __int32 dxlen[tbl_size];
    __int32 offset[tbl_size];
    float tbl[];
};


class Convert2ECG
{
private:
	gaborFactorTbl *gtbl;
	bool	optVerbose;
	bool	optWholedata;
	bool	optRaw;
	bool	optThroughCalibration;
	int		optSerialNo;
	double	optDataOnly;
	bool	optDebug;
	CTime	procTime;
	

	int		samplingRateI;
	float	samplingRateF;

	float	*pcmdata;
	double	durationPCMTime;
	double	currentPCMTime;

	int		rawECG[MaxECGTable];
	int		idxECG;
	int		serialNo;
	int		serialSum;
	int		checkSum;

private:
	int setupGTable( int samplingrate, std::string currentPath );
	int loadSoundData( const char* soundf );
	int pcm2ecg( void );
	int covertWholeData(void);
	int convetECGData(void);
	float *Convert2ECG::getCurrentPcmp();
	int detectHeader(void);
	int analyzeCalibration(void);
	int analyzeSerialNo(void);
	int collectData(void);
	void outECGRaw(char *fpath);
	void outECG(char *fpath);
	void gabor_transform(float pcm[], int baseF, int stepF, float wt[], int wt_len);
	int fvconvert(float pcm[], int minF, int maxF, int pitch);
	int fast_fcnv(float pcm[], int minF, int maxF, int pitch);

	void fconvTest( void );
	void maketabl(void);

public:
	Convert2ECG(void);
	virtual ~Convert2ECG(void);
	int convert(Arguments arg);
	void outStatus(Arguments arg, int status);
};

