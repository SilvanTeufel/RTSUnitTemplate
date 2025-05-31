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
	// Mesh setup
	FogMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FogMesh"));
	RootComponent = FogMesh;

	FogMesh->SetHiddenInGame(true);  // Hidden by default
	FogMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FogMesh->SetRelativeScale3D(FVector(FogSize, FogSize, 1.f));
	FogMinBounds = FVector2D(-FogSize*50, -FogSize*50);
	FogMaxBounds = FVector2D(FogSize*50, FogSize*50);
}

void AFogActor::BeginPlay()
{
	Super::BeginPlay();
	InitFogMaskTexture();
	// Start timer to apply material every 0.2 seconds
	GetWorldTimerManager().SetTimer(FogUpdateTimerHandle, this, &AFogActor::Multicast_ApplyFogMask, FogUpdateRate, true);
}

void AFogActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AFogActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFogActor, TeamId);
}


void AFogActor::InitFogMaskTexture()
{

	FogMesh->SetRelativeScale3D(FVector(FogSize, FogSize, 1.f));
	FogMinBounds = FVector2D(-FogSize*50, -FogSize*50);
	FogMaxBounds = FVector2D(FogSize*50, FogSize*50);
	
	FogMaskTexture = UTexture2D::CreateTransient(FogTexSize, FogTexSize, PF_B8G8R8A8);
	check(FogMaskTexture);

	FogMaskTexture->SRGB = false;
	FogMaskTexture->CompressionSettings = TC_VectorDisplacementmap;
//#if WITH_EDITORONLY_DATA
	//FogMaskTexture->MipGenSettings = TMGS_NoMipmaps;
//#endif
	FogMaskTexture->AddToRoot();
	FogMaskTexture->UpdateResource();

	FogPixels.SetNumUninitialized(FogTexSize * FogTexSize);
}

void AFogActor::ApplyFogMaskToMesh(UStaticMeshComponent* MeshComponent, UMaterialInterface* BaseMaterial,
	int32 MaterialIndex)
{
	if (!MeshComponent || !BaseMaterial || !FogMaskTexture) return;

	MeshComponent->SetMaterial(MaterialIndex, BaseMaterial);
	UMaterialInstanceDynamic* MID = MeshComponent->CreateDynamicMaterialInstance(MaterialIndex, BaseMaterial);
	if (MID)
	{
		MID->SetTextureParameterValue(TEXT("FogMaskTex"), FogMaskTexture);
	}
}

void AFogActor::Multicast_ApplyFogMask_Implementation()
{
	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (ACustomControllerBase* CustomPC = Cast<ACustomControllerBase>(PC))
	{
		if (CustomPC->SelectableTeamId == TeamId)
		{
			if (FogMesh && FogMaterial)
			{
				FogMesh->SetHiddenInGame(false);
				ApplyFogMaskToMesh(FogMesh, FogMaterial, 0);
			}
		}
	}
}

