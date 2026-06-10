// Surrender voiceline framework. When a unit genuinely surrenders, play an
// authored sound event on it ("hands up!", "don't shoot!", etc). This is the
// framework + trigger only; the audio is author-supplied so the 25th can drop in
// their own voicelines from the Workbench:
//
//   1. Add the audio file(s) and create/open a SoundProject (.acp).
//   2. Define a sound event (e.g. "SOUND_DCO_SURRENDER") that plays them, ideally
//      with a random pick across several lines.
//   3. Make sure the character's SoundComponent can resolve that event (it must
//      live in a bank the component loads), or attach the .acp via an emitter.
//   4. In the "25th DCO" GM tab enable "Surrender Voicelines" and set the event
//      name (m_sSurrenderSoundEvent), or bake it into a scenario.
//
// Triggered from DCO_GroupMorale.DCO_Surrender (staggered per member). Default OFF
// with an empty event, so nothing fires until real audio is wired in.
//
// MP note: surrender is decided server-side and SoundComponent.SoundEvent() plays
// on the machine it's called on - fine in the Workbench / SP / a listen host, but
// a dedicated server has no audio sink. To have every client hear the line the
// call needs broadcasting (an RPC from a replicated component, or a client-side
// reaction to replicated "surrendered" state). That broadcast layer is the next
// step; this file is the framework + local-play implementation it plugs into.
class DCO_SurrenderVoice
{
	// Play the configured surrender sound event on the character, if enabled and set.
	static void Play(IEntity character)
	{
		if (!character)
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableSurrenderVoice || cfg.m_sSurrenderSoundEvent == string.Empty)
			return;

		// SoundComponent (base of the character's SCR_CharacterSoundComponent) exposes SoundEvent().
		SoundComponent sc = SoundComponent.Cast(character.FindComponent(SoundComponent));
		if (!sc)
			return;

		sc.SoundEvent(cfg.m_sSurrenderSoundEvent);
	}
}
