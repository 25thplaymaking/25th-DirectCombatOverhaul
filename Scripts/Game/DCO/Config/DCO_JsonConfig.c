// Server JSON config (load + auto-deploy). Lets a server owner set every DCO
// morale/surrender/recovery/coordination variable up front in one JSON file, on top
// of the baked defaults and the live "25th DCO" GM tab.
//
// File: "$profile:DCO_Settings.json". $profile: is the engine's writable profile
// directory - on a dedicated server, the folder passed via -profile (where the logs
// live). It's per-instance and persists across restarts, which is what a server config
// wants.
//
// Deploy behaviour:
//   - First boot, if the file doesn't exist, write the current defaults to it so the
//     owner finds a fully populated, editable file in their profile dir.
//   - Every later boot the file exists, so we load it (the owner's edits are the baseline).
//   The write is server-only, so a connected client never scribbles into its own profile.
//   A client read is harmless (its file is absent, defaults kept), and gameplay is
//   server-authoritative anyway, so only the server's file drives AI.
//
// Apply order: DCO_MoraleSettings.Get() calls LoadInto once when the singleton is first
// created (the baseline). GM tab changes happen later and override the JSON for that
// session - JSON is the startup baseline, the GM tab is the live override.
//
// Each key is optional on load: a missing key leaves the field at its default, because
// ReadValue only writes the out-value when the key is present. A human-readable
// reference also ships as Configs/DCO_Settings.example.json.
class DCO_JsonConfig
{
	static const string FILE_PATH = "$profile:DCO_Settings.json";

	// Apply the server JSON onto the settings singleton. If the file is missing,
	// auto-deploy the defaults to the profile dir (server-only) so the owner has an
	// editable file next boot. Safe no-op otherwise.
	static void LoadInto(DCO_MoraleSettings cfg)
	{
		if (!cfg)
			return;

		if (!FileIO.FileExists(FILE_PATH))
		{
			// No file yet: on the server, deploy a populated default into the profile dir. Keep the baked
			// in-memory defaults for this session (they equal what we just wrote).
			if (Replication.IsServer())
				WriteDefaults(cfg);
			return;
		}

		// Engine base JsonLoadContext (the SCR_ wrapper is obsolete in 1.7); same API.
		JsonLoadContext ctx = new JsonLoadContext();
		if (!ctx.LoadFromFile(FILE_PATH))
		{
			Print("[DCO] DCO_Settings.json present but failed to parse - keeping defaults: " + FILE_PATH, LogLevel.WARNING);
			return;	// present but unparseable - keep defaults rather than half-applying
		}

		ApplyKeys(ctx, cfg);
		Print("[DCO] Loaded server settings override from " + FILE_PATH, LogLevel.NORMAL);
	}

