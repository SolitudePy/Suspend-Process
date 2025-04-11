#include <windows.h>
#include <stdio.h>

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

    // Create the child process, which will inherit the job object and start in a suspended state
    if (!CreateProcess(
            NULL,                // Application name
            "notepad.exe",      // Command line
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

    printf("Child process created in suspended state using job object. PID: %lu\n", processInfo.dwProcessId);

    // Clean up
    DeleteProcThreadAttributeList(startupInfoEx.lpAttributeList);
    free(startupInfoEx.lpAttributeList);
    CloseHandle(jobHandle);
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);

    return 0;
}