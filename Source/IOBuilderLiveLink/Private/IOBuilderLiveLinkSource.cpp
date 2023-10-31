// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "IOBuilderLiveLinkSource.h"

#include "ILiveLinkClient.h"
#include "LiveLinkTypes.h"
#include "Roles/LiveLinkAnimationRole.h"
#include "Roles/LiveLinkAnimationTypes.h"

#include "Async/Async.h"
#include "Common/UdpSocketBuilder.h"
#include "HAL/RunnableThread.h"
#include "Json.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

#include "Roles/LiveLinkCameraTypes.h"
#include "Roles/LiveLinkCameraRole.h"
#include "Roles/LiveLinkLightRole.h"
#include "Roles/LiveLinkLightTypes.h"
#include "Roles/LiveLinkTransformRole.h"
#include "Roles/LiveLinkTransformTypes.h"

//enable logging step 2
DEFINE_LOG_CATEGORY(ModuleLog)

#define LOCTEXT_NAMESPACE "IOBuilderLiveLinkSource"

#define RECV_BUFFER_SIZE 1024 * 1024

FIOBuilderLiveLinkSource::FIOBuilderLiveLinkSource(FIPv4Endpoint InEndpoint)
: Socket(nullptr)
, Stopping(false)
, Thread(nullptr)
, WaitTime(FTimespan::FromMilliseconds(100))
{
	// defaults
	DeviceEndpoint = InEndpoint;

	SourceStatus = LOCTEXT("SourceStatus_DeviceNotFound", "Device Not Found");
	SourceType = LOCTEXT("IOBuilderLiveLinkSourceType", "IO Builder LiveLink");
	SourceMachineName = LOCTEXT("IOBuilderLiveLinkSourceMachineName", "localhost");

	if (DeviceEndpoint.Address.IsMulticastAddress())
	{
		Socket = FUdpSocketBuilder(TEXT("IOBLLSOCKET"))
			.AsNonBlocking()
			.AsReusable()
			.BoundToPort(DeviceEndpoint.Port)
			.WithReceiveBufferSize(RECV_BUFFER_SIZE)

			.BoundToAddress(FIPv4Address::Any)
			.JoinedToGroup(DeviceEndpoint.Address)
			.WithMulticastLoopback()
			.WithMulticastTtl(2);
					
	}
	else
	{
		Socket = FUdpSocketBuilder(TEXT("IOBLLSOCKET"))
			.AsNonBlocking()
			.AsReusable()
			.BoundToAddress(DeviceEndpoint.Address)
			.BoundToPort(DeviceEndpoint.Port)
			.WithReceiveBufferSize(RECV_BUFFER_SIZE);
	}

	RecvBuffer.SetNumUninitialized(RECV_BUFFER_SIZE);

	if ((Socket != nullptr) && (Socket->GetSocketType() == SOCKTYPE_Datagram))
	{
		SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

		Start();

		SourceStatus = LOCTEXT("SourceStatus_Receiving", "Receiving");
	}
}

FIOBuilderLiveLinkSource::~FIOBuilderLiveLinkSource()
{
	Stop();
	if (Thread != nullptr)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
	if (Socket != nullptr)
	{
		Socket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
	}
}

void FIOBuilderLiveLinkSource::ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid)
{
	Client = InClient;
	SourceGuid = InSourceGuid;
}


bool FIOBuilderLiveLinkSource::IsSourceStillValid() const
{
	bool bIsSourceValid = !Stopping && Thread != nullptr && Socket != nullptr;
	return bIsSourceValid;
}


bool FIOBuilderLiveLinkSource::RequestSourceShutdown()
{
	Stop();
	return true;
}

void FIOBuilderLiveLinkSource::Start()
{
	ThreadName = "IOBLL";
	ThreadName.AppendInt(FAsyncThreadIndex::GetNext());
	Thread = FRunnableThread::Create(this, *ThreadName, 128 * 1024, TPri_AboveNormal, FPlatformAffinity::GetPoolThreadMask());
}

void FIOBuilderLiveLinkSource::Stop()
{
	Stopping = true;
}

uint32 FIOBuilderLiveLinkSource::Run()
{
	TSharedRef<FInternetAddr> Sender = SocketSubsystem->CreateInternetAddr();
	
	while (!Stopping) {
		if (Socket->Wait(ESocketWaitConditions::WaitForRead, WaitTime))	{
			uint32 Size;

			while (Socket->HasPendingData(Size)) {
				int32 Read = 0;

				if (Socket->RecvFrom(RecvBuffer.GetData(), RecvBuffer.Num(), Read, *Sender)) {
					if (Read > 0) {
						TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> ReceivedData = MakeShareable(new TArray<uint8>());
						ReceivedData->SetNumUninitialized(Read);
						memcpy(ReceivedData->GetData(), RecvBuffer.GetData(), Read);
						AsyncTask(ENamedThreads::GameThread, [this, ReceivedData]() { HandleReceivedData(ReceivedData); });
					}
				}
			}
		}
	}
	return 0;
}

