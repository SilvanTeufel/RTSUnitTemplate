// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/FogActor.h"

#include "MassActorSubsystem.h"
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
}

void AFogActor::BeginPlay()
{
	Super::BeginPlay();
	InitFogMaskTexture();
	// Start timer to apply material every 0.2 seconds
	GetWorldTimerManager().SetTimer(FogUpdateTimerHandle, this, &AFogActor::Multicast_ApplyFogMask, 0.2f, true);
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
	FogMaskTexture = UTexture2D::CreateTransient(FogTexSize, FogTexSize, PF_B8G8R8A8);
	check(FogMaskTexture);

	FogMaskTexture->SRGB = false;
	FogMaskTexture->CompressionSettings = TC_VectorDisplacementmap;
	FogMaskTexture->MipGenSettings = TMGS_NoMipmaps;
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

void AFogActor::Server_RequestFogUpdate_Implementation(const TArray<FMassEntityHandle>& Entities)
{
	Multicast_UpdateFogMaskWithCircles(Entities);
}

void AFogActor::Multicast_UpdateFogMaskWithCircles_Implementation(const TArray<FMassEntityHandle>& Entities)
{
	UE_LOG(LogTemp, Warning, TEXT("Multicast_UpdateFogMaskWithCircles_Implementation"));
	UE_LOG(LogTemp, Warning, TEXT("%s: UpdateFogMaskWithCircles called on %s"),
		HasAuthority() ? TEXT("[Server]") : TEXT("[Client]"),
		*GetName());

	if (!FogMaskTexture) return;

	for (FColor& C : FogPixels)
		C = FColor::Black;

	TArray<FVector> UnitWorldPositions;

	if (UWorld* World = GetWorld())
	{
		UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(World);
		if (!EntitySubsystem) return;

		FMassEntityManager& EM = EntitySubsystem->GetMutableEntityManager();

		for (const FMassEntityHandle& E : Entities)
		{
			if (!EM.IsEntityValid(E)) continue;
			
			const FTransformFragment* TF = EM.GetFragmentDataPtr<FTransformFragment>(E);
			FMassActorFragment* AF = EM.GetFragmentDataPtr<FMassActorFragment>(E);
			if (!TF || !AF) continue;

			AUnitBase* Unit = Cast<AUnitBase>(AF->GetMutable());
			if (Unit && Unit->TeamId == TeamId)  // Assuming you have GetTeamId()
			{
				UnitWorldPositions.Add(Unit->GetActorLocation());
			}
		}
		/*
		for (const FMassEntityHandle& E : Entities)
		{
			const FTransformFragment* TF = EM.GetFragmentDataPtr<FTransformFragment>(E);
			const FMassCombatStatsFragment* CF = EM.GetFragmentDataPtr<FMassCombatStatsFragment>(E);
			// Instead of Using CF do a Cast to UnitBase and do ->GetActorLocation to get the Location
			if (!TF || !CF) continue;

			if (CF->TeamId == TeamId)
			{
				UnitWorldPositions.Add(TF->GetTransform().GetLocation());
			}
		}*/
	}

	for (const FVector& WP : UnitWorldPositions)
	{
		float U = (WP.X - FogMinBounds.X) / (FogMaxBounds.X - FogMinBounds.X);
		float V = (WP.Y - FogMinBounds.Y) / (FogMaxBounds.Y - FogMinBounds.Y);

		int32 CenterX = FMath::Clamp(FMath::RoundToInt(U * FogTexSize), 0, FogTexSize - 1);
		int32 CenterY = FMath::Clamp(FMath::RoundToInt(V * FogTexSize), 0, FogTexSize - 1);

		for (int32 dY = -CircleRadius; dY <= CircleRadius; ++dY)
		{
			int32 Y = CenterY + dY;
			if (Y < 0 || Y >= FogTexSize) continue;

			int32 HalfW = FMath::FloorToInt(FMath::Sqrt(FMath::Max(0.f, CircleRadius * CircleRadius - dY * dY)));
			int32 MinX = FMath::Clamp(CenterX - HalfW, 0, FogTexSize - 1);
			int32 MaxX = FMath::Clamp(CenterX + HalfW, 0, FogTexSize - 1);

			FColor* Row = FogPixels.GetData() + Y * FogTexSize;
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				Row[X] = FColor::White;
			}
		}
	}

	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, FogTexSize, FogTexSize);
	FogMaskTexture->UpdateTextureRegions(0, 1, Region, FogTexSize * sizeof(FColor), sizeof(FColor), (uint8*)FogPixels.GetData());
}