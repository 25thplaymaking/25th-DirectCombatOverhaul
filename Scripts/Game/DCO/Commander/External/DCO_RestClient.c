// External AI commander - REST client. Thin wrapper over the engine RestApi for
// talking to an external provider (an OpenAI-compatible chat endpoint, or the RECOM
// backend). Adapted from ReforgerCommanderClient's RCM_RCMBackendClient but updated
// to the 1.7 callback API: RECOM overrode the obsolete RestCallback.OnSuccess/OnError;
// 1.7 wants SetOnSuccess/SetOnError with a callback function instead.
//
// Engine facts worth remembering:
//   - GetGame().GetRestApi().GetContext(baseUrl) returns a RestContext.
//   - RestContext.POST(cb, endpoint, data) / GET(cb, endpoint) are async;
//     SetHeaders("A: x\nB: y") is how we attach Authorization + Content-Type.
//   - In the callback, read cb.GetData() / cb.GetHttpCode() / cb.GetRestResult().
//   - The RestCallback must be held as a ref while the request is in flight, or the
//     engine deletes it after it runs. The owner of this client must keep it alive too.
//
// Security: the API key comes from the caller (server profile JSON), never hardcoded
// or committed. Server-only: only the server issues external requests.

// Implement OnResponse/OnFailure to consume a request's result. Held by DCO_RestClient.
class DCO_RestResponseHandler : Managed
{
	void OnResponse(string data) {}		// HTTP success: raw response body
	void OnFailure(int httpCode) {}		// HTTP/Rest error
}

class DCO_RestClient
{
	protected RestContext					m_Context;
	protected ref RestCallback				m_Callback;		// held while a request is in flight
	protected ref DCO_RestResponseHandler	m_Handler;

	void DCO_RestClient(string baseUrl)
	{
		RestApi api = GetGame().GetRestApi();
		if (api)
			m_Context = api.GetContext(baseUrl);
	}

	bool IsReady()
	{
		return m_Context != null;
	}

	// Async POST a JSON body to endpoint with a caller-built header block (auth scheme
	// varies by provider - see DCO_ExternalProvider). The handler gets the result. False if
	// the REST context isn't available.
	bool Post(string endpoint, string jsonBody, string headers, DCO_RestResponseHandler handler)
	{
		if (!m_Context)
			return false;
		m_Handler = handler;

		m_Context.SetHeaders(headers);

		m_Callback = new RestCallback();
		m_Callback.SetOnSuccess(DCO_OnSuccess);
		m_Callback.SetOnError(DCO_OnError);
		m_Context.POST(m_Callback, endpoint, jsonBody);
		return true;
	}

	protected void DCO_OnSuccess(RestCallback cb)
	{
		if (m_Handler && cb)
			m_Handler.OnResponse(cb.GetData());
		m_Callback = null;	// request done - release the held callback
	}

	protected void DCO_OnError(RestCallback cb)
	{
		if (m_Handler && cb)
			m_Handler.OnFailure(cb.GetHttpCode());
		m_Callback = null;
	}
}
