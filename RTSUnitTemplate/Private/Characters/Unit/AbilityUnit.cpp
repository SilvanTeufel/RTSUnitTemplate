// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/AbilityUnit.h"

#include "AIController.h"
#include "GAS/AttributeSetBase.h"
#include "GAS/AbilitySystemComponentBase.h"
#include "GAS/GameplayAbilityBase.h"
#include "Characters/Unit/SpeakingUnit.h"
#include "Blueprint/UserWidget.h"
#include "Core/TalentSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound\SoundCue.h"

void AAbilityUnit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAbilityUnit, UnitState);
	DOREPLIFETIME(AAbilityUnit, UnitStatePlaceholder);
}

void AAbilityUnit::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	GetAbilitiesArrays();
	AutoAbility();
}

void AAbilityUnit::LevelUp_Implementation()
{
	//Super::LevelUp_Implementation();

	if(LevelData.CharacterLevel < LevelUpData.MaxCharacterLevel && LevelData.Experience > LevelUpData.ExperiencePerLevel*LevelData.CharacterLevel)
	{
		LevelData.CharacterLevel++;
		LevelData.TalentPoints += LevelUpData.TalentPointsPerLevel; // Define TalentPointsPerLevel as appropriate
		LevelData.Experience -= LevelUpData.ExperiencePerLevel*LevelData.CharacterLevel;
		// Trigger any additional level-up effects or logic here
		if(HasAuthority())
			AddAbilityPoint();
	}
}

void AAbilityUnit::TeleportToValidLocation(const FVector& Destination, float MaxZDifference)
{
	FVector Start = Destination + FVector(0.f, 0.f, 1000.f);
	FVector End = Destination - FVector(0.f, 0.f, 200.f);
	FHitResult HitResult;

	// Perform a line trace to check if the location is valid
	if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility))
	{
		// If the hit location is valid (e.g., on the ground), teleport to that location
		if (HitResult.bBlockingHit && abs(GetActorLocation().Z-HitResult.Location.Z) < MaxZDifference)
		{
			// Optionally, you might want to add additional checks on HitResult to ensure it's a valid surface
			SetActorLocation(FVector(HitResult.Location.X, HitResult.Location.Y, HitResult.Location.Z + 70.f));
			return;
		}
	}

	//SetActorLocation(GetActorLocation());
}

void AAbilityUnit::StartAcceleratingTowardsDestination(const FVector& NewDestination, const FVector& NewTargetVelocity, float NewAccelerationRate, float NewRequiredDistanceToStart)
{
	if (GetUnitState() == UnitData::Dead && FVector::Dist(GetActorLocation(), ChargeDestination) >= RequiredDistanceToStart && FVector::Dist(GetActorLocation(), ChargeDestination) <= Attributes->Range.GetCurrentValue())
		return;
	
	ChargeDestination = NewDestination;
	TargetVelocity = NewTargetVelocity;
	AccelerationRate = NewAccelerationRate;
	RequiredDistanceToStart = NewRequiredDistanceToStart;
	CurrentVelocity = FVector::ZeroVector;
	
	// Start a repeating timer which calls the Accelerate function
	GetWorld()->GetTimerManager().SetTimer(AccelerationTimerHandle, this, &AAbilityUnit::Accelerate, 0.1f, true);
}

void AAbilityUnit::Accelerate()
{
	if (GetUnitState() == UnitData::Dead)
		return;
	// If the actor is close enough to the destination, stop accelerating
	if(FVector::Dist(GetActorLocation(), ChargeDestination) <= Attributes->Range.GetCurrentValue()) //Attributes->Range.GetCurrentValue()+10.f
	{
		// Stop the character if it's past the destination
		//LaunchCharacter(FVector::ZeroVector, false, false);
		GetWorld()->GetTimerManager().ClearTimer(AccelerationTimerHandle);
	}else if (FVector::Dist(GetActorLocation(), ChargeDestination) <= RequiredDistanceToStart)
	{
		// Gradually increase CurrentVelocity towards TargetVelocity
		CurrentVelocity = FMath::VInterpTo(CurrentVelocity, TargetVelocity, GetWorld()->DeltaTimeSeconds, AccelerationRate);

		// Launch character with the current velocity
		LaunchCharacter(CurrentVelocity, true, true);
	}
	

	// If the current velocity is approximately equal to the target velocity, clear the timer
	if (CurrentVelocity.Equals(TargetVelocity, 1.0f))
	{
		GetWorld()->GetTimerManager().ClearTimer(AccelerationTimerHandle);
	}
}

