// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <easyhook.h>
#include <string>
#include <iostream>
#include <vector>
#include <Windows.h>
#include <time.h>

struct v_elem {
	HWAVEOUT hwo;
	FILE *file;
};

typedef struct WaveHeader {
	// Riff Wave Header
	char chunkId[4];
	int  chunkSize;
	char format[4];

	// Format Subchunk
	char subChunk1Id[4];
	int  subChunk1Size;
	short int audioFormat;
	short int numChannels;
	int sampleRate;
	int byteRate;
	short int blockAlign;
	short int bitsPerSample;
	//short int extraParamSize;

	// Data Subchunk
	char subChunk2Id[4];
	int  subChunk2Size;

} WaveHeader;

WaveHeader makeWaveHeader(int const sampleRate,
	short int const numChannels,
	short int const bitsPerSample) {
	WaveHeader myHeader;

	// RIFF WAVE Header
	myHeader.chunkId[0] = 'R';
	myHeader.chunkId[1] = 'I';
	myHeader.chunkId[2] = 'F';
	myHeader.chunkId[3] = 'F';
	myHeader.format[0] = 'W';
	myHeader.format[1] = 'A';
	myHeader.format[2] = 'V';
	myHeader.format[3] = 'E';

	// Format subchunk
	myHeader.subChunk1Id[0] = 'f';
	myHeader.subChunk1Id[1] = 'm';
	myHeader.subChunk1Id[2] = 't';
	myHeader.subChunk1Id[3] = ' ';
	myHeader.audioFormat = 1; // FOR PCM
	myHeader.numChannels = numChannels; // 1 for MONO, 2 for stereo
	myHeader.sampleRate = sampleRate; // ie 44100 hertz, cd quality audio
	myHeader.bitsPerSample = bitsPerSample; // 
	myHeader.byteRate = myHeader.sampleRate * myHeader.numChannels * myHeader.bitsPerSample / 8;
	myHeader.blockAlign = myHeader.numChannels * myHeader.bitsPerSample / 8;

	// Data subchunk
	myHeader.subChunk2Id[0] = 'd';
	myHeader.subChunk2Id[1] = 'a';
	myHeader.subChunk2Id[2] = 't';
	myHeader.subChunk2Id[3] = 'a';

	// All sizes for later:
	// chuckSize = 4 + (8 + subChunk1Size) + (8 + subChubk2Size)
	// subChunk1Size is constanst, i'm using 16 and staying with PCM
	// subChunk2Size = nSamples * nChannels * bitsPerSample/8
	// Whenever a sample is added:
	//    chunkSize += (nChannels * bitsPerSample/8)
	//    subChunk2Size += (nChannels * bitsPerSample/8)
	myHeader.chunkSize = 4 + 8 + 16 + 8 + 0;
	myHeader.subChunk1Size = 16;
	myHeader.subChunk2Size = 0;

	return myHeader;
}

char *wavdir;
std::vector<v_elem> vlphwo;
MMRESULT WINAPI myWaveOutOpenHook(LPHWAVEOUT phwo, UINT_PTR uDeviceID, LPWAVEFORMATEX pwfx,	DWORD_PTR dwCallback, DWORD_PTR dwCallbackInstance, DWORD fdwOpen)
{
	v_elem vel;
	char tms[15];
	time_t rawtime;
	struct tm tm1;
	WaveHeader wh;
	time(&rawtime);
	localtime_s(&tm1, &rawtime);
	std::string fname = wavdir;
	if (fname[fname.length() - 1] != '\\') {
		fname += '\\';
	}
	strftime(tms, 15, "%Y%m%d%H%M%S", &tm1);
	fname += tms;
	fname += ".wav";
	wh = makeWaveHeader(pwfx->nSamplesPerSec, pwfx->nChannels, pwfx->wBitsPerSample);
	MMRESULT ret = waveOutOpen(phwo, uDeviceID, pwfx, dwCallback, dwCallbackInstance, fdwOpen);
	vel.hwo = *phwo;
	fopen_s(&vel.file, fname.c_str(), "wb");
	fwrite(&wh, sizeof wh, 1, vel.file);
	vlphwo.push_back(vel);
	return ret;
}
MMRESULT WINAPI myWaveOutWriteHook(HWAVEOUT hwo, LPWAVEHDR pwh, UINT cbwh)
{
	for (size_t i = 0; i < vlphwo.size(); i++) {
		if (vlphwo[i].hwo == hwo) {
			fwrite(pwh->lpData, 1, pwh->dwBufferLength, vlphwo[i].file);
			break;
		}
	}
	return waveOutWrite(hwo, pwh, cbwh);
}
MMRESULT WINAPI myWaveOutCloseHook(HWAVEOUT hwo)
{
	for (size_t i = 0; i < vlphwo.size(); i++) {
		if (vlphwo[i].hwo == hwo) {
			long pos = ftell(vlphwo[i].file) - 8;
			fseek(vlphwo[i].file, 4, SEEK_SET);
			fwrite(&pos, 4, 1, vlphwo[i].file);
			fseek(vlphwo[i].file, 40, SEEK_SET);
			pos -= 4 + 8 + 16 + 8;
			fwrite(&pos, 4, 1, vlphwo[i].file);
			fclose(vlphwo[i].file);
			vlphwo.erase(vlphwo.begin() + i);
			break;
		}
	}
	return waveOutClose(hwo);
}
// EasyHook will be looking for this export to support DLL injection. If not found then 
// DLL injection will fail.
extern "C" void __declspec(dllexport) __stdcall NativeInjectionEntryPoint(REMOTE_ENTRY_INFO* inRemoteInfo);

