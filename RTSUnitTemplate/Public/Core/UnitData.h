// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/DataTable.h"
#include "Sound/SoundBase.h"
#include "UnitData.generated.h"


UENUM()
namespace UnitData
{
	enum EState
	{
		Idle   UMETA(DisplayName = "Idle"),
		Run  UMETA(DisplayName = "Run"),
		Patrol UMETA(DisplayName = "Patrol"),
		PatrolRandom UMETA(DisplayName = "PatrolRandom"),
		PatrolIdle UMETA(DisplayName = "PatrolIdle"),
		Jump   UMETA(DisplayName = "Jump"),
		Attack UMETA(DisplayName = "Attack"),
		Healing UMETA(DisplayName = "Healing"),
		Pause UMETA(DisplayName = "Pause"),
		Chase UMETA(DisplayName = "Chase"),
		IsAttacked UMETA(DisplayName = "IsAttacked") ,
		Speaking UMETA(DisplayName = "Speaking") ,
		Listening UMETA(DisplayName = "Listening") ,
		CustomStateOne UMETA(DisplayName = "CustomStateOne") ,
		CustomStateTwo UMETA(DisplayName = "CustomStateTwo") ,
		CustomStateThree UMETA(DisplayName = "CustomStateThree") ,
		CustomStateFour UMETA(DisplayName = "CustomStateFour") ,
		CustomStateFive UMETA(DisplayName = "CustomStateFive") ,
		CustomAbilityOne UMETA(DisplayName = "CustomAbilityOne") ,
		CustomAbilityTwo UMETA(DisplayName = "CustomAbilityTwo") ,
		CustomAbilityThree UMETA(DisplayName = "CustomAbilityThree") ,
		CustomAbilityFour UMETA(DisplayName = "CustomAbilityFour") ,
		CustomAbilityFive UMETA(DisplayName = "CustomAbilityFive") ,
		Dead UMETA(DisplayName = "Dead"),
		None UMETA(DisplayName = "None"),
	};

}

UENUM()
namespace CameraData
{
	enum CameraState
	{
		UseScreenEdges     UMETA(DisplayName = "UseScreenEdges"),
		MoveWASD  UMETA(DisplayName = "MoveWASD"),
		RotateToStart  UMETA(DisplayName = "RotateToStart"),
		MoveToPosition  UMETA(DisplayName = "MoveToPosition"),
		OrbitAtPosition  UMETA(DisplayName = "OrbitAtPosition"),
		OrbitAndMove UMETA(DisplayName = "OrbitAndMove"),
		ZoomIn  UMETA(DisplayName = "ZoomIn"),
		ZoomOut  UMETA(DisplayName = "ZoomOut"),
		ScrollZoomIn  UMETA(DisplayName = "ScrollZoomIn"),
		ScrollZoomOut  UMETA(DisplayName = "ScrollZoomOut"),
		ZoomOutPosition  UMETA(DisplayName = "ZoomOutPosition"),
		ZoomInPosition  UMETA(DisplayName = "ZoomInPosition"),
		HoldRotateLeft UMETA(DisplayName = "HoldRotateLeft"),
		HoldRotateRight UMETA(DisplayName = "HoldRotateRight"),
		RotateLeft UMETA(DisplayName = "RotateLeft"),
		RotateRight UMETA(DisplayName = "RotateRight"),
		LockOnCharacter UMETA(DisplayName = "LockOnCharacter"),
		LockOnSpeaking UMETA(DisplayName = "LockOnSpeaking"),
		ZoomToNormalPosition  UMETA(DisplayName = "ZoomToNormalPosition"),
		ThirdPerson UMETA(DisplayName = "ThirdPerson"),
		ZoomToThirdPerson UMETA(DisplayName = "RotateBeforeThirdPerson"),
	};
}

