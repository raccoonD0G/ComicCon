// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MirroredPlayer.generated.h"

UENUM(BlueprintType)
enum class EWeaponState
{
	Sword,
	Amulet
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCurrentWeaponChanged, EWeaponState, NewWeapon);

UCLASS()
class COMICCON_API AMirroredPlayer : public AActor
{
	GENERATED_BODY()
	
public:
	AMirroredPlayer();
	virtual void Tick(float DeltaSeconds) override;

public:
	FORCEINLINE class UPoseUdpReceiverComponent* GetPoseReceiver() { return PoseReceiver; }
	FORCEINLINE class UPoseClassifierComponent* GetPoseClassifier() { return PoseClassifier; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UPoseUdpReceiverComponent> PoseReceiver;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UPoseClassifierComponent> PoseClassifier;

// Weapon State Section
public:
	FORCEINLINE EWeaponState GetCurrentWeapon() const { return CurrentWeapon; }

	FOnCurrentWeaponChanged OnCurrentWeaponChanged;

private:
	void SetCurrentWeapon(EWeaponState NewCurrentWeapon);

	// HandDistance < Threshold �̸� Sword, �ƴϸ� Amulet
	UPROPERTY(EditAnywhere, Category = "Weapon")
	float SwordHandDistanceThreshold = 0.6f;  // �ȼ� ����

	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	EWeaponState CurrentWeapon = EWeaponState::Amulet;

// Hand Section:
public:
	UFUNCTION(BlueprintCallable, Category = "Pose|Hand")
	float GetHandDistance() const { return HandDistance; }

private:
	// �� Tick: LatestPoses �о� �ո� �Ÿ� ������Ʈ
	void UpdateHandDistanceFromLatestPoses();

	// ��ȿ�� �ո��� ���� ����� ��� �Ÿ� ���
	bool TryComputeHandDistance(float& OutDistance) const;

	// ���� + ��ε�ĳ��Ʈ
	void SetHandDistance(float InHandDistance);

	// ����
	UPROPERTY(VisibleAnywhere, Category = "Pose|Hand")
	float HandDistance = 0.f;
};
