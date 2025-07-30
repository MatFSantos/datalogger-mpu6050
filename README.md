# Título do Projeto  

Datalogger de movimento

## Objetivo Geral  

Desenvolver um dispositivo portátil capaz de capturar, armazenar e exibir dados de movimento (*aceleração* e *giroscópio*) utilizando um sensor **MPU6050** conectado ao microcontrolador **Raspberry Pi Pico W**.

O sistema deve registrar os dados em um cartão **microSD** em formato `.csv`, permitindo a análise posterior por meio de gráficos gerados com um script **Python**. A interação com o usuário é aprimorada através de recursos visuais e sonoros fornecidos pela plataforma BitDogLab.

## Descrição Funcional

O sistema embarcado inicia com a montagem segura do cartão **SD**, exibindo o status no **display OLED** e sinalizando via **LED RGB**. Um botão permite **montar** ou **desmontar** o cartão SD com segurança, evitando perda de dados. Outro botão **inicia** e **para** a **captura** de dados do sensor **MPU6050**.

O **LED RGB** possui um código de cores para visualização do usuário.

- **Amarelo**: montando/ desmontando SD;
- **Verde**: aguardando comando;
- **Azul (piscando)**: capturando dados e salvando;
- **Vermelho**: ao apresentar um erro.

### Durante inicialização:

- O **display OLED** avisa a inicialização e pede que seja aguardada a conclusão.
- Após início o **display OLED** mostra o  (STS: *UNMOUNTED*), a ação que está sendo executada  (ACT: N/A) e o número de amostras capturadas (COUNT: 0).
- O SD deve ser montado, caso não, um erro é lançado, avisando no **display OLED**.
- O botão B deve ser apertado para que ocorra a montagem do SD.
- O LED RGB sinaliza o estado atual.
    - **Amarelo**: montando/ desmontando SD;
- Após a montagem, os dados podem ser capturados.

### Durante a captura:

- O botão A deve ser apertado para que ocorra a captura de dados.
- Quando não quiser mais dados, basta apertar o botão novamente.
- O sensor envia dados brutos de aceleração (ax, ay, az) e giroscópio (gx, gy, gz) ao microcontrolador.
- Os dados são convertidos e salvos no cartão SD no formato .csv.
- O **display OLED** mostra o status do sistema (STS: *MOUNTED*), a ação que está sendo executada  (ACT: *Capturing*) e o número de amostras capturadas (COUNT: X).
- O **LED RGB** sinaliza o estado atual.
    - **Azul (piscando)**: capturando dados e salvando;
- Um **buzzer** emite um som ao ocorrer algum erro.

Após o experimento, os dados podem ser transferidos para um computador e visualizados através de um script Python. O script gera seis gráficos separados (um para cada eixo), organizados em uma janela com subplots, facilitando a análise temporal dos dados.

## BitDogLab e Código

O projeto utiliza os seguintes periféricos da plataforma BitDogLab:

- O **Display OLED SSD1306**: Mostra mensagens como de status (*Mounted* ou *Unmounted*), mensagens da ação que está sendo feita ( *Capturing*, *mounting*, *unmounting* e N/A que quer dizer *no action*), além também do número de amostras coletadas naquela execução.
  - Além disso, o display apresenta uma segunda tela, que mostra uma legenda contendo o que cada botão pode fazer, basta apertar o botão do ***joystick***, o **Botão J**.
- **LED RGB**:
    - **Amarelo**: Montando/desmontando SD.
    - **Verde**: Esperando comando.
    - **Vermelho**: Erro.
    - **Azul piscando**: Gravando e acessando SD.
- **Buzzer**: usado para indicar um erro, em conjunto com o LED RED. É emitido um beep longo.
- **Push Buttons**: os três botões foram usados, todos com a interrupção externa.
  - Botão A: ativa e desativa a captura de dados
  - Botão B: monta e desmonta o cartão SD
  - Botão J: muda a tela de visualização do **display OLED**.

- Tratamento por interrupções com lógica de debounce para evitar múltiplos acionamentos indevidos.

- **Código-fonte (Microcontrolador)**:
  - Utiliza a biblioteca FatFs para gerenciamento de arquivos no cartão SD.
  - Dados do sensor MPU6050 são lidos continuamente, de acordo com o acionamento do usuário, e armazenados no cartão SD no arquivo data.csv.
  - O sistema verifica se o arquivo já existe e só adiciona linhas se já existir. Caso não, cria o arquivo
- **Código Python (Computador)**
  - Script run.py lê o arquivo .csv e plota os gráficos dos 6 eixos em uma única janela com layout 3x2.
  - Cada gráfico mostra a variação dos valores ao longo do tempo (amostras sequenciais).
  - Usa pandas para manipulação dos dados e matplotlib para visualização gráfica.

## Link para acesso ao vídeo