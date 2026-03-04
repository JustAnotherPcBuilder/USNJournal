#include <Windows.h>
#include <WinIoCtl.h>
#include <iostream>
#include <stdio.h>
#include <sstream>

#define BUF_LEN 64 * 1024 // 64kB

// Why do I need this pragma comment?
#pragma comment(lib, "Advapi32.lib") 

/**
 * Allows pausing the console so view USN Journal output while debugging.
 * Can be left alone as Release will not cause a system pause regardless.
 */
#ifdef _DEBUG
#include <cstdlib>
#define DEBUG_PAUSE() std::system("pause")
#else
#define DEBUG_PAUSE() ((void)0)
#endif

/**
 * Queries the Token Elevation State by opening the current process 
 * security token to determine whether the process is running with
 * elevated privilege.
 * 
 * @return True if the process token indicates elevation
 */
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

/*
 * Terminates the program after printing an error message.
 * 
 * @param msg Error message to display.
 */
void fatal(const std::string& msg, int status = 1)
{
    std::cerr << msg << "\n";
    DEBUG_PAUSE();
    std::exit(status);
}

// In order to determine the file path, you would need to use the File Reference Number (FRN) to query the file system for the full path.
// This is highly inefficient for multiple files detected in a volume.
// Instead, we can cre

void buildDirectoryStructure() {
    // Build a directory structure in memory by querying the file system for all files and their FRNs.
    // This allows for quick lookups of file paths based on FRNs when processing USN records.
}


bool isInDirectoryOfInterest(PUSN_RECORD_V3 record)
{
    // Need to check the Directory Structure to 
    return true;
}



/**
 * Determines whether the given UTF-16 filename ends with the extension 'XML'
 * using a fast case-insensitive comparison. 
 * 
 * @param name Pointer to a UTF-16 filename buffer.
 * @param len  Length of the filename in UTF-16 code units.
 *
 * @return true if the filename is an XML file.
 */
bool isXml(const WCHAR* name, int len)
{
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
    if (!IsProcessElevated())
        fatal("ERROR! Must run this program as Administrator!");
    
	// Build the directory structure in memory for quick lookups of file paths based on FRNs when processing USN records.
    buildDirectoryStructure();

    HANDLE hVol = CreateFile( TEXT("\\\\.\\c:"), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

    if (hVol == INVALID_HANDLE_VALUE)
        fatal("CreateFile failed (" + std::to_string(GetLastError()) + ")");
    
    DWORD BytesRemaining, BytesReturned;
    USN_JOURNAL_DATA Journal;
    if (!DeviceIoControl( hVol, FSCTL_QUERY_USN_JOURNAL, NULL, 0, &Journal, sizeof(Journal), &BytesReturned, NULL))
        fatal("Query journal failed (" + std::to_string(GetLastError()) + ")");
   
    READ_USN_JOURNAL_DATA_V1 UsnData = { 0 };
    UsnData.ReasonMask = USN_REASON_CLOSE;
    UsnData.ReturnOnlyOnClose = FALSE;
    UsnData.MaxMajorVersion = 3;
    UsnData.UsnJournalID = Journal.UsnJournalID;
    UsnData.StartUsn = Journal.NextUsn;
    UsnData.BytesToWaitFor = 1;
    UsnData.Timeout = 0;

    static CHAR Buffer[BUF_LEN];
    PUSN_RECORD_V3 UsnRecord, CurrentUsnRecord;
    while(true)
    {
        /*
        * DeviceIoControl returns The USN (64bit) along with any records associated 
        * with that USN. You can think of the USN as the file's ID.
        +---------------- + ---------------- + ---------------- + ...
        | USN(8 bytes) | USN_RECORD_V3 | USN_RECORD_V3 | ...
        + ---------------- + ---------------- + ---------------- + ...
        We are interested in data only, so we skip the USN to process the USN Records.
        */
        if (!DeviceIoControl( hVol, FSCTL_READ_USN_JOURNAL, &UsnData, sizeof(UsnData), &Buffer, BUF_LEN, &BytesReturned, NULL))
            fatal("Read journal failed (" + std::to_string(GetLastError()) + ")");
        
        UsnRecord = (PUSN_RECORD_V3)(((PUCHAR)Buffer) + sizeof(USN));
        BytesRemaining = BytesReturned - sizeof(USN);        

        // Process all USN Record Data in Buffer
        while (BytesRemaining > 0)
        {
            CurrentUsnRecord = UsnRecord;
            BytesRemaining -= UsnRecord->RecordLength;
            UsnRecord = (PUSN_RECORD_V3)(((PCHAR)UsnRecord) + UsnRecord->RecordLength);

            // Filters NTFS Metadata Files
            if (CurrentUsnRecord->FileName[0] == L'$')
                continue;
 
            if (isInDirectoryOfInterest(CurrentUsnRecord)){

                // Filters non-XML files
                if (!isXml(CurrentUsnRecord->FileName, CurrentUsnRecord->FileNameLength / 2))
                    continue;
                printf("USN: %I64x\n", CurrentUsnRecord->Usn);
                printf("File name: %.*S\n", CurrentUsnRecord->FileNameLength / 2, CurrentUsnRecord->FileName);
                printf("Reason: %x\n", CurrentUsnRecord->Reason);
                printf("\n");
            }
            
        }

        // Update starting USN for next call
        UsnData.StartUsn = *(USN*)&Buffer;
    }

    CloseHandle(hVol);
    DEBUG_PAUSE();
    return 0;
}