// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/TalentedUnit.h"

#include "Characters/UnitBase.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
//#include "Widgets/UnitTimerWidget.h"

ATalentedUnit::ATalentedUnit(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer) // Pass initializer to intermediate class
{
	// ATalentedUnit initialization code
	//TimerWidgetComp = ObjectInitializer.CreateDefaultSubobject<UWidgetComponent>(this, TEXT("Timer"));
	//TimerWidgetComp->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
}

void ATalentedUnit::BeginPlay()
{
	Super::BeginPlay();
	//SetupTimerWidget();
	if(DisableSaveGame)// DisableSaveGame
	{
		CreateTalentData();
	}else
	{
		TalentDataArray = LoadTalentPoints();

	
		SetTalentPoints();
		if(!TalentDataArray.Num())
		{
			CreateTalentData();
		}	
	}
}

void ATalentedUnit::CreateTalentData()
{
	if (TalentTable)
	{
		// Iterate through the WeaponTalentTable
		TMap<FName, uint8*> RowMap = TalentTable->GetRowMap();
		for (auto& Pair : RowMap)
		{
			FName RowName = Pair.Key;
			uint8* RowData = Pair.Value;

			// Convert the row data to the appropriate struct
			FClassTalents* ClassTalents = reinterpret_cast<FClassTalents*>(RowData);

			// Create a new FWeaponTalentData instance
			FClassTalentData TalentData;

			// Assign the WeaponTalents to the Talents property
			TalentData.Talents = *ClassTalents;

			// Create a new FWeaponTalentPoints instance and initialize its properties
			FClassTalentPoints TalentPoints;
			TalentPoints.ClassId = ClassId; //ClassTalents->ClassIds;
			TalentPoints.RangeOffset = 0;
			TalentPoints.WalkSpeedOffset = 0;
			TalentPoints.AttackSpeedScaler = 0;
			TalentPoints.PiercedTargetsOffset = 0;
			TalentPoints.ProjectileScaler = 0;
			TalentPoints.ProjectileSpeedScaler = 0;
			TalentPoints.ProjectileCount = 0;
			TalentPoints.HealthReg = 0;
			TalentPoints.ShieldReg = 0;
			TalentPoints.Ultimate = 0;
			// Assign other properties similarly

			// Assign the TalentPoints to the TalentPoints property
			TalentData.TalentPoints = TalentPoints;
			TalentDataArray.Add(TalentData);


			// Add the TalentData to the WeaponTalentDataArray
	
			
		
			AvailableTalentPoints.Add(MaxAvailableTalentPoints);
		}
	}
	//return 1;
}


