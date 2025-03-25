// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "GameModes/RTSGameModeBase.h"
#include "PlayerStart/PlayerStartBase.h"
#include "Characters/Camera/CameraBase.h"
#include "EngineUtils.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "Hud/PathProviderHUD.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "AIController.h"
#include "Actors/Waypoint.h"
#include "Characters/Camera/ExtendedCameraBase.h"
#include "Controller/AIController/BuildingControllerBase.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"


void ARTSGameModeBase::BeginPlay()
{
	Super::BeginPlay();
	
	if(!DisableSpawn)SetupTimerFromDataTable_Implementation(FVector(0.f), nullptr);
	

	FillUnitArrays();

	FTimerHandle TimerHandleGatherController;
	GetWorldTimerManager().SetTimer(TimerHandleGatherController, this, &ARTSGameModeBase::SetTeamIdsAndWaypoints, GatherControllerTimer, false);
}

void ARTSGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
}

void ARTSGameModeBase::ApplyCustomizationsFromPlayerStart(APlayerController* PC, const APlayerStartBase* CustomStart)
{
	if (!PC || !CustomStart)
	{
		UE_LOG(LogTemp, Warning, TEXT("ApplyCustomizationsFromPlayerStart: Invalid PC or CustomStart"));
		return;
	}

	// --- Update the PlayerController with sound settings ---
	ACameraControllerBase* CameraController = Cast<ACameraControllerBase>(PC);
	if (CameraController)
	{
		// Check each sound property in the CustomStart. If available, use it; otherwise, use the current controller setting.
		USoundBase* NewWaypointSound = CustomStart->WaypointSound ? CustomStart->WaypointSound : CameraController->WaypointSound;
		USoundBase* NewRunSound = CustomStart->RunSound ? CustomStart->RunSound : CameraController->RunSound;
		USoundBase* NewAbilitySound = CustomStart->AbilitySound ? CustomStart->AbilitySound : CameraController->AbilitySound;
		USoundBase* NewAttackSound = CustomStart->AttackSound ? CustomStart->AttackSound : CameraController->AttackSound;
		USoundBase* NewDropWorkAreaFailedSound = CustomStart->DropWorkAreaFailedSound ? CustomStart->DropWorkAreaFailedSound : CameraController->DropWorkAreaFailedSound;
		USoundBase* NewDropWorkAreaSound = CustomStart->DropWorkAreaSound ? CustomStart->DropWorkAreaSound : CameraController->DropWorkAreaSound;

		// Update the controller's properties on the server.
		CameraController->WaypointSound = NewWaypointSound;
		CameraController->RunSound = NewRunSound;
		CameraController->AbilitySound = NewAbilitySound;
		CameraController->AttackSound = NewAttackSound;
		CameraController->DropWorkAreaFailedSound = NewDropWorkAreaFailedSound;
		CameraController->DropWorkAreaSound = NewDropWorkAreaSound;

		// Now push the updated settings to the client via a client RPC.
		CameraController->Client_ApplyCustomizations(
			NewWaypointSound,
			NewRunSound,
			NewAbilitySound,
			NewAttackSound,
			NewDropWorkAreaFailedSound,
			NewDropWorkAreaSound);
        
		
		UE_LOG(LogTemp, Log, TEXT("Updated sounds on PlayerController: %s"), *CameraController->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayerController is not of type ACameraControllerBase"));
	}
	// --- Update widget settings on the pawn ---
	APawn* Pawn = PC->GetPawn();
	if (Pawn)
	{
		AExtendedCameraBase* CameraPawn = Cast<AExtendedCameraBase>(Pawn);
		if (CameraPawn)
		{
			TSubclassOf<UUserWidget> NewWidgetSelector = CustomStart->WidgetSelector ? CustomStart->WidgetSelector : nullptr;
			TSubclassOf<UUserWidget> NewTaggedSelector = CustomStart->TaggedUnitSelector ? CustomStart->TaggedUnitSelector : nullptr;
			TSubclassOf<UUserWidget> NewResourceWidget = CustomStart->ResourceWidget ? CustomStart->ResourceWidget : nullptr;

			
			// Update the WidgetSelector widget component if a new widget class is provided.
			if (CameraPawn->WidgetSelector && CustomStart->WidgetSelector)
			{
				CameraPawn->WidgetSelector->SetWidget(nullptr);
				CameraPawn->WidgetSelector->SetWidgetClass(NewWidgetSelector);
				CameraPawn->WidgetSelector->InitWidget();
				UE_LOG(LogTemp, Log, TEXT("Updated WidgetSelector on Pawn: %s"), *CameraPawn->GetName());
			}
    
			// Update the TaggedSelector widget component.
			if (CameraPawn->TaggedSelector && CustomStart->TaggedUnitSelector)
			{
				CameraPawn->TaggedSelector->SetWidget(nullptr);
				CameraPawn->TaggedSelector->SetWidgetClass(NewTaggedSelector);
				CameraPawn->TaggedSelector->InitWidget();
				UE_LOG(LogTemp, Log, TEXT("Updated TaggedSelector on Pawn: %s"), *CameraPawn->GetName());
			}
    
			// Update the ResourceWidget widget component.
			if (CameraPawn->ResourceWidget && CustomStart->ResourceWidget)
			{
				CameraPawn->ResourceWidget->SetWidget(nullptr); // Remove current widget
				CameraPawn->ResourceWidget->SetWidgetClass(NewResourceWidget);
				CameraPawn->ResourceWidget->InitWidget();
				UE_LOG(LogTemp, Log, TEXT("Updated ResourceWidget on Pawn: %s"), *CameraPawn->GetName());
			}

			// Call Client RPC to update widgets on the client
			CameraPawn->Client_UpdateWidgets(NewWidgetSelector, NewTaggedSelector, NewResourceWidget);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Pawn is not of type ACameraBase"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("No Pawn possessed by PlayerController: %s"), *PC->GetName());
	}
}


void ARTSGameModeBase::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	//DOREPLIFETIME(ARTSGameModeBase, SpawnTimerHandleMap);
	DOREPLIFETIME(ARTSGameModeBase, TimerIndex);
	DOREPLIFETIME(ARTSGameModeBase, AllUnits);
}

int32 FindMatchingIndex(const TArray<int32>& IdArray, int32 SearchId)
{
	int32 Index = INDEX_NONE; // Initialize Index with an invalid value

	for (int32 i = 0; i < IdArray.Num(); ++i)
	{
		if (IdArray[i] == SearchId)
		{
			Index = i;
			break;
		}
	}

	return Index; // Return the found index, or INDEX_NONE if not found
}

void ARTSGameModeBase::SetTeamIdAndDefaultWaypoint_Implementation(int Id, AWaypoint* Waypoint, ACameraControllerBase* CameraControllerBase)
{

	if(CameraControllerBase)
	{
		CameraControllerBase->Multi_SetControllerTeamId(Id);
		CameraControllerBase->Multi_SetControllerDefaultWaypoint(Waypoint);
	}
	
}

void ARTSGameModeBase::FillUnitArrays()
{
	TArray <AActor*> GatheredUnits;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AUnitBase::StaticClass(), GatheredUnits);
	
	for(int i = 0; i < GatheredUnits.Num(); i++)
	{
		HighestUnitIndex++;
	
		AUnitBase* Unit = Cast<AUnitBase>(GatheredUnits[i]);
		FGameplayTag CameraUnitTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Character.CameraUnit")));
		if (Unit)
		{
			Unit->SetUnitIndex(HighestUnitIndex);
			AllUnits.Add(Unit);

			if (Unit && Unit->UnitTags.HasTagExact(CameraUnitTag))
			{
				CameraUnits.Add(Unit);
			}
		}

		ASpeakingUnit* SpeakingUnit = Cast<ASpeakingUnit>(Unit);
		if(SpeakingUnit)
			SpeakingUnits.Add(SpeakingUnit);

		AWorkingUnitBase* WorkingUnit = Cast<AWorkingUnitBase>(Unit);
		if(WorkingUnit)
			WorkingUnits.Add(WorkingUnit);
	}
}

