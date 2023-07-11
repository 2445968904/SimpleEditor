// Fill out your copyright notice in the Description page of Project Settings.


#include "SinpleEditorGameEngine.h"

#include "ContentStreaming.h"
#include "CustomResourcePool.h"
#include "EngineAnalytics.h"
#include "EngineModule.h"
#include "GameMapsSettings.h"
#include "IMediaModule.h"
#include "MoviePlayerProxy.h"
#include "MovieSceneCaptureModule.h"
#include "RenderGraphBuilder.h"
#include "RenderTargetPool.h"
#include "StudioAnalytics.h"
#include "Android/AndroidPlatformSplash.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/ReflectionCaptureComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/CoreSettings.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/EmbeddedCommunication.h"
#include "MovieSceneCapture/Public/IMovieSceneCapture.h"
#include "Net/NetworkProfiler.h"

class IMovieSceneCaptureInterface;
class UGameMapsSettings;

void USinpleEditorGameEngine::Init(IEngineLoop* InEngineLoop)
{
	//关于性能的
	//DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UGameEngine Init"), STAT_GameEngineStartup, STATGROUP_LoadTime);
	

	// Call base.
	UEngine::Init(InEngineLoop);

#if USE_NETWORK_PROFILER
	FString NetworkProfilerTag;
	if( FParse::Value(FCommandLine::Get(), TEXT("NETWORKPROFILER="), NetworkProfilerTag ) )
	{
		GNetworkProfiler.EnableTracking(true);
	}
#endif

	// Load and apply user game settings
	GetGameUserSettings()->LoadSettings();
	GetGameUserSettings()->ApplyNonResolutionSettings();

	// Create game instance.  For GameEngine, this should be the only GameInstance that ever gets created.
	{
		FSoftClassPath GameInstanceClassName = GetDefault<UGameMapsSettings>()->GameInstanceClass;
		UClass* GameInstanceClass = (GameInstanceClassName.IsValid() ? LoadObject<UClass>(NULL, *GameInstanceClassName.ToString()) : UGameInstance::StaticClass());
		
		if (GameInstanceClass == nullptr)
		{
			//UE_LOG(LogEngine, Error, TEXT("Unable to load GameInstance Class '%s'. Falling back to generic UGameInstance."), *GameInstanceClassName.ToString());
			GameInstanceClass = UGameInstance::StaticClass();
		}

		GameInstance = NewObject<UGameInstance>(this, GameInstanceClass);

		GameInstance->InitializeStandalone();
	}
 
//  	// Creates the initial world context. For GameEngine, this should be the only WorldContext that ever gets created.
//  	FWorldContext& InitialWorldContext = CreateNewWorldContext(EWorldType::Game);

	IMovieSceneCaptureInterface* MovieSceneCaptureImpl = nullptr;
#if WITH_EDITOR
	if (!IsRunningDedicatedServer() && !IsRunningCommandlet())
	{
		MovieSceneCaptureImpl = IMovieSceneCaptureModule::Get().InitializeFromCommandLine();
		if (MovieSceneCaptureImpl)
		{
			StartupMovieCaptureHandle = MovieSceneCaptureImpl->GetHandle();
		}
	}
#endif

	// Initialize the viewport client.
	UGameViewportClient* ViewportClient = NULL;
	if(GIsClient)
	{
		ViewportClient = NewObject<UGameViewportClient>(this, GameViewportClientClass);
		ViewportClient->Init(*GameInstance->GetWorldContext(), GameInstance);
		GameViewport = ViewportClient;
		GameInstance->GetWorldContext()->GameViewport = ViewportClient;
	}

	LastTimeLogsFlushed2 = FPlatformTime::Seconds();

	// Attach the viewport client to a new viewport.
	if(ViewportClient)
	{
		// This must be created before any gameplay code adds widgets
		bool bWindowAlreadyExists = GameViewportWindow.IsValid();
		if (!bWindowAlreadyExists)
		{
			//UE_LOG(LogEngine, Log, TEXT("GameWindow did not exist.  Was created"));
			GameViewportWindow = CreateGameWindow();
		}

		CreateGameViewport_New( ViewportClient );

		if( !bWindowAlreadyExists )
		{
			SwitchGameWindowToUseGameViewport_New();
		}

		FString Error;
		if(ViewportClient->SetupInitialLocalPlayer(Error) == NULL)
		{
			//UE_LOG(LogEngine, Fatal,TEXT("%s"),*Error);
		}

		UGameViewportClient::OnViewportCreated().Broadcast();
	}

	//UE_LOG(LogInit, Display, TEXT("Game Engine Initialized.") );

	// for IsInitialized()
	bIsInitialized = true;
}

