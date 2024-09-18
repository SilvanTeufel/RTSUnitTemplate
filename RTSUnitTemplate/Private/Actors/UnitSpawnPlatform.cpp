// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/UnitSpawnPlatform.h"
#include "AIController.h"
#include "Actors/Waypoint.h"
#include "Components/StaticMeshComponent.h"
#include "Controller/PlayerController/ExtendedControllerBase.h"
#include "GameModes/RTSGameModeBase.h"
#include "Widgets/SpawnEnergyBar.h"

// Sets default values
AUnitSpawnPlatform::AUnitSpawnPlatform(const FObjectInitializer& ObjectInitializer)
{
	// Set this actor to call Tick() every frame. You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Create and set up the static mesh component
	PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
	RootComponent = PlatformMesh;

	// Create spawn point
	SpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SpawnPoint"));
	SpawnPoint->SetupAttachment(RootComponent);

	EnergyWidgetComp = ObjectInitializer.CreateDefaultSubobject<UWidgetComponent>(this, TEXT("EnergyBar"));
	EnergyWidgetComp->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
}

// Called when the game starts or when spawned
void AUnitSpawnPlatform::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (PlayerController)
	{
		AExtendedControllerBase* ControllerBase = Cast<AExtendedControllerBase>(PlayerController);
		if (ControllerBase)
		{
			TeamId = ControllerBase->SelectableTeamId;
			DefaultWaypoint = ControllerBase->DefaultWaypoint;
			ControllerBase->SpawnPlatform = this;
		}
	}
	
	SpawnUnitsFromArray();

	const float RespawnInterval = 3.0f; // Time in seconds between respawns
	GetWorld()->GetTimerManager().SetTimer(RespawnTimerHandle, this, &AUnitSpawnPlatform::ReSpawnUnits, RespawnInterval, true);

	const float PositionInterval = 1.0f; // Time in seconds between Resposition
	GetWorld()->GetTimerManager().SetTimer(PositionTimerHandle, this, &AUnitSpawnPlatform::UpdateUnitPositions, PositionInterval, true);

	const float EnergyInterval = 1.0f; // Time in seconds between Resposition
	GetWorld()->GetTimerManager().SetTimer(EnergyTimerHandle, this, &AUnitSpawnPlatform::UpdateEnergy, PositionInterval, true);

	
	if (EnergyWidgetComp) {
		
		USpawnEnergyBar* Energybar = Cast<USpawnEnergyBar>(EnergyWidgetComp->GetUserWidgetObject());

		if (EnergyWidgetComp) {
			Energybar->SetOwnerActor(this);
		}
	}
}

void AUnitSpawnPlatform::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUnitSpawnPlatform, Energy);
	DOREPLIFETIME(AUnitSpawnPlatform, MaxEnergy);
	DOREPLIFETIME(AUnitSpawnPlatform, EnergyWidgetComp);
	DOREPLIFETIME(AUnitSpawnPlatform, TeamId);
}


void AUnitSpawnPlatform::SpawnUnitsFromArray()
{
	int NumUnits = DefaultUnitBaseClass.Num();
	
	if(NumUnits > 0)
	{
		FVector BaseOffset = FVector(0.0f, (UnitSpacing/2)+(-UnitSpacing * NumUnits / 2), UnitZOffset); // Adjust BaseOffset to start more to the left
		FQuat QuatRotation = GetActorQuat(); // Get the quaternion representing the platform's rotation
		BaseOffset = QuatRotation.RotateVector(BaseOffset);
		
		for(int i = 0; i < DefaultUnitBaseClass.Num(); i++)
		{
			if(i < DefaultAIControllerClass.Num())
			{
				if(DefaultAIControllerClass[i] && DefaultUnitBaseClass[i])
				{
					FVector Offset = FVector(0.0f, UnitSpacing * i, 0.0f); // Offset for each unit
					FVector RotatedOffset = QuatRotation.RotateVector(Offset); // Rotate the offset according to the platform's rotation
					FVector SpawnLocation = GetActorLocation() + BaseOffset + RotatedOffset; // Calculate the final spawn location
                    
					//FVector SpawnLocation = GetActorLocation() + BaseOffset + FVector(0.0f, UnitSpacing * i, 0.0f); // Adjust X or Y depending on orientation
					SpawnUnit(DefaultAIControllerClass[i], DefaultUnitBaseClass[i], DefaultMaterial, DefaultCharacterMesh, DefaultHostMeshRotation, SpawnLocation, DefaultState, DefaultStatePlaceholder, TeamId, DefaultWaypoint, DefaultUIndex);
				}
			}
		}
	}
}