void AAbilityUnit::StartAcceleratingFromDestination(const FVector& NewDestination, const FVector& NewTargetVelocity, float NewAccelerationRate, float NewRequiredDistanceToStart)
{
	if (GetUnitState() == UnitData::Dead && FVector::Dist(GetActorLocation(), ChargeDestination) >= RequiredDistanceToStart && FVector::Dist(GetActorLocation(), ChargeDestination) <= Attributes->Range.GetCurrentValue())
		return;
	
	ChargeDestination = NewDestination;
	TargetVelocity = NewTargetVelocity;
	AccelerationRate = NewAccelerationRate;
	RequiredDistanceToStart = NewRequiredDistanceToStart;
	CurrentVelocity = FVector::ZeroVector;
	
	// Start a repeating timer which calls the Accelerate function
	GetWorld()->GetTimerManager().SetTimer(AccelerationTimerHandle, this, &AAbilityUnit::AccelerateFrom, 0.1f, true);
}

void AAbilityUnit::AccelerateFrom()
{
	if (GetUnitState() == UnitData::Dead || !RequiredDistanceToStart)
		return;
	// If the actor is close enough to the destination, stop accelerating
	if(FVector::Dist(GetActorLocation(), ChargeDestination) <= 10.f) //Attributes->Range.GetCurrentValue()+10.f
	{
		// Stop the character if it's past the destination
		//LaunchCharacter(FVector::ZeroVector, false, false);
		GetWorld()->GetTimerManager().ClearTimer(AccelerationTimerHandle);
	}else if (FVector::Dist(GetActorLocation(), ChargeDestination) <= RequiredDistanceToStart)
	{
		// Gradually increase CurrentVelocity towards TargetVelocity
		CurrentVelocity = FMath::VInterpTo(CurrentVelocity, TargetVelocity, GetWorld()->DeltaTimeSeconds, AccelerationRate);

		// Launch character with the current velocity
		LaunchCharacter(CurrentVelocity, true, true);
	}
	

	// If the current velocity is approximately equal to the target velocity, clear the timer
	if (CurrentVelocity.Equals(TargetVelocity, 1.0f))
	{
		GetWorld()->GetTimerManager().ClearTimer(AccelerationTimerHandle);
	}
}

void AAbilityUnit::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (RegenerationTimer >= RegenerationDelayTime)
	{
		if (AutoApplyAbility && HasAuthority()) 
		{
			AutoAbility(); // Assuming AutoAbility is called here
		}
		RegenerationTimer = 0.f;
	}
	

}

void AAbilityUnit::GetAbilitiesArrays()
{
	
	if(HasAuthority() && AbilitySystemComponent)
	{

		if(OffensiveAbilities.Num())
		for(TSubclassOf<UGameplayAbilityBase>& StartupAbility : OffensiveAbilities)
		{
			if(StartupAbility)
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(StartupAbility, 1, static_cast<int32>(StartupAbility.GetDefaultObject()->AbilityInputID), this));
		}

		if(DefensiveAbilities.Num())
		for(TSubclassOf<UGameplayAbilityBase>& StartupAbility : DefensiveAbilities)
		{
			if(StartupAbility)
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(StartupAbility, 1, static_cast<int32>(StartupAbility.GetDefaultObject()->AbilityInputID), this));
		}

		if(AttackAbilities.Num())
		for(TSubclassOf<UGameplayAbilityBase>& StartupAbility : AttackAbilities)
		{
			if(StartupAbility)
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(StartupAbility, 1, static_cast<int32>(StartupAbility.GetDefaultObject()->AbilityInputID), this));
		}

		if(ThrowAbilities.Num())
		for(TSubclassOf<UGameplayAbilityBase>& StartupAbility : ThrowAbilities)
		{
			if(StartupAbility)
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(StartupAbility, 1, static_cast<int32>(StartupAbility.GetDefaultObject()->AbilityInputID), this));
		}
	}
}

