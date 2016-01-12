#include "stdafx.h"
#include "Convert2ECG.h"
#include "ErrorStatusNo.h"

#include <fstream>
#include <iostream>
#include <locale.h>

Convert2ECG::Convert2ECG(void)
{
	gtbl = nullptr;
	pcmdata = nullptr;

	optVerbose = false;
	optWholedata = false;
	optRaw = false;
	optThroughCalibration = false;
	optSerialNo = 0;

	memset(rawECG, 0, sizeof(rawECG));
	idxECG = 0;
	serialNo = 0;
	procTime = CTime::GetCurrentTime();
}

Convert2ECG::~Convert2ECG(void)
{
	if (gtbl) 		free(gtbl);
	if (pcmdata)	free(pcmdata);
}

int Convert2ECG::setupGTable( int samplingrate, std::string currentPaht )
{
	std::ifstream fs;
	std::string gtblPath = std::string(currentPaht);
	gtblPath.append((samplingrate == SamplingRate441) ? tblFilePath441 : tblFilePath480);

	fs.open(gtblPath.c_str(), std::ios::in | std::ios::binary);
	if (fs.fail()) {
		std::cerr << "Error! cannot open GDataFile:" << gtblPath.c_str() << "\n";
		return -1;
	}

	fs.seekg(0, std::ios::end);
	std::streamsize dataSize = fs.tellg();
	fs.clear();
	fs.seekg(0, std::ios::beg);

	gtbl = (gaborFactorTbl *)malloc((size_t)dataSize);
	if (!gtbl) {
		std::cerr << "Error! out of memory. (" << dataSize << ")Byte\n";
		return -1;
	}

	fs.read((char *)gtbl, dataSize);
	if (fs.fail()) {
		std::cerr << "Error! cannot read GDataFile:" << gtblPath.c_str() << "\n";
		return -1;
	}

	fs.close();

	return ERR_OK;
}

typedef struct {
	union {
		char	chankId[4];
		__int32 chankIdVal;
	};
	__int32	chankSize;
} _chankHeader;

#define CHANK_RIFF	0x46464952		//  'RIFF'
#define CHANK_WAVE	0x45564157		//  'WAVE'
#define CHANK_fmt	0x20746d66		//	'fmt '
#define CHANK_data	0x61746164		//	'data'


typedef struct {
	__int16	 wFormatTag;		// LinearPCM:	1
	__int16	 wChannels;			// Monoral:		1
	__int32	 dwSamplesPerSec;	// 44100
	__int32	 dwAvgBytesPerSec;	// 44100*2
	__int16 wBlockAlign;		// 2
	__int16 wBitsPerSample;		// 16
} _fmtChunk;

int Convert2ECG::loadSoundData( const char* soundf )
{
	int samples = 0;
	_chankHeader chk;
	std::ifstream fs;

	fs.open(soundf, std::ios::in | std::ios::binary);
	if (fs.fail()) {
		std::cerr << "Error! cannot open input file:" << soundf << "\n";
		return -1;
	}

	while (1) {
		fs.read((char *)&chk, sizeof(chk));
		if (fs.fail()) {
			std::cerr << "Error! wav file '" << soundf << "' is broken. \n";
			return -1;
		}
		if (chk.chankIdVal == CHANK_RIFF) {
			_int32 tagWave;
			fs.read((char *)&tagWave, sizeof(tagWave));
			if (tagWave != CHANK_WAVE) {
				std::cerr << "Error! wav file is not 'WAVE' format\n";
				return -1;
			}
		}
		else if (chk.chankIdVal == CHANK_fmt) {
			_fmtChunk fmt;
			memset(&fmt, 0, sizeof(fmt));
			if (chk.chankSize < sizeof(fmt)) {
				std::cerr << "Error! 'fmt ' chank size is worng:" << chk.chankSize << "\n";
				return -1;
			}
			fs.read((char *)&fmt, sizeof(fmt));
			if (fmt.wFormatTag != 1) {
				std::cerr << "Error! wave file is not Linear PCM:" << fmt.wFormatTag << "\n";
				return -1;
			}
			if (fmt.wChannels != 1) {
				std::cerr << "Error! wave file is not MONORAL:" << fmt.wChannels << "\n";
				return -1;
			}
			if (fmt.dwSamplesPerSec == SamplingRate441 || fmt.dwSamplesPerSec == SamplingRate480) {
				samplingRateI = fmt.dwSamplesPerSec;
				samplingRateF = (float)fmt.dwSamplesPerSec;
			}
			else {
				std::cerr << "Error! Samplingrate is not 44100 or 48000:" << fmt.dwSamplesPerSec << "\n";
				return -1;
			}
			if (fmt.wBitsPerSample != 16) {
				std::cerr << "Error! Sampl data size is not 16bits:" << fmt.wBitsPerSample << "\n";
				return -1;
			}
		}
		else if (chk.chankIdVal == CHANK_data) {
			samples = chk.chankSize/2;
			break;
		}
		else {
			// onother chank! skip it.
			fs.seekg(chk.chankSize, std::ios::cur);
		}
	}

	if (samples == 0) {
		std::cerr << "Error! wave file has no data. \n";
		return -1;
	}
	pcmdata = (float *)malloc((samples+samplingRateI) * sizeof(float));
	if (!pcmdata) {
		std::cerr << "Error! out of memory. (pcmdata)\n";
		return -1;
	}
	memset(pcmdata, 0, (samples+samplingRateI) * sizeof(float));

	__int16 *readPcm = (__int16 *)malloc(samples * sizeof(__int16));
	if (!readPcm) {
		std::cerr << "Error! out of memory. (read pcm)\n";
		return -1;
	}
	fs.read((char *)readPcm, samples * sizeof(__int16));

	float *pcmf = &pcmdata[samplingRateI/2];
	for (int i=0; i<samples; i++) {
		*pcmf++ = (float)readPcm[i] / (float)SHRT_MAX;
	}

	free(readPcm);

	durationPCMTime = samples/samplingRateF;
	if (optVerbose) {
		std::cout << "SamplingRate:" << samplingRateI << "\n";
		std::cout << "PCM Samples:" << samples << "\n";
		std::cout << "\t" << durationPCMTime << " sec\n";
	}

	return ERR_OK;
}

