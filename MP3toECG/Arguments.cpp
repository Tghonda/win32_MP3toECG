#include "stdafx.h"
#include "Arguments.h"

#include <fstream>
#include <iostream>
#include <Windows.h>

Arguments::Arguments(void)
{
	opt_c = false;
	opt_v = false;
	opt_w = false;
	opt_r = false;
	opt_X = false;
	owSerialNo = 0;
	donlyStartTime = 0.0;
	memset(pathInput, 0, sizeof(pathInput));
	memset(pathOutput, 0, sizeof(pathOutput));
}


/// TCHAR -> std::string ‚Ì•ÏŠ·

int Arguments::toStdString(_TCHAR* tchar, char* cchar, int clen)
{
#ifdef UNICODE
	{
		if(!cchar)
			return 0;

		//char‚É•K—v‚È•¶Žš”‚ÌŽæ“¾
		int nLen = ::WideCharToMultiByte(CP_THREAD_ACP,0,tchar,-1,NULL,0,NULL,NULL);
		if (nLen >= clen) {
			*cchar = 0;
			return 0;
		}
		//•ÏŠ·
		nLen = ::WideCharToMultiByte(CP_THREAD_ACP,0,tchar,(int)::wcslen(tchar)+1,cchar,nLen,NULL,NULL);
		return nLen;
	}
#else
	{
		size_t	nLen = ::_tcslen(tchar) + 1;
		if(cchar)
			strcpy_s(cchar, nLen * sizeof(char), tchar);
	}
#endif
	return 0;
}

// String - std::string version
int Arguments::parseArgs(int argc, _TCHAR* argv[])
{
	errno_t err = 0;
	char cstr[_MAX_PATH];

	toStdString(argv[0], cstr, sizeof(cstr));
	std::string exepath = std::string(cstr);
	int pidx = exepath.find_last_of('\\');
	currentPath = std::string(exepath, 0, pidx+1);

	err = parseConfigf();

	for (int idx=1; idx<argc; idx++) {
		toStdString(argv[idx], cstr, sizeof(cstr));
		if (cstr[0] == '-') {
			switch (cstr[1]) {
			case 'c':
				opt_c = true;			// through Calibration
				break;
			case 'v':
				opt_v = true;			// verbose mode.
				break;
			case 'w':
				opt_w = true;			// convert Whole data.
				break;
			case 'r':
				opt_r = true;			// convert to Raw data.
				break;
			case 'X':
				opt_X = true;			// debug.
				break;
			case 'o':
				idx++;
				if (idx >= argc)
					return -1;
				toStdString(argv[idx], cstr, sizeof(cstr));
				outputFname = std::string(cstr);
				break;
			case 's':					// over write Serial NO.
				idx++;
				if (idx >= argc)
					return -1;
				owSerialNo = _tstoi(argv[idx]);
				break;
			case 'd':					// onvert only Data section. (sec)
				idx++;
				if (idx >= argc)
					return -1;
				donlyStartTime = _tstof(argv[idx]);
				break;
			}
		}
		else {
			if (inputMp3Fname.length() > 0)
				return -1;					// dupricated input file.
			toStdString(argv[idx], cstr, sizeof(cstr));
			inputMp3Fname = std::string(cstr);
		}
	}

	// check input MP3 file.
	if (inputMp3Fname.empty()) {
		std::cerr << "Error! Not found input file(mp3).\n";
		return -1;					// not found input file.
	}
	
	// set intpu WAV file.
	if (inputWavFname.empty()) {
		inputWavFname = inputMp3Fname.substr(0);
		int pposi = inputWavFname.find_last_of('.');
		if (pposi > 0)
			inputWavFname.replace(pposi, sizeof(EXT_WAVFILE), EXT_WAVFILE);
		else
			inputWavFname.append(EXT_WAVFILE);
	}

	// set output file.
	if (outputFname.empty()) {
		outputFname = inputMp3Fname.substr(0);
		int pposi = outputFname.find_last_of('.');
		if (pposi > 0)
			outputFname.replace(pposi, sizeof(EXT_ECGFILE), EXT_ECGFILE);
		else
			outputFname.append(EXT_ECGFILE);
	}

	// set status file.
	statusFname = outputFname.substr(0);
	int pposi_stat = outputFname.find_last_of('.');
	if (pposi_stat > 0)
		statusFname.replace(pposi_stat, sizeof(EXT_STATUSFILE), EXT_STATUSFILE);
	else
		statusFname.append(EXT_STATUSFILE);

	if ( opt_v ) {
		std::cout << "Convert Mp3 --> ECG. arguments...\n";
		std::cout << "\tEXE Path:    " << currentPath<< "\n";
		std::cout << "\tECG Path:    " << pathECGBase << "\n";
		std::cout << "\tECG Folder:  " << outFolderName << "\n";
		std::cout << "\tInput  Mp3 File: " << inputMp3Fname << "\n";
		std::cout << "\tWork   Wav File: " << inputWavFname << "\n";
		std::cout << "\tOutput Ecg File: " << outputFname << "\n";
		std::cout << "\tOut Status File: " << statusFname << "\n";	}

	return 0;
}

