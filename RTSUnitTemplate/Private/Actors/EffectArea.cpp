// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/EffectArea.h"
#include "Mass/MassActorBindingComponent.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "NiagaraComponent.h"
#include "Net/UnrealNetwork.h"
#include "Components/InstancedStaticMeshComponent.h"


// Sets default values
AEffectArea::AEffectArea()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ISMTemplate = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ISMTemplate"));
	ISMTemplate->SetupAttachment(SceneRoot);
	// Template is only for Blueprint setup; no collision/overlap
	ISMTemplate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ISMTemplate->SetGenerateOverlapEvents(false);
	ISMTemplate->SetHiddenInGame(true);

	Niagara_A = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Niagara"));
	Niagara_A->SetupAttachment(SceneRoot);
	
	MassBindingComponent = CreateDefaultSubobject<UMassActorBindingComponent>(TEXT("MassBindingComponent"));

	bUseEffectAreaImpactProcessor = false;
	StartRadius = 100.f;
	EndRadius = 500.f;
	TimeToEndRadius = 5.f;
	ScaleMesh = false;
	bIsRadiusScaling = true;
	BaseRadius = 100.f;
	bPulsate = false;
	bDestroyOnImpact = false;
	bScaleOnImpact = false;
	bIsScalingAfterImpact = false;
	bPendingDestructionRep = false;
	HideOnDestructionDelay = 0.0f;
	DestroyOnDestructionDelay = 0.5f;
	EarlySpawnTime = 1.0f;
	SpawnCountOnDestruction = 1;
	bSpawnDoGroundTrace = true;
	SpawnVerticalOffset = 0.f;
	SpawnRandomOffsetMin = 0.f;
	SpawnRandomOffsetMax = 0.f;

	if (HasAuthority())
	{
		bReplicates = true;
	}
}

// Called when the game starts or when spawned
void AEffectArea::BeginPlay()
{
	Super::BeginPlay();
	SetReplicateMovement(true);


	if (MassBindingComponent)
	{
		MassBindingComponent->SetupMassOnEffectArea();
	}
}

// Called every frame
void AEffectArea::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bUseEffectAreaImpactProcessor)
	{
		LifeTime += DeltaTime;
		if (MaxLifeTime > 0.f && LifeTime >= MaxLifeTime) {
			Destroy(true, false);
		}
	}
}

void AEffectArea::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Adds an instance for preview in the editor if a mesh is selected
	if (ISMTemplate && ISMTemplate->GetInstanceCount() == 0 && ISMTemplate->GetStaticMesh())
	{
		ISMTemplate->AddInstance(FTransform::Identity);
	}
}

void AEffectArea::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AEffectArea, AreaEffectOne);
	DOREPLIFETIME(AEffectArea, AreaEffectTwo);
	DOREPLIFETIME(AEffectArea, AreaEffectThree);
	DOREPLIFETIME(AEffectArea, Niagara_A);
	DOREPLIFETIME(AEffectArea, bImpactVFXTriggered);
	DOREPLIFETIME(AEffectArea, bIsScalingAfterImpact);
	DOREPLIFETIME(AEffectArea, bImpactScaleTriggered);
	DOREPLIFETIME(AEffectArea, bPendingDestructionRep);
}

void AEffectArea::SetActorVisibility(bool bVisible)
{
	if (Niagara_A) Niagara_A->SetVisibility(bVisible);
}

void AEffectArea::SetEnemyVisibility(AActor* DetectingActor, bool bVisible)
{
	if (bUseEffectAreaImpactProcessor) return;

	if (DetectingActor == nullptr || !DetectingActor->IsValidLowLevelFast())
	{
		return;
	}

	int32 DetectorTeamId = -1;
	if (AUnitBase* Unit = Cast<AUnitBase>(DetectingActor))
	{
		DetectorTeamId = Unit->TeamId;
	}

	if (DetectorTeamId == -1) return;
	
	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	ACustomControllerBase* MyController = Cast<ACustomControllerBase>(PC);

	if (MyController && MyController->IsValidLowLevelFast())
	{
		if (MyController->SelectableTeamId == DetectorTeamId)
		{
			bIsVisibleByFog = bVisible;
		}
	}
}

bool AEffectArea::ComputeLocalVisibility() const
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	ACustomControllerBase* CustomPC = Cast<ACustomControllerBase>(PC);
	if (!CustomPC) return true;

	int32 LocalTeamId = CustomPC->SelectableTeamId;
	bool bIsMyTeam = (TeamId == LocalTeamId || LocalTeamId == 0);

	bool VisibleByTeam = true;
	if (bInvisibleToEnemies && !bIsMyTeam && TeamId != 0)
	{
		VisibleByTeam = false;
	}

	bool VisibleByFog = true;
	if (bAffectedByFogOfWar && !bIsMyTeam && TeamId != 0)
	{
		VisibleByFog = bIsVisibleByFog;
	}

	// Invisibility status (relevant for enemies)
	if (bIsInvisible && !bIsMyTeam)
	{
		return false;
	}

	return VisibleByTeam && VisibleByFog;
}

void AEffectArea::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bUseEffectAreaImpactProcessor) return;

	if(OtherActor)
	{
		AUnitBase* UnitToHit = Cast<AUnitBase>(OtherActor);


		if(UnitToHit && UnitToHit->GetUnitState() == UnitData::Dead)
		{
			// Do Nothing
		}else if(UnitToHit && UnitToHit->TeamId != TeamId && !IsHealing)
		{
			UnitToHit->ApplyInvestmentEffect(AreaEffectOne);
			UnitToHit->ApplyInvestmentEffect(AreaEffectTwo);
			UnitToHit->ApplyInvestmentEffect(AreaEffectThree);
			ImpactEvent(UnitToHit);
		}else if(UnitToHit && UnitToHit->TeamId == TeamId && IsHealing)
		{
			UnitToHit->ApplyInvestmentEffect(AreaEffectOne);
			UnitToHit->ApplyInvestmentEffect(AreaEffectTwo);
			UnitToHit->ApplyInvestmentEffect(AreaEffectThree);
			ImpactEvent(UnitToHit);
		}
	}
}
