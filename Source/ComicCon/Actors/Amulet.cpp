// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/Amulet.h"
#include "Actors/MirroredPlayer.h"
#include "Components/PoseUdpReceiverComponent.h"
#include "Components/SingleSwingClassifierComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Actors/AmuletAttack.h"
#include "Utils.h"



AAmulet::AAmulet()
{
    PrimaryActorTick.bCanEverTick = true;
	VisibleWhenState = EWeaponState::Amulet;
}

void AAmulet::SetOwingSingleSwingClassifierComponent(USingleSwingClassifierComponent* InSingleSwingClassifierComponent)
{
	SingleSwingClassifierComponent = InSingleSwingClassifierComponent;
}

void AAmulet::BeginPlay()
{
	Super::BeginPlay();

	check(OwningPlayer);

	if (UPoseUdpReceiverComponent* Recv = OwningPlayer->GetPoseReceiver())
	{
		Recv->OnPoseBufferChanged.AddDynamic(this, &AAmulet::OnPoseInput);
	}

    if (SingleSwingClassifierComponent)
    {
        SingleSwingClassifierComponent->OnSingleSwingDetected.AddDynamic(this, &AAmulet::HandleSingleSwingDetected);
    }
}

// 한쪽 손(오/왼) 연장 지점 계산
bool AAmulet::TryComputeExtendedPoint(const FVector2f& Pelvis2D, const FPersonPose& Pose, const FTransform& OwnerXform, bool bRightHand, FVector& OutPoint, float* OutScore) const
{
    const int32 IdxElbow = bRightHand ? COCO_REL : COCO_LEL;
    const int32 IdxWrist = bRightHand ? COCO_RWR : COCO_LWR;

    if (!Pose.XY.IsValidIndex(IdxElbow) || !Pose.XY.IsValidIndex(IdxWrist))
        return false;

    const FVector2f El2D = Pose.XY[IdxElbow];
    const FVector2f Wr2D = Pose.XY[IdxWrist];
    if (!(IsFinite2D(El2D) && IsFinite2D(Wr2D)))
        return false;

    const FVector ElLoc = MakeLocalFrom2D(El2D, Pelvis2D, SingleSwingClassifierComponent->GetPixelToUU(), SingleSwingClassifierComponent->GetDepthOffsetX());
    const FVector WrLoc = MakeLocalFrom2D(Wr2D, Pelvis2D, SingleSwingClassifierComponent->GetPixelToUU(), SingleSwingClassifierComponent->GetDepthOffsetX());

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

    // DrawDebugLine(GetWorld(), ElW, OutPoint, bRightHand ? FColor::Cyan : FColor::Magenta, false, 0.f, 0, 1.5f);
    // DrawDebugPoint(GetWorld(), OutPoint, 6.f, FColor::Yellow, false, 0.f);

    return true;
}