	// Read every known key from the load context into the settings (missing keys leave the default).
	protected static void ApplyKeys(JsonLoadContext ctx, DCO_MoraleSettings cfg)
	{
		// --- Core morale ---
		ctx.ReadValue("m_bEnabled", cfg.m_bEnabled);
		ctx.ReadValue("m_bDebug", cfg.m_bDebug);
		ctx.ReadValue("m_fUpdateIntervalSec", cfg.m_fUpdateIntervalSec);
		ctx.ReadValue("m_fFleeThreshold", cfg.m_fFleeThreshold);
		ctx.ReadValue("m_fSurrenderThreshold", cfg.m_fSurrenderThreshold);
		ctx.ReadValue("m_fRallyThreshold", cfg.m_fRallyThreshold);
		ctx.ReadValue("m_fRecoveryPerTick", cfg.m_fRecoveryPerTick);
		ctx.ReadValue("m_fDrainPerTick", cfg.m_fDrainPerTick);
		ctx.ReadValue("m_fCasualtyWeight", cfg.m_fCasualtyWeight);
		ctx.ReadValue("m_fHeavyThreat", cfg.m_fHeavyThreat);
		ctx.ReadValue("m_fMinCasualtyFractionForSurrender", cfg.m_fMinCasualtyFractionForSurrender);
		ctx.ReadValue("m_fSurrenderChancePerTick", cfg.m_fSurrenderChancePerTick);
		ctx.ReadValue("m_fFleeChancePerTick", cfg.m_fFleeChancePerTick);
		ctx.ReadValue("m_fSurrenderLullSec", cfg.m_fSurrenderLullSec);
		ctx.ReadValue("m_iSurrenderMinSquadToFight", cfg.m_iSurrenderMinSquadToFight);
		ctx.ReadValue("m_fSurrenderStaggerMaxSec", cfg.m_fSurrenderStaggerMaxSec);
		ctx.ReadValue("m_fMoraleLostPerCasualty", cfg.m_fMoraleLostPerCasualty);
		ctx.ReadValue("m_fMoraleLostOnLeaderLoss", cfg.m_fMoraleLostOnLeaderLoss);
		ctx.ReadValue("m_fFleeDistance", cfg.m_fFleeDistance);

		// --- Surrender behaviour ---
		ctx.ReadValue("m_bEnableSurrender", cfg.m_bEnableSurrender);
		ctx.ReadValue("m_bDropGearOnSurrender", cfg.m_bDropGearOnSurrender);
		ctx.ReadValue("m_iSurrenderDropDelayMs", cfg.m_iSurrenderDropDelayMs);
		ctx.ReadValue("m_iSurrenderProneDelayMs", cfg.m_iSurrenderProneDelayMs);
		ctx.ReadValue("m_bFreezeSurrenderedAI", cfg.m_bFreezeSurrenderedAI);
		ctx.ReadValue("m_iSurrenderFreezeDelayMs", cfg.m_iSurrenderFreezeDelayMs);
		ctx.ReadValue("m_bEnableSurrenderFlee", cfg.m_bEnableSurrenderFlee);
		ctx.ReadValue("m_fSurrenderStopRange", cfg.m_fSurrenderStopRange);
		ctx.ReadValue("m_fSurrenderFleeAwareness", cfg.m_fSurrenderFleeAwareness);
		ctx.ReadValue("m_bEnableLastStand", cfg.m_bEnableLastStand);

		// --- Fake surrender / grenade ambush ---
		ctx.ReadValue("m_bEnableFakeSurrender", cfg.m_bEnableFakeSurrender);
		ctx.ReadValue("m_fFakeSurrenderChance", cfg.m_fFakeSurrenderChance);
		ctx.ReadValue("m_fFakeSurrenderTriggerRange", cfg.m_fFakeSurrenderTriggerRange);
		ctx.ReadValue("m_fFakeSurrenderDropChance", cfg.m_fFakeSurrenderDropChance);
		ctx.ReadValue("m_fFakeSurrenderThrowSpeed", cfg.m_fFakeSurrenderThrowSpeed);
		ctx.ReadValue("m_sFakeSurrenderGrenadePrefab", cfg.m_sFakeSurrenderGrenadePrefab);

		// --- Surrender recovery / re-arm (Phase 10) ---
		ctx.ReadValue("m_bEnableSurrenderRecovery", cfg.m_bEnableSurrenderRecovery);
		ctx.ReadValue("m_fRecoveryMoraleThreshold", cfg.m_fRecoveryMoraleThreshold);
		ctx.ReadValue("m_fRecoveryNoContactSec", cfg.m_fRecoveryNoContactSec);
		ctx.ReadValue("m_fRecoveryMinSurrenderSec", cfg.m_fRecoveryMinSurrenderSec);
		ctx.ReadValue("m_fRecoveryCheckSec", cfg.m_fRecoveryCheckSec);
		ctx.ReadValue("m_fRecoveryChancePerTick", cfg.m_fRecoveryChancePerTick);
		ctx.ReadValue("m_fRecoveryMoralePerTick", cfg.m_fRecoveryMoralePerTick);
		ctx.ReadValue("m_iRecoveryMagazineCount", cfg.m_iRecoveryMagazineCount);
		ctx.ReadValue("m_bRecoverySpawnWeaponIfLost", cfg.m_bRecoverySpawnWeaponIfLost);
		ctx.ReadValue("m_sRecoveryThrowablePrefab", cfg.m_sRecoveryThrowablePrefab);
		ctx.ReadValue("m_iRecoveryThrowableCount", cfg.m_iRecoveryThrowableCount);

		// --- Surrender voiceline ---
		ctx.ReadValue("m_bEnableSurrenderVoice", cfg.m_bEnableSurrenderVoice);
		ctx.ReadValue("m_sSurrenderSoundEvent", cfg.m_sSurrenderSoundEvent);
		ctx.ReadValue("m_iSurrenderVoiceStaggerMs", cfg.m_iSurrenderVoiceStaggerMs);

		// --- Stance cooldown ---
		ctx.ReadValue("m_bEnableStanceCooldown", cfg.m_bEnableStanceCooldown);
		ctx.ReadValue("m_fStanceCooldownSec", cfg.m_fStanceCooldownSec);
		ctx.ReadValue("m_bStanceCooldownInCover", cfg.m_bStanceCooldownInCover);
		ctx.ReadValue("m_bStanceCooldownDebug", cfg.m_bStanceCooldownDebug);

		// --- Panic (R1) ---
		ctx.ReadValue("m_bEnablePanic", cfg.m_bEnablePanic);
		ctx.ReadValue("m_fPanicBand", cfg.m_fPanicBand);
		ctx.ReadValue("m_fPanicChancePerTick", cfg.m_fPanicChancePerTick);
		ctx.ReadValue("m_fPanicDurationSec", cfg.m_fPanicDurationSec);
		ctx.ReadValue("m_fPanicCooldownSec", cfg.m_fPanicCooldownSec);

		// --- Morale -> accuracy (R2) ---
		ctx.ReadValue("m_bEnableMoraleAccuracy", cfg.m_bEnableMoraleAccuracy);
		ctx.ReadValue("m_fAccuracyRookieMorale", cfg.m_fAccuracyRookieMorale);
		ctx.ReadValue("m_fAccuracyRegularMorale", cfg.m_fAccuracyRegularMorale);
		ctx.ReadValue("m_fLowMoraleFireRateCoef", cfg.m_fLowMoraleFireRateCoef);
		ctx.ReadValue("m_bEnableAimZeroing", cfg.m_bEnableAimZeroing);
		ctx.ReadValue("m_fZeroStep1Sec", cfg.m_fZeroStep1Sec);
		ctx.ReadValue("m_fZeroStep2Sec", cfg.m_fZeroStep2Sec);
		ctx.ReadValue("m_fZeroResetSec", cfg.m_fZeroResetSec);
		ctx.ReadValue("m_fZeroCheckSec", cfg.m_fZeroCheckSec);
		ctx.ReadValue("m_bZeroBoostPerception", cfg.m_bZeroBoostPerception);
		ctx.ReadValue("m_fZeroBasePerception", cfg.m_fZeroBasePerception);
		ctx.ReadValue("m_fZeroMaxPerception", cfg.m_fZeroMaxPerception);

		// --- Vehicle armour angling ---
		ctx.ReadValue("m_bEnableVehicleArmor", cfg.m_bEnableVehicleArmor);
		ctx.ReadValue("m_fVehicleFrontalArcDeg", cfg.m_fVehicleFrontalArcDeg);
		ctx.ReadValue("m_fVehicleReorientDeadbandDeg", cfg.m_fVehicleReorientDeadbandDeg);
		ctx.ReadValue("m_fVehicleReorderDist", cfg.m_fVehicleReorderDist);
		ctx.ReadValue("m_fVehicleMinEngageRange", cfg.m_fVehicleMinEngageRange);
		ctx.ReadValue("m_fVehicleCheckIntervalSec", cfg.m_fVehicleCheckIntervalSec);

		// --- Night illum / smoke / hijack / HVT ---
		ctx.ReadValue("m_bEnableNightIllum", cfg.m_bEnableNightIllum);
		ctx.ReadValue("m_fIllumCheckIntervalSec", cfg.m_fIllumCheckIntervalSec);
		ctx.ReadValue("m_bEnableVisionLimit", cfg.m_bEnableVisionLimit);
		ctx.ReadValue("m_fNightPerceptionFactor", cfg.m_fNightPerceptionFactor);
		ctx.ReadValue("m_fVisionSunriseHour", cfg.m_fVisionSunriseHour);
		ctx.ReadValue("m_fVisionSunsetHour", cfg.m_fVisionSunsetHour);
		ctx.ReadValue("m_fVisionTwilightHours", cfg.m_fVisionTwilightHours);
		ctx.ReadValue("m_fVisionCheckSec", cfg.m_fVisionCheckSec);
		ctx.ReadValue("m_bEnableIdleBehaviour", cfg.m_bEnableIdleBehaviour);
		ctx.ReadValue("m_fIdleDelaySec", cfg.m_fIdleDelaySec);
		ctx.ReadValue("m_fIdleMoveEpsilon", cfg.m_fIdleMoveEpsilon);
		ctx.ReadValue("m_fIdleSpreadRadius", cfg.m_fIdleSpreadRadius);
		ctx.ReadValue("m_fIdlePatrolRadius", cfg.m_fIdlePatrolRadius);
		ctx.ReadValue("m_fIdlePatrolIntervalSec", cfg.m_fIdlePatrolIntervalSec);
		ctx.ReadValue("m_fIdlePatrolChance", cfg.m_fIdlePatrolChance);
		ctx.ReadValue("m_bEnableCoverStance", cfg.m_bEnableCoverStance);
		ctx.ReadValue("m_fCoverStanceCheckSec", cfg.m_fCoverStanceCheckSec);
		ctx.ReadValue("m_fCoverStanceMaxRange", cfg.m_fCoverStanceMaxRange);
		ctx.ReadValue("m_fCoverStandEye", cfg.m_fCoverStandEye);
		ctx.ReadValue("m_fCoverCrouchEye", cfg.m_fCoverCrouchEye);
		ctx.ReadValue("m_fCoverProneEye", cfg.m_fCoverProneEye);
		ctx.ReadValue("m_bEnableVocalShare", cfg.m_bEnableVocalShare);
		ctx.ReadValue("m_fVocalRadius", cfg.m_fVocalRadius);
		ctx.ReadValue("m_fVocalCheckSec", cfg.m_fVocalCheckSec);
		ctx.ReadValue("m_iVocalMaxListeners", cfg.m_iVocalMaxListeners);
		ctx.ReadValue("m_bEnableRadioShare", cfg.m_bEnableRadioShare);
		ctx.ReadValue("m_fRadioRadius", cfg.m_fRadioRadius);
		ctx.ReadValue("m_fRadioCheckSec", cfg.m_fRadioCheckSec);
		ctx.ReadValue("m_iRadioMaxListeners", cfg.m_iRadioMaxListeners);
		ctx.ReadValue("m_bRadioFlagsReinforcement", cfg.m_bRadioFlagsReinforcement);
		ctx.ReadValue("m_bEnableMemberMorale", cfg.m_bEnableMemberMorale);
		ctx.ReadValue("m_fMemberMoraleCheckSec", cfg.m_fMemberMoraleCheckSec);
		ctx.ReadValue("m_fMemberMoraleSuppressionWeight", cfg.m_fMemberMoraleSuppressionWeight);
		ctx.ReadValue("m_fMemberMoraleHesitateThreshold", cfg.m_fMemberMoraleHesitateThreshold);
		ctx.ReadValue("m_fMemberMoraleHesitateSec", cfg.m_fMemberMoraleHesitateSec);
		ctx.ReadValue("m_bMemberMoraleCower", cfg.m_bMemberMoraleCower);
		ctx.ReadValue("m_fMemberMoraleCowerThreshold", cfg.m_fMemberMoraleCowerThreshold);
		ctx.ReadValue("m_bEnableEmergencyRearm", cfg.m_bEnableEmergencyRearm);
		ctx.ReadValue("m_fRearmCheckSec", cfg.m_fRearmCheckSec);
		ctx.ReadValue("m_iRearmMagazineCount", cfg.m_iRearmMagazineCount);
		ctx.ReadValue("m_bRearmScavenge", cfg.m_bRearmScavenge);
		ctx.ReadValue("m_fRearmScavengeRange", cfg.m_fRearmScavengeRange);
		ctx.ReadValue("m_fRearmPickupDist", cfg.m_fRearmPickupDist);
		ctx.ReadValue("m_bEnableVehicleCombat", cfg.m_bEnableVehicleCombat);
		ctx.ReadValue("m_fVehicleCombatCheckSec", cfg.m_fVehicleCombatCheckSec);
		ctx.ReadValue("m_fVehicleSightHeight", cfg.m_fVehicleSightHeight);
		ctx.ReadValue("m_fVehicleRepositionRadius", cfg.m_fVehicleRepositionRadius);
		ctx.ReadValue("m_iVehicleSampleCount", cfg.m_iVehicleSampleCount);
		ctx.ReadValue("m_bVehicleBackoff", cfg.m_bVehicleBackoff);
		ctx.ReadValue("m_fVehicleBackoffThreat", cfg.m_fVehicleBackoffThreat);
		ctx.ReadValue("m_fVehicleBackoffHoldSec", cfg.m_fVehicleBackoffHoldSec);
		ctx.ReadValue("m_bEnableCQB", cfg.m_bEnableCQB);
		ctx.ReadValue("m_fCqbCheckSec", cfg.m_fCqbCheckSec);
		ctx.ReadValue("m_fCqbEngageRange", cfg.m_fCqbEngageRange);
		ctx.ReadValue("m_fCqbBuildingScan", cfg.m_fCqbBuildingScan);
		ctx.ReadValue("m_fCqbArriveDist", cfg.m_fCqbArriveDist);
		ctx.ReadValue("m_fCqbReorderDist", cfg.m_fCqbReorderDist);
		ctx.ReadValue("m_bEnableArtillery", cfg.m_bEnableArtillery);
		ctx.ReadValue("m_fArtyCheckSec", cfg.m_fArtyCheckSec);
		ctx.ReadValue("m_iArtyMinTargets", cfg.m_iArtyMinTargets);
		ctx.ReadValue("m_fArtyMinRange", cfg.m_fArtyMinRange);
		ctx.ReadValue("m_fArtyCooldownSec", cfg.m_fArtyCooldownSec);
		ctx.ReadValue("m_fArtyPriority", cfg.m_fArtyPriority);
		ctx.ReadValue("m_bEnableCommander", cfg.m_bEnableCommander);
		ctx.ReadValue("cmd_visualize", cfg.m_bCmdVisualize);
		ctx.ReadValue("m_fCommanderIntervalSec", cfg.m_fCommanderIntervalSec);
		ctx.ReadValue("m_fCommanderThreatTrigger", cfg.m_fCommanderThreatTrigger);
		ctx.ReadValue("m_fCommanderDispatchRange", cfg.m_fCommanderDispatchRange);
		ctx.ReadValue("m_iCommanderMaxDispatch", cfg.m_iCommanderMaxDispatch);
		ctx.ReadValue("m_fCommanderReissueSec", cfg.m_fCommanderReissueSec);
		// Map intelligence (RECOM merge) - key-location areas for the commander.
		ctx.ReadValue("m_bEnableCommanderAreas", cfg.m_bEnableCommanderAreas);
		ctx.ReadValue("m_iCommanderAreaMode", cfg.m_iCommanderAreaMode);
		ctx.ReadValue("m_fAreaCellSize", cfg.m_fAreaCellSize);
		ctx.ReadValue("m_iAreaMinBuildings", cfg.m_iAreaMinBuildings);
		ctx.ReadValue("m_iAreaSignificantWeight", cfg.m_iAreaSignificantWeight);
		ctx.ReadValue("m_fAreaMaxRadius", cfg.m_fAreaMaxRadius);
		ctx.ReadValue("m_iAreaCellsPerStep", cfg.m_iAreaCellsPerStep);
		ctx.ReadValue("m_sAreaMilitaryPrefabs", cfg.m_sAreaMilitaryPrefabs);
		ctx.ReadValue("m_bAreaIncludeForest", cfg.m_bAreaIncludeForest);
		ctx.ReadValue("m_sAreaForestPrefabs", cfg.m_sAreaForestPrefabs);
		ctx.ReadValue("m_iAreaMinForest", cfg.m_iAreaMinForest);
		ctx.ReadValue("m_bAreaHighGroundBias", cfg.m_bAreaHighGroundBias);
		ctx.ReadValue("m_fAreaHighGroundWeight", cfg.m_fAreaHighGroundWeight);
		ctx.ReadValue("m_bDumpMapIntelOnLoad", cfg.m_bDumpMapIntelOnLoad);
		// External AI Commander (RECOM/LLM bridge) - endpoint + API key are server-JSON ONLY.
		ctx.ReadValue("m_bEnableExternalCommander", cfg.m_bEnableExternalCommander);
		ctx.ReadValue("external_endpoint", cfg.m_sExternalEndpoint);
		ctx.ReadValue("external_eval_path", cfg.m_sExternalEvalPath);
		ctx.ReadValue("external_api_key", cfg.m_sExternalApiKey);
		ctx.ReadValue("external_faction", cfg.m_sExternalFaction);
		ctx.ReadValue("external_interval_sec", cfg.m_fExternalIntervalSec);
		ctx.ReadValue("external_provider", cfg.m_iExternalProvider);
		ctx.ReadValue("external_model", cfg.m_sExternalModel);
		ctx.ReadValue("external_max_tokens", cfg.m_iExternalMaxTokens);
		ctx.ReadValue("external_system_prompt", cfg.m_sExternalSystemPrompt);
		ctx.ReadValue("m_bEnableVehicleDoctrine", cfg.m_bEnableVehicleDoctrine);
		ctx.ReadValue("m_fVehDoctrineSectionRadius", cfg.m_fVehDoctrineSectionRadius);
		ctx.ReadValue("m_fVehDoctrineCheckSec", cfg.m_fVehDoctrineCheckSec);
		ctx.ReadValue("m_fVehDoctrineEnterThreat", cfg.m_fVehDoctrineEnterThreat);
		ctx.ReadValue("m_fVehDoctrineHandbackSec", cfg.m_fVehDoctrineHandbackSec);
		ctx.ReadValue("m_fVehDoctrineReorderDist", cfg.m_fVehDoctrineReorderDist);
		ctx.ReadValue("m_fVehDoctrineRoadBlockDist", cfg.m_fVehDoctrineRoadBlockDist);
		ctx.ReadValue("m_bVehDoctrineBounding", cfg.m_bVehDoctrineBounding);
		ctx.ReadValue("m_fVehDoctrineBoundDist", cfg.m_fVehDoctrineBoundDist);
		ctx.ReadValue("m_bVehDoctrineDismountStuck", cfg.m_bVehDoctrineDismountStuck);
		ctx.ReadValue("m_bVehDoctrineRecovery", cfg.m_bVehDoctrineRecovery);
		ctx.ReadValue("m_fVehDoctrineDisabledSec", cfg.m_fVehDoctrineDisabledSec);
		ctx.ReadValue("m_fVehDoctrineDisabledEps", cfg.m_fVehDoctrineDisabledEps);
		ctx.ReadValue("m_bDebugVehDoctrine", cfg.m_bDebugVehDoctrine);
		ctx.ReadValue("m_bEnableCqbClear", cfg.m_bEnableCqbClear);
		ctx.ReadValue("m_bCqbClearMarkedOnly", cfg.m_bCqbClearMarkedOnly);
		ctx.ReadValue("m_fCqbClearRadius", cfg.m_fCqbClearRadius);
		ctx.ReadValue("m_fCqbNodeDwellSec", cfg.m_fCqbNodeDwellSec);
		ctx.ReadValue("m_fCqbStackSpacing", cfg.m_fCqbStackSpacing);
		ctx.ReadValue("m_iCqbMaxBuildings", cfg.m_iCqbMaxBuildings);
		ctx.ReadValue("m_fCqbClearCheckSec", cfg.m_fCqbClearCheckSec);
		ctx.ReadValue("m_fCqbReorderDistClear", cfg.m_fCqbReorderDistClear);
		ctx.ReadValue("m_fCqbApproachTimeoutSec", cfg.m_fCqbApproachTimeoutSec);
		ctx.ReadValue("m_bDebugCqbClear", cfg.m_bDebugCqbClear);
		ctx.ReadValue("m_bEnableGarrison", cfg.m_bEnableGarrison);
		ctx.ReadValue("m_fGarrisonCheckSec", cfg.m_fGarrisonCheckSec);
		ctx.ReadValue("m_fGarrisonRange", cfg.m_fGarrisonRange);
		ctx.ReadValue("m_fGarrisonRingRadius", cfg.m_fGarrisonRingRadius);
		ctx.ReadValue("m_bGarrisonUseSmartActions", cfg.m_bGarrisonUseSmartActions);
		ctx.ReadValue("m_bEnableCompositionDefense", cfg.m_bEnableCompositionDefense);
		ctx.ReadValue("m_fCompositionRadius", cfg.m_fCompositionRadius);
		ctx.ReadValue("m_fCompositionScanSec", cfg.m_fCompositionScanSec);
		ctx.ReadValue("m_fCompositionCheckSec", cfg.m_fCompositionCheckSec);
		ctx.ReadValue("m_bCompositionReposition", cfg.m_bCompositionReposition);
		ctx.ReadValue("m_fCompositionHoldLeash", cfg.m_fCompositionHoldLeash);
		ctx.ReadValue("m_fCompositionMoralePerUnit", cfg.m_fCompositionMoralePerUnit);
		ctx.ReadValue("m_fCompositionMoraleCap", cfg.m_fCompositionMoraleCap);
		ctx.ReadValue("m_bEnableMelee", cfg.m_bEnableMelee);
		ctx.ReadValue("m_fMeleeCheckSec", cfg.m_fMeleeCheckSec);
		ctx.ReadValue("m_fMeleeRange", cfg.m_fMeleeRange);
		ctx.ReadValue("m_bMeleeOnlyWhenDry", cfg.m_bMeleeOnlyWhenDry);
		ctx.ReadValue("m_bEnableMachineGunner", cfg.m_bEnableMachineGunner);
		ctx.ReadValue("m_fMGCheckSec", cfg.m_fMGCheckSec);
		ctx.ReadValue("m_bMGReposition", cfg.m_bMGReposition);
		ctx.ReadValue("m_fMGRepositionRadius", cfg.m_fMGRepositionRadius);
		ctx.ReadValue("m_fMGSightHeight", cfg.m_fMGSightHeight);
		ctx.ReadValue("m_bEnableFleeSmoke", cfg.m_bEnableFleeSmoke);
		ctx.ReadValue("m_bEnableVehicleHijack", cfg.m_bEnableVehicleHijack);
		ctx.ReadValue("m_fHijackRange", cfg.m_fHijackRange);
		ctx.ReadValue("m_fHijackCheckIntervalSec", cfg.m_fHijackCheckIntervalSec);
		ctx.ReadValue("m_bEnableHVTTargeting", cfg.m_bEnableHVTTargeting);
		ctx.ReadValue("m_fHVTCheckIntervalSec", cfg.m_fHVTCheckIntervalSec);

		// --- F2 shared morale / F1 fratricide / F3 straggler merge ---
		ctx.ReadValue("m_bEnableSharedMorale", cfg.m_bEnableSharedMorale);
		ctx.ReadValue("m_fMoralePoolRadius", cfg.m_fMoralePoolRadius);
		ctx.ReadValue("m_bEnableAssetUse", cfg.m_bEnableAssetUse);
		ctx.ReadValue("m_fAssetCheckSec", cfg.m_fAssetCheckSec);
		ctx.ReadValue("m_fAssetRange", cfg.m_fAssetRange);
		ctx.ReadValue("m_bAssetProactive", cfg.m_bAssetProactive);
		ctx.ReadValue("m_bEnableSuppression", cfg.m_bEnableSuppression);
		ctx.ReadValue("m_fSuppressionCheckSec", cfg.m_fSuppressionCheckSec);
		ctx.ReadValue("m_fSuppressionThreshold", cfg.m_fSuppressionThreshold);
		ctx.ReadValue("m_fSuppressedFireRateCoef", cfg.m_fSuppressedFireRateCoef);
		ctx.ReadValue("m_bSuppressionSeekCover", cfg.m_bSuppressionSeekCover);
		ctx.ReadValue("m_fSuppressionCoverRadius", cfg.m_fSuppressionCoverRadius);
		ctx.ReadValue("m_fSuppressionSmokeFraction", cfg.m_fSuppressionSmokeFraction);
		ctx.ReadValue("m_bSuppressionDigIn", cfg.m_bSuppressionDigIn);
		ctx.ReadValue("m_fSuppressionDashMax", cfg.m_fSuppressionDashMax);
		ctx.ReadValue("m_fSuppressionMaxDescend", cfg.m_fSuppressionMaxDescend);
		ctx.ReadValue("m_bEnableMoraleContagion", cfg.m_bEnableMoraleContagion);
		ctx.ReadValue("m_fContagionRadius", cfg.m_fContagionRadius);
		ctx.ReadValue("m_fContagionCheckSec", cfg.m_fContagionCheckSec);
		ctx.ReadValue("m_fContagionMoralePerBrokenGroup", cfg.m_fContagionMoralePerBrokenGroup);
		ctx.ReadValue("m_fContagionMaxLossPerTick", cfg.m_fContagionMaxLossPerTick);
		ctx.ReadValue("m_bEnableSafeEjectGate", cfg.m_bEnableSafeEjectGate);
		ctx.ReadValue("m_fSafeEjectMaxSpeedKmh", cfg.m_fSafeEjectMaxSpeedKmh);
		ctx.ReadValue("m_fSafeEjectMaxHeightM", cfg.m_fSafeEjectMaxHeightM);
		ctx.ReadValue("m_bEnableFriendlyFire", cfg.m_bEnableFriendlyFire);
		ctx.ReadValue("m_fFriendlyFireHold", cfg.m_fFriendlyFireHold);
		ctx.ReadValue("m_fFriendlyLaneCheckSec", cfg.m_fFriendlyLaneCheckSec);
		ctx.ReadValue("m_bEnableStragglerMerge", cfg.m_bEnableStragglerMerge);
		ctx.ReadValue("m_iStragglerSize", cfg.m_iStragglerSize);
		ctx.ReadValue("m_fMergeRadius", cfg.m_fMergeRadius);
		ctx.ReadValue("m_fMergeCheckSec", cfg.m_fMergeCheckSec);

		// --- Reinforcement / shared SA ---
		ctx.ReadValue("m_bEnableReinforcement", cfg.m_bEnableReinforcement);
		ctx.ReadValue("m_fReinforcementRadius", cfg.m_fReinforcementRadius);
		ctx.ReadValue("m_fReinforcementCooldownSec", cfg.m_fReinforcementCooldownSec);
		ctx.ReadValue("m_iReinforcementMaxResponders", cfg.m_iReinforcementMaxResponders);
		ctx.ReadValue("m_bReinforcementConverge", cfg.m_bReinforcementConverge);
		ctx.ReadValue("m_bReinforcementRespectWaypoints", cfg.m_bReinforcementRespectWaypoints);
		ctx.ReadValue("m_fReinforcementReissueSec", cfg.m_fReinforcementReissueSec);
		ctx.ReadValue("m_fReinforcementRetargetDist", cfg.m_fReinforcementRetargetDist);
		ctx.ReadValue("m_bClearWaypointOnOverride", cfg.m_bClearWaypointOnOverride);
		ctx.ReadValue("m_fQRFCriticalMorale", cfg.m_fQRFCriticalMorale);
		ctx.ReadValue("m_fQRFHoldLeash", cfg.m_fQRFHoldLeash);
		ctx.ReadValue("m_bEnableVehicleReinforcement", cfg.m_bEnableVehicleReinforcement);
		ctx.ReadValue("m_fVehicleReinforceMinDist", cfg.m_fVehicleReinforceMinDist);

		// --- Hit flinch / hide from armour ---
		ctx.ReadValue("m_bEnableHitFlinch", cfg.m_bEnableHitFlinch);
		ctx.ReadValue("m_fHitFlinchDuration", cfg.m_fHitFlinchDuration);
		ctx.ReadValue("m_fHitFlinchCooldownSec", cfg.m_fHitFlinchCooldownSec);
		ctx.ReadValue("m_bEnableFleeFromArmor", cfg.m_bEnableFleeFromArmor);
		ctx.ReadValue("m_fFleeFromArmorRange", cfg.m_fFleeFromArmorRange);
		ctx.ReadValue("m_fFleeFromArmorCheckSec", cfg.m_fFleeFromArmorCheckSec);

		// --- Tactical movement / flanking (separate DCO_TacticalMoveSettings singleton) ---
		DCO_TacticalMoveSettings tmove = DCO_TacticalMoveSettings.Get();
		if (tmove)
		{
			ctx.ReadValue("m_bEnableTacticalMove", tmove.m_bEnableTacticalMove);
			ctx.ReadValue("m_fMinMoveDistance", tmove.m_fMinMoveDistance);
			ctx.ReadValue("m_fCheckInInterval", tmove.m_fCheckInInterval);
			ctx.ReadValue("m_fObservePause", tmove.m_fObservePause);
			ctx.ReadValue("m_bAvoidThreatFunnel", tmove.m_bAvoidThreatFunnel);
			ctx.ReadValue("m_fFunnelSidestep", tmove.m_fFunnelSidestep);
			ctx.ReadValue("m_fFunnelCorridorHalfWidth", tmove.m_fFunnelCorridorHalfWidth);
			ctx.ReadValue("m_bEnableFlanking", tmove.m_bEnableFlanking);
			ctx.ReadValue("m_fFlankAngleDeg", tmove.m_fFlankAngleDeg);
			ctx.ReadValue("m_fFlankMaxEnemyDist", tmove.m_fFlankMaxEnemyDist);
			ctx.ReadValue("m_fFlankMinEnemyDist", tmove.m_fFlankMinEnemyDist);
			ctx.ReadValue("m_bEnableExposureScoring", tmove.m_bEnableExposureScoring);
			ctx.ReadValue("m_iExposureLayerMask", tmove.m_iExposureLayerMask);
			ctx.ReadValue("m_fExposureSidestep", tmove.m_fExposureSidestep);
			ctx.ReadValue("m_bEnableProceduralPath", tmove.m_bEnableProceduralPath);
			ctx.ReadValue("m_bPathDebugDraw", tmove.m_bPathDebugDraw);
			ctx.ReadValue("m_fPathReassessSec", tmove.m_fPathReassessSec);
			ctx.ReadValue("m_fPathLegLength", tmove.m_fPathLegLength);
			ctx.ReadValue("m_fPathConeHalfAngleDeg", tmove.m_fPathConeHalfAngleDeg);
			ctx.ReadValue("m_iPathConeSamples", tmove.m_iPathConeSamples);
			ctx.ReadValue("m_fPathArriveDist", tmove.m_fPathArriveDist);
			ctx.ReadValue("m_iPathMaxLegs", tmove.m_iPathMaxLegs);
			ctx.ReadValue("m_fPathThreatActivation", tmove.m_fPathThreatActivation);
			ctx.ReadValue("m_fPathConcealmentBonus", tmove.m_fPathConcealmentBonus);
			ctx.ReadValue("m_fPathStraightBias", tmove.m_fPathStraightBias);
			ctx.ReadValue("m_bEnableBaseOfFireSplit", tmove.m_bEnableBaseOfFireSplit);
			ctx.ReadValue("m_fPathSplitThreatActivation", tmove.m_fPathSplitThreatActivation);
			ctx.ReadValue("m_bPathHaltOnContact", tmove.m_bPathHaltOnContact);
			ctx.ReadValue("m_fPathContactHaltRange", tmove.m_fPathContactHaltRange);
			ctx.ReadValue("m_bPathLeaderInBase", tmove.m_bPathLeaderInBase);
			ctx.ReadValue("m_bPathBaseMaintainLos", tmove.m_bPathBaseMaintainLos);
			ctx.ReadValue("m_fPathBaseLosRadius", tmove.m_fPathBaseLosRadius);
			ctx.ReadValue("m_bEnableTravelingOverwatch", tmove.m_bEnableTravelingOverwatch);
			ctx.ReadValue("m_fPathTravelOverwatchThreat", tmove.m_fPathTravelOverwatchThreat);
			ctx.ReadValue("m_bEnableCoverSeek", tmove.m_bEnableCoverSeek);
			ctx.ReadValue("m_fCoverSeekCheckSec", tmove.m_fCoverSeekCheckSec);
			ctx.ReadValue("m_fCoverSeekRadius", tmove.m_fCoverSeekRadius);
			ctx.ReadValue("m_iCoverSeekSamples", tmove.m_iCoverSeekSamples);
			ctx.ReadValue("m_fCoverMaxClimb", tmove.m_fCoverMaxClimb);
			ctx.ReadValue("m_bEnableReactToContact", tmove.m_bEnableReactToContact);
			ctx.ReadValue("m_fReactCheckSec", tmove.m_fReactCheckSec);
			ctx.ReadValue("m_fSupportFireRateCoef", tmove.m_fSupportFireRateCoef);
			ctx.ReadValue("m_fAssaultFlankAngleDeg", tmove.m_fAssaultFlankAngleDeg);
			ctx.ReadValue("m_bEnableStandoff", tmove.m_bEnableStandoff);
			ctx.ReadValue("m_fStandoffRifle", tmove.m_fStandoffRifle);
			ctx.ReadValue("m_fStandoffMG", tmove.m_fStandoffMG);
			ctx.ReadValue("m_fStandoffSniper", tmove.m_fStandoffSniper);
			ctx.ReadValue("m_fStandoffLauncher", tmove.m_fStandoffLauncher);
			ctx.ReadValue("m_fStandoffPistol", tmove.m_fStandoffPistol);
			ctx.ReadValue("m_fStandoffBand", tmove.m_fStandoffBand);
			ctx.ReadValue("m_fBreakContactDistance", tmove.m_fBreakContactDistance);
			ctx.ReadValue("m_bEnableTacticalBrain", tmove.m_bEnableTacticalBrain);
			ctx.ReadValue("m_fBrainCheckSec", tmove.m_fBrainCheckSec);
			ctx.ReadValue("m_fBrainAssaultPowerRatio", tmove.m_fBrainAssaultPowerRatio);
			ctx.ReadValue("m_fBrainBreakPowerRatio", tmove.m_fBrainBreakPowerRatio);
			ctx.ReadValue("m_fBrainEnemyScanRadius", tmove.m_fBrainEnemyScanRadius);
			ctx.ReadValue("m_bEnableDcoFormations", tmove.m_bEnableDcoFormations);
			ctx.ReadValue("m_fFormationShapeCheckSec", tmove.m_fFormationShapeCheckSec);
			ctx.ReadValue("m_fFormationSpacing", tmove.m_fFormationSpacing);
			ctx.ReadValue("m_iFormationTravelShape", tmove.m_iFormationTravelShape);
			ctx.ReadValue("m_iFormationContactShape", tmove.m_iFormationContactShape);
		}

		// --- Adaptive formation ---
		ctx.ReadValue("m_bEnableAdaptiveFormation", cfg.m_bEnableAdaptiveFormation);
		ctx.ReadValue("m_fFormationCheckSec", cfg.m_fFormationCheckSec);
		ctx.ReadValue("m_sFormationTravel", cfg.m_sFormationTravel);
		ctx.ReadValue("m_sFormationContact", cfg.m_sFormationContact);
	}