#include <Windows.h>
int Convert2ECG::convert(Arguments arg)
{
	int err = 0;
	char pathInput[_MAX_PATH];

//	maketabl();		// Special... make g-table.

	optVerbose = arg.opt_v;			// verbose option.
	optWholedata = arg.opt_w;		// convert whole data.
	optRaw = arg.opt_r;				// convert raw mode.
	optSerialNo = arg.owSerialNo;	// over write Serial No
	optDataOnly = arg.donlyStartTime;	// start time, convert only Data Section. 
	optThroughCalibration = arg.opt_c;
	optDebug = arg.opt_X;

	arg.getWavFilePath(pathInput, _MAX_PATH);
	err = loadSoundData(pathInput);
	if (err) return err;

	err = setupGTable(samplingRateI, arg.currentPath);
	if (err) return err;

	err = pcm2ecg();
	if (err) return err;

	char fpath[MAX_PATH];
	arg.getEcgFilePath(fpath, sizeof(fpath));
	if (optRaw) {
		outECGRaw(fpath);
	}
	else {
		outECG(fpath);
	}

	return ERR_OK;
}

int Convert2ECG::pcm2ecg( void )
{
	int err = ERR_OK;

#if 0
	fconvTest();
	return;
#endif
	double offsetTime = samplingRateF/2.0;

	if (optWholedata)
		err = covertWholeData();
	else
		err = convetECGData();

	return err;
}

void Convert2ECG::outECGRaw(char *fpath)
{
	std::ofstream fs;

	fs.open(fpath, std::ios::out);
	if (fs.fail()) {
		std::cerr << "Error! cannot open output file:" << fpath << "\n";
		return;
	}

	for (int i=0; i<idxECG; i++)
		fs << rawECG[i] << "\n";

	fs.close();
}


void Convert2ECG::outECG(char *fpath)
{
	static const int offsetECGValue = 1700;
	std::ofstream fs;

	fs.open(fpath, std::ios::out);
	if (fs.fail()) {
		std::cerr << "Error! cannot open output file:" << fpath << "\n";
		return;
	}

	fs << "[TRANSMISSION HEADER]\n";

    fs << "Version=3.7.0.4\n";
    fs << "DeviceSoftwareCode=24\n";
    fs << "SampleRate=225\n";
    fs << "DynamicRange=6\n";
    fs << "EventsNumber=1\n";
    fs << "SamplesNumberInEvent=" << idxECG << "\n";
    fs << "PostEventInSec=0\n";
    fs << "LeadsNumber=1\n";
    fs << "[HEADER Event1]\n";
    fs << "EventDate=";

	const int dtimeLength = 64;
	WCHAR  strWch[dtimeLength];
//	CString pdate = procTime.FormatGmt(L"%m/%d/%Y");
	CString pdate = procTime.Format(L"%m/%d/%Y");
	_tcscpy_s( strWch, pdate );
	char   str[dtimeLength];
	setlocale(LC_ALL,"japanese");
	size_t datelen = dtimeLength;
	wcstombs_s(&datelen, str, dtimeLength, strWch, _TRUNCATE);
	fs << str << "\n";
	
	fs << "EventTime=";
//	CString ptime = procTime.FormatGmt(L"%H:%M");
	CString ptime = procTime.Format(L"%H:%M");
	_tcscpy_s( strWch, ptime );
	datelen = dtimeLength;
	wcstombs_s(&datelen, str, dtimeLength, strWch, _TRUNCATE);
	fs << str << "\n";

    fs << "DateTimeOfRecording=No\n";
    fs << "EventAuto=No\n";
    fs << "MonitorSerialNumber=" << serialNo << "\n";
    fs << "[ECG Event1]\n";

	for (int i=0; i<idxECG; i++)
		fs << offsetECGValue -rawECG[i] << "\n";

	fs.close();
}