void AAmulet::HandleSingleSwingDetected(TArray<FTimedPoseSnapshot> Snaps, TArray<int32> PersonIdxOf, bool bRightHand, int32 EnterIdx, int32 ExitIdx)
{

    if (OwningPlayer->GetCurrentWeapon() != EWeaponState::Amulet) return;

    UWorld* World = GetWorld();
    if (!World || Snaps.Num() < 3) return;
    if (PersonIdxOf.Num() != Snaps.Num()) return;

    // 1) 카메라/오너 축
    FVector CamLoc, CamFwd;
    if (!SingleSwingClassifierComponent || !SingleSwingClassifierComponent->GetCameraView(CamLoc, CamFwd)) return;

    // ★ 원본과 동일한 기준 트랜스폼 사용
    const FTransform OwnerXf =
        (SingleSwingClassifierComponent && SingleSwingClassifierComponent->GetOwner())
        ? SingleSwingClassifierComponent->GetOwner()->GetActorTransform()
        : GetActorTransform();

    // COCO 인덱스
    const int32 IdxWr = bRightHand ? COCO_RWR : COCO_LWR;
    const int32 IdxEl = bRightHand ? COCO_REL : COCO_LEL;

    // 안전 클램프
    EnterIdx = FMath::Clamp(EnterIdx, 0, Snaps.Num() - 1);
    ExitIdx = FMath::Clamp(ExitIdx, 0, Snaps.Num() - 1);

    auto PersonAt = [&](int i)->const FPersonPose*
        {
            const int32 p = PersonIdxOf.IsValidIndex(i) ? PersonIdxOf[i] : INDEX_NONE;
            return (p == INDEX_NONE || !Snaps[i].Poses.IsValidIndex(p)) ? nullptr : &Snaps[i].Poses[p];
        };

    const FPersonPose* PEnter = PersonAt(EnterIdx);
    const FPersonPose* PExit = PersonAt(ExitIdx);
    if (!PEnter || !PExit) return;

    // 골반 기준 2D
    FVector2f Pelvis2D_Enter, Pelvis2D_Exit;
    if (!GetPelvis2D(*PEnter, Pelvis2D_Enter)) return;
    if (!GetPelvis2D(*PExit, Pelvis2D_Exit))  return;

    // 손목 2D
    if (!PEnter->XY.IsValidIndex(IdxWr) || !IsFinite2D(PEnter->XY[IdxWr])) return;
    if (!PExit->XY.IsValidIndex(IdxWr) || !IsFinite2D(PExit->XY[IdxWr]))  return;

    const FVector2f HandEnter2D = PEnter->XY[IdxWr];
    const FVector2f HandExit2D = PExit->XY[IdxWr];

    // ★ 원본과 동일한 매핑 파라미터 사용 (컴포넌트에서 가져오기)
    const bool  bInvertY = SingleSwingClassifierComponent ? SingleSwingClassifierComponent->GetIsInvertImageYToUp() : false;
    const float PixelToUU = SingleSwingClassifierComponent ? SingleSwingClassifierComponent->GetPixelToUU() : 1.f;
    const float DepthOffsetX = SingleSwingClassifierComponent ? SingleSwingClassifierComponent->GetDepthOffsetX() : 0.f;
    // (선택) 미러 X가 필요하다면 컴포넌트/세팅에서 플래그를 받아 추가하세요.

    // 2D→월드
    const FVector HandEnterW = ToWorldFrom2D(Pelvis2D_Enter, HandEnter2D, OwnerXf, PixelToUU, bInvertY, DepthOffsetX);
    const FVector HandExitW = ToWorldFrom2D(Pelvis2D_Exit, HandExit2D, OwnerXf, PixelToUU, bInvertY, DepthOffsetX);

    // 팔꿈치
    if (!PEnter->XY.IsValidIndex(IdxEl) || !IsFinite2D(PEnter->XY[IdxEl])) return;
    if (!PExit->XY.IsValidIndex(IdxEl) || !IsFinite2D(PExit->XY[IdxEl]))  return;

    const FVector ElbowEnterW = ToWorldFrom2D(Pelvis2D_Enter, PEnter->XY[IdxEl], OwnerXf, PixelToUU, bInvertY, DepthOffsetX);
    const FVector ElbowExitW = ToWorldFrom2D(Pelvis2D_Exit, PExit->XY[IdxEl], OwnerXf, PixelToUU, bInvertY, DepthOffsetX);

    // 진행 방향
    FVector DirMove = (HandExitW - HandEnterW).GetSafeNormal();
    if (DirMove.IsNearlyZero())
    {
        FVector ForearmEnter = (HandEnterW - ElbowEnterW);
        DirMove = ForearmEnter.Normalize() ? ForearmEnter : CamFwd;
    }

    // 스폰 위치
    const FVector ForearmEnterVec = (HandEnterW - ElbowEnterW);
    const float   ForearmLenEn = ForearmEnterVec.Size();
    const float   ForearmExtendRatio = 0.25f;
    const FVector SpawnLoc = (ForearmLenEn > KINDA_SMALL_NUMBER)
        ? (HandEnterW + ForearmEnterVec.GetSafeNormal() * (ForearmLenEn * ForearmExtendRatio))
        : HandEnterW;

    // 평면 축 (필요시 외적 순서 토글로 점검)
    FVector N = FVector::CrossProduct(CamFwd, DirMove).GetSafeNormal();
    if (N.IsNearlyZero())
    {
        N = FVector::CrossProduct(FVector::UpVector, DirMove).GetSafeNormal();
        if (N.IsNearlyZero()) N = FVector::RightVector;
    }
    const FVector Zaxis = FVector::CrossProduct(N, DirMove).GetSafeNormal();

    const FQuat RotFixed = FQuat(FMatrix(
        FPlane(N.X, N.Y, N.Z, 0.f),
        FPlane(DirMove.X, DirMove.Y, DirMove.Z, 0.f),
        FPlane(Zaxis.X, Zaxis.Y, Zaxis.Z, 0.f),
        FPlane(0, 0, 0, 1)));

    // ===== 스윕 경로: HandEnterW → HandExitW =====
    const int32 TotalPlanes = FMath::Clamp(FMath::Max(3, MinInterpPlanes), MinInterpPlanes, MaxInterpPlanes);
    const FVector HalfExtent(PlaneHalfThickness, PlaneHalfSize.X, PlaneHalfSize.Y);

    TSet<AActor*> UniqueActors;

    auto SpawnProjectileOnce = [&](const FVector& L, const FVector& D)
        {
            if (!AmuletAttackClass) return;

            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            Params.Owner = this;
            Params.Instigator = GetInstigator();

            if (AAmuletAttack* AmuletAttack = World->SpawnActor<AAmuletAttack>(AmuletAttackClass, L, D.Rotation(), Params))
            {
                if (AttackLifeSeconds > 0.f) AmuletAttack->SetLifeSpan(AttackLifeSeconds);
            }
        };

    // 한 발만 미리 발사
    if (!bSpawnAtEachPlane && AmuletAttackClass)
    {
        SpawnProjectileOnce(SpawnLoc, DirMove);
    }

    for (int32 k = 0; k < TotalPlanes; ++k)
    {
        const float t = (TotalPlanes == 1) ? 0.f : (float)k / (float)(TotalPlanes - 1);
        const FVector HandK = FMath::Lerp(HandEnterW, HandExitW, t);

        const FVector SpawnOffsetEnter = (ForearmLenEn > KINDA_SMALL_NUMBER)
            ? (ForearmEnterVec.GetSafeNormal() * (ForearmLenEn * ForearmExtendRatio))
            : FVector::ZeroVector;

        const FVector CenterK = HandK + SpawnOffsetEnter;

        OverlapPlaneOnce(CenterK, RotFixed, UniqueActors, DebugSingleSwingDrawTime);

        if (bSpawnAtEachPlane && AmuletAttackClass)
        {
            SpawnProjectileOnce(SpawnLoc, DirMove);
        }

        // 디버그 원하면 주석 해제
        DrawDebugDirectionalArrow(World, HandK, HandK + DirMove * 80.f, 15.f, FColor::Green, false, DebugSingleSwingDrawTime, 0, 1.5f);
        DrawDebugDirectionalArrow(World, HandK, HandK + N * 80.f, 15.f, FColor::Magenta, false, DebugSingleSwingDrawTime, 0, 1.5f);
        DrawDebugSphere(World, SpawnLoc, 4.f, 8, FColor::Yellow, false, DebugSingleSwingDrawTime);
    }

    // === 데미지 ===
    if (UniqueActors.Num() > 0)
    {
        AController* Inst = GetInstigatorController();
        for (AActor* Hit : UniqueActors)
        {
            UGameplayStatics::ApplyDamage(Hit, SingleSwingDamageAmount, Inst, this, DamageTypeClass);
        }
    }
}