void ATalentedUnit::IncrementTalentPoint(const FString& TalentKey)
{
	// Iterate through the WeaponTalentDataArray
	if(TalentDataArray.Num())
	for (FClassTalentData& TalentData : TalentDataArray)
	{
		// Check if the WeaponId matches
		if (TalentData.Talents.ClassIds.Contains(ClassId) && AvailableTalentPoints.Num() && AvailableTalentPoints[ClassId] > 0)
		{
			// Check the talent key and increment the corresponding talent point
			if (TalentKey == "RangeOffset" && TalentData.TalentPoints.RangeOffset < MaxTalentPoints)
			{
				TalentData.TalentPoints.RangeOffset += 1;
				AvailableTalentPoints[ClassId]-=1;
				Attributes->SetRange(Attributes->GetRange() +TalentData.Talents.RangeOffset);
			}
			else if (TalentKey == "WalkSpeedOffset" && TalentData.TalentPoints.WalkSpeedOffset < MaxTalentPoints)
			{
				TalentData.TalentPoints.WalkSpeedOffset += 1;
				AvailableTalentPoints[ClassId]-=1;
				UCharacterMovementComponent* MovementPtr = GetCharacterMovement();
				if(MovementPtr)
				SetWalkSpeed(MovementPtr->MaxWalkSpeed+TalentData.Talents.WalkSpeedOffset);
				WalkSpeedOffset = WalkSpeedOffset+TalentData.Talents.WalkSpeedOffset;
			}
			else if (TalentKey == "AttackSpeedScaler" && TalentData.TalentPoints.AttackSpeedScaler < MaxTalentPoints)
			{
				TalentData.TalentPoints.AttackSpeedScaler += 1;
				AvailableTalentPoints[ClassId]-=1;
				//AttackDuration = AttackDuration*TalentData.Talents.AttackSpeedScaler;
				PauseDuration = PauseDuration*TalentData.Talents.AttackSpeedScaler;
			}
			else if (TalentKey == "PiercedTargetsOffset" && TalentData.TalentPoints.PiercedTargetsOffset < MaxTalentPoints)
			{
				TalentData.TalentPoints.PiercedTargetsOffset += 1;
				AvailableTalentPoints[ClassId]-=1;
				PiercedTargetsOffset = PiercedTargetsOffset +TalentData.Talents.PiercedTargetsOffset;
			}
			else if (TalentKey == "ProjectileScaler" && TalentData.TalentPoints.ProjectileScaler < MaxTalentPoints)
			{
				TalentData.TalentPoints.ProjectileScaler += 1;
				AvailableTalentPoints[ClassId]-=1;
				ProjectileScale = ProjectileScale*TalentData.Talents.ProjectileScaler;
			}
			else if (TalentKey == "ProjectileSpeedScaler" && TalentData.TalentPoints.ProjectileSpeedScaler < MaxTalentPoints)
			{
				TalentData.TalentPoints.ProjectileSpeedScaler += 1;
				AvailableTalentPoints[ClassId]-=1;
				Attributes->SetProjectileSpeed(Attributes->GetProjectileSpeed()*TalentData.Talents.ProjectileSpeedScaler);
			}else if(TalentKey == "ProjectileCount" && TalentData.TalentPoints.ProjectileCount < MaxTalentPoints)
			{
				TalentData.TalentPoints.ProjectileCount += 1;
				AvailableTalentPoints[ClassId]-=1;
				ProjectileCount = ProjectileCount +TalentData.Talents.ProjectileCount;
			}else if(TalentKey == "ShieldReg" && TalentData.TalentPoints.HealthReg < MaxTalentPoints){
				TalentData.TalentPoints.HealthReg += 1;
				AvailableTalentPoints[ClassId]-=1;
				HealthReg = HealthReg+TalentData.Talents.HealthReg;
			}else if(TalentKey == "ShieldReg" && TalentData.TalentPoints.ShieldReg < MaxTalentPoints){
				TalentData.TalentPoints.ShieldReg += 1;
				AvailableTalentPoints[ClassId]-=1;
				ShieldReg = ShieldReg+TalentData.Talents.ShieldReg;
			}else if(TalentKey == "Ultimate" && TalentData.TalentPoints.Ultimate < MaxTalentPoints)
			{
				TalentData.TalentPoints.Ultimate += 1;
				AvailableTalentPoints[ClassId]-=1;
				Ultimate = TalentData.Talents.Ultimate;
			}

			// AttackDamage = Weapon->GetWeaponDamage(Weapons[ActualWeaponId]);
			// Break the loop since we found and updated the talent point
			break;
		}
	}
	SaveTalentPoints(TalentDataArray);
}