void USinpleEditorGameEngine::Tick(float DeltaSeconds, bool bIdleMode)
{
	//CSV_SCOPED_TIMING_STAT_EXCLUSIVE(EngineTickMisc);
	//SCOPE_TIME_GUARD(TEXT("UGameEngine::Tick"));
	//SCOPE_CYCLE_COUNTER(STAT_GameEngineTick);
	//以上三个都是性能检测相关的
	
	NETWORK_PROFILER(GNetworkProfiler.TrackFrameBegin());
	//这个是网络方面的东西
	

	
	// -----------------------------------------------------
	// Non-World related stuff
	// -----------------------------------------------------

	if( DeltaSeconds < 0.0f )
	{
#if (UE_BUILD_SHIPPING && WITH_EDITOR)
		// End users don't have access to the secure parts of UDN.  Regardless, they won't
		// need the warning because the game ships with AMD drivers that address the issue.
		UE_LOG(LogEngine, Fatal,TEXT("Negative delta time!"));
#else
		// Send developers to the support list thread.
		//UE_LOG(LogEngine, Fatal,TEXT("Negative delta time! Please see https://udn.epicgames.com/lists/showpost.php?list=ue3bugs&id=4364"));
#endif
	}

	// if ((GSlowFrameLoggingThreshold > 0.0f) && (DeltaSeconds > GSlowFrameLoggingThreshold))
	// {
	// 	//UE_LOG(LogEngine, Log, TEXT("Slow GT frame detected (GT frame %u, delta time %f s)"), GFrameCounter - 1, DeltaSeconds);
	// }

	if (IsRunningDedicatedServer())
	{
		double CurrentTime = FPlatformTime::Seconds();
		if (CurrentTime - LastTimeLogsFlushed2 > static_cast<double>(ServerFlushLogInterval))
		{
			CSV_SCOPED_TIMING_STAT_EXCLUSIVE(LogFlush);
			GLog->Flush();

			LastTimeLogsFlushed2 = FPlatformTime::Seconds();
		}
	}
	else if (!IsRunningCommandlet() && FApp::CanEverRender())	// skip in case of commandlets, dedicated servers and headless games
	{
		// Clean up the game viewports that have been closed.
		CleanupGameViewport();
	}

	// If all viewports closed, time to exit - unless we're running headless
	if (GIsClient && (GameViewport == nullptr) && FApp::CanEverRender())
	{
		//UE_LOG(LogEngine, Log,  TEXT("All Windows Closed") );
		FPlatformMisc::RequestExit( 0 );
		return;
	}

	if (GameViewport != NULL)
	{
		// Decide whether to drop high detail because of frame rate.
		//这个是性能检测方面的东西
		//QUICK_SCOPE_CYCLE_COUNTER(STAT_UGameEngine_Tick_SetDropDetail);
		GameViewport->SetDropDetail(DeltaSeconds);
	}

#if !UE_SERVER
	// Media module present?
	static const FName MediaModuleName(TEXT("Media"));
	IMediaModule* MediaModule = FModuleManager::LoadModulePtr<IMediaModule>(MediaModuleName);
	if (MediaModule != nullptr)
	{
		// Yes. Will a world trigger the MediaFramework tick due to an active Sequencer?
		bool bWorldWillTickMediaFramework = false;
		if (!bIdleMode)
		{
			for (int32 i = 0; i < WorldList.Num(); ++i)
			{
				FWorldContext& Context = WorldList[i];
				if (Context.World() != nullptr && Context.World()->ShouldTick() && Context.World()->IsMovieSceneSequenceTickHandlerBound())
				{
					bWorldWillTickMediaFramework = true;
					break;
				}
			}
		}
		if (!bWorldWillTickMediaFramework)
		{
			// tick media framework if no world would do it later on
			// (so we can normally - no Sequencer active - assume that the media state changes are all done early)
			MediaModule->TickPreEngine();
		}
	}
#endif

	// Update subsystems.
	{
		// This assumes that UObject::StaticTick only calls ProcessAsyncLoading.
		//SCOPE_TIME_GUARD(TEXT("UGameEngine::Tick - StaticTick"));
		StaticTick(DeltaSeconds, !!GAsyncLoadingUseFullTimeLimit, GAsyncLoadingTimeLimit / 1000.f);
	}

	{
		//SCOPE_TIME_GUARD(TEXT("UGameEngine::Tick - Analytics"));
		FEngineAnalytics::Tick(DeltaSeconds);
	}

	{
		//SCOPE_TIME_GUARD(TEXT("UGameEngine::Tick - Studio Analytics"));
		FStudioAnalytics::Tick(DeltaSeconds);
	}

	// -----------------------------------------------------
	// Begin ticking worlds
	// -----------------------------------------------------

	bool bIsAnyNonPreviewWorldUnpaused = false;

	FName OriginalGWorldContext = NAME_None;
	for (int32 i=0; i < WorldList.Num(); ++i)
	{
		if (WorldList[i].World() == GWorld)
		{
			OriginalGWorldContext = WorldList[i].ContextHandle;
			break;
		}
	}

	for (int32 WorldIdx = 0; WorldIdx < WorldList.Num(); ++WorldIdx)
	{
		FWorldContext &Context = WorldList[WorldIdx];
		if (Context.World() == NULL || !Context.World()->ShouldTick())
		{
			continue;
		}

		GWorld = Context.World();

		// Tick all travel and Pending NetGames (Seamless, server, client)
		{
			//QUICK_SCOPE_CYCLE_COUNTER(STAT_UGameEngine_Tick_TickWorldTravel);
			TickWorldTravel(Context, DeltaSeconds);
		}

		if (!bIdleMode)
		{
			//SCOPE_TIME_GUARD(TEXT("UGameEngine::Tick - WorldTick"));

			// Tick the world.
			Context.World()->Tick( LEVELTICK_All, DeltaSeconds );
		}

		if (!IsRunningDedicatedServer() && !IsRunningCommandlet())
		{
			//QUICK_SCOPE_CYCLE_COUNTER(STAT_UGameEngine_Tick_CheckCaptures);
			// Only update reflection captures in game once all 'always loaded' levels have been loaded
			// This won't work with actual level streaming though
			if (Context.World()->AreAlwaysLoadedLevelsLoaded())
			{
				// Update sky light first because it's considered direct lighting, sky diffuse will be visible in reflection capture indirect specular
				USkyLightComponent::UpdateSkyCaptureContents(Context.World());
				UReflectionCaptureComponent::UpdateReflectionCaptureContents(Context.World());
			}
		}



		// Issue cause event after first tick to provide a chance for the game to spawn the player and such.
		if( Context.World()->bWorldWasLoadedThisTick )
		{
			Context.World()->bWorldWasLoadedThisTick = false;
			
			const TCHAR* InitialExec = Context.LastURL.GetOption(TEXT("causeevent="),NULL);
			ULocalPlayer* GamePlayer = Context.OwningGameInstance ? Context.OwningGameInstance->GetFirstGamePlayer() : NULL;
			if( InitialExec && GamePlayer )
			{
				//UE_LOG(LogEngine, Log, TEXT("Issuing initial cause event passed from URL: %s"), InitialExec);
				GamePlayer->Exec( GamePlayer->GetWorld(), *(FString("CAUSEEVENT ") + InitialExec), *GLog );
			}

			Context.World()->bTriggerPostLoadMap = true;
		}
	
		UpdateTransitionType(Context.World());

		// Block on async loading if requested.
		if (Context.World()->bRequestedBlockOnAsyncLoading)
		{
			BlockTillLevelStreamingCompleted(Context.World());
			Context.World()->bRequestedBlockOnAsyncLoading = false;
		}

		// streamingServer
		if( GIsServer == true )
		{
			//SCOPE_CYCLE_COUNTER(STAT_UpdateLevelStreaming);
			Context.World()->UpdateLevelStreaming();
		}

		// See whether any map changes are pending and we requested them to be committed.
		//QUICK_SCOPE_CYCLE_COUNTER(STAT_UGameEngine_Tick_ConditionalCommitMapChange);
		ConditionalCommitMapChange(Context);

		if (Context.WorldType != EWorldType::EditorPreview && !Context.World()->IsPaused())
		{
			bIsAnyNonPreviewWorldUnpaused = true;
		}
	}

	// ----------------------------
	//	End per-world ticking
	// ----------------------------
	{
		//SCOPE_TIME_GUARD(TEXT("UGameEngine::Tick - TickObjects"));
		FTickableGameObject::TickObjects(nullptr, LEVELTICK_All, false, DeltaSeconds);
	}

	// Restore original GWorld*. This will go away one day.
	if (OriginalGWorldContext != NAME_None)
	{
		//QUICK_SCOPE_CYCLE_COUNTER(STAT_UGameEngine_Tick_GetWorldContextFromHandleChecked);
		GWorld = GetWorldContextFromHandleChecked(OriginalGWorldContext).World();
	}

#if !UE_SERVER
	if (MediaModule != nullptr)
	{
		MediaModule->TickPostEngine();
	}
#endif

	// Tick the viewport
	if ( GameViewport != NULL && !bIdleMode )
	{
		//SCOPE_TIME_GUARD(TEXT("UGameEngine::Tick - TickViewport"));
		//SCOPE_CYCLE_COUNTER(STAT_GameViewportTick);
		GameViewport->Tick(DeltaSeconds);
	}

	FMoviePlayerProxy::BlockingForceFinished();
	if (FPlatformProperties::SupportsWindowedMode())
	{
		// Hide the splashscreen and show the game window
		static bool bFirstTime = true;
		if ( bFirstTime )
		{
			bFirstTime = false;
			FPlatformSplash::Hide();
			if ( GameViewportWindow.IsValid() )
			{
				// Don't show window in off-screen rendering mode as it doesn't render to screen
				if (!FSlateApplication::Get().IsRenderingOffScreen())
				{
					GameViewportWindow.Pin()->ShowWindow();
				}
				FSlateApplication::Get().RegisterGameViewport( GameViewportWidget.ToSharedRef() );
			}
		}
	}

	if (!bIdleMode && !IsRunningDedicatedServer() && !IsRunningCommandlet() && FEmbeddedCommunication::IsAwakeForRendering())
	{
		// Render everything.
		RedrawViewports();

		// Some tasks can only be done once we finish all scenes/viewports
		GetRendererModule().PostRenderAllViewports();
	}

	if( GIsClient )
	{
		// Update resource streaming after viewports have had a chance to update view information. Normal update.
		//QUICK_SCOPE_CYCLE_COUNTER(STAT_UGameEngine_Tick_IStreamingManager);
		IStreamingManager::Get().Tick( DeltaSeconds );
	}

	// Update Audio. This needs to occur after rendering as the rendering code updates the listener position.
	FAudioDeviceManager* GameAudioDeviceManager = GEngine->GetAudioDeviceManager();
	if (GameAudioDeviceManager)
	{
		//SCOPE_TIME_GUARD(TEXT("UGameEngine::Tick - Update Audio"));
		GameAudioDeviceManager->UpdateActiveAudioDevices(bIsAnyNonPreviewWorldUnpaused);
	}

	// rendering thread commands
	{
		bool bPauseRenderingRealtimeClock = GPauseRenderingRealtimeClock;
		ENQUEUE_RENDER_COMMAND(TickRenderingTimer)(
			[bPauseRenderingRealtimeClock, DeltaSeconds](FRHICommandListImmediate& RHICmdList)
		{
			if(!bPauseRenderingRealtimeClock)
			{
				// Tick the GRenderingRealtimeClock, unless it's paused
				GRenderingRealtimeClock.Tick(DeltaSeconds);
			}
			
			GRenderTargetPool.TickPoolElements();
			FRDGBuilder::TickPoolElements();
			ICustomResourcePool::TickPoolElements(RHICmdList);
		});
	}

#if WITH_EDITOR
	BroadcastPostEditorTick(DeltaSeconds);

	// Tick the asset registry
	FAssetRegistryModule::TickAssetRegistry(DeltaSeconds);
#endif
}

void USinpleEditorGameEngine::CreateGameViewport_New(UGameViewportClient* GameViewportClient)
{
	
}

void USinpleEditorGameEngine::SwitchGameWindowToUseGameViewport_New()
{
	
}
