// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Actors/SelectionCircleActor.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Controller/PlayerController/CustomControllerBase.h"

// Sets default values
ASelectionCircleActor::ASelectionCircleActor()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    // Mesh setup
   
    CircleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CircleMesh"));
    RootComponent = CircleMesh;

    CircleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CircleMesh->SetRelativeScale3D(FVector(CircleMapSize, CircleMapSize, 1.f));
    CircleMesh->SetHiddenInGame(true);  
    CircleMapMinBounds = FVector2D(-CircleMapSize * 50, -CircleMapSize * 50);
    CircleMapMaxBounds = FVector2D(CircleMapSize * 50, CircleMapSize * 50);
}

void ASelectionCircleActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASelectionCircleActor, TeamId);
}

void ASelectionCircleActor::BeginPlay()
{
    Super::BeginPlay();
    InitCircleMaskTexture();
    GetWorldTimerManager().SetTimer(SelectionCircleUpdateTimerHandle, this, &ASelectionCircleActor::Multicast_ApplyCircleMask, SelectionCircleUpdateRate, true, 0.0f);
}

void ASelectionCircleActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void ASelectionCircleActor::InitCircleMaskTexture()
{
    CircleMesh->SetRelativeScale3D(FVector(CircleMapSize, CircleMapSize, 1.f));
    CircleMapMinBounds = FVector2D(-CircleMapSize*50, -CircleMapSize*50);
    CircleMapMaxBounds = FVector2D(CircleMapSize*50, CircleMapSize*50);

    CircleMaskTexture = UTexture2D::CreateTransient(CircleTexSize, CircleTexSize, PF_B8G8R8A8);
    check(CircleMaskTexture);

    CircleMaskTexture->SRGB = false;
    CircleMaskTexture->CompressionSettings = TC_VectorDisplacementmap;
    CircleMaskTexture->AddToRoot();
    CircleMaskTexture->UpdateResource();

    CirclePixels.Init(FColor::Transparent, CircleTexSize * CircleTexSize);
}

void ASelectionCircleActor::ApplyCircleMaskToMesh(UStaticMeshComponent* MeshComponent, UMaterialInterface* BaseMaterial, int32 MaterialIndex)
{
    if (!MeshComponent || !BaseMaterial || !CircleMaskTexture) return;
    
    MeshComponent->SetMaterial(MaterialIndex, BaseMaterial);
    UMaterialInstanceDynamic* MID = MeshComponent->CreateDynamicMaterialInstance(MaterialIndex, BaseMaterial);

    if (MID)
    {
       MID->SetTextureParameterValue(TEXT("SelectionCircleTex"), CircleMaskTexture);
    }
}


void ASelectionCircleActor::Multicast_ApplyCircleMask_Implementation()
{
    UWorld* World = GetWorld();
    if (!World) return;

    APlayerController* PC = World->GetFirstPlayerController();
    if (ACustomControllerBase* CustomPC = Cast<ACustomControllerBase>(PC))
    {
        if (CustomPC->SelectableTeamId == TeamId)
        {
            if (CircleMesh && CircleMaterial)
            {
                CircleMesh->SetHiddenInGame(false);
                ApplyCircleMaskToMesh(CircleMesh, CircleMaterial, 0);
            }
        }
    }
}


void ASelectionCircleActor::SetCircleBounds(const FVector2D& Min, const FVector2D& Max)
{
    CircleMapMinBounds = Min;
    CircleMapMaxBounds = Max;
}

void ASelectionCircleActor::Multicast_UpdateSelectionCircles_Implementation(const TArray<FVector_NetQuantize>& Positions, const TArray<float>& WorldRadii)
{
    if (!CircleMaskTexture)
    {
        return;
    }

    // 1) Clear mask
    FMemory::Memset(CirclePixels.GetData(), 0, CirclePixels.Num() * sizeof(FColor));

    // 2) Precompute
    const float WorldExtentX = CircleMapMaxBounds.X - CircleMapMinBounds.X;
    const float WorldExtentY = CircleMapMaxBounds.Y - CircleMapMinBounds.Y;

    // 3) Loop over the arrays
    const int32 Count = FMath::Min(Positions.Num(), WorldRadii.Num());
    for (int32 i = 0; i < Count; ++i)
    {
        // [MODIFIED] 3a) Compute pixel‐space center using integers for a "sharp" grid-snapped result.
        const FVector WorldPos = Positions[i];
        const float U = (WorldPos.X - CircleMapMinBounds.X) / WorldExtentX;
        const float V = (WorldPos.Y - CircleMapMinBounds.Y) / WorldExtentY;
        const int32 CenterX = FMath::Clamp(FMath::RoundToInt(U * CircleTexSize), 0, CircleTexSize - 1);
        const int32 CenterY = FMath::Clamp(FMath::RoundToInt(V * CircleTexSize), 0, CircleTexSize - 1);

        // [MODIFIED] 3b) Compute the square's "half side" length from the world radius.
        const int32 HalfSide = FMath::Clamp(FMath::RoundToInt((WorldRadii[i] / WorldExtentX) * CircleTexSize), 1, CircleTexSize - 1);

        // 3c) Draw the 1px sharp square outline
        // Define the bounding box for the loop for efficiency.
        const int32 MinX = FMath::Max(0, CenterX - HalfSide);
        const int32 MaxX = FMath::Min(CircleTexSize - 1, CenterX + HalfSide);
        const int32 MinY = FMath::Max(0, CenterY - HalfSide);
        const int32 MaxY = FMath::Min(CircleTexSize - 1, CenterY + HalfSide);

        for (int32 Y = MinY; Y <= MaxY; ++Y)
        {
            FColor* Row = CirclePixels.GetData() + Y * CircleTexSize;
            for (int32 X = MinX; X <= MaxX; ++X)
            {
                const int32 dX_abs = FMath::Abs(X - CenterX);
                const int32 dY_abs = FMath::Abs(Y - CenterY);
                
                // [MODIFIED] Check if the pixel lies on the border of the square.
                if ((dX_abs == HalfSide && dY_abs <= HalfSide) || (dY_abs == HalfSide && dX_abs <= HalfSide))
                {
                    Row[X] = FColor::White;
                }
            }
        }
    }

    // 4) Upload updated pixels to the GPU texture
    FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, CircleTexSize, CircleTexSize);
    CircleMaskTexture->UpdateTextureRegions(
        0, 1, Region,
        CircleTexSize * sizeof(FColor),
        sizeof(FColor),
        reinterpret_cast<uint8*>(CirclePixels.GetData())
    );
}