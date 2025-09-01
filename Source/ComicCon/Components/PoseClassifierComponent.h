// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PoseUdpReceiverComponent.h"
#include "PoseClassifierComponent.generated.h"

UENUM(BlueprintType)
enum class EMainHand : uint8
{
    Right  UMETA(DisplayName = "Right"),
    Left   UMETA(DisplayName = "Left"),
    Auto   UMETA(DisplayName = "Auto (prefer higher confidence)")
};

// 타임스탬프가 붙은 스냅샷(한 번의 수신에서의 모든 사람 포즈)
USTRUCT(BlueprintType)
struct FTimedPoseSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	uint64 TimestampMs = 0; // 송신측 ms

	UPROPERTY(VisibleAnywhere)
	TArray<FPersonPose> Poses;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPoseWindowUpdated);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class COMICCON_API UPoseClassifierComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPoseClassifierComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** 리시버 참조 (동일 액터에 붙어 있으면 자동 탐색, 아니면 에디터에서 세팅) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose")
    TObjectPtr<UPoseUdpReceiverComponent> Receiver;

    /** 윈도우 길이(초) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose")
    float WindowSeconds = 2.0f;

    /** 윈도우에 새로운 스냅샷이 반영될 때마다 브로드캐스트 */
    UPROPERTY(BlueprintAssignable, Category = "Pose")
    FOnPoseWindowUpdated OnPoseWindowUpdated;

    /** 현재 윈도우(최근 2초)의 스냅샷들을 읽기 전용으로 반환 */
    UFUNCTION(BlueprintCallable, Category = "Pose")
    const TArray<FTimedPoseSnapshot>& GetWindow() const { return WindowBuffer; }

    /** 현재 윈도우 안의 총 사람 수(중복 포함, 스냅샷마다 합산) */
    UFUNCTION(BlueprintCallable, Category = "Pose|Stats")
    int32 GetWindowTotalPersons() const;

    /** 윈도우 안의 최신 스냅샷에서 첫 번째 사람의 특정 관절 좌표(없으면 false) */
    UFUNCTION(BlueprintCallable, Category = "Pose|Query")
    bool GetLatestJoint(int32 CocoIndex, FVector2D& OutXY) const;

    /** (예시) 윈도우 내에서 가장 자주 등장한 personId (동점 시 임의) — 없으면 -1 */
    UFUNCTION(BlueprintCallable, Category = "Pose|Stats")
    int32 GetDominantPersonId() const;

private:
    // 내부 버퍼: 최근 수 초의 스냅샷
    TArray<FTimedPoseSnapshot> WindowBuffer;

    // 중복 삽입 방지용 마지막 타임스탬프 캐시
    uint64 LastIngestedTs = 0;

    void IngestLatestFromReceiver();
    void PruneOld(uint64 NowMs);

    bool PickBestPerson(const TArray<FPersonPose>& Poses, int32& OutIdx) const;
    bool IsHandsClose(const FPersonPose& P, float& OutShoulderW, float& OutWristDist) const;

    // 현재(수신 기준) ms — Receiver의 LatestTimestamp를 Now로 사용
    uint64 NowMsFromReceiver() const;

// Detect Section
public:
    FORCEINLINE EMainHand GetMainHand() { return HandSource; }

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow|Damage", meta = (AllowPrivateAccess = "true"))
    float PixelToUU = 0.5f;           // 픽셀 → UU 스케일 (장면에 맞게 튜닝)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow|Damage", meta = (AllowPrivateAccess = "true"))
    float DepthOffsetX = 30.f;        // 액터 앞쪽으로 띄우는 로컬 X 오프셋

    FVector Map2DToLocal(const FVector2f& P, const FVector2f& Pelvis2D) const;
    bool GetExtendedArmWorld(const FPersonPose& Pose, FVector& OutStart, FVector& OutDir, FVector& OutUpperArmDir) const;
    bool GetCameraView(FVector& OutCamLoc, FVector& OutCamForward) const;
    void OverlapPlaneOnce(const FVector& Center, const FQuat& Rot, TSet<AActor*>& UniqueActors, float DebugLifeTime);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow|Plane", meta = (AllowPrivateAccess = "true"))
    FVector2D PlaneHalfSize = FVector2D(500.f, 1000.f); // 평면 절반 크기(Y=가로, Z=세로)

    // 어느 손을 사용할지
    UPROPERTY(EditAnywhere, Category = "Pose")
    EMainHand HandSource = EMainHand::Right;