void ARTSGameModeBase::SetTeamIdsAndWaypoints_Implementation()
{
	UE_LOG(LogTemp, Log, TEXT("SetTeamIdsAndWaypoints_Implementation!!!"));
	
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
			if (CameraControllerBase->CameraBase) CameraControllerBase->CameraBase->BlockControls = true;

			
			APlayerStartBase* CustomPlayerStart = PlayerStarts[PlayerStartIndex];

			ApplyCustomizationsFromPlayerStart(CameraControllerBase, CustomPlayerStart);
			
			UE_LOG(LogTemp, Log, TEXT("Assigning TeamId: %d to Controller: %s"), 
				CustomPlayerStart->SelectableTeamId, *CameraControllerBase->GetName());
			
			SetTeamIdAndDefaultWaypoint_Implementation(CustomPlayerStart->SelectableTeamId, CustomPlayerStart->DefaultWaypoint, CameraControllerBase);

			UE_LOG(LogTemp, Log, TEXT("TeamId is now: %d from Controller: %s"), 
			CameraControllerBase->SelectableTeamId, *CameraControllerBase->GetName());
			UE_LOG(LogTemp, Log, TEXT("AllUnits.Num(): %d"), AllUnits.Num());

			
			CameraControllerBase->Multi_SetFogManager(AllUnits);;
			
			CameraControllerBase->Multi_ShowWidgetsWhenLocallyControlled();
			CameraControllerBase->Multi_SetCamLocation(CustomPlayerStart->GetActorLocation());
			FGameplayTag CameraUnitTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Character.CameraUnit")));
			CameraControllerBase->SetCameraUnitWithTag_Implementation(CameraUnitTag, CameraControllerBase->SelectableTeamId);
			CameraControllerBase->Multi_HideEnemyWaypoints();
			
			PlayerStartIndex++;  // Move to the next PlayerStart for the next iteration

			if (CameraControllerBase->CameraBase) CameraControllerBase->CameraBase->BlockControls = false;
		}
	}

	SpawnAIFogManager();
}