/*
Status=0
SerialNo=10018
TimeStamp=2015/11/17 15:19:11
*/

void Convert2ECG::outStatus(Arguments arg, int status)
{
	std::ofstream fs;
	char fpath[_MAX_PATH];

	arg.getStatusPath(fpath, sizeof(fpath));

	fs.open(fpath, std::ios::out);
	if (fs.fail()) {
		std::cerr << "Error! cannot create status file:" << fpath << "\n";
		return;
	}

	fs << "Status=" << status << "\n";
    fs << "SerialNo=" << serialNo << "\n";
    fs << "TimeStamp=";


	const int dtimeLength = 64;
	WCHAR  strWch[dtimeLength];
	CString pdate = procTime.Format(L"%Y/%m/%d %H:%M:%S");
	_tcscpy_s( strWch, pdate );
	char   str[dtimeLength];
	setlocale(LC_ALL,"japanese");
	size_t datelen = dtimeLength;
	wcstombs_s(&datelen, str, dtimeLength, strWch, _TRUNCATE);
	fs << str << "\n";
	
	fs.close();
}

int Convert2ECG::covertWholeData(void)
{
	const int startinglength = 10;
	const int terminatelength = 100;

	int pcmOffset = samplingRateI/2;
	double currentTime = 0.0;
	int val;

	int count = 0;
	while (currentTime < durationPCMTime) {
		int pcmidx = pcmOffset+(int)(currentTime*samplingRateF);
		currentTime += 1.0/DataRate;
		val = fast_fcnv(&pcmdata[pcmidx], 1100, 2300, 1);
		rawECG[idxECG++] = val;
		if (idxECG >= MaxECGTable)
			break;
	}

	std::cout << "Data Length : " << idxECG << "\n";
	return ERR_OK;
}

int Convert2ECG::convetECGData(void)
{
	int err = ERR_OK;

	if (optDataOnly == 0.0) {
		err = detectHeader();
		if (err != ERR_OK) {
			std::cerr << "Error! canot detect the header part.\n";
			return err;
		}
		err = analyzeCalibration();
		if (err != ERR_OK && !optThroughCalibration) {
			std::cerr << "Error! canot detect the calibration part.\n";
			return err;
		}
		err = analyzeSerialNo();
		if (err != ERR_OK && optSerialNo == 0) {
			std::cerr << "Error! canot detect the serial_NO part.\n";
			return err;
		}
	}
	if (optSerialNo) {
	    if (optVerbose) {
			std::cout << "\tReplaced Serial No: " << serialNo << " --> " << optSerialNo << "\n";
		}
		serialNo = optSerialNo;
	}
	err = collectData();
	return err;
}

float *Convert2ECG::getCurrentPcmp(void)
{
	if (currentPCMTime >= durationPCMTime)
		return 0;
	return &pcmdata[samplingRateI/2 + (int)(currentPCMTime*samplingRateF)];
}


int Convert2ECG::collectData(void)
{
    // Config.
    const int errorLimit = 80;
    const int kTotalErrLimit = 600;
    const double kDataRate	= (225.0 * 2.0);
    
	int err = ERR_OK;
    int f;
    int errorCounter = 0;
    int totalErrors = 0;
	double dataStartTime = currentPCMTime;
	double duratinTime = 0.0;
    float* pcm;
    
	int anchorIdx = 0;
    currentPCMTime += 1.0/kDataRate / 2.0;
	if (optDataOnly > 0.0)
		currentPCMTime = optDataOnly;

	while ( idxECG < MaxECG &&
           errorCounter < errorLimit && totalErrors < kTotalErrLimit) {
        if ((pcm = getCurrentPcmp()) == 0) break;
		
        f = fast_fcnv(pcm, 1000, 2280, 1);
        
        currentPCMTime += 1.0/kDataRate;  // Data Rate
		duratinTime += 1.0/kDataRate; 
               
        if ( 1180 <= f && f <= 2220) {
            errorCounter = 0;
			anchorIdx = idxECG;
        }
        else {
            errorCounter++;
            if ( f < 1000 ) f = 1000;
            if ( f > 2300 ) f = 2300;
        }

        rawECG[idxECG++] = f;
    }
    
	idxECG = anchorIdx;
    if (optVerbose) {
		std::cout << "\n- - - - - - - - - - - -\n";
		std::cout << "Data Part.\n";
		std::cout << "\tstartTime : " << dataStartTime << " sec\n";
		std::cout << "\tduration  : " << duratinTime << " sec\n";
		std::cout << "\terrors    : " << errorCounter << "\n";
		std::cout << "\tsamples   : " << idxECG << "\n";
	}  
    return err;
}

