// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PoseUdpReceiverComponent.generated.h"


// COCO ÀÎµ¦½º »ó¼ö
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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPoseBufferChanged, const FVector2f&, Pelvis2D, const FPersonPose&, Pose, const FTransform&, OwnerXform);

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
    uint64 GetLatestTimestamp() const { return LatestTimestamp; }

private:
    FSocket* Socket = nullptr;
    TArray<uint8> RecvBuffer;

    TArray<FPersonPose> LatestPoses;
    uint64 LatestTimestamp = 0;

    bool InitSocket(int32 Port = 7777);
    void CloseSocket();
    bool ReceiveOnce(TArray<FPersonPose>& OutPoses, uint64& OutTsMs);

// Weapon Section
public:
    FOnPoseBufferChanged OnPoseBufferChanged;

};
