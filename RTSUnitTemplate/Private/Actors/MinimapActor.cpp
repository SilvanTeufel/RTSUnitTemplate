// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Actors/MinimapActor.h"

#include "Characters/Unit/UnitBase.h"
#include "Engine/Texture2D.h"
#include "Net/UnrealNetwork.h"
#include "Actors/MapSwitchActor.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Materials/MaterialInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "LandscapeProxy.h"
#include "Actors/WorkArea.h"
#include "Actors/Pickup.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
AMinimapActor::AMinimapActor()
{
    PrimaryActorTick.bCanEverTick = false; // No ticking needed, updates are event-driven.
    bReplicates = true;
    bAlwaysRelevant = true;

    MapBoundsComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("MapBoundsComponent"));
    SetRootComponent(MapBoundsComponent);
    MapBoundsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MapBoundsComponent->SetBoxExtent(FVector(15000.f, 15000.f, 1000.f));

    // Kein SceneCaptureComponent mehr nötig — Topographie wird per LineTrace erzeugt.

    SetNetUpdateFrequency(10);
    SetMinNetUpdateFrequency(10);

    TagConfigs.Add({FName(TEXT("ColorA")), FLinearColor(0.8f, 0.2f, 0.2f), 1.0f});
    TagConfigs.Add({FName(TEXT("ColorB")), FLinearColor(0.2f, 0.6f, 0.8f), 1.0f});
    TagConfigs.Add({FName(TEXT("ColorC")), FLinearColor(0.8f, 0.7f, 0.1f), 1.0f});
    TagConfigs.Add({FName(TEXT("ColorD")), FLinearColor(0.6f, 0.2f, 0.8f), 1.0f});
    TagConfigs.Add({FName(TEXT("ColorE")), FLinearColor(1.0f, 0.5f, 0.0f), 1.0f});
    TagConfigs.Add({FName(TEXT("ColorF")), FLinearColor(0.0f, 1.0f, 1.0f), 1.0f});
    TagConfigs.Add({FName(TEXT("ColorG")), FLinearColor(0.5f, 1.0f, 0.0f), 1.0f});
    TagConfigs.Add({FName(TEXT("ColorH")), FLinearColor(1.0f, 0.0f, 1.0f), 1.0f});

    MaxHeightLimit = 1000.f;
    MinHeightLimit = -500.f;
}

void AMinimapActor::BeginPlay()
{
    Super::BeginPlay();

    if (MapBoundsComponent)
    {
        const FVector Origin = MapBoundsComponent->GetComponentLocation();
        const FVector Extent = MapBoundsComponent->GetScaledBoxExtent();

        MinimapMinBounds = FVector2D(Origin.X - Extent.X, Origin.Y - Extent.Y);
        MinimapMaxBounds = FVector2D(Origin.X + Extent.X, Origin.Y + Extent.Y);
    }

    InitMinimapTexture();
    
    if (DelayTime > 0.f)
    {
        GetWorldTimerManager().SetTimer(CaptureTimerHandle, this, &AMinimapActor::CaptureMapTopography, DelayTime, false);
    }
    else
    {
        CaptureMapTopography();
    }
}

