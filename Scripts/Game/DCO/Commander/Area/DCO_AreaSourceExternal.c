// External map-intelligence tier: load a pre-baked area file. This is where the full
// RECOM-style intelligence enters DCO - villages, military sites, AND forests (which can't
// be scanned cheaply at runtime, so they're produced offline and baked here). Zero runtime
// cost: it's a flat JSON the server owner drops into the profile directory.
//
// File: "$profile:DCO_MapIntel.json" (one per server/map; see docs). Schema is DCO's own
// (retagged from RECOM's ClusterResponseDto) and deliberately flat parallel-arrays so it
// round-trips through JsonApiStruct's proven primitive-array path:
//   {
//     "mapName": "Everon",
//     "types":   ["VILLAGE","MILITARY","FOREST", ...],
//     "xs":      [1234.5, ...],   "zs":   [6789.0, ...],
//     "radii":   [150.0,  ...],   "weights": [40, ...]
//   }
// Missing/short arrays are tolerated (an entry is skipped if any field is absent). Returns
// false if the file is absent or yields no areas, so the registry falls back to the native
// scan.
class DCO_AreaSourceExternal
{
	static const string FILE_PATH = "$profile:DCO_MapIntel.json";

	static bool Load(out array<ref DCO_MapArea> outAreas)
	{
		outAreas = {};
		if (!FileIO.FileExists(FILE_PATH))
			return false;

		string raw = SCR_FileIOHelper.GetFileStringContent(FILE_PATH, false);
		if (raw == string.Empty)
			return false;

		DCO_MapIntelFileDto dto = new DCO_MapIntelFileDto();
		dto.ExpandFromRAW(raw);
		if (!dto.xs || !dto.zs)
			return false;

		int n = dto.xs.Count();
		if (dto.zs.Count() < n)
			n = dto.zs.Count();

		for (int i = 0; i < n; i++)
		{
			float radius = 120.0;
			if (dto.radii && i < dto.radii.Count())
				radius = dto.radii[i];

			int weight = 1;
			if (dto.weights && i < dto.weights.Count())
				weight = dto.weights[i];

			DCO_EMapAreaType type = DCO_EMapAreaType.VILLAGE;
			if (dto.types && i < dto.types.Count())
				type = DCO_AreaTypeFromString(dto.types[i]);

			outAreas.Insert(new DCO_MapArea(type, Vector(dto.xs[i], 0, dto.zs[i]), radius, weight));
		}

		return !outAreas.IsEmpty();
	}

	protected static DCO_EMapAreaType DCO_AreaTypeFromString(string s)
	{
		if (s == "MILITARY")	return DCO_EMapAreaType.MILITARY;
		if (s == "FOREST")		return DCO_EMapAreaType.FOREST;
		if (s == "KEYPOINT")	return DCO_EMapAreaType.KEYPOINT;
		return DCO_EMapAreaType.VILLAGE;
	}
}

// Parallel-array file payload. Primitive arrays only, so JsonApiStruct's RegV auto-processing
// (the same path ReforgerCommander's DTOs use) parses it without a nested-struct dependency.
class DCO_MapIntelFileDto : JsonApiStruct
{
	string				mapName;
	ref array<string>	types;
	ref array<float>	xs;
	ref array<float>	zs;
	ref array<float>	radii;
	ref array<int>		weights;

	void DCO_MapIntelFileDto()
	{
		RegV("mapName");
		RegV("types");
		RegV("xs");
		RegV("zs");
		RegV("radii");
		RegV("weights");
	}
}
