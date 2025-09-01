// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/Sword.h"
#include "Actors/MirroredPlayer.h"
#include "Components/PoseUdpReceiverComponent.h"

inline float HalfLifeToAlpha(float HalfLife, float DeltaTime)
{
    if (HalfLife <= KINDA_SMALL_NUMBER) return 1.f;
    const float k = 0.69314718056f; // ln(2)
    return 1.f - FMath::Exp(-k * DeltaTime / HalfLife);
}

void ASword::BeginPlay()
{
	Super::BeginPlay();

    VisibleWhenState = EWeaponState::Sword;

    if (UPoseUdpReceiverComponent* Recv = OwningPlayer->GetPoseReceiver())
    {
        Recv->OnPoseBufferChanged.AddDynamic(this, &ASword::OnSwordPoseInput);
    }
}

void ASword::OnSwordPoseInput(const FVector2f& Pelvis2D, const FPersonPose& Pose, const FTransform& OwnerXform)
{
    FVector Dir, Start;
    if (TryComputeSwordPoseFromPose(Pelvis2D, Pose, OwnerXform, Dir, Start))
    {
        if (bInvertDirectionForSword) Dir *= -1.f; // 기존 좌표 정의 유지
        SwordDirectionWorld = Dir;
        SwordStartCenterWorld = Start;
        SetSwordPos(Dir, Start); // 위치/회전 적용(아래 함수)
    }
}

FVector ASword::MakeLocal(const FVector2f& P, const FVector2f& Pelvis) const
{
    // 우리 좌표: +Y=오른쪽, +Z=위 (이미지 Y가 아래로 증가하면 Z에 부호 반전)
    const float dY = (P.X - Pelvis.X) * PixelToUU;
    const float dZpx = (P.Y - Pelvis.Y);
    const float dZ = (bInvertImageYToUp ? -dZpx : dZpx) * PixelToUU;
    return FVector(DepthOffsetX, dY, dZ);
}

