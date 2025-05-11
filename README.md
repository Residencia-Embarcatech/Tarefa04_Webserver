# Sistema de Monitoramento de Enchentes em Rios

Sistema desenvolvimento como Projeto Final do curso de capacitação Embarcatech. 

## Descrição do Projeto

Este projeto implementa um sistema embarcado para monitoramento do nível de rios, com foco no Rio Cachoeira (Ilhéus, Itabuna), para controle e alerta de possíveis enchentes e/ou inundações. O sistema utiliza sensores para medir o nível do rio e a intensidade da chuva, gerando relatórios periódicos e enviando alertas quando necessário.

## Funcionalidades

- Monitoramento do nível do rio utilizando um sensor ultrassônico HC-SR04 (simulado pelo eixo Y do joystick).
- Detecção da intensidade da chuva utilizando um sensor de chuva YL-83 (simulado pelo eixo X do joystick).
- Exibição de alertas em um display SSD1306 via comunicação I2C.
- Geração de relatórios periódicos via UART.
- Geração de relatórios sob demanda ao pressionar o botão A.
- Classificação do risco em diferentes níveis: SEGURO, ATENÇÃO, ALERTA e PERIGO.

## Hardware Utilizado

- **Microcontrolador:** Raspberry Pi Pico W
- **Joystick:** Simulando sensores:
  - Eixo Y: Sensor ultrassônico HC-SR04 (para medir nível do rio)
  - Eixo X: Sensor de chuva YL-83 (para medir intensidade da chuva)
- **Botão A:** Para solicitação de relatórios
- **Display SSD1306:** Para exibição de alertas
- **UART:** Para comunicação serial e envio de relatórios

## Configuração dos Pinos

| Componente                  | Pino GPIO |
| --------------------------- | --------- |
| Sensor YL-83 (Joystick X)   | 26        |
| Sensor HC-SR04 (Joystick Y) | 27        |
| Botão A                     | 5         |
| SDA (I2C para Display)      | 14        |
| SCL (I2C para Display)      | 15        |
| UART0 (TX/RX)               | Padrão    |

## Compilação e Execução

1. **Pré-requisitos**:
   - Ter o ambiente de desenvolvimento para o Raspberry Pi Pico configurado (compilador, SDK, etc.).
   - CMake instalado.

2. **Compilação**:
   - Clone o repositório ou baixe os arquivos do projeto.
   - Navegue até a pasta do projeto e crie uma pasta de build:
     ```bash
     mkdir build
     cd build
     ```
   - Execute o CMake para configurar o projeto:
     ```bash
     cmake ..
     ```
   - Compile o projeto:
     ```bash
     make
     ```

3. **Upload para a placa**:
   - Conecte o Raspberry Pi Pico ao computador.
   - Copie o arquivo `.uf2` gerado para a placa.

## Simulação no Wokwi
Para visualizar a simulação do projeto no Wokwi:
1. Instale e configure o simulador Wokwi seguindo as instruções encontradas no link a seguir:  
   [Introdução ao Wokwi para VS Code](https://docs.wokwi.com/pt-BR/vscode/getting-started).
2. Abra o arquivo `diagram.json` no VS Code.
3. Clique em "Start Simulation".

## Vídeo de Demonstração
O vídeo demonstrativo do projeto está disponível no YouTube e pode ser acessado através do link abaixo:  
[Link para o vídeo](https://youtu.be/TJ7RmlqV_RI?si=Kxw_2LpnzQWSc2Uc)