#define printf(...)			// debug...

/*
 Detect header part.
 */
int Convert2ECG::detectHeader(void)
{
    // Config.
    const int bitUTimeSearch = 2;					// mSec (2)
    const int bitUTimeSweep = 4;					// mSec (8)
    const float kDurationTimeSweep = 510.0f;		// duretion time of sweep.
    enum {
        DetectingHeader,
        RecognizingHeader,
    };
    const int errorLimit = 30;						//
    const int derogationLimit = 100/bitUTimeSweep;	//
    
    int f;
    int duratinTime = 0;
	int errDurationCount = 0;
    int lastF = 0;
    int errorCounter = 0;
    int derogation = 0;
    double detectFreq = 0.0;
    double sweepBaseTime = 0.0;
	double sweepStartTime = 0.0;
    int phase = DetectingHeader;
    int adjustOffset = 0;
    float* pcm;

	currentPCMTime = 0.0;
    BOOL done = FALSE;
    while (!done) {
        if ((pcm = getCurrentPcmp()) == 0) {
			std::cerr << "Error! cannot detect header part. proc time:" << currentPCMTime << "\n";
			return ERR_STOP_EMPTY;		// no data.
		}

		switch (phase) {
            case DetectingHeader:
                
            {
                f = fvconvert(pcm, 1180, 1320, 10 );
                printf(" HEADER t:%.4f f:%d\n", currentPCMTime, f);
                
                if (f < 1195) {
                    printf(" Searching......  t:%.4f f:%d\n", currentPCMTime, f);
                    currentPCMTime += k1mSecond;
                    break;
                }
                
                if (f > 1205 && f < 1280) {
                    printf(" !!!! Header Sweep area Start!!!! t:%.4f f:%d\n", currentPCMTime, f);
                    phase = RecognizingHeader;
                    duratinTime = 0;
					errDurationCount = 0;
                    errorCounter = 0;
                    derogation = 0;
                    lastF = f;
                    detectFreq = (float)f;
                    sweepBaseTime = currentPCMTime;
					sweepStartTime = currentPCMTime;
                }
                
                currentPCMTime += bitUTimeSearch * k1mSecond;
                lastF = f;
                break;
            }
                
            case RecognizingHeader:     // Sweep up 部チェック
            {
                f = fast_fcnv(pcm, 1000, 2280, 2 );
                
                if (f <= 1200 || 2220 <= f) {
	                if (errDurationCount > 10) {
		                printf(" Header! out of range f:%d t:%.4f\n", f, currentPCMTime);
                    
			            phase = DetectingHeader;	// Leed部再検出
				        break;
					}
					else {
						errDurationCount++;
					}
                }
				else {
					errDurationCount = 0;
				}
                
                duratinTime += bitUTimeSweep;
                if (detectFreq-20*bitUTimeSweep < f && f < detectFreq+20*bitUTimeSweep) {
                    if (f > detectFreq+10) {
                        printf(" BASE ---------------\n");
                        sweepBaseTime -= k1mSecond;
                        --adjustOffset;
                    }
                    if (f < detectFreq-10) {
                        printf(" BASE +++++++++++++++\n");
                        sweepBaseTime += k1mSecond;
                        ++adjustOffset;
                    }
                    if (derogation > 0)
                        derogation--;
                    printf(" Sweep!! df:%d f:%d t:%.4f   (diff:%d)\n", (int)detectFreq, f, currentPCMTime, f - (int)detectFreq );
                }
                else {
                    if (1190 < f && f < 2210) {
                        derogation++;
                    }
                    errorCounter++;
                    printf(" Sweep ERROR ! df:%d f:%d t:%.4f   (%d) duration:%d err:%d\n",
                           (int)detectFreq, f, currentPCMTime, f - (int)detectFreq, duratinTime, errorCounter);
                    if (f <= detectFreq-20 && duratinTime > 500 && detectFreq >= 2190) {
                        done = TRUE;
                        break;
                    }
                }
                // calcurate detecting frequency.
                detectFreq = (currentPCMTime-sweepBaseTime)*1000.0*1000.0/kDurationTimeSweep + 1200.0;
                if (detectFreq > 2190.0)
                    detectFreq = 2190.0;
                
                if (errorCounter > errorLimit || derogation > derogationLimit || duratinTime > 700) {
                    printf("OVER ERROR Retry Header lead... f:%d t:%.4f err:%d dero:%d\n", f, currentPCMTime, errorCounter, derogation);
                    phase = DetectingHeader;	// Leed部再検出
					currentPCMTime = sweepStartTime;		// 2015/12/22
                }
                
                currentPCMTime += bitUTimeSweep * k1mSecond;
                lastF = f;
                
                break;
            }
            default:
                break;
        }
    }
    
	if (optVerbose) {
		std::cout << "\n- - - - - - - - - - - -\n";
		std::cout << "Header Part.\n";
		std::cout << "\tstartTime : " << sweepStartTime << " sec\n";
		std::cout << "\tduration  : " << duratinTime << " msec\n";
		std::cout << "\terrors    : " << errorCounter << "\n";
		std::cout << "\tderogation: " << derogation << "\n";
	}

    return ERR_OK;
}

