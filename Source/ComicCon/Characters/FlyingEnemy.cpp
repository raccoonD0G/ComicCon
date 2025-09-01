// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/FlyingEnemy.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PaperFlipbookComponent.h"

AFlyingEnemy::AFlyingEnemy()
{
	PrimaryActorTick.bCanEverTick = true;
	if (auto* Move = GetCharacterMovement())
	{
		Move->GravityScale = 0.f;
		Move->bOrientRotationToMovement = false;
	}
	bUseControllerRotationYaw = false;

	CurrentHealth = MaxHealth;
}

void AFlyingEnemy::Init(const FVector& InStart, const FVector& InEnd)
{
	StartPoint = InStart;
	EndPoint = InEnd;
	SetActorLocation(StartPoint);

	// ---- 제어점(P1) 만들기: 직선의 중점에서 옆으로 CurveOffset만큼 이동
	const FVector AB = (EndPoint - StartPoint);
	const float   Len = AB.Size();
	const FVector Dir = (Len > KINDA_SMALL_NUMBER) ? (AB / Len) : FVector::ForwardVector;

	// 옆방향(수평면 기준)을 만들기: Dir x Up
	FVector Side = FVector::CrossProduct(Dir, FVector::UpVector);
	if (Side.IsNearlyZero())           // Dir이 위/아래와 평행하면 다른 축 사용
		Side = FVector::CrossProduct(Dir, FVector::RightVector);
	Side.Normalize();

	if (bRandomizeCurveSide && FMath::RandBool())
		Side *= -1.f;

	const FVector Mid = (StartPoint + EndPoint) * 0.5f;
	ControlPoint = Mid + Side * CurveOffset;

	// ---- 아크 길이 테이블 구축(일정 속도용)
	CumLen.Reset();
	TotalLen = 0.f;
	Elapsed = 0.f;

	if (bUseCurve)
	{
		BuildArcTable();
		// 시작 방향을 곡선 접선으로 정렬(Z축 정렬 유지)
		FaceAlongVelocity(BezierTangent(0.f).GetSafeNormal());
	}
	else
	{
		// 직선 기본
		FaceAlongVelocity((EndPoint - StartPoint).GetSafeNormal());
	}

	bInitialized = true;
}

void AFlyingEnemy::BeginPlay()
{
	Super::BeginPlay();
}

void AFlyingEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bInitialized || bDead) return;

	if (!bUseCurve)
	{
		// ---- 기존 직선 이동
		const FVector Pos = GetActorLocation();
		const FVector ToEnd = EndPoint - Pos;
		const float   Dist = ToEnd.Size();

		if (Dist < FMath::Max(5.f, MoveSpeed * DeltaSeconds))
		{
			SetActorLocation(EndPoint);
			Destroy();
			return;
		}

		const FVector Dir = ToEnd / Dist;
		SetActorLocation(Pos + Dir * MoveSpeed * DeltaSeconds);
		FaceAlongVelocity(Dir);
		return;
	}

	// ---- 곡선 이동(거의 일정 속도)
	Elapsed += DeltaSeconds;
	const float s = MoveSpeed * Elapsed;   // 이동해야 할 누적 거리

	if (TotalLen <= KINDA_SMALL_NUMBER)
	{
		// 길이 계산 실패시 안전망: 바로 도착
		SetActorLocation(EndPoint);
		Destroy();
		return;
	}

	if (s >= TotalLen)
	{
		SetActorLocation(EndPoint);
		Destroy();
		return;
	}

	const float t = FindTForDistance(s);       // s -> t 역매핑
	const FVector P = BezierPos(t);
	const FVector dP = BezierTangent(t);

	SetActorLocation(P);
	FaceAlongVelocity(dP.GetSafeNormal());
}

