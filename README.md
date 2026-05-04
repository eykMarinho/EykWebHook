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

## Configuração Opcional
Antes de incluir a biblioteca, você pode ajustar esses defines para suas necessidades:

```pawn
#define DISCORDWH_QUEUE_SIZE (32)   // Tamanho da fila de requisições
#define DISCORDWH_MAX_POST (4096)     // Tamanho máximo do payload
#define DISCORDWH_MAX_HOOKS (8)       // Número máximo de webhooks
#define DISCORDWH_MAX_URL (256)        // Tamanho máximo da URL do webhook
#include <EykWebHook>
```

## Funcionamento
- A biblioteca usa a função `HTTP()` padrão do SA-MP
- Todas as requisições são enfileiradas para evitar rate limits
- Em caso de erro 429 (Too Many Requests), a biblioteca tenta novamente automaticamente

## Otimizações
- Baixo consumo de memória (valores padrão otimizados)
- Sem dependências de plugins ou includes adicionais
- Funciona em SA-MP e open.mp