/*
 Analyze Calibration part.
 */
int Convert2ECG::analyzeCalibration(void)
{
    // Config.
    int bitUTime = 1;						// mSec
    const int clearance = 25;				// Hz
    const int limitTime = 40*3*(18+1);
    
	int err = ERR_OK;
    int f;
    int errorCounter = 0;
    int duratinTime = 0;
    int groups = 0;
    int totalDuratinTime = 0;
    
    int count800Hz = 0;
    int count700Hz = 0;
    int count600Hz = 0;
    int lastHML = 0;
    int PLLCount = 0;
    int PLLTime = 0;
    int adjustCount = 0;
    int detectHML = 0;
    int total800Hz = 0;
    int total700Hz = 0;
    int total600Hz = 0;
	double calibrationStartTime = currentPCMTime;

    float* pcm;
    
    int done = 0;
    while ( !done && totalDuratinTime < limitTime ) {
        if ((pcm = getCurrentPcmp()) == 0) return -1;
        
        // sigma value, errRate 1.0:32% 1.5:12% 2.0:4%
        f = fast_fcnv(pcm, 1500, 1900, 5);
        
        if (1800-clearance <= f && f <= 1800+clearance) {
            count800Hz += bitUTime;
            detectHML = 800;
        }
        else if (1700-clearance <= f && f <= 1700+clearance) {
            count700Hz += bitUTime;
            detectHML = 700;
        }
        else if (1600-clearance <= f && f <= 1600+clearance) {
            count600Hz += bitUTime;
            detectHML = 600;
        }
        else {
            errorCounter++;
        }
        
        // PLL detecting...
        int count = count800Hz + count700Hz + count600Hz;
        if (detectHML != lastHML && count > bitUTime && count < (40-bitUTime)) {
            PLLCount++;
            PLLTime = duratinTime;
            printf(" PLL-%d f:%d t:%.4f count:%d time:%d\n", detectHML, f, currentPCMTime, PLLCount, PLLTime);
        }
        lastHML = detectHML;
        
        duratinTime += bitUTime;
        
        if (duratinTime >= 40) {
            adjustCount++;
            if (PLLCount == 1) {
                printf(" PLL Adjust! %d \n", PLLTime);
                adjustCount = 0;
                if (PLLTime < 40/2) {  // +Adjust
                    printf(" Adjustted! %f + %f \n", currentPCMTime, k1mSecond * PLLTime/2);
                    currentPCMTime += k1mSecond * PLLTime/2;
                }
                else {						// -Adjust
                    printf(" Adjustted! %f - %f \n", currentPCMTime, k1mSecond * ( 40 - PLLTime)/2);
                    currentPCMTime -= k1mSecond * ( 40 - PLLTime)/2;
                }
            }
            PLLCount = 0;
            detectHML = 0;
            
            if (adjustCount > 3) {
                printf(" PLL Locked...................! count:%d UT:%d\n", adjustCount, bitUTime);
                //              bitUTime = 8;       // speedup.(8)
                bitUTime = 2;
            }
            printf(" H:%d M:%d L:%d Grp:%d err:%d\n", count800Hz, count700Hz, count600Hz, groups, errorCounter);
			int threshold = 20 - (20*errorCounter)/40;
            printf(" Total Count H:%d M:%d L:%d thresh:%d\n\n", total800Hz, total700Hz, total600Hz, threshold);
            if (count800Hz > threshold) {
                total800Hz++;
            }
            else if (count700Hz > threshold) {
                total700Hz++;
            }
            else if (count600Hz > threshold) {
                total600Hz++;
                
                if (total800Hz == 1 && total700Hz == 1 && total600Hz == 1)
                    groups++;
                if (groups == 18) {
                    printf("Calib Last Done! (%f)\n", currentPCMTime);
                    done = 1;
                }
                total800Hz = total700Hz = total600Hz = 0;
            }
            else {
                printf(" Calibration ERR! H:%d M:%d L:%d err:%d\n", count800Hz, count700Hz, count600Hz, errorCounter);
                // キャリブレーションエラー H M L のウィンドから外れた
//                err = -1;
//                break;
            }
            
            totalDuratinTime += duratinTime;
            
            count800Hz = count700Hz = count600Hz =0;
            duratinTime = 0;
			errorCounter = 0;
        }
        
        currentPCMTime += k1mSecond * bitUTime;		// + 1msec
    }
    
    if(totalDuratinTime > limitTime || groups != 18) {
		std::cerr <<" Calibration ERR! [detected Groups:" << groups << "]\n";
        printf(" Calibration ERR! Total Duration:%d err:%d Groups:%d \n", totalDuratinTime, errorCounter, groups);
        // キャリブレーションエラー 時間内に必要なグループ数が見つからない
        err = -1;
    }
          
	if (optVerbose) {
		std::cout << "\n- - - - - - - - - - - -\n";
		std::cout << "Calibration Part.\n";
		std::cout << "\tstartTime : " << calibrationStartTime << " sec\n";
		std::cout << "\tduration  : " << totalDuratinTime << " msec\n";
		std::cout << "\terrors    : " << errorCounter << "\n";
	} 
    return err;
}

