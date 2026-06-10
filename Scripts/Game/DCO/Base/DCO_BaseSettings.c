// Global AI base-settings store (singleton). Sibling to DCO_MoraleSettings,
// holding the "engrained behaviour" levers surfaced under the "25th DCO BASE
// SETTINGS" GM tab. Lazily created, so the settings exist in every GM mode with
// no per-scenario setup. Every lever affects AI only.
//
// Each numeric lever is an int 0..100; DCO_BaseSettingsUtil maps it onto the
// engine's native range. Troop grades snap these fields to a quarter step
// (25/50/75/100) and stay editable afterwards.
enum DCO_EBaseGrade
{
	NONE,       // manual - leave sliders untouched
	CONSCRIPT,  // 25
	TRAINED,    // 50
	ELITE,      // 75
	FANATIC,    // 100
}

enum DCO_EBaseStance
{
	NONE,    // do not force
	STAND,
	CROUCH,
	PRONE,
}

class DCO_BaseSettings
{
	protected static ref DCO_BaseSettings s_Instance;

	bool m_bEnableBaseSettings = false;
	bool m_bDebugBaseSettings  = false;

	int  m_iAiSkill        = 50;
	int  m_iPerception     = 50;
	int  m_iReactionTime   = 50;
	int  m_iFireRate       = 50;

	// Percentage of groups that will engage in AT/asset behaviour (man statics via
	// DCO_AssetUse, prioritise armour as HVTs via DCO_HVTTargeting). Each group rolls
	// once and caches the result for its lifetime. 100 = every group (default), 0 =
	// none. Only applied while base settings is enabled.
	int  m_iAssetUtilizationPct = 100;

	DCO_EBaseGrade  m_eGlobalGrade           = DCO_EBaseGrade.NONE;
	DCO_EBaseStance m_eGlobalStance          = DCO_EBaseStance.NONE;
	int             m_eDefaultSpawnFormation = 0;

	bool  m_bFanaticHoldsGround = false;
	float m_fApplyIntervalSec   = 5.0;

	static DCO_BaseSettings Get()
	{
		if (!s_Instance)
		{
			s_Instance = new DCO_BaseSettings();
			DCO_JsonConfig.LoadBaseInto(s_Instance);   // server JSON = startup baseline
		}
		return s_Instance;
	}
}