void ARTSGameModeBase::SpawnAIFogManager()
{
	// TSGameMode->AllUnits is onyl available on Server why we start running on server
	// We Mutlicast to CLient and send him AllUnits as parameter

	for (int32 i = 0; i < AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(AllUnits[i]);
		// Every Controller can Check his own TeamId
		if (Unit && Unit->GetUnitState() != UnitData::Dead && !Unit->SpawnedFogManager)
		{
			Unit->SpawnFogOfWarManagerTeamIndependent(GetWorld()->GetFirstPlayerController());
		}
	}


}

void ARTSGameModeBase::SetupTimerFromDataTable_Implementation(FVector Location, AUnitBase* UnitToChase)
{

	for (UDataTable* UnitSpawnParameter : UnitSpawnParameters)
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
	

						auto TimerCallback = [WeakThis, SpawnParameter, Location, UnitToChase]()
						{
							// Check if the GameMode is still valid
							if (!WeakThis.IsValid()) return;

							// Now call SpawnUnits_Implementation with all the parameters
							WeakThis->SpawnUnits_Implementation(SpawnParameter, Location, UnitToChase);
						};
						/*
						// Use SpawnParameter by value in the lambda
						auto TimerCallback = [WeakThis, SpawnParameter, Location, UnitToChase]()
						{
							// Check if the GameMode is still valid
							if (!WeakThis.IsValid()) return;

							WeakThis->SpawnUnits_Implementation(SpawnParameter, Location, UnitToChase, 0, nullptr, 0, nullptr, -1);
						};*/
					
						FTimerHandle TimerHandle;
						//SpawnTimerHandles.Add(TimerHandle);
						GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerCallback, SpawnParameter.LoopTime, true);

						FTimerHandleMapping TimerMap;
						TimerMap.Id = SpawnParameter.Id;
						TimerMap.Timer = TimerHandle;
						TimerMap.SkipTimer = false;
						SpawnTimerHandleMap.Add(TimerMap);
						TimerIndex++;
					}
				}

			}
		}
	}
}

