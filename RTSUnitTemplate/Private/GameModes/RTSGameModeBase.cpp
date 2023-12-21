// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "GameModes/RTSGameModeBase.h"
#include "PlayerStart/PlayerStartBase.h"
#include "Characters/Camera/CameraBase.h"
#include "EngineUtils.h"
#include "Controller/CameraControllerBase.h"
#include "Hud/PathProviderHUD.h"
#include "Components/SkeletalMeshComponent.h"
#include "AIController.h"
#include "Actors/Waypoint.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"


void ARTSGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	FTimerHandle TimerHandle;
	SetTeamIds();
	if(!DisableSpawn)SetupTimerFromDataTable_Implementation(FVector(0.f), nullptr);

}

void ARTSGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	SetTeamIds();
}

void ARTSGameModeBase::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARTSGameModeBase, SpawnTimerHandles);
	DOREPLIFETIME(ARTSGameModeBase, TimerIndex);
	
}

void ARTSGameModeBase::SetTeamId_Implementation(int Id, ACameraControllerBase* CameraControllerBase)
{

	if(CameraControllerBase)
	{
		CameraControllerBase->SetControlerTeamId(Id);
	}
	
}

void ARTSGameModeBase::SetTeamIds_Implementation()
{
	TArray<APlayerStartBase*> PlayerStarts;
	for (TActorIterator<APlayerStartBase> It(GetWorld()); It; ++It)
	{
		PlayerStarts.Add(*It);
	}

	int32 PlayerStartIndex = 0;
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		
		AController* PlayerController = It->Get();
		ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(PlayerController);

		if (CameraControllerBase && PlayerStarts.IsValidIndex(PlayerStartIndex))
		{
			APlayerStartBase* CustomPlayerStart = PlayerStarts[PlayerStartIndex];
			SetTeamId_Implementation(CustomPlayerStart->SelectableTeamId, CameraControllerBase);
			PlayerStartIndex++;  // Move to the next PlayerStart for the next iteration
		}
	}
}

void ARTSGameModeBase::SetupTimerFromDataTable_Implementation(FVector Location, AUnitBase* UnitToChase)
{
	if (UnitSpawnParameter && HasAuthority())
	{
		TArray<FName> RowNames = UnitSpawnParameter->GetRowNames();
		for (const FName& RowName : RowNames)
		{
			FUnitSpawnParameter* SpawnParameterPtr = UnitSpawnParameter->FindRow<FUnitSpawnParameter>(RowName, TEXT(""));

			if (SpawnParameterPtr) 
			{
				FUnitSpawnParameter SpawnParameter = *SpawnParameterPtr; // Copy the struct
				
				// Use a weak pointer for the GameMode to ensure it's still valid when the timer fires
				TWeakObjectPtr<ARTSGameModeBase> WeakThis(this);
				
				// Set the timer
				if (SpawnParameter.ShouldLoop)
				{
					// Use SpawnParameter by value in the lambda
					auto TimerCallback = [WeakThis, SpawnParameter, Location, UnitToChase]()
					{
						// Check if the GameMode is still valid
						if (!WeakThis.IsValid()) return;

						WeakThis->SpawnUnits_Implementation(SpawnParameter, Location, UnitToChase, 0, nullptr);
					};
					
					FTimerHandle TimerHandle;
					SpawnTimerHandles.Add(TimerHandle);
					GetWorld()->GetTimerManager().SetTimer(SpawnTimerHandles[TimerIndex], TimerCallback, SpawnParameter.LoopTime, true);
					TimerIndex++;
				}

			
			}

		}
	}
}


void ARTSGameModeBase::SpawnUnitFromDataTable(int id, FVector Location, AUnitBase* UnitToChase, int TeamId, AWaypoint* Waypoint)
{
	if (UnitSpawnParameter)
	{
		TArray<FName> RowNames = UnitSpawnParameter->GetRowNames();
		for (const FName& RowName : RowNames)
		{
			FUnitSpawnParameter* SpawnParameter = UnitSpawnParameter->FindRow<FUnitSpawnParameter>(RowName, TEXT(""));
			if (SpawnParameter && SpawnParameter->Id == id)
			{
			
				SpawnUnits_Implementation(*SpawnParameter, Location, UnitToChase, TeamId, Waypoint);
			}
		}
	}
}

AUnitBase* ARTSGameModeBase::SpawnSingleUnitFromDataTable(int id, FVector Location, AUnitBase* UnitToChase, int TeamId,
	AWaypoint* Waypoint)
{
	if (UnitSpawnParameter)
	{
		TArray<FName> RowNames = UnitSpawnParameter->GetRowNames();
		for (const FName& RowName : RowNames)
		{
			FUnitSpawnParameter* SpawnParameter = UnitSpawnParameter->FindRow<FUnitSpawnParameter>(RowName, TEXT(""));
			if (SpawnParameter && SpawnParameter->Id == id)
			{
			
				return SpawnSingleUnits(*SpawnParameter, Location, UnitToChase, TeamId, Waypoint);
			}
		}
	}
	return nullptr;
}

