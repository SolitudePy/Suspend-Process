#include <windows.h>
#include <stdio.h>
#include <tchar.h>

// Define missing structures and constants for Windows Native API
#define ProcessBasicInformation 0

typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
} UNICODE_STRING;

typedef struct _RTL_USER_PROCESS_PARAMETERS {
    BYTE Reserved1[16];
    PVOID Reserved2[10];
    UNICODE_STRING ImagePathName;
    UNICODE_STRING CommandLine;
} RTL_USER_PROCESS_PARAMETERS;

typedef struct _PEB {
    BYTE Reserved1[2];
    BYTE BeingDebugged;
    BYTE Reserved2[1];
    PVOID Reserved3[2];
    struct _RTL_USER_PROCESS_PARAMETERS* ProcessParameters;
} PEB;

typedef struct _PROCESS_BASIC_INFORMATION {
    PVOID Reserved1;
    PEB* PebBaseAddress;
    PVOID Reserved2[2];
    ULONG_PTR UniqueProcessId;
    PVOID Reserved3;
} PROCESS_BASIC_INFORMATION;

typedef LONG NTSTATUS;

// Declare the NtQueryInformationProcess function
NTSTATUS NTAPI NtQueryInformationProcess(
    HANDLE ProcessHandle,
    ULONG ProcessInformationClass,
    PVOID ProcessInformation,
    ULONG ProcessInformationLength,
    PULONG ReturnLength
);

int main() {
    STARTUPINFOEX startupInfoEx = {0};
    PROCESS_INFORMATION processInfo = {0};
    HANDLE jobHandle = NULL;
    SIZE_T attributeListSize = 0;

    // Create a job object
    jobHandle = CreateJobObject(NULL, NULL);
    if (jobHandle == NULL) {
        printf("Failed to create job object. Error: %lu\n", GetLastError());
        return 1;
    }

    // Enable the Freeze feature for the job object
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo = {0};
    jobInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_BREAKAWAY_OK;
    if (!SetInformationJobObject(jobHandle, JobObjectExtendedLimitInformation, &jobInfo, sizeof(jobInfo))) {
        printf("Failed to set job object information. Error: %lu\n", GetLastError());
        CloseHandle(jobHandle);
        return 1;
    }

    // Initialize STARTUPINFOEX
    startupInfoEx.StartupInfo.cb = sizeof(STARTUPINFOEX);

    // Query the size of the attribute list
    InitializeProcThreadAttributeList(NULL, 1, 0, &attributeListSize);
    startupInfoEx.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)malloc(attributeListSize);
    if (startupInfoEx.lpAttributeList == NULL) {
        printf("Failed to allocate memory for attribute list.\n");
        CloseHandle(jobHandle);
        return 1;
    }

    // Initialize the attribute list
    if (!InitializeProcThreadAttributeList(startupInfoEx.lpAttributeList, 1, 0, &attributeListSize)) {
        printf("Failed to initialize attribute list. Error: %lu\n", GetLastError());
        free(startupInfoEx.lpAttributeList);
        CloseHandle(jobHandle);
        return 1;
    }

    // Set the job object in the attribute list
    if (!UpdateProcThreadAttribute(startupInfoEx.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_JOB_LIST, &jobHandle, sizeof(HANDLE), NULL, NULL)) {
        printf("Failed to update attribute list with job object. Error: %lu\n", GetLastError());
        DeleteProcThreadAttributeList(startupInfoEx.lpAttributeList);
        free(startupInfoEx.lpAttributeList);
        CloseHandle(jobHandle);
        return 1;
    }

    // Create the child process, which will inherit the job object
    if (!CreateProcess(
            NULL,                // Application name
            "cmd.exe \"                                                                 \"", // Command line with enough spaces
            NULL,                // Process attributes
            NULL,                // Thread attributes
            FALSE,               // Inherit handles
            EXTENDED_STARTUPINFO_PRESENT, // Creation flags
            NULL,                // Environment
            NULL,                // Current directory
            &startupInfoEx.StartupInfo, // Startup info
            &processInfo)) {     // Process information
        printf("Failed to create process. Error: %lu\n", GetLastError());
        DeleteProcThreadAttributeList(startupInfoEx.lpAttributeList);
        free(startupInfoEx.lpAttributeList);
        CloseHandle(jobHandle);
        return 1;
    }

    printf("Child process created using job object. PID: %lu\n", processInfo.dwProcessId);

    // Modify the PEB to update the command line
    PROCESS_BASIC_INFORMATION pbi;
    ULONG returnLength;
    NTSTATUS status = NtQueryInformationProcess(processInfo.hProcess, ProcessBasicInformation, &pbi, sizeof(pbi), &returnLength);
    if (status != 0) {
        printf("Failed to query process information. Error: %lu\n", GetLastError());
        TerminateProcess(processInfo.hProcess, 1);
        DeleteProcThreadAttributeList(startupInfoEx.lpAttributeList);
        free(startupInfoEx.lpAttributeList);
        CloseHandle(jobHandle);
        return 1;
    }

    PEB* peb = (PEB*)pbi.PebBaseAddress;
    RTL_USER_PROCESS_PARAMETERS* params;
    ReadProcessMemory(processInfo.hProcess, &peb->ProcessParameters, &params, sizeof(params), NULL);

    WCHAR newCommandLine[] = L"cmd.exe /C echo Hello, World!";
    size_t newCommandLineLength = wcslen(newCommandLine) * sizeof(WCHAR);
    WriteProcessMemory(processInfo.hProcess, params->CommandLine.Buffer, newCommandLine, newCommandLineLength, NULL);

    // Unfreeze the job object to allow the process to run
    JOBOBJECT_BASIC_LIMIT_INFORMATION jobLimitInfo = {0};
    jobLimitInfo.LimitFlags = 0; // Clear the freeze flag
    if (!SetInformationJobObject(jobHandle, JobObjectBasicLimitInformation, &jobLimitInfo, sizeof(jobLimitInfo))) {
        printf("Failed to unfreeze the job object. Error: %lu\n", GetLastError());
        TerminateProcess(processInfo.hProcess, 1);
        DeleteProcThreadAttributeList(startupInfoEx.lpAttributeList);
        free(startupInfoEx.lpAttributeList);
        CloseHandle(jobHandle);
        return 1;
    }

    // Clean up
    DeleteProcThreadAttributeList(startupInfoEx.lpAttributeList);
    free(startupInfoEx.lpAttributeList);
    CloseHandle(jobHandle);
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);

    return 0;
}