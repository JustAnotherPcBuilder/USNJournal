
#include <stdlib.h>
#include <Windows.h>
#include <WinIoCtl.h>
#include <stdio.h>

#define BUF_LEN 4096

int main()
{
    HANDLE hVol;
    CHAR Buffer[BUF_LEN];

    USN_JOURNAL_DATA JournalData;
    PUSN_RECORD_V3 UsnRecord;
    DWORD dwBytes;
    DWORD dwRetBytes;


    READ_USN_JOURNAL_DATA_V1 ReadData = { 0 };
    ReadData.ReasonMask = 0xFFFFFFFF;
    ReadData.ReturnOnlyOnClose = FALSE;
    ReadData.MaxMajorVersion = 3;

    hVol = CreateFile(  
                TEXT("\\\\.\\c:"),
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                OPEN_EXISTING,
                0,
                NULL);

    if (hVol == INVALID_HANDLE_VALUE)
    {
        printf("CreateFile failed (%d)\n", GetLastError());
		system("pause");
        return 1;
    }

    if (!DeviceIoControl(hVol,
        FSCTL_QUERY_USN_JOURNAL,
        NULL,
        0,
        &JournalData,
        sizeof(JournalData),
        &dwBytes,
        NULL))
    {
        printf("Query journal failed (%d)\n", GetLastError());
		system("pause");
        return 1; 
    }

    ReadData.UsnJournalID = JournalData.UsnJournalID;
	//ReadData.StartUsn = JournalData.NextUsn;
    ReadData.StartUsn = JournalData.FirstUsn;

    printf("Journal ID: %I64x\n", JournalData.UsnJournalID);
    printf("FirstUsn: %I64x\n\n", JournalData.FirstUsn);

    //for (int I = 0; I <= 10; I++)
    while(ReadData.StartUsn > 0)
    {
        memset(Buffer, 0, BUF_LEN);

        if (!DeviceIoControl(hVol,
            FSCTL_READ_USN_JOURNAL,
            &ReadData,
            sizeof(ReadData),
            &Buffer,
            BUF_LEN,
            &dwBytes,
            NULL))
        {
            printf("Read journal failed (%d)\n", GetLastError());
			system("pause");
            return 1 ;
        }

        dwRetBytes = dwBytes - sizeof(USN);

        // Find the first record
        UsnRecord = (PUSN_RECORD_V3)(((PUCHAR)Buffer) + sizeof(USN));

        printf("****************************************\n");

        // This loop could go on for a long time, given the current buffer size.
        while (dwRetBytes > 0)
        {

            //printf("USN: %I64x\n", UsnRecord->Usn);
            //printf("File name: %.*S\n",
            //    UsnRecord->FileNameLength / 2,
            //    UsnRecord->FileName);
            //printf("Reason: %x\n", UsnRecord->Reason);
            //printf("\n");

            dwRetBytes -= UsnRecord->RecordLength;

            // Find the next record
            UsnRecord = (PUSN_RECORD_V3)(((PCHAR)UsnRecord) +
                UsnRecord->RecordLength);
        }
        // Update starting USN for next call
        ReadData.StartUsn = *(USN*)&Buffer;
    }

    CloseHandle(hVol);

    system("pause");
    return 0;
}