bool ASword::TryComputeSwordPoseFromPose(const FVector2f& Pelvis2D, const FPersonPose& Pose, const FTransform& OwnerXform, FVector& OutDirWorld, FVector& OutStartCenterWorld) const
{
    OutDirWorld = FVector::ZeroVector;
    OutStartCenterWorld = FVector::ZeroVector;

    auto IsFinite2DLocal = [](const FVector2f& v) { return FMath::IsFinite(v.X) && FMath::IsFinite(v.Y); };
    auto HasGood = [&](int32 k)->bool {
        return Pose.XY.IsValidIndex(k) && Pose.Conf.IsValidIndex(k) && IsFinite2DLocal(Pose.XY[k]);
        };

    // 필수 관절
    if (!(HasGood(5) && HasGood(6) && HasGood(9) && HasGood(10))) return false;

    // 로컬 변환
    const FVector LShLoc = MakeLocal(Pose.XY[5], Pelvis2D);
    const FVector RShLoc = MakeLocal(Pose.XY[6], Pelvis2D);
    const FVector LWrLoc = MakeLocal(Pose.XY[9], Pelvis2D);
    const FVector RWrLoc = MakeLocal(Pose.XY[10], Pelvis2D);

    // 어깨/손목 거리 (YZ 평면)
    const float ShoulderLen =
        FVector2D::Distance(FVector2D(RShLoc.Y, RShLoc.Z), FVector2D(LShLoc.Y, LShLoc.Z));
    const float HandDist =
        FVector2D::Distance(FVector2D(RWrLoc.Y, RWrLoc.Z), FVector2D(LWrLoc.Y, LWrLoc.Z));

    if (!(ShoulderLen > KINDA_SMALL_NUMBER && HandDist < HandCloseRatio * ShoulderLen))
        return false;

    // 손 박스
    float minY = FMath::Min(LWrLoc.Y, RWrLoc.Y) - BoxPadUU;
    float maxY = FMath::Max(LWrLoc.Y, RWrLoc.Y) + BoxPadUU;
    float minZ = FMath::Min(LWrLoc.Z, RWrLoc.Z) - BoxPadUU;
    float maxZ = FMath::Max(LWrLoc.Z, RWrLoc.Z) + BoxPadUU;

    if (maxY - minY < BoxMinSizeUU) { const float c = 0.5f * (minY + maxY); minY = c - 0.5f * BoxMinSizeUU; maxY = c + 0.5f * BoxMinSizeUU; }
    if (maxZ - minZ < BoxMinSizeUU) { const float c = 0.5f * (minZ + maxZ); minZ = c - 0.5f * BoxMinSizeUU; maxZ = c + 0.5f * BoxMinSizeUU; }

    const float X = LWrLoc.X;
    const FVector P00(X, minY, minZ), P10(X, maxY, minZ), P11(X, maxY, maxZ), P01(X, minY, maxZ);
    const FVector W00 = OwnerXform.TransformPosition(P00);
    const FVector W10 = OwnerXform.TransformPosition(P10);
    const FVector W11 = OwnerXform.TransformPosition(P11);
    const FVector W01 = OwnerXform.TransformPosition(P01);

    const FVector BottomCenter = 0.5f * (W00 + W10);
    const FVector TopCenter = 0.5f * (W01 + W11);

    if (bDrawSwordDebug)
    {
        const FColor C = FColor::Green; const float Thick = 2.f;
        DrawDebugLine(GetWorld(), W00, W10, C, false, 0.f, 0, Thick);
        DrawDebugLine(GetWorld(), W10, W11, C, false, 0.f, 0, Thick);
        DrawDebugLine(GetWorld(), W11, W01, C, false, 0.f, 0, Thick);
        DrawDebugLine(GetWorld(), W01, W00, C, false, 0.f, 0, Thick);
    }

    // 칼 방향: 각 팔 손목→팔꿈치 수직의 평균(방향 통일)
    const FVector AxisX = OwnerXform.GetUnitAxis(EAxis::X); // 깊이축
    auto ProjectToPlane = [&](const FVector& v) { return v - FVector::DotProduct(v, AxisX) * AxisX; };
    auto PerpInYZPlane = [&](const FVector& v) {
        const FVector v2 = ProjectToPlane(v).GetSafeNormal();
        if (v2.IsNearlyZero()) return FVector::ZeroVector;
        return FVector::CrossProduct(AxisX, v2).GetSafeNormal();
        };

    FVector SwordDirWorld = FVector::ZeroVector;

    const bool bHasElbows = Pose.XY.IsValidIndex(7) && Pose.XY.IsValidIndex(8)
        && IsFinite2DLocal(Pose.XY[7]) && IsFinite2DLocal(Pose.XY[8]);

    if (bHasElbows)
    {
        const FVector LElLoc = MakeLocal(Pose.XY[7], Pelvis2D);
        const FVector RElLoc = MakeLocal(Pose.XY[8], Pelvis2D);

        const FVector vLw = OwnerXform.TransformVectorNoScale(LElLoc - LWrLoc);
        const FVector vRw = OwnerXform.TransformVectorNoScale(RElLoc - RWrLoc);

        const FVector pL0 = PerpInYZPlane(vLw);
        const FVector pR0 = PerpInYZPlane(vRw);

        FVector UpRef = (TopCenter - BottomCenter).GetSafeNormal();
        if (UpRef.IsNearlyZero())
        {
            UpRef = ProjectToPlane(FVector::UpVector).GetSafeNormal();
            if (UpRef.IsNearlyZero()) UpRef = (TopCenter - BottomCenter);
        }

        FVector pL = pL0, pR = pR0;
        if (!pL.IsNearlyZero() && FVector::DotProduct(pL, UpRef) < 0.f) pL *= -1.f;
        if (!pR.IsNearlyZero() && FVector::DotProduct(pR, UpRef) < 0.f) pR *= -1.f;

        SwordDirWorld = (pL + pR).GetSafeNormal();
        if (SwordDirWorld.IsNearlyZero())
        {
            const FVector vL = vLw.GetSafeNormal();
            const FVector vR = vRw.GetSafeNormal();
            SwordDirWorld = (vL + vR).GetSafeNormal();
            if (SwordDirWorld.IsNearlyZero()) SwordDirWorld = UpRef.GetSafeNormal();
        }

        const FVector LWw = OwnerXform.TransformPosition(LWrLoc);
        const FVector RWw = OwnerXform.TransformPosition(RWrLoc);
        const FVector LElW = OwnerXform.TransformPosition(LElLoc);
        const FVector RElW = OwnerXform.TransformPosition(RElLoc);

        const FVector LForearm = (LWw - LElW);
        const FVector RForearm = (RWw - RElW);

        const FVector LStart = LWw + LForearm * HandForwardRatio;
        const FVector RStart = RWw + RForearm * HandForwardRatio;

        OutStartCenterWorld = (LStart + RStart) * 0.5f;

        if (bDrawSwordDebug)
        {
            if (!pL.IsNearlyZero())
                DrawDebugLine(GetWorld(), LWw, LWw + pL * 30.f, FColor::Yellow, false, 0.f, 0, 1.5f);
            if (!pR.IsNearlyZero())
                DrawDebugLine(GetWorld(), RWw, RWw + pR * 30.f, FColor::Yellow, false, 0.f, 0, 1.5f);
        }
    }
    else
    {
        SwordDirWorld = (TopCenter - BottomCenter).GetSafeNormal();
        const FVector LWw = OwnerXform.TransformPosition(LWrLoc);
        const FVector RWw = OwnerXform.TransformPosition(RWrLoc);
        OutStartCenterWorld = (LWw + RWw) * 0.5f;
    }

    if (bDrawSwordDebug)
    {
        DrawDebugLine(GetWorld(), OutStartCenterWorld, OutStartCenterWorld + SwordDirWorld * 60.f,
            FColor::Blue, false, 0.f, 0, 2.f);
    }

    OutDirWorld = SwordDirWorld;
    return !OutDirWorld.IsNearlyZero();
}

