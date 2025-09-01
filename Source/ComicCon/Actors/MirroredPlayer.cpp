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

    // 1) �ո� �Ÿ� ����
    UpdateHandDistanceFromLatestPoses();

    // 2) ���� ���� ����: 250 �̸��� ���� Sword
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
        // �ո��� �� ã���� 0���� ����(���ϸ� "����" ��å���� �ٲ㵵 ��)
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

    // "�� �ո� + �� ���"�� ��� ��ȿ�� ��� �� ���� �ŷڵ� ���� ��� ����
    int32 BestIdx = INDEX_NONE;
    float BestScore = -FLT_MAX;

    for (int32 i = 0; i < LatestPoses.Num(); ++i)
    {
        const FPersonPose& P = LatestPoses[i];

        // �ε���/��ȿ�� üũ
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

        // ����: �� ����Ʈ�� confidence ��(������ 1�� ��ü)
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

    // ���õ� ���: "���=1"�� ����ȭ�� �� �� �Ÿ� ���
    const FPersonPose& PBest = LatestPoses[BestIdx];
    const FVector2f LWr = PBest.XY[COCO_LWR];
    const FVector2f RWr = PBest.XY[COCO_RWR];
    const FVector2f LSh = PBest.XY[COCO_LSH];
    const FVector2f RSh = PBest.XY[COCO_RSH];

    const float ShoulderLen = FVector2f::Distance(LSh, RSh);
    if (ShoulderLen <= KINDA_SMALL_NUMBER) return false; // �и� ��ȣ

    const float HandDist = FVector2f::Distance(LWr, RWr);

    // ����ȭ: ��� ���̸� 1�� �� �� �� �Ÿ�(������ ����)
    OutDistance = HandDist / ShoulderLen;
    return true;
}

void AMirroredPlayer::SetHandDistance(float InHandDistance)
{
    HandDistance = InHandDistance;
}