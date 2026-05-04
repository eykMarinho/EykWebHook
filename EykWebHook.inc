#if defined _INC_EykWebHook
	#endinput
#endif
#define _INC_EykWebHook

#pragma tabsize 4

#if !defined EYKWEBHOOK_VERSION
	#define EYKWEBHOOK_VERSION (0x010000)
#endif

#if !defined HTTP_POST
	#include <a_http>
#endif

#if !defined DISCORDWH_QUEUE_SIZE
	#define DISCORDWH_QUEUE_SIZE (8)
#endif

#if !defined DISCORDWH_MAX_URL
	#define DISCORDWH_MAX_URL (256)
#endif

#if !defined DISCORDWH_MAX_JSON
	#define DISCORDWH_MAX_JSON (1900)
#endif

#if !defined DISCORDWH_MAX_POST
	#define DISCORDWH_MAX_POST (2048)
#endif

#if !defined DISCORDWH_SEND_DELAY_MS
	#define DISCORDWH_SEND_DELAY_MS (450)
#endif

#if defined DISCORDWH_USE_PAYLOAD_JSON
	#if DISCORDWH_USE_PAYLOAD_JSON
		#define DISCORDWH_MODE (1)
	#else
		#define DISCORDWH_MODE (0)
	#endif
#endif

#if !defined DISCORDWH_MODE
	#define DISCORDWH_MODE (0)
#endif

#if !defined DISCORDWH_MAX_RETRIES
	#define DISCORDWH_MAX_RETRIES (3)
#endif

#if !defined DISCORDWH_MAX_HOOKS
	#define DISCORDWH_MAX_HOOKS (4)
#endif

#if !defined DISCORDWH_STRICT_EMBED_CONTENT
	#define DISCORDWH_STRICT_EMBED_CONTENT (0)
#endif

#if !defined DISCORDWH_EMBED_FALLBACK_CONTENT
	#define DISCORDWH_EMBED_FALLBACK_CONTENT "."
#endif

#define DISCORDWH_OK (0)
#define DISCORDWH_ERR_QUEUE_FULL (-1)
#define DISCORDWH_ERR_URL_TOO_LONG (-2)
#define DISCORDWH_ERR_BODY_TOO_LONG (-3)
#define DISCORDWH_ERR_EMPTY_CONTENT (-4)
#define DISCORDWH_ERR_INVALID_HOOK (-5)
#define DISCORDWH_ERR_NOT_SET (-6)

static DISCORDWH__QueueUrl[DISCORDWH_QUEUE_SIZE][DISCORDWH_MAX_URL];
static DISCORDWH__QueuePost[DISCORDWH_QUEUE_SIZE][DISCORDWH_MAX_POST];
static DISCORDWH__RetryCount[DISCORDWH_QUEUE_SIZE];

static bool:DISCORDWH__InFlight;
static DISCORDWH__NextSendTick;
static DISCORDWH__Head;
static DISCORDWH__Tail;
static DISCORDWH__Count;
static DISCORDWH__TimerId;

static DISCORDWH__HookUrl[DISCORDWH_MAX_HOOKS][DISCORDWH_MAX_URL];
static bool:DISCORDWH__HookSet[DISCORDWH_MAX_HOOKS];
static bool:DISCORDWH__Initialized;

stock DiscordWH_SetupWebhooks()
{
	if (DISCORDWH__TimerId)
	{
		KillTimer(DISCORDWH__TimerId);
		DISCORDWH__TimerId = 0;
	}

	DISCORDWH__InFlight = false;
	DISCORDWH__NextSendTick = 0;
	DISCORDWH__Head = 0;
	DISCORDWH__Tail = 0;
	DISCORDWH__Count = 0;

	for (new i; i < DISCORDWH_QUEUE_SIZE; ++i)
	{
		DISCORDWH__QueueUrl[i][0] = '\0';
		DISCORDWH__QueuePost[i][0] = '\0';
		DISCORDWH__RetryCount[i] = 0;
	}

	for (new i; i < DISCORDWH_MAX_HOOKS; ++i)
	{
		DISCORDWH__HookSet[i] = false;
		DISCORDWH__HookUrl[i][0] = '\0';
	}

	DISCORDWH__Initialized = true;
	return 1;
}

