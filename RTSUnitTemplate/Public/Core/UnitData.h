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