void ATalentedUnit::SetTalentPoints()
{
	// Iterate through the WeaponTalentDataArray

	if(TalentDataArray.Num())
	for (FClassTalentData& TalentData : TalentDataArray)
	{
		// Check if the WeaponId matches
		
		if (TalentData.Talents.ClassIds.Contains(ClassId))
		{
			// Check the talent key and increment the corresponding talent point
			Attributes->SetRange(Attributes->GetRange() +TalentData.TalentPoints.RangeOffset*TalentData.Talents.RangeOffset);
			WalkSpeedOffset = WalkSpeedOffset +TalentData.TalentPoints.WalkSpeedOffset*TalentData.Talents.WalkSpeedOffset;
			PiercedTargetsOffset = PiercedTargetsOffset+TalentData.TalentPoints.PiercedTargetsOffset*TalentData.Talents.PiercedTargetsOffset;
			ProjectileCount = ProjectileCount+TalentData.TalentPoints.ProjectileCount*TalentData.Talents.ProjectileCount;
			HealthReg = HealthReg+TalentData.TalentPoints.HealthReg*TalentData.Talents.HealthReg;
			ShieldReg = ShieldReg+TalentData.TalentPoints.ShieldReg*TalentData.Talents.ShieldReg;
			if(TalentData.TalentPoints.Ultimate)
			Ultimate = TalentData.Talents.Ultimate;
			PauseDuration =  PauseDuration * pow(TalentData.Talents.AttackSpeedScaler, TalentData.TalentPoints.AttackSpeedScaler);
			ProjectileScale = ProjectileScale * pow(TalentData.Talents.ProjectileScaler, TalentData.TalentPoints.ProjectileScaler);
			Attributes->SetProjectileSpeed(Attributes->GetProjectileSpeed() * pow(TalentData.Talents.ProjectileSpeedScaler, TalentData.TalentPoints.ProjectileSpeedScaler));

			
			break;
		}
	}

	SaveTalentPoints(TalentDataArray);
}

void ATalentedUnit::ResetTalentPoint()
{
	// Iterate through the WeaponTalentDataArray
	for (FClassTalentData& TalentData : TalentDataArray)
	{
		// Check if the WeaponId matches
		if (TalentData.Talents.ClassIds.Contains(ClassId))
		{
				TalentData.TalentPoints.RangeOffset = 0;
				TalentData.TalentPoints.WalkSpeedOffset = 0;
				TalentData.TalentPoints.AttackSpeedScaler  = 0;
				TalentData.TalentPoints.PiercedTargetsOffset = 0;
				TalentData.TalentPoints.ProjectileScaler = 0;
				TalentData.TalentPoints.ProjectileSpeedScaler  = 0;
				TalentData.TalentPoints.ProjectileCount = 0;
				TalentData.TalentPoints.HealthReg = 0;
				TalentData.TalentPoints.ShieldReg = 0;
				TalentData.TalentPoints.Ultimate = 0;
			
			
				SetTalentPoints();
				AvailableTalentPoints[ClassId] = MaxAvailableTalentPoints;
				// Handle unsupported talent key
				
			break;
		}

			// AttackDamage = Weapon->GetWeaponDamage(Weapons[ActualWeaponId]);
			// Break the loop since we found and updated the talent point

	}
	SaveTalentPoints(TalentDataArray);
}


void ATalentedUnit::AddTalentPoint(int Amount)
{
	for (int& TalentPoint : AvailableTalentPoints)
	{
		TalentPoint += Amount;
	}
}

void ATalentedUnit::SaveTalentPoints(const TArray<FClassTalentData>& DataArray)
{
	UTalentSaveGame* SaveGameInstance = Cast<UTalentSaveGame>(UGameplayStatics::CreateSaveGameObject(UTalentSaveGame::StaticClass()));

	if (SaveGameInstance)
	{
		SaveGameInstance->WeaponTalentDataArray = DataArray;
		SaveGameInstance->AvailableTalentPoints = AvailableTalentPoints;
		UGameplayStatics::SaveGameToSlot(SaveGameInstance, TEXT("TalentsSave0100"), 0);
	}
}

TArray<FClassTalentData> ATalentedUnit::LoadTalentPoints()
{
	UTalentSaveGame* SaveGameInstance = Cast<UTalentSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("TalentsSave0100"), 0));

	if (SaveGameInstance)
	{
		AvailableTalentPoints = SaveGameInstance->AvailableTalentPoints;
		return SaveGameInstance->WeaponTalentDataArray;
	}

	// Return an empty array if no save game exists
	return TArray<FClassTalentData>();
}

/*
void ATalentedUnit::SetupTimerWidget()
{
	if (TimerWidgetComp) {

		TimerWidgetComp->SetRelativeLocation(TimerWidgetCompLocation, false, 0, ETeleportType::None);
		UUnitTimerWidget* Timerbar = Cast<UUnitTimerWidget>(TimerWidgetComp->GetUserWidgetObject());

		if (Timerbar) {
			Timerbar->SetOwnerActor(this);
		}
	}
}*/