static stock DISCORDWH__EnsureInit()
{
	if (DISCORDWH__Initialized) return 0;
	return DiscordWH_SetupWebhooks();
}

static stock bool:DISCORDWH__AppendChar(dest[], destSize, &pos, ch)
{
	if (pos + 1 >= destSize) return false;
	dest[pos++] = ch;
	dest[pos] = '\0';
	return true;
}

static stock bool:DISCORDWH__Append(dest[], destSize, &pos, const src[])
{
	for (new i; src[i] != '\0'; ++i)
	{
		if (!DISCORDWH__AppendChar(dest, destSize, pos, src[i])) return false;
	}
	return true;
}

static stock bool:DISCORDWH__StrCopy(dest[], destSize, const src[])
{
	new i;
	for (; i + 1 < destSize && src[i] != '\0'; ++i)
	{
		dest[i] = src[i];
	}
	dest[i] = '\0';
	return src[i] == '\0';
}

static stock DISCORDWH__IsUnreserved(ch)
{
	return (ch >= 'a' && ch <= 'z')
		|| (ch >= 'A' && ch <= 'Z')
		|| (ch >= '0' && ch <= '9')
		|| ch == '-' || ch == '_' || ch == '.' || ch == '~';
}

static stock DISCORDWH__HexNibble(n)
{
	return (n < 10) ? ('0' + n) : ('A' + (n - 10));
}

static stock bool:DISCORDWH__UrlEncode(dest[], destSize, const src[])
{
	new di;
	new si;
	for (; src[si] != '\0' && di + 1 < destSize; ++si)
	{
		new ch = src[si] & 0xFF;
		if (DISCORDWH__IsUnreserved(ch))
		{
			dest[di++] = ch;
		}
		else
		{
			if (di + 3 >= destSize)
			{
				break;
			}
			dest[di++] = '%';
			dest[di++] = DISCORDWH__HexNibble((ch >> 4) & 0xF);
			dest[di++] = DISCORDWH__HexNibble(ch & 0xF);
		}
	}
	dest[di] = '\0';
	return src[si] == '\0';
}

static stock bool:DISCORDWH__AppendUrlEncoded(post[], postSize, &pos, const src[])
{
	new tmp[512];
	if (!DISCORDWH__UrlEncode(tmp, sizeof tmp, src)) return false;
	return DISCORDWH__Append(post, postSize, pos, tmp);
}

static stock bool:DISCORDWH__BuildFormMessage(post[], postSize, const content[], const username[], const avatarUrl[], bool:tts)
{
	post[0] = '\0';
	new pos;

	if (content[0] == '\0') return false;

	if (!DISCORDWH__Append(post, postSize, pos, "content=")) return false;
	if (!DISCORDWH__AppendUrlEncoded(post, postSize, pos, content)) return false;

	if (username[0] != '\0')
	{
		if (!DISCORDWH__Append(post, postSize, pos, "&username=")) return false;
		if (!DISCORDWH__AppendUrlEncoded(post, postSize, pos, username)) return false;
	}

	if (avatarUrl[0] != '\0')
	{
		if (!DISCORDWH__Append(post, postSize, pos, "&avatar_url=")) return false;
		if (!DISCORDWH__AppendUrlEncoded(post, postSize, pos, avatarUrl)) return false;
	}

	if (tts)
	{
		if (!DISCORDWH__Append(post, postSize, pos, "&tts=true")) return false;
	}

	return true;
}

