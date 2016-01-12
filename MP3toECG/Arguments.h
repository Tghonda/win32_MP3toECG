#pragma once
#include <stdlib.h>
#include <string>

#define EXT_WAVFILE ".wav"
#define EXT_ECGFILE ".ecg"
#define EXT_STATUSFILE ".rst"

const std::string ConfigFilePath = "MP3toECG.cfg";

class Arguments
{
private:
	std::string	pathECGBase;
	std::string	outFolderName;
	std::string inputMp3Fname;
	std::string inputWavFname;
	std::string outputFname;
	std::string statusFname;

	char ecgFPath[_MAX_PATH];			// default folder path.
	char ecgOutFolder[_MAX_PATH];		// out folder name.
	char pathInput[_MAX_PATH];
	char pathOutput[_MAX_PATH];

public:
	bool opt_c;							// through Calibration
	bool opt_v;							// verbose mode.
	bool opt_w;							// convert Whole data.
	bool opt_r;							// convert to Raw data.
	bool opt_X;							// debug..
	int		owSerialNo;
	double	donlyStartTime;
	std::string currentPath;

	Arguments(void);

	int toStdString(_TCHAR* tchar, char* cchar, int clen);
	int convertToWave(void);
	int delteWaveFile(void);
	int parseArgs(int argc, _TCHAR* argv[]);
	int parseConfigf(void);
	int getMp3FilePath(char *, size_t len);
	int getWavFilePath(char *, size_t len);
	int getEcgFilePath(char *, size_t len);
	int getStatusPath(char *, size_t len);
};