void ARTSGameModeBase::SetupUnitsFromDataTable_Implementation(FVector Location, AUnitBase* UnitToChase, const TArray<class UDataTable*>& UnitTable) // , int TeamId , const FString& WaypointTag, int32 UnitIndex, AUnitBase* SummoningUnit, int SummonIndex
{
	for (UDataTable* UnitSpawnParameter : UnitTable)
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
					SpawnUnits_Implementation(SpawnParameter, Location, UnitToChase);
				}

			}
		}
	}
}

FTimerHandleMapping ARTSGameModeBase::GetTimerHandleMappingById(int32 SearchId)
{
	for (FTimerHandleMapping& TimerMap : SpawnTimerHandleMap)
	{
		if (TimerMap.Id == SearchId)
		{
			return TimerMap; // Return a pointer to the struct
		}
	}

	return FTimerHandleMapping(); // Return nullptr if not found
}

void ARTSGameModeBase::SetSkipTimerMappingById(int32 SearchId, bool Value)
{
	for (FTimerHandleMapping& TimerMap : SpawnTimerHandleMap)
	{
		if (TimerMap.Id == SearchId)
		{
			TimerMap.SkipTimer = Value; // Return a pointer to the struct
		}
	}
}

void ARTSGameModeBase::SpawnUnitFromDataTable(int id, FVector Location, AUnitBase* UnitToChase) // , int TeamId, AWaypoint* Waypoint
{

	
	for (UDataTable* UnitSpawnParameter : UnitSpawnParameters)
	{
		if (UnitSpawnParameter)
		{
			TArray<FName> RowNames = UnitSpawnParameter->GetRowNames();
			for (const FName& RowName : RowNames)
			{
				FUnitSpawnParameter* SpawnParameter = UnitSpawnParameter->FindRow<FUnitSpawnParameter>(RowName, TEXT(""));
				if (SpawnParameter && SpawnParameter->Id == id)
				{
					SpawnUnits_Implementation(*SpawnParameter, Location, UnitToChase);
				}
			}
		}
	}
}

AUnitBase* ARTSGameModeBase::SpawnSingleUnitFromDataTable(int id, FVector Location, AUnitBase* UnitToChase, int TeamId,
	AWaypoint* Waypoint)
{
	for (UDataTable* UnitSpawnParameter : UnitSpawnParameters)
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
	}
	return nullptr;
}

bool ARTSGameModeBase::IsUnitWithIndexDead(int32 UnitIndex)
{
	for (int32 i = UnitSpawnDataSets.Num() - 1; i >= 0; --i)
	{
		FUnitSpawnData& UnitData = UnitSpawnDataSets[i];
	
		// Assuming AUnitBase has a method to check if the unit is dead
		if (UnitData.UnitBase && UnitData.UnitBase->GetUnitState() == UnitData::Dead && UnitData.UnitBase->UnitIndex == UnitIndex)
		{
			// Check if the index is not already in the array
			return true;
		}
	}
	return false;
}

