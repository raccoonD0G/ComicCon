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

	// ---- ������(P1) �����: ������ �������� ������ CurveOffset��ŭ �̵�
	const FVector AB = (EndPoint - StartPoint);
	const float   Len = AB.Size();
	const FVector Dir = (Len > KINDA_SMALL_NUMBER) ? (AB / Len) : FVector::ForwardVector;

	// ������(����� ����)�� �����: Dir x Up
	FVector Side = FVector::CrossProduct(Dir, FVector::UpVector);
	if (Side.IsNearlyZero())           // Dir�� ��/�Ʒ��� �����ϸ� �ٸ� �� ���
		Side = FVector::CrossProduct(Dir, FVector::RightVector);
	Side.Normalize();

	if (bRandomizeCurveSide && FMath::RandBool())
		Side *= -1.f;

	const FVector Mid = (StartPoint + EndPoint) * 0.5f;
	ControlPoint = Mid + Side * CurveOffset;

	// ---- ��ũ ���� ���̺� ����(���� �ӵ���)
	CumLen.Reset();
	TotalLen = 0.f;
	Elapsed = 0.f;

	if (bUseCurve)
	{
		BuildArcTable();
		// ���� ������ � �������� ����(Z�� ���� ����)
		FaceAlongVelocity(BezierTangent(0.f).GetSafeNormal());
	}
	else
	{
		// ���� �⺻
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
		// ---- ���� ���� �̵�
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

	// ---- � �̵�(���� ���� �ӵ�)
	Elapsed += DeltaSeconds;
	const float s = MoveSpeed * Elapsed;   // �̵��ؾ� �� ���� �Ÿ�

	if (TotalLen <= KINDA_SMALL_NUMBER)
	{
		// ���� ��� ���н� ������: �ٷ� ����
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

	const float t = FindTForDistance(s);       // s -> t ������
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

	const FVector From = FVector::UpVector; // ���� ���� Z��
	const float Dot = FVector::DotProduct(From, Z);

	FQuat Q;
	if (Dot < -0.9999f)
	{
		// From�� Z�� ���ݴ��(180��) ������ ���������� ȸ������ ����
		FVector Axis = FVector::CrossProduct(From, FVector::ForwardVector);
		if (Axis.IsNearlyZero()) Axis = FVector::CrossProduct(From, FVector::RightVector);
		Axis.Normalize();
		Q = FQuat(Axis, PI);
	}
	else
	{
		Q = FQuat::FindBetweenNormals(From, Z);
	}

	SetActorRotation(Q); // ������ Z���� Dir�� ���ϰ�
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

// ���Լ�: B'(t) = 2(1-t)(P1-P0) + 2t(P2-P1)
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
	// ���� Ž��(UpperBound ����)
	int32 lo = 0, hi = N;
	while (lo < hi)
	{
		int32 mid = (lo + hi) >> 1;
		if (CumLen[mid] < s) lo = mid + 1; else hi = mid;
	}
	const int32 i = FMath::Clamp(lo, 1, N); // ���� i-1..i

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
	// �θ� ����(��׷�/��Ʈ���׼� ��)�� ���� �� ������ ȣ��
	const float SuperDealt = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// �̹� ��������� ����
	if (bDead) return SuperDealt;

	// "�ѹ� �ǰ� = ü�� 1 ����" ��Ģ ����
	HitOnce();
	return FMath::Max(SuperDealt, 1.f); // �ǹ̻� '���ذ� 1�� ����' ������ ��ȯ
}

void AFlyingEnemy::HitOnce()
{
	if (bDead) return;

	// ü�� 1 ����(�ִ�ü��=1�̹Ƿ� 1��0)
	CurrentHealth = FMath::Clamp(CurrentHealth - 1, 0, MaxHealth);

	// BP �̺�Ʈ(��Ʈ ����Ʈ/���� ��)
	OnDamagedOnce();

	// 0�̸� ��� ó��
	if (CurrentHealth <= 0)
	{
		Die();
	}
}

void AFlyingEnemy::Die()
{
	if (bDead) return;
	bDead = true;

	// BP �̺�Ʈ(��� �ִ�/��ƼŬ/��� ��)
	OnDied();

	// 1) ��� �ø��� ���
	if (GetSprite() && DieFlipbook)
	{
		GetSprite()->SetFlipbook(DieFlipbook);
		GetSprite()->PlayFromStart();
	}

	// 2) 5�� �� Destroy
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