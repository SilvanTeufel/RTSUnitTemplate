// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Animations/UnitBaseAnimInstance.h"

#include "Characters/Unit/SpeakingUnit.h"
#include "Characters/Unit/UnitBase.h"
#include "Core/UnitData.h"
#include "Net/UnrealNetwork.h"

UUnitBaseAnimInstance::UUnitBaseAnimInstance() {
	CharAnimState = UnitData::Idle;
}

void UUnitBaseAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	bHasLastActorLocation = false;
}

void UUnitBaseAnimInstance::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UUnitBaseAnimInstance, CharAnimState);
	DOREPLIFETIME(UUnitBaseAnimInstance, LastAnimState);
	
	DOREPLIFETIME(UUnitBaseAnimInstance, BlendPoint_1);
	DOREPLIFETIME(UUnitBaseAnimInstance, BlendPoint_2);
	DOREPLIFETIME(UUnitBaseAnimInstance, CurrentBlendPoint_1);
	DOREPLIFETIME(UUnitBaseAnimInstance, CurrentBlendPoint_2);
	DOREPLIFETIME(UUnitBaseAnimInstance, TransitionRate_1);
	DOREPLIFETIME(UUnitBaseAnimInstance, TransitionRate_2);
	DOREPLIFETIME(UUnitBaseAnimInstance, Resolution_1);
	DOREPLIFETIME(UUnitBaseAnimInstance, Resolution_2);
	DOREPLIFETIME(UUnitBaseAnimInstance, Sound);
	
	DOREPLIFETIME(UUnitBaseAnimInstance, SoundTimer);
	DOREPLIFETIME(UUnitBaseAnimInstance, AnimDataTable);
}

void UUnitBaseAnimInstance::NativeUpdateAnimation(float Deltaseconds)
{
	Super::NativeUpdateAnimation(Deltaseconds);
	AActor* OwningActor = GetOwningActor();

	if (OwningActor != nullptr)
	{
		AUnitBase* UnitBase = Cast<AUnitBase>(OwningActor);
		if (UnitBase != nullptr)
		{
			const bool bIsUnitOnViewport = UnitBase->IsOnViewport;
			CharAnimState = UnitBase->GetUnitState();
			UpdateLocomotionData(UnitBase, Deltaseconds);

			const bool bIsInLocomotion = bUseLocomotionBlendspaceInputs && IsLocomotionState(CharAnimState);

			// Detect state transition: leaving locomotion
			if (!bIsInLocomotion && bWasInLocomotionLastFrame)
			{
				SmoothedSpeed = 0.0f;
				SmoothedDirection = 0.0f;
			}
			bWasInLocomotionLastFrame = bIsInLocomotion;

			if (bIsInLocomotion)
			{
				// Locomotion path: smooth speed and direction, drive blend points directly
				const float TargetSpeed = LocomotionData.Speed;

				// Lerp speed with asymmetric rates: fast up, slow down
				const float SpeedInterp = (TargetSpeed > SmoothedSpeed)
					? LocomotionSpeedInterpUp
					: LocomotionSpeedInterpDown;
				SmoothedSpeed = FMath::FInterpTo(SmoothedSpeed, TargetSpeed, Deltaseconds, SpeedInterp);

				// Direction: interpolate while moving, snap to 0 when idle
				if (LocomotionData.bIsMoving)
				{
					const float RawTarget = LocomotionData.DirectionDegrees;
					SmoothedDirection = FMath::FInterpTo(SmoothedDirection, RawTarget, Deltaseconds, LocomotionDirectionInterp);
				}
				else if (SmoothedSpeed <= LocomotionIdleSpeedThreshold)
				{
					SmoothedDirection = 0.0f;
				}

				// Feed directly to blend points
				BlendPoint_1 = SmoothedDirection;
				BlendPoint_2 = SmoothedSpeed;
				CurrentBlendPoint_1 = SmoothedDirection;
				CurrentBlendPoint_2 = SmoothedSpeed;
				Sound = nullptr;
			}
			else
			{
				// Non-locomotion path: use data table, apply traditional interp to blend points
				SetBlendPoints(UnitBase, Deltaseconds);

				if(LastAnimState != CharAnimState)
				{
					SoundTimer = 0.f;
					LastAnimState = CharAnimState;
				}

				if(Sound && bIsUnitOnViewport)
				{
					if(SoundTimer == 0.f)
						UGameplayStatics::PlaySoundAtLocation(UnitBase, Sound, UnitBase->GetActorLocation(), 1.f);
					SoundTimer += Deltaseconds;
				}

				// Traditional blend point smoothing
				if(FMath::Abs(CurrentBlendPoint_1 - BlendPoint_1) <= Resolution_1)
					CurrentBlendPoint_1 = BlendPoint_1;
				else
					CurrentBlendPoint_1 = FMath::FInterpTo(CurrentBlendPoint_1, BlendPoint_1, Deltaseconds, FMath::Max(0.0f, TransitionRate_1));

				if(FMath::Abs(CurrentBlendPoint_2 - BlendPoint_2) <= Resolution_2)
					CurrentBlendPoint_2 = BlendPoint_2;
				else
					CurrentBlendPoint_2 = FMath::FInterpTo(CurrentBlendPoint_2, BlendPoint_2, Deltaseconds, FMath::Max(0.0f, TransitionRate_2));
			}

		}
	}
}