int32 ARTSGameModeBase::CheckAndRemoveDeadUnits(int32 SpecificUnitID)
{
	int32 CountOfSpecificID = 0;

	for (int32 i = UnitSpawnDataSets.Num() - 1; i >= 0; --i)
	{
		FUnitSpawnData& UnitData = UnitSpawnDataSets[i];

		// Assuming AUnitBase has a method to check if the unit is dead
		if (UnitData.UnitBase && UnitData.UnitBase->GetUnitState() == UnitData::Dead)
		{
			UnitSpawnDataSets.RemoveAt(i);
		}
		else if (UnitData.Id == SpecificUnitID)
		{
			CountOfSpecificID++;
		}
	}

	return CountOfSpecificID;
}

AUnitBase* ARTSGameModeBase::SpawnSingleUnits(FUnitSpawnParameter SpawnParameter, FVector Location,
	AUnitBase* UnitToChase, int TeamId, AWaypoint* Waypoint)
{
	// Waypointspawn
	const FVector FirstLocation = CalcLocation(SpawnParameter.UnitOffset+Location, SpawnParameter.UnitMinRange, SpawnParameter.UnitMaxRange);

	FTransform EnemyTransform;
	EnemyTransform.SetLocation(FVector(FirstLocation.X, FirstLocation.Y, 100.f));
		
	const auto UnitBase = Cast<AUnitBase>
		(UGameplayStatics::BeginDeferredActorSpawnFromClass
		(this, SpawnParameter.UnitBaseClass, EnemyTransform, ESpawnActorCollisionHandlingMethod::AlwaysSpawn));

		

	AAIController* ControllerBase = GetWorld()->SpawnActor<AAIController>(SpawnParameter.UnitControllerBaseClass, FTransform());

	if(!ControllerBase) return nullptr;

	ControllerBase->Possess(UnitBase);
		
	if (UnitBase != nullptr)
	{
		if(UnitBase->UnitToChase)
		{
			UnitBase->UnitToChase = UnitToChase;
			UnitBase->SetUnitState(UnitData::Chase);
		}

		// Check and apply CharacterMesh
		if (SpawnParameter.CharacterMesh)
		{
			UnitBase->MeshAssetPath = SpawnParameter.CharacterMesh->GetPathName();
		}

		// Check and apply Material
		if (SpawnParameter.Material)
		{
			UnitBase->MeshMaterialPath = SpawnParameter.Material->GetPathName();
		}

		if (SpawnParameter.TeamId)
		{
			UnitBase->TeamId = SpawnParameter.TeamId;
		}

		if(TeamId)
		{
			UnitBase->TeamId = TeamId;
		}

		UnitBase->ServerMeshRotation = SpawnParameter.ServerMeshRotation;
			
		UnitBase->OnRep_MeshAssetPath();
		UnitBase->OnRep_MeshMaterialPath();

		UnitBase->SetReplicateMovement(true);
		SetReplicates(true);
		UnitBase->GetMesh()->SetIsReplicated(true);

		// Does this have to be replicated?
		UnitBase->SetMeshRotationServer();
			
		AssignWaypointToUnit(UnitBase, SpawnParameter.WaypointTag);


		if(Waypoint)
		{
			UnitBase->NextWaypoint = Waypoint;
		}
		UnitBase->UnitState = SpawnParameter.State;
		UnitBase->UnitStatePlaceholder = SpawnParameter.StatePlaceholder;
		
		UGameplayStatics::FinishSpawningActor(UnitBase, EnemyTransform);

		if(SpawnParameter.Attributes)
		{
			UnitBase->DefaultAttributeEffect = SpawnParameter.Attributes;
		}
		
		UnitBase->InitializeAttributes();
		
		return UnitBase;
	}

	return nullptr;
}