bool AMinimapActor::TryGetMaterialColor(const FHitResult& Hit, FLinearColor& OutColor) const
{
    UPrimitiveComponent* HitComp = Hit.GetComponent();
    if (!HitComp) return false;

    // --- Strategy 0 (highest priority): Check actor tags for dynamic TagConfigs ---
    AActor* TagActor = Hit.GetActor();
    if (TagActor)
    {
        for (const FMinimapTagConfig& Config : TagConfigs)
        {
            if (TagActor->Tags.Contains(Config.Tag))
            {
                OutColor = Config.Color;
                return true;
            }
        }
    }

    // --- Strategy 1: Try to read a vector parameter from the material ---
    UMaterialInterface* Material = HitComp->GetMaterial(Hit.ElementIndex);
    if (Material)
    {
        FLinearColor ParamColor;
        static const FName BaseColorNames[] = {
            FName(TEXT("BaseColor")),
            FName(TEXT("Base Color")),
            FName(TEXT("Color")),
            FName(TEXT("DiffuseColor")),
            FName(TEXT("Diffuse Color")),
            FName(TEXT("Albedo")),
        };

        for (const FName& ParamName : BaseColorNames)
        {
            if (Material->GetVectorParameterValue(FHashedMaterialParameterInfo(ParamName), ParamColor))
            {
                OutColor = ParamColor;
                return true;
            }
        }
    }

    // --- Strategy 2: Map Physical Material SurfaceType to a color ---
    UPhysicalMaterial* PhysMat = Hit.PhysMaterial.Get();
    if (PhysMat && PhysMat->SurfaceType != EPhysicalSurface::SurfaceType_Default)
    {
        switch (PhysMat->SurfaceType)
        {
        case EPhysicalSurface::SurfaceType1:  // Often: Grass / Vegetation
            OutColor = FLinearColor(0.15f, 0.4f, 0.08f);
            return true;
        case EPhysicalSurface::SurfaceType2:  // Often: Dirt / Earth
            OutColor = FLinearColor(0.35f, 0.25f, 0.12f);
            return true;
        case EPhysicalSurface::SurfaceType3:  // Often: Rock / Stone
            OutColor = FLinearColor(0.4f, 0.38f, 0.35f);
            return true;
        case EPhysicalSurface::SurfaceType4:  // Often: Sand
            OutColor = FLinearColor(0.6f, 0.55f, 0.35f);
            return true;
        case EPhysicalSurface::SurfaceType5:  // Often: Water
            OutColor = FLinearColor(0.1f, 0.2f, 0.5f);
            return true;
        case EPhysicalSurface::SurfaceType6:  // Often: Snow / Ice
            OutColor = FLinearColor(0.85f, 0.88f, 0.9f);
            return true;
        case EPhysicalSurface::SurfaceType7:  // Often: Metal
            OutColor = FLinearColor(0.5f, 0.5f, 0.55f);
            return true;
        case EPhysicalSurface::SurfaceType8:  // Often: Wood
            OutColor = FLinearColor(0.4f, 0.3f, 0.15f);
            return true;
        default:
            break;
        }
    }

    // --- Strategy 3: Color based on Actor type ---
    AActor* HitActor = Hit.GetActor();
    if (HitActor)
    {
        if (HitActor->IsA<ALandscapeProxy>())
        {
            OutColor = LandscapeColor;
            return true;
        }
        if (HitActor->GetClass()->GetName().Contains(TEXT("Foliage")))
        {
            OutColor = FoliageColor;
            return true;
        }
        if (HitComp->IsA<UStaticMeshComponent>())
        {
            OutColor = StaticMeshColor;
            return true;
        }
        OutColor = DefaultActorColor;
        return true;
    }

    return false;
}