void ASword::SetSwordPos(FVector DirWorld, FVector CenterWorld)
{
    // 무기 표시 조건 체크
    if (!OwningPlayer || OwningPlayer->GetCurrentWeapon() != EWeaponState::Sword)
    {
        HideWeapon();
        bHasSmoothedState = false; // 상태 리셋
        return;
    }

    // 방향이 불안정하면 업데이트 보류(진동 방지)
    const FVector YAxis = DirWorld.GetSafeNormal();
    if (YAxis.IsNearlyZero() || FVector::DotProduct(YAxis, YAxis) < MinDirDotToUpdate)
        return;

    // 타깃 회전 계산(기존 로직 그대로, 단 최종은 FQuat로)
    FVector WorldUp = FVector::UpVector;
    FVector ZAxis = FVector::CrossProduct(YAxis, WorldUp).GetSafeNormal();
    if (ZAxis.IsNearlyZero())
    {
        WorldUp = FVector::ForwardVector;
        ZAxis = FVector::CrossProduct(YAxis, WorldUp).GetSafeNormal();
    }
    const FVector XAxis = FVector::CrossProduct(ZAxis, YAxis).GetSafeNormal();
    const FMatrix RotMatrix(XAxis, YAxis, ZAxis, FVector::ZeroVector);
    const FQuat   TargetRot = FQuat(RotMatrix);

    const float dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : (1.f / 60.f);

    // 최초 프레임은 바로 스냅
    if (!bHasSmoothedState)
    {
        SmoothedCenter = CenterWorld;
        SmoothedRot = TargetRot;
        bHasSmoothedState = true;
        SetActorLocationAndRotation(SmoothedCenter, SmoothedRot);
        ShowWeapon();
        return;
    }

    // --- 텔레포트 감지(큰 점프는 바로 스냅해서 순간이동 느낌 제거) ---
    const float distJump = FVector::Dist(SmoothedCenter, CenterWorld);
    if (distJump > TeleportDistThreshold)
    {
        SmoothedCenter = CenterWorld;
        SmoothedRot = TargetRot;
        SetActorLocationAndRotation(SmoothedCenter, SmoothedRot);
        ShowWeapon();
        return;
    }

    // --- 지수 평활(half-life) ---
    const float aPos = HalfLifeToAlpha(PosHalfLife, dt);
    const float aRot = HalfLifeToAlpha(RotHalfLife, dt);

    // 위치: 지수평활 후 속도 클램프(최대 이동량 제한)
    FVector desired = FMath::Lerp(SmoothedCenter, CenterWorld, aPos);
    FVector step = desired - SmoothedCenter;
    const float maxStep = MaxPosSpeedUUps * dt;
    if (step.Size() > maxStep)
        step = step.GetClampedToMaxSize(maxStep);
    SmoothedCenter += step;

    // 회전: Slerp 후 각속도 클램프
    // 먼저 half-life 기반 Slerp
    FQuat slerped = FQuat::Slerp(SmoothedRot, TargetRot, aRot);
    slerped.Normalize();

    // 각속도 제한(프레임당 이동 가능한 최대 각도)
    const float maxAngleRad = FMath::DegreesToRadians(MaxAngularSpeedDegPs) * dt;
    const float wantAngleRad = SmoothedRot.AngularDistance(TargetRot);
    if (wantAngleRad > maxAngleRad + KINDA_SMALL_NUMBER)
    {
        const float t = maxAngleRad / wantAngleRad;
        SmoothedRot = FQuat::Slerp(SmoothedRot, TargetRot, t);
    }
    else
    {
        SmoothedRot = slerped;
    }
    SmoothedRot.Normalize();

    SetActorLocationAndRotation(SmoothedCenter, SmoothedRot);
    ShowWeapon();
}
