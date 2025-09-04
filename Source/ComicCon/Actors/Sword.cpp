// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/Sword.h"
#include "Actors/MirroredPlayer.h"
#include "Components/PoseUdpReceiverComponent.h"
#include "Components/SwingClassifierComponent.h"
#include "Utils.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Actors/SwingProjectile.h"
#include <Kismet/GameplayStatics.h>

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
        Recv->OnHandBufferChanged.AddDynamic(this, &ASword::OnSwordHandsInput); // <— 추가
    }

    if (SwingClassifierComponent)
    {
        SwingClassifierComponent->OnSwingDetected.AddDynamic(this, &ASword::HandleSwingDetected);
    }
}

void ASword::HandleSwingDetected(TArray<FTimedPoseSnapshot> SnapsVal)
{
    // 원 함수는 TArray<const FTimedPoseSnapshot*> 이었음 → 값 배열로 대체
    if (!GetWorld() || SnapsVal.Num() < 2) return;

    // 1) 카메라 시선
    FVector CamLoc, CamFwd;
    if (!SwingClassifierComponent->GetCameraView(CamLoc, CamFwd)) return;

    // ---------- MakePlaneFromSnap 람다 ----------
    auto MakePlaneFromSnap = [&](const FTimedPoseSnapshot* S,
        FVector& OutStartW, FQuat& OutRot, FVector& OutU, FVector& OutDirToWrist)->bool
        {
            int32 PersonIdx = INDEX_NONE;
            if (!SwingClassifierComponent->PickBestPerson(S->Poses, PersonIdx)) return false;
            const FPersonPose& P = S->Poses[PersonIdx];
            if (P.XY.Num() < 17) return false;

            // 기존 어깨→손목 방향/팔축 얻기
            FVector StartW_tmp, DirW, UpperW;
            if (!SwingClassifierComponent->GetExtendedArmWorld(P, StartW_tmp, DirW, UpperW)) return false;

            auto Finite2D = [](const FVector2f& V) { return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y); };

            const FVector2f& LHip = P.XY[11];
            const FVector2f& RHip = P.XY[12];
            if (!Finite2D(LHip) || !Finite2D(RHip)) return false;
            const FVector2f Pelvis2D = (LHip + RHip) * 0.5f;

            auto Map2DToLocal = [&](const FVector2f& Q)->FVector
                {
                    const FVector2f d = Q - Pelvis2D;
                    const float YY = d.X * SwingClassifierComponent->GetPixelToUU();
                    const float ZZ = (SwingClassifierComponent->GetIsInvertImageYToUp() ? -d.Y : d.Y) * SwingClassifierComponent->GetPixelToUU();
                    return FVector(SwingClassifierComponent->GetDepthOffsetX(), YY, ZZ);
                };

            auto ToWorld = [&](const FVector& L)->FVector
                {
                    return GetActorTransform().TransformPosition(L);
                };

            const int32 LEL = COCO_LEL, REL = COCO_REL, LWR = COCO_LWR, RWR = COCO_RWR;
            if (!(P.XY.IsValidIndex(LEL) && P.XY.IsValidIndex(REL) && P.XY.IsValidIndex(LWR) && P.XY.IsValidIndex(RWR))) return false;

            const FVector2f LEl2 = P.XY[LEL], REl2 = P.XY[REL], LWr2 = P.XY[LWR], RWr2 = P.XY[RWR];
            if (!(Finite2D(LEl2) && Finite2D(REl2) && Finite2D(LWr2) && Finite2D(RWr2))) return false;

            const FVector LElW = ToWorld(Map2DToLocal(LEl2));
            const FVector RElW = ToWorld(Map2DToLocal(REl2));
            const FVector LWw = ToWorld(Map2DToLocal(LWr2));
            const FVector RWw = ToWorld(Map2DToLocal(RWr2));

            const FVector vL = (LWw - StartW_tmp).GetSafeNormal();
            const FVector vR = (RWw - StartW_tmp).GetSafeNormal();
            const float dL = FVector::DotProduct(DirW, vL);
            const float dR = FVector::DotProduct(DirW, vR);
            const bool bUseLeft = (dL >= dR);

            const FVector WristW = bUseLeft ? LWw : RWw;
            const FVector ElbowW = bUseLeft ? LElW : RElW;

            const float ExtendRatio = 0.25f;
            const FVector Forearm = (WristW - ElbowW);
            const FVector Anchor = WristW + Forearm * ExtendRatio;

            FVector N = FVector::CrossProduct(CamFwd, UpperW).GetSafeNormal();
            if (N.IsNearlyZero())
            {
                N = FVector::CrossProduct(CamFwd, FVector::UpVector).GetSafeNormal();
                if (N.IsNearlyZero()) N = FVector::RightVector;
            }
            FVector U = (UpperW - FVector::DotProduct(UpperW, N) * N).GetSafeNormal();
            if (U.IsNearlyZero())
                U = (CamFwd - FVector::DotProduct(CamFwd, N) * N).GetSafeNormal();

            FVector Z = FVector::CrossProduct(N, U).GetSafeNormal();
            if (Z.IsNearlyZero()) Z = FVector::UpVector;

            const FMatrix M(
                FPlane(N.X, N.Y, N.Z, 0.f),
                FPlane(U.X, U.Y, U.Z, 0.f),
                FPlane(Z.X, Z.Y, Z.Z, 0.f),
                FPlane(0, 0, 0, 1)
            );

            OutStartW = Anchor;
            OutRot = FQuat(M);
            OutU = U;
            OutDirToWrist = DirW.GetSafeNormal();
            if (OutDirToWrist.IsNearlyZero()) OutDirToWrist = U;
            return true;
        };

    // ---------- 준비 ----------
    FVector Start0, Start1;  FQuat Rot0, Rot1;  FVector U0, U1;
    FVector DirW0, DirW1;

    const FTimedPoseSnapshot* SnapFirst = &SnapsVal[0];
    const FTimedPoseSnapshot* SnapLast = &SnapsVal.Last();

    if (!MakePlaneFromSnap(SnapFirst, Start0, Rot0, U0, DirW0)) return;
    if (!MakePlaneFromSnap(SnapLast, Start1, Rot1, U1, DirW1)) return;

    const float AngleRad = Rot0.AngularDistance(Rot1);
    const float AngleDeg = FMath::RadiansToDegrees(AngleRad);
    const float StepDeg = FMath::Max(1e-3f, DegreesPerInterp);
    int32 TotalPlanes = FMath::CeilToInt(AngleDeg / StepDeg) + 1;
    TotalPlanes = FMath::Clamp(TotalPlanes, FMath::Max(2, MinInterpPlanes), MaxInterpPlanes);

    const FVector HalfExtent(PlaneHalfThickness, PlaneHalfSize.X, PlaneHalfSize.Y);
    TSet<AActor*> UniqueActors;

    auto SpawnProjectile = [&](TSubclassOf<ASwingProjectile> InClass, const FVector& SpawnLoc, const FVector& DirWorld)
        {
            if (!InClass) return;

            const FRotator Rot = DirWorld.Rotation();
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            Params.Owner = this;
            Params.Instigator = GetInstigator();

            ASwingProjectile* Proj = GetWorld()->SpawnActor<ASwingProjectile>(InClass, SpawnLoc, Rot, Params);
            if (!Proj) return;

            if (UProjectileMovementComponent* PMC = Proj->FindComponentByClass<UProjectileMovementComponent>())
            {
                PMC->Velocity = DirWorld * ProjectileSpeed;
            }
            else
            {
                Proj->SetActorLocation(SpawnLoc + DirWorld * (ProjectileSpeed * GetWorld()->GetDeltaSeconds()));
            }
            if (ProjectileLifeSeconds > 0.f)
            {
                Proj->SetLifeSpan(ProjectileLifeSeconds);
            }
        };

    TArray<FVector> ArcPoints;
    ArcPoints.Reserve(TotalPlanes);

    FVector SumU = FVector::ZeroVector;
    FVector SumZ = FVector::ZeroVector;
    FVector SumStart = FVector::ZeroVector;
    int32   SumCount = 0;

    // ---------- 메인 루프 ----------
    for (int32 k = 0; k < TotalPlanes; ++k)
    {
        const float t = (TotalPlanes == 1) ? 0.f : (float)k / (float)(TotalPlanes - 1);
        const FQuat RotK = FQuat::Slerp(Rot0, Rot1, t).GetNormalized();

        const FVector N = RotK.GetAxisX();
        const FVector U = RotK.GetAxisY();
        const FVector Z = FVector::CrossProduct(N, U).GetSafeNormal();

        const FVector StartK = FMath::Lerp(Start0, Start1, t);

        const FVector DirK = ((1.f - t) * DirW0 + t * DirW1).GetSafeNormal();
        const FVector FireDir = DirK.IsNearlyZero() ? U : DirK;

        const FVector CenterK = StartK + U * HalfExtent.Y;

        OverlapPlaneOnce(CenterK, RotK, UniqueActors, DebugSwingDrawTime);

        if (bSpawnAtEachPlane && SwingProjectileClass)
        {
            const FVector SpawnLoc = StartK + FireDir * SpawnForwardOffset;
            SpawnProjectile(SwingProjectileClass, SpawnLoc, FireDir);
        }

        ArcPoints.Add(StartK);

        SumU += U;
        SumZ += Z;
        SumStart += StartK;
        ++SumCount;
    }

    // (옵션) ArcPoints를 이용한 별도 트레일/리본 액터를 스폰하고 싶다면 여기에서 처리

    // ---------- 마지막 한 발: AvgZ를 U쪽으로 Tilt ----------
    if (!bSpawnAtEachPlane && SwingProjectileClass && SumCount > 0)
    {
        FVector AvgZ = (SumZ / float(SumCount));
        if (AvgZ.IsNearlyZero())
        {
            FVector FallbackZ = FVector::CrossProduct(Rot1.GetAxisX(), Rot1.GetAxisY()).GetSafeNormal();
            if (FallbackZ.IsNearlyZero()) FallbackZ = CamFwd;
            AvgZ = FallbackZ;
        }
        AvgZ = AvgZ.GetSafeNormal();

        FVector AvgU = (SumU / float(SumCount));
        if (AvgU.IsNearlyZero()) AvgU = U1;
        AvgU = AvgU.GetSafeNormal();

        const float DotZU = FMath::Clamp(FVector::DotProduct(AvgZ, AvgU), -1.f, 1.f);
        const float FullAngleDeg = FMath::RadiansToDegrees(FMath::Acos(DotZU));
        const float TiltDeg = FMath::Clamp(TiltDegFromBlueTowardYellow, 0.f, FullAngleDeg);

        FVector Axis = FVector::CrossProduct(AvgZ, AvgU);
        if (!Axis.Normalize())
        {
            Axis = FVector::CrossProduct(AvgZ, FVector::UpVector);
            if (!Axis.Normalize())
            {
                Axis = FVector::CrossProduct(AvgZ, FVector::RightVector);
                Axis.Normalize();
            }
        }

        const FQuat Q(Axis, FMath::DegreesToRadians(TiltDeg));
        const FVector FireDir = (Q.RotateVector(AvgZ)).GetSafeNormal();

        const FVector AvgStart = SumStart / float(SumCount);
        const FVector SpawnLoc = AvgStart + FireDir * SpawnForwardOffset;

        SpawnProjectile(SwingProjectileClass, SpawnLoc, FireDir);
    }

    // ---------- 데미지 ----------
    if (UniqueActors.Num() > 0)
    {
        AController* Inst = GetInstigatorController();
        for (AActor* A : UniqueActors)
        {
            UGameplayStatics::ApplyDamage(A, SwingDamageAmount, Inst, this, DamageTypeClass);
        }
    }
}

