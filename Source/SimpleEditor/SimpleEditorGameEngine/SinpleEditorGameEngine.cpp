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
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/ReflectionCaptureComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/CoreSettings.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/EmbeddedCommunication.h"
#include "MovieSceneCapture/Public/IMovieSceneCapture.h"
#include "Net/NetworkProfiler.h"
#include "SimpleEditor/MultiWindowMgr/MultiWindowMgr.h"
#include "Slate/SceneViewport.h"
#include "Slate/SGameLayerManager.h"


#if PLATFORM_WINDOWS
#include "WIndows/WindowsPlatformSplash.h"
# elif  PLATFORM_MAC
#include "Mac/MacPlatformSplash.h"
#endif 


#if WITH_EDITOR
#include "PIEPreviewDeviceProfileSelectorModule.h"
#include "IPIEPreviewDeviceModule.h"
#endif



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
			//FWindowsPlatformSplash::FPlatformSplash::Hide();
			FPlatformSplash::Hide();
			if ( GameViewportWindow.IsValid() )
			{
				// Don't show window in off-screen rendering mode as it doesn't render to screen
				if (!FSlateApplication::Get().IsRenderingOffScreen())
				{
					GameViewportWindow.Pin()->ShowWindow();
				}
				//临时代替
				FSlateApplication::Get().RegisterGameViewport( New_GameViewportWidget.ToSharedRef() );
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
	check(GameViewportWindow.IsValid());

	//先拦截一下，暂时不让引擎得到游戏界面的Widget,之后包裹一下再还给引擎
	if( !New_GameViewportWidget.IsValid() )
	{
		CreateGameViewportWidget_New( GameViewportClient );
	}
	TSharedRef<SViewport> GameViewportWidgetRef = New_GameViewportWidget.ToSharedRef();

	auto Window = GameViewportWindow.Pin();

	Window->SetOnWindowClosed( FOnWindowClosed::CreateUObject( this, &UGameEngine::OnGameWindowClosed ) );

	// SAVEWINPOS tells us to load/save window positions to user settings (this is disabled by default)
	int32 SaveWinPos;
	if (FParse::Value(FCommandLine::Get(), TEXT("SAVEWINPOS="), SaveWinPos) && SaveWinPos > 0 )
	{
		// Get WinX/WinY from GameSettings, apply them if valid.
		FIntPoint PiePosition = GetGameUserSettings()->GetWindowPosition();
		if (PiePosition.X >= 0 && PiePosition.Y >= 0)
		{
			int32 WinX = GetGameUserSettings()->GetWindowPosition().X;
			int32 WinY = GetGameUserSettings()->GetWindowPosition().Y;
			Window->MoveWindowTo(FVector2D(WinX, WinY));
		}
		Window->SetOnWindowMoved( FOnWindowMoved::CreateUObject( this, &UGameEngine::OnGameWindowMoved ) );
	}

	SceneViewport = MakeShareable( GameViewportClient->CreateGameViewport(GameViewportWidgetRef) );
	GameViewportClient->Viewport = SceneViewport.Get();
	//GameViewportClient->CreateHighresScreenshotCaptureRegionWidget(); //  Disabled until mouse based input system can be made to work correctly.

	// The viewport widget needs an interface so it knows what should render
	GameViewportWidgetRef->SetViewportInterface( SceneViewport.ToSharedRef() );

	FSceneViewport* ViewportFrame = SceneViewport.Get();

	GameViewport->SetViewportFrame(ViewportFrame);

	GameViewport->GetGameLayerManager()->SetSceneViewport(ViewportFrame);

	FViewport::ViewportResizedEvent.AddUObject(this, &UGameEngine::OnViewportResized);

	
}

