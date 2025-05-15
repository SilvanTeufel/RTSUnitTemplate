// SimpleFogManager.cpp
#include "Fog/SimpleFogOfWarManager.h"
#include "Fog/FogRevealerInterface.h" // Include your interface
#include "DrawDebugHelpers.h"     // For debug drawing
#include "Kismet/GameplayStatics.h" // For finding actors (optional)

ASimpleFogOfWarManager::ASimpleFogOfWarManager()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ASimpleFogOfWarManager::BeginPlay()
{
	Super::BeginPlay();
	InitializeFogGrid();

	// Example: Find all actors implementing the interface at startup (optional)
	// TArray<AActor*> FoundActors;
	// UGameplayStatics::GetAllActorsWithInterface(GetWorld(), UFogRevealerInterface::StaticClass(), FoundActors);
	// for (AActor* Actor : FoundActors)
	// {
	// 	AddRevealer(Actor);
	// }
}

void ASimpleFogOfWarManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TimeSinceLastUpdate += DeltaTime;
	if (TimeSinceLastUpdate >= UpdateInterval)
	{
		UpdateFog();
		TimeSinceLastUpdate = 0.0f;
	}

	if (bUseDebugDrawing)
	{
		DrawDebugFog();
	}
}

void ASimpleFogOfWarManager::InitializeFogGrid()
{
	FogGrid.Init(EFogState::Hidden, FogGridWidth * FogGridHeight);
	UE_LOG(LogTemp, Log, TEXT("Fog Grid Initialized with %d cells."), FogGrid.Num());
}

void ASimpleFogOfWarManager::AddRevealer(AActor* RevealerActor)
{
	// Check if the actor is valid and implements the interface
	if (RevealerActor && RevealerActor->GetClass()->ImplementsInterface(UFogRevealerInterface::StaticClass()))
	{
		// Create a TScriptInterface and set the object.
		// TScriptInterface handles finding and setting the interface pointer internally
		// if the object implements the interface.
		TScriptInterface<IFogRevealerInterface> RevealerInterface;
		RevealerInterface.SetObject(RevealerActor);

		// Now, you can directly use the TScriptInterface
		if (RevealerInterface) // This checks if both Object and Interface pointers are valid
		{
			// Use AddUnique to prevent duplicates
			Revealers.AddUnique(RevealerInterface);
			UE_LOG(LogTemp, Log, TEXT("Added revealer: %s"), *RevealerActor->GetName());
		}
		else
		{
			// This case should ideally not be reached if ImplementsInterface returned true,
			// but it's a good safety check if something goes wrong internally with TScriptInterface.
			UE_LOG(LogTemp, Error, TEXT("Failed to set TScriptInterface for actor %s despite implementing interface."), *RevealerActor->GetName());
		}
	}
	else
	{
		// Log if the actor is invalid or doesn't implement the interface
		UE_LOG(LogTemp, Warning, TEXT("Attempted to add an invalid revealer or actor not implementing IFogRevealerInterface."));
	}
}

void ASimpleFogOfWarManager::RemoveRevealer(AActor* RevealerActor)
{
	if (RevealerActor)
	{
		for (int32 i = Revealers.Num() - 1; i >= 0; --i)
		{
			if (Revealers[i].GetObject() == RevealerActor)
			{
				Revealers.RemoveAt(i);
				UE_LOG(LogTemp, Log, TEXT("Removed revealer: %s"), *RevealerActor->GetName());
				return;
			}
		}
	}
}

void ASimpleFogOfWarManager::UpdateFog()
{
	// 1. Transition all currently 'Visible' cells to 'Explored' (Shroud)
	for (int32 i = 0; i < FogGrid.Num(); ++i)
	{
		if (FogGrid[i] == EFogState::Visible)
		{
			FogGrid[i] = EFogState::Explored;
		}
	}

	// 2. Reveal new areas based on revealer positions
	for (const auto& Revealer : Revealers)
	{
		if (Revealer && IFogRevealerInterface::Execute_IsRevealerActive(Revealer.GetObject()))
		{
			FVector RevealerLocation = IFogRevealerInterface::Execute_GetRevealerLocation(Revealer.GetObject());
			float VisionRadius = IFogRevealerInterface::Execute_GetVisionRadius(Revealer.GetObject());
			float VisionRadiusSq = VisionRadius * VisionRadius;

			// Determine grid cells affected by this revealer
			int32 MinX, MinY, MaxX, MaxY;
			WorldToGrid(RevealerLocation - FVector(VisionRadius, VisionRadius, 0.f), MinX, MinY);
			WorldToGrid(RevealerLocation + FVector(VisionRadius, VisionRadius, 0.f), MaxX, MaxY);
			
			MinX = FMath::Max(0, MinX);
			MinY = FMath::Max(0, MinY);
			MaxX = FMath::Min(FogGridWidth - 1, MaxX);
			MaxY = FMath::Min(FogGridHeight - 1, MaxY);

			for (int32 Y = MinY; Y <= MaxY; ++Y)
			{
				for (int32 X = MinX; X <= MaxX; ++X)
				{
					FVector CellWorldCenter = GridToWorld(X, Y) + FVector(CellSize * 0.5f, CellSize * 0.5f, 0.f);
					if (FVector::DistSquaredXY(RevealerLocation, CellWorldCenter) <= VisionRadiusSq)
					{
						UpdateCellState(X, Y, EFogState::Visible);
					}
				}
			}
		}
	}
}

