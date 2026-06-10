// QRF range circle visual (GM preview). Draws a range cylinder (radius = the group's QRF range) at
// a world position using the engine's Shape visualizer, no asset dependencies. Shown while the GM
// previews/adjusts the QRF Range attribute on a selected group (DCO_QRFRangeEditorAttribute),
// mirroring how base-game GM area effects show a radius circle while being configured.
// Client-side: the held `ref Shape` keeps the visual alive until Hide() releases it.
class DCO_QRFRangeVisual
{
	protected static ref Shape s_Shape;

	protected static const int		DCO_QRF_CIRCLE_COLOR	= 0xFF3FA9F5;	// ARGB - bright blue
	protected static const float	DCO_QRF_CIRCLE_HEIGHT	= 3.0;

	// Show (or refresh) the range circle at the given world origin.
	static void Show(vector origin, float radius)
	{
		if (radius < 1.0)
		{
			Hide();
			return;
		}

		s_Shape = Shape.CreateCylinder(DCO_QRF_CIRCLE_COLOR, ShapeFlags.WIREFRAME | ShapeFlags.NOZBUFFER, origin, radius, DCO_QRF_CIRCLE_HEIGHT);
	}

	// Remove the range circle (releasing the held Shape reference frees the visualizer).
	static void Hide()
	{
		s_Shape = null;
	}
}
