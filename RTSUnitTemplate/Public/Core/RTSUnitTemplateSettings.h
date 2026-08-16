// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTSUnitTemplateSettings.generated.h"

/**
 * Project Settings -> Plugins -> RTS Unit Template.
 *
 * Values here are saved to DefaultGame.ini and ship with a packaged build, which is the difference to
 * the console variables: those live only for the current session unless someone hand-edits
 * DefaultEngine.ini's [SystemSettings].
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "RTS Unit Template"))
class RTSUNITTEMPLATE_API URTSUnitTemplateSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URTSUnitTemplateSettings();

	/** Shows up under "Plugins" instead of the crowded "Game" section. */
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/**
	 * Global time dilation applied when a game world starts. 1 = real time, 6 = six times faster.
	 * Used for AI test runs and RL recording: at real time a single useful match takes half an hour.
	 *
	 * The console variable `rts.ai.timescale` still works and OVERRIDES this whenever it is set to
	 * anything other than 1 - handy for trying a value without touching project settings.
	 * Note it is applied at world initialisation, so it takes effect on the NEXT PIE start, not the
	 * running one.
	 */
	UPROPERTY(config, EditAnywhere, Category = "AI|Testing",
	          meta = (DisplayName = "AI Time Scale", ClampMin = "0.1", UIMin = "1.0", UIMax = "20.0"))
	float AITimeScale = 1.f;

	/** Convenience accessor; never returns null (UDeveloperSettings are CDO-backed). */
	static const URTSUnitTemplateSettings* Get();
};