void ASword::OnSwordPoseInput(const FVector2f& Pelvis2D, const TArray<FPersonPose>& Poses, float PixelToUU, const FTransform& OwnerXform)
{
    FVector Dir, Start;
    if (TryComputeSwordPoseFromPose(Pelvis2D, Poses.Last(), PixelToUU, OwnerXform, Dir, Start))
    {
        if (bInvertDirectionForSword) Dir *= -1.f; // 기존 좌표 정의 유지
        SwordDirectionWorld = Dir;
        SwordStartCenterWorld = Start;
        SetSwordPos(Dir, Start); // 위치/회전 적용(아래 함수)
    }
}

bool ASword::FindHandCenter(uint16 PersonId, uint8 Which, const FVector2f& FallbackNearTo, FVector2f& OutCenter) const
{
    int best = INDEX_NONE;

    // 1) pid & which 일치 우선
    for (int i = 0; i < CachedHands.Num(); ++i)
    {
        const FHandPose& H = CachedHands[i];
        if (H.PersonId == PersonId && H.Which == Which && IsFinite2D(H.Center))
        {
            best = i; break;
        }
    }
    // 2) pid 일치하며 which 미지정(2) → 손목 근접
    if (best == INDEX_NONE)
    {
        double bestD = DBL_MAX;
        for (int i = 0; i < CachedHands.Num(); ++i)
        {
            const FHandPose& H = CachedHands[i];
            if (H.PersonId == PersonId && IsFinite2D(H.Center))
            {
                const double d = FVector2D::Distance((FVector2D)H.Center, (FVector2D)FallbackNearTo);
                if (d < bestD) { bestD = d; best = i; }
            }
        }
    }
    // 3) pid 매칭 실패 → 전체 중 근접
    if (best == INDEX_NONE)
    {
        double bestD = DBL_MAX;
        for (int i = 0; i < CachedHands.Num(); ++i)
        {
            const FHandPose& H = CachedHands[i];
            if (!IsFinite2D(H.Center)) continue;
            const double d = FVector2D::Distance((FVector2D)H.Center, (FVector2D)FallbackNearTo);
            if (d < bestD) { bestD = d; best = i; }
        }
    }

    if (best != INDEX_NONE)
    {
        OutCenter = CachedHands[best].Center;
        return true;
    }
    return false;
}

