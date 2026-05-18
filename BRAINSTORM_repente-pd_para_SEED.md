# repente-pd — Brainstorming SEED-Ready

> *A plugdata fork with… [completar na sessão SEED]*

**Autor:** Carlos Eduardo Batista (Bidu) — UFPB  
**Data:** Abril 2026  
**Versão:** 1.0  
**Tipo SEED:** Application (Deep, 10 seções)  
**Destino:** Input para sessão `/seed`, depois `/seed launch` → PAUL  

---

## Como usar este documento

Este é o **dossiê de brainstorming** que alimenta a sessão SEED. Cada seção tem três partes:

- **Decidido:** decisões já fechadas — SEED não precisa rediscutir, só validar
- **Aberto:** questões genuínas que SEED deve coachar até fechar
- **Rationale:** porque a decisão é o que é (vira ADR no PAUL)

Ao final do SEED, tudo de "Aberto" vira "Decidido", o documento se condensa num PLANNING.md PAUL-ready, e `/seed launch` dispara a init do PAUL.

Materiais de referência **não repetidos** aqui (consultar nos arquivos de projeto):
- `00_Arquitetura_Consolidada.md` — arquitetura interna do plugdata
- `01_CommandInput_Estudo.md` — sistema de comandos atual
- `02_API_Patch_Estudo.md` — API `pd::Patch` (essencial para o executor)
- `03_Sistema_Mensagens_PD_Estudo.md` — passing de mensagens Pure Data
- `04_Integracao_pdlua_Estudo.md` — pd-lua para o REPL
- `repente-whitepaper v3.1` — estado do Repente (v0.3 produção, v0.4 em treino)
- `01-project-vision.md` — visão de longo prazo do Repente (Tríade Convergente)

---

## 1. Visão & Hipótese

### Decidido

**O que é:** `repente-pd` é um fork do plugdata que integra o LLM **Repente** (já em produção, v0.3+) como assistente de criação musical, mantendo o pd-script como mecanismo paralelo de manipulação programática de patches.

**Hipótese central:** programação musical assistida por LLM local-first é viável **hoje** quando o LLM já fala a linguagem nativa do host (Pure Data) e o host expõe APIs estáveis de manipulação de canvas (`pd::Patch`). A integração não inventa protocolo nem formato — o Repente já gera `.pd` válido com 100% de validade sintática vanilla, e já serve via API OpenAI-compatible em `localhost`.

**Princípio organizador (Tríade Convergente):** o usuário opera em três camadas simultâneas que o sistema deve atender — **C1 semântica** (linguagem natural / sinestésica), **C2 estrutural** (o `.pd` em si, formato canvas) e **C3 perceptual** (áudio resultante). MVP cobre C1↔C2; C3 fica para V2.

**Diferenciador:** ao contrário de chat-LLMs genéricos que falam sobre código musical, o repente-pd **executa** — o patch nasce no canvas, soa imediatamente, e pode ser editado tanto por arrasto/clique do usuário quanto por comandos pd-script.

### Aberto (para SEED)

- Como articular o "elevator pitch" do projeto em uma frase pública? "A plugdata fork with…" precisa fechar a frase de forma que comunique valor sem jargão.
- Posicionamento do produto: ferramenta de **estúdio** (DAW plugin), de **performance live coding**, ou ambas? (afeta UX prioritária)
- Tagline / nome final: `repente-pd` é nome de trabalho — manter ou repensar?

---

## 2. Personas & Casos de Uso

### Decidido

Quatro personas, ordenadas por prioridade de MVP:

**P1. Bidu (você) — Sound designer / pesquisador / professor.** RTX 5070, conhece Pure Data profundamente, quer iterar rápido em patches DSP. Caso de uso típico: descrever em texto uma textura granular, ver o patch nascer, refinar com pd-script ou manualmente.

**P2. Live coder.** Performance ao vivo. Precisa de **latência baixa de geração** e de **comandos compactos** que reaproveitem patches já existentes ("adicione um delay no oscilador 3").

**P3. Estudante de computação musical.** Hardware mais modesto (Tier 2 — Ollama). Caso de uso: aprender Pure Data com explicação sinestésica do que cada bloco faz (modo Análise do Repente).

**P4. Compositor experimental.** Quer iteração lenta e refletida — workflow do Battery F do whitepaper (pad → drums → pattern → combined).