bool ARTSGameModeBase::RemoveDeadUnitWithIndexFromDataSet(int32 UnitIndex)
{
	for (int32 i = UnitSpawnDataSets.Num() - 1; i >= 0; --i)
	{
		FUnitSpawnData& UnitData = UnitSpawnDataSets[i];
	
			// Assuming AUnitBase has a method to check if the unit is dead
			if (UnitData.UnitBase && UnitData.UnitBase->GetUnitState() == UnitData::Dead && UnitData.UnitBase->UnitIndex == UnitIndex)
			{
				// Check if the index is not already in the array
			
				//UnitData.UnitBase->SaveLevelDataAndAttributes(FString::FromInt(UnitData.UnitBase->UnitIndex));
				UnitData.UnitBase->SaveAbilityAndLevelData(FString::FromInt(UnitData.UnitBase->UnitIndex));
	
		
				AllUnits.Remove(UnitData.UnitBase);
					

				
				UnitSpawnDataSets.RemoveAt(i);
				return true;
			}
		}
	return false;
}

int32 ARTSGameModeBase::CheckAndRemoveDeadUnits(int32 SpawnParaId)
{
	int32 CountOfSpecificID = 0;
	bool FoundDeadUnit = false;
	
	for (int32 i = UnitSpawnDataSets.Num() - 1; i >= 0; --i)
	{
		FUnitSpawnData& UnitData = UnitSpawnDataSets[i];
		if(UnitData.Id == SpawnParaId)
		{
			// Assuming AUnitBase has a method to check if the unit is dead
			if (UnitData.UnitBase && UnitData.UnitBase->GetUnitState() == UnitData::Dead)
			{
				// Check if the index is not already in the array
				if (!AvailableUnitIndexArray.Contains(UnitData.UnitBase->UnitIndex))
				{
					//UnitData.UnitBase->SaveLevelDataAndAttributes(FString::FromInt(UnitData.UnitBase->UnitIndex));
					UnitData.UnitBase->SaveAbilityAndLevelData(FString::FromInt(UnitData.UnitBase->UnitIndex));
					AvailableUnitIndexArray.Add(UnitData.UnitBase->UnitIndex);
					SpawnParameterIdArray.Add(SpawnParaId);
					FoundDeadUnit = true;
				}

				AllUnits.Remove(UnitData.UnitBase);

				UnitSpawnDataSets.RemoveAt(i);
			}
			else //if (UnitData.Id == SpawnParaId)
			{
				CountOfSpecificID++;
			}
		}
	}

	if(CountOfSpecificID == 0 && FoundDeadUnit)
	{
		SetSkipTimerMappingById(SpawnParaId, true);
	}

	return CountOfSpecificID;
}

AUnitBase* ARTSGameModeBase::SpawnSingleUnits(FUnitSpawnParameter SpawnParameter, FVector Location,
	AUnitBase* UnitToChase, int TeamId, AWaypoint* Waypoint)
{
	if (!SpawnParameter.UnitBaseClass) return nullptr;
	// Waypointspawn
	const FVector FirstLocation = CalcLocation(SpawnParameter.UnitOffset+Location, SpawnParameter.UnitMinRange, SpawnParameter.UnitMaxRange);

	FTransform EnemyTransform;
	
	EnemyTransform.SetLocation(FVector(FirstLocation.X, FirstLocation.Y, SpawnParameter.UnitOffset.Z));
		
		
	const auto UnitBase = Cast<AUnitBase>
		(UGameplayStatics::BeginDeferredActorSpawnFromClass
		(this, SpawnParameter.UnitBaseClass, EnemyTransform, ESpawnActorCollisionHandlingMethod::AlwaysSpawn));

		

	if(SpawnParameter.UnitControllerBaseClass)
	{
		AAIController* AIController = GetWorld()->SpawnActor<AAIController>(SpawnParameter.UnitControllerBaseClass, FTransform());
		if(!AIController) return nullptr;
		AIController->Possess(UnitBase);
	}
	
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
		//UnitBase->GetMesh()->SetIsReplicated(true);

		// Does this have to be replicated?
		UnitBase->SetMeshRotationServer();
			
		AssignWaypointToUnit(UnitBase, SpawnParameter.WaypointTag);


		if(Waypoint)
		{
			UnitBase->NextWaypoint = Waypoint;
		}
		UnitBase->UnitState = SpawnParameter.State;
		UnitBase->UnitStatePlaceholder = SpawnParameter.StatePlaceholder;


		if(SpawnParameter.SpawnAtWaypoint && UnitBase->NextWaypoint)
		{
			FVector NewLocation = CalcLocation(FVector(UnitBase->NextWaypoint->GetActorLocation().X, UnitBase->NextWaypoint->GetActorLocation().Y, UnitBase->NextWaypoint->GetActorLocation().Z+50.f), SpawnParameter.UnitMinRange, SpawnParameter.UnitMaxRange);
			UnitBase->SetActorLocation(NewLocation);
		}

		
		UGameplayStatics::FinishSpawningActor(UnitBase, EnemyTransform);

		APlayerController* MyPC = GetWorld()->GetFirstPlayerController();
		if (MyPC)
		{
			UnitBase->SpawnFogOfWarManagerTeamIndependent(MyPC);
		}
		
		if(SpawnParameter.Attributes)
		{
			UnitBase->DefaultAttributeEffect = SpawnParameter.Attributes;
		}

		UnitBase->InitializeAttributes();

		
		AddUnitIndexAndAssignToAllUnitsArray(UnitBase);

		UnitBase->ScheduleDelayedNavigationUpdate();
		
		return UnitBase;
	}

	return nullptr;
}


