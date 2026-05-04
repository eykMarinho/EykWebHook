# EykWebHook

Uma biblioteca leve para enviar mensagens e embeds via webhooks Discord para SA-MP/open.mp, sem dependências externas.

## Funcionalidades
- Envio de mensagens simples
- Envio de embeds
- Fila de requisições com retry automático em caso de rate limit
- Modos de payload JSON e form-data
- Gerenciamento de múltiplos webhooks

## Testes
<img width="393" height="427" alt="image" src="https://github.com/user-attachments/assets/f6e4e32b-3c29-4ae9-bbae-a02f1d77fdce" />

## Uso Básico

### Exemplo
```pawn

stock DiscordWH_StartupTest()
{
    if (DiscordWH_CountWebhooks() <= 0) return 0;

    new msg[192];
    for (new i = 1; i <= 10; ++i)
    {
        format(msg, sizeof msg, "Startup test #%d | tick=%d", i, GetTickCount());
        DiscordWH_BroadcastMessage(msg, "EykWebHook");
    }

    DiscordWH_BroadcastEmbed("", "Startup embed", "Embed funcionando", 0x57F287, "Avg.pwn");
    return 1;
}

public OnGameModeInit()
{
    DiscordWH_SetWebhook(0, "https://discord.com/api/webhooks/SEU_ID/SEU_TOKEN");
    DiscordWH_StartupTest();
    return 1;
}

forward DiscordWH_OnResponse(requestid, responseCode, data[]);
public DiscordWH_OnResponse(requestid, responseCode, data[])
{
    printf("[DiscordWH] req=%d code=%d body=%s", requestid, responseCode, data);
    return 1;
}
```

### Enviar Mensagem Simples
```pawn
public OnGameModeInit()
{
    DiscordWH_SetWebhook(0, "https://discord.com/api/webhooks/SEU_ID/SEU_TOKEN");
    DiscordWH_SendMessage(DISCORDWH_GetWebhook(0), "Olá, mundo!", "Meu Bot");
    return 1;
}
```

### Enviar Embed
```pawn
public OnPlayerConnect(playerid)
{
    new name[MAX_PLAYER_NAME];
    GetPlayerName(playerid, name, sizeof name);
    
    new json[DISCORDWH_MAX_JSON];
    DiscordWH_BuildEmbed(
        json, sizeof json,
        "",
        "Novo Jogador",
        name,
        0x57F287,
        "Servidor"
    );
    DiscordWH_SendJson(DISCORDWH_GetWebhook(0), json);
}
```

## Funcionamento
- A biblioteca usa a função `HTTP()` padrão do SA-MP
- Todas as requisições são enfileiradas para evitar rate limits
- Em caso de erro 429 (Too Many Requests), a biblioteca tenta novamente automaticamente
- Sem dependências de plugins ou includes adicionais
- Funciona em SA-MP e open.mp
