// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/ProceduralPond.h"

#include "AI/NavigationModifier.h"
#include "AI/Navigation/NavigationRelevantData.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "UObject/UObjectIterator.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "NavAreas/NavArea_Null.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UPondNavComponent::UPondNavComponent()
{
	bAttachToOwnersRoot = false;
}

void UPondNavComponent::CalcAndCacheBounds() const
{
	const AProceduralPond* Pond = Cast<AProceduralPond>(GetOwner());
	if (!Pond)
	{
		Bounds = FBox(ForceInit);
		return;
	}
	const float R = FMath::Max(Pond->RadiusX, Pond->RadiusY) + Pond->NavPadding;
	const FVector C = Pond->GetActorLocation();
	Bounds = FBox(C - FVector(R, R, HalfHeight), C + FVector(R, R, HalfHeight));
}

void UPondNavComponent::GetNavigationData(FNavigationRelevantData& Data) const
{
	const AProceduralPond* Pond = Cast<AProceduralPond>(GetOwner());
	if (!Pond || Pond->RadiusX <= 0.f || Pond->RadiusY <= 0.f)
	{
		return;
	}

	// Same ellipse the material draws (water edge), grown by NavPadding. Below and above the centre so the
	// prism catches the navmesh on uneven ground. An ellipse is convex - one modifier, no box corners.
	const FTransform Xf(FRotator(0.f, Pond->GetActorRotation().Yaw, 0.f), Pond->GetActorLocation());
	TArray<FVector> Points;
	Points.Reserve(NumPoints * 2);
	for (const float Z : { -HalfHeight, HalfHeight })
	{
		for (int32 i = 0; i < NumPoints; ++i)
		{
			const float A = 2.f * PI * i / NumPoints;
			const FVector Local((Pond->RadiusX + Pond->NavPadding) * FMath::Cos(A), (Pond->RadiusY + Pond->NavPadding) * FMath::Sin(A), Z);
			Points.Add(Xf.TransformPosition(Local));
		}
	}
	Data.Modifiers.Add(FAreaNavModifier(Points, ENavigationCoordSystem::Unreal, FTransform::Identity, UNavArea_Null::StaticClass()));
}

void UPondNavComponent::RefreshPond()
{
	bBoundsInitialized = false;
	RefreshNavigationModifiers();
}

AProceduralPond::AProceduralPond()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetHidden(true);
	SetCanBeDamaged(false);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	NavComponent = CreateDefaultSubobject<UPondNavComponent>(TEXT("PondNav"));
}

void AProceduralPond::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AProceduralPond, RadiusX);
	DOREPLIFETIME(AProceduralPond, RadiusY);
	DOREPLIFETIME(AProceduralPond, Depth);
	DOREPLIFETIME(AProceduralPond, NavPadding);
	DOREPLIFETIME(AProceduralPond, Index);
	DOREPLIFETIME(AProceduralPond, Collection);
}

void AProceduralPond::BeginPlay()
{
	Super::BeginPlay();
	ApplyPond();
	ClearVegetation();

	// PCG graphs may still be generating at match start: sweep again after 3, 10 and 30 s - and after
	// every later regeneration.
	if (GetNetMode() != NM_DedicatedServer)
	{
		GetWorldTimerManager().SetTimer(PlantTimer, this, &AProceduralPond::OnPlantTimer, 3.f, false);
		BindPCGGenerated();
	}
}

void AProceduralPond::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindPCGGenerated();
	ApplyToCollection(true);
	for (UPondGrassStrip* Strip : GrassStrips)
	{
		ALandscapeProxy::RemoveExclusionBox(FWeakObjectPtr(Strip));
	}
	GrassStrips.Reset();
	Super::EndPlay(EndPlayReason);
}

void AProceduralPond::OnRep_Pond()
{
	if (HasActorBegunPlay())
	{
		ApplyPond();
	}
}

void AProceduralPond::ApplyPond()
{
	ApplyToCollection(false);
	if (NavComponent)
	{
		NavComponent->RefreshPond();
	}
}

