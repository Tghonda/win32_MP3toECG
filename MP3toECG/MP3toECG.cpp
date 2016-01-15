// Win32_Mp3toECG.cpp : コンソール アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"

#include "Arguments.h"
#include "Convert2ECG.h"
#include "ErrorStatusNo.h"

namespace ECGConverter {
	void usage( _TCHAR* exepath )
	{
		_tprintf(_T("usage  MP3toECG.exe (Rev. 0.91)\n\n"));
		_tprintf(_T("%s inputFile [options]\n"), exepath);
		_tprintf(_T("\toptions:\n"));
		_tprintf(_T("\t-v (verbose mode on)\n"));
		_tprintf(_T("\t-c ignore calibration ERROR\n"));
		_tprintf(_T("\t-s serialNo (over write serial No)\n"));
		_tprintf(_T("\t-d startTime (convert only data section)\n"));
	}
}

int _tmain(int argc, _TCHAR* argv[])
{
	_tsetlocale(LC_ALL, _T(""));

	Arguments argument;
	if ( argument.parseArgs(argc, argv)) {
		ECGConverter::usage(argv[0]);
		return -1;
	}

	int status = argument.convertToWave();
	if (status != ERR_OK) {
		std::cerr << "Error Internal cannot convert MP3 to WAV\n";
		return -1;
	}

	Convert2ECG converter;
	status = converter.convert(argument);
	converter.outStatus(argument, status);

	argument.delteWaveFile();

	if (argument.opt_v) {
		std::cerr <<  "\n\tlast status:" << status << "\n";
	}

	return status;
}