void ARTSGameModeBase::SpawnUnits_Implementation(FUnitSpawnParameter SpawnParameter, FVector Location, AUnitBase* UnitToChase) // , int TeamId, AWaypoint* Waypoint, int32 UnitIndex, AUnitBase* SummoningUnit, int SummonIndex
{

	if (!SpawnParameter.UnitBaseClass) return;
	
	int UnitCount = CheckAndRemoveDeadUnits(SpawnParameter.Id);

	// Assuming UnitIndexArray is populated
	for (int32 Index : AvailableUnitIndexArray)
	{
		//UE_LOG(LogTemp, Warning, TEXT("Unit Index: %d"), Index);
	}

	FTimerHandleMapping TimerMap = GetTimerHandleMappingById(SpawnParameter.Id);
	if(UnitCount < SpawnParameter.MaxUnitSpawnCount && TimerMap.SkipTimer && SpawnParameter.SkipTimerAfterDeath){
		SetSkipTimerMappingById(SpawnParameter.Id, false);
		return;
	}
	
	HighestSquadId++;
	
	if(UnitCount < SpawnParameter.MaxUnitSpawnCount)
	{
		HighestSquadId++;
		for(int i = 0; i < SpawnParameter.UnitCount; i++)
		{
			// Waypointspawn
			const FVector FirstLocation = CalcLocation(SpawnParameter.UnitOffset+Location, SpawnParameter.UnitMinRange, SpawnParameter.UnitMaxRange);

			FTransform EnemyTransform;
			EnemyTransform.SetLocation(FVector(FirstLocation.X, FirstLocation.Y, SpawnParameter.UnitOffset.Z));
			
			const auto UnitBase = Cast<AUnitBase>
				(UGameplayStatics::BeginDeferredActorSpawnFromClass
				(this, SpawnParameter.UnitBaseClass, EnemyTransform, ESpawnActorCollisionHandlingMethod::AlwaysSpawn));

			

			AAIController* ControllerBase = GetWorld()->SpawnActor<AAIController>(SpawnParameter.UnitControllerBaseClass, FTransform());

			if(!ControllerBase) return;

			ControllerBase->Possess(UnitBase);
			
			if (UnitBase != nullptr)
			{
				if(UnitToChase != nullptr)
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

				UnitBase->ServerMeshRotation = SpawnParameter.ServerMeshRotation;
				
				UnitBase->OnRep_MeshAssetPath();
				UnitBase->OnRep_MeshMaterialPath();

				UnitBase->SetReplicateMovement(true);
				SetReplicates(true);
				//UnitBase->GetMesh()->SetIsReplicated(true);
				
				UnitBase->SetMeshRotationServer();
				
				AssignWaypointToUnit(UnitBase, SpawnParameter.WaypointTag);

				/*
				if(Waypoint != nullptr)
				{
					UnitBase->NextWaypoint = Waypoint;
				}*/
				
				UnitBase->UnitState = SpawnParameter.State;
				UnitBase->UnitStatePlaceholder = SpawnParameter.StatePlaceholder;
				UnitBase->SquadId = HighestSquadId;
				if(SpawnParameter.SpawnAtWaypoint && UnitBase->NextWaypoint)
				{
					FVector NewLocation = CalcLocation(FVector(UnitBase->NextWaypoint->GetActorLocation().X, UnitBase->NextWaypoint->GetActorLocation().Y, UnitBase->NextWaypoint->GetActorLocation().Z+50.f), SpawnParameter.UnitMinRange, SpawnParameter.UnitMaxRange);
					UnitBase->SetActorLocation(NewLocation);
				}

				/*
				if (UnitBase->EnableFog)
				{
					UnitBase->SetCharacterVisibility(false);
					UnitBase->IsVisibileEnemy = false;
				}
				*/
				UGameplayStatics::FinishSpawningActor(UnitBase, EnemyTransform);

		
				APlayerController* MyPC = GetWorld()->GetFirstPlayerController();
				if (MyPC)
				{
					UnitBase->SpawnFogOfWarManagerTeamIndependent(MyPC);
				}

				
				if(SpawnParameter.Attributes)
				{
					UnitBase->DefaultAttributeEffect = SpawnParameter.Attributes;
				}
			
				UnitBase->InitializeAttributes();

				int32 Index;
				
				Index = FindMatchingIndex(SpawnParameterIdArray, SpawnParameter.Id);
				AddUnitIndexAndAssignToAllUnitsArrayWithIndex(UnitBase, Index, SpawnParameter);

				FUnitSpawnData UnitSpawnDataSet;
				UnitSpawnDataSet.Id = SpawnParameter.Id;
				UnitSpawnDataSet.UnitBase = UnitBase;
				UnitSpawnDataSet.SpawnParameter = SpawnParameter;

				UnitBase->ScheduleDelayedNavigationUpdate();
				
				UnitSpawnDataSets.Add(UnitSpawnDataSet);
				
			}
			
		}
	}
	// Enemyspawn
}