/*
 Analyze Serial No part.
 */
int Convert2ECG::analyzeSerialNo(void)
{
    // Config.
    int bitUTime = 1;					// mSec
    //  const int bitWidth = (80/bitUTime);		// times
    const int SNclearance = 66;				// Hz
    const int errorLimit = 80*40/80;	// over
    
	int err = ERR_OK;
    int f;
    int errorCounter = 0;
    int duratinTime = 0;
    int totalDuratinTime = 0;
    unsigned  __int8 serialData[5] = {0, 0, 0, 0, 0};
    int bitNo = 0;
    int countL = 0;
    int countH = 0;
    int lastHL = -1;
    int PLLCount = 0;
    int PLLTime = 0;
    int adjustCount = 0;
    int detectHL = 0;
	double selialNoStartTime = currentPCMTime;
    float* pcm;
    
    int done = 0;
    while ( !done && errorCounter < errorLimit) {
        if ((pcm = getCurrentPcmp()) == 0) return -1;
        
        // sigma は1.0以上！
        f = fvconvert(pcm, 1200, 2200, 20);
        
        if (1366-SNclearance <= f && f <=1366+SNclearance) {
            detectHL = 0;
            countL++;
        }
        else if (2035-SNclearance <= f && f <=2035+SNclearance) {
            detectHL = 1;
            countH++;
        }
        else {
            errorCounter++;
        }
        
        // PLL...
        if (detectHL != lastHL && (countL+countH) > 1 && (countL+countH) < (80-bitUTime)) {
            PLLCount++;
            PLLTime = duratinTime;
            printf(" PLL-%d f:%d t:%.4f count:%d time:%d\n", detectHL, f, currentPCMTime, PLLCount, PLLTime);
        }
        lastHL = detectHL;
        
        duratinTime += bitUTime;
        
        if (duratinTime >= 80) {
            adjustCount++;
            if (PLLCount == 1) {
                printf(" PLL Adjust! %d \n", PLLTime);
                if (PLLTime < 80/2) {  // +Adjust
                    printf(" Adustted! %f + %f \n", currentPCMTime, k1mSecond * PLLTime/2);
                    currentPCMTime += k1mSecond * PLLTime/2;
                }
                else {							// -Adjust
                    printf(" Adustted! %f - %f \n", currentPCMTime, k1mSecond * ( 80 - PLLTime)/2);
                    currentPCMTime -= k1mSecond * ( 80 - PLLTime)/2;
                }
            }
            PLLCount = 0;
            lastHL = 0;
            
            if (adjustCount > 2) {
                printf(" PLL Locked...................! count:%d UT:%d\n", adjustCount, bitUTime);
                //              bitUTime = 16;          // speed up. (16)
                bitUTime = 4;           // speed up. (16)
                
            }
            printf(" Bitno:%d  L:%d H:%d err:%d\n", bitNo, countL, countH, errorCounter);
            if (countH > countL) {
                serialData[bitNo/8] |= 1 << (bitNo % 8);
                lastHL = 1;
            }
            
            totalDuratinTime += duratinTime;
            countL = countH = 0;
            duratinTime = 0;
            
            bitNo++;
            if (bitNo >= 40) {
                done = 1;
            }
        }
        currentPCMTime += k1mSecond * bitUTime;		// + 1msec*unit
    }
    
    serialNo  = serialData[2]<<16 | serialData[1]<<8 | serialData[0];
    serialSum = serialData[0] + serialData[1] + serialData[2];
    checkSum  = serialData[4]<<8 | serialData[3];
    
    printf("\n- - - - - - - - - - - -\n");
    printf("SerialNo DONE. duration:%d  errs:%d\n", totalDuratinTime, errorCounter);
    printf("    SN:%d(%X) CHKSUM:%d(%X) is %s\n",serialNo, serialNo,checkSum, checkSum,
           (serialSum == checkSum)? " OK!" : "NG.");
    
    
	          
	if (optVerbose) {
		std::cout << "\n- - - - - - - - - - - -\n";
		std::cout << "SerialNo Part.\n";
		std::cout << "\tstartTime : " << selialNoStartTime << " sec\n";
		std::cout << "\tduration  : " << totalDuratinTime << " msec\n";
		std::cout << "\terrors    : " << errorCounter << "\n";
		std::cout << "\tserialNo  : " << serialNo << "\n";
		std::cout << "\tcheckSum  : " << checkSum << ((serialSum == checkSum)? "  (OK!)" : "  (NG.)") << "\n";
	} 
    
    if (serialSum != checkSum)
        err = ERR_INVALID_CHKSUM;

	return err;
}

