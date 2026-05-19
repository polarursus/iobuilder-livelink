// IO Builder LiveLink - Lens controller implementation.

#include "LiveLinkIOBLensController.h"

#include "LiveLinkIOBLensRole.h"
#include "LiveLinkIOBLensTypes.h"

#include "LensComponent.h"
#include "LensDistortionModelHandlerBase.h"
#include "LensData.h"

void ULiveLinkIOBLensController::Tick(float DeltaTime, const FLiveLinkSubjectFrameData& SubjectData)
{
	const FLiveLinkIOBLensStaticData* StaticData =
		SubjectData.StaticData.Cast<FLiveLinkIOBLensStaticData>();
	const FLiveLinkIOBLensFrameData* FrameData =
		SubjectData.FrameData.Cast<FLiveLinkIOBLensFrameData>();
	if (!StaticData || !FrameData)
	{
		return;
	}

	ULensComponent* LensComponent = Cast<ULensComponent>(GetAttachedComponent());
	if (!LensComponent)
	{
		return;
	}

	// The project supplies the distortion handler (a Spherical lens model on
	// the Lens Component / Lens File). We only stream the live state into it;
	// if none is set yet there is nothing to drive (camera still works).
	ULensDistortionModelHandlerBase* Handler = LensComponent->GetLensDistortionHandler();
	if (!Handler)
	{
		return;
	}

	// Normalized focal length: prefer the calibrated sensor size; fall back
	// to the horizontal FOV when no CameraData was streamed.
	double Fx = 0.0;
	double Fy = 0.0;
	if (StaticData->SensorWidthMM > 0.0f && FrameData->FocalLengthMM > 0.0f)
	{
		Fx = FrameData->FocalLengthMM / StaticData->SensorWidthMM;
		Fy = (StaticData->SensorHeightMM > 0.0f)
			? FrameData->FocalLengthMM / StaticData->SensorHeightMM
			: Fx;
	}
	else if (FrameData->Fov > 0.0f)
	{
		const double HalfFovRad = FMath::DegreesToRadians(FrameData->Fov) * 0.5;
		Fx = 0.5 / FMath::Max(FMath::Tan(HalfFovRad), KINDA_SMALL_NUMBER);
		const double Aspect = (StaticData->ImageWidth > 0 && StaticData->ImageHeight > 0)
			? (double)StaticData->ImageWidth / (double)StaticData->ImageHeight
			: 1.0;
		Fy = Fx * Aspect;
	}
	else
	{
		return; // no usable intrinsics this frame
	}

	FLensDistortionState State;
	// Spherical (Brown-Conrady) parameter order: K1, K2, K3, P1, P2.
	State.DistortionInfo.Parameters = {
		FrameData->K1, FrameData->K2, FrameData->K3, FrameData->P1, FrameData->P2
	};
	State.FocalLengthInfo.FxFy = FVector2D(Fx, Fy);
	// cx/cy are normalized offsets from centre (0 = centre); Unreal's image
	// centre is normalized with 0.5 = centre.
	State.ImageCenter.PrincipalPoint =
		FVector2D(0.5 + FrameData->Cx, 0.5 + FrameData->Cy);

	Handler->SetDistortionState(State);
}

bool ULiveLinkIOBLensController::IsRoleSupported(const TSubclassOf<ULiveLinkRole>& RoleToSupport)
{
	return RoleToSupport == ULiveLinkIOBLensRole::StaticClass();
}

TSubclassOf<UActorComponent> ULiveLinkIOBLensController::GetDesiredComponentClass() const
{
	return ULensComponent::StaticClass();
}