int ARTSGameModeBase::AssignNewHighestIndex(AUnitBase* Unit)
{
	HighestUnitIndex++;
	Unit->SetUnitIndex(HighestUnitIndex);
	return HighestUnitIndex;
}

int ARTSGameModeBase::AddUnitIndexAndAssignToAllUnitsArray(AUnitBase* UnitBase)
{
	int Index = AssignNewHighestIndex(UnitBase);
	AllUnits.Add(UnitBase);
	return Index;
}

void ARTSGameModeBase::AddUnitIndexAndAssignToAllUnitsArrayWithIndex(AUnitBase* UnitBase, int32 Index, FUnitSpawnParameter SpawnParameter)
{
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();

	if(PlayerController)
	{

			if(Index == INDEX_NONE || !AvailableUnitIndexArray.Num() || !AvailableUnitIndexArray[Index])
			{
				AssignNewHighestIndex(UnitBase);
				AllUnits.Add(UnitBase);
		
			}else
			{
				UnitBase->SetUnitIndex(AvailableUnitIndexArray[Index]);

				if(SpawnParameter.LoadLevelAfterSpawn)
				{
					UnitBase->LoadAbilityAndLevelData(FString::FromInt(AvailableUnitIndexArray[Index]));
				}
				AvailableUnitIndexArray.RemoveAt(Index);
				SpawnParameterIdArray.RemoveAt(Index);
				AllUnits.Add(UnitBase);
			
		}
	}
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

