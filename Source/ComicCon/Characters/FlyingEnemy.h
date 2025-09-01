// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/EnemyBase.h"
#include "FlyingEnemy.generated.h"

/**
 * 
 */
UCLASS()
class COMICCON_API AFlyingEnemy : public AEnemyBase
{
	GENERATED_BODY()

public:
	AFlyingEnemy();

	UFUNCTION(BlueprintCallable)
	void Init(const FVector& InStart, const FVector& InEnd);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(VisibleAnywhere)
	FVector StartPoint = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere)
	FVector EndPoint = FVector(400.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere)
	float MoveSpeed = 100.f; // uu/s

	uint8 bInitialized : 1 = false;

	void FaceAlongVelocity(const FVector& Dir);

// Animation Section
private:
	UPROPERTY(EditAnywhere, Category = "Flipbook")
	TObjectPtr<class UPaperFlipbook> DieFlipbook;

// Section Curve
private:
	
	UPROPERTY(EditAnywhere, Category = "Flight|Curve")
	uint8 bUseCurve : 1= true;                 // 곡선 비행 켜기

	UPROPERTY(EditAnywhere, Category = "Flight|Curve", meta = (ClampMin = "0"))
	float CurveOffset = 600.f;             // 중간 제어점이 직선에서 떨어지는 거리(휨 정도)

	UPROPERTY(EditAnywhere, Category = "Flight|Curve")
	bool bRandomizeCurveSide = true;       // 왼/오 임의 방향으로 휘기

	UPROPERTY(EditAnywhere, Category = "Flight|Curve", meta = (ClampMin = "8", ClampMax = "256"))
	int32 ArcSamples = 50;                 // 호 길이 샘플 수(높을수록 일정속도 정확)

	FVector ControlPoint;                  // 베지어 제어점 P1
	TArray<float> CumLen;                  // 누적 길이 테이블(0..TotalLen)
	float TotalLen = 0.f;                  // 전체 호 길이
	float Elapsed = 0.f;                   // 누적 시간

	FVector BezierPos(float t) const;      // B(t)
	FVector BezierTangent(float t) const;  // B'(t) 정규화 전
	void   BuildArcTable();                // CumLen/TotalLen 채우기
	float  FindTForDistance(float s) const;// s(=이동거리) -> t 역매핑 [0,1]

// Damage Section
protected:
	/** 데미지 시스템 진입점: 어떤 DamageAmount든 1회 피격으로 간주해 체력 1 감소 */
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator, AActor* DamageCauser) override;

	/** 블루프린트/코드에서 직접 1회 피격 처리하고 싶을 때 호출 */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void HitOnce();

	/** 현재 사망 상태 여부 */
	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return bDead; }

	/** 체력이 0 이하가 되면 호출 */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void Die();

	/** BP용 이벤트 훅들 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Health")
	void OnDamagedOnce();     // 1회 피격 시(체력 감소 후) 호출

	UFUNCTION(BlueprintImplementableEvent, Category = "Health")
	void OnDied();            // 사망 시 호출

	void OnDeathFinished();

protected:
	/** 최대 체력: 요구사항에 따라 1로 고정(원하면 EditDefaultsOnly로 노출해도 됨) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "1", ClampMax = "1"))
	int32 MaxHealth = 1;

	/** 현재 체력 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Health")
	int32 CurrentHealth = 1;

	/** 사망 플래그(중복 처리 방지) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Health")
	uint8 bDead : 1 = false;

	FTimerHandle DestroyTimerHandle;
};