void AProceduralPond::ApplyToCollection(bool bClear)
{
	if (!Collection || GetNetMode() == NM_DedicatedServer || Index < 0 || Index >= MaxPonds)
	{
		return;
	}
	const FName PondName(*FString::Printf(TEXT("Pond%d"), Index));
	const FName PondBName(*FString::Printf(TEXT("PondB%d"), Index));
	if (bClear)
	{
		UKismetMaterialLibrary::SetVectorParameterValue(this, Collection, PondName, FLinearColor(0.f, 0.f, 0.f, 0.f));
		UKismetMaterialLibrary::SetVectorParameterValue(this, Collection, PondBName, FLinearColor(1.f, 0.f, 0.f, 0.f));
		return;
	}

	// Pond<i>  = centre X, centre Y, half axis X, half axis Y
	// PondB<i> = cos(yaw), sin(yaw), depth, phase (shore waves of neighbouring ponds do not run in step)
	const FVector C = GetActorLocation();
	const float Yaw = FMath::DegreesToRadians(GetActorRotation().Yaw);
	UKismetMaterialLibrary::SetVectorParameterValue(this, Collection, PondName, FLinearColor(C.X, C.Y, RadiusX, RadiusY));
	UKismetMaterialLibrary::SetVectorParameterValue(this, Collection, PondBName,
		FLinearColor(FMath::Cos(Yaw), FMath::Sin(Yaw), Depth, FMath::Frac(C.X * 0.0137f + C.Y * 0.0071f) * 6.2832f));

	// The material only walks the slots below PondCount.
	const float Count = UKismetMaterialLibrary::GetScalarParameterValue(this, Collection, TEXT("PondCount"));
	if (Count < Index + 1)
	{
		UKismetMaterialLibrary::SetScalarParameterValue(this, Collection, TEXT("PondCount"), float(Index + 1));
	}
}

void AProceduralPond::ClearVegetation()
{
	UWorld* World = GetWorld();
	if (!World || GetNetMode() == NM_DedicatedServer || RadiusX <= 0.f || RadiusY <= 0.f)
	{
		return;
	}
	const FVector C = GetActorLocation();
	const FTransform Xf(FRotator(0.f, GetActorRotation().Yaw, 0.f), C);
	const float Margin = NavPadding + 30.f;

	// The ellipse outline in world space.
	TArray<FVector2D> Outline;
	FBox2D Box(ForceInit);
	for (int32 i = 0; i < 64; ++i)
	{
		const float A = 2.f * PI * i / 64.f;
		const FVector P = Xf.TransformPosition(FVector((RadiusX + Margin) * FMath::Cos(A), (RadiusY + Margin) * FMath::Sin(A), 0.f));
		Outline.Add(FVector2D(P));
		Box += FVector2D(P);
	}

	// Landscape grass: the landscape keeps one exclusion box per owner - several strips follow the ellipse
	// instead of one box that would leave bare corners.
	for (UPondGrassStrip* Strip : GrassStrips)
	{
		ALandscapeProxy::RemoveExclusionBox(FWeakObjectPtr(Strip));
	}
	GrassStrips.Reset();
	const int32 NumStrips = 6;
	const float StripW = (Box.Max.X - Box.Min.X) / NumStrips;
	for (int32 s = 0; s < NumStrips; ++s)
	{
		const float X0 = Box.Min.X + s * StripW, X1 = X0 + StripW;
		float Y0 = TNumericLimits<float>::Max(), Y1 = TNumericLimits<float>::Lowest();
		for (int32 i = 0; i < Outline.Num(); ++i)
		{
			// Edge from i to i+1: the part of it inside the strip widens the strip's Y range.
			const FVector2D& A = Outline[i];
			const FVector2D& B = Outline[(i + 1) % Outline.Num()];
			for (int32 k = 0; k <= 4; ++k)
			{
				const FVector2D P = FMath::Lerp(A, B, k / 4.f);
				if (P.X >= X0 - 1.f && P.X <= X1 + 1.f)
				{
					Y0 = FMath::Min(Y0, P.Y);
					Y1 = FMath::Max(Y1, P.Y);
				}
			}
		}
		if (Y0 > Y1)
		{
			continue;
		}
		UPondGrassStrip* Strip = NewObject<UPondGrassStrip>(this);
		GrassStrips.Add(Strip);
		ALandscapeProxy::AddExclusionBox(FWeakObjectPtr(Strip), FBox(FVector(X0, Y0, C.Z - 2000.f), FVector(X1, Y1, C.Z + 2000.f)));
	}
	const FBox WorldBox(FVector(Box.Min, C.Z - 2000.f), FVector(Box.Max, C.Z + 2000.f));
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		if (It->GetComponentsBoundingBox().Intersect(WorldBox))
		{
			It->FlushGrassComponents(nullptr, false);
		}
	}

	RemoveInstancedPlants();
}

