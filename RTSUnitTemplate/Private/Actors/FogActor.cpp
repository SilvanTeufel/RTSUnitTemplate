// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Actors/FogActor.h"

#include "MassActorSubsystem.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Controller/PlayerController/CustomControllerBase.h"

// Sets default values
AFogActor::AFogActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent")));

	// --- 1. CREATE THE COMPONENT ---
	PostProcessComponent = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcessComponent"));
	PostProcessComponent->SetupAttachment(RootComponent); // Attach it to our root

	// --- 2. SET ITS CORE PROPERTIES ---
    
	// Set a high priority to ensure it overrides other volumes in the scene.
	PostProcessComponent->Priority = 100.0f;

	// Make the volume affect the entire world, not just a box area.
	PostProcessComponent->bUnbound = true;
    
	// We initially disable it. The code will enable it for the correct player.
	PostProcessComponent->bEnabled = false; 

	FogMinBounds = FVector2D(-FogSize*50, -FogSize*50);
	FogMaxBounds = FVector2D(FogSize*50, FogSize*50);

	bReplicates = false;
	SetNetUpdateFrequency(1);
	SetMinNetUpdateFrequency(1);
	SetReplicates(false);
}

void AFogActor::BeginPlay()
{
	Super::BeginPlay();
	InitFogMaskTexture();
}

void AFogActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AFogActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFogActor, TeamId);
	//DOREPLIFETIME(AFogActor, PostProcessComponent);
}


void AFogActor::InitFogMaskTexture()
{

	//FogMesh->SetRelativeScale3D(FVector(FogSize, FogSize, 1.f));
	FogMinBounds = FVector2D(-FogSize*50, -FogSize*50);
	FogMaxBounds = FVector2D(FogSize*50, FogSize*50);
	
	FogMaskTexture = UTexture2D::CreateTransient(FogTexSize, FogTexSize, PF_B8G8R8A8);
	check(FogMaskTexture);

	FogMaskTexture->SRGB = false;
	FogMaskTexture->CompressionSettings = TC_VectorDisplacementmap;

	FogMaskTexture->AddToRoot();
	FogMaskTexture->UpdateResource();

	FogPixels.SetNumUninitialized(FogTexSize * FogTexSize);
}

void AFogActor::InitializeFogPostProcess()
{

	APlayerController* PC = GetWorld()->GetFirstPlayerController();

	if (ACustomControllerBase* CustomPC = Cast<ACustomControllerBase>(PC))
	{
		// Check if this fog actor belongs to the local player's team
		if (CustomPC->SelectableTeamId == TeamId)
		{
			if (FogMaterial && FogMaskTexture)
			{
				if (FogMaterial && FogMaskTexture && PostProcessComponent)
				{
					// Create the Dynamic Material Instance
					UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(FogMaterial, this);
					MID->SetTextureParameterValue(TEXT("FogMaskTex"), FogMaskTexture);

					FVector4 FogBounds(FogMinBounds.X, FogMinBounds.Y, FogMaxBounds.X, FogMaxBounds.Y);
					MID->SetVectorParameterValue(TEXT("FogBounds"), FogBounds);

					// --- 3. APPLY THE MATERIAL DIRECTLY TO OUR COMPONENT ---
					// This is much cleaner than finding the camera manager.
					PostProcessComponent->Settings.AddBlendable(MID, 1.0f);
					PostProcessComponent->bEnabled = true; // Enable the effect!
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("FogMaterial, Texture, or PPComponent is missing!"));
				}
			}
		}
	}
}

void AFogActor::SetFogBounds(const FVector2D& Min, const FVector2D& Max)
{
	FogMinBounds = Min;
	FogMaxBounds = Max;
}

void AFogActor::UpdateFogMaskWithCircles_Local(
    const TArray<FVector_NetQuantize>& Positions,
    const TArray<float>&              WorldRadii,
    const TArray<uint8>&              UnitTeamIds)
{
    if (!FogMaskTexture)
    {
        return;
    }

    // 1) Clear mask to black
    for (FColor& Pixel : FogPixels)
    {
        Pixel = FColor::Black;
    }

    // 2) Precompute for pixel conversion
    const float WorldExtentX = FogMaxBounds.X - FogMinBounds.X;
    const float WorldExtentY = FogMaxBounds.Y - FogMinBounds.Y;

    // 3) Loop over the parallel arrays
    const int32 Count = FMath::Min3(Positions.Num(), WorldRadii.Num(), UnitTeamIds.Num());
    for (int32 i = 0; i < Count; ++i)
    {
        // 3a) Team filter
        if (UnitTeamIds[i] != TeamId)
        {
            continue;
        }

        // 3b) Compute pixel‐space center
        const FVector WorldPos = Positions[i];
        const float U = (WorldPos.X - FogMinBounds.X) / WorldExtentX;
        const float V = (WorldPos.Y - FogMinBounds.Y) / WorldExtentY;
        const int32 CenterX = FMath::Clamp(FMath::RoundToInt(U * FogTexSize), 0, FogTexSize - 1);
        const int32 CenterY = FMath::Clamp(FMath::RoundToInt(V * FogTexSize), 0, FogTexSize - 1);

        // 3c) Compute soft circle radii in pixels
        const float Normalized = WorldRadii[i] / WorldExtentX;  
        int32 HardRadius = FMath::RoundToInt(Normalized * FogTexSize);
        HardRadius = FMath::Clamp(HardRadius, 0, FogTexSize - 1);
        const int32 FalloffPixels = FMath::Max(1, HardRadius / 10);

        const float HardRadiusSq  = float(HardRadius * HardRadius);
        const float OuterRadiusSq = float((HardRadius + FalloffPixels) * (HardRadius + FalloffPixels));

        // 3d) Draw the circle into FogPixels
        for (int32 dY = - (HardRadius + FalloffPixels); dY <= (HardRadius + FalloffPixels); ++dY)
        {
            const int32 Y = CenterY + dY;
            if (Y < 0 || Y >= FogTexSize) continue;

            FColor* Row = FogPixels.GetData() + Y * FogTexSize;
            for (int32 dX = - (HardRadius + FalloffPixels); dX <= (HardRadius + FalloffPixels); ++dX)
            {
                const int32 X = CenterX + dX;
                if (X < 0 || X >= FogTexSize) continue;

                const float DistSq = float(dX*dX + dY*dY);
                if (DistSq > OuterRadiusSq)
                {
                    continue;
                }

                float Intensity;
                if (DistSq <= HardRadiusSq)
                {
                    Intensity = 1.0f;
                }
                else
                {
                    const float Dist   = FMath::Sqrt(DistSq);
                    const float Delta  = (Dist - float(HardRadius)) / float(FalloffPixels); // 0..1
                    Intensity          = FMath::Clamp(1.0f - Delta, 0.0f, 1.0f);
                }

                const uint8 Gray = uint8(FMath::RoundToInt(Intensity * 255.0f));
                FColor& Pixel    = Row[X];
                if (Gray > Pixel.R)  // since R=G=B
                {
                    Pixel = FColor(Gray, Gray, Gray, 255);
                }
            }
        }
    }

    // 4) Upload updated pixels to the GPU texture
    FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, FogTexSize, FogTexSize);
    FogMaskTexture->UpdateTextureRegions(
        0, 1, Region,
        FogTexSize * sizeof(FColor),
        sizeof(FColor),
        reinterpret_cast<uint8*>(FogPixels.GetData())
    );
}