void AFlyingEnemy::FaceAlongVelocity(const FVector& Dir)
{
	const FVector Z = Dir.GetSafeNormal();
	if (Z.IsNearlyZero())
		return;

	const FVector From = FVector::UpVector; // 현재 로컬 Z축
	const float Dot = FVector::DotProduct(From, Z);

	FQuat Q;
	if (Dot < -0.9999f)
	{
		// From와 Z가 정반대면(180°) 임의의 직교축으로 회전축을 선택
		FVector Axis = FVector::CrossProduct(From, FVector::ForwardVector);
		if (Axis.IsNearlyZero()) Axis = FVector::CrossProduct(From, FVector::RightVector);
		Axis.Normalize();
		Q = FQuat(Axis, PI);
	}
	else
	{
		Q = FQuat::FindBetweenNormals(From, Z);
	}

	SetActorRotation(Q); // 액터의 Z축이 Dir을 향하게
}

FVector AFlyingEnemy::BezierPos(float t) const
{
	const FVector& P0 = StartPoint;
	const FVector& P1 = ControlPoint;
	const FVector& P2 = EndPoint;

	const float u = 1.f - t;
	const float uu = u * u;
	const float tt = t * t;

	return uu * P0 + 2.f * u * t * P1 + tt * P2;
}

// 도함수: B'(t) = 2(1-t)(P1-P0) + 2t(P2-P1)
FVector AFlyingEnemy::BezierTangent(float t) const
{
	const FVector& P0 = StartPoint;
	const FVector& P1 = ControlPoint;
	const FVector& P2 = EndPoint;

	const float u = 1.f - t;
	return 2.f * (u * (P1 - P0) + t * (P2 - P1));
}

void AFlyingEnemy::BuildArcTable()
{
	const int32 N = FMath::Clamp(ArcSamples, 8, 256);
	CumLen.SetNumUninitialized(N + 1);

	FVector prev = BezierPos(0.f);
	CumLen[0] = 0.f;

	float sum = 0.f;
	for (int32 i = 1; i <= N; ++i)
	{
		const float t = (float)i / (float)N;
		FVector p = BezierPos(t);
		sum += FVector::Dist(prev, p);
		CumLen[i] = sum;
		prev = p;
	}
	TotalLen = sum;
}

float AFlyingEnemy::FindTForDistance(float s) const
{
	const int32 N = FMath::Max(ArcSamples, 1);
	// 이진 탐색(UpperBound 유사)
	int32 lo = 0, hi = N;
	while (lo < hi)
	{
		int32 mid = (lo + hi) >> 1;
		if (CumLen[mid] < s) lo = mid + 1; else hi = mid;
	}
	const int32 i = FMath::Clamp(lo, 1, N); // 구간 i-1..i

	const float s0 = CumLen[i - 1];
	const float s1 = CumLen[i];
	const float a = (s1 > s0) ? (s - s0) / (s1 - s0) : 0.f;

	const float t0 = (float)(i - 1) / (float)N;
	const float t1 = (float)i / (float)N;
	return FMath::Lerp(t0, t1, a);
}

float AFlyingEnemy::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	// 부모 로직(어그로/히트리액션 등)이 있을 수 있으니 호출
	const float SuperDealt = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// 이미 사망했으면 무시
	if (bDead) return SuperDealt;

	// "한번 피격 = 체력 1 감소" 규칙 고정
	HitOnce();
	return FMath::Max(SuperDealt, 1.f); // 의미상 '피해가 1은 들어갔다' 정도로 반환
}

void AFlyingEnemy::HitOnce()
{
	if (bDead) return;

	// 체력 1 감소(최대체력=1이므로 1→0)
	CurrentHealth = FMath::Clamp(CurrentHealth - 1, 0, MaxHealth);

	// BP 이벤트(히트 이펙트/사운드 등)
	OnDamagedOnce();

	// 0이면 사망 처리
	if (CurrentHealth <= 0)
	{
		Die();
	}
}

void AFlyingEnemy::Die()
{
	if (bDead) return;
	bDead = true;

	// BP 이벤트(사망 애니/파티클/드랍 등)
	OnDied();

	// 1) 사망 플립북 재생
	if (GetSprite() && DieFlipbook)
	{
		GetSprite()->SetFlipbook(DieFlipbook);
		GetSprite()->PlayFromStart();
	}

	// 2) 5초 뒤 Destroy
	GetWorldTimerManager().SetTimer(
		DestroyTimerHandle,
		this,
		&AFlyingEnemy::OnDeathFinished,
		2.5f,
		false
	);
}

void AFlyingEnemy::OnDeathFinished()
{
	Destroy();
}