void AFogActor::SetFogBounds(const FVector2D& Min, const FVector2D& Max)
{
	FogMinBounds = Min;
	FogMaxBounds = Max;
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
/*
void AFogActor::Multicast_UpdateFogMaskWithCircles_Implementation(const TArray<FMassEntityHandle>& Entities)
{
    if (!FogMaskTexture)
        return;

    // Clear the fog mask to black
    for (FColor& Pixel : FogPixels)
    {
        Pixel = FColor::Black;
    }

    // Arrays to store unit positions and their corresponding sight radii (in pixels)
    TArray<FVector> UnitWorldPositions;
    TArray<int32> UnitPixelRadii;

    if (UWorld* World = GetWorld())
    {
        if (UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>())
        {
            FMassEntityManager& EM = EntitySubsystem->GetMutableEntityManager();

            for (const FMassEntityHandle& E : Entities)
            {
                if (!EM.IsEntityValid(E))
                    continue;

                const FTransformFragment* TF = EM.GetFragmentDataPtr<FTransformFragment>(E);
                FMassActorFragment*        AF = EM.GetFragmentDataPtr<FMassActorFragment>(E);

            	const FMassAIStateFragment* AI = EM.GetFragmentDataPtr<FMassAIStateFragment>(E);
            	const FMassCombatStatsFragment* StateFrag = EM.GetFragmentDataPtr<FMassCombatStatsFragment>(E);
            	const FMassAgentCharacteristicsFragment* CharFrag =  EM.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(E);
                if (!TF || !AF || !AI || !StateFrag || !CharFrag)
                    continue;

            	if(StateFrag->Health <= 0.f && AI->StateTimer >= CharFrag->DespawnTime) continue;
            	
                AUnitBase* Unit = Cast<AUnitBase>(AF->GetMutable());
                if (Unit && Unit->TeamId == TeamId)
                {
                    // Store world position
                	FVector Loc = TF->GetTransform().GetLocation();
                    //FVector Loc = Unit->GetActorLocation();
                    UnitWorldPositions.Add(Loc);

                    // Convert sight radius (world units) to texture-space pixels
                    float WorldRadius = Unit->SightRadius;
                    float Normalized = WorldRadius / (FogMaxBounds.X - FogMinBounds.X);
                    int32 PixelRadius = FMath::RoundToInt(Normalized * FogTexSize);
                    PixelRadius = FMath::Clamp(PixelRadius, 0, FogTexSize - 1)/1.5f;

                    UnitPixelRadii.Add(PixelRadius);
                }
            }
        }
    }

    // Draw a circle per unit based on its sight radius
    for (int32 i = 0; i < UnitWorldPositions.Num(); ++i)
    {
        const FVector& WP = UnitWorldPositions[i];
        int32 Radius = UnitPixelRadii[i];

        // Normalize world position into UV
        float U = (WP.X - FogMinBounds.X) / (FogMaxBounds.X - FogMinBounds.X);
        float V = (WP.Y - FogMinBounds.Y) / (FogMaxBounds.Y - FogMinBounds.Y);

        int32 CenterX = FMath::Clamp(FMath::RoundToInt(U * FogTexSize), 0, FogTexSize - 1);
        int32 CenterY = FMath::Clamp(FMath::RoundToInt(V * FogTexSize), 0, FogTexSize - 1);
    	
    	// Innerer harter Radius + Dicke des Übergangs (in Pixeln)
    	const int32 HardRadius      = Radius;
    	const int32 FalloffPixels   = FMath::Max(1, Radius / 10);  // z.B. ein Drittel des Radius

    	// Precompute Squared Radii
    	const float HardRadiusSq    = float(HardRadius * HardRadius);
    	const float OuterRadiusSq   = float((HardRadius + FalloffPixels) * (HardRadius + FalloffPixels));

    	for (int32 dY = - (HardRadius + FalloffPixels); dY <= HardRadius + FalloffPixels; ++dY)
    	{
    		const int32 Y = CenterY + dY;
    		if (Y < 0 || Y >= FogTexSize) 
    			continue;

    		// Berechne maximale X-Ausdehnung in der erweiterten Zone
    		int32 MaxDX = HardRadius + FalloffPixels;
    		int32 MinX  = FMath::Clamp(CenterX - MaxDX, 0, FogTexSize - 1);
    		int32 MaxX  = FMath::Clamp(CenterX + MaxDX, 0, FogTexSize - 1);

    		FColor* Row = FogPixels.GetData() + Y * FogTexSize;
    		for (int32 X = MinX; X <= MaxX; ++X)
    		{
    			float dX = float(X - CenterX);
    			float DistSq = dX*dX + float(dY*dY);

    			if (DistSq > OuterRadiusSq)
    			{
    				// außerhalb des gesamten Bereichs: schwarz lassen
    				continue;
    			}

    			float Intensity;
    			if (DistSq <= HardRadiusSq)
    			{
    				// Innerhalb des harten Kreises: volle Helligkeit
    				Intensity = 1.0f;
    			}
    			else
    			{
    				// im Falloff-Bereich: linear von 1.0 → 0.0
    				float Dist   = FMath::Sqrt(DistSq);
    				float Delta  = (Dist - float(HardRadius)) / float(FalloffPixels); // 0..1
    				Intensity    = FMath::Clamp(1.0f - Delta, 0.0f, 1.0f);
    			}

    			uint8 Gray = uint8(FMath::RoundToInt(Intensity * 255.0f));

    			// Bei Überlappungen: behalte den hellsten Wert
    			FColor& Pixel = Row[X];
    			if (Gray > Pixel.R)  // R,G,B sind gleich, also genügt R-Vergleich
    			{
    				Pixel = FColor(Gray, Gray, Gray, 255);
    			}
    		}
    	}
    }

    // Update the GPU texture
    FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, FogTexSize, FogTexSize);
    FogMaskTexture->UpdateTextureRegions(0, 1, Region, FogTexSize * sizeof(FColor), sizeof(FColor), reinterpret_cast<uint8*>(FogPixels.GetData()));
}*/