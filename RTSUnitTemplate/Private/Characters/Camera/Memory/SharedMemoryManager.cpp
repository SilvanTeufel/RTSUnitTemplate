// SharedMemoryManager.cpp
#include "Characters/Camera/Memory/SharedMemoryManager.h"
#include "HAL/Platform.h"
#include "HAL/PlatformMemory.h"
#include "Logging/LogMacros.h"

FSharedMemoryManager::FSharedMemoryManager(const FString& SharedMemoryName, size_t MemorySize)
{
    // Create a named shared memory mapping
    FString MappingName = TEXT("Global\\") + SharedMemoryName;
    MappingHandle = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, MemorySize, *MappingName);
    UE_LOG(LogTemp, Log, TEXT("[FSharedMemoryManager] Creating shared memory: Name=%s, Size=%zu, Handle=%p"), *MappingName, MemorySize, MappingHandle);
    if (MappingHandle)
    {
       SharedDataPtr = (SharedData*) MapViewOfFile(MappingHandle, FILE_MAP_ALL_ACCESS, 0, 0, MemorySize);
       UE_LOG(LogTemp, Log, TEXT("[FSharedMemoryManager] Mapping view of file: Handle=%p, Ptr=%p"), MappingHandle, SharedDataPtr);
       if (SharedDataPtr)
       {
           // Initialize flags
           SharedDataPtr->bNewGameStateAvailable = false;
           SharedDataPtr->bNewActionAvailable = false;
       }
       else
       {
           UE_LOG(LogTemp, Error, TEXT("[FSharedMemoryManager] Failed to map view of file. Error=%d"), GetLastError());
           CloseHandle(MappingHandle);
           MappingHandle = nullptr;
       }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[FSharedMemoryManager] Failed to create file mapping. Name=%s, Size=%zu, Error=%d"), *MappingName, MemorySize, GetLastError());
    }
}

FSharedMemoryManager::~FSharedMemoryManager()
{
    if (SharedDataPtr)
    {
       UnmapViewOfFile(SharedDataPtr);
       UE_LOG(LogTemp, Log, TEXT("[FSharedMemoryManager] Unmapped view of file: Ptr=%p"), SharedDataPtr);
       SharedDataPtr = nullptr;
    }
    if (MappingHandle)
    {
       CloseHandle(MappingHandle);
       UE_LOG(LogTemp, Log, TEXT("[FSharedMemoryManager] Closed file mapping handle: Handle=%p"), MappingHandle);
       MappingHandle = nullptr;
    }
}



bool FSharedMemoryManager::WriteGameState(const FString& GameStateJSON)
{
    if (!SharedDataPtr)
    {
       UE_LOG(LogTemp, Error, TEXT("[FSharedMemoryManager] SharedDataPtr is null in WriteGameState."));
       return false;
    }
    if (!SharedDataPtr->GameState)
    {
        UE_LOG(LogTemp, Error, TEXT("[FSharedMemoryManager] SharedDataPtr->GameState is null in WriteGameState. This should not happen if memory is allocated correctly."));
        return false;
    }

	//UE_LOG(LogTemp, Log, TEXT("[FSharedMemoryManager] WriteGameState called. GameStateJSON Length: %d"), GameStateJSON.Len());
    //UE_LOG(LogTemp, Log, TEXT("[FSharedMemoryManager] Address of SharedDataPtr->GameState: %p"), SharedDataPtr->GameState);

    int32 StringLength = GameStateJSON.Len();
    int32 BufferSizeInChars = 1024;
    int32 BytesToCopy = FMath::Min(StringLength, BufferSizeInChars - 1) * sizeof(TCHAR); // Leave space for null terminator

    if (BytesToCopy > 0)
    {
        FMemory::Memcpy(SharedDataPtr->GameState, *GameStateJSON, BytesToCopy);
    }
    SharedDataPtr->GameState[BytesToCopy / sizeof(TCHAR)] = TEXT('\0'); // Null-terminate


	// Log the content of the GameState buffer
	FString LogString;
	for (int32 i = 0; i < BufferSizeInChars; ++i)
	{
		LogString += SharedDataPtr->GameState[i];
		if (SharedDataPtr->GameState[i] == TEXT('\0'))
		{
			break; // Stop logging at the null terminator
		}
	}
	//UE_LOG(LogTemp, Log, TEXT("[FSharedMemoryManager] Content of SharedDataPtr->GameState: %s"), *LogString);


    SharedDataPtr->bNewGameStateAvailable = true;
    return true;
}

FString FSharedMemoryManager::ReadAction()
{
    if (!SharedDataPtr || !SharedDataPtr->bNewActionAvailable)
    {
       return FString();
    }

    FString Action = FString(SharedDataPtr->Action);
    // Reset flag
    SharedDataPtr->bNewActionAvailable = false;
    return Action;
}