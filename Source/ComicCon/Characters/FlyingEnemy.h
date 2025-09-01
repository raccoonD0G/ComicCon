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
	uint8 bUseCurve : 1= true;                 // � ���� �ѱ�

	UPROPERTY(EditAnywhere, Category = "Flight|Curve", meta = (ClampMin = "0"))
	float CurveOffset = 600.f;             // �߰� �������� �������� �������� �Ÿ�(�� ����)

	UPROPERTY(EditAnywhere, Category = "Flight|Curve")
	bool bRandomizeCurveSide = true;       // ��/�� ���� �������� �ֱ�

	UPROPERTY(EditAnywhere, Category = "Flight|Curve", meta = (ClampMin = "8", ClampMax = "256"))
	int32 ArcSamples = 50;                 // ȣ ���� ���� ��(�������� �����ӵ� ��Ȯ)

	FVector ControlPoint;                  // ������ ������ P1
	TArray<float> CumLen;                  // ���� ���� ���̺�(0..TotalLen)
	float TotalLen = 0.f;                  // ��ü ȣ ����
	float Elapsed = 0.f;                   // ���� �ð�

	FVector BezierPos(float t) const;      // B(t)
	FVector BezierTangent(float t) const;  // B'(t) ����ȭ ��
	void   BuildArcTable();                // CumLen/TotalLen ä���
	float  FindTForDistance(float s) const;// s(=�̵��Ÿ�) -> t ������ [0,1]

// Damage Section
protected:
	/** ������ �ý��� ������: � DamageAmount�� 1ȸ �ǰ����� ������ ü�� 1 ���� */
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator, AActor* DamageCauser) override;

	/** �������Ʈ/�ڵ忡�� ���� 1ȸ �ǰ� ó���ϰ� ���� �� ȣ�� */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void HitOnce();

	/** ���� ��� ���� ���� */
	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return bDead; }

	/** ü���� 0 ���ϰ� �Ǹ� ȣ�� */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void Die();

	/** BP�� �̺�Ʈ �ŵ� */
	UFUNCTION(BlueprintImplementableEvent, Category = "Health")
	void OnDamagedOnce();     // 1ȸ �ǰ� ��(ü�� ���� ��) ȣ��

	UFUNCTION(BlueprintImplementableEvent, Category = "Health")
	void OnDied();            // ��� �� ȣ��

	void OnDeathFinished();

protected:
	/** �ִ� ü��: �䱸���׿� ���� 1�� ����(���ϸ� EditDefaultsOnly�� �����ص� ��) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "1", ClampMax = "1"))
	int32 MaxHealth = 1;

	/** ���� ü�� */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Health")
	int32 CurrentHealth = 1;

	/** ��� �÷���(�ߺ� ó�� ����) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Health")
	uint8 bDead : 1 = false;

	FTimerHandle DestroyTimerHandle;
};