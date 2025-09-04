// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PoseUdpReceiverComponent.generated.h"


// COCO 인덱스 상수
static constexpr int32 COCO_LSH = 5;
static constexpr int32 COCO_RSH = 6;
static constexpr int32 COCO_LEL = 7;
static constexpr int32 COCO_REL = 8;
static constexpr int32 COCO_LWR = 9;
static constexpr int32 COCO_RWR = 10;
static constexpr int32 COCO_LHIP = 11;
static constexpr int32 COCO_RHIP = 12;

USTRUCT()
struct FPersonPose
{
    GENERATED_BODY()

public:
    uint16 PersonId;
    TArray<FVector2f> XY;   // 17
    TArray<float> Conf;     // 17
};

USTRUCT(BlueprintType)
struct FHandPose
{
    GENERATED_BODY()

public:
    uint16 PersonId = 0xFFFF; // 0xFFFF = unknown
    uint8  Which = 2;      // 0=left, 1=right, 2=unknown
    FVector2f Center = FVector2f(NAN, NAN);
    TArray<FVector2f> XY;     // 21
    TArray<float>     Conf;   // 21
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnPoseBufferChanged, const FVector2f&, Pelvis2D, const TArray<FPersonPose>&, Poses, float, PixelToUU, const FTransform&, OwnerXform);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnHandBufferChanged, const FVector2f&, Pelvis2D, const TArray<FHandPose>&, Hands, float, PixelToUU, const FTransform&, OwnerXform);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class COMICCON_API UPoseUdpReceiverComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
    UPoseUdpReceiverComponent();

public:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	const TArray<FPersonPose>& GetLatestPoses() const { return LatestPoses; }
    const TArray<FHandPose>& GetLatestHands() const { return LatestHands; }
	FORCEINLINE float GetPixelToUU() const { return PixelToUU; }
	FORCEINLINE float GetDepthOffsetX() const { return DepthOffsetX; }
    uint64 GetLatestTimestamp() const { return LatestTimestamp; }

private:
    FSocket* Socket = nullptr;
    TArray<uint8> RecvBuffer;

    TArray<FPersonPose> LatestPoses;
    TArray<FHandPose> LatestHands;

    uint64 LatestTimestamp = 0;

    float PixelToUU = 2.0f; // 픽셀→언리얼 유닛 스케일
    float DepthOffsetX = 0.f; // 액터 로컬 X(전방)으로 살짝 띄우기

    bool InitSocket(int32 Port = 7777);
    void CloseSocket();
    bool ReceiveOnce(TArray<FPersonPose>& OutPoses, TArray<FHandPose>& OutHands, uint64& OutTsMs);

// Weapon Section
public:
    FOnPoseBufferChanged OnPoseBufferChanged;

    FOnHandBufferChanged OnHandBufferChanged;

};
