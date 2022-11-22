// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/MelleWeapon.h"
#include "Components/BoxComponent.h"
#include "Engine/DataTable.h"
#include "UObject/ConstructorHelpers.h"
#include "Character/PC_Character.h"
#include "NPC/NPC_Enemy.h"
#include "NPC/NPC_Companion.h"

AMelleWeapon::AMelleWeapon() {

	m_WeaponName = "DefaultName";
	m_WeaponType = "Melle";
	m_WeaponDamage = 0.f;
	bHasHit = false;
	bOwnerIsAttacking = false;

}

void AMelleWeapon::BeginPlay() {
	Super::BeginPlay();

	m_CombatCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	m_CombatCollision->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	m_CombatCollision->SetCollisionResponseToChannels(ECollisionResponse::ECR_Ignore);
	m_CombatCollision->SetCollisionResponseToChannel(ECC_Pawn, ECollisionResponse::ECR_Overlap);
	m_CombatCollision->SetGenerateOverlapEvents(true);

	m_CombatCollision->OnComponentBeginOverlap.AddDynamic(this, &AMelleWeapon::CombatOnOverlapBegin);
	m_CombatCollision->OnComponentEndOverlap.AddDynamic(this, &AMelleWeapon::CombatOnOverlapEnd);

}
// Called every frame
void AMelleWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}


void AMelleWeapon::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) {


}

void AMelleWeapon::OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex) {
	
	/*if (OtherActor) {
		AMain* Main = Cast<AMain>(OtherActor);
		if (Main) {
			Main->SetActiveOverlappingItem(nullptr);
		}
	}*/
}

void AMelleWeapon::CombatOnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) {

	//Colliding with owner
	if (OtherActor->GetUniqueID() == OwnerID) {
		return;
	}
	//Placed weapon with no owner
	if (!this->GetOwner()) {
		return;
	}

	//if (!bHasHit) {
		if (this->GetOwner()->IsA(APC_Character::StaticClass())) {
			APC_Character* iOwner = Cast<APC_Character>(GetOwner());
			if (iOwner->bIsAttacking) {
				if (OtherActor->IsA(ANPC_Enemy::StaticClass()) && !HitTargets.Contains(OtherActor->GetUniqueID())) {
					ANPC_Enemy* HitTarget = Cast<ANPC_Enemy>(OtherActor);
					HitTarget->HitByAttack(this->m_WeaponDamage);
					//bHasHit = true;
					UE_LOG(LogTemp, Warning, TEXT("Character Hit Enemy"));
				}
			}
		}

		if (this->GetOwner()->IsA(ANPC_Companion::StaticClass())) {
			ANPC_Companion* iOwner = Cast<ANPC_Companion>(GetOwner());
			if (iOwner->bIsAttacking) {
				if (OtherActor->IsA(ANPC_Enemy::StaticClass()) && !HitTargets.Contains(OtherActor->GetUniqueID())) {
					ANPC_Enemy* HitTarget = Cast<ANPC_Enemy>(OtherActor);

					HitTarget->HitByAttack(this->m_WeaponDamage);
					//bHasHit = true;
					UE_LOG(LogTemp, Warning, TEXT("Companion Hit Enemy"));
				}
			}
		}

		if (this->GetOwner()->IsA(ANPC_Enemy::StaticClass())) {
			ANPC_Enemy* iOwner = Cast<ANPC_Enemy>(GetOwner());
			if (iOwner->bIsAttacking) {
				if (OtherActor->IsA(ANPC_Companion::StaticClass()) && !HitTargets.Contains(OtherActor->GetUniqueID())) {
					ANPC_Companion* HitTarget = Cast<ANPC_Companion>(OtherActor);

					HitTarget->HitByAttack(this->m_WeaponDamage);
					//bHasHit = true;
					UE_LOG(LogTemp, Warning, TEXT("Enemy Hit Companion"));

				}
				if (OtherActor->IsA(APC_Character::StaticClass()) && !HitTargets.Contains(OtherActor->GetUniqueID())) {
					APC_Character* HitTarget = Cast<APC_Character>(OtherActor);

					HitTarget->HitByAttack(this->m_WeaponDamage);
					//bHasHit = true;
					UE_LOG(LogTemp, Warning, TEXT("Enemy Hit Character"));

				}
			}

		}
	//}

	if (!HitTargets.Contains(OtherActor->GetUniqueID())) {
		HitTargets.Add(OtherActor->GetUniqueID());
	}
}

void AMelleWeapon::CombatOnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex) {
	

}

FString AMelleWeapon::GetWeaponName() {
	return this->m_WeaponName;
}