void __stdcall NativeInjectionEntryPoint(REMOTE_ENTRY_INFO* inRemoteInfo)
{
	/*
	std::cout << "Injected by process Id: " << inRemoteInfo->HostPID << "\n";
	std::cout << "Passed in data size: " << inRemoteInfo->UserDataSize << "\n";
	if (inRemoteInfo->UserDataSize == sizeof(DWORD))
	{
		gFreqOffset = *reinterpret_cast<DWORD *>(inRemoteInfo->UserData);
		std::cout << "Adjusting Beep frequency by: " << gFreqOffset << "\n";
	}
	*/
	wavdir = (char*)malloc(inRemoteInfo->UserDataSize);
	if (!wavdir)
		return;
	memmove(wavdir, inRemoteInfo->UserData, inRemoteInfo->UserDataSize);
	//MessageBoxA(NULL, wavdir, "Hook", MB_OK | MB_ICONERROR);
	// Perform hooking
	HOOK_TRACE_INFO hHook1 = { NULL }; // keep track of our hook
	HOOK_TRACE_INFO hHook2 = { NULL }; // keep track of our hook
	HOOK_TRACE_INFO hHook3 = { NULL }; // keep track of our hook
									   /*
	std::cout << "\n";
	std::cout << "Win32 Beep found at address: " << GetProcAddress(GetModuleHandle(TEXT("kernel32")), "Beep") << "\n";
	*/
	// Install the hook
	NTSTATUS result = LhInstallHook(
		GetProcAddress(GetModuleHandle(TEXT("WINMM")), "waveOutOpen"),
		myWaveOutOpenHook,
		NULL,
		&hHook1);
	if (FAILED(result))
	{
		std::wstring s(RtlGetLastErrorString());
		s = L"Failed to install hook waveOutOpen: " + s;
		MessageBoxW(NULL, s.c_str(), L"Hook", MB_OK | MB_ICONERROR);
	}
	else
	{
		MessageBoxA(NULL,"Hook 'waveOutOpen installed successfully.","Hook", MB_OK | MB_ICONINFORMATION);
	}

	result = LhInstallHook(
		GetProcAddress(GetModuleHandle(TEXT("WINMM")), "waveOutWrite"),
		myWaveOutWriteHook,
		NULL,
		&hHook2);
	if (FAILED(result))
	{
		std::wstring s(RtlGetLastErrorString());
		s = L"Failed to install hook waveOutWrite: " + s;
		MessageBoxW(NULL, s.c_str(), L"Hook", MB_OK | MB_ICONERROR);
	}
	else
	{
		MessageBoxA(NULL, "Hook 'waveOutWrite installed successfully.", "Hook", MB_OK | MB_ICONINFORMATION);
	}
	result = LhInstallHook(
		GetProcAddress(GetModuleHandle(TEXT("WINMM")), "waveOutClose"),
		myWaveOutCloseHook,
		NULL,
		&hHook3);
	if (FAILED(result))
	{
		std::wstring s(RtlGetLastErrorString());
		s = L"Failed to install hook waveOutClose: " + s;
		MessageBoxW(NULL, s.c_str(), L"Hook", MB_OK | MB_ICONERROR);
	}
	else
	{
		MessageBoxA(NULL, "Hook 'waveOutClose installed successfully.", "Hook", MB_OK | MB_ICONINFORMATION);
	}

	// If the threadId in the ACL is set to 0,
	// then internally EasyHook uses GetCurrentThreadId()
	ULONG ACLEntries[1] = { 0 };

	// Disable the hook for the provided threadIds, enable for all others
	LhSetExclusiveACL(ACLEntries, 1, &hHook1);
	LhSetExclusiveACL(ACLEntries, 1, &hHook2);
	LhSetExclusiveACL(ACLEntries, 1, &hHook3);

	return;
}