void AAbilityUnit::SetUnitState(TEnumAsByte<UnitData::EState> NewUnitState)
{
	UnitState = NewUnitState;
}

TEnumAsByte<UnitData::EState> AAbilityUnit::GetUnitState() const
{
	return UnitState;
}

void AAbilityUnit::SetAutoAbilitySequence(int Index, int32 Value)
{
	AutoAbilitySequence[Index] = Value;
}

bool AAbilityUnit::IsAbilityAllowed(EGASAbilityInputID AbilityID, int AbilityIndex)
{

		switch (AbilityID)
		{
		case EGASAbilityInputID::AbilityOne:
		case EGASAbilityInputID::AbilityTwo:
		case EGASAbilityInputID::AbilityThree:
		case EGASAbilityInputID::AbilityFour:
		case EGASAbilityInputID::AbilityFive:
		case EGASAbilityInputID::AbilitySix:
			return LevelData.UsedAbilityPointsArray[AbilityIndex] < MaxAbilityPointsToInvest;
		default:
			return false;
		}

}

void AAbilityUnit::SpendAbilityPoints(EGASAbilityInputID AbilityID, int AbilityIndex)
{
	// Ensure the AbilityIndex is within the expected range before proceeding
	if (AbilityIndex < 0 || AbilityIndex >= LevelData.UsedAbilityPointsArray.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("SpendAbilityPoints called with an out of bounds AbilityIndex: %d"), AbilityIndex);
		return; // Exit the function as we have an invalid index
	}
	
	//UE_LOG(LogTemp, Log, TEXT("SpendAbilityPoints called with AbilityID: %d, Ability: %d"), static_cast<int32>(AbilityID), AbilityIndex);

	int32 AbilityIDIndex = static_cast<int32>(AbilityID) - static_cast<int32>(EGASAbilityInputID::AbilityOne);
	int32 AbilityCost = (1 + AbilityCostIncreaser*AbilityIDIndex);
	if (LevelData.AbilityPoints <= 0 || !IsAbilityAllowed(AbilityID, AbilityIndex) || LevelData.AbilityPoints < AbilityCost) return;

	//UE_LOG(LogTemp, Log, TEXT("Ability point spent. Remaining: %d, Used: %d"), LevelData.AbilityPoints, LevelData.UsedAbilityPoints);
	
	switch (AbilityIndex)
	{
	case 0:
		if(OffensiveAbilityID == EGASAbilityInputID::None) OffensiveAbilityID = AbilityID;
		else if(OffensiveAbilityID != AbilityID) return;
		break;
	case 1:
		if(DefensiveAbilityID == EGASAbilityInputID::None) DefensiveAbilityID = AbilityID;
		else if(DefensiveAbilityID != AbilityID) return;
		break;
	case 2:
		if(AttackAbilityID == EGASAbilityInputID::None) AttackAbilityID = AbilityID;
		else if(AttackAbilityID != AbilityID) return;
		break;
	case 3:
		if(ThrowAbilityID == EGASAbilityInputID::None) ThrowAbilityID = AbilityID;
		else if(ThrowAbilityID != AbilityID) return;
		break;

	default:
		return;
	}

	LevelData.AbilityPoints -= AbilityCost;
	LevelData.UsedAbilityPoints += AbilityCost;

	if(AbilityIndex <= 3)
		LevelData.UsedAbilityPointsArray[AbilityIndex] += AbilityCost;
}


int32 AAbilityUnit::DetermineAbilityID(int32 Level)
{

	int returnState = 99;
	const int32 X = LevelUpData.AbilityPointsEveryXLevel; // The interval for determining abilities
	if (Level % X == 0 && Level / X > 0)
	{
		returnState =  (Level / X) - 1;
	
		return AutoAbilitySequence[returnState];
	}

	return returnState;
}


