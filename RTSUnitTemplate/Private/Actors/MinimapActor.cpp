// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Actors/MinimapActor.h"
#include "Engine/Texture2D.h"
#include "Net/UnrealNetwork.h"

// Sets default values
AMinimapActor::AMinimapActor()
{
    PrimaryActorTick.bCanEverTick = false; // No ticking needed, updates are event-driven.
    bReplicates = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent")));
}

void AMinimapActor::BeginPlay()
{
    Super::BeginPlay();
    InitMinimapTexture();
}

void AMinimapActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AMinimapActor, TeamId);
}

void AMinimapActor::InitMinimapTexture()
{
    // Create the transient texture that will be updated and displayed in UMG.
    MinimapTexture = UTexture2D::CreateTransient(MinimapTexSize, MinimapTexSize, PF_B8G8R8A8);
    check(MinimapTexture);

    MinimapTexture->SRGB = false; // Use linear color space for accurate color representation.
    MinimapTexture->CompressionSettings = TC_VectorDisplacementmap;
    MinimapTexture->AddToRoot(); // Prevent garbage collection.
    MinimapTexture->UpdateResource();

    // Initialize the pixel buffer.
    MinimapPixels.SetNumUninitialized(MinimapTexSize * MinimapTexSize);
}

void AMinimapActor::Multicast_UpdateMinimap_Implementation(
    const TArray<FVector_NetQuantize>& Positions,
    const TArray<float>& UnitRadii,
    const TArray<float>& FogRadii,
    const TArray<uint8>& UnitTeamIds)
{
    if (!MinimapTexture || MinimapPixels.Num() == 0)
    {
        return;
    }

    // --- Precompute for coordinate conversion ---
    const float WorldExtentX = MinimapMaxBounds.X - MinimapMinBounds.X;
    const float WorldExtentY = MinimapMaxBounds.Y - MinimapMinBounds.Y;

    if (WorldExtentX <= 0 || WorldExtentY <= 0) return;

    // --- Pass 1: Clear the entire map with the Fog Color ---
    for (FColor& Pixel : MinimapPixels)
    {
        Pixel = FogColor;
    }

    const int32 Count = FMath::Min3(Positions.Num(), FogRadii.Num(), UnitTeamIds.Num());

    // --- Pass 2: Reveal the fog for friendly units ---
    // We "punch holes" in the fog by drawing the background color where there is vision.
    for (int32 i = 0; i < Count; ++i)
    {
        // Only reveal fog for units on our team.
        if (UnitTeamIds[i] != TeamId)
        {
            continue;
        }

        const FVector WorldPos = Positions[i];
        const float U = (WorldPos.X - MinimapMinBounds.X) / WorldExtentX;
        const float V = (WorldPos.Y - MinimapMinBounds.Y) / WorldExtentY;
        const int32 CenterX = FMath::RoundToInt(U * MinimapTexSize);
        const int32 CenterY = FMath::RoundToInt(V * MinimapTexSize);
        
        const float NormalizedRadius = FogRadii[i] / WorldExtentX;
        const int32 PixelRadius = FMath::RoundToInt(NormalizedRadius * MinimapTexSize);

        // Draw a circle with the background color to reveal this area.
        DrawFilledCircle(MinimapPixels, MinimapTexSize, CenterX, CenterY, PixelRadius, BackgroundColor);
    }
    
    // --- Pass 3: Draw the units on top ---
    const int32 UnitCount = FMath::Min3(Positions.Num(), UnitRadii.Num(), UnitTeamIds.Num());
    for (int32 i = 0; i < UnitCount; ++i)
    {
        // Note: A full implementation would check if the enemy is actually visible (i.e., inside a revealed fog area).
        // For this example, we assume any enemy passed in this array is visible to the player.

        const FVector WorldPos = Positions[i];
        const float U = (WorldPos.X - MinimapMinBounds.X) / WorldExtentX;
        const float V = (WorldPos.Y - MinimapMinBounds.Y) / WorldExtentY;
        const int32 CenterX = FMath::RoundToInt(U * MinimapTexSize);
        const int32 CenterY = FMath::RoundToInt(V * MinimapTexSize);

        const float NormalizedRadius = UnitRadii[i] / WorldExtentX;
        const int32 PixelRadius = FMath::Max(1, FMath::RoundToInt(NormalizedRadius * MinimapTexSize));

        const FColor& UnitColor = (UnitTeamIds[i] == TeamId) ? FriendlyUnitColor : EnemyUnitColor;

        DrawFilledCircle(MinimapPixels, MinimapTexSize, CenterX, CenterY, PixelRadius, UnitColor);
    }

    // --- Finally, upload the updated pixel data to the GPU texture ---
    FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, MinimapTexSize, MinimapTexSize);
    MinimapTexture->UpdateTextureRegions(
        0, 1, Region,
        MinimapTexSize * sizeof(FColor),
        sizeof(FColor),
        reinterpret_cast<uint8*>(MinimapPixels.GetData())
    );
}

void AMinimapActor::DrawFilledCircle(TArray<FColor>& Pixels, int32 TexSize, int32 CenterX, int32 CenterY, int32 Radius, const FColor& Color)
{
    if (Radius <= 0) return;

    const int32 RadiusSq = Radius * Radius;

    // Define the bounding box of the circle
    const int32 MinX = FMath::Max(0, CenterX - Radius);
    const int32 MaxX = FMath::Min(TexSize - 1, CenterX + Radius);
    const int32 MinY = FMath::Max(0, CenterY - Radius);
    const int32 MaxY = FMath::Min(TexSize - 1, CenterY + Radius);

    for (int32 Y = MinY; Y <= MaxY; ++Y)
    {
        FColor* Row = Pixels.GetData() + Y * TexSize;
        for (int32 X = MinX; X <= MaxX; ++X)
        {
            const int32 dx = X - CenterX;
            const int32 dy = Y - CenterY;
            if (dx * dx + dy * dy <= RadiusSq)
            {
                Row[X] = Color;
            }
        }
    }
}