void AProceduralPond::OnPlantTimer()
{
	++PlantPass;
	RemoveInstancedPlants();
	if (PlantPass < 3)
	{
		GetWorldTimerManager().SetTimer(PlantTimer, this, &AProceduralPond::OnPlantTimer, PlantPass == 1 ? 7.f : 20.f, false);
	}
}

namespace
{
	const FMulticastDelegateProperty* FindPCGGeneratedDelegate(UClass*& OutPCGClass)
	{
		OutPCGClass = FindObject<UClass>(nullptr, TEXT("/Script/PCG.PCGComponent"));
		return OutPCGClass
			? CastField<FMulticastDelegateProperty>(OutPCGClass->FindPropertyByName(TEXT("OnPCGGraphGeneratedExternal")))
			: nullptr;
	}
}

void AProceduralPond::BindPCGGenerated()
{
	UClass* PCGClass = nullptr;
	const FMulticastDelegateProperty* Generated = FindPCGGeneratedDelegate(PCGClass);
	if (!Generated)
	{
		return;   // PCG plugin not loaded
	}
	FScriptDelegate Delegate;
	Delegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(AProceduralPond, HandlePCGGraphGenerated));
	TArray<UObject*> Components;
	GetObjectsOfClass(PCGClass, Components, true);
	for (UObject* Obj : Components)
	{
		UActorComponent* Comp = Cast<UActorComponent>(Obj);
		if (IsValid(Comp) && Comp->GetWorld() == GetWorld())
		{
			Generated->AddDelegate(Delegate, Comp);
			BoundPCGComponents.Add(Comp);
		}
	}
}

void AProceduralPond::UnbindPCGGenerated()
{
	UClass* PCGClass = nullptr;
	const FMulticastDelegateProperty* Generated = FindPCGGeneratedDelegate(PCGClass);
	if (Generated)
	{
		FScriptDelegate Delegate;
		Delegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(AProceduralPond, HandlePCGGraphGenerated));
		for (const TWeakObjectPtr<UActorComponent>& Comp : BoundPCGComponents)
		{
			if (Comp.IsValid())
			{
				Generated->RemoveDelegate(Delegate, Comp.Get());
			}
		}
	}
	BoundPCGComponents.Reset();
}

void AProceduralPond::HandlePCGGraphGenerated(UActorComponent* PCGComponent)
{
	RemoveInstancedPlants();
}