void ARTSGameModeBase::SpawnUnits_Implementation(FUnitSpawnParameter SpawnParameter, FVector Location, AUnitBase* UnitToChase, int TeamId, AWaypoint* Waypoint)
{

	int UnitCount = CheckAndRemoveDeadUnits(SpawnParameter.Id);
	
	if(UnitCount < SpawnParameter.MaxUnitSpawnCount)
	for(int i = 0; i < SpawnParameter.UnitCount; i++)
	{
		// Waypointspawn
		const FVector FirstLocation = CalcLocation(SpawnParameter.UnitOffset+Location, SpawnParameter.UnitMinRange, SpawnParameter.UnitMaxRange);

		FTransform EnemyTransform;
		 EnemyTransform.SetLocation(FVector(FirstLocation.X, FirstLocation.Y, 100.f));
		
		const auto UnitBase = Cast<AUnitBase>
			(UGameplayStatics::BeginDeferredActorSpawnFromClass
			(this, SpawnParameter.UnitBaseClass, EnemyTransform, ESpawnActorCollisionHandlingMethod::AlwaysSpawn));

		

		AAIController* ControllerBase = GetWorld()->SpawnActor<AAIController>(SpawnParameter.UnitControllerBaseClass, FTransform());

		if(!ControllerBase) return;

		ControllerBase->Possess(UnitBase);
		
		if (UnitBase != nullptr)
		{
			if(UnitBase->UnitToChase)
			{
				UnitBase->UnitToChase = UnitToChase;
				UnitBase->SetUnitState(UnitData::Chase);
			}

			// Check and apply CharacterMesh
			if (SpawnParameter.CharacterMesh)
			{
				UnitBase->MeshAssetPath = SpawnParameter.CharacterMesh->GetPathName();
			}

			// Check and apply Material
			if (SpawnParameter.Material)
			{
				UnitBase->MeshMaterialPath = SpawnParameter.Material->GetPathName();
			}

			if (SpawnParameter.TeamId)
			{
				UnitBase->TeamId = SpawnParameter.TeamId;
			}
			
			if(TeamId)
			{
				UnitBase->TeamId = TeamId;
			}

			UnitBase->ServerMeshRotation = SpawnParameter.ServerMeshRotation;
			
			UnitBase->OnRep_MeshAssetPath();
			UnitBase->OnRep_MeshMaterialPath();

			UnitBase->SetReplicateMovement(true);
			SetReplicates(true);
			UnitBase->GetMesh()->SetIsReplicated(true);

			// Does this have to be replicated?
			UnitBase->SetMeshRotationServer();
			
			AssignWaypointToUnit(UnitBase, SpawnParameter.WaypointTag);


			if(Waypoint)
			{
				UnitBase->NextWaypoint = Waypoint;
			}
			UnitBase->UnitState = SpawnParameter.State;
			UnitBase->UnitStatePlaceholder = SpawnParameter.StatePlaceholder;
			
			UGameplayStatics::FinishSpawningActor(UnitBase, EnemyTransform);

			if(SpawnParameter.Attributes)
			{
				UnitBase->DefaultAttributeEffect = SpawnParameter.Attributes;
			}
		
			UnitBase->InitializeAttributes();
			
			FUnitSpawnData UnitSpawnDataSet;
			UnitSpawnDataSet.Id = SpawnParameter.Id;
			UnitSpawnDataSet.UnitBase = UnitBase;
			UnitSpawnDataSet.SpawnParameter = SpawnParameter;
			UnitSpawnDataSets.Add(UnitSpawnDataSet);
			
		}
		
	}
	// Enemyspawn
}

void ARTSGameModeBase::AssignWaypointToUnit(AUnitBase* UnitBase, const FString& WaypointTag)
{
	if (UnitBase == nullptr)
	{
		return;
	}

	// Find Waypoints with the specified tag
	TArray<AActor*> FoundWaypoints;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWaypoint::StaticClass(), FoundWaypoints);

	// Filter waypoints by tag
	AWaypoint* SelectedWaypoint = nullptr;
	for (AActor* Actor : FoundWaypoints)
	{
		AWaypoint* Waypoint = Cast<AWaypoint>(Actor);
		if (Waypoint && Waypoint->Tag == WaypointTag)
		{
			SelectedWaypoint = Waypoint;
			break; // or choose waypoints based on some other criteria
		}
	}

	// Assign the found waypoint to the unit
	if (SelectedWaypoint != nullptr)
	{
		UnitBase->NextWaypoint = SelectedWaypoint;
	}

}


FVector ARTSGameModeBase::CalcLocation(FVector Offset, FVector MinRange, FVector MaxRange)
{
	int MultiplierA;
	const float randChooserA = FMath::RandRange(1, 10);
	if(randChooserA <= 5)
	{
		MultiplierA = -1;
	}else
	{
		MultiplierA = 1;
	}

	int MultiplierB;
	const float randChooserB = FMath::RandRange(1, 10);
	if(randChooserB <= 5)
	{
		MultiplierB = -1;
	}else
	{
		MultiplierB = 1;
	}
				
	const float RandomOffsetX = FMath::RandRange(MinRange.X, MaxRange.X);
	const float RandomOffsetY = FMath::RandRange(MinRange.Y, MaxRange.Y);
	const float RandomOffsetZ = FMath::RandRange(MinRange.Z, MaxRange.Z);
				
	const float X = RandomOffsetX*MultiplierA+Offset.X; 
	const float Y = RandomOffsetY*MultiplierB+Offset.Y; 
	const float Z = RandomOffsetZ+Offset.Z;
				
	return FVector(X, Y, Z);
}