static stock bool:DISCORDWH__JsonEscapeAppend(dest[], destSize, &pos, const src[])
{
	for (new si; src[si] != '\0'; ++si)
	{
		new ch = src[si] & 0xFF;
		if (ch == '\\' || ch == '"')
		{
			if (!DISCORDWH__AppendChar(dest, destSize, pos, '\\')) return false;
			if (!DISCORDWH__AppendChar(dest, destSize, pos, ch)) return false;
		}
		else if (ch == '\n')
		{
			if (!DISCORDWH__AppendChar(dest, destSize, pos, '\\')) return false;
			if (!DISCORDWH__AppendChar(dest, destSize, pos, 'n')) return false;
		}
		else if (ch == '\r')
		{
			if (!DISCORDWH__AppendChar(dest, destSize, pos, '\\')) return false;
			if (!DISCORDWH__AppendChar(dest, destSize, pos, 'r')) return false;
		}
		else if (ch == '\t')
		{
			if (!DISCORDWH__AppendChar(dest, destSize, pos, '\\')) return false;
			if (!DISCORDWH__AppendChar(dest, destSize, pos, 't')) return false;
		}
		else if (ch < 0x20)
		{
			if (!DISCORDWH__AppendChar(dest, destSize, pos, '\\')) return false;
			if (!DISCORDWH__AppendChar(dest, destSize, pos, 'u')) return false;
			if (!DISCORDWH__AppendChar(dest, destSize, pos, '0')) return false;
			if (!DISCORDWH__AppendChar(dest, destSize, pos, '0')) return false;
			if (!DISCORDWH__AppendChar(dest, destSize, pos, DISCORDWH__HexNibble((ch >> 4) & 0xF))) return false;
			if (!DISCORDWH__AppendChar(dest, destSize, pos, DISCORDWH__HexNibble(ch & 0xF))) return false;
		}
		else
		{
			if (!DISCORDWH__AppendChar(dest, destSize, pos, ch)) return false;
		}
	}
	return true;
}

static stock DISCORDWH__NormalizeUrl(dest[], destSize, const url[])
{
	new skip;
	if (((url[0] | 0x20) == 'h')
		&& ((url[1] | 0x20) == 't')
		&& ((url[2] | 0x20) == 't')
		&& ((url[3] | 0x20) == 'p'))
	{
		if ((url[4] | 0x20) == 's')
		{
			if (url[5] == ':' && url[6] == '/' && url[7] == '/') skip = 8;
		}
		else
		{
			if (url[4] == ':' && url[5] == '/' && url[6] == '/') skip = 7;
		}
	}

	if (skip) DISCORDWH__StrCopy(dest, destSize, url[skip]);
	else DISCORDWH__StrCopy(dest, destSize, url);
	return 1;
}

static stock DISCORDWH__ParseRetryAfterMs(const data[])
{
	new pos = strfind(data, "\"retry_after\":", true);
	if (pos == -1) return -1;

	pos += 14;
	while (data[pos] == ' ' || data[pos] == '\t') ++pos;

	new tmp[32];
	new ti;
	while (ti + 1 < sizeof tmp)
	{
		new ch = data[pos++];
		if ((ch >= '0' && ch <= '9') || ch == '.' || ch == '-')
		{
			tmp[ti++] = ch;
		}
		else
		{
			break;
		}
	}
	tmp[ti] = '\0';
	if (ti == 0) return -1;

	new Float:seconds = floatstr(tmp);
	if (seconds <= 0.0) return -1;
	return floatround(seconds * 1000.0) + 50;
}

static stock DISCORDWH__EnsureTimer()
{
	if (DISCORDWH__TimerId) return 0;
	if (DISCORDWH__InFlight) return 0;
	if (DISCORDWH__Count <= 0) return 0;

	new now = GetTickCount();
	new waitMs = DISCORDWH__NextSendTick - now;
	if (waitMs < 1) waitMs = 1;

	DISCORDWH__TimerId = SetTimer("DISCORDWH__Timer", waitMs, false);
	return 1;
}

static stock DISCORDWH__Pump()
{
	if (DISCORDWH__InFlight) return 0;

	new now = GetTickCount();
	if (now < DISCORDWH__NextSendTick) return 0;

	if (DISCORDWH__Count <= 0) return 0;

	new id = DISCORDWH__Head;

	new url[DISCORDWH_MAX_URL];
	DISCORDWH__NormalizeUrl(url, sizeof url, DISCORDWH__QueueUrl[id]);

	DISCORDWH__InFlight = true;

	HTTP(id, HTTP_POST, url, DISCORDWH__QueuePost[id], "DISCORDWH__OnHttp");
	return 1;
}

