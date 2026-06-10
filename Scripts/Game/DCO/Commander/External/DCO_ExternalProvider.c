// External AI commander - provider adapters (bring-your-own model). Lets the external
// commander talk DIRECTLY to the common LLM APIs, not only a custom proxy. Three request
// styles cover everything:
//   - RAW (0)       : POST DCO's battlefield state as-is; the endpoint returns the order
//                     JSON directly. For a custom proxy / the RECOM backend.
//   - OPENAI (1)    : OpenAI Chat Completions shape. Covers OpenAI, DeepSeek, OpenRouter,
//                     Groq, Together, local LM Studio / Ollama - anything OpenAI-compatible.
//                     OpenRouter in particular routes to Claude, GPT, DeepSeek, Gemini, etc.
//                     through this one path, so it's the simplest "all models" option.
//   - ANTHROPIC (2) : Anthropic Messages API (x-api-key + anthropic-version header).
//
// Requests are built with JsonApiStruct (auto-escapes the summary); responses are parsed
// with JsonApiStruct too (no fragile string scraping). The model is told to answer with the
// bare order JSON; whatever text it returns is then parsed by DCO_ExternalCommander into a
// DCO_ExternalOrdersDto. Endpoint + path + key + model all come from the server JSON.
class DCO_ExternalProvider
{
	// HTTP headers for the provider's auth scheme.
	static string BuildHeaders(int provider, string apiKey)
	{
		if (provider == 2)	// ANTHROPIC
			return "Content-Type: application/json\nx-api-key: " + apiKey + "\nanthropic-version: 2023-06-01";

		// OPENAI-compatible and RAW both use bearer auth (RAW only if a key was given).
		string h = "Content-Type: application/json";
		if (apiKey != string.Empty)
			h = h + "\nAuthorization: Bearer " + apiKey;
		return h;
	}

	// Build the request body for OPENAI / ANTHROPIC. (RAW posts the state DTO from the caller.)
	static string BuildBody(int provider, string model, string systemPrompt, string summary, int maxTokens)
	{
		int maxTok = maxTokens;
		if (maxTok < 64)
			maxTok = 1024;

		if (provider == 2)	// ANTHROPIC messages API
		{
			string m = model;
			if (m == string.Empty)
				m = "claude-opus-4-8";

			DCO_LlmMsgDto user = new DCO_LlmMsgDto();
			user.role = "user";
			user.content = summary;

			DCO_AntReqDto req = new DCO_AntReqDto();
			req.model = m;
			req.max_tokens = maxTok;
			req.system = systemPrompt;
			req.messages.Insert(user);
			req.Pack();
			return req.AsString();
		}

		// OPENAI-compatible chat completions
		string om = model;
		if (om == string.Empty)
			om = "gpt-4o";

		DCO_LlmMsgDto sys = new DCO_LlmMsgDto();
		sys.role = "system";
		sys.content = systemPrompt;
		DCO_LlmMsgDto usr = new DCO_LlmMsgDto();
		usr.role = "user";
		usr.content = summary;

		DCO_OAReqDto oreq = new DCO_OAReqDto();
		oreq.model = om;
		oreq.max_tokens = maxTok;
		oreq.messages.Insert(sys);
		oreq.messages.Insert(usr);
		oreq.Pack();
		return oreq.AsString();
	}

	// Pull the model's answer text out of a provider response. RAW returns the body
	// unchanged (it already IS the order JSON). Returns "" if the shape doesn't match.
	static string ExtractContent(int provider, string rawResponse)
	{
		if (rawResponse == string.Empty)
			return string.Empty;

		if (provider == 1)	// OPENAI: choices[0].message.content
		{
			DCO_OARespDto r = new DCO_OARespDto();
			r.ExpandFromRAW(rawResponse);
			if (r.choices && !r.choices.IsEmpty() && r.choices[0] && r.choices[0].message)
				return r.choices[0].message.content;
			return string.Empty;
		}

		if (provider == 2)	// ANTHROPIC: content[0].text
		{
			DCO_AntRespDto r = new DCO_AntRespDto();
			r.ExpandFromRAW(rawResponse);
			if (r.content && !r.content.IsEmpty() && r.content[0])
				return r.content[0].text;
			return string.Empty;
		}

		return rawResponse;	// RAW
	}

	// Default commander instruction when the server JSON doesn't override it.
	static string DefaultSystemPrompt(string factionKey)
	{
		return "You are the AI ground commander for faction '" + factionKey + "' in an Arma Reforger battle."
			+ " You are given a terse battlefield summary (own groups with position/contact/threat and current objectives)."
			+ " Reply with ONE main-effort order as a single minified JSON object and nothing else - no prose, no markdown, no code fences."
			+ " Schema: {\"objectiveType\":\"DEFEND|HOLD|ATTACK|PATROL|SUPPORT|RESERVE\",\"x\":<east metres>,\"z\":<north metres>,"
			+ "\"radius\":<metres>,\"priority\":<0-100>,\"faction\":\"" + factionKey + "\",\"reasoning\":\"<one short clause, no braces>\"}.";
	}
}

// Shared chat message {role, content} for both OpenAI and Anthropic request bodies.
class DCO_LlmMsgDto : JsonApiStruct
{
	string	role;
	string	content;
	void DCO_LlmMsgDto()
	{
		RegV("role");
		RegV("content");
	}
}

// OpenAI-compatible chat-completions request.
class DCO_OAReqDto : JsonApiStruct
{
	string						model;
	int							max_tokens;
	ref array<ref DCO_LlmMsgDto>	messages;
	void DCO_OAReqDto()
	{
		messages = {};
		RegV("model");
		RegV("max_tokens");
		RegV("messages");
	}
}

// Anthropic Messages request (system is a top-level field, not a message).
class DCO_AntReqDto : JsonApiStruct
{
	string						model;
	int							max_tokens;
	string						system;
	ref array<ref DCO_LlmMsgDto>	messages;
	void DCO_AntReqDto()
	{
		messages = {};
		RegV("model");
		RegV("max_tokens");
		RegV("system");
		RegV("messages");
	}
}

// --- response shapes (only the fields we read are registered) ---

class DCO_OAMsgDto : JsonApiStruct
{
	string	content;
	void DCO_OAMsgDto() { RegV("content"); }
}

class DCO_OAChoiceDto : JsonApiStruct
{
	ref DCO_OAMsgDto	message;
	void DCO_OAChoiceDto() { RegV("message"); }
}

class DCO_OARespDto : JsonApiStruct
{
	ref array<ref DCO_OAChoiceDto>	choices;
	void DCO_OARespDto()
	{
		choices = {};
		RegV("choices");
	}
}

class DCO_AntBlockDto : JsonApiStruct
{
	string	text;
	void DCO_AntBlockDto() { RegV("text"); }
}

class DCO_AntRespDto : JsonApiStruct
{
	ref array<ref DCO_AntBlockDto>	content;
	void DCO_AntRespDto()
	{
		content = {};
		RegV("content");
	}
}