// Called every frame
void AUnitSpawnPlatform::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AUnitSpawnPlatform::UpdateUnitPositions()
{
	
	FVector BaseOffset = FVector(0.0f, (UnitSpacing/2)+(-UnitSpacing * SpawnedUnits.Num() / 2), UnitZOffset);
	FQuat QuatRotation = GetActorQuat();
	BaseOffset = QuatRotation.RotateVector(BaseOffset);
	
	for (int i = 0; i < SpawnedUnits.Num(); ++i)
	{
		AUnitBase* Unit = SpawnedUnits[i];
		if (Unit && Unit->IsOnPlattform && !Unit->IsDragged)
		{
			FVector Offset = FVector(0.0f, UnitSpacing * i, 0.0f);
			FVector RotatedOffset = QuatRotation.RotateVector(Offset);
			FVector NewSpawnLocation = GetActorLocation() + BaseOffset + RotatedOffset;
			if (FVector::Dist(Unit->GetActorLocation(), NewSpawnLocation) > 100.0f)
			{
				Unit->SetActorLocation(NewSpawnLocation);
			}
		}
	}
}

void AUnitSpawnPlatform::UpdateEnergy()
{
	SetEnergy(GetEnergy()+1.f);
}
void AUnitSpawnPlatform::ReSpawnUnits()
{
	int NumUnits = SpawnedUnits.Num();
    
	if (NumUnits > 0)
	{
		FVector BaseOffset = FVector(0.0f, (UnitSpacing/2)+(-UnitSpacing * NumUnits / 2), UnitZOffset); // Adjust BaseOffset to start more to the left
		FQuat QuatRotation = GetActorQuat(); // Get the quaternion representing the platform's rotation
		BaseOffset = QuatRotation.RotateVector(BaseOffset);

		TArray<AUnitBase*> UnitsToRemove;
		
		for (int Index = 0; Index < NumUnits; ++Index)
		{
			AUnitBase* Unit = SpawnedUnits[Index];
			if (Unit && !Unit->IsOnPlattform)
			{
				FVector Offset = FVector(0.0f, UnitSpacing * Index, 0.0f); // Offset for each unit
				FVector RotatedOffset = QuatRotation.RotateVector(Offset); // Rotate the offset according to the platform's rotation
				FVector SpawnLocation = GetActorLocation() + BaseOffset + RotatedOffset; // Calculate the final spawn location
				
				TSubclassOf<AAIController> AIController = DefaultAIControllerClass.IsValidIndex(Index) ? DefaultAIControllerClass[Index] : nullptr;
				TSubclassOf<AUnitBase> UnitBaseClass = DefaultUnitBaseClass.IsValidIndex(Index) ? DefaultUnitBaseClass[Index] : nullptr;
            
				// Spawn the unit with these parameters:
				
				if (AIController && UnitBaseClass)
				{
					SpawnUnit(AIController, UnitBaseClass, DefaultMaterial, DefaultCharacterMesh, DefaultHostMeshRotation, SpawnLocation, DefaultState, DefaultStatePlaceholder, TeamId, DefaultWaypoint, DefaultUIndex);
					UnitsToRemove.Add(Unit); // Mark this unit for removal
				}
			}
		}

		// Remove units that were re-spawned
		for (AUnitBase* Unit : UnitsToRemove)
		{
			SpawnedUnits.Remove(Unit);
		}
	}
}