stock DiscordWH_SendJson(const webhookUrl[], const jsonBody[])
{
	DISCORDWH__EnsureInit();
	if (DISCORDWH__Count >= DISCORDWH_QUEUE_SIZE) return DISCORDWH_ERR_QUEUE_FULL;

	new id = DISCORDWH__Tail;

	DISCORDWH__QueueUrl[id][0] = '\0';
	DISCORDWH__QueuePost[id][0] = '\0';
	DISCORDWH__RetryCount[id] = 0;

	if (!DISCORDWH__StrCopy(DISCORDWH__QueueUrl[id], DISCORDWH_MAX_URL, webhookUrl)) return DISCORDWH_ERR_URL_TOO_LONG;

	#if DISCORDWH_MODE == 1
		new prefixLen = format(DISCORDWH__QueuePost[id], DISCORDWH_MAX_POST, "payload_json=");
		if (!DISCORDWH__UrlEncode(DISCORDWH__QueuePost[id][prefixLen], DISCORDWH_MAX_POST - prefixLen, jsonBody)) return DISCORDWH_ERR_BODY_TOO_LONG;
	#else
		if (!DISCORDWH__StrCopy(DISCORDWH__QueuePost[id], DISCORDWH_MAX_POST, jsonBody)) return DISCORDWH_ERR_BODY_TOO_LONG;
	#endif

	DISCORDWH__Tail = (DISCORDWH__Tail + 1) % DISCORDWH_QUEUE_SIZE;
	++DISCORDWH__Count;

	DISCORDWH__Pump();
	DISCORDWH__EnsureTimer();
	return id;
}

stock DiscordWH_BuildMessage(dest[], destSize, const content[], const username[] = "", const avatarUrl[] = "", bool:tts = false)
{
	dest[0] = '\0';
	new pos;

	if (!DISCORDWH__Append(dest, destSize, pos, "{\"content\":\"")) return 0;
	if (!DISCORDWH__JsonEscapeAppend(dest, destSize, pos, content)) return 0;
	if (!DISCORDWH__Append(dest, destSize, pos, "\"")) return 0;

	if (username[0] != '\0')
	{
		if (!DISCORDWH__Append(dest, destSize, pos, ",\"username\":\"")) return 0;
		if (!DISCORDWH__JsonEscapeAppend(dest, destSize, pos, username)) return 0;
		if (!DISCORDWH__Append(dest, destSize, pos, "\"")) return 0;
	}

	if (avatarUrl[0] != '\0')
	{
		if (!DISCORDWH__Append(dest, destSize, pos, ",\"avatar_url\":\"")) return 0;
		if (!DISCORDWH__JsonEscapeAppend(dest, destSize, pos, avatarUrl)) return 0;
		if (!DISCORDWH__Append(dest, destSize, pos, "\"")) return 0;
	}

	if (tts)
	{
		if (!DISCORDWH__Append(dest, destSize, pos, ",\"tts\":true")) return 0;
	}

	if (!DISCORDWH__Append(dest, destSize, pos, "}")) return 0;
	return pos;
}

stock DiscordWH_BuildEmbed(dest[], destSize, const content[], const title[], const description[], color, const username[] = "", const avatarUrl[] = "")
{
	dest[0] = '\0';
	new pos;

	if (!DISCORDWH__Append(dest, destSize, pos, "{\"content\":\"")) return 0;
	if (!DISCORDWH__JsonEscapeAppend(dest, destSize, pos, content)) return 0;
	if (!DISCORDWH__Append(dest, destSize, pos, "\"")) return 0;

	if (username[0] != '\0')
	{
		if (!DISCORDWH__Append(dest, destSize, pos, ",\"username\":\"")) return 0;
		if (!DISCORDWH__JsonEscapeAppend(dest, destSize, pos, username)) return 0;
		if (!DISCORDWH__Append(dest, destSize, pos, "\"")) return 0;
	}

	if (avatarUrl[0] != '\0')
	{
		if (!DISCORDWH__Append(dest, destSize, pos, ",\"avatar_url\":\"")) return 0;
		if (!DISCORDWH__JsonEscapeAppend(dest, destSize, pos, avatarUrl)) return 0;
		if (!DISCORDWH__Append(dest, destSize, pos, "\"")) return 0;
	}

	if (!DISCORDWH__Append(dest, destSize, pos, ",\"embeds\":[{\"title\":\"")) return 0;
	if (!DISCORDWH__JsonEscapeAppend(dest, destSize, pos, title)) return 0;
	if (!DISCORDWH__Append(dest, destSize, pos, "\",\"description\":\"")) return 0;
	if (!DISCORDWH__JsonEscapeAppend(dest, destSize, pos, description)) return 0;

	new tmp[32];
	format(tmp, sizeof tmp, "\",\"color\":%d}]}", color);
	if (!DISCORDWH__Append(dest, destSize, pos, tmp)) return 0;
	return pos;
}

