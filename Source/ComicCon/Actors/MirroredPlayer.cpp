// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/MirroredPlayer.h"
#include "Components/PoseUdpReceiverComponent.h"
#include "Components/PoseClassifierComponent.h"
#include "Utils.h"

AMirroredPlayer::AMirroredPlayer()
{
	PrimaryActorTick.bCanEverTick = true;
	SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComponent"));
	RootComponent = SceneComponent;

	PoseReceiver = CreateDefaultSubobject<UPoseUdpReceiverComponent>(TEXT("PoseReceiver"));
	PoseClassifier = CreateDefaultSubobject<UPoseClassifierComponent>(TEXT("PoseClassifier"));
}

void AMirroredPlayer::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // 1) 손목 거리 갱신
    UpdateHandDistanceFromLatestPoses();

    // 2) 무기 상태 결정: 250 미만일 때만 Sword
    const EWeaponState NewState = (HandDistance < SwordHandDistanceThreshold) ? EWeaponState::Sword : EWeaponState::Amulet;
    SetCurrentWeapon(NewState);
}

void AMirroredPlayer::SetCurrentWeapon(EWeaponState NewCurrentWeapon)
{
    OnCurrentWeaponChanged.Broadcast(NewCurrentWeapon);
    CurrentWeapon = NewCurrentWeapon;
}

void AMirroredPlayer::UpdateHandDistanceFromLatestPoses()
{
    float Distance = 0.f;
    if (TryComputeHandDistance(Distance))
    {
        SetHandDistance(Distance);
    }
    else
    {
        // 손목을 못 찾으면 0으로 리셋(원하면 "유지" 정책으로 바꿔도 됨)
        SetHandDistance(0.f);
    }
}

bool AMirroredPlayer::TryComputeHandDistance(float& OutDistance) const
{
    OutDistance = 0.f;

    if (!PoseReceiver) return false;

    const TArray<FPersonPose>& LatestPoses = PoseReceiver->GetLatestPoses();
    if (LatestPoses.Num() == 0) return false;

    auto IsFinite2DLocal = [](const FVector2f& P)
        {
            return FMath::IsFinite(P.X) && FMath::IsFinite(P.Y);
        };

    // "양 손목 + 양 어깨"가 모두 유효한 사람 중 가장 신뢰도 높은 사람 선택
    int32 BestIdx = INDEX_NONE;
    float BestScore = -FLT_MAX;

    for (int32 i = 0; i < LatestPoses.Num(); ++i)
    {
        const FPersonPose& P = LatestPoses[i];

        // 인덱스/유효성 체크
        const bool bHasLWr = P.XY.IsValidIndex(COCO_LWR);
        const bool bHasRWr = P.XY.IsValidIndex(COCO_RWR);
        const bool bHasLSh = P.XY.IsValidIndex(COCO_LSH);
        const bool bHasRSh = P.XY.IsValidIndex(COCO_RSH);
        if (!(bHasLWr && bHasRWr && bHasLSh && bHasRSh)) continue;

        const FVector2f LWr = P.XY[COCO_LWR];
        const FVector2f RWr = P.XY[COCO_RWR];
        const FVector2f LSh = P.XY[COCO_LSH];
        const FVector2f RSh = P.XY[COCO_RSH];
        if (!(IsFinite2DLocal(LWr) && IsFinite2DLocal(RWr) && IsFinite2DLocal(LSh) && IsFinite2DLocal(RSh)))
            continue;

        // 점수: 네 포인트의 confidence 합(없으면 1로 대체)
        const float cLWr = (P.Conf.IsValidIndex(COCO_LWR) ? P.Conf[COCO_LWR] : 1.f);
        const float cRWr = (P.Conf.IsValidIndex(COCO_RWR) ? P.Conf[COCO_RWR] : 1.f);
        const float cLSh = (P.Conf.IsValidIndex(COCO_LSH) ? P.Conf[COCO_LSH] : 1.f);
        const float cRSh = (P.Conf.IsValidIndex(COCO_RSH) ? P.Conf[COCO_RSH] : 1.f);
        const float Score = cLWr + cRWr + cLSh + cRSh;

        if (Score > BestScore)
        {
            BestScore = Score;
            BestIdx = i;
        }
    }

    if (BestIdx == INDEX_NONE) return false;

    // 선택된 사람: "어깨=1"로 정규화한 손 간 거리 계산
    const FPersonPose& PBest = LatestPoses[BestIdx];
    const FVector2f LWr = PBest.XY[COCO_LWR];
    const FVector2f RWr = PBest.XY[COCO_RWR];
    const FVector2f LSh = PBest.XY[COCO_LSH];
    const FVector2f RSh = PBest.XY[COCO_RSH];

    const float ShoulderLen = FVector2f::Distance(LSh, RSh);
    if (ShoulderLen <= KINDA_SMALL_NUMBER) return false; // 분모 보호

    const float HandDist = FVector2f::Distance(LWr, RWr);

    // 정규화: 어깨 길이를 1로 본 손 간 거리(무차원 비율)
    OutDistance = HandDist / ShoulderLen;
    return true;
}

void AMirroredPlayer::SetHandDistance(float InHandDistance)
{
    HandDistance = InHandDistance;
}