	// Write the current settings out to the profile JSON (server-only auto-deploy on
	// first boot). Keys mirror ApplyKeys 1:1 so the file round-trips through the loader.
	protected static void WriteDefaults(DCO_MoraleSettings cfg)
	{
		// Engine base JsonSaveContext (the SCR_ wrapper is obsolete in 1.7); same API.
		JsonSaveContext ctx = new JsonSaveContext();

		// --- Core morale ---
		ctx.WriteValue("m_bEnabled", cfg.m_bEnabled);
		ctx.WriteValue("m_bDebug", cfg.m_bDebug);
		ctx.WriteValue("m_fUpdateIntervalSec", cfg.m_fUpdateIntervalSec);
		ctx.WriteValue("m_fFleeThreshold", cfg.m_fFleeThreshold);
		ctx.WriteValue("m_fSurrenderThreshold", cfg.m_fSurrenderThreshold);
		ctx.WriteValue("m_fRallyThreshold", cfg.m_fRallyThreshold);
		ctx.WriteValue("m_fRecoveryPerTick", cfg.m_fRecoveryPerTick);
		ctx.WriteValue("m_fDrainPerTick", cfg.m_fDrainPerTick);
		ctx.WriteValue("m_fCasualtyWeight", cfg.m_fCasualtyWeight);
		ctx.WriteValue("m_fHeavyThreat", cfg.m_fHeavyThreat);
		ctx.WriteValue("m_fMinCasualtyFractionForSurrender", cfg.m_fMinCasualtyFractionForSurrender);
		ctx.WriteValue("m_fSurrenderChancePerTick", cfg.m_fSurrenderChancePerTick);
		ctx.WriteValue("m_fFleeChancePerTick", cfg.m_fFleeChancePerTick);
		ctx.WriteValue("m_fSurrenderLullSec", cfg.m_fSurrenderLullSec);
		ctx.WriteValue("m_iSurrenderMinSquadToFight", cfg.m_iSurrenderMinSquadToFight);
		ctx.WriteValue("m_fSurrenderStaggerMaxSec", cfg.m_fSurrenderStaggerMaxSec);
		ctx.WriteValue("m_fMoraleLostPerCasualty", cfg.m_fMoraleLostPerCasualty);
		ctx.WriteValue("m_fMoraleLostOnLeaderLoss", cfg.m_fMoraleLostOnLeaderLoss);
		ctx.WriteValue("m_fFleeDistance", cfg.m_fFleeDistance);

		// --- Surrender behaviour ---
		ctx.WriteValue("m_bEnableSurrender", cfg.m_bEnableSurrender);
		ctx.WriteValue("m_bDropGearOnSurrender", cfg.m_bDropGearOnSurrender);
		ctx.WriteValue("m_iSurrenderDropDelayMs", cfg.m_iSurrenderDropDelayMs);
		ctx.WriteValue("m_iSurrenderProneDelayMs", cfg.m_iSurrenderProneDelayMs);
		ctx.WriteValue("m_bFreezeSurrenderedAI", cfg.m_bFreezeSurrenderedAI);
		ctx.WriteValue("m_iSurrenderFreezeDelayMs", cfg.m_iSurrenderFreezeDelayMs);
		ctx.WriteValue("m_bEnableSurrenderFlee", cfg.m_bEnableSurrenderFlee);
		ctx.WriteValue("m_fSurrenderStopRange", cfg.m_fSurrenderStopRange);
		ctx.WriteValue("m_fSurrenderFleeAwareness", cfg.m_fSurrenderFleeAwareness);
		ctx.WriteValue("m_bEnableLastStand", cfg.m_bEnableLastStand);

		// --- Fake surrender / grenade ambush ---
		ctx.WriteValue("m_bEnableFakeSurrender", cfg.m_bEnableFakeSurrender);
		ctx.WriteValue("m_fFakeSurrenderChance", cfg.m_fFakeSurrenderChance);
		ctx.WriteValue("m_fFakeSurrenderTriggerRange", cfg.m_fFakeSurrenderTriggerRange);
		ctx.WriteValue("m_fFakeSurrenderDropChance", cfg.m_fFakeSurrenderDropChance);
		ctx.WriteValue("m_fFakeSurrenderThrowSpeed", cfg.m_fFakeSurrenderThrowSpeed);
		ctx.WriteValue("m_sFakeSurrenderGrenadePrefab", cfg.m_sFakeSurrenderGrenadePrefab);

		// --- Surrender recovery / re-arm (Phase 10) ---
		ctx.WriteValue("m_bEnableSurrenderRecovery", cfg.m_bEnableSurrenderRecovery);
		ctx.WriteValue("m_fRecoveryMoraleThreshold", cfg.m_fRecoveryMoraleThreshold);
		ctx.WriteValue("m_fRecoveryNoContactSec", cfg.m_fRecoveryNoContactSec);
		ctx.WriteValue("m_fRecoveryMinSurrenderSec", cfg.m_fRecoveryMinSurrenderSec);
		ctx.WriteValue("m_fRecoveryCheckSec", cfg.m_fRecoveryCheckSec);
		ctx.WriteValue("m_fRecoveryChancePerTick", cfg.m_fRecoveryChancePerTick);
		ctx.WriteValue("m_fRecoveryMoralePerTick", cfg.m_fRecoveryMoralePerTick);
		ctx.WriteValue("m_iRecoveryMagazineCount", cfg.m_iRecoveryMagazineCount);
		ctx.WriteValue("m_bRecoverySpawnWeaponIfLost", cfg.m_bRecoverySpawnWeaponIfLost);
		ctx.WriteValue("m_sRecoveryThrowablePrefab", cfg.m_sRecoveryThrowablePrefab);
		ctx.WriteValue("m_iRecoveryThrowableCount", cfg.m_iRecoveryThrowableCount);

		// --- Surrender voiceline ---
		ctx.WriteValue("m_bEnableSurrenderVoice", cfg.m_bEnableSurrenderVoice);
		ctx.WriteValue("m_sSurrenderSoundEvent", cfg.m_sSurrenderSoundEvent);
		ctx.WriteValue("m_iSurrenderVoiceStaggerMs", cfg.m_iSurrenderVoiceStaggerMs);

		// --- Stance cooldown ---
		ctx.WriteValue("m_bEnableStanceCooldown", cfg.m_bEnableStanceCooldown);
		ctx.WriteValue("m_fStanceCooldownSec", cfg.m_fStanceCooldownSec);
		ctx.WriteValue("m_bStanceCooldownInCover", cfg.m_bStanceCooldownInCover);
		ctx.WriteValue("m_bStanceCooldownDebug", cfg.m_bStanceCooldownDebug);

		// --- Panic (R1) ---
		ctx.WriteValue("m_bEnablePanic", cfg.m_bEnablePanic);
		ctx.WriteValue("m_fPanicBand", cfg.m_fPanicBand);
		ctx.WriteValue("m_fPanicChancePerTick", cfg.m_fPanicChancePerTick);
		ctx.WriteValue("m_fPanicDurationSec", cfg.m_fPanicDurationSec);
		ctx.WriteValue("m_fPanicCooldownSec", cfg.m_fPanicCooldownSec);

		// --- Morale -> accuracy (R2) ---
		ctx.WriteValue("m_bEnableMoraleAccuracy", cfg.m_bEnableMoraleAccuracy);
		ctx.WriteValue("m_fAccuracyRookieMorale", cfg.m_fAccuracyRookieMorale);
		ctx.WriteValue("m_fAccuracyRegularMorale", cfg.m_fAccuracyRegularMorale);
		ctx.WriteValue("m_fLowMoraleFireRateCoef", cfg.m_fLowMoraleFireRateCoef);
		ctx.WriteValue("m_bEnableAimZeroing", cfg.m_bEnableAimZeroing);
		ctx.WriteValue("m_fZeroStep1Sec", cfg.m_fZeroStep1Sec);
		ctx.WriteValue("m_fZeroStep2Sec", cfg.m_fZeroStep2Sec);
		ctx.WriteValue("m_fZeroResetSec", cfg.m_fZeroResetSec);
		ctx.WriteValue("m_fZeroCheckSec", cfg.m_fZeroCheckSec);
		ctx.WriteValue("m_bZeroBoostPerception", cfg.m_bZeroBoostPerception);
		ctx.WriteValue("m_fZeroBasePerception", cfg.m_fZeroBasePerception);
		ctx.WriteValue("m_fZeroMaxPerception", cfg.m_fZeroMaxPerception);

		// --- Vehicle armour angling ---
		ctx.WriteValue("m_bEnableVehicleArmor", cfg.m_bEnableVehicleArmor);
		ctx.WriteValue("m_fVehicleFrontalArcDeg", cfg.m_fVehicleFrontalArcDeg);
		ctx.WriteValue("m_fVehicleReorientDeadbandDeg", cfg.m_fVehicleReorientDeadbandDeg);
		ctx.WriteValue("m_fVehicleReorderDist", cfg.m_fVehicleReorderDist);
		ctx.WriteValue("m_fVehicleMinEngageRange", cfg.m_fVehicleMinEngageRange);
		ctx.WriteValue("m_fVehicleCheckIntervalSec", cfg.m_fVehicleCheckIntervalSec);

		// --- Night illum / smoke / hijack / HVT ---
		ctx.WriteValue("m_bEnableNightIllum", cfg.m_bEnableNightIllum);
		ctx.WriteValue("m_fIllumCheckIntervalSec", cfg.m_fIllumCheckIntervalSec);
		ctx.WriteValue("m_bEnableVisionLimit", cfg.m_bEnableVisionLimit);
		ctx.WriteValue("m_fNightPerceptionFactor", cfg.m_fNightPerceptionFactor);
		ctx.WriteValue("m_fVisionSunriseHour", cfg.m_fVisionSunriseHour);
		ctx.WriteValue("m_fVisionSunsetHour", cfg.m_fVisionSunsetHour);
		ctx.WriteValue("m_fVisionTwilightHours", cfg.m_fVisionTwilightHours);
		ctx.WriteValue("m_fVisionCheckSec", cfg.m_fVisionCheckSec);
		ctx.WriteValue("m_bEnableIdleBehaviour", cfg.m_bEnableIdleBehaviour);
		ctx.WriteValue("m_fIdleDelaySec", cfg.m_fIdleDelaySec);
		ctx.WriteValue("m_fIdleMoveEpsilon", cfg.m_fIdleMoveEpsilon);
		ctx.WriteValue("m_fIdleSpreadRadius", cfg.m_fIdleSpreadRadius);
		ctx.WriteValue("m_fIdlePatrolRadius", cfg.m_fIdlePatrolRadius);
		ctx.WriteValue("m_fIdlePatrolIntervalSec", cfg.m_fIdlePatrolIntervalSec);
		ctx.WriteValue("m_fIdlePatrolChance", cfg.m_fIdlePatrolChance);
		ctx.WriteValue("m_bEnableCoverStance", cfg.m_bEnableCoverStance);
		ctx.WriteValue("m_fCoverStanceCheckSec", cfg.m_fCoverStanceCheckSec);
		ctx.WriteValue("m_fCoverStanceMaxRange", cfg.m_fCoverStanceMaxRange);
		ctx.WriteValue("m_fCoverStandEye", cfg.m_fCoverStandEye);
		ctx.WriteValue("m_fCoverCrouchEye", cfg.m_fCoverCrouchEye);
		ctx.WriteValue("m_fCoverProneEye", cfg.m_fCoverProneEye);
		ctx.WriteValue("m_bEnableVocalShare", cfg.m_bEnableVocalShare);
		ctx.WriteValue("m_fVocalRadius", cfg.m_fVocalRadius);
		ctx.WriteValue("m_fVocalCheckSec", cfg.m_fVocalCheckSec);
		ctx.WriteValue("m_iVocalMaxListeners", cfg.m_iVocalMaxListeners);
		ctx.WriteValue("m_bEnableRadioShare", cfg.m_bEnableRadioShare);
		ctx.WriteValue("m_fRadioRadius", cfg.m_fRadioRadius);
		ctx.WriteValue("m_fRadioCheckSec", cfg.m_fRadioCheckSec);
		ctx.WriteValue("m_iRadioMaxListeners", cfg.m_iRadioMaxListeners);
		ctx.WriteValue("m_bRadioFlagsReinforcement", cfg.m_bRadioFlagsReinforcement);
		ctx.WriteValue("m_bEnableMemberMorale", cfg.m_bEnableMemberMorale);
		ctx.WriteValue("m_fMemberMoraleCheckSec", cfg.m_fMemberMoraleCheckSec);
		ctx.WriteValue("m_fMemberMoraleSuppressionWeight", cfg.m_fMemberMoraleSuppressionWeight);
		ctx.WriteValue("m_fMemberMoraleHesitateThreshold", cfg.m_fMemberMoraleHesitateThreshold);
		ctx.WriteValue("m_fMemberMoraleHesitateSec", cfg.m_fMemberMoraleHesitateSec);
		ctx.WriteValue("m_bMemberMoraleCower", cfg.m_bMemberMoraleCower);
		ctx.WriteValue("m_fMemberMoraleCowerThreshold", cfg.m_fMemberMoraleCowerThreshold);
		ctx.WriteValue("m_bEnableEmergencyRearm", cfg.m_bEnableEmergencyRearm);
		ctx.WriteValue("m_fRearmCheckSec", cfg.m_fRearmCheckSec);
		ctx.WriteValue("m_iRearmMagazineCount", cfg.m_iRearmMagazineCount);
		ctx.WriteValue("m_bRearmScavenge", cfg.m_bRearmScavenge);
		ctx.WriteValue("m_fRearmScavengeRange", cfg.m_fRearmScavengeRange);
		ctx.WriteValue("m_fRearmPickupDist", cfg.m_fRearmPickupDist);
		ctx.WriteValue("m_bEnableVehicleCombat", cfg.m_bEnableVehicleCombat);
		ctx.WriteValue("m_fVehicleCombatCheckSec", cfg.m_fVehicleCombatCheckSec);
		ctx.WriteValue("m_fVehicleSightHeight", cfg.m_fVehicleSightHeight);
		ctx.WriteValue("m_fVehicleRepositionRadius", cfg.m_fVehicleRepositionRadius);
		ctx.WriteValue("m_iVehicleSampleCount", cfg.m_iVehicleSampleCount);
		ctx.WriteValue("m_bVehicleBackoff", cfg.m_bVehicleBackoff);
		ctx.WriteValue("m_fVehicleBackoffThreat", cfg.m_fVehicleBackoffThreat);
		ctx.WriteValue("m_fVehicleBackoffHoldSec", cfg.m_fVehicleBackoffHoldSec);
		ctx.WriteValue("m_bEnableCQB", cfg.m_bEnableCQB);
		ctx.WriteValue("m_fCqbCheckSec", cfg.m_fCqbCheckSec);
		ctx.WriteValue("m_fCqbEngageRange", cfg.m_fCqbEngageRange);
		ctx.WriteValue("m_fCqbBuildingScan", cfg.m_fCqbBuildingScan);
		ctx.WriteValue("m_fCqbArriveDist", cfg.m_fCqbArriveDist);
		ctx.WriteValue("m_fCqbReorderDist", cfg.m_fCqbReorderDist);
		ctx.WriteValue("m_bEnableArtillery", cfg.m_bEnableArtillery);
		ctx.WriteValue("m_fArtyCheckSec", cfg.m_fArtyCheckSec);
		ctx.WriteValue("m_iArtyMinTargets", cfg.m_iArtyMinTargets);
		ctx.WriteValue("m_fArtyMinRange", cfg.m_fArtyMinRange);
		ctx.WriteValue("m_fArtyCooldownSec", cfg.m_fArtyCooldownSec);
		ctx.WriteValue("m_fArtyPriority", cfg.m_fArtyPriority);
		ctx.WriteValue("m_bEnableCommander", cfg.m_bEnableCommander);
		ctx.WriteValue("cmd_visualize", cfg.m_bCmdVisualize);
		ctx.WriteValue("m_fCommanderIntervalSec", cfg.m_fCommanderIntervalSec);
		ctx.WriteValue("m_fCommanderThreatTrigger", cfg.m_fCommanderThreatTrigger);
		ctx.WriteValue("m_fCommanderDispatchRange", cfg.m_fCommanderDispatchRange);
		ctx.WriteValue("m_iCommanderMaxDispatch", cfg.m_iCommanderMaxDispatch);
		ctx.WriteValue("m_fCommanderReissueSec", cfg.m_fCommanderReissueSec);
		// Map intelligence (RECOM merge) - key-location areas for the commander.
		ctx.WriteValue("m_bEnableCommanderAreas", cfg.m_bEnableCommanderAreas);
		ctx.WriteValue("m_iCommanderAreaMode", cfg.m_iCommanderAreaMode);
		ctx.WriteValue("m_fAreaCellSize", cfg.m_fAreaCellSize);
		ctx.WriteValue("m_iAreaMinBuildings", cfg.m_iAreaMinBuildings);
		ctx.WriteValue("m_iAreaSignificantWeight", cfg.m_iAreaSignificantWeight);
		ctx.WriteValue("m_fAreaMaxRadius", cfg.m_fAreaMaxRadius);
		ctx.WriteValue("m_iAreaCellsPerStep", cfg.m_iAreaCellsPerStep);
		ctx.WriteValue("m_sAreaMilitaryPrefabs", cfg.m_sAreaMilitaryPrefabs);
		ctx.WriteValue("m_bAreaIncludeForest", cfg.m_bAreaIncludeForest);
		ctx.WriteValue("m_sAreaForestPrefabs", cfg.m_sAreaForestPrefabs);
		ctx.WriteValue("m_iAreaMinForest", cfg.m_iAreaMinForest);
		ctx.WriteValue("m_bAreaHighGroundBias", cfg.m_bAreaHighGroundBias);
		ctx.WriteValue("m_fAreaHighGroundWeight", cfg.m_fAreaHighGroundWeight);
		ctx.WriteValue("m_bDumpMapIntelOnLoad", cfg.m_bDumpMapIntelOnLoad);
		// External AI Commander (RECOM/LLM bridge). external_api_key ships empty - the owner fills it in their profile.
		ctx.WriteValue("m_bEnableExternalCommander", cfg.m_bEnableExternalCommander);
		ctx.WriteValue("external_endpoint", cfg.m_sExternalEndpoint);
		ctx.WriteValue("external_eval_path", cfg.m_sExternalEvalPath);
		ctx.WriteValue("external_api_key", cfg.m_sExternalApiKey);
		ctx.WriteValue("external_faction", cfg.m_sExternalFaction);
		ctx.WriteValue("external_interval_sec", cfg.m_fExternalIntervalSec);
		ctx.WriteValue("external_provider", cfg.m_iExternalProvider);
		ctx.WriteValue("external_model", cfg.m_sExternalModel);
		ctx.WriteValue("external_max_tokens", cfg.m_iExternalMaxTokens);
		ctx.WriteValue("external_system_prompt", cfg.m_sExternalSystemPrompt);
		ctx.WriteValue("m_bEnableVehicleDoctrine", cfg.m_bEnableVehicleDoctrine);
		ctx.WriteValue("m_fVehDoctrineSectionRadius", cfg.m_fVehDoctrineSectionRadius);
		ctx.WriteValue("m_fVehDoctrineCheckSec", cfg.m_fVehDoctrineCheckSec);
		ctx.WriteValue("m_fVehDoctrineEnterThreat", cfg.m_fVehDoctrineEnterThreat);
		ctx.WriteValue("m_fVehDoctrineHandbackSec", cfg.m_fVehDoctrineHandbackSec);
		ctx.WriteValue("m_fVehDoctrineReorderDist", cfg.m_fVehDoctrineReorderDist);
		ctx.WriteValue("m_fVehDoctrineRoadBlockDist", cfg.m_fVehDoctrineRoadBlockDist);
		ctx.WriteValue("m_bVehDoctrineBounding", cfg.m_bVehDoctrineBounding);
		ctx.WriteValue("m_fVehDoctrineBoundDist", cfg.m_fVehDoctrineBoundDist);
		ctx.WriteValue("m_bVehDoctrineDismountStuck", cfg.m_bVehDoctrineDismountStuck);
		ctx.WriteValue("m_bVehDoctrineRecovery", cfg.m_bVehDoctrineRecovery);
		ctx.WriteValue("m_fVehDoctrineDisabledSec", cfg.m_fVehDoctrineDisabledSec);
		ctx.WriteValue("m_fVehDoctrineDisabledEps", cfg.m_fVehDoctrineDisabledEps);
		ctx.WriteValue("m_bDebugVehDoctrine", cfg.m_bDebugVehDoctrine);
		ctx.WriteValue("m_bEnableCqbClear", cfg.m_bEnableCqbClear);
		ctx.WriteValue("m_bCqbClearMarkedOnly", cfg.m_bCqbClearMarkedOnly);
		ctx.WriteValue("m_fCqbClearRadius", cfg.m_fCqbClearRadius);
		ctx.WriteValue("m_fCqbNodeDwellSec", cfg.m_fCqbNodeDwellSec);
		ctx.WriteValue("m_fCqbStackSpacing", cfg.m_fCqbStackSpacing);
		ctx.WriteValue("m_iCqbMaxBuildings", cfg.m_iCqbMaxBuildings);
		ctx.WriteValue("m_fCqbClearCheckSec", cfg.m_fCqbClearCheckSec);
		ctx.WriteValue("m_fCqbReorderDistClear", cfg.m_fCqbReorderDistClear);
		ctx.WriteValue("m_fCqbApproachTimeoutSec", cfg.m_fCqbApproachTimeoutSec);
		ctx.WriteValue("m_bDebugCqbClear", cfg.m_bDebugCqbClear);
		ctx.WriteValue("m_bEnableGarrison", cfg.m_bEnableGarrison);
		ctx.WriteValue("m_fGarrisonCheckSec", cfg.m_fGarrisonCheckSec);
		ctx.WriteValue("m_fGarrisonRange", cfg.m_fGarrisonRange);
		ctx.WriteValue("m_fGarrisonRingRadius", cfg.m_fGarrisonRingRadius);
		ctx.WriteValue("m_bGarrisonUseSmartActions", cfg.m_bGarrisonUseSmartActions);
		ctx.WriteValue("m_bEnableCompositionDefense", cfg.m_bEnableCompositionDefense);
		ctx.WriteValue("m_fCompositionRadius", cfg.m_fCompositionRadius);
		ctx.WriteValue("m_fCompositionScanSec", cfg.m_fCompositionScanSec);
		ctx.WriteValue("m_fCompositionCheckSec", cfg.m_fCompositionCheckSec);
		ctx.WriteValue("m_bCompositionReposition", cfg.m_bCompositionReposition);
		ctx.WriteValue("m_fCompositionHoldLeash", cfg.m_fCompositionHoldLeash);
		ctx.WriteValue("m_fCompositionMoralePerUnit", cfg.m_fCompositionMoralePerUnit);
		ctx.WriteValue("m_fCompositionMoraleCap", cfg.m_fCompositionMoraleCap);
		ctx.WriteValue("m_bEnableMelee", cfg.m_bEnableMelee);
		ctx.WriteValue("m_fMeleeCheckSec", cfg.m_fMeleeCheckSec);
		ctx.WriteValue("m_fMeleeRange", cfg.m_fMeleeRange);
		ctx.WriteValue("m_bMeleeOnlyWhenDry", cfg.m_bMeleeOnlyWhenDry);
		ctx.WriteValue("m_bEnableMachineGunner", cfg.m_bEnableMachineGunner);
		ctx.WriteValue("m_fMGCheckSec", cfg.m_fMGCheckSec);
		ctx.WriteValue("m_bMGReposition", cfg.m_bMGReposition);
		ctx.WriteValue("m_fMGRepositionRadius", cfg.m_fMGRepositionRadius);
		ctx.WriteValue("m_fMGSightHeight", cfg.m_fMGSightHeight);
		ctx.WriteValue("m_bEnableFleeSmoke", cfg.m_bEnableFleeSmoke);
		ctx.WriteValue("m_bEnableVehicleHijack", cfg.m_bEnableVehicleHijack);
		ctx.WriteValue("m_fHijackRange", cfg.m_fHijackRange);
		ctx.WriteValue("m_fHijackCheckIntervalSec", cfg.m_fHijackCheckIntervalSec);
		ctx.WriteValue("m_bEnableHVTTargeting", cfg.m_bEnableHVTTargeting);
		ctx.WriteValue("m_fHVTCheckIntervalSec", cfg.m_fHVTCheckIntervalSec);

		// --- F2 shared morale / F1 fratricide / F3 straggler merge ---
		ctx.WriteValue("m_bEnableSharedMorale", cfg.m_bEnableSharedMorale);
		ctx.WriteValue("m_fMoralePoolRadius", cfg.m_fMoralePoolRadius);
		ctx.WriteValue("m_bEnableAssetUse", cfg.m_bEnableAssetUse);
		ctx.WriteValue("m_fAssetCheckSec", cfg.m_fAssetCheckSec);
		ctx.WriteValue("m_fAssetRange", cfg.m_fAssetRange);
		ctx.WriteValue("m_bAssetProactive", cfg.m_bAssetProactive);
		ctx.WriteValue("m_bEnableSuppression", cfg.m_bEnableSuppression);
		ctx.WriteValue("m_fSuppressionCheckSec", cfg.m_fSuppressionCheckSec);
		ctx.WriteValue("m_fSuppressionThreshold", cfg.m_fSuppressionThreshold);
		ctx.WriteValue("m_fSuppressedFireRateCoef", cfg.m_fSuppressedFireRateCoef);
		ctx.WriteValue("m_bSuppressionSeekCover", cfg.m_bSuppressionSeekCover);
		ctx.WriteValue("m_fSuppressionCoverRadius", cfg.m_fSuppressionCoverRadius);
		ctx.WriteValue("m_fSuppressionSmokeFraction", cfg.m_fSuppressionSmokeFraction);
		ctx.WriteValue("m_bSuppressionDigIn", cfg.m_bSuppressionDigIn);
		ctx.WriteValue("m_fSuppressionDashMax", cfg.m_fSuppressionDashMax);
		ctx.WriteValue("m_fSuppressionMaxDescend", cfg.m_fSuppressionMaxDescend);
		ctx.WriteValue("m_bEnableMoraleContagion", cfg.m_bEnableMoraleContagion);
		ctx.WriteValue("m_fContagionRadius", cfg.m_fContagionRadius);
		ctx.WriteValue("m_fContagionCheckSec", cfg.m_fContagionCheckSec);
		ctx.WriteValue("m_fContagionMoralePerBrokenGroup", cfg.m_fContagionMoralePerBrokenGroup);
		ctx.WriteValue("m_fContagionMaxLossPerTick", cfg.m_fContagionMaxLossPerTick);
		ctx.WriteValue("m_bEnableSafeEjectGate", cfg.m_bEnableSafeEjectGate);
		ctx.WriteValue("m_fSafeEjectMaxSpeedKmh", cfg.m_fSafeEjectMaxSpeedKmh);
		ctx.WriteValue("m_fSafeEjectMaxHeightM", cfg.m_fSafeEjectMaxHeightM);
		ctx.WriteValue("m_bEnableFriendlyFire", cfg.m_bEnableFriendlyFire);
		ctx.WriteValue("m_fFriendlyFireHold", cfg.m_fFriendlyFireHold);
		ctx.WriteValue("m_fFriendlyLaneCheckSec", cfg.m_fFriendlyLaneCheckSec);
		ctx.WriteValue("m_bEnableStragglerMerge", cfg.m_bEnableStragglerMerge);
		ctx.WriteValue("m_iStragglerSize", cfg.m_iStragglerSize);
		ctx.WriteValue("m_fMergeRadius", cfg.m_fMergeRadius);
		ctx.WriteValue("m_fMergeCheckSec", cfg.m_fMergeCheckSec);

		// --- Reinforcement / shared SA ---
		ctx.WriteValue("m_bEnableReinforcement", cfg.m_bEnableReinforcement);
		ctx.WriteValue("m_fReinforcementRadius", cfg.m_fReinforcementRadius);
		ctx.WriteValue("m_fReinforcementCooldownSec", cfg.m_fReinforcementCooldownSec);
		ctx.WriteValue("m_iReinforcementMaxResponders", cfg.m_iReinforcementMaxResponders);
		ctx.WriteValue("m_bReinforcementConverge", cfg.m_bReinforcementConverge);
		ctx.WriteValue("m_bReinforcementRespectWaypoints", cfg.m_bReinforcementRespectWaypoints);
		ctx.WriteValue("m_fReinforcementReissueSec", cfg.m_fReinforcementReissueSec);
		ctx.WriteValue("m_fReinforcementRetargetDist", cfg.m_fReinforcementRetargetDist);
		ctx.WriteValue("m_bClearWaypointOnOverride", cfg.m_bClearWaypointOnOverride);
		ctx.WriteValue("m_fQRFCriticalMorale", cfg.m_fQRFCriticalMorale);
		ctx.WriteValue("m_fQRFHoldLeash", cfg.m_fQRFHoldLeash);
		ctx.WriteValue("m_bEnableVehicleReinforcement", cfg.m_bEnableVehicleReinforcement);
		ctx.WriteValue("m_fVehicleReinforceMinDist", cfg.m_fVehicleReinforceMinDist);

		// --- Hit flinch / hide from armour ---
		ctx.WriteValue("m_bEnableHitFlinch", cfg.m_bEnableHitFlinch);
		ctx.WriteValue("m_fHitFlinchDuration", cfg.m_fHitFlinchDuration);
		ctx.WriteValue("m_fHitFlinchCooldownSec", cfg.m_fHitFlinchCooldownSec);
		ctx.WriteValue("m_bEnableFleeFromArmor", cfg.m_bEnableFleeFromArmor);
		ctx.WriteValue("m_fFleeFromArmorRange", cfg.m_fFleeFromArmorRange);
		ctx.WriteValue("m_fFleeFromArmorCheckSec", cfg.m_fFleeFromArmorCheckSec);

		// --- Tactical movement / flanking (separate DCO_TacticalMoveSettings singleton) ---
		DCO_TacticalMoveSettings tmove = DCO_TacticalMoveSettings.Get();
		if (tmove)
		{
			ctx.WriteValue("m_bEnableTacticalMove", tmove.m_bEnableTacticalMove);
			ctx.WriteValue("m_fMinMoveDistance", tmove.m_fMinMoveDistance);
			ctx.WriteValue("m_fCheckInInterval", tmove.m_fCheckInInterval);
			ctx.WriteValue("m_fObservePause", tmove.m_fObservePause);
			ctx.WriteValue("m_bAvoidThreatFunnel", tmove.m_bAvoidThreatFunnel);
			ctx.WriteValue("m_fFunnelSidestep", tmove.m_fFunnelSidestep);
			ctx.WriteValue("m_fFunnelCorridorHalfWidth", tmove.m_fFunnelCorridorHalfWidth);
			ctx.WriteValue("m_bEnableFlanking", tmove.m_bEnableFlanking);
			ctx.WriteValue("m_fFlankAngleDeg", tmove.m_fFlankAngleDeg);
			ctx.WriteValue("m_fFlankMaxEnemyDist", tmove.m_fFlankMaxEnemyDist);
			ctx.WriteValue("m_fFlankMinEnemyDist", tmove.m_fFlankMinEnemyDist);
			ctx.WriteValue("m_bEnableExposureScoring", tmove.m_bEnableExposureScoring);
			ctx.WriteValue("m_iExposureLayerMask", tmove.m_iExposureLayerMask);
			ctx.WriteValue("m_fExposureSidestep", tmove.m_fExposureSidestep);
			ctx.WriteValue("m_bEnableProceduralPath", tmove.m_bEnableProceduralPath);
			ctx.WriteValue("m_bPathDebugDraw", tmove.m_bPathDebugDraw);
			ctx.WriteValue("m_fPathReassessSec", tmove.m_fPathReassessSec);
			ctx.WriteValue("m_fPathLegLength", tmove.m_fPathLegLength);
			ctx.WriteValue("m_fPathConeHalfAngleDeg", tmove.m_fPathConeHalfAngleDeg);
			ctx.WriteValue("m_iPathConeSamples", tmove.m_iPathConeSamples);
			ctx.WriteValue("m_fPathArriveDist", tmove.m_fPathArriveDist);
			ctx.WriteValue("m_iPathMaxLegs", tmove.m_iPathMaxLegs);
			ctx.WriteValue("m_fPathThreatActivation", tmove.m_fPathThreatActivation);
			ctx.WriteValue("m_fPathConcealmentBonus", tmove.m_fPathConcealmentBonus);
			ctx.WriteValue("m_fPathStraightBias", tmove.m_fPathStraightBias);
			ctx.WriteValue("m_bEnableBaseOfFireSplit", tmove.m_bEnableBaseOfFireSplit);
			ctx.WriteValue("m_fPathSplitThreatActivation", tmove.m_fPathSplitThreatActivation);
			ctx.WriteValue("m_bPathHaltOnContact", tmove.m_bPathHaltOnContact);
			ctx.WriteValue("m_fPathContactHaltRange", tmove.m_fPathContactHaltRange);
			ctx.WriteValue("m_bPathLeaderInBase", tmove.m_bPathLeaderInBase);
			ctx.WriteValue("m_bPathBaseMaintainLos", tmove.m_bPathBaseMaintainLos);
			ctx.WriteValue("m_fPathBaseLosRadius", tmove.m_fPathBaseLosRadius);
			ctx.WriteValue("m_bEnableTravelingOverwatch", tmove.m_bEnableTravelingOverwatch);
			ctx.WriteValue("m_fPathTravelOverwatchThreat", tmove.m_fPathTravelOverwatchThreat);
			ctx.WriteValue("m_bEnableCoverSeek", tmove.m_bEnableCoverSeek);
			ctx.WriteValue("m_fCoverSeekCheckSec", tmove.m_fCoverSeekCheckSec);
			ctx.WriteValue("m_fCoverSeekRadius", tmove.m_fCoverSeekRadius);
			ctx.WriteValue("m_iCoverSeekSamples", tmove.m_iCoverSeekSamples);
			ctx.WriteValue("m_fCoverMaxClimb", tmove.m_fCoverMaxClimb);
			ctx.WriteValue("m_bEnableReactToContact", tmove.m_bEnableReactToContact);
			ctx.WriteValue("m_fReactCheckSec", tmove.m_fReactCheckSec);
			ctx.WriteValue("m_fSupportFireRateCoef", tmove.m_fSupportFireRateCoef);
			ctx.WriteValue("m_fAssaultFlankAngleDeg", tmove.m_fAssaultFlankAngleDeg);
			ctx.WriteValue("m_bEnableStandoff", tmove.m_bEnableStandoff);
			ctx.WriteValue("m_fStandoffRifle", tmove.m_fStandoffRifle);
			ctx.WriteValue("m_fStandoffMG", tmove.m_fStandoffMG);
			ctx.WriteValue("m_fStandoffSniper", tmove.m_fStandoffSniper);
			ctx.WriteValue("m_fStandoffLauncher", tmove.m_fStandoffLauncher);
			ctx.WriteValue("m_fStandoffPistol", tmove.m_fStandoffPistol);
			ctx.WriteValue("m_fStandoffBand", tmove.m_fStandoffBand);
			ctx.WriteValue("m_fBreakContactDistance", tmove.m_fBreakContactDistance);
			ctx.WriteValue("m_bEnableTacticalBrain", tmove.m_bEnableTacticalBrain);
			ctx.WriteValue("m_fBrainCheckSec", tmove.m_fBrainCheckSec);
			ctx.WriteValue("m_fBrainAssaultPowerRatio", tmove.m_fBrainAssaultPowerRatio);
			ctx.WriteValue("m_fBrainBreakPowerRatio", tmove.m_fBrainBreakPowerRatio);
			ctx.WriteValue("m_fBrainEnemyScanRadius", tmove.m_fBrainEnemyScanRadius);
			ctx.WriteValue("m_bEnableDcoFormations", tmove.m_bEnableDcoFormations);
			ctx.WriteValue("m_fFormationShapeCheckSec", tmove.m_fFormationShapeCheckSec);
			ctx.WriteValue("m_fFormationSpacing", tmove.m_fFormationSpacing);
			ctx.WriteValue("m_iFormationTravelShape", tmove.m_iFormationTravelShape);
			ctx.WriteValue("m_iFormationContactShape", tmove.m_iFormationContactShape);
		}

		// --- Adaptive formation ---
		ctx.WriteValue("m_bEnableAdaptiveFormation", cfg.m_bEnableAdaptiveFormation);
		ctx.WriteValue("m_fFormationCheckSec", cfg.m_fFormationCheckSec);
		ctx.WriteValue("m_sFormationTravel", cfg.m_sFormationTravel);
		ctx.WriteValue("m_sFormationContact", cfg.m_sFormationContact);

		// --- Base settings (optional section; keys absent on legacy files = defaults kept on load) ---
		DCO_BaseSettings base = DCO_BaseSettings.Get();
		if (base)
		{
			ctx.WriteValue("base_enable",     base.m_bEnableBaseSettings);
			ctx.WriteValue("base_ai_skill",   base.m_iAiSkill);
			ctx.WriteValue("base_perception", base.m_iPerception);
			ctx.WriteValue("base_reaction",   base.m_iReactionTime);
			ctx.WriteValue("base_fire_rate",  base.m_iFireRate);
			ctx.WriteValue("base_asset_pct",  base.m_iAssetUtilizationPct);
			ctx.WriteValue("base_grade",      base.m_eGlobalGrade);
			ctx.WriteValue("base_stance",     base.m_eGlobalStance);
			ctx.WriteValue("base_formation",  base.m_eDefaultSpawnFormation);
			ctx.WriteValue("base_debug",      base.m_bDebugBaseSettings);
		}

		if (ctx.SaveToFile(FILE_PATH))
			Print("[DCO] No server config found - wrote default DCO_Settings.json to " + FILE_PATH, LogLevel.NORMAL);
		else
			Print("[DCO] Failed to write default DCO_Settings.json to " + FILE_PATH, LogLevel.WARNING);
	}