void AProceduralPond::RemoveInstancedPlants()
{
	UWorld* World = GetWorld();
	if (!World || RadiusX <= 0.f || RadiusY <= 0.f)
	{
		return;
	}
	const FVector C = GetActorLocation();
	const float Margin = NavPadding + 30.f;
	const float Reach = FMath::Max(RadiusX, RadiusY) + Margin;
	const FBox WorldBox(C - FVector(Reach, Reach, 2000.f), C + FVector(Reach, Reach, 2000.f));

	// Painted foliage and PCG plants: remove the instances standing in the water. Units, buildings and
	// everything else that uses instanced meshes are left alone.
	int32 Removed = 0;
	TMap<FString, int32> Skipped;
	for (TObjectIterator<UInstancedStaticMeshComponent> It; It; ++It)
	{
		UInstancedStaticMeshComponent* ISM = *It;
		if (!IsValid(ISM) || ISM->GetWorld() != World || ISM->GetInstanceCount() == 0)
		{
			continue;
		}
		if (!ISM->Bounds.GetBox().Intersect(WorldBox))
		{
			continue;
		}
		const AActor* ISMOwner = ISM->GetOwner();
		const FString OwnerClass = ISMOwner ? ISMOwner->GetClass()->GetName() : FString();
		// Plants: painted foliage, or anything a PCG graph generated (PCG tags its components; the owner
		// may also be any actor that carries a PCG component, not only a PCG volume).
		bool bPlant = OwnerClass.Contains(TEXT("InstancedFoliageActor")) || OwnerClass.Contains(TEXT("PCG"))
			|| ISM->ComponentHasTag(TEXT("PCG Generated Component"));
		if (!bPlant && ISMOwner)
		{
			for (const UActorComponent* Comp : ISMOwner->GetComponents())
			{
				if (Comp && Comp->GetClass()->GetName().Contains(TEXT("PCGComponent")))
				{
					bPlant = true;
					break;
				}
			}
		}
		if (!bPlant)
		{
			int32 Inside = 0;
			for (int32 i = 0; i < ISM->GetInstanceCount() && Inside < 1000; ++i)
			{
				FTransform T;
				Inside += (ISM->GetInstanceTransform(i, T, true) && ContainsPoint2D(T.GetLocation(), Margin)) ? 1 : 0;
			}
			if (Inside > 0)
			{
				Skipped.FindOrAdd(OwnerClass + TEXT("/") + GetNameSafe(ISM->GetStaticMesh())) += Inside;
			}
			continue;
		}
		TArray<int32> ToRemove;
		for (int32 i = 0; i < ISM->GetInstanceCount(); ++i)
		{
			FTransform T;
			if (ISM->GetInstanceTransform(i, T, true) && ContainsPoint2D(T.GetLocation(), Margin))
			{
				ToRemove.Add(i);
			}
		}
		if (ToRemove.Num() > 0)
		{
			ISM->RemoveInstances(ToRemove);
			Removed += ToRemove.Num();
		}
	}
	// Small decoration placed as single actors (twigs, pebbles): hidden in the water. Big rocks stay - the
	// pond placement keeps away from nothing that small, but a cliff would never sit on flat pond ground.
	int32 Hidden = 0;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		AStaticMeshActor* Deco = *It;
		if (!IsValid(Deco) || Deco->IsHidden())
		{
			continue;
		}
		FVector Origin, Extent;
		Deco->GetActorBounds(false, Origin, Extent);
		if (Extent.Size2D() < 250.f && ContainsPoint2D(Origin, -10.f))
		{
			Deco->SetActorHiddenInGame(true);
			Deco->SetActorEnableCollision(false);
			++Hidden;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Ponds] Pond %d (pass %d): %d grass strips, %d plants removed, %d props hidden."), Index, PlantPass, GrassStrips.Num(), Removed, Hidden);
	for (const TPair<FString, int32>& S : Skipped)
	{
		UE_LOG(LogTemp, Log, TEXT("[Ponds] Pond %d: kept %d instances of %s in the water."), Index, S.Value, *S.Key);
	}
}

float AProceduralPond::EdgeRadiusTowards(const FVector& WorldDirection) const
{
	const float Yaw = FMath::DegreesToRadians(GetActorRotation().Yaw);
	const FVector2D D = FVector2D(WorldDirection).GetSafeNormal();
	const float Lx = D.X * FMath::Cos(Yaw) + D.Y * FMath::Sin(Yaw);
	const float Ly = -D.X * FMath::Sin(Yaw) + D.Y * FMath::Cos(Yaw);
	const float Denom = FMath::Sqrt(FMath::Square(Lx / FMath::Max(RadiusX, 1.f)) + FMath::Square(Ly / FMath::Max(RadiusY, 1.f)));
	return Denom > KINDA_SMALL_NUMBER ? 1.f / Denom : FMath::Max(RadiusX, RadiusY);
}

bool AProceduralPond::ContainsPoint2D(const FVector& WorldPoint, float Margin) const
{
	const FVector Delta = WorldPoint - GetActorLocation();
	const float Dist = FVector2D(Delta).Size();
	return Dist <= EdgeRadiusTowards(Delta) + Margin;
}
