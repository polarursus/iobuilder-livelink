// IO Builder LiveLink - Lens controller implementation.

#include "LiveLinkIOBLensController.h"

#include "LiveLinkIOBLensRole.h"
#include "LiveLinkIOBLensTypes.h"

#include "LensComponent.h"
#include "LensData.h"
#include "Models/SphericalLensModel.h"

DEFINE_LOG_CATEGORY_STATIC(LogIOBLens, Log, All);

void ULiveLinkIOBLensController::OnEvaluateRegistered()
{
	bSetupApplied = false;
}

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

	// First valid tick: make sure a Spherical handler exists and the
	// component is in a mode that will pump our DistortionState into it.
	// Only set what the user hasn't configured; don't override their choices.
	if (!bSetupApplied)
	{
		if (!LensComponent->GetLensModel())
		{
			LensComponent->SetLensModel(USphericalLensModel::StaticClass());
		}
		if (LensComponent->GetDistortionSource() == EDistortionSource::LensFile)
		{
			// LensFile mode would overwrite our state with file-evaluated
			// values every tick, defeating the LiveLink path. Warn once.
			UE_LOG(LogIOBLens, Warning,
				TEXT("ULiveLinkIOBLensController: LensComponent DistortionSource is 'LensFile'; "
					 "live distortion will be ignored. Set it to 'Live Link Lens Subject' or 'Manual'."));
		}
		bSetupApplied = true;
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

	// Drive the LensComponent's own DistortionState member. Its TickComponent
	// runs as a tick-prerequisite-dependent of the LiveLink controller, so it
	// will pick up this value the same frame and pump it into the handler
	// (and the displacement-map render). Writing the handler directly is
	// pointless: LensComponent overwrites it with its own member every tick.
	LensComponent->SetDistortionState(State);
}

bool ULiveLinkIOBLensController::IsRoleSupported(const TSubclassOf<ULiveLinkRole>& RoleToSupport)
{
	return RoleToSupport == ULiveLinkIOBLensRole::StaticClass();
}

TSubclassOf<UActorComponent> ULiveLinkIOBLensController::GetDesiredComponentClass() const
{
	return ULensComponent::StaticClass();
}