//****************************** Frequency Convert Tes ***********************//

#define _USE_MATH_DEFINES
#include <math.h>

#include <atltime.h>
float pcm0[SamplingRate441*2];
void Convert2ECG::fconvTest( void ){
	float f = 1100.0;
	CFileTime cTimeStart, cTimeEnd;
	CFileTimeSpan cTimeSpan;

	cTimeStart = CFileTime::GetCurrentTime();
	for (int i=0; i<1200; i++) {
		for (int j=0; j<SamplingRate441*2; j++)
			pcm0[j] = sin(2.0F*(float)M_PI*f*j/SamplingRate441);
//		int cf = fvconvert(&pcm0[SamplingRate441*1], 1100, 2200, 1, 0.999F);
		int cf = fast_fcnv(&pcm0[SamplingRate441*1], 1100, 2200, 1);
		std::cout << f << " : " << cf << "\n";
		f += 1.0;
	}
	cTimeEnd = CFileTime::GetCurrentTime();
	cTimeSpan = cTimeEnd - cTimeStart;
	std::cout<< "処理時間:" << cTimeSpan.GetTimeSpan()/10000 << "[ms]" << std::endl;
}


//****************************** Wave Transrom ***********************//

int Convert2ECG::fast_fcnv(float pcm[],							// 解析データ
              int minF, int maxF, int pitch)		// 解析周波数、下限、上限、間隔
{
    int pitchdiv = (maxF - minF)/16;
    int f;
    
    while (pitchdiv > pitch * 4) {
        //		printf(" %d - %d : %d (%d)\n", maxF, minF, pitchdiv, pitchx);
        f = fvconvert(pcm, minF, maxF, pitchdiv);
        if (f < 0) {
            return f;
        }
        minF = f - pitchdiv;
        maxF = f + pitchdiv;
        pitchdiv /= 4;
    }
    
    maxF += pitchdiv*3;
    f = fvconvert(pcm, minF, maxF, pitch);
    return f;
}


int Convert2ECG::fvconvert(float pcm[],             // 調べたいPCMの中央データポジション
              int minF, int maxF, int pitch)		// 解析周波数、下限、上限、間隔
{
	const float peek_shift = 0.999F;
    float wt[2048];
    float peek;
    int i, peek_idx;
    
    int wt_len = (maxF-minF)/pitch;
	gabor_transform(pcm, minF, pitch, wt, wt_len);
    
    peek = thresholdLevel;
    peek_idx = -1;
    for (i=0; i<wt_len; i++) {
        if (wt[i] > peek) {
            peek = wt[i];
            peek_idx = i;
        }
    }

    if (peek_idx == -1)
        return -1;

	if (peek_shift > 0) {
		peek *= peek_shift;
		for (i=peek_idx; i<wt_len; i++) {
			if (wt[i] > peek)
			peek_idx = i;
		}
    }
    return minF + peek_idx * pitch;
}

