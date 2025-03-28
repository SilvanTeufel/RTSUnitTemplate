// SharedMemoryManager.h
#pragma once

#include "CoreMinimal.h"
#include "Windows/WindowsHWrapper.h"

struct SharedData
{
	bool bNewGameStateAvailable;
	bool bNewActionAvailable;
	TCHAR GameState[1024];  // Use TCHAR instead of char
	TCHAR Action[1024];     // Use TCHAR instead of char
};

class FSharedMemoryManager
{
public:
	FSharedMemoryManager(const FString& SharedMemoryName, size_t MemorySize);
	~FSharedMemoryManager();

	bool WriteGameState(const FString& GameStateJSON);
	FString ReadAction();

private:
	HANDLE MappingHandle;
	SharedData* SharedDataPtr;
};