void FIOBuilderLiveLinkSource::HandleReceivedData(TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> ReceivedData) {
	FString JsonString;
	JsonString.Empty(ReceivedData->Num());
	for (uint8& Byte : *ReceivedData.Get()) {
		JsonString += TCHAR(Byte);
	}
	
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (FJsonSerializer::Deserialize(Reader, JsonObject)) {
		
		for (TPair<FString, TSharedPtr<FJsonValue>>& JsonField : JsonObject->Values) {
			FName SubjectName(*JsonField.Key);
			const TSharedPtr<FJsonObject> SubjectObject = JsonField.Value->AsObject();

			FString Role;
			if (!SubjectObject->TryGetStringField(TEXT("Role"), Role)){
				UE_LOG(ModuleLog, Error, TEXT("IO Builder LiveLink - 'Role' field is missing"));
				return;
			}

			bool bCreateSubject = !EncounteredSubjects.Contains(SubjectName);
			if (Role == "Camera"){
				if (bCreateSubject){
					UE_LOG(ModuleLog, Warning, TEXT("Creating Camera Subject"));
					FLiveLinkStaticDataStruct StaticDataStruct = FLiveLinkStaticDataStruct(FLiveLinkCameraStaticData::StaticStruct());
					FLiveLinkCameraStaticData& CameraData = *StaticDataStruct.Cast<FLiveLinkCameraStaticData>();					

					double d; bool b; 
					CameraData.bIsAspectRatioSupported = SubjectObject->TryGetNumberField(TEXT("ar"), d);
					CameraData.bIsFieldOfViewSupported = SubjectObject->TryGetNumberField(TEXT("fov"), d);
					CameraData.bIsFocalLengthSupported = SubjectObject->TryGetNumberField(TEXT("fl"), d);						
					CameraData.bIsProjectionModeSupported = SubjectObject->TryGetBoolField(TEXT("prm"), b);
					CameraData.bIsApertureSupported = SubjectObject->TryGetNumberField(TEXT("ap"), d);
					CameraData.bIsFocusDistanceSupported = SubjectObject->TryGetNumberField(TEXT("f"), d);
					
					//CameraData.FilmBackWidth = CameraStructure->filmBackWidth;
					//CameraData.FilmBackHeight = CameraStructure->filmBackHeight;

					Client->PushSubjectStaticData_AnyThread({ SourceGuid, SubjectName }, ULiveLinkCameraRole::StaticClass(), MoveTemp(StaticDataStruct));
					EncounteredSubjects.Add(SubjectName);
				} else {
					FLiveLinkFrameDataStruct FrameDataStruct = FLiveLinkFrameDataStruct(FLiveLinkCameraFrameData::StaticStruct());
					FLiveLinkCameraFrameData& FrameData = *FrameDataStruct.Cast<FLiveLinkCameraFrameData>();

					double x = 0.0; double y = 0.0; double z = 0.0; double w = 0.0;
					SubjectObject->TryGetNumberField(TEXT("tx"), x);
					SubjectObject->TryGetNumberField(TEXT("ty"), y);
					SubjectObject->TryGetNumberField(TEXT("tz"), z);
					FVector translation = FVector(x, y, z);

					x = 0.0; y = 0.0; z = 0.0; w = 1.0;
					SubjectObject->TryGetNumberField(TEXT("qx"), x);
					SubjectObject->TryGetNumberField(TEXT("qy"), y);
					SubjectObject->TryGetNumberField(TEXT("qz"), z);
					SubjectObject->TryGetNumberField(TEXT("qw"), w);
					FQuat quaternion = FQuat(x, y, z, w);

					x = 1.0; y = 1.0; z = 1.0;
					SubjectObject->TryGetNumberField(TEXT("sx"), x);
					SubjectObject->TryGetNumberField(TEXT("sy"), y);
					SubjectObject->TryGetNumberField(TEXT("sz"), z);
					FVector scale = FVector(x, y, z);

					FrameData.Transform = FTransform(quaternion, translation, scale);

					SubjectObject->TryGetNumberField(TEXT("ar"), FrameData.AspectRatio);
					SubjectObject->TryGetNumberField(TEXT("fov"), FrameData.FieldOfView);
					SubjectObject->TryGetNumberField(TEXT("fl"), FrameData.FocalLength);
					SubjectObject->TryGetNumberField(TEXT("ap"), FrameData.Aperture);
					SubjectObject->TryGetNumberField(TEXT("f"), FrameData.FocusDistance);
					
					bool isPerspective = true;
					SubjectObject->TryGetBoolField(TEXT("prm"), isPerspective);
					FrameData.ProjectionMode = isPerspective ? ELiveLinkCameraProjectionMode::Perspective : ELiveLinkCameraProjectionMode::Orthographic;													

					Client->PushSubjectFrameData_AnyThread({ SourceGuid, SubjectName }, MoveTemp(FrameDataStruct));
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