stock DiscordWH_SendMessage(const webhookUrl[], const content[], const username[] = "", const avatarUrl[] = "", bool:tts = false)
{
	#if DISCORDWH_MODE == 0
		new post[DISCORDWH_MAX_POST];
		if (!DISCORDWH__BuildFormMessage(post, sizeof post, content, username, avatarUrl, tts)) return -1;
		return DiscordWH_SendJson(webhookUrl, post);
	#else
		new json[DISCORDWH_MAX_JSON];
		if (!DiscordWH_BuildMessage(json, sizeof json, content, username, avatarUrl, tts)) return -1;
		return DiscordWH_SendJson(webhookUrl, json);
	#endif
}

stock DiscordWH_SendEmbed(const webhookUrl[], const content[], const title[], const description[], color = 0x2F3136, const username[] = "", const avatarUrl[] = "")
{
	new json[DISCORDWH_MAX_JSON];
	if (!DiscordWH_BuildEmbed(json, sizeof json, content, title, description, color, username, avatarUrl)) return -1;
	#if DISCORDWH_MODE == 0
		new post[DISCORDWH_MAX_POST];
		new tmpContent[2] = ".";
		if (!DISCORDWH__BuildFormMessage(post, sizeof post, (content[0] != '\0') ? content : tmpContent, username, avatarUrl, false)) return -1;

		new pos = strlen(post);
		if (!DISCORDWH__Append(post, sizeof post, pos, "&payload_json=")) return -1;

		if (!DISCORDWH__AppendUrlEncoded(post, sizeof post, pos, json)) return -1;
		return DiscordWH_SendJson(webhookUrl, post);
	#else
		return DiscordWH_SendJson(webhookUrl, json);
	#endif
}

stock DiscordWH_SetWebhook(hookid, const webhookUrl[])
{
	DISCORDWH__EnsureInit();
	if (hookid < 0 || hookid >= DISCORDWH_MAX_HOOKS) return 0;
	if (!DISCORDWH__StrCopy(DISCORDWH__HookUrl[hookid], DISCORDWH_MAX_URL, webhookUrl)) return 0;
	DISCORDWH__HookSet[hookid] = (webhookUrl[0] != '\0');
	return 1;
}

stock DiscordWH_IsWebhookSet(hookid)
{
	if (hookid < 0 || hookid >= DISCORDWH_MAX_HOOKS) return 0;
	return DISCORDWH__HookSet[hookid];
}

stock DiscordWH_GetWebhook(hookid, dest[], destSize)
{
	DISCORDWH__EnsureInit();
	if (hookid < 0 || hookid >= DISCORDWH_MAX_HOOKS) return 0;
	return DISCORDWH__StrCopy(dest, destSize, DISCORDWH__HookUrl[hookid]);
}

stock DiscordWH_ClearWebhooks()
{
	DISCORDWH__EnsureInit();
	for (new i; i < DISCORDWH_MAX_HOOKS; ++i)
	{
		DISCORDWH__HookSet[i] = false;
		DISCORDWH__HookUrl[i][0] = '\0';
	}
	return 1;
}

stock DiscordWH_CountWebhooks()
{
	DISCORDWH__EnsureInit();
	new count;
	for (new i; i < DISCORDWH_MAX_HOOKS; ++i)
	{
		if (DISCORDWH__HookSet[i]) ++count;
	}
	return count;
}

stock DiscordWH_AddWebhook(const webhookUrl[])
{
	DISCORDWH__EnsureInit();
	for (new i; i < DISCORDWH_MAX_HOOKS; ++i)
	{
		if (!DISCORDWH__HookSet[i])
		{
			if (!DiscordWH_SetWebhook(i, webhookUrl)) return -1;
			return i;
		}
	}
	return -1;
}

