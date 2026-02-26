
#include <stdlib.h>
#include <Windows.h>
#include <WinIoCtl.h>
#include <iostream>
#include <stdio.h>
#include <sstream>

#define BUF_LEN 65536 //64kB

// Why do I need this pragma comment?
#pragma comment(lib, "Advapi32.lib") 


bool IsProcessElevated()
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
    {
        return false;
    }

    TOKEN_ELEVATION elevation;
    DWORD size;
    BOOL result = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);

    CloseHandle(token);
    return result && elevation.TokenIsElevated;
}

void fatal(const std::string& msg, int status = 1)
{
    std::cout << msg << "\n";
    system("pause");
    std::exit(status);
}

bool isXml(const WCHAR* name, int len) {
    // Minimum length: ".xml" = 4 chars
    if (len < 4)
        return false;

    // Compare last 4 characters
    return (name[len - 4] == L'.' &&
        (name[len - 3] == L'x' || name[len - 3] == L'X') &&
        (name[len - 2] == L'm' || name[len - 2] == L'M') &&
        (name[len - 1] == L'l' || name[len - 1] == L'L'));
}

int main()
{
    if (!IsProcessElevated()) {
        fatal("ERROR! Must run this program as Administrator!");
    }
    HANDLE hVol;
    CHAR Buffer[BUF_LEN];

    USN_JOURNAL_DATA JournalData;
    PUSN_RECORD_V3 UsnRecord;
    DWORD dwBytes, dwRetBytes;

    READ_USN_JOURNAL_DATA_V1 ReadData = { 0 };
    ReadData.ReasonMask = 0xFFFFFFFF;
    ReadData.ReturnOnlyOnClose = FALSE;
    ReadData.MaxMajorVersion = 3;

    hVol = CreateFile( TEXT("\\\\.\\c:"), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

    if (hVol == INVALID_HANDLE_VALUE)
    {
        fatal("CreateFile failed (" + std::to_string(GetLastError()) + ")");
    }

    if (!DeviceIoControl( hVol, FSCTL_QUERY_USN_JOURNAL, NULL, 0, &JournalData, sizeof(JournalData), &dwBytes, NULL))
    {
        fatal("Query journal failed (" + std::to_string(GetLastError()) + ")");
    }

    ReadData.UsnJournalID = JournalData.UsnJournalID;
    //ReadData.StartUsn = JournalData.FirstUsn;
    ReadData.StartUsn = JournalData.NextUsn;

    while(true)
    {
        memset(Buffer, 0, BUF_LEN);

        if (!DeviceIoControl( hVol, FSCTL_READ_USN_JOURNAL, &ReadData, sizeof(ReadData), &Buffer, BUF_LEN, &dwBytes, NULL))
        {
            fatal("Read journal failed (" + std::to_string(GetLastError()) + ")");
        }

        dwRetBytes = dwBytes - sizeof(USN);

        // Find the first record
        UsnRecord = (PUSN_RECORD_V3)(((PUCHAR)Buffer) + sizeof(USN));

        while (dwRetBytes > 0)
        {
            // Filters Out Active and Temporary Files
            if ((UsnRecord->Reason & USN_REASON_CLOSE ) && !(UsnRecord->FileName[0] == L'$'))
            {
                // Filters XML files Only
                if (isXml(UsnRecord->FileName, UsnRecord->FileNameLength / 2))
                {
                    //Only Interested In XML Files
                    printf("USN: %I64x\n", UsnRecord->Usn);
                    printf("File name: %.*S\n", UsnRecord->FileNameLength / 2, UsnRecord->FileName);
                    printf("Reason: %x\n", UsnRecord->Reason);
                    printf("\n");
                }                
            }

            dwRetBytes -= UsnRecord->RecordLength;

            // Find the next record
            UsnRecord = (PUSN_RECORD_V3)(((PCHAR)UsnRecord) + UsnRecord->RecordLength);
        }
        // Update starting USN for next call
        ReadData.StartUsn = *(USN*)&Buffer;
    }

    CloseHandle(hVol);

    system("pause");
    return 0;
}