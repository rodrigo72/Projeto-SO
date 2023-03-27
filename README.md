# SO 

# Projeto de Sistemas Operativos

## Comunicação entre monitor/server e tracer/client

O server cria um *named pipe* chamado `server_fifo`. Os clients enviam requests de diferentes tipos para o server. Independentemente do tipo de request, é criado um "private" *named pipe* em que participa apenas o client e o server.

Request EXECUTE - o cliente envia client_info do tipo FIRST e LAST. FIRST envia o pid, time stamp de início e nome do programa que o client executa. LAST indica que o server pode parar de ouvir por mais client_info e tem o time stamp final. Os dados ativos e inativos são guardados num ficheiro binário de structs chamado storage.

Request STATUS - o server envia para o client mensagens com informação acerca dos processos ativos, pesquisando por esses processes do ficheiro binário de structs process_info. O client para de ouvir quando recebe uma pensagem com type STOP.

## Logs

(não é pedido no enunciado)
Foram criadas funções de escrita de logs para errors e para requests.

## Possíveis coisas a melhorar/ testar

- Criar ficheiros de storage separados para processos ativos e processos inativos (pode ser ineficiente procurar por processos ativos no meio de muitos inativos, ou então arranjar uma maneira de guardar indexes). UPDATE: na verdade, no enunciado parece dizer que se deve guardar uma informação de um processo por ficheiro.

## Tarefas

### Funcionalidades básicas (14 valores)
- [x] tracer execute -u
- [x] tracer status

### Funcionalidades avançadas (6 valores)
- [ ] tracer execute -p (execução encadeada de programas)
- [ ] Armazenamento de informação sobre programas terminados
- [ ] Consulta de programas terminados:
    - [ ] tracer stats-time
    - [ ] tracer stats-command
    - [ ] tracer stats-uniq

## Informações

- O trabalho deve ser entregue até às 23:59 de 13 de Maio.
- Deve ser entregue o código fonte e um relatório de até 10 páginas (A4, 11pt) no formato PDF (excluindo capas e anexos), justificando a solução, nomeadamente no que diz respeito à arquitectura de processos e da escolha e uso concreto dos mecanismos de comunicação.