// IO Builder LiveLink - controller that applies the streamed Lens role data
// to a Camera Calibration ULensComponent (UE 5.3+).

#pragma once

#include "CoreMinimal.h"
#include "LiveLinkControllerBase.h"
#include "LiveLinkIOBLensController.generated.h"

/**
 * Binds an "IOBuilder Lens" LiveLink subject to a ULensComponent and pushes
 * the live Brown-Conrady distortion + intrinsics into its distortion handler
 * each tick, so the engine renders the calibrated lens distortion. Optional:
 * if no Lens subject is present nothing is created and the camera behaves as
 * before.
 */
UCLASS()
class ULiveLinkIOBLensController : public ULiveLinkControllerBase
{
	GENERATED_BODY()

public:
	//~ Begin ULiveLinkControllerBase interface
	virtual void Tick(float DeltaTime, const FLiveLinkSubjectFrameData& SubjectData) override;
	virtual bool IsRoleSupported(const TSubclassOf<ULiveLinkRole>& RoleToSupport) override;
	virtual TSubclassOf<UActorComponent> GetDesiredComponentClass() const override;
	//~ End ULiveLinkControllerBase interface
};