**Não-personas** (deliberadamente fora do MVP): usuários comerciais de Max/MSP, programadores SuperCollider, plataformas embedded (PatchBox / Orange Pi vai num projeto irmão).

### Aberto

- Quem é a persona real do **launch** (V1.0 público)? Bidu+UFPB-circle ou abrir community-wide?
- Como acomodar P3 (Tier 2 / Ollama) sem que isso bloqueie o MVP focado em P1?

### Rationale

P1 é "tá lá no espelho" — designar o próprio dev como persona prioritária acelera o ciclo dogfood e evita design por suposição.

---

## 3. Modelo de Domínio

### Decidido

**Três entidades principais e suas relações:**

```
┌──────────────────────┐         ┌────────────────────────┐
│   USUÁRIO            │         │  REPENTE (externo)     │
│   prompt natural     │◄───────►│  llama-server :PORT    │
│   ou pd-script       │  HTTP   │  OpenAI-compat API     │
└──────────┬───────────┘         │  modelo GGUF Q4_K_M    │
           │                     └────────────────────────┘
           ▼
┌─────────────────────────────────────────────────────────┐
│  REPENTE-PD (esta aplicação)                            │
│                                                         │
│  ┌──────────────────────────────────────────────┐       │
│  │  PROMPT BAR (UI única, comandos /pds, /rep)  │       │
│  └──────────────────┬───────────────────────────┘       │
│                     │                                   │
│       ┌─────────────┴─────────────┐                     │
│       ▼                           ▼                     │
│  ┌─────────┐                 ┌─────────────┐            │
│  │ PD-SCRIPT│                │ REPENTE     │            │
│  │ engine  │                 │ BRIDGE      │            │
│  │ (Lua +  │                 │ (HTTP/SSE   │            │
│  │ direct  │                 │  client +   │            │
│  │ cmds)   │                 │  parser .pd)│            │
│  └────┬────┘                 └──────┬──────┘            │
│       │                             │                   │
│       └──────────────┬──────────────┘                   │
│                      ▼                                  │
│       ┌──────────────────────────────┐                  │
│       │  EXECUTOR (pd::Patch wrapper)│                  │
│       │  createObject, connect, ...  │                  │
│       │  thread-safe via libpd lock  │                  │
│       └──────────────┬───────────────┘                  │
│                      ▼                                  │
│              ┌───────────────┐                          │
│              │ PURE DATA     │                          │
│              │ canvas (libpd)│                          │
│              └───────────────┘                          │
└─────────────────────────────────────────────────────────┘
```

**Pontos centrais do modelo:**

1. **pd-script ≠ interlíngua LLM.** O Repente fala `.pd` nativo. O pd-script é uma DSL paralela do usuário. As duas pernas só convergem no Executor.

2. **Executor compartilhado.** Tanto pd-script quanto Repente Bridge usam a mesma camada `pd::Patch` (createObject, connect, etc.). Esta camada é **thread-safe** via os métodos do plugdata original.

3. **Canvas como fonte de verdade.** A IA não tem estado paralelo do patch — sempre lê do canvas atual ao serializar contexto.

4. **Sessão = histórico de prompts + diffs do canvas.** Persistência global em `~/.repente-pd/sessions/` (não embebida no `.pd`, para não poluir o formato do Pure Data).

### Aberto

- **Granularidade da serialização do canvas para contexto:** todo o canvas? apenas o subgrafo selecionado? diff incremental? formato `.pd` literal ou JSON intermediário? — isto é a "bidirecionalidade detalhada" que o SEED vai destrinchar.
- **Identidade de objetos entre prompts:** como o usuário se refere a "o oscilador que você criou agora" no próximo prompt? Por nome auto-gerado, por índice, por seleção visual?
- **Sessões aninhadas:** múltiplos chats simultâneos com Repente, um por canvas aberto? Ou sessão global com canvas-switching?

---

## 4. Arquitetura de Sistemas

### Decidido

**Stack do MVP:**

| Camada | Tecnologia | Origem |
|---|---|---|
| Host | C++17, JUCE, libpd | plugdata existente |
| GUI | JUCE Component (Prompt Bar) | extensão nova |
| HTTP client | `cpp-httplib` (header-only) ou `juce::URL` | nova dependência ou nativo |
| JSON | `nlohmann/json` (header-only) | nova dependência |
| Streaming | Server-Sent Events (SSE) sobre HTTP | padrão OpenAI-compat |
| LLM backend | Repente via `llama-server` (llama.cpp) | externo, já existe |
| Persistência sessões | filesystem JSON em `~/.repente-pd/` | nova |
| Scripting (pd-script) | pd-lua + parser custom | já planejado |