void AMinimapActor::CaptureMapTopography()
{
    UWorld* World = GetWorld();
    if (!World) return;

    const int32 TexSize = MinimapTexSize;
    const float WorldMinX = MinimapMinBounds.X;
    const float WorldMinY = MinimapMinBounds.Y;
    const float WorldExtentX = MinimapMaxBounds.X - MinimapMinBounds.X;
    const float WorldExtentY = MinimapMaxBounds.Y - MinimapMinBounds.Y;

    if (WorldExtentX <= 0.f || WorldExtentY <= 0.f) return;

    // --- Pass 1: LineTraces über das gesamte Raster, Höhen und Farben sammeln ---
    TArray<float> Heights;
    Heights.SetNumUninitialized(TexSize * TexSize);
    
    TArray<FLinearColor> MaterialColors;
    MaterialColors.SetNumUninitialized(TexSize * TexSize);
    
    TArray<bool> HasMaterialColor;
    HasMaterialColor.SetNumZeroed(TexSize * TexSize);
    
    TArray<bool> IsLandscapePixel;
    IsLandscapePixel.SetNumZeroed(TexSize * TexSize);
    
    float MinHeight = MAX_FLT;
    float MaxHeight = -MAX_FLT;

    FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(MinimapTopo), true); // true = trace complex
    TraceParams.bReturnPhysicalMaterial = true;

    // --- Actors ignorieren (analog zu den alten SceneCapture HiddenActors) ---
    {
        TArray<AActor*> ActorsToIgnore;

        // Units ignorieren (AUnitBase)
        UGameplayStatics::GetAllActorsOfClass(World, AUnitBase::StaticClass(), ActorsToIgnore);
        TraceParams.AddIgnoredActors(ActorsToIgnore);

        // Visual ISM Manager der Units ignorieren
        ActorsToIgnore.Reset();
        UGameplayStatics::GetAllActorsWithTag(World, FName(TEXT("UnitVisualISMManager")), ActorsToIgnore);
        TraceParams.AddIgnoredActors(ActorsToIgnore);

        // Pickups ignorieren (APickup)
        ActorsToIgnore.Reset();
        UGameplayStatics::GetAllActorsOfClass(World, APickup::StaticClass(), ActorsToIgnore);
        TraceParams.AddIgnoredActors(ActorsToIgnore);

        // MapSwitchActors ignorieren (optional, wie im alten Code)
        if (bLiveUpdateMapSwitcher)
        {
            ActorsToIgnore.Reset();
            UGameplayStatics::GetAllActorsOfClass(World, AMapSwitchActor::StaticClass(), ActorsToIgnore);
            TraceParams.AddIgnoredActors(ActorsToIgnore);
        }

        // WorkAreas ignorieren (werden separat in Pass 2.5 als Marker gezeichnet)
        ActorsToIgnore.Reset();
        UGameplayStatics::GetAllActorsOfClass(World, AWorkArea::StaticClass(), ActorsToIgnore);
        TraceParams.AddIgnoredActors(ActorsToIgnore);

        // Editierbare Ignore-Liste aus dem Detail-Panel
        for (const TSubclassOf<AActor>& IgnoreClass : TopographyIgnoredActorClasses)
        {
            if (!IgnoreClass) continue;
            ActorsToIgnore.Reset();
            UGameplayStatics::GetAllActorsOfClass(World, IgnoreClass, ActorsToIgnore);
            TraceParams.AddIgnoredActors(ActorsToIgnore);
        }
    }

    for (int32 Y = 0; Y < TexSize; ++Y)
    {
        for (int32 X = 0; X < TexSize; ++X)
        {
            // UV → World Position
            const float U = (static_cast<float>(X) + 0.5f) / static_cast<float>(TexSize);
            const float V = (static_cast<float>(Y) + 0.5f) / static_cast<float>(TexSize);
            const float WorldX = WorldMinX + U * WorldExtentX;
            const float WorldY = WorldMinY + V * WorldExtentY;

            const FVector TraceStart(WorldX, WorldY, TraceHeightStart);
            const FVector TraceEnd(WorldX, WorldY, TraceHeightEnd);

            FHitResult Hit;
            const int32 PixelIndex = Y * TexSize + X;
            float HitZ = TraceHeightEnd;

            if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, TraceParams))
            {
                HitZ = Hit.ImpactPoint.Z;
                
                // Track if this pixel hit a landscape
                AActor* HitActor = Hit.GetActor();
                if (HitActor && HitActor->IsA<ALandscapeProxy>())
                {
                    IsLandscapePixel[PixelIndex] = true;
                }
                
                // Try to extract material color
                FLinearColor MatColor;
                if (TryGetMaterialColor(Hit, MatColor))
                {
                    MaterialColors[PixelIndex] = MatColor;
                    HasMaterialColor[PixelIndex] = true;
                }
            }

            Heights[PixelIndex] = HitZ;
            MinHeight = FMath::Min(MinHeight, HitZ);
            MaxHeight = FMath::Max(MaxHeight, HitZ);
        }
    }

    // --- Pass 2: Höhen normalisieren und in Farben umwandeln ---
    TArray<FColor> Pixels;
    Pixels.SetNumUninitialized(TexSize * TexSize);

    const float HeightRange = MaxHeightLimit - MinHeightLimit;
    const float InvHeightRange = (HeightRange > KINDA_SMALL_NUMBER) ? (1.0f / HeightRange) : 0.0f;

    for (int32 i = 0; i < TexSize * TexSize; ++i)
    {
        const float NormalizedHeight = FMath::Clamp((Heights[i] - MinHeightLimit) * InvHeightRange, 0.0f, 1.0f);
        
        // Height-Gradient: Always interpolate between TopoLowColor and TopoHighColor
        const FLinearColor HeightGradient = FLinearColor(
            FMath::Lerp(static_cast<float>(TopoLowColor.R), static_cast<float>(TopoHighColor.R), NormalizedHeight) / 255.f,
            FMath::Lerp(static_cast<float>(TopoLowColor.G), static_cast<float>(TopoHighColor.G), NormalizedHeight) / 255.f,
            FMath::Lerp(static_cast<float>(TopoLowColor.B), static_cast<float>(TopoHighColor.B), NormalizedHeight) / 255.f,
            1.0f
        );
        
        FLinearColor FinalColor;
        
        if (HasMaterialColor[i])
        {
            // Mix material color with height gradient
            // HeightShadingStrength = 0: Pure material color
            // HeightShadingStrength = 1: Pure height gradient (TopoLowColor -> TopoHighColor)
            const FLinearColor& MatColor = MaterialColors[i];
            FinalColor = FLinearColor(
                FMath::Lerp(MatColor.R, HeightGradient.R, HeightShadingStrength),
                FMath::Lerp(MatColor.G, HeightGradient.G, HeightShadingStrength),
                FMath::Lerp(MatColor.B, HeightGradient.B, HeightShadingStrength),
                1.0f
            );
        }
        else
        {
            // No material hit: Pure height gradient
            FinalColor = HeightGradient;
        }

        // Apply landscape gravel/rubble pattern (only on landscape pixels, varies by height)
        if (bDrawLandscapePattern && IsLandscapePixel[i])
        {
            const int32 PX = i % TexSize;
            const int32 PY = i / TexSize;

            // Hash-based value noise for organic, non-repetitive gravel look.
            // Integer hash function (Robert Jenkins' 32-bit mix)
            auto IntHash = [](int32 K) -> uint32
            {
                uint32 H = static_cast<uint32>(K);
                H = ((H >> 16) ^ H) * 0x45d9f3b;
                H = ((H >> 16) ^ H) * 0x45d9f3b;
                H = (H >> 16) ^ H;
                return H;
            };

            // Returns pseudo-random float in [-1, 1] for integer grid coordinates
            auto HashNoise = [&IntHash](int32 IX, int32 IY) -> float
            {
                const uint32 H = IntHash(IX * 15731 + IY * 789221 + 1376312589);
                return (static_cast<float>(H & 0xFFFF) / 32767.5f) - 1.0f;
            };

            // Smooth value noise with bilinear interpolation
            auto ValueNoise = [&HashNoise](float FX, float FY) -> float
            {
                const int32 IX = FMath::FloorToInt(FX);
                const int32 IY = FMath::FloorToInt(FY);
                const float TX = FX - static_cast<float>(IX);
                const float TY = FY - static_cast<float>(IY);
                // Smoothstep for less blocky interpolation
                const float SX = TX * TX * (3.0f - 2.0f * TX);
                const float SY = TY * TY * (3.0f - 2.0f * TY);
                const float N00 = HashNoise(IX, IY);
                const float N10 = HashNoise(IX + 1, IY);
                const float N01 = HashNoise(IX, IY + 1);
                const float N11 = HashNoise(IX + 1, IY + 1);
                const float NX0 = FMath::Lerp(N00, N10, SX);
                const float NX1 = FMath::Lerp(N01, N11, SX);
                return FMath::Lerp(NX0, NX1, SY);
            };

            const float Scale = FMath::Max(LandscapePatternScale, 0.1f);
            const float BaseX = static_cast<float>(PX) / Scale;
            const float BaseY = static_cast<float>(PY) / Scale;
            // Height shifts the noise domain so pattern changes with elevation
            const float HeightOffset = NormalizedHeight * 17.31f;

            // 4 octaves of value noise (fBm) — large shapes + fine gravel detail
            float NoiseAccum = 0.0f;
            float Amplitude = 0.50f;
            float Frequency = 1.0f;
            for (int32 Oct = 0; Oct < 4; ++Oct)
            {
                NoiseAccum += ValueNoise(BaseX * Frequency + HeightOffset, BaseY * Frequency + static_cast<float>(Oct) * 43.17f) * Amplitude;
                Amplitude *= 0.45f;
                Frequency *= 2.3f;
            }

            // Add sparse bright specks (gravel highlights)
            const float Speck = ValueNoise(BaseX * 7.9f + 100.0f, BaseY * 7.9f + 200.0f + HeightOffset);
            if (Speck > 0.7f)
            {
                NoiseAccum += (Speck - 0.7f) * 1.5f; // Occasional bright pebble
            }

            const float NoiseValue = NoiseAccum * LandscapePatternStrength;
            FinalColor.R = FMath::Clamp(FinalColor.R + NoiseValue, 0.f, 1.f);
            FinalColor.G = FMath::Clamp(FinalColor.G + NoiseValue, 0.f, 1.f);
            FinalColor.B = FMath::Clamp(FinalColor.B + NoiseValue, 0.f, 1.f);
        }

        // Apply brightness and contrast
        FinalColor.R = FMath::Clamp((FinalColor.R - 0.5f) * FMath::Max(MinimapContrast, 0.01f) + 0.5f + MinimapBrightness, 0.f, 1.f);
        FinalColor.G = FMath::Clamp((FinalColor.G - 0.5f) * FMath::Max(MinimapContrast, 0.01f) + 0.5f + MinimapBrightness, 0.f, 1.f);
        FinalColor.B = FMath::Clamp((FinalColor.B - 0.5f) * FMath::Max(MinimapContrast, 0.01f) + 0.5f + MinimapBrightness, 0.f, 1.f);

        Pixels[i] = FinalColor.ToFColor(true); // true = sRGB
    }

    // --- Pass 2.25: Height-edge outlines (ramps, cliffs) ---
    if (bDrawHeightEdges)
    {
        // Work on a copy so edge detection reads original pixels, not already-modified ones
        TArray<FColor> EdgeOverlay;
        EdgeOverlay.SetNumZeroed(TexSize * TexSize);

        for (int32 Y = 1; Y < TexSize - 1; ++Y)
        {
            for (int32 X = 1; X < TexSize - 1; ++X)
            {
                const int32 Idx = Y * TexSize + X;
                const float H = Heights[Idx] * InvHeightRange; // Use raw height scaled
                const float H_N = Heights[(Y - 1) * TexSize + X] * InvHeightRange;
                const float H_S = Heights[(Y + 1) * TexSize + X] * InvHeightRange;
                const float H_W = Heights[Y * TexSize + (X - 1)] * InvHeightRange;
                const float H_E = Heights[Y * TexSize + (X + 1)] * InvHeightRange;

                // Sobel-like gradient magnitude
                const float GradX = FMath::Abs(H_E - H_W);
                const float GradY = FMath::Abs(H_S - H_N);
                const float Gradient = FMath::Sqrt(GradX * GradX + GradY * GradY);

                if (Gradient > HeightEdgeThreshold)
                {
                    EdgeOverlay[Idx] = HeightEdgeColor;
                }
            }
        }

        // Apply edge overlay
        for (int32 i = 0; i < TexSize * TexSize; ++i)
        {
            if (EdgeOverlay[i].A > 0)
            {
                Pixels[i] = HeightEdgeColor;
            }
        }
    }

    // --- Pass 2.5: WorkArea-Positionen einzeichnen ---
    {
        TArray<AActor*> WorkAreaActors;
        UGameplayStatics::GetAllActorsOfClass(World, AWorkArea::StaticClass(), WorkAreaActors);

        for (AActor* Actor : WorkAreaActors)
        {
            AWorkArea* WorkArea = Cast<AWorkArea>(Actor);
            if (!WorkArea) continue;

            FColor MarkerColor;
            switch (WorkArea->Type)
            {
            case WorkAreaData::Primary:
                MarkerColor = WorkAreaPrimaryColor;
                break;
            case WorkAreaData::Secondary:
                MarkerColor = WorkAreaSecondaryColor;
                break;
            case WorkAreaData::Tertiary:
                MarkerColor = WorkAreaTertiaryColor;
                break;
            default:
                continue;
            }

            const FVector WALoc = WorkArea->GetActorLocation();
            const float U_WA = (WALoc.X - WorldMinX) / WorldExtentX;
            const float V_WA = (WALoc.Y - WorldMinY) / WorldExtentY;

            if (U_WA < 0.f || U_WA > 1.f || V_WA < 0.f || V_WA > 1.f) continue;

            const int32 CenterX = FMath::Clamp(FMath::RoundToInt(U_WA * (TexSize - 1)), 0, TexSize - 1);
            const int32 CenterY = FMath::Clamp(FMath::RoundToInt(V_WA * (TexSize - 1)), 0, TexSize - 1);

            DrawFilledCircle(Pixels, TexSize, CenterX, CenterY, WorkAreaMarkerRadius, MarkerColor);

            if (bDrawResourceOutline)
            {
                DrawCircleOutline(Pixels, TexSize, CenterX, CenterY, WorkAreaMarkerRadius, ResourceOutlineThickness, ResourceOutlineColor);
            }
        }
    }

    // --- Pass 2.6: Tagged Actor Markers (Scaling) ---
    for (const FMinimapTagConfig& Config : TagConfigs)
    {
        if (Config.Scale <= 1.0f) continue; // Only draw markers for scaled-up actors, others use line-trace coloring

        TArray<AActor*> TaggedActors;
        UGameplayStatics::GetAllActorsWithTag(World, Config.Tag, TaggedActors);

        for (AActor* Actor : TaggedActors)
        {
            if (!Actor) continue;
            
            const FVector Loc = Actor->GetActorLocation();
            const float U_Tag = (Loc.X - WorldMinX) / WorldExtentX;
            const float V_Tag = (Loc.Y - WorldMinY) / WorldExtentY;

            if (U_Tag < 0.f || U_Tag > 1.f || V_Tag < 0.f || V_Tag > 1.f) continue;

            const int32 CenterX = FMath::Clamp(FMath::RoundToInt(U_Tag * (TexSize - 1)), 0, TexSize - 1);
            const int32 CenterY = FMath::Clamp(FMath::RoundToInt(V_Tag * (TexSize - 1)), 0, TexSize - 1);
            
            // Base marker radius is 5.f
            const int32 ScaledRadius = FMath::RoundToInt(5.f * Config.Scale);

            DrawFilledCircle(Pixels, TexSize, CenterX, CenterY, ScaledRadius, Config.Color.ToFColor(true));
        }
    }

    // --- Pass 3: In UTexture2D schreiben ---
    if (!TopographyTexture)
    {
        TopographyTexture = UTexture2D::CreateTransient(TexSize, TexSize, PF_B8G8R8A8);
        TopographyTexture->SRGB = true;
        TopographyTexture->AddToRoot();
        TopographyTexture->UpdateResource();
    }

    FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, TexSize, TexSize);
    TopographyTexture->UpdateTextureRegions(
        0, 1, Region,
        TexSize * sizeof(FColor),
        sizeof(FColor),
        reinterpret_cast<uint8*>(Pixels.GetData())
    );

    UE_LOG(LogTemp, Log, TEXT("Minimap topography captured via LineTrace (%dx%d, Height: %.0f - %.0f)"),
        TexSize, TexSize, MinHeight, MaxHeight);
}

void AMinimapActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AMinimapActor, TeamId);
    DOREPLIFETIME(AMinimapActor, MinimapMinBounds);
    DOREPLIFETIME(AMinimapActor, MinimapMaxBounds);
    DOREPLIFETIME(AMinimapActor, TagConfigs);
}

void AMinimapActor::InitMinimapTexture()
{
    // Create the transient texture that will be updated and displayed in UMG.
    MinimapTexture = UTexture2D::CreateTransient(MinimapTexSize, MinimapTexSize, PF_B8G8R8A8);
    check(MinimapTexture);

    MinimapTexture->SRGB = true; // Use sRGB color space for accurate color representation in UI.
    MinimapTexture->CompressionSettings = TC_Default;
    MinimapTexture->AddToRoot(); // Prevent garbage collection.
    MinimapTexture->UpdateResource();

    // Initialize the pixel buffer.
    MinimapPixels.SetNumUninitialized(MinimapTexSize * MinimapTexSize);

    // TopographyTexture wird in CaptureMapTopography() erzeugt (kein RenderTarget mehr nötig).
}

void AMinimapActor::Multicast_UpdateMinimap_Implementation(
    const TArray<AUnitBase*>& UnitRefs, // NEUER PARAMETER
    const TArray<FVector_NetQuantize>& Positions,
    const TArray<float>& UnitRadii,
    const TArray<float>& FogRadii,
    const TArray<uint8>& UnitTeamIds)
{
    if (!MinimapTexture || MinimapPixels.Num() == 0) return;

    // --- Pass 1: Clear the entire map with the Fog Color (apply FogOpacity) ---
    FColor AdjustedFogColor = FogColor;
    AdjustedFogColor.A = FMath::Clamp(FMath::RoundToInt(FogOpacity * 255.f), 0, 255);
    for (FColor& Pixel : MinimapPixels)
    {
        Pixel = AdjustedFogColor;
    }

    // --- Pass 2: Reveal the fog for friendly units ---
    const float WorldExtentX = FMath::Max(100.0f, MinimapMaxBounds.X - MinimapMinBounds.X);
    const float WorldExtentY = FMath::Max(100.0f, MinimapMaxBounds.Y - MinimapMinBounds.Y);
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

            // Draw outline first (behind the fill) if enabled
            if (bDrawUnitOutline)
            {
                DrawCircleOutline(MinimapPixels, MinimapTexSize, CenterX, CenterY, PixelRadius, UnitOutlineThickness, UnitOutlineColor);
            }

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

                const float ActorRadius = SwitchActor->GetMinimapRadius();
                const float NormalizedRadius = ActorRadius / WorldExtentX;
                const int32 PixelRadius = FMath::Max(2, FMath::RoundToInt(NormalizedRadius * MinimapTexSize * MapSwitcherDotMultiplier));

                DrawFilledCircle(MinimapPixels, MinimapTexSize, CenterX, CenterY, PixelRadius, MapSwitcherColor);
            }
        }
    }

    // --- Pass 5: Draw the player viewport rectangle ---
    if (bDrawViewport)
    {
        APlayerController* PC = GetWorld()->GetFirstPlayerController();
        if (PC)
        {
            // Get viewport size
            int32 ViewportWidth = 0;
            int32 ViewportHeight = 0;
            PC->GetViewportSize(ViewportWidth, ViewportHeight);

            if (ViewportWidth > 0 && ViewportHeight > 0)
            {
                // Deproject the four screen corners to world positions on a horizontal plane
                auto DeprojectToWorld = [&](const FVector2D& ScreenPos, FVector& OutWorldPos) -> bool
                {
                    FVector WorldPos, WorldDir;
                    if (PC->DeprojectScreenPositionToWorld(ScreenPos.X, ScreenPos.Y, WorldPos, WorldDir))
                    {
                        // Intersect ray with Z=0 plane (ground level)
                        if (FMath::Abs(WorldDir.Z) > KINDA_SMALL_NUMBER)
                        {
                            const float T = -WorldPos.Z / WorldDir.Z;
                            if (T > 0.f)
                            {
                                OutWorldPos = WorldPos + WorldDir * T;
                                return true;
                            }
                        }
                    }
                    return false;
                };

                FVector Corners[4];
                const FVector2D ScreenCorners[4] = {
                    FVector2D(0.f, 0.f),                                                  // Top-Left
                    FVector2D(static_cast<float>(ViewportWidth), 0.f),                     // Top-Right
                    FVector2D(static_cast<float>(ViewportWidth), static_cast<float>(ViewportHeight)), // Bottom-Right
                    FVector2D(0.f, static_cast<float>(ViewportHeight))                     // Bottom-Left
                };

                bool bAllValid = true;
                for (int32 c = 0; c < 4; ++c)
                {
                    if (!DeprojectToWorld(ScreenCorners[c], Corners[c]))
                    {
                        bAllValid = false;
                        break;
                    }
                }

                if (bAllValid)
                {
                    // Convert world positions to pixel coordinates
                    auto WorldToPixel = [&](const FVector& WorldPos, int32& OutX, int32& OutY)
                    {
                        const float U = (WorldPos.X - MinimapMinBounds.X) / WorldExtentX;
                        const float V = (WorldPos.Y - MinimapMinBounds.Y) / WorldExtentY;
                        OutX = FMath::Clamp(FMath::RoundToInt(U * (MinimapTexSize - 1)), 0, MinimapTexSize - 1);
                        OutY = FMath::Clamp(FMath::RoundToInt(V * (MinimapTexSize - 1)), 0, MinimapTexSize - 1);
                    };

                    int32 PixelCorners[4][2];
                    for (int32 c = 0; c < 4; ++c)
                    {
                        WorldToPixel(Corners[c], PixelCorners[c][0], PixelCorners[c][1]);
                    }

                    // Draw lines between consecutive corners (closed quad)
                    auto DrawLine = [&](int32 X0, int32 Y0, int32 X1, int32 Y1)
                    {
                        // Bresenham line with thickness
                        const int32 DX = FMath::Abs(X1 - X0);
                        const int32 DY = FMath::Abs(Y1 - Y0);
                        const int32 SX = (X0 < X1) ? 1 : -1;
                        const int32 SY = (Y0 < Y1) ? 1 : -1;
                        int32 Err = DX - DY;
                        const int32 HalfThick = ViewportLineThickness / 2;

                        while (true)
                        {
                            // Draw a small square at each point for thickness
                            for (int32 TY = -HalfThick; TY <= HalfThick; ++TY)
                            {
                                for (int32 TX = -HalfThick; TX <= HalfThick; ++TX)
                                {
                                    const int32 PX = X0 + TX;
                                    const int32 PY = Y0 + TY;
                                    if (PX >= 0 && PX < MinimapTexSize && PY >= 0 && PY < MinimapTexSize)
                                    {
                                        MinimapPixels[PY * MinimapTexSize + PX] = ViewportColor;
                                    }
                                }
                            }

                            if (X0 == X1 && Y0 == Y1) break;
                            const int32 E2 = 2 * Err;
                            if (E2 > -DY) { Err -= DY; X0 += SX; }
                            if (E2 < DX) { Err += DX; Y0 += SY; }
                        }
                    };

                    for (int32 c = 0; c < 4; ++c)
                    {
                        const int32 Next = (c + 1) % 4;
                        DrawLine(PixelCorners[c][0], PixelCorners[c][1], PixelCorners[Next][0], PixelCorners[Next][1]);
                    }
                }
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

void AMinimapActor::DrawCircleOutline(TArray<FColor>& Pixels, int32 TexSize, int32 CenterX, int32 CenterY, int32 Radius, int32 Thickness, const FColor& Color)
{
    const int32 OuterRadius = Radius + Thickness;
    const int32 OuterRadiusSq = OuterRadius * OuterRadius;
    const int32 InnerRadiusSq = Radius * Radius;

    for (int32 DY = -OuterRadius; DY <= OuterRadius; ++DY)
    {
        for (int32 DX = -OuterRadius; DX <= OuterRadius; ++DX)
        {
            const int32 DistSq = DX * DX + DY * DY;
            if (DistSq <= OuterRadiusSq && DistSq > InnerRadiusSq)
            {
                const int32 PX = CenterX + DX;
                const int32 PY = CenterY + DY;
                if (PX >= 0 && PX < TexSize && PY >= 0 && PY < TexSize)
                {
                    Pixels[PY * TexSize + PX] = Color;
                }
            }
        }
    }
}

void AMinimapActor::DrawRectOutline(TArray<FColor>& Pixels, int32 TexSize, int32 X0, int32 Y0, int32 X1, int32 Y1, int32 Thickness, const FColor& Color)
{
    // Ensure X0 <= X1 and Y0 <= Y1
    if (X0 > X1) Swap(X0, X1);
    if (Y0 > Y1) Swap(Y0, Y1);

    for (int32 T = 0; T < Thickness; ++T)
    {
        // Top edge
        for (int32 X = X0 - T; X <= X1 + T; ++X)
        {
            const int32 Y = Y0 - T;
            if (X >= 0 && X < TexSize && Y >= 0 && Y < TexSize)
                Pixels[Y * TexSize + X] = Color;
        }
        // Bottom edge
        for (int32 X = X0 - T; X <= X1 + T; ++X)
        {
            const int32 Y = Y1 + T;
            if (X >= 0 && X < TexSize && Y >= 0 && Y < TexSize)
                Pixels[Y * TexSize + X] = Color;
        }
        // Left edge
        for (int32 Y = Y0 - T; Y <= Y1 + T; ++Y)
        {
            const int32 X = X0 - T;
            if (X >= 0 && X < TexSize && Y >= 0 && Y < TexSize)
                Pixels[Y * TexSize + X] = Color;
        }
        // Right edge
        for (int32 Y = Y0 - T; Y <= Y1 + T; ++Y)
        {
            const int32 X = X1 + T;
            if (X >= 0 && X < TexSize && Y >= 0 && Y < TexSize)
                Pixels[Y * TexSize + X] = Color;
        }
    }
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
