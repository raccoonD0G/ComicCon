// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/Amulet.h"
#include "Actors/MirroredPlayer.h"
#include "Components/PoseUdpReceiverComponent.h"
#include "DrawDebugHelpers.h"
#include "Utils.h"
#include "Components/PoseClassifierComponent.h"

AAmulet::AAmulet()
{
    PrimaryActorTick.bCanEverTick = true;
	VisibleWhenState = EWeaponState::Amulet;
}

void AAmulet::BeginPlay()
{
	Super::BeginPlay();

	check(OwningPlayer);

	if (UPoseUdpReceiverComponent* Recv = OwningPlayer->GetPoseReceiver())
	{
		Recv->OnPoseBufferChanged.AddDynamic(this, &AAmulet::OnPoseInput);
	}
}

// 이미지 좌표 → 로컬(Y=오른쪽, Z=위) 변환
FVector AAmulet::MakeLocal(const FVector2f& P, const FVector2f& Pelvis) const
{
    const float dY = (P.X - Pelvis.X) * PixelToUU;
    const float dZpx = (P.Y - Pelvis.Y);
    const float dZ = (bInvertImageYToUp ? -dZpx : dZpx) * PixelToUU;
    return FVector(DepthOffsetX, dY, dZ); // X=깊이 고정
}

// 한쪽 손(오/왼) 연장 지점 계산
bool AAmulet::TryComputeExtendedPoint(const FVector2f& Pelvis2D, const FPersonPose& Pose, const FTransform& OwnerXform,
    bool bRightHand, FVector& OutPoint, float* OutScore) const
{
    const int32 IdxElbow = bRightHand ? COCO_REL : COCO_LEL;
    const int32 IdxWrist = bRightHand ? COCO_RWR : COCO_LWR;

    if (!Pose.XY.IsValidIndex(IdxElbow) || !Pose.XY.IsValidIndex(IdxWrist))
        return false;

    const FVector2f El2D = Pose.XY[IdxElbow];
    const FVector2f Wr2D = Pose.XY[IdxWrist];
    if (!(IsFinite2D(El2D) && IsFinite2D(Wr2D)))
        return false;

    const FVector ElLoc = MakeLocal(El2D, Pelvis2D);
    const FVector WrLoc = MakeLocal(Wr2D, Pelvis2D);

    const FVector ElW = OwnerXform.TransformPosition(ElLoc);
    const FVector WrW = OwnerXform.TransformPosition(WrLoc);

    const FVector Forearm = (WrW - ElW);
    OutPoint = WrW + Forearm * ExtendRatio;

    if (OutScore)
    {
        // 신뢰도 점수(있으면 conf 사용, 없으면 길이 사용)
        float cEl = 1.f, cWr = 1.f;
        if (Pose.Conf.IsValidIndex(IdxElbow)) cEl = Pose.Conf[IdxElbow];
        if (Pose.Conf.IsValidIndex(IdxWrist)) cWr = Pose.Conf[IdxWrist];
        *OutScore = cEl + cWr + 0.001f * Forearm.Size(); // conf 우선 + 미세 길이 가중
    }

    if (bDrawDebug)
    {
        DrawDebugLine(GetWorld(), ElW, OutPoint, bRightHand ? FColor::Cyan : FColor::Magenta, false, 0.f, 0, 1.5f);
        DrawDebugPoint(GetWorld(), OutPoint, 6.f, FColor::Yellow, false, 0.f);
    }

    return true;
}

void AAmulet::OnPoseInput(const FVector2f& Pelvis2D, const FPersonPose& Pose, const FTransform& OwnerXform)
{
    // Amulet 상태일 때만 갱신하고 싶으면 게이트
    if (OwningPlayer && OwningPlayer->GetCurrentWeapon() != EWeaponState::Amulet)
        return;

    FVector Center;
    bool bOk = false;

    if (UPoseClassifierComponent* PoseClassifierComponent = OwningPlayer->GetPoseClassifier())
    switch (PoseClassifierComponent->GetMainHand())
    {
    case EMainHand::Right:
        bOk = TryComputeExtendedPoint(Pelvis2D, Pose, OwnerXform, /*bRightHand=*/true, Center, nullptr);
        break;

    case EMainHand::Left:
        bOk = TryComputeExtendedPoint(Pelvis2D, Pose, OwnerXform, /*bRightHand=*/false, Center, nullptr);
        break;

    case EMainHand::Auto:
    default:
    {
        FVector RightP, LeftP;
        float ScoreR = -FLT_MAX, ScoreL = -FLT_MAX;
        const bool bR = TryComputeExtendedPoint(Pelvis2D, Pose, OwnerXform, true, RightP, &ScoreR);
        const bool bL = TryComputeExtendedPoint(Pelvis2D, Pose, OwnerXform, false, LeftP, &ScoreL);

        if (bR && (!bL || ScoreR >= ScoreL)) { Center = RightP; bOk = true; }
        else if (bL) { Center = LeftP;  bOk = true; }
        break;
    }
    }

    if (!bOk) return;

    SetAmuletPose(Center);
}

void AAmulet::SetAmuletPose(const FVector& CenterWorld)
{
    // 회전은 유지하고 위치만 갱신
    SetActorLocation(CenterWorld);
}