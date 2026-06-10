// Base-settings mapping helpers. Pure functions converting the 0..100 UI levers
// onto the engine's native ranges, the baseline helpers both the applier and the
// morale layer call, and troop-grade application.
class DCO_BaseSettingsUtil
{
	// Per-group AT/asset utilization roll cache (0 = doesn't use assets, 1 = does),
	// keyed by the group utility component. A plain static (not a modded-class field)
	// so GroupUsesAssets is callable regardless of folder order - DCO_AssetUse sorts
	// before "Base", so a modded-class method here would be undefined to it.
	protected static ref map<SCR_AIGroupUtilityComponent, int> s_AssetUseRolls = new map<SCR_AIGroupUtilityComponent, int>();

	// Per-group AT/asset gate. Rolls once against m_iAssetUtilizationPct and caches
	// the result for the group's lifetime. Returns true (no gating) when base
	// settings is off. Used by DCO_UpdateAssetUse and DCO_UpdateHVT.
	static bool GroupUsesAssets(SCR_AIGroupUtilityComponent gu)
	{
		DCO_BaseSettings bs = DCO_BaseSettings.Get();
		if (!bs.m_bEnableBaseSettings || !gu)
			return true;	// base settings off -> percentage not applied (assets gated only by their own toggle)

		int cached;
		if (s_AssetUseRolls.Find(gu, cached))
			return cached == 1;

		int pct = bs.m_iAssetUtilizationPct;
		int roll;
		if (pct >= 100)
			roll = 1;
		else if (pct <= 0)
			roll = 0;
		else if (Math.RandomFloat01() * 100.0 <= pct)
			roll = 1;
		else
			roll = 0;

		if (s_AssetUseRolls.Count() > 4000)
			s_AssetUseRolls.Clear();	// bound memory over a long session (a re-roll is cosmetically invisible)
		s_AssetUseRolls.Set(gu, roll);
		return roll == 1;
	}

	// Formation ordinals map directly to SCR_EAIGroupFormation (resolved from source):
	//   0 = Wedge, 1 = Line, 2 = Column, 3 = StaggeredColumn.
	// The applier converts the stored ordinal to the SetFormation name via SCR_Enum.GetEnumName at runtime,
	// so no name table is kept here. The "Default Spawn Formation" slider in DCO_Attributes.conf is bounded
	// 0..3 with labels matching this order.

	static EAISkill SkillFromScore(int score)
	{
		if (score <= 12)  return EAISkill.NONE;
		if (score <= 37)  return EAISkill.ROOKIE;
		if (score <= 62)  return EAISkill.REGULAR;
		if (score <= 87)  return EAISkill.VETERAN;
		return EAISkill.EXPERT;
	}

	static float PerceptionFromScore(int score)   { return 0.5 + (score / 100.0); }
	static float FireRateFromScore(int score)     { return 0.7 + (score / 100.0) * 0.5; }
	static float ReactionDelayFromScore(int score){ return 2.0 - (score / 100.0) * 1.8; }

	static EAISkill BaselineSkill()         { return SkillFromScore(DCO_BaseSettings.Get().m_iAiSkill); }
	static float    BaselineFireRateCoef()  { return FireRateFromScore(DCO_BaseSettings.Get().m_iFireRate); }
	static float    BaselinePerception()    { return PerceptionFromScore(DCO_BaseSettings.Get().m_iPerception); }

	// Apply a troop grade: write every lever to the grade's quarter value and set the
	// hold-ground flag. One-shot; doesn't lock sliders. NONE leaves everything alone.
	static void ApplyGrade(DCO_BaseSettings cfg, DCO_EBaseGrade grade)
	{
		int score;
		switch (grade)
		{
			case DCO_EBaseGrade.CONSCRIPT: score = 25;  break;
			case DCO_EBaseGrade.TRAINED:   score = 50;  break;
			case DCO_EBaseGrade.ELITE:     score = 75;  break;
			case DCO_EBaseGrade.FANATIC:   score = 100; break;
			default: return;
		}
		cfg.m_iAiSkill            = score;
		cfg.m_iPerception         = score;
		cfg.m_iReactionTime       = score;
		cfg.m_iFireRate           = score;
		cfg.m_bFanaticHoldsGround = (grade == DCO_EBaseGrade.FANATIC);
	}
}