stock DiscordWH_RemoveWebhook(hookid)
{
	DISCORDWH__EnsureInit();
	if (hookid < 0 || hookid >= DISCORDWH_MAX_HOOKS) return 0;
	DISCORDWH__HookSet[hookid] = false;
	DISCORDWH__HookUrl[hookid][0] = '\0';
	return 1;
}

stock DiscordWH_SetWebhookToken(hookid, const webhookId[], const webhookToken[])
{
	DISCORDWH__EnsureInit();
	if (hookid < 0 || hookid >= DISCORDWH_MAX_HOOKS) return 0;
	new url[DISCORDWH_MAX_URL];
	format(url, sizeof url, "discord.com/api/webhooks/%s/%s", webhookId, webhookToken);
	return DiscordWH_SetWebhook(hookid, url);
}

static stock DISCORDWH__TrimInPlace(str[])
{
	new start;
	while (str[start] == ' ' || str[start] == '\t') ++start;
	if (start)
	{
		new i;
		for (; str[start + i] != '\0'; ++i) str[i] = str[start + i];
		str[i] = '\0';
	}
	new end = strlen(str);
	while (end > 0 && (str[end - 1] == ' ' || str[end - 1] == '\t')) --end;
	str[end] = '\0';
	return end;
}

static stock DISCORDWH__LooksLikeUrl(const str[])
{
	for (new i; str[i] != '\0'; ++i)
	{
		if (str[i] == '.') return 1;
		if (str[i] == '/' || str[i] == '\\') break;
	}
	return 0;
}

static stock DISCORDWH__SplitIdToken(const src[], idDest[], idSize, tokenDest[], tokenSize)
{
	new slash = -1;
	for (new i; src[i] != '\0'; ++i)
	{
		if (src[i] == '/')
		{
			slash = i;
			break;
		}
	}
	if (slash == -1) return 0;

	new idLen = slash;
	if (idLen <= 0) return 0;
	if (idLen >= idSize) idLen = idSize - 1;
	for (new j; j < idLen; ++j) idDest[j] = src[j];
	idDest[idLen] = '\0';

	new tokenLen;
	for (new i = slash + 1; src[i] != '\0' && tokenLen + 1 < tokenSize; ++i)
	{
		tokenDest[tokenLen++] = src[i];
	}
	tokenDest[tokenLen] = '\0';
	return tokenLen > 0;
}

stock DiscordWH_SetWebhookTokenList(const list[])
{
	DISCORDWH__EnsureInit();
	DiscordWH_ClearWebhooks();

	new hookid;
	new start;
	for (new i; ; ++i)
	{
		new ch = list[i];
		if (ch == '\0' || ch == '\n' || ch == '\r' || ch == ';' || ch == '|')
		{
			if (hookid >= DISCORDWH_MAX_HOOKS) break;
			if (i > start)
			{
				new tmp[DISCORDWH_MAX_URL];
				new len = i - start;
				if (len >= sizeof tmp) len = sizeof tmp - 1;
				for (new j; j < len; ++j) tmp[j] = list[start + j];
				tmp[len] = '\0';
				DISCORDWH__TrimInPlace(tmp);
				if (tmp[0] != '\0')
				{
					if (DISCORDWH__LooksLikeUrl(tmp))
					{
						DiscordWH_SetWebhook(hookid, tmp);
						++hookid;
					}
					else
					{
						new wid[64], wtoken[192];
						if (DISCORDWH__SplitIdToken(tmp, wid, sizeof wid, wtoken, sizeof wtoken))
						{
							DiscordWH_SetWebhookToken(hookid, wid, wtoken);
							++hookid;
						}
					}
				}
			}
			start = i + 1;
			if (ch == '\0') break;
		}
	}
	return hookid;
}