void UUnitBaseAnimInstance::UpdateLocomotionData(const AUnitBase* Unit, float DeltaSeconds)
{
	if (!Unit)
	{
		return;
	}

	const FVector CurrentLocation = Unit->GetActorLocation();
	FVector CurrentVelocity = Unit->GetVelocity();

	if (bHasLastActorLocation && DeltaSeconds > KINDA_SMALL_NUMBER)
	{
		const FVector DeltaLocation = CurrentLocation - LastActorLocation;
		const FVector EstimatedVelocity(
			DeltaLocation.X / DeltaSeconds,
			DeltaLocation.Y / DeltaSeconds,
			DeltaLocation.Z / DeltaSeconds);
		if (CurrentVelocity.SizeSquared2D() <= KINDA_SMALL_NUMBER && EstimatedVelocity.SizeSquared2D() > KINDA_SMALL_NUMBER)
		{
			CurrentVelocity = EstimatedVelocity;
		}
	}

	LastActorLocation = CurrentLocation;
	bHasLastActorLocation = true;

	LocomotionData.Rotation = Unit->GetActorRotation();
	LocomotionData.Velocity = CurrentVelocity;
	LocomotionData.Speed = CurrentVelocity.Size2D();
	LocomotionData.bIsMoving = LocomotionData.Speed > LocomotionIdleSpeedThreshold;

	if (LocomotionData.bIsMoving)
	{
		const FVector VelocityDir2D = CurrentVelocity.GetSafeNormal2D();
		const FVector Forward2D = Unit->GetActorForwardVector().GetSafeNormal2D();
		const FVector Right2D = Unit->GetActorRightVector().GetSafeNormal2D();

		const float ForwardDot = FVector::DotProduct(Forward2D, VelocityDir2D);
		const float RightDot = FVector::DotProduct(Right2D, VelocityDir2D);
		LocomotionData.DirectionDegrees = FMath::RadiansToDegrees(FMath::Atan2(RightDot, ForwardDot));
	}
	else
	{
		LocomotionData.DirectionDegrees = 0.0f;
	}
}

bool UUnitBaseAnimInstance::IsLocomotionState(TEnumAsByte<UnitData::EState> State) const
{
	return State == UnitData::Idle
		|| State == UnitData::Run
		|| State == UnitData::Patrol
		|| State == UnitData::PatrolRandom
		|| State == UnitData::PatrolIdle
		|| State == UnitData::Chase
		|| State == UnitData::EvasionChase
		|| State == UnitData::GoToBase
		|| State == UnitData::GoToBuild
		|| State == UnitData::GoToRepair
		|| State == UnitData::GoToResourceExtraction;
}

void UUnitBaseAnimInstance::SetBlendPoints(AUnitBase* Unit, float Deltaseconds)
{
	TEnumAsByte<UnitData::EState> AnimState = Unit->GetUnitState();
	
	if (AnimDataTable)
	{
		for(auto it : AnimDataTable->GetRowMap())
		{
			UnitAnimData = reinterpret_cast<FUnitAnimData*>(it.Value);
			if(UnitAnimData->AnimState == AnimState && UnitAnimData->AnimState != UnitData::Speaking)
			{
				BlendPoint_1 = UnitAnimData->BlendPoint_1;
				BlendPoint_2 = UnitAnimData->BlendPoint_2;
				TransitionRate_1 = UnitAnimData->TransitionRate_1;
				TransitionRate_2 = UnitAnimData->TransitionRate_2;
				Resolution_1 = UnitAnimData->Resolution_1;
				Resolution_2 = UnitAnimData->Resolution_2;
				Sound = UnitAnimData->Sound;
				break;
			}
			
			ASpeakingUnit* SpeakingUnit = Cast<ASpeakingUnit>(Unit);

			if(SpeakingUnit && SpeakingUnit->SpeechBubble)
			{
				SpeakingUnit->SpeechBubble->AnimationTime += Deltaseconds;
				if(SpeakingUnit->SpeechBubble->AnimationTime <= SpeakingUnit->SpeechBubble->MaxAnimationTime)
				{
					BlendPoint_1 = SpeakingUnit->SpeechBubble->BlendPoint_1;
					BlendPoint_2 = SpeakingUnit->SpeechBubble->BlendPoint_2;
				}
				else
				{
					BlendPoint_1 = UnitAnimData->BlendPoint_1;
					BlendPoint_2 = UnitAnimData->BlendPoint_2;
				}

				TransitionRate_1 = UnitAnimData->TransitionRate_1;
				TransitionRate_2 = UnitAnimData->TransitionRate_2;
				Resolution_1 = UnitAnimData->Resolution_1;
				Resolution_2 = UnitAnimData->Resolution_2;
			}
		}
	}


}