void AAbilityUnit::AutoAbility()
{
	if((LevelData.CharacterLevel % LevelUpData.AbilityPointsEveryXLevel) == 0) // Every 5 levels
	{
		int Ability = DetermineAbilityID(LevelData.CharacterLevel);
		SpendAbilityPoints(EGASAbilityInputID::AbilityOne, Ability);
	}
}



void AAbilityUnit::AddAbilityPoint()
{
	if((LevelData.CharacterLevel % LevelUpData.AbilityPointsEveryXLevel) == 0) // Every 5 levels
	{
		LevelData.AbilityPoints++;
	}
}


void AAbilityUnit::SaveAbilityAndLevelData(const FString& SlotName)
{
	UTalentSaveGame* SaveGameInstance = Cast<UTalentSaveGame>(UGameplayStatics::CreateSaveGameObject(UTalentSaveGame::StaticClass()));

	if (SaveGameInstance)
	{
		SaveGameInstance->OffensiveAbilityID = OffensiveAbilityID;
		SaveGameInstance->DefensiveAbilityID = DefensiveAbilityID;
		SaveGameInstance->AttackAbilityID = AttackAbilityID;
		SaveGameInstance->ThrowAbilityID = ThrowAbilityID;
		/// LEVEL DATA ///
		SaveGameInstance->LevelData = LevelData;
		SaveGameInstance->LevelUpData = LevelUpData;
		SaveGameInstance->PopulateAttributeSaveData(Attributes);
		/////////////////////
		UGameplayStatics::SaveGameToSlot(SaveGameInstance, SlotName+"Ability", 0);
	}
}

void AAbilityUnit::LoadAbilityAndLevelData(const FString& SlotName)
{

	FString LoadSlot = SlotName+"Ability";
	UTalentSaveGame* SaveGameInstance = Cast<UTalentSaveGame>(UGameplayStatics::LoadGameFromSlot(LoadSlot, 0));

	if (SaveGameInstance)
	{
		OffensiveAbilityID = SaveGameInstance->OffensiveAbilityID;
		DefensiveAbilityID = SaveGameInstance->DefensiveAbilityID;
		AttackAbilityID = SaveGameInstance->AttackAbilityID;
		ThrowAbilityID = SaveGameInstance->ThrowAbilityID;
		LevelData = SaveGameInstance->LevelData;
		LevelUpData = SaveGameInstance->LevelUpData;
		Attributes->UpdateAttributes(SaveGameInstance->AttributeSaveData);
	}
	
}

void AAbilityUnit::ResetAbility()
{
	OffensiveAbilityID = EGASAbilityInputID::None;
	DefensiveAbilityID = EGASAbilityInputID::None;
	AttackAbilityID = EGASAbilityInputID::None;
	ThrowAbilityID = EGASAbilityInputID::None;
	LevelData.AbilityPoints = LevelData.UsedAbilityPoints+LevelData.AbilityPoints-AbilityResetPenalty;
	LevelData.UsedAbilityPoints = 0;
	LevelData.UsedAbilityPointsArray = { 0, 0, 0, 0, 0 };
}


void AAbilityUnit::RollRandomAbilitys()
{
	// Create an array of ability IDs
	TArray<EGASAbilityInputID> AbilityIDs = {
		EGASAbilityInputID::AbilityOne,
		EGASAbilityInputID::AbilityTwo,
		EGASAbilityInputID::AbilityThree,
		EGASAbilityInputID::AbilityFour
	};
	
	OffensiveAbilityID =  AbilityIDs[FMath::RandRange(0, AbilityIDs.Num() - 1)];
	DefensiveAbilityID = AbilityIDs[FMath::RandRange(0, AbilityIDs.Num() - 1)];
	AttackAbilityID = AbilityIDs[FMath::RandRange(0, AbilityIDs.Num() - 1)];
	ThrowAbilityID = AbilityIDs[FMath::RandRange(0, AbilityIDs.Num() - 1)];
}