int AUnitSpawnPlatform::SpawnUnit(
TSubclassOf<class AAIController> AIControllerBaseClass,
TSubclassOf<class AUnitBase> UnitBaseClass, UMaterialInstance* Material, USkeletalMesh* CharacterMesh, FRotator HostMeshRotation, FVector Location,
TEnumAsByte<UnitData::EState> UState,
TEnumAsByte<UnitData::EState> UStatePlaceholder,
int NewTeamId, AWaypoint* Waypoint, int UIndex)
{
	FUnitSpawnParameter SpawnParameter;
	SpawnParameter.UnitControllerBaseClass = AIControllerBaseClass;
	SpawnParameter.UnitBaseClass = UnitBaseClass;
	SpawnParameter.UnitOffset = FVector3d(0.f,0.f,0.f);
	SpawnParameter.ServerMeshRotation = HostMeshRotation;
	SpawnParameter.State = UState;
	SpawnParameter.StatePlaceholder = UStatePlaceholder;
	SpawnParameter.Material = Material;
	SpawnParameter.CharacterMesh = CharacterMesh;
	// Waypointspawn

	ARTSGameModeBase* GameMode = Cast<ARTSGameModeBase>(GetWorld()->GetAuthGameMode());

	if(!GameMode) return 0;
	
	FTransform UnitTransform;
	
	UnitTransform.SetLocation(FVector(Location.X+SpawnParameter.UnitOffset.X, Location.Y+SpawnParameter.UnitOffset.Y, Location.Z+SpawnParameter.UnitOffset.Z));
		
		
	const auto UnitBase = Cast<AUnitBase>
		(UGameplayStatics::BeginDeferredActorSpawnFromClass
		(this, *SpawnParameter.UnitBaseClass, UnitTransform, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn));

	
	if(SpawnParameter.UnitControllerBaseClass)
	{
		AAIController* ControllerBase = GetWorld()->SpawnActor<AAIController>(SpawnParameter.UnitControllerBaseClass, FTransform());
		if(!ControllerBase) return 0;
		APawn* PawnBase = Cast<APawn>(UnitBase);
		if(PawnBase)
		{
			ControllerBase->Possess(PawnBase);
		}
	}
	
	if (UnitBase != nullptr)
	{

		if (SpawnParameter.CharacterMesh)
		{
			UnitBase->MeshAssetPath = SpawnParameter.CharacterMesh->GetPathName();
		}
		
		if (SpawnParameter.Material)
		{
			UnitBase->MeshMaterialPath = SpawnParameter.Material->GetPathName();
		}
		
		if(NewTeamId)
		{
			UnitBase->TeamId = NewTeamId;
		}

		UnitBase->ServerMeshRotation = SpawnParameter.ServerMeshRotation;
			
		UnitBase->OnRep_MeshAssetPath();
		UnitBase->OnRep_MeshMaterialPath();

		UnitBase->SetReplicateMovement(true);
		SetReplicates(true);
		UnitBase->GetMesh()->SetIsReplicated(true);

		// Does this have to be replicated?
		UnitBase->SetMeshRotationServer();
		
		UnitBase->UnitState = SpawnParameter.State;
		UnitBase->UnitStatePlaceholder = SpawnParameter.StatePlaceholder;
		UnitBase->IsOnPlattform = true;
		
		UGameplayStatics::FinishSpawningActor(
		 Cast<AActor>(UnitBase), 
		 UnitTransform
		);


		UnitBase->InitializeAttributes();

		if(Waypoint)
			UnitBase->NextWaypoint = Waypoint;
		
		if(UIndex == 0)
		{
			GameMode->AddUnitIndexAndAssignToAllUnitsArray(UnitBase);
		}

		SpawnedUnits.Emplace(UnitBase);
		return UnitBase->UnitIndex;
	}

	return 0;
}


void AUnitSpawnPlatform::HideOnTeamId(int PCTeamId)
{
	// Hide actor if it doesn't match the player's team
	SetActorHiddenInGame(PCTeamId != TeamId);
}

float AUnitSpawnPlatform::GetEnergy()
{
	return Energy;
}

float AUnitSpawnPlatform::GetMaxEnergy()
{
	return MaxEnergy;
}

void AUnitSpawnPlatform::SetEnergy(float NewEnergy)
{
	if(NewEnergy > MaxEnergy)
		Energy = MaxEnergy;
	else if(NewEnergy <= 0.f)
		Energy = 0.f;
	else
	{
		Energy = NewEnergy;
	}
}