int Arguments::parseConfigf(void)
{
//	const char *configPath = "MP3toECG.cfg";
	std::string configPath = std::string(currentPath);
	std::ifstream cfst;
	std::string   str;

	configPath.append(ConfigFilePath);
	cfst.open(configPath.c_str());
	if (cfst.fail()) {
		std::cerr << "Error! cannot open config file [MP3toECG.cfg]\n";
		std::cerr << "  search path: " << ConfigFilePath << "\n";
		TCHAR  cdir[512];
		GetCurrentDirectory(512,cdir);
		std::cerr << cdir << "\n";
		return cfst.fail();
	}
	std::cout << "using Config File... " << configPath << "\n";

	while (!cfst.fail()) {
		cfst >> str;
		if (str.compare("ECGPATH") == 0) {		// Ex. D:\ECG_Data
			cfst >> pathECGBase;
			if (pathECGBase.at(pathECGBase.length()-1) != '\\')
				pathECGBase.append( "\\" );
		}
		if (str.compare("OUTFOLDER") == 0) {
			cfst >> outFolderName;
			if (outFolderName.at(outFolderName.length()-1) != '\\')
				outFolderName.append( "\\" );		}
	}

	cfst.close();

	return 0;
}

int Arguments::getMp3FilePath(char* fpath, size_t len)
{
	std::string path(pathECGBase.c_str(), pathECGBase.length());
	path.append(inputMp3Fname);
	strcpy_s(fpath, len, path.c_str());
	return 0;
}

int Arguments::getWavFilePath(char* fpath, size_t len)
{
	std::string path(pathECGBase.c_str(), pathECGBase.length());
	path.append(inputWavFname);
	strcpy_s(fpath, len, path.c_str());
	return 0;
}

int Arguments::getEcgFilePath(char *fpath, size_t len)
{
	std::string path(pathECGBase.c_str(), pathECGBase.length());
	path.append(outFolderName);
	path.append(outputFname);
	strcpy_s(fpath, len, path.c_str());
	return 0;
}

int Arguments::getStatusPath(char *fpath, size_t len)
{
	std::string path(pathECGBase.c_str(), pathECGBase.length());
	path.append(outFolderName);
	path.append(statusFname);
	strcpy_s(fpath, len, path.c_str());
	return 0;
}

int Arguments::convertToWave(void)
{
	static std::string CmdName_ffmpeg = "ffmpeg";
	char path[_MAX_PATH];


	std::string cmd_ffmpeg = std::string(currentPath);
	cmd_ffmpeg.append(CmdName_ffmpeg);
	cmd_ffmpeg.append(" -i ");
	getMp3FilePath(path, sizeof(path));
	cmd_ffmpeg.append(path);
	cmd_ffmpeg.append(" -ar 48000 -y ");
	getWavFilePath(path, sizeof(path));
	cmd_ffmpeg.append(path);

//	std::cout << cmd_ffmpeg << "\n";
	int status = system(cmd_ffmpeg.c_str());

	return status;
}

int Arguments::delteWaveFile(void)
{
	char path[_MAX_PATH];
	getWavFilePath(path, sizeof(path));
	remove(path);

	return 0;
}