void ASimpleFogOfWarManager::UpdateCellState(int32 X, int32 Y, EFogState NewState)
{
	if (X >= 0 && X < FogGridWidth && Y >= 0 && Y < FogGridHeight)
	{
		int32 Index = Y * FogGridWidth + X;
		// Prevent going from Visible/Explored back to Hidden unless explicitly designed
		if (NewState == EFogState::Hidden && FogGrid[Index] != EFogState::Hidden)
		{
			// Generally, we don't want to "re-hide" explored areas in a simple model
			// Unless you want a dynamic fog that can re-cover areas.
			return;
		}
		FogGrid[Index] = NewState;
	}
}

EFogState ASimpleFogOfWarManager::GetCellState(int32 X, int32 Y) const
{
	if (X >= 0 && X < FogGridWidth && Y >= 0 && Y < FogGridHeight)
	{
		return FogGrid[Y * FogGridWidth + X];
	}
	return EFogState::Hidden; // Or some other default for out-of-bounds
}


EFogState ASimpleFogOfWarManager::GetFogStateAtWorldLocation(const FVector& WorldLocation) const
{
	int32 X, Y;
	if (WorldToGrid(WorldLocation, X, Y))
	{
		return GetCellState(X, Y);
	}
	return EFogState::Hidden; // Outside defined fog area
}

bool ASimpleFogOfWarManager::WorldToGrid(const FVector& WorldLocation, int32& OutX, int32& OutY) const
{
	FVector LocalPos = WorldLocation - FogOrigin;
	OutX = FMath::FloorToInt(LocalPos.X / CellSize);
	OutY = FMath::FloorToInt(LocalPos.Y / CellSize);

	return (OutX >= 0 && OutX < FogGridWidth && OutY >= 0 && OutY < FogGridHeight);
}

FVector ASimpleFogOfWarManager::GridToWorld(int32 X, int32 Y) const
{
	return FogOrigin + FVector(X * CellSize, Y * CellSize, FogOrigin.Z);
}


void ASimpleFogOfWarManager::DrawDebugFog() const
{
	if (!GetWorld()) return;

	for (int32 Y = 0; Y < FogGridHeight; ++Y)
	{
		for (int32 X = 0; X < FogGridWidth; ++X)
		{
			FVector CellWorldPos = GridToWorld(X, Y);
			FVector BoxExtent(CellSize * 0.5f, CellSize * 0.5f, 10.0f); // Small Z extent for visibility
			FVector BoxCenter = CellWorldPos + BoxExtent;
			BoxCenter.Z = FogOrigin.Z + 10.0f; // Adjust Z for better debug visibility

			FColor DebugColor;
			switch (GetCellState(X,Y))
			{
			case EFogState::Hidden:
				DebugColor = FColor(0, 0, 0, 150); // Dark semi-transparent
				break;
			case EFogState::Explored:
				DebugColor = FColor(100, 100, 100, 100); // Greyish semi-transparent
				break;
			case EFogState::Visible:
				DebugColor = FColor(0, 255, 0, 50); // Greenish, more transparent (or no box)
				// For visible, you might choose not to draw a box, or make it very light
				DrawDebugBox(GetWorld(), BoxCenter, BoxExtent, FColor::Transparent, false, -1.f, 0, 1.f); // Effectively clear
				continue; // Skip drawing a colored box for visible if you prefer
			default:
				DebugColor = FColor::Magenta;
				break;
			}
			DrawDebugSolidBox(GetWorld(), BoxCenter, BoxExtent, DebugColor, false, -1.f);
		}
	}
}