// Swing Section
private:
    // === Swing 판별 파라미터 ===
    // 손목-손목 거리가 어깨폭의 이 비율 이하이면 "가깝다"로 간주
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess="true"))
    float HandsCloseRatio = 0.6f; // 0.5~0.8 권장

    // 속도 계산 시 포함할 프레임의 "손 가까움" 비율
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    float MinCloseCoverage = 0.2f; // 80% 이상 가까워야

    // 로그 중복 방지 쿨다운(초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    float SwingCooldownSeconds = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow|Plane", meta = (AllowPrivateAccess = "true"))
    float SwingPlaneHalfThickness = 20.f;

    // 스윙 속도 강화용 파라미터
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|Strict", meta = (AllowPrivateAccess = "true"))
    float SwingRecentSeconds = 0.8f;          // 최근 이 시간 내 구간만 사용(예: 0.6~1.0s)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|Strict", meta = (AllowPrivateAccess = "true"))
    float MinPathLenNorm = 1.5f;              // 이동 누적거리(어깨폭 단위) 최저치

    // 각도 간격(도). 이 값이 작을수록 더 촘촘히 샘플링
    UPROPERTY(EditAnywhere, Category = "Swing|Sweep")
    float DegreesPerInterp = 12.f;

    // 보간 평면 최소/최대 개수(엔드포인트 포함)
    UPROPERTY(EditAnywhere, Category = "Swing|Sweep")
    int32 MinInterpPlanes = 2;

    UPROPERTY(EditAnywhere, Category = "Swing|Sweep")
    int32 MaxInterpPlanes = 24;

    // === Down-swing 전용 판정 파라미터 ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|Down", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    float DownVyThresholdNorm = 0.40f;      // vy_norm > 이 값일 때 '충분히 아래로 빠름'으로 집계(어깨폭/초)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|Down", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
    float MinDownCoverage = 0.45f;          // vy_norm > DownVyThresholdNorm 인 샘플 비율의 하한(0~1)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|Down", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    float MinDownAvgSpeedNorm = 0.55f;      // 평균 ↓속도(어깨폭/초)의 하한

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|Down", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    float MinDownPeakSpeedNorm = 1.00f;     // 최대 ↓속도(어깨폭/초)의 하한

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|Down", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    float MinDownDeltaNorm = 0.70f;         // 시작→끝 순수 ↓변위(어깨폭 단위) 최소치

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|Down", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
    int32 MaxVertReversals = 1;             // 수직 방향(위/아래) 반전 허용 횟수

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|Down", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    float VertHoldSeconds = 0.06f;          // 반전 유효 디바운스 시간(초). 이 시간 이상 지속 시 '진짜 반전'으로 카운트

    // ===== Swing Plane Sweep 파라미터 =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|PlaneSweep", meta = (AllowPrivateAccess = "true"))
    float DebugSweepDrawTime = 0.6f;   // 디버그 박스 표시 시간(초)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow|Damage", meta = (AllowPrivateAccess = "true"))
    float SwingDamageAmount = 2.f;        // 데미지 양

private:
    uint64 LastSwingLoggedMs = 0;

    void DetectSwing(); // <- Tick에서 호출

    void ApplySwingPlaneSweepDamage(const TArray<const FTimedPoseSnapshot*>& Snaps, TSubclassOf<AActor> ProjectileClass = nullptr, float ProjectileSpeed = 1500.f, float ProjectileLifeSeconds = 3.f, float SpawnForwardOffset = 10.f, bool  bSpawnAtEachPlane = false);