UENUM()
namespace UInvestmentData
{
	enum InvestmentState
	{
		None UMETA(DisplayName = "None"),
	   Stamina UMETA(DisplayName = "Stamina"),
	   AttackPower UMETA(DisplayName = "AttackPower"),
	   WillPower UMETA(DisplayName = "WillPower"),
	   Haste UMETA(DisplayName = "Haste"),
	   Armor UMETA(DisplayName = "Armor"),
	   MagicResistance UMETA(DisplayName = "MagicResistance")
	};

}

USTRUCT(BlueprintType)
struct FSpeechData_Texts : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	int Id;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate, meta = (MultiLine=true))
	FText Text;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	USoundBase* SpeechSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float BlendPoint_1 = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float BlendPoint_2 = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float Time = 5.f;
	
};

UENUM()
namespace SelectableData
{
	enum SelectableType
	{
		Health     UMETA(DisplayName = "Health"),
		Shield   UMETA(DisplayName = "Shield "),
		WeaponTalentPoint UMETA(DisplayName = "WeaponTalentPoint"),
	};
}

USTRUCT(BlueprintType)
struct FSpeechData_Buttons : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	int Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	int Text_Id;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate, meta = (MultiLine=true))
	FText Text;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	int New_Text_Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	USoundBase* ButtonSound;
};


USTRUCT(BlueprintType)
struct FLevelData
{
	GENERATED_BODY()

	// Experience points of the character
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	int32 Experience = 0;

	// Current level of the character
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	int32 CharacterLevel = 1;

	// Talent points available to the character
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	int32 TalentPoints = 0;

	// Talent points already used
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	int32 UsedTalentPoints = 0;
};

USTRUCT(BlueprintType)
struct FLevelUpData
{
	GENERATED_BODY()

	// Experience points required per level
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	int32 ExperiencePerLevel = 2;

	// Maximum level the character can achieve
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	int32 MaxCharacterLevel = 10;

	// Talent points awarded per level
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	int32 TalentPointsPerLevel = 5;

	// Maximum talents per stat
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	int32 MaxTalentsPerStat = 15;
};

inline float CheckAngle(FVector Vector, float Angle) {

	float X = Vector.X;
	float Y = Vector.Y;

	if (Vector.X < 0.001 && Vector.X > -0.001 && Vector.Y > 0.000) {
		Angle = 90;
	}
	else if (Vector.X < 0.001 && Vector.X > -0.001 && Vector.Y < 0.000) {
		Angle = 270;
	}
	else if (Vector.Y < 0.001 && Vector.Y > -0.001 && Vector.X > 0.000) {
		Angle = 0.;
	}
	else if (Vector.Y < 0.001f && Vector.Y > -0.001 && Vector.X < 0.000) {
		Angle = 180;
	}
	else if (Vector.X > 0.000 && Vector.Y > 0.000) {
		Angle += 0.000;
	}
	else if (Vector.X < 0.000 && Vector.Y > 0.000) {
		Angle += 0.000;
	}
	else if (Vector.X > 0.000 && Vector.Y < 0.000) {
		Angle = 360 - Angle;
	}
	else if (Vector.X < 0.000 && Vector.Y < 0.000) {
		Angle = 360 - Angle;
	}
	else {
	}

	return Angle;
};

inline float CalcAngle(FVector VectorOne, FVector VectorTwo)
{
	float AngleOneX = acosf(UKismetMathLibrary::Vector_CosineAngle2D(VectorOne, FVector(1, 0, 0))) * (180 / 3.1415926);
	AngleOneX = CheckAngle(VectorOne, AngleOneX);

	float AngleTwoX = acosf(UKismetMathLibrary::Vector_CosineAngle2D(VectorTwo, FVector(1, 0, 0))) * (180 / 3.1415926);
	AngleTwoX = CheckAngle(VectorTwo, AngleTwoX);

	float AngleDiff = AngleOneX - AngleTwoX;
	return AngleOneX - AngleTwoX;
};
