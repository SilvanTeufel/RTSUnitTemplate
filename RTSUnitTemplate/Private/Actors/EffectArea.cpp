// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/EffectArea.h"
#include "Mass/MassActorBindingComponent.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "NiagaraComponent.h"
#include "Net/UnrealNetwork.h"


// Sets default values
AEffectArea::AEffectArea()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);
	
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionProfileName(TEXT("Trigger")); // Kollisionsprofil festlegen
	Mesh->SetGenerateOverlapEvents(true);

	Niagara_A = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Niagara"));
	Niagara_A->SetupAttachment(SceneRoot);
	
	MassBindingComponent = CreateDefaultSubobject<UMassActorBindingComponent>(TEXT("MassBindingComponent"));

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
	SetScaleTimer();

	if (MassBindingComponent)
	{
		MassBindingComponent->SetupMassOnEffectArea();
	}
}

// Called every frame
void AEffectArea::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	LifeTime+=DeltaTime;
	
	if(LifeTime >= MaxLifeTime){
		Destroy(true, false);
	}
	
}

void AEffectArea::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AEffectArea, AreaEffectOne);
	DOREPLIFETIME(AEffectArea, AreaEffectTwo);
	DOREPLIFETIME(AEffectArea, AreaEffectThree);
	DOREPLIFETIME(AEffectArea, Mesh);
	DOREPLIFETIME(AEffectArea, Niagara_A);
}

void AEffectArea::ScaleMesh()
{
	if (IsGettingBigger && Mesh)
	{
		FVector NewScale = Mesh->GetComponentScale() * BiggerScaler;
		Mesh->SetWorldScale3D(NewScale);
	}
}

void AEffectArea::SetActorVisibility(bool bVisible)
{
	if (Mesh) Mesh->SetVisibility(bVisible);
	if (Niagara_A) Niagara_A->SetVisibility(bVisible);
}

void AEffectArea::SetEnemyVisibility(AActor* DetectingActor, bool bVisible)
{
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

void AEffectArea::SetScaleTimer()
{
	float ScaleInterval = BiggerScaleInterval; // Set this to your desired interval in seconds
	if (IsGettingBigger)
	{
		GetWorld()->GetTimerManager().SetTimer(ScaleTimerHandle, this, &AEffectArea::ScaleMesh, ScaleInterval, true);
	}
}


void AEffectArea::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
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