stock DiscordWH_SetWebhookList(const list[])
{
	DISCORDWH__EnsureInit();
	DiscordWH_ClearWebhooks();
	new hookid;
	new start;

	for (new i; ; ++i)
	{
		new ch = list[i];
		if (ch == '\0' || ch == '\n' || ch == '\r' || ch == ';' || ch == '|')
		{
			if (hookid >= DISCORDWH_MAX_HOOKS) break;
			if (i > start)
			{
				new tmp[DISCORDWH_MAX_URL];
				new len = i - start;
				if (len >= sizeof tmp) len = sizeof tmp - 1;
				for (new j; j < len; ++j) tmp[j] = list[start + j];
				tmp[len] = '\0';
				DISCORDWH__TrimInPlace(tmp);
				DiscordWH_SetWebhook(hookid, tmp);
				++hookid;
			}
			start = i + 1;
			if (ch == '\0') break;
		}
	}
	return hookid;
}

stock DiscordWH_SendMessageTo(hookid, const content[], const username[] = "", const avatarUrl[] = "", bool:tts = false)
{
	if (hookid < 0 || hookid >= DISCORDWH_MAX_HOOKS) return -1;
	if (!DISCORDWH__HookSet[hookid]) return -1;
	return DiscordWH_SendMessage(DISCORDWH__HookUrl[hookid], content, username, avatarUrl, tts);
}

stock DiscordWH_SendEmbedTo(hookid, const content[], const title[], const description[], color = 0x2F3136, const username[] = "", const avatarUrl[] = "")
{
	if (hookid < 0 || hookid >= DISCORDWH_MAX_HOOKS) return -1;
	if (!DISCORDWH__HookSet[hookid]) return -1;
	return DiscordWH_SendEmbed(DISCORDWH__HookUrl[hookid], content, title, description, color, username, avatarUrl);
}

stock DiscordWH_BroadcastMessage(const content[], const username[] = "", const avatarUrl[] = "", bool:tts = false)
{
	new sent;
	for (new i; i < DISCORDWH_MAX_HOOKS; ++i)
	{
		if (DISCORDWH__HookSet[i])
		{
			if (DiscordWH_SendMessage(DISCORDWH__HookUrl[i], content, username, avatarUrl, tts) != -1) ++sent;
		}
	}
	return sent;
}

stock DiscordWH_BroadcastEmbed(const content[], const title[], const description[], color = 0x2F3136, const username[] = "", const avatarUrl[] = "")
{
	new sent;
	for (new i; i < DISCORDWH_MAX_HOOKS; ++i)
	{
		if (DISCORDWH__HookSet[i])
		{
			if (DiscordWH_SendEmbed(DISCORDWH__HookUrl[i], content, title, description, color, username, avatarUrl) != -1) ++sent;
		}
	}
	return sent;
}

forward DISCORDWH__OnHttp(index, responseCode, data[]);
forward DISCORDWH__Timer();

public DISCORDWH__Timer()
{
	DISCORDWH__TimerId = 0;
	DISCORDWH__Pump();
	DISCORDWH__EnsureTimer();
	return 1;
}

public DISCORDWH__OnHttp(index, responseCode, data[])
{
	DISCORDWH__InFlight = false;

	if (DISCORDWH__Count <= 0 || index != DISCORDWH__Head)
	{
		CallLocalFunction("DiscordWH_OnResponse", "iis", index, responseCode, data);
		return 1;
	}

	new retryMs = -1;
	if (responseCode == 429)
	{
		retryMs = DISCORDWH__ParseRetryAfterMs(data);
	}

	new now = GetTickCount();
	if (responseCode == 429 && DISCORDWH__RetryCount[index] < DISCORDWH_MAX_RETRIES)
	{
		++DISCORDWH__RetryCount[index];
		if (retryMs <= 0) retryMs = DISCORDWH_SEND_DELAY_MS;
		DISCORDWH__NextSendTick = now + retryMs;
		DISCORDWH__Pump();
		DISCORDWH__EnsureTimer();
		return 1;
	}

	DISCORDWH__QueueUrl[index][0] = '\0';
	DISCORDWH__QueuePost[index][0] = '\0';
	DISCORDWH__RetryCount[index] = 0;
	DISCORDWH__Head = (DISCORDWH__Head + 1) % DISCORDWH_QUEUE_SIZE;
	--DISCORDWH__Count;
	DISCORDWH__NextSendTick = now + DISCORDWH_SEND_DELAY_MS;

	CallLocalFunction("DiscordWH_OnResponse", "iis", index, responseCode, data);
	DISCORDWH__Pump();
	DISCORDWH__EnsureTimer();
	return 1;
}