void AFogActor::Multicast_UpdateFogMaskWithCircles_Implementation(
    const TArray<FVector_NetQuantize>& Positions,
    const TArray<float>&              WorldRadii,
    const TArray<uint8>&              UnitTeamIds)
{
    if (!FogMaskTexture)
    {
        return;
    }

    // 1) Clear mask to black
    for (FColor& Pixel : FogPixels)
    {
        Pixel = FColor::Black;
    }

    // 2) Precompute for pixel conversion
    const float WorldExtentX = FogMaxBounds.X - FogMinBounds.X;
    const float WorldExtentY = FogMaxBounds.Y - FogMinBounds.Y;

    // 3) Loop over the parallel arrays
    const int32 Count = FMath::Min3(Positions.Num(), WorldRadii.Num(), UnitTeamIds.Num());
    for (int32 i = 0; i < Count; ++i)
    {
        // 3a) Team filter
        if (UnitTeamIds[i] != TeamId)
        {
            continue;
        }

        // 3b) Compute pixel‐space center
        const FVector WorldPos = Positions[i];
        const float U = (WorldPos.X - FogMinBounds.X) / WorldExtentX;
        const float V = (WorldPos.Y - FogMinBounds.Y) / WorldExtentY;
        const int32 CenterX = FMath::Clamp(FMath::RoundToInt(U * FogTexSize), 0, FogTexSize - 1);
        const int32 CenterY = FMath::Clamp(FMath::RoundToInt(V * FogTexSize), 0, FogTexSize - 1);

        // 3c) Compute soft circle radii in pixels
        const float Normalized = WorldRadii[i] / WorldExtentX;  
        int32 HardRadius = FMath::RoundToInt(Normalized * FogTexSize);
        HardRadius = FMath::Clamp(HardRadius, 0, FogTexSize - 1);
        const int32 FalloffPixels = FMath::Max(1, HardRadius / 10);

        const float HardRadiusSq  = float(HardRadius * HardRadius);
        const float OuterRadiusSq = float((HardRadius + FalloffPixels) * (HardRadius + FalloffPixels));

        // 3d) Draw the circle into FogPixels
        for (int32 dY = - (HardRadius + FalloffPixels); dY <= (HardRadius + FalloffPixels); ++dY)
        {
            const int32 Y = CenterY + dY;
            if (Y < 0 || Y >= FogTexSize) continue;

            FColor* Row = FogPixels.GetData() + Y * FogTexSize;
            for (int32 dX = - (HardRadius + FalloffPixels); dX <= (HardRadius + FalloffPixels); ++dX)
            {
                const int32 X = CenterX + dX;
                if (X < 0 || X >= FogTexSize) continue;

                const float DistSq = float(dX*dX + dY*dY);
                if (DistSq > OuterRadiusSq)
                {
                    continue;
                }

                float Intensity;
                if (DistSq <= HardRadiusSq)
                {
                    Intensity = 1.0f;
                }
                else
                {
                    const float Dist   = FMath::Sqrt(DistSq);
                    const float Delta  = (Dist - float(HardRadius)) / float(FalloffPixels); // 0..1
                    Intensity          = FMath::Clamp(1.0f - Delta, 0.0f, 1.0f);
                }

                const uint8 Gray = uint8(FMath::RoundToInt(Intensity * 255.0f));
                FColor& Pixel    = Row[X];
                if (Gray > Pixel.R)  // since R=G=B
                {
                    Pixel = FColor(Gray, Gray, Gray, 255);
                }
            }
        }
    }

    // 4) Upload updated pixels to the GPU texture
    FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, FogTexSize, FogTexSize);
    FogMaskTexture->UpdateTextureRegions(
        0, 1, Region,
        FogTexSize * sizeof(FColor),
        sizeof(FColor),
        reinterpret_cast<uint8*>(FogPixels.GetData())
    );
}