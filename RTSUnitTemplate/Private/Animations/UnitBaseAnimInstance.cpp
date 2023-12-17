// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Animations/UnitBaseAnimInstance.h"

#include "Characters/Unit/SpeakingUnit.h"
#include "Characters/Unit/UnitBase.h"
#include "Characters/Unit/SpeakingUnit.h"
#include "Core/UnitData.h"
#include "Net/UnrealNetwork.h"

UUnitBaseAnimInstance::UUnitBaseAnimInstance() {
	CharAnimState = UnitData::Idle;
}


void UUnitBaseAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

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

	if (OwningActor != nullptr) {
		AUnitBase* UnitBase = Cast<AUnitBase>(OwningActor);
		if (UnitBase != nullptr) {
			CharAnimState = UnitBase->GetUnitState();
			SetBlendPoints(UnitBase, Deltaseconds);

			if(LastAnimState != CharAnimState)
			{
				SoundTimer = 0.f;
				LastAnimState = CharAnimState;
			}
			
			if(Sound && UnitBase)
			{
				if(SoundTimer == 0.f)
					UGameplayStatics::PlaySoundAtLocation(UnitBase, Sound, UnitBase->GetActorLocation(), 1.f);

				SoundTimer += Deltaseconds;
				
			}
			
			
			if(abs(CurrentBlendPoint_1-BlendPoint_1) <= Resolution_1) CurrentBlendPoint_1 = BlendPoint_1;
			else if(CurrentBlendPoint_1 < BlendPoint_1) CurrentBlendPoint_1 += TransitionRate_1;
			else if(CurrentBlendPoint_1 > BlendPoint_1) CurrentBlendPoint_1 += TransitionRate_1*(-1);

			if(abs(CurrentBlendPoint_2-BlendPoint_2) <= Resolution_2) CurrentBlendPoint_2 = BlendPoint_2;
			else if(CurrentBlendPoint_2 < BlendPoint_2) CurrentBlendPoint_2 += TransitionRate_2;
			else if(CurrentBlendPoint_2 > BlendPoint_2) CurrentBlendPoint_2 += TransitionRate_2*(-1);

			
		}
	}
}


void UUnitBaseAnimInstance::SetBlendPoints(AUnitBase* Unit, float Deltaseconds)
{
	TEnumAsByte<UnitData::EState> AnimState = Unit->GetUnitState();
	
	if (AnimDataTable)
	{
		for(auto it : AnimDataTable->GetRowMap())
		{
			FString Key = it.Key.ToString();
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
			}else
			{
				ASpeakingUnit* SpeakingUnit = Cast<ASpeakingUnit>(Unit);

				if(SpeakingUnit && SpeakingUnit->SpeechBubble)
				{
					SpeakingUnit->SpeechBubble->AnimationTime += Deltaseconds;
					if(SpeakingUnit->SpeechBubble->AnimationTime <= SpeakingUnit->SpeechBubble->MaxAnimationTime)
					{
						BlendPoint_1 = SpeakingUnit->SpeechBubble->BlendPoint_1;
						BlendPoint_2 = SpeakingUnit->SpeechBubble->BlendPoint_2;
					}else
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

}
