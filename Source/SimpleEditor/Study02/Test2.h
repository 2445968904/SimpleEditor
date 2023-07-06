// Fill out your copyright notice in the Description page of Project Settings.
//第二章学习的时候一些必要的知识点
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Test2.generated.h"

UCLASS()
class SIMPLEEDITOR_API ATest2 : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ATest2();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	//静态变量

	static int32 SValue;
	int32 z;
};
