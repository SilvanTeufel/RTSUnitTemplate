#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MapMarkerWidget.generated.h"

class UTextBlock;

/**
 * A simple widget to display a text label in world space.
 */
UCLASS()
class RTSUNITTEMPLATE_API UMapMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Sets the text for the marker label */
	void SetMarkerText(const FText& InText);

protected:
	UPROPERTY(meta = (BindWidget))
	UTextBlock* MarkerText;
};