void Convert2ECG::gabor_transform(float pcm[], int baseF, int stepF, float wt[], int wt_len)
{
    int y, m;
    
    for (y = 0; y < wt_len; y++)
    {
        int freq = baseF + stepF*y;
        if (freq < tbl_minf || freq >= tbl_maxf) {
            wt[y] = 0.0;				// Out of Range.
            continue;
        }
        
        int dx = gtbl->dxlen[freq - tbl_minf];
        float real_wt = 0;
        float imag_wt = 0;
        
        float* gf = &gtbl->tbl[gtbl->offset[freq - tbl_minf]];
        float* pcmp = &pcm[-dx];
        
        for (m = -dx; m <= dx; m++)
        {
            float pcmd = *pcmp++;
            real_wt += pcmd * *gf++;
            imag_wt += pcmd * *gf++;
        }
        wt[y] = (float)(freq)*sqrtf(1.0F/(float)(freq)) * sqrtf(real_wt*real_wt + imag_wt*imag_wt);
    }
}


void gabor_transform_calc(float pcm[],
					 int baseF, int stepF, 
					 float wt[], int wt_len)
{
	const int samplingrate = SamplingRate441;
	const float sigma = 2.0;
    int y, m;
    
    for (y = 0; y < wt_len; y++)
    {
        float	a = 1.0F/(float)(baseF + stepF*y);			// 調べる周波数の逆数
        float	dt = a*sigma*sqrtf(-2.0F*logf(0.01F));		// 窓幅(秒)
        int		dx = (int)(dt * samplingrate);				// 窓幅(サンプル数)
        
        // 窓幅の範囲を積分
        float real_wt = 0;
        float imag_wt = 0;
        
        for (m = -dx; m <= dx; m++)
        {
			float t = (float)m/samplingrate/a;
			float gauss = 1.0F/sqrtf(2.0F*(float)M_PI*sigma*sigma) * expf(-t*t/(2.0F*sigma*sigma)); // ガウス関数
			float omega_t = 2.0F*(float)M_PI*t;		// ωt
			real_wt += (float)(pcm[m]) * gauss * cosf(omega_t);
			imag_wt += (float)(pcm[m]) * gauss * sinf(omega_t);
        }
        wt[y] = 1.0F/sqrtf(a) * sqrtf(real_wt*real_wt + imag_wt*imag_wt);
    }
}

#if 0	// Make G-Table.
struct mkGaborFactorTbl {
    int dxlen[tbl_size];
    int offset[tbl_size];
    float tbl[512*2*tbl_size];
} mkGaborFactorTbl;

void Convert2ECG::maketabl(void)
{
    int i, m;
    float sigma = 2.0;
	const int kSamplingFrequency = 48000;
    int pcmoffset = 0;

    for (i=0; i<tbl_size; i++) {
        float a = 1.0F/(float)(tbl_minf + i);		// 調べる周波数の逆数
        float dt = a*sigma*sqrtf(-2.0F*logf(0.01F));		// 窓幅(秒)
        int    dx = (int)(dt * kSamplingFrequency);		// 窓幅(サンプル数)
        
        mkGaborFactorTbl.dxlen[i] = dx;
        mkGaborFactorTbl.offset[i] = pcmoffset;
        
        //		printf("Freq:%d dx:%d Off:%d\n", tbl_minf+i, gaborFactorTbl.dxlen[i], gaborFactorTbl.offset[i]);
        
        for (m = -dx; m <= dx; m++)
        {
            float t = (float)m/kSamplingFrequency/a;
            float gauss = 1.0F/sqrtf(2.0F*(float)M_PI*sigma*sigma) * expf(-t*t/(2.0F*sigma*sigma)); // ガウス関数
            float omega_t = 2.0F*(float)M_PI*t;		// ωt
            mkGaborFactorTbl.tbl[pcmoffset++] = gauss * cosf(omega_t);
            mkGaborFactorTbl.tbl[pcmoffset++] = gauss * sinf(omega_t);
        }
    }
    
    printf("Total factors:%d\n", pcmoffset);

	std::ofstream fs;
	const char *tblFname = "GFactorTable480.dat";

	fs.open(tblFname, std::ios::out | std::ios::binary);
	if (fs.fail()) {
		std::cerr << "Error! cannot open GDataFile:" << tblFname << "\n";
		return;
	}

	fs.write((char*)&mkGaborFactorTbl, sizeof(int)*tbl_size*2 + sizeof(float)*pcmoffset);
}
#endif