**Threading model (CRÍTICO — thread safety):**

- **Audio thread** (libpd `process()`): nunca bloqueada, jamais. Locks só no **GUI/message thread**.
- **GUI thread:** rendering, input, prompt bar, parsing de comandos pd-script.
- **HTTP thread (juce::Thread separado):** chamadas síncronas para Repente, streaming SSE.
- **Comunicação HTTP→GUI:** via `MessageManager::callAsync` ou `AsyncUpdater` — nunca tocar canvas direto da HTTP thread.
- **Executor (pd::Patch):** chamadas devem rodar na message thread; libpd já tem mutex interno (`sys_lock` / `sys_unlock`).

**Modos de operação no MVP:**

| Modo | Input | Output | Estado |
|---|---|---|---|
| **Geração** | C1 (texto natural) + contexto opcional do canvas | `.pd` parcial → executor | já validado no Repente v0.3 |
| **Análise** | C2 (canvas serializado) | C1 (descrição sinestésica) | já validado no Repente v0.3 |
| **Manipulação direta** | comando pd-script | mutação no canvas | extensão do CommandInput existente |

**Tiers de hardware:**

| Tier | Hardware | Backend | Latência |
|---|---|---|---|
| T1 — Studio (MVP) | RTX 5070+ / M4+ | llama-server local | ~111 tok/s |
| T2 — Casual (V1) | GPU média / iGPU | Ollama local | ~30-60 tok/s |
| T3 — Embedded (V2/PatchBox) | Orange Pi 5+ | NPU + 3B destilado | TBD |
| T4 — Fallback (stretch) | sem GPU local | API remota OpenAI-compat | rede |

**Princípio de portabilidade:** o repente-pd só fala com `http(s)://host:port/v1/chat/completions`. Trocar tier = trocar URL. Nenhum código de inferência roda dentro do plugdata.

### Aberto

- HTTP client: `cpp-httplib` (mais portável, header-only) vs `juce::URL` (zero dependência nova, integra naturalmente com message loop)? — discutir trade-offs no SEED.
- WebSocket vs Server-Sent Events para streaming? SSE é mais simples (HTTP one-way) e o llama-server suporta nativamente.
- **Configuração do servidor Repente:** wizard na primeira execução? `repente-pd.config.json`? Detecção automática de localhost:8080?
- Comportamento se o servidor Repente cair durante streaming: cancelar geração? Recuperar parcial?

### Rationale

Nada de inferência embarcada no plugdata. Toda a complexidade do Repente fica no servidor — repente-pd é cliente leve. Isso desacopla os dois projetos completamente: Repente v0.4, v0.5, 3B-distilled, todos servem a mesma API, e o repente-pd não muda.

---

## 5. Stack Técnico

### Decidido

```
runtime:
  - C++17
  - JUCE 7.x (já em uso pelo plugdata)
  - libpd (já em uso)
  - lua 5.4 (via pd-lua)

dependências novas:
  - cpp-httplib OU juce::URL  (decidir no SEED)
  - nlohmann/json
  - (nada mais — sem Boost, sem Qt, sem nada pesado)

build:
  - CMake 3.21+ (já em uso)
  - cross-platform: Windows (MSVC), macOS (Xcode), Linux (gcc/clang)

externo (não buildamos, só consumimos):
  - Repente: llama-server / Ollama
  - modelos GGUF Q4_K_M (~4.5GB, distribuído via HuggingFace ou similar)

testing:
  - JUCE UnitTest para parser/executor
  - testes de integração com servidor mock
  - benchmark contra patches do Battery B do whitepaper
```

### Aberto

- Bundling do Repente: distribuir junto (instalador único) ou separado? (afeta UX de onboarding)
- Versionamento: `repente-pd v1.0` requer Repente `v0.x+`? — definir contrato de compatibilidade.

### Rationale

Header-only para minimizar atrito de build. Cross-platform sem exceção: tudo que entra precisa funcionar nos três OSes do plugdata.

---

## 6. API / Contrato de Dados

### Decidido

