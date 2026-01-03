#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SoundControlWidget.generated.h"

UCLASS(BlueprintType, Blueprintable)
class RTSUNITTEMPLATE_API USoundControlWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "SoundControl")
	void SetDefaultSoundVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category = "SoundControl")
	float GetDefaultSoundVolume() const;

	UFUNCTION(BlueprintCallable, Category = "SoundControl")
	void SetMasterVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category = "SoundControl")
	float GetMasterVolume() const;
};