	// Apply the server JSON onto the base-settings singleton (same file as morale;
	// missing keys keep defaults). First-boot deployment is handled by LoadInto().
	static void LoadBaseInto(DCO_BaseSettings cfg)
	{
		if (!cfg || !FileIO.FileExists(FILE_PATH))
			return;
		JsonLoadContext ctx = new JsonLoadContext();
		if (!ctx.LoadFromFile(FILE_PATH))
			return;
		ctx.ReadValue("base_enable",     cfg.m_bEnableBaseSettings);
		ctx.ReadValue("base_ai_skill",   cfg.m_iAiSkill);
		ctx.ReadValue("base_perception", cfg.m_iPerception);
		ctx.ReadValue("base_reaction",   cfg.m_iReactionTime);
		ctx.ReadValue("base_fire_rate",  cfg.m_iFireRate);
		ctx.ReadValue("base_asset_pct",  cfg.m_iAssetUtilizationPct);
		// Enum fields: ReadValue into int temp, then cast. JsonLoadContext serialises enum values as
		// their underlying integer (ordinal), so a round-trip through int is safe.
		int gradeInt = cfg.m_eGlobalGrade;
		ctx.ReadValue("base_grade", gradeInt);
		cfg.m_eGlobalGrade = gradeInt;
		int stanceInt = cfg.m_eGlobalStance;
		ctx.ReadValue("base_stance", stanceInt);
		cfg.m_eGlobalStance = stanceInt;
		ctx.ReadValue("base_formation",  cfg.m_eDefaultSpawnFormation);
		ctx.ReadValue("base_debug",      cfg.m_bDebugBaseSettings);
	}
}