**Endpoint usado:** `POST {repente_url}/v1/chat/completions` (padrão OpenAI).

**Request shape (modo Geração):**
```json
{
  "model": "repente-v0.3",
  "messages": [
    {"role": "system", "content": "<system prompt: pd vanilla only, ...>"},
    {"role": "user",   "content": "<prompt do usuário>"},
    {"role": "user",   "content": "Current canvas context:\n<.pd serializado>"}
  ],
  "stream": true,
  "max_tokens": 1536,
  "temperature": 0.7
}
```

**Response shape:** SSE stream de `data: {choices:[{delta:{content:"..."}}]}` até `data: [DONE]`.

**Parser de output (`.pd` → executor):**
- Detectar bloco `#N canvas` (início de patch).
- Linha-a-linha: `#X obj <x> <y> <typename> <args...>` → `pd::Patch::createObject`.
- Linha `#X connect <src> <out> <dst> <in>` → `pd::Patch::createConnection`.
- Robustez: aceitar truncamento (limite 512 tok no v0.3, 1536 no v0.4) — finalizar parcial sem crash.

**System prompt template (MVP):**
```
You are Repente, an assistant for Pure Data patch creation.
Always respond with valid Pure Data vanilla code in canvas format,
starting with #N canvas. Never use externals (ELSE, Cyclone, etc.).
When given a current canvas context, modify or extend it coherently.
```

**Persistência de sessão (formato local):**
```
~/.repente-pd/sessions/<session-id>/
├── meta.json         { canvas_path, created, last_active }
├── messages.jsonl    (jsonl: {role, content, timestamp, canvas_diff?})
└── snapshots/        (snapshots periódicos do canvas como .pd)
```

### Aberto

- **Formato de serialização do canvas para contexto:** `.pd` literal (humano e Repente entendem, Repente foi treinado nele) vs JSON intermediário (mais robusto, menos token-eficiente). Inclinação: **`.pd` literal**, mas SEED valida.
- Se o usuário tem N canvases abertos, qual vira contexto? Ativo? Selecionado? Todos?
- **Token budget management:** canvases grandes estouram contexto. Truncar como? (head, tail, smart-summarize?)
- Versionamento de sessions: migração entre versões do schema.

### Rationale

OpenAI-compatible API é padrão de fato em LLM-as-a-service local. Suporta `llama-server`, `Ollama`, `vLLM`, `LM Studio`, e até OpenAI/Anthropic remotos com pequeno adapter — máxima portabilidade.

---

## 7. Decisões de Design (ADRs)

ADRs já fechados, para virarem `.paul/decisions/` no PAUL:

**ADR-001: Repente como serviço HTTP local (OpenAI-compatible).**  
Inferência fora do processo plugdata; cliente HTTP fino; trocar tier = trocar URL. *Alternativas rejeitadas: embed via llama.cpp linkado (complexidade de build cross-platform, lock-in com versão); subprocess CLI (latência alta, sem streaming nativo).*

**ADR-002: pd-script é mecanismo paralelo, NÃO interlíngua LLM.**  
Repente fala `.pd` nativo (já validado v0.3). pd-script existe como DSL do usuário com valor próprio. Convergem só no Executor. *Alternativa rejeitada: pd-script como C2 da Tríade — adicionaria layer de tradução desnecessária e desperdiçaria capacidade nativa do Repente.*

**ADR-003: Pure Data only no MVP.**  
Sem SuperCollider, sem MAX. Repente v0.3 vanilla-only é o sweet spot. SC e MAX podem voltar quando o Repente reincorporá-los e quando houver demanda. *Alternativa rejeitada: Multi-Language Preview lado-a-lado — escopo MVP excessivo.*

**ADR-004: Persistência de sessões global em `~/.repente-pd/`.**  
Não embebida no `.pd` (preserva formato Pure Data limpo). Sessões transcendem patches individuais (usuário pode trabalhar em múltiplos canvases, retomar conversas).

**ADR-005: Barra de prompt única com comandos namespaced.**  
Uma só barra. `/pds <comando>` para pd-script direto. `/rep <prompt>` ou texto livre para Repente. *Alternativa rejeitada: dois componentes UI separados — duplicação visual, confusão.* **NB:** a granularidade exata dos comandos (`/help`, `/clear`, `/canvas`, etc.) **será refinada com SEED.**

