// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
class SIMPLEEDITOR_API FMultiWindowMgr
{
public:

	static void SeparateUI();

	static TSharedPtr<SWidget>BuildTabManagerWidget(); //静态的创建TabManager的方法
	
};