void ASword::OnSwordHandsInput(const FVector2f& Pelvis2D, const TArray<FHandPose>& Hands, float PixelToUU, const FTransform& OwnerXform)
{
    CachedHands = Hands; // 최신으로 교체
}

FVector ASword::MakeLocal(const FVector2f& P, const FVector2f& Pelvis, float PixelToUU) const
{
    // 우리 좌표: +Y=오른쪽, +Z=위 (이미지 Y가 아래로 증가하면 Z에 부호 반전)
    const float dY = (P.X - Pelvis.X) * PixelToUU;
    const float dZpx = (P.Y - Pelvis.Y);
    const float dZ = (SwingClassifierComponent->GetIsInvertImageYToUp() ? -dZpx : dZpx) * PixelToUU;
    return FVector(DepthOffsetX, dY, dZ);
}

bool ASword::TryComputeSwordPoseFromPose(const FVector2f& Pelvis2D, const FPersonPose& Pose, float PixelToUU, const FTransform& OwnerXform, FVector& OutDirWorld, FVector& OutStartCenterWorld) const
{
    OutDirWorld = FVector::ZeroVector;
    OutStartCenterWorld = FVector::ZeroVector;

    auto HasGood = [&](int32 k)->bool {
        return Pose.XY.IsValidIndex(k) && Pose.Conf.IsValidIndex(k) && IsFinite2D(Pose.XY[k]);
        };

    // 어깨 체크(모드 유지 조건/스케일용)
    if (!(HasGood(5) && HasGood(6))) return false;

    // ===== 1) 왼/오른손 2D: Hands 우선, 폴백 손목 =====
    FVector2f L2, R2;
    bool bHaveL = false, bHaveR = false;

    const bool hasLwr = HasGood(9);
    const bool hasRwr = HasGood(10);
    const bool hasLel = HasGood(7);
    const bool hasRel = HasGood(8);

    const FVector2f Lwr2 = hasLwr ? Pose.XY[9] : FVector2f(NAN, NAN);
    const FVector2f Rwr2 = hasRwr ? Pose.XY[10] : FVector2f(NAN, NAN);

    if (Pose.PersonId >= 0)
    {
        FVector2f cand;
        if (hasLwr && FindHandCenter((uint16)Pose.PersonId, /*Which*/0, Lwr2, cand)) { L2 = cand; bHaveL = true; }
        if (hasRwr && FindHandCenter((uint16)Pose.PersonId, /*Which*/1, Rwr2, cand)) { R2 = cand; bHaveR = true; }
    }
    if (!bHaveL && hasLwr) { L2 = Lwr2; bHaveL = IsFinite2D(L2); }
    if (!bHaveR && hasRwr) { R2 = Rwr2; bHaveR = IsFinite2D(R2); }

    auto ToW = [&](const FVector2f& P)->FVector {
        const FVector L = MakeLocal(P, Pelvis2D, PixelToUU);
        return OwnerXform.TransformPosition(L);
        };

    FVector YAxis = FVector::ZeroVector;
    FVector Center = FVector::ZeroVector;

    if (bHaveL && bHaveR)
    {
        // (A) 양손 모두 있음 → 오른손 - 왼손
        const FVector LW = ToW(L2);
        const FVector RW = ToW(R2);
        YAxis = (RW - LW).GetSafeNormal();
        if (YAxis.IsNearlyZero()) return false;
        Center = 0.5f * (LW + RW);
    }
    else if (bHaveR || bHaveL)
    {
        // (B) 한 손만 있음 → 전완(팔꿈치→손목) 방향으로 손목의 25% 연장선 사용
        const bool useRight = bHaveR;
        const FVector2f Wr2 = useRight ? R2 : L2;
        const int32 idxEl = useRight ? 8 : 7; // REL or LEL
        if (!HasGood(idxEl)) return false;

        const FVector WristW = ToW(Wr2);
        const FVector ElbowW = ToW(Pose.XY[idxEl]);

        FVector Forearm = (WristW - ElbowW);
        const float ForearmLen = Forearm.Size();
        if (!Forearm.Normalize() || ForearmLen <= KINDA_SMALL_NUMBER) return false;

        // 칼 축: 전완 방향
        YAxis = Forearm;

        // 중심: 손목에서 전완 방향으로 '전완 길이의 25%' 만큼 연장한 지점
        Center = WristW + YAxis * (0.25f * ForearmLen);
    }
    else
    {
        return false;
    }

    // 약간 전방(+X)으로 내밀어 자연스럽게
    const FVector AxisX = OwnerXform.GetUnitAxis(EAxis::X);
    Center += AxisX * (HandForwardRatio * 20.f);

    OutDirWorld = YAxis;
    OutStartCenterWorld = Center;
    return true;
}

void ASword::SetSwordPos(FVector DirWorld, FVector CenterWorld)
{
    // 무기 표시 조건 체크
    if (!OwningPlayer || OwningPlayer->GetCurrentWeapon() != EWeaponState::Sword)
    {
        HideWeapon();
        return;
    }

    // 방향이 불안정하면 업데이트 보류
    const FVector YAxis = DirWorld.GetSafeNormal();
    if (YAxis.IsNearlyZero()) return;

    // 타깃 회전 계산 (스냅)
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

    // 그냥 즉시 적용
    SmoothedCenter = CenterWorld;
    SmoothedRot = TargetRot;

    SetActorLocationAndRotation(SmoothedCenter, SmoothedRot);
    ShowWeapon();
}
