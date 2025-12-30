// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Actors/MinimapActor.h"

#include "Characters/Unit/UnitBase.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Net/UnrealNetwork.h"
#include "Actors/MapSwitchActor.h"
#include "EngineUtils.h"

// Sets default values
AMinimapActor::AMinimapActor()
{
    PrimaryActorTick.bCanEverTick = false; // No ticking needed, updates are event-driven.
    bReplicates = true;
    //SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent")));

    MapBoundsComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("MapBoundsComponent"));
    SetRootComponent(MapBoundsComponent);
    MapBoundsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);


    
    // --- NEU: Scene Capture Komponente erstellen ---
    SceneCaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCaptureComponent"));
    SceneCaptureComponent->SetupAttachment(RootComponent);

    // Wichtige Standardeinstellungen für eine saubere Top-Down-Aufnahme
    SceneCaptureComponent->ProjectionType = ECameraProjectionMode::Orthographic; // Perfekte Top-Down-Sicht ohne Perspektive
    SceneCaptureComponent->CaptureSource = ESceneCaptureSource::SCS_BaseColor;   // Nur die "Unlit"-Farben der Karte, ohne Licht/Schatten
    SceneCaptureComponent->bCaptureEveryFrame = false; // SEHR WICHTIG für die Performance!
    SceneCaptureComponent->bCaptureOnMovement = false; // Wir wollen nur einmal am Anfang aufnehmen.

    bReplicates = false;
    SetNetUpdateFrequency(1);
    SetMinNetUpdateFrequency(1);
    SetReplicates(false);
}

void AMinimapActor::BeginPlay()
{
    Super::BeginPlay();

    //MinimapMinBounds = FVector2D(-Size*40.f, -Size*40.f);
    //MinimapMaxBounds = FVector2D(Size*40.f, Size*40.f);

    if (MapBoundsComponent)
    {
        const FVector Origin = MapBoundsComponent->GetComponentLocation();
        const FVector Extent = MapBoundsComponent->GetScaledBoxExtent();

        MinimapMinBounds = FVector2D(Origin.X - Extent.X, Origin.Y - Extent.Y);
        MinimapMaxBounds = FVector2D(Origin.X + Extent.X, Origin.Y + Extent.Y);
    }

    
    InitMinimapTexture();
    CaptureMapTopography();
}


void AMinimapActor::CaptureMapTopography()
{
    if (!SceneCaptureComponent) return;

    // 1. Berechne die Position und Größe der Aufnahme basierend auf den Bounds
    const FVector CenterLocation(
        (MinimapMinBounds.X + MinimapMaxBounds.X) * 0.5f,
        (MinimapMinBounds.Y + MinimapMaxBounds.Y) * 0.5f,
        10000.f // Eine hohe Z-Koordinate, um sicher über allem zu sein
    );

    // Die Kamera muss direkt nach unten schauen (-90 Grad Pitch)
    const FRotator CaptureRotation = FRotator(-90.f, -90.f, 0.f);

    // Die Breite der orthographischen Aufnahme muss den gesamten Bereich abdecken
    const float OrthoWidth = FMath::Max(MinimapMaxBounds.X - MinimapMinBounds.X, MinimapMaxBounds.Y - MinimapMinBounds.Y);

    SceneCaptureComponent->SetWorldLocationAndRotation(CenterLocation, CaptureRotation);
    SceneCaptureComponent->OrthoWidth = OrthoWidth;

    TArray<AActor*> ActorsToHide;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AUnitBase::StaticClass(), ActorsToHide);
    SceneCaptureComponent->HiddenActors.Append(ActorsToHide);
    
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APickup::StaticClass(), ActorsToHide);
    SceneCaptureComponent->HiddenActors.Append(ActorsToHide);
    // 2. Erstelle das Render Target (die "Leinwand" für unser Foto)
    TopographyRenderTarget = NewObject<UTextureRenderTarget2D>(this);
    if (TopographyRenderTarget)
    {
        // Initialisiere die Textur mit der gleichen Größe wie unsere Nebel-Textur
        TopographyRenderTarget->InitCustomFormat(MinimapTexSize, MinimapTexSize, PF_B8G8R8A8, true); // true für lineares Gamma
        TopographyRenderTarget->UpdateResource();
        
        // 3. Weise das Render Target der Capture-Komponente zu
        SceneCaptureComponent->TextureTarget = TopographyRenderTarget;

        // 4. Mache die Aufnahme!
        SceneCaptureComponent->CaptureScene();

        UE_LOG(LogTemp, Log, TEXT("Minimap topography successfully captured."));
    }
}

void AMinimapActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AMinimapActor, TeamId);
    DOREPLIFETIME(AMinimapActor, MinimapMinBounds);
    DOREPLIFETIME(AMinimapActor, MinimapMaxBounds);
    
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
    const TArray<AUnitBase*>& UnitRefs, // NEUER PARAMETER
    const TArray<FVector_NetQuantize>& Positions,
    const TArray<float>& UnitRadii,
    const TArray<float>& FogRadii,
    const TArray<uint8>& UnitTeamIds)
{
    if (!MinimapTexture || MinimapPixels.Num() == 0) return;

    // --- Pass 1: Clear the entire map with the Fog Color ---
    for (FColor& Pixel : MinimapPixels)
    {
        Pixel = FogColor;
    }

    // --- Pass 2: Reveal the fog for friendly units ---
    const float WorldExtentX = MinimapMaxBounds.X - MinimapMinBounds.X;
    const float WorldExtentY = MinimapMaxBounds.Y - MinimapMinBounds.Y;
    if (WorldExtentX <= 0 || WorldExtentY <= 0) return;
    const int32 Count = FMath::Min(UnitRefs.Num(), Positions.Num());

    for (int32 i = 0; i < Count; ++i)
    {
        if (UnitTeamIds[i] != TeamId) continue;
        const FVector WorldPos = Positions[i];
        const float U = (WorldPos.X - MinimapMinBounds.X) / WorldExtentX;
        const float V = (WorldPos.Y - MinimapMinBounds.Y) / WorldExtentY;
        const int32 CenterX = FMath::RoundToInt(U * MinimapTexSize);
        const int32 CenterY = FMath::RoundToInt(V * MinimapTexSize);
        const float NormalizedRadius = FogRadii[i] / WorldExtentX;
        const int32 PixelRadius = FMath::RoundToInt(NormalizedRadius * MinimapTexSize);
        DrawFilledCircle(MinimapPixels, MinimapTexSize, CenterX, CenterY, PixelRadius, BackgroundColor);
    }
    
    // --- Pass 3: Draw the units on top (MIT NEUER LOGIK) ---
    for (int32 i = 0; i < Count; ++i)
    {
        bool bShouldDrawUnit = false;
        
        if (UnitTeamIds[i] == TeamId)
        {
            bShouldDrawUnit = true;
        }
        else
        {
            if (UnitRefs[i] && UnitRefs[i]->IsVisibleEnemy)
            {
                bShouldDrawUnit = true;
            }
        }
        
        if (bShouldDrawUnit)
        {
            const FVector WorldPos = Positions[i];
            const float U = (WorldPos.X - MinimapMinBounds.X) / WorldExtentX;
            const float V = (WorldPos.Y - MinimapMinBounds.Y) / WorldExtentY;
            const int32 CenterX = FMath::RoundToInt(U * MinimapTexSize);
            const int32 CenterY = FMath::RoundToInt(V * MinimapTexSize);
            const float NormalizedRadius = UnitRadii[i] / WorldExtentX;
            const int32 PixelRadius = FMath::Max(1, FMath::RoundToInt(NormalizedRadius * MinimapTexSize * DotMultiplier));
            const FColor& UnitColor = (UnitTeamIds[i] == TeamId) ? FriendlyUnitColor : EnemyUnitColor;

            DrawFilledCircle(MinimapPixels, MinimapTexSize, CenterX, CenterY, PixelRadius, UnitColor);
        }
    }

    // --- Pass 4: Draw MapSwitchActors if enabled ---
    if (bLiveUpdateMapSwitcher)
    {
        for (TActorIterator<AMapSwitchActor> It(GetWorld()); It; ++It)
        {
            AMapSwitchActor* SwitchActor = *It;
            if (SwitchActor)
            {
                const FVector WorldPos = SwitchActor->GetActorLocation();
                const float U = (WorldPos.X - MinimapMinBounds.X) / WorldExtentX;
                const float V = (WorldPos.Y - MinimapMinBounds.Y) / WorldExtentY;
                const int32 CenterX = FMath::RoundToInt(U * MinimapTexSize);
                const int32 CenterY = FMath::RoundToInt(V * MinimapTexSize);

                const float NormalizedRadius = 45.f / WorldExtentX;
                const int32 PixelRadius = FMath::Max(2, FMath::RoundToInt(NormalizedRadius * MinimapTexSize * DotMultiplier));

                DrawFilledCircle(MinimapPixels, MinimapTexSize, CenterX, CenterY, PixelRadius, MapSwitcherColor);
            }
        }
    }

    // --- Upload updated pixels to the GPU texture ---
    FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, MinimapTexSize, MinimapTexSize);
    MinimapTexture->UpdateTextureRegions(0, 1, Region, MinimapTexSize * sizeof(FColor), sizeof(FColor), reinterpret_cast<uint8*>(MinimapPixels.GetData()));
}

void AMinimapActor::UpdateMinimap_Local(
    const TArray<AUnitBase*>& UnitRefs,
    const TArray<FVector_NetQuantize>& Positions,
    const TArray<float>& UnitRadii,
    const TArray<float>& FogRadii,
    const TArray<uint8>& UnitTeamIds)
{
    // Simply reuse the same logic as the multicast implementation
    Multicast_UpdateMinimap_Implementation(UnitRefs, Positions, UnitRadii, FogRadii, UnitTeamIds);
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