void USinpleEditorGameEngine::CreateGameViewportWidget_New(UGameViewportClient* GameViewportClient)
{
	bool bRenderDirectlyToWindow = (!StartupMovieCaptureHandle.IsValid() || IMovieSceneCaptureModule::Get().IsStereoAllowed()) && GIsDumpingMovie == 0;

	TSharedRef<SOverlay> ViewportOverlayWidgetRef = SNew( SOverlay );

	TSharedRef<SGameLayerManager> GameLayerManagerRef = SNew(SGameLayerManager)
		.SceneViewport_UObject(this, &USinpleEditorGameEngine::GetGameSceneViewport, GameViewportClient)
		[
			ViewportOverlayWidgetRef
		];

	// when we're running in a "device simulation" window, render the scene to an intermediate texture
	// in the mobile device "emulation" case this is needed to properly position the viewport (as a widget) inside its bezel
	#if WITH_EDITOR
	auto PIEPreviewDeviceModule = FModuleManager::LoadModulePtr<IPIEPreviewDeviceModule>("PIEPreviewDeviceProfileSelector");
	if (PIEPreviewDeviceModule && FPIEPreviewDeviceModule::IsRequestingPreviewDevice())
	{
		bRenderDirectlyToWindow = false;
		PIEPreviewDeviceModule->SetGameLayerManagerWidget(GameLayerManagerRef);
	}
#endif

	const bool bStereoAllowed = bRenderDirectlyToWindow;

	TSharedRef<SViewport> GameViewportWidgetRef = 
		SNew( SViewport )
			// Render directly to the window backbuffer unless capturing a movie or getting screenshots
			// @todo TEMP
			.RenderDirectlyToWindow(bRenderDirectlyToWindow)
			//gamma handled by the scene renderer
			.EnableGammaCorrection(false)
			.EnableStereoRendering(bStereoAllowed)
			[
				GameLayerManagerRef
			];

	GameViewportWidget = GameViewportWidgetRef;

	GameViewportClient->SetViewportOverlayWidget( GameViewportWindow.Pin(), ViewportOverlayWidgetRef );
	GameViewportClient->SetGameLayerManager(GameLayerManagerRef);
}

void USinpleEditorGameEngine::SwitchGameWindowToUseGameViewport_New()
{
	

	
	//if (GameViewportWindow.IsValid() && GameViewportWindow.Pin()->GetContent() != GameViewportWidget)
	{
		TSharedPtr<SWidget> Container;
		
		//if( !GameViewportWidget.IsValid() )
		{
			//CreateGameViewport( GameViewport );
			//要创建新的布局
			Container = FMultiWindowMgr::BuildTabManagerWidget(New_GameViewportWidget);
		}
		
		//TSharedRef<SViewport> GameViewportWidgetRef = GameViewportWidget.ToSharedRef();
		//还给GameVIewportWidget,只不过值的类型不是SViewport
		GameViewportWidget = StaticCastSharedRef<SViewport>(Container.Get()->AsShared());

		
		TSharedPtr<SWindow> GameViewportWindowPtr = GameViewportWindow.Pin();
		
		GameViewportWindowPtr->SetContent(Container.ToSharedRef());
		GameViewportWindowPtr->SlatePrepass(FSlateApplication::Get().GetApplicationScale() * GameViewportWindowPtr->GetNativeWindow()->GetDPIScaleFactor());
		
		if ( SceneViewport.IsValid() )
		{
			SceneViewport->ResizeFrame((uint32)GSystemResolution.ResX, (uint32)GSystemResolution.ResY, GSystemResolution.WindowMode);
		}

		// Registration of the game viewport to that messages are correctly received.
		// Could be a re-register, however it's necessary after the window is set.
		FSlateApplication::Get().RegisterGameViewport(New_GameViewportWidget.ToSharedRef());

		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetAllUserFocusToGameViewport(EFocusCause::SetDirectly);
		}
	}
}

FSceneViewport* USinpleEditorGameEngine::GetGameSceneViewport(UGameViewportClient* ViewportClient) const
{
	return ViewportClient->GetGameViewport();
}