// Arrow Section
private:
    // === Awrrow 판별 파라미터 ===
    // "충분히 멀어짐" 종료 조건: 손목 간 거리 ≥ ArrowFarRatio * 어깨폭
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow", meta = (AllowPrivateAccess = "true"))
    float ArrowFarRatio = 1.8f;            // 어깨폭의 1.2배 이상 벌어지면 화살 발사로 간주

    // 윈도우 내에서 가까움→멀어짐 증가량(정규화) 최소치 (히스테리시스)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow", meta = (AllowPrivateAccess = "true"))
    float ArrowMinDeltaNorm = 0.5f;        // (멀어짐 - 가까움) / 어깨폭

    // 멀어지는 속도(정규화: 어깨폭/초) 최소치 (너무 느린 변화 방지)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow", meta = (AllowPrivateAccess = "true"))
    float ArrowMinSpeedNorm = 1.0f;

    // 로그 후 쿨다운(초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow", meta = (AllowPrivateAccess = "true"))
    float ArrowCooldownSeconds = 1.0f;

    // ===== Arrow 평면(Plane) 데미지 파라미터 =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow|Plane", meta = (AllowPrivateAccess = "true"))
    float PlaneDistance = 150.f;        // 시작점(Start)에서 방향(Dir)로 얼마나 떨어진 곳에 평면을 둘지

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow|Plane", meta = (AllowPrivateAccess = "true"))
    float ArrowPlaneHalfThickness = 20.f;     // 시선 방향(X) 두께 (얇을수록 '면'에 가까움)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow|Plane", meta = (AllowPrivateAccess = "true"))
    TEnumAsByte<ECollisionChannel> PlaneChannel = ECC_Pawn; // 겹침 판정 채널

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow|Plane", meta = (AllowPrivateAccess = "true"))
    bool bDrawDebugPlane = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow|Damage", meta = (AllowPrivateAccess = "true"))
    float ArrowDamageAmount = 2.f;        // 데미지 양

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow|Damage", meta = (AllowPrivateAccess = "true"))
    TSubclassOf<UDamageType> DamageTypeClass;

private:
    uint64 LastArrowLoggedMs = 0;

    void DetectArrow(); // ← Tick에서 호출

    void ApplyArrowPlaneDamage(const FVector& Start, const FVector& UpperArmDir);
    
// Single Swing Section
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    double MinOpenCoverage = 0.55;

    // 속도 임계(어깨폭/초). ↓ 1.50 → 1.20, 0.80 → 0.60 로 완화
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    double MinPeakSpeedNorm = 1.20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    double MinAvgSpeedNorm = 0.60;

    // “몸에서 바깥” 조건(어깨폭 단위). ↓ 0.60 → 0.45, 0.80 → 0.60 로 완화
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    double MinDeltaRadNorm = 0.45;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    double MinOutwardPathNorm = 0.60;

    // 반전 허용 수/디바운스/데드밴드. ↑ 2 → 3 (조금 더 관대),
    // AxisHoldSeconds 0.06 → 0.05, SignDeadband 0.15 → 0.12
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    int32 MaxRadialReversals = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    double AxisHoldSeconds = 0.05;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    double SignDeadband = 0.12;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    TSubclassOf<AActor> AmuletProjectileClass;

    UPROPERTY(EditAnywhere, Category = "Pose|Swing")
    bool bInvertImageYToUp = true;

    void DetectSingleSwing(EMainHand WhichHand);

    void ApplyMainHandMovementPlaneAndFire(const TArray<const FTimedPoseSnapshot*>& Snaps, EMainHand WhichHand, TSubclassOf<AActor> ProjectileClass = nullptr, float ProjectileSpeed = 1500.f, float ProjectileLifeSeconds = 3.f, float SpawnForwardOffset = 10.f, bool  bSpawnAtEachPlane = false);
};