void AAmulet::OnPoseInput(const FVector2f& Pelvis2D, const TArray<FPersonPose>& Poses, float PixelToUU, const FTransform& OwnerXform)
{
    // Amulet 상태일 때만 갱신하고 싶으면 게이트
    if (OwningPlayer && OwningPlayer->GetCurrentWeapon() != EWeaponState::Amulet) return;

    FVector Center;
    bool bOk = false;

	if (!SingleSwingClassifierComponent) return;

    switch (SingleSwingClassifierComponent->GetMainHand())
    {
    case EMainHand::Right:
        bOk = TryComputeExtendedPoint(Pelvis2D, Poses.Last(), OwnerXform, /*bRightHand=*/true, Center, nullptr);
        break;

    case EMainHand::Left:
        bOk = TryComputeExtendedPoint(Pelvis2D, Poses.Last(), OwnerXform, /*bRightHand=*/false, Center, nullptr);
        break;

    case EMainHand::Auto:
    default:
    {
        FVector RightP, LeftP;
        float ScoreR = -FLT_MAX, ScoreL = -FLT_MAX;
        const bool bR = TryComputeExtendedPoint(Pelvis2D, Poses.Last(), OwnerXform, true, RightP, &ScoreR);
        const bool bL = TryComputeExtendedPoint(Pelvis2D, Poses.Last(), OwnerXform, false, LeftP, &ScoreL);

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