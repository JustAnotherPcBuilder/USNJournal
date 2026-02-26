#include <Windows.h>
#include <WinIoCtl.h>
#include <iostream>
#include <stdio.h>
#include <sstream>

#ifdef _DEBUG
#include <cstdlib>
#define DEBUG_PAUSE() std::system("pause")
#else
#define DEBUG_PAUSE() ((void)0)
#endif

#define BUF_LEN 64 * 1024 //64kB

// Why do I need this pragma comment?
#pragma comment(lib, "Advapi32.lib") 

bool IsProcessElevated()
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return false;
    
    TOKEN_ELEVATION elevation;
    DWORD size;
    BOOL result = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);

    CloseHandle(token);
    return result && elevation.TokenIsElevated;
}

void fatal(const std::string& msg, int status = 1)
{
    std::cerr << msg << "\n";
    DEBUG_PAUSE();
    std::exit(status);
}

bool isXml(const WCHAR* name, int len) {
    // Minimum length: ".xml" = 4 chars
    if (len < 4) 
        return false;

    // Compare last 3 characters
    return (name[len - 3] == L'x' || name[len - 3] == L'X') &&
           (name[len - 2] == L'm' || name[len - 2] == L'M') &&
           (name[len - 1] == L'l' || name[len - 1] == L'L');
}

int main()
{
    if (!IsProcessElevated()) {
        fatal("ERROR! Must run this program as Administrator!");
    }
    HANDLE hVol;
    CHAR Buffer[BUF_LEN];

    USN_JOURNAL_DATA JournalData;
    PUSN_RECORD_V3 UsnRecord, CurrentUsnRecord;
    DWORD dwBytes, dwRetBytes;

    READ_USN_JOURNAL_DATA_V1 ReadData = { 0 };
    ReadData.ReasonMask = 0xFFFFFFFF;
    ReadData.ReturnOnlyOnClose = FALSE;
    ReadData.MaxMajorVersion = 3;

    hVol = CreateFile( TEXT("\\\\.\\c:"), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

    if (hVol == INVALID_HANDLE_VALUE)
        fatal("CreateFile failed (" + std::to_string(GetLastError()) + ")");
    

    if (!DeviceIoControl( hVol, FSCTL_QUERY_USN_JOURNAL, NULL, 0, &JournalData, sizeof(JournalData), &dwBytes, NULL))
        fatal("Query journal failed (" + std::to_string(GetLastError()) + ")");
    
    ReadData.UsnJournalID = JournalData.UsnJournalID;
    ReadData.StartUsn = JournalData.NextUsn;

    while(true)
    {
        if (!DeviceIoControl( hVol, FSCTL_READ_USN_JOURNAL, &ReadData, sizeof(ReadData), &Buffer, BUF_LEN, &dwBytes, NULL))
            fatal("Read journal failed (" + std::to_string(GetLastError()) + ")"); 

        dwRetBytes = dwBytes - sizeof(USN);

        // Find the first record
        UsnRecord = (PUSN_RECORD_V3)(((PUCHAR)Buffer) + sizeof(USN));

        while (dwRetBytes > 0)
        {
            CurrentUsnRecord = UsnRecord;
            dwRetBytes -= UsnRecord->RecordLength;
            UsnRecord = (PUSN_RECORD_V3)(((PCHAR)UsnRecord) + UsnRecord->RecordLength);

            // Filters Active Files
            if (!(CurrentUsnRecord->Reason & USN_REASON_CLOSE))
                continue;

            // Filters Temporary Files
            if (CurrentUsnRecord->FileName[0] == L'$')
                continue;

            // Filters XML files Only
            if (!isXml(CurrentUsnRecord->FileName, CurrentUsnRecord->FileNameLength / 2))
                continue;
                
            // Process Logic
            printf("USN: %I64x\n", CurrentUsnRecord->Usn);
            printf("File name: %.*S\n", CurrentUsnRecord->FileNameLength / 2, CurrentUsnRecord->FileName);
            printf("Reason: %x\n", CurrentUsnRecord->Reason);
            printf("\n");
        }

        // Update starting USN for next call
        ReadData.StartUsn = *(USN*)&Buffer;
    }

    CloseHandle(hVol);
    DEBUG_PAUSE();
    return 0;
}