**ADR-006: Modelo GGUF Q4_K_M via llama.cpp como referência.**  
Whitepaper demonstra: único quantize que mantém 100% de consistência cross-platform. MLX 4-bit é desconselhado (33% analysis success).

**ADR-007: Tier 1 (RTX 5070 / M4) como persona MVP.**  
Bidu é a primeira persona. Tier 2 (Ollama) em V1 sem mudança de protocolo.

**ADR-008: Streaming via Server-Sent Events.**  
Padrão `llama-server` e `Ollama`. UX: usuário vê patch sendo construído token-a-token. *Alternativa rejeitada: WebSocket — overkill para half-duplex.*

**ADR-009: Thread isolation estrita.**  
HTTP em juce::Thread; mutações de canvas em message thread via `MessageManager::callAsync`. Audio thread nunca tocada por código novo.

### Aberto (a virar ADRs no SEED)

- **ADR-XXX:** HTTP client específico (`cpp-httplib` vs `juce::URL`).
- **ADR-XXX:** Formato de serialização canvas-→-contexto.
- **ADR-XXX:** Estratégia de truncamento/sumário para canvases grandes.
- **ADR-XXX:** Escopo dos comandos da barra unificada.

---

## 8. Phase Breakdown — PAUL-Ready

Cada phase é **independentemente shippable**: ao final dela, o repente-pd compila, roda e tem valor demonstrável (mesmo que limitado). Isto é o que o PAUL exige (`/seed launch` → milestones).

### Phase 1 — Foundation (1-2 semanas)
**Objetivo:** Fork do plugdata buildável, prompt bar visível (sem lógica), infraestrutura de testes.  
**Build:**
- Fork plugdata, novo branch `repente-pd-main`
- Adicionar `Source/RepentePd/` com estrutura de pastas
- `PromptBar.h/cpp` (componente JUCE básico, só renderiza)
- CMake updates, compila Win/Mac/Linux
- Suite de testes JUCE UnitTest mínima  

**AC (BDD):**  
*Given* usuário builda do branch `repente-pd-main`  
*When* abre plugdata  
*Then* a Prompt Bar aparece, aceita texto, e os testes unitários passam nos três OSes.

**Outcome:** dev environment pronto, base estável.

---

### Phase 2 — pd-script (REPL básico) (2-3 semanas)
**Objetivo:** Comandos `/pds` funcionais sem nenhuma LLM envolvida — pd-script standalone.  
**Build:**
- `CommandParser` (tokenização)
- `DirectCommands`: create, connect, delete, list (ver `01_CommandInput_Estudo.md`)
- Integração com pd-lua para scripts Lua via `/pds lua <expr>`
- Histórico (↑/↓), autocomplete contextual
- Executor wrapper sobre `pd::Patch` (thread-safe, ver `02_API_Patch_Estudo.md`)

**AC:**  
*Given* canvas vazio  
*When* digito `/pds create osc~ 100 100`  
*Then* objeto `osc~` aparece na coordenada (100,100), executor manteve `pd::Patch` lock corretamente.

*Given* `/pds connect 0 0 1 0` com objetos válidos  
*When* enter  
*Then* conexão criada, audio thread não interrompida.

**Outcome:** repente-pd já entrega valor (pd-script funcional) mesmo sem Repente conectado.

---

### Phase 3 — Repente Bridge (MVP de geração) (3-4 semanas)
**Objetivo:** Texto livre na prompt bar → Repente gera `.pd` → aparece no canvas.  
**Build:**
- HTTP client (decidido em ADR-XXX)
- `RepenteClient`: chamada síncrona OpenAI-compatible
- SSE streaming + parser incremental de `.pd`
- `PdParser` robusto a truncamento
- Conexão Bridge → Executor
- Tela de configuração (URL do servidor, modelo)

**AC:**  
*Given* Repente rodando em `localhost:8080` com modelo v0.3  
*When* digito "create a granular cloud with stereo movement"  
*Then* dentro de ~5s o canvas tem objetos correspondentes a um patch granular válido, audio toca quando o usuário clicar play.

*Given* servidor offline  
*When* envio prompt  
*Then* erro claro na UI, plugdata não trava.

**Outcome:** geração end-to-end funcional. **Demo-ready.**

---

### Phase 4 — Bidirecionalidade + Análise (2-3 semanas)
**Objetivo:** Repente recebe canvas atual como contexto + modo Análise.  
**Build:**
- Serializador canvas → `.pd` (subgrafo selecionado por padrão; full canvas opcional)
- Inclusão automática no payload da request
- Comando `/rep analyze` ou similar (exato fica para SEED) — modo Análise
- Histórico de mensagens persistido em `~/.repente-pd/sessions/`
- Snapshots periódicos do canvas

**AC:**  
*Given* canvas com objetos selecionados  
*When* peço "add a lowpass filter after the oscillator"  
*Then* Repente recebe contexto do que existe e adiciona objeto coerentemente conectado.

*Given* canvas com patch existente  
*When* peço análise  
*Then* recebo descrição sinestésica do que aquele patch produz (estilo do Battery C do whitepaper).

**Outcome:** UX rica, conversa real com a IA sobre o patch.

---

### Phase 5 — Tier 2 (Ollama) + Polish (2 semanas)
**Objetivo:** Suporte oficial a Ollama, wizard de configuração, documentação.  
**Build:**
- Detecção automática `localhost:8080` (llama-server) e `localhost:11434` (Ollama)
- Wizard de primeira execução
- Documentação de usuário (instalação Repente + Ollama)
- Testes manuais em 3 OSes
- Release v1.0

**AC:**  
*Given* usuário instala Ollama com modelo Repente  
*When* abre repente-pd pela primeira vez  
*Then* wizard detecta o servidor e funciona sem configuração manual.

**Outcome:** público mais amplo, V1.0 lançável.

---

### Backlog (pós-V1)
- Modos Transformação e Facilitação
- PatchBox / Orange Pi (Tier 3)
- Loop multimodal (áudio renderizado → análise espectral → feedback)
- Cross-language (quando Repente reincorporar SC/MAX)
- Style Transfer Sonoro

---

## 9. Estratégia de Validação

### Decidido

**Validação técnica (cada phase):**
- Unit tests JUCE para parser, command tokenizer, executor wrapper
- Integration tests: servidor mock OpenAI-compatible
- Smoke tests cross-platform (Win/Mac/Linux build matrix)

**Validação musical (Phases 3+):**
- Reuso direto do **Battery B** do whitepaper Repente (5 testes Pd Generation): synth, sequencer, delay, drum, AM. Se o Repente passa nestes, o repente-pd deve passá-los end-to-end (texto → canvas → áudio).
- **Battery F** (composition): pad → drums → pattern → combined. Validação de workflow iterativo bidirecional.

**Validação de UX (Phase 5):**
- Sessão dogfood com 3-5 usuários P1/P2/P3.
- Métricas: tempo para primeiro patch que toca, taxa de comandos pd-script vs prompts Repente, abandono.

### Aberto

- Critério de aceitação **musical** (não-técnico): "soa bem"? "soa correto"? Como objetivar?
- Comparação com baseline humano (quanto tempo um usuário leva para criar o mesmo patch sem Repente)?

---

## 10. Riscos & Mitigações

### Decidido

| Risco | Severidade | Mitigação |
|---|---|---|
| **Truncamento de geração** (512 tok no v0.3) corta patches complexos | Alta | Aguardar Repente v0.4 (1536 tok); parser tolera truncamento; UI permite "continue" no streaming |
| **Latência de inferência** mata UX live coding | Média-alta | Streaming SSE (token-a-token visível); pre-warm do servidor; modo "fast" com `temperature=0` |
| **Patch gerado sintaticamente válido mas musicalmente ruim** | Média | Modo Análise pré-execução; usuário pode rejeitar antes de aplicar; iteração cheap |
| **Audio thread block** por bug em código novo | Crítica | Thread isolation estrita (ADR-009); reviews focadas em locks; `JUCE_ASSERT_MESSAGE_THREAD` |
| **Server Repente cai em streaming** | Média | Timeout + cancelamento limpo; mostrar parcial; opção de retry |
| **Versão do Repente incompatível** com repente-pd | Baixa-média | Contrato de versão (header X-Repente-Version?); detecção e aviso ao usuário |
| **Build cross-platform quebra** (dependência nova) | Média | Header-only por padrão; CI matrix nos 3 OSes desde Phase 1 |
| **Drift entre canvas real e contexto enviado** (concorrência) | Baixa | Snapshot atomic na message thread antes do send |

### Aberto

- Como lidar com **modelos não-Repente** apontados no mesmo endpoint (usuário rodando Mistral genérico achando que vai funcionar)? Detectar e avisar?
- Privacy: o canvas vai num servidor remoto se o usuário trocar URL para uma API cloud — alertar?

---

## Apêndice A — Questões prioritárias para a sessão SEED

Lista enxuta do que precisa fechar com SEED antes de `launch`:

1. **Elevator pitch / tagline.** Completar "A plugdata fork with…" — definir narrativa pública.
2. **UX da barra unificada.** Catálogo definitivo de comandos: `/pds`, `/rep`, `/help`, `/clear`, `/canvas`, `/sessions`, `/config`. Quais são essenciais? Qual o comportamento default (texto livre vai pra Repente?)
3. **Bidirecionalidade detalhada.** Granularidade da serialização do canvas (subgrafo? full? diff?), formato (`.pd` literal? JSON?), quando enviar (cada turno? sob demanda?).
4. **HTTP client.** `cpp-httplib` vs `juce::URL` — trade-offs.
5. **Scope dos comandos pd-script Phase 2.** Lista mínima viable: `create`, `connect`, `delete`, `list`, `lua`. O que mais?
6. **Configuração de servidor Repente.** Wizard? Auto-detect? Arquivo de config?
7. **Naming final.** `repente-pd` é o nome ou trocamos? Qual a relação visível com plugdata original (fork? extension? rebrand)?
8. **Estratégia de release.** Open source desde dia 1? Acompanha ciclo de release do Repente?
9. **Critério de aceitação musical** (Phase 3+): como objetivar "patch é bom"?

---

## Apêndice B — Referências

**Documentos do projeto** (já uploaded, não duplicados aqui):
- `00_Arquitetura_Consolidada.md` — fluxo GUI ↔ PD ↔ Audio, threading, integration points
- `01_CommandInput_Estudo.md` — sistema de comandos atual do plugdata (base para Phase 2)
- `02_API_Patch_Estudo.md` — `pd::Patch` API completa (essencial para Executor)
- `03_Sistema_Mensagens_PD_Estudo.md` — passing de mensagens
- `04_Integracao_pdlua_Estudo.md` — pd-lua para scripts Lua
- `05_Guia_Inicio_Rapido.md` — setup do ambiente
- `CHEATSHEET.md` — lookups rápidos
- `BMAD_PROJECT_PLAN_plugdata-script.md` — plano BMAD original (algumas partes ainda válidas)

**Externos:**
- `repente-whitepaper v3.1` (anexo) — estado do Repente, benchmark, decisões de quantização
- `01-project-vision.md` — visão de longo prazo do Repente (Tríade Convergente, V2 multimodal)
- plugdata source: https://github.com/plugdata-team/plugdata
- llama.cpp: https://github.com/ggerganov/llama.cpp
- SEED: https://github.com/ChristopherKahler/seed
- PAUL: https://github.com/ChristopherKahler/paul
- Caveman: https://github.com/JuliusBrussee/caveman

---

## Apêndice C — Templates PAUL para Phase 1

Pré-formatado para `/paul:plan` da Phase 1 (Foundation), serve de modelo para as outras:

