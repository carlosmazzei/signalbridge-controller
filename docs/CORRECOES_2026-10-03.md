# Implementação das correções da revisão — 2026-10-03

Branch: `codex/revisao-completa-2026-10-03`. Base da implementação: `4c307f8`.
O diagnóstico original permanece em [REVISAO_2026-10-03.md](REVISAO_2026-10-03.md).

## Correções implementadas

| Achado | Alteração | Evidência local |
|---|---|---|
| R01 | Validação de comprimento e ID antes do acesso ao mapa de displays | IDs zero com todos os subcomandos; ASan/UBSan |
| R02 | Uma CDC, duas interfaces, descriptor de 75 bytes e assertions de tamanho | Teste compila o descriptor real e percorre interfaces/endpoints |
| R03 | Firmware, análise e testes executados no mesmo job obrigatório | YAML validado; dependência de release explícita |
| R04 | Exit code bloqueante, configuração ARM de 32 bits, headers SDK/GCC resolvidos, sem compilação duplicada de `app_context.c` | Cppcheck completo e simplificado passaram; teste injeta erro no analisador e exige saída 2 |
| R05 | Primeira leitura ADC publicada mesmo sem mudança | Duas varreduras da tarefa real com ADC constante em 2048 |
| R06 | Snapshots de teclas/ADC com versão, tentativa pendente, confirmação pelo writer e reenvio por sessão | Falha/retry, atualização durante envio e ACK de sessão antiga |
| R07 | Decoder COBS verifica limites, blocos truncados e zeros internos | Casos negativos, capacidade de saída e sanitizadores; encoder também cobre bloco final de 254 bytes |
| R08 | Overflow descarta toda a cauda até o delimitador | Teste tenta completar um comando dentro da cauda descartada |
| R09 | PWM e consultas indexadas exigem exatamente um byte | Rejeição de payloads de zero e dois bytes nos três comandos |
| R10 | Dispatch usa quatro dígitos para TM1637 e oito para TM1639 | Teste de mapa misto executa os drivers reais e LEDs TM1639 |
| R11 | Brilho zero desliga; 1–8 controla intensidade | TM1637 real com ACK/NACK; estado só muda após sucesso |
| R12 | Erros do mux e drivers propagados por `output_init` | Injeção de falha de criação de driver |
| R13 | Contadores e watermarks com atomics C11 | Quatro threads somam 400 mil incrementos sem perdas |
| R14 | Geração de sessão invalida quadros, snapshots e fragmentos em reset/unmount/suspend/mudança de linhas; a transição de DTR/RTS limpa RX pendente | Transições de sessão, limpeza de RX e invalidação de ACKs; integração TinyUSB compilada para RP2040 |
| R15 | Writer mantém quadro parcial e bloqueia 10 ms sem progresso; registra stall após 1 s | Inspeção do laço e build ARM; ensaio de host parado em placa ainda necessário |
| R16 | Supervisor único verifica oito tarefas e deixa o watchdog expirar em falha parcial/fatal | Heartbeats, tarefa parada, espera intencional, wrap do relógio e publicação concorrente de timestamp |
| R17 | Release só publica artefatos do job que concluiu todas as verificações com sucesso | Inspeção do grafo; sem `always()` no caminho de publicação |

A confirmação de entrada significa aceitação integral do pacote na FIFO
TinyUSB. O protocolo não possui ACK da aplicação no computador. Eventos
relativos de encoder e respostas a comandos continuam best effort; eventos
de encoder são descartados quando desconectado e podem ser perdidos por
saturação. Estados absolutos convergem para o último valor conhecido após
retomada da leitura/reconexão.

## Débitos tratados

- Políticas de warnings, stack usage e stack protector também aplicadas à biblioteca de firmware.
- Buffer ativo, brilho e limpeza de displays só são confirmados depois da transferência; falhas mantêm o buffer desejado pendente.
- Inicialização/limpeza de filas limitada ao boot; reinicialização de saídas libera handles anteriores.
- CI cobre PRs, scripts, análise, workflow, toolchain e submódulos; constrói a imagem a partir do checkout e exige as versões de pacotes do manifesto.
- Cobertura usa atualização atômica dos contadores GCC nos testes concorrentes.
- Análise de placement usa o tamanho real de `ucHeap`, `__end__`/`__HeapLimit` e os limites de ambas as pilhas. Não presume volatile, alinhamento de quatro bytes para todos os objetos, ou problemas de cache em scratch SRAM.
- README, arquitetura, protocolo e instruções de teste sincronizados com a implementação.

## Validação

Executada no DevContainer já existente, sem instalar dependências novas:

| Verificação | Resultado |
|---|---|
| Host, `ctest --preset host-tests` | 15/15 entradas passaram, incluindo cobertura e testes dos scripts |
| AddressSanitizer + UndefinedBehaviorSanitizer | 14/14 entradas passaram |
| Build ARM Release | Passou; ELF/UF2 gerados |
| Build ARM Debug | Passou; ELF/UF2 gerados |
| Cppcheck + addon MISRA e script simplificado | Passaram sem erros bloqueantes ou abortos do analisador |
| Placement de `ucHeap` no ELF Release | 131.072 bytes, sem falso positivo de sobreposição consigo mesmo |
| Sintaxe de scripts Bash, YAML e `git diff --check` | Passaram |

Cobertura dos módulos compilados no host: **71,5% das linhas (962/1.346)**.
Esse denominador inclui drivers, entrada ADC e descriptor USB, mas exclui
`app_tasks.c`, `main.c` e `hooks.c`; não representa cobertura de todo o firmware.
O relatório HTML fica em `build-tests/coverage/index.html`.

Release: **61.264 bytes de Flash e 153.744 bytes de RAM estática**, incluindo o
heap FreeRTOS; piso do heap C de 32.768 bytes e 8.192 bytes reservados para
pilhas dos cores. A folga estática calculada é 75.632 bytes. Esses valores não
medem ocupação máxima das pilhas das tarefas em execução.

Os diagnósticos MISRA e alguns avisos de estilo legados permanecem visíveis,
mas consultivos em `.cppcheck_advisory`. Falhas internas do analisador,
configuração incompleta e outros diagnósticos continuam bloqueantes. Não há
alegação de conformidade MISRA. LeakSanitizer foi desabilitado para os
fixtures host que mantêm alocações de boot até encerrar o processo; ASan e
UBSan permanecem ativos.

## Validações e decisões ainda pendentes

- Placa física: enumeração USB nos sistemas usados pelo host, saturação/reconexão, ACKs elétricos, SMP sob carga, latência das entradas, uso máximo de pilhas e reset por falha parcial/fatal.
- Pipeline remoto: não foi disparado um workflow, publicado release ou reconstruída a imagem completa do DevContainer. A imagem base Debian ainda usa tag; disponibilidade futura das versões APT requer snapshot/digest para reprodução integral.
- Arquitetura: o despacho de comandos ainda aciona hardware sincronamente na tarefa de decode. Separar essa execução exige medir latência e definir prioridade/coalescência, além de testes de integração dos laços completos RX/TX.
- Protocolo: os três bits de ID mantêm displays 1–7; habilitar o oitavo exige decisão de compatibilidade. A contagem de dígitos é obtida do mapa de tipos; uma interface genérica de capacidades continua sendo uma evolução possível.

Nenhuma gravação de firmware, alteração do layout persistente, push ou
publicação foi realizada.
