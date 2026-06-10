// Unified debug logger. One central, category-tagged logger for all DCO systems, so a
// server owner / designer can turn on verbose diagnostics and see what the AI is
// deciding (morale, surrender reasons, flee direction, recovery, flanking) instead of
// guessing.
//
//   Usage:  DCO_Debug.Log("SURRENDER", string.Format("morale=%1 lull=%2", morale, lullOk));
//   Output: [DCO:SURRENDER] morale=7 lull=1
//
// Gated by the single master switch DCO_MoraleSettings.m_bDebug (GM toggle "DCO Debug
// Logging" + JSON key "m_bDebug", default OFF). When off, Log() early-outs on one bool
// read, so it costs ~nothing live. The category tag makes the log filterable.
//
// Plain static helper (not a modded class), so it's order-independent and callable from
// any DCO file. Runs server-side, so logs come from the authoritative machine.
class DCO_Debug
{
	// True if DCO debug logging is on. Use to guard expensive string-building.
	static bool Enabled()
	{
		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		return cfg && cfg.m_bDebug;
	}

	// Log a category-tagged line if debug is on. `category` is a short uppercase tag.
	static void Log(string category, string message)
	{
		if (!Enabled())
			return;
		Print("[DCO:" + category + "] " + message, LogLevel.NORMAL);
	}

	// Log a line tagged with the group's leader ID, so lines are traceable per group.
	static void LogGroup(string category, IEntity groupLeader, string message)
	{
		if (!Enabled())
			return;
		string who = "?";
		if (groupLeader)
			who = groupLeader.GetID().ToString();
		Print("[DCO:" + category + "] (grp " + who + ") " + message, LogLevel.NORMAL);
	}
}