```markdown
---
phase: 01-foundation
plan: 01
type: execute
autonomous: true
---

<objective>
Fork plugdata, set up repente-pd build infrastructure, render empty
PromptBar component, validate cross-platform build.
</objective>

<context>
@docs/00_Arquitetura_Consolidada.md
@docs/01_CommandInput_Estudo.md
@BRAINSTORM_repente-pd_para_SEED.md
</context>

<acceptance_criteria>
## AC-1: Fork builds on three OSes
Given a fresh clone of repente-pd-main
When CMake configure + build runs
Then plugdata.app/dll/so produces no errors on Windows, macOS, Linux

## AC-2: PromptBar component renders
Given plugdata is running
When user opens any canvas
Then PromptBar is visible at the bottom, accepts text input,
     does not break existing UI layout

## AC-3: Test infrastructure ready
Given the build directory is configured
When `ctest` runs
Then all baseline tests pass (smoke + lint)
</acceptance_criteria>

<tasks>
<task type="auto">
  <name>Fork plugdata + branch setup</name>
  <files>.git/, CMakeLists.txt</files>
  <action>git clone --recursive https://github.com/plugdata-team/plugdata
          git checkout -b repente-pd-main</action>
  <verify>git status clean, builds plugdata original on local machine</verify>
  <done>AC-1 partial</done>
</task>

<task type="auto">
  <name>Create Source/RepentePd structure</name>
  <files>Source/RepentePd/{Core,Bridge,Commands,UI,Integration}/.gitkeep</files>
  <action>Create folder skeleton matching ADR-005 architecture</action>
  <verify>Directory tree exists, CMakeLists includes new sources</verify>
  <done>structure ready for Phase 2 work</done>
</task>

<task type="auto">
  <name>PromptBar.h/cpp minimal</name>
  <files>Source/RepentePd/UI/PromptBar.{h,cpp}</files>
  <action>JUCE Component with TextEditor child, no logic, just rendering.
          Wire into PluginEditor::resized().</action>
  <verify>Open plugdata, see prompt bar at bottom, type text without crash</verify>
  <done>AC-2</done>
</task>

<task type="auto">
  <name>Cross-platform CI smoke</name>
  <files>.github/workflows/ci.yml</files>
  <action>GitHub Actions matrix: ubuntu-latest, macos-latest, windows-latest.
          Build only, no tests yet.</action>
  <verify>All three matrix cells green on push</verify>
  <done>AC-1 full</done>
</task>

<task type="auto">
  <name>Baseline JUCE UnitTest</name>
  <files>Tests/RepentePdTests.cpp</files>
  <action>One trivial test: PromptBar instantiable. Wire into ctest.</action>
  <verify>ctest runs, passes</verify>
  <done>AC-3</done>
</task>
</tasks>

<boundaries>
## DO NOT CHANGE
- Source/Pd/* (libpd integration — frozen until Phase 2)
- Source/Canvas.cpp (until Phase 2 needs it)
- existing CommandInput in Sidebar/ (paralelo, não tocamos no MVP)
- audio thread code paths (jamais)
</boundaries>
```

---

## Apêndice D — Quando ativar Caveman

Recomendação de uso do plugin Caveman (~75% redução de output tokens):

| Phase | Caveman? | Razão |
|---|---|---|
| 1 — Foundation | ❌ | Setup precisa explicação completa |
| 2 — pd-script | 🟡 parcial | Ativar em tasks repetitivas (parser, comandos similares) |
| 3 — Repente Bridge | ✅ recomendado | Implementação repetitiva (HTTP wiring, JSON serialization) |
| 4 — Bidirecional | ❌ | Decisões de design exigem prosa |
| 5 — Polish | ✅ | Documentação automática, refactors |
| Debugging session | ✅ sempre | Caveman shines em loops error→fix |

Trigger: dizer `/caveman` ou "talk like caveman" em sessões longas de Claude Code dentro de PAUL apply.

---

## Apêndice E — Ecosystem Map

```
                    ┌──────────────────────┐
                    │  repente-pd          │
                    │  (este projeto)      │
                    └──────┬──────┬────────┘
                           │      │
                  consome  │      │  fork de
                           ▼      ▼
                  ┌─────────┐  ┌──────────┐
                  │ Repente │  │ plugdata │
                  │ (Bidu)  │  │ (Timothy │
                  │         │  │  Schoen) │
                  └─────────┘  └──────────┘

   ┌────────────────── ferramentas de desenvolvimento ─────────────────┐
   │                                                                   │
   │  SEED → /seed → PLANNING.md → /seed launch → PAUL                 │
   │                                              │                    │
   │                                              ▼                    │
   │                                   PLAN → APPLY → UNIFY            │
   │                                              │                    │
   │                                              + Caveman opcional   │
   └───────────────────────────────────────────────────────────────────┘
```

---

## Notas finais

Este documento é um **dossiê**, não um spec. SEED vai refinar, condensar e produzir o `PLANNING.md` definitivo. Após `/seed launch`, o PAUL toma posse do projeto e roda os ciclos PLAN-APPLY-UNIFY phase a phase.

**Estado esperado depois do SEED:**
- `apps/repente-pd/PLANNING.md` (denso, PAUL-ready)
- `apps/repente-pd/.paul/` inicializado
- `apps/repente-pd/.paul/ROADMAP.md` com 5 phases
- `apps/repente-pd/.paul/PROJECT.md` com identidade/contexto
- README sintetizado pelo `/seed graduate`

A partir daí: `/paul:plan 01-foundation` → começa o trabalho real.

---

*Fim do documento.*
