# SO 

# Projeto de Sistemas Operativos

## Comunicação entre monitor/server e tracer/client

O server cria um *named pipe* chamado `server_fifo`. Os clients enviam requests de diferentes tipos para o server. Independentemente do tipo de request, é criado um "private" *named pipe* em que participa apenas o client e o server.

Request EXECUTE - o cliente envia client_info do tipo FIRST e LAST. FIRST envia o pid, time stamp de início e nome do programa que o client executa. LAST indica que o server pode parar de ouvir por mais client_info e tem o time stamp final. Os dados ativos e inativos são guardados num ficheiro binário de structs chamado storage.

Request STATUS - o server envia para o client mensagens com informação acerca dos processos ativos, pesquisando por esses projetos do ficheiro binário de structs process_info. O client para de ouvir quando recebe uma pensagem com pid igual a -1.

## Logs

(não é pedido no enunciado)
Foram criadas funções de escrita de logs para errors e para requests.

## Possíveis coisas a melhorar/ testar

- Criar ficheiros de storage separados para processos ativos e processos inativos (pode ser ineficiente procurar por processos ativos no meio de muitos inativos, ou então arranjar uma maneira de guardar indexes)

## Tasks

- [x] tracer execute -u
- [x] tracer status