
#include "DaqServerInterface.h"
#include <stdexcept> 
#include <string>

HANDLE DaqServerInterface::hPipe = INVALID_HANDLE_VALUE;
HANDLE DaqServerInterface::hRewardEvent = INVALID_HANDLE_VALUE;
HANDLE DaqServerInterface::hRewardDoneEvent = INVALID_HANDLE_VALUE;
HANDLE DaqServerInterface::hDaqServerDoneEvent = INVALID_HANDLE_VALUE;

PROCESS_INFORMATION DaqServerInterface::processInformation;
STARTUPINFOA DaqServerInterface::startupInfo;

DWORD DaqServerInterface::Connect()
{
	if (DaqServerInterface::hPipe != INVALID_HANDLE_VALUE) {		
		return S_OK;
	}
	DaqServerInterface::hPipe = CreateFileA("\\\\.\\pipe\\NidaqServerPipe",
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

	DaqServerInterface::hRewardEvent = CreateEventA(NULL, FALSE, FALSE, "Reward");

	DaqServerInterface::hRewardDoneEvent = CreateEventA(NULL, FALSE, FALSE, "RewardDone");

	if (
		(DaqServerInterface::hPipe != INVALID_HANDLE_VALUE) &
		(DaqServerInterface::hRewardEvent != INVALID_HANDLE_VALUE) & 
		(DaqServerInterface::hRewardDoneEvent != INVALID_HANDLE_VALUE)
		) 
	{
		return S_OK;
	}
	else {
		return GetLastError();
	}


}

DWORD DaqServerInterface::Disconnect()
{
	// Close pipe
	bool closeSuccess = CloseHandle(hPipe);
	DaqServerInterface::hPipe = INVALID_HANDLE_VALUE;

	if (closeSuccess) {
		return S_OK;
	}
	else
	{
		return GetLastError();
	}
	
}

DWORD DaqServerInterface::SendEventmarker(unsigned short markercode)
{
	#pragma pack(push, 1)
	struct {
		BYTE type = 6;
		unsigned short code = 0;
	} msg;
	#pragma pack(pop)		

	msg.code = markercode;
	DWORD nBytesWritten = 0;
	
	bool success = WriteFile(hPipe, &msg.type, 3, &nBytesWritten, NULL);
	if ( success & (nBytesWritten == 3) ) { return S_OK; }
	else { return GetLastError(); }

}

DWORD DaqServerInterface::GiveReward()
{
	bool success = SetEvent(hRewardEvent);	
	if (success) { return S_OK; }
	else { return GetLastError(); }
	//DWORD result = WaitForSingleObject(DaqServerInterface::hRewardDoneEvent, DWORD(60000));
	//if (result == WAIT_OBJECT_0) {
	//	return S_OK;
	//}
	//else
	//{
	//	return result;
	//}
}

DWORD DaqServerInterface::GiveReward( unsigned short * pTimes, unsigned short nTimes )
{
	if (nTimes > 15)
	{
		return E_INVALIDARG;
	}
	#pragma pack(push, 1)
	static struct {
		BYTE type = 5;
		unsigned short times[31] = { 0 };
	} RewardSequence;
	#pragma pack(pop)	

	for (unsigned int idx = 0; idx < nTimes; idx++)
	{
		RewardSequence.times[idx] = pTimes[idx];
	}

	DWORD nBytesWritten = 0;
	bool success = WriteFile(DaqServerInterface::hPipe, &RewardSequence.type, 2*nTimes + 1 , &nBytesWritten, NULL);
	if ( (!success) & (nBytesWritten == 2 * nTimes + 1))
	{
		return GetLastError();
	}

	return S_OK;
	
	//DWORD result = WaitForSingleObject(DaqServerInterface::hRewardDoneEvent, DWORD(20000));
	//if (result == WAIT_OBJECT_0) {
	//	return S_OK;
	//}
	//else
	//{
	//	return result;
	//}
	
}

DWORD DaqServerInterface::SetRewardTime(unsigned short int timems)
{
	#pragma pack(push, 1)
	struct {
		BYTE type = 4;
		unsigned short time = 0;
	} RewardTimeMsg;
	#pragma pack(pop)	
	RewardTimeMsg.time = timems;

	DWORD nBytesWritten = 0;
	bool success = WriteFile(DaqServerInterface::hPipe, &RewardTimeMsg.type, 3, &nBytesWritten, NULL);

	if ((success) & (nBytesWritten == 3))
	{
		return S_OK;			
	}
	else
	{
		return GetLastError();
	}
	
}

DWORD DaqServerInterface::GetTotalRewardTime(unsigned long int * totalTime)
{
	DWORD nBytesWritten = 0;
	DWORD msg = 8;
	bool success = WriteFile(DaqServerInterface::hPipe, &msg, 1, &nBytesWritten, NULL);
	if (!success) {
		return GetLastError();
	}	
	DWORD nRead = 0;
	success = ReadFile(DaqServerInterface::hPipe, totalTime, 4, &nRead, NULL);
	if (success) {
		return S_OK;
	}
	else
	{
		return GetLastError();
	}

}

DWORD DaqServerInterface::AddLinePulse(BYTE linenumber, std::string pulseEventName)
{
	#pragma pack(push, 1)
	struct {
		BYTE type = 1;
		BYTE line = 0;
		std::string pulse;
	} PulseLineMsg;
	#pragma pack(pop)
	
	PulseLineMsg.line = linenumber;
	PulseLineMsg.pulse = pulseEventName;


	DWORD nBytesWritten = 0;
	bool success = WriteFile(hPipe, &PulseLineMsg.type, sizeof(PulseLineMsg), &nBytesWritten, NULL);
	if ( success & (nBytesWritten == sizeof(PulseLineMsg)) ) { return S_OK; }
	else { return GetLastError(); }

}

DWORD DaqServerInterface::AddLineOnOff(BYTE linenumber, std::string onEventName, std::string offEventName)
{
	int nOn = onEventName.length();
	int nOff = offEventName.length();
	
	if (nOn+nOff+2 > 30)
	{
		return E_INVALIDARG;
	}
	#pragma pack(push, 1)
	struct {
		BYTE type = 2;
		BYTE line = 0;
		//char name[30] = "OnEvent\0OffEvent";
		char name[30] = {};
	} OnOffLineMsg;
	#pragma pack(pop)

	// Need memcpy to include the null character
	memcpy(OnOffLineMsg.name, onEventName.c_str(), nOn+1);
	memcpy(&OnOffLineMsg.name[nOn+1], offEventName.c_str(), nOff+1);

	OnOffLineMsg.line = linenumber;

	DWORD nBytesWritten = 0;
	bool success = WriteFile(hPipe, &OnOffLineMsg.type, sizeof(OnOffLineMsg), &nBytesWritten, NULL);
	if ( success & (nBytesWritten == sizeof(OnOffLineMsg)) ) { return S_OK; }
	else { return GetLastError(); }

}

DWORD DaqServerInterface::AddOutputLinePulse(BYTE linenumber, std::string pulseOutName)
{
	#pragma pack(push, 1)
	struct {
		BYTE type = 9;
		BYTE line = 0;
		std::string pulse;
	} OutLineMsg;
	#pragma pack(pop)
	
	OutLineMsg.line = linenumber;
	OutLineMsg.pulse = pulseOutName;


	DWORD nBytesWritten = 0;
	bool success = WriteFile(hPipe, &OutLineMsg.type, sizeof(OutLineMsg), &nBytesWritten, NULL);
	if ( success & (nBytesWritten == sizeof(OutLineMsg)) ) { return S_OK; }
	else { return GetLastError(); }

}

DWORD DaqServerInterface::SetPulseDuration(BYTE linenumber, unsigned short int timems)
{
	#pragma pack(push, 1)
	struct {
		BYTE type = 4;
		BYTE line = 0;
		unsigned short time = 0;
	} PulseTimeMsg;
	#pragma pack(pop)
	PulseTimeMsg.line = linenumber;
	PulseTimeMsg.time = timems;

	DWORD nBytesWritten = 0;
	bool success = WriteFile(DaqServerInterface::hPipe, &PulseTimeMsg.type, sizeof(PulseTimeMsg), &nBytesWritten, NULL);
	if ( success & (nBytesWritten == sizeof(PulseTimeMsg)) ) { return S_OK; }
	else { return GetLastError(); }
	
}

DWORD DaqServerInterface::StartTrackingLines()
{
	DaqServerInterface::hDaqServerDoneEvent = CreateEventA(NULL, FALSE, FALSE, "DaqServerDone");
	bool success = ResetEvent(DaqServerInterface::hDaqServerDoneEvent);
	if (!success) {
		return GetLastError();
	}
	DWORD nBytesWritten = 0;
	DWORD msg = 3;
	bool success2 = WriteFile(DaqServerInterface::hPipe, &msg, 1, &nBytesWritten, NULL);
	if (!success2) {
		return GetLastError();
	}
	DWORD WaitResult;
	WaitResult = WaitForSingleObject(DaqServerInterface::hDaqServerDoneEvent, DWORD(10000));
	switch (WaitResult)
    {
        case WAIT_OBJECT_0:
			return S_OK;
        // An error occurred
        default:
            return GetLastError();
    }

}

DWORD DaqServerInterface::StartDaqserverProcess()
{

	ZeroMemory(&DaqServerInterface::startupInfo, sizeof(DaqServerInterface::startupInfo));
	DaqServerInterface::startupInfo.cb = sizeof(DaqServerInterface::startupInfo);
	ZeroMemory(&DaqServerInterface::processInformation, sizeof(DaqServerInterface::processInformation));

	// Start the child process. 
	if (!CreateProcessA("NidaqServer.exe",   // Module name
		NULL,           // Command line
		NULL,           // Process handle not inheritable
		NULL,           // Thread handle not inheritable
		FALSE,          // Set handle inheritance to FALSE
		0,              // No creation flags
		NULL,           // Use parent's environment block
		NULL,           // Use parent's starting directory 
		&DaqServerInterface::startupInfo,            // Pointer to STARTUPINFO structure
		&DaqServerInterface::processInformation)           // Pointer to PROCESS_INFORMATION structure
		)
	{
		return GetLastError();
	}
	else {
		return S_OK;
	}
}

DWORD DaqServerInterface::StopDaqserverProcess()
{
	TerminateProcess(DaqServerInterface::processInformation.hProcess, 1);
	CloseHandle(DaqServerInterface::processInformation.hProcess);
	CloseHandle(DaqServerInterface::processInformation.hThread);
	return S_OK;
}

