
# NeuroSync: Sistema de Biofeedback para Treinamento Cognitivo

## Visão Geral

O NeuroSync é um sistema educacional baseado no Raspberry Pi Pico que simula princípios de neurofeedback e biofeedback. Projetado para demonstrar conceitos de treinamento cognitivo, o sistema oferece feedback visual e sonoro em tempo real com base em entradas simuladas de atenção e relaxamento.

 **Contexto Educacional** : Este projeto faz parte de uma atividade de revisão para a Residência do Embarcatech, utilizando a placa da BitDogLab baseada no Raspberry Pi Pico.

## Licença

Este projeto está licenciado sob a [Licença MIT](LICENSE) - veja o arquivo LICENSE para detalhes.

## Características

* **Simulação de Ondas Cerebrais** : Simula ondas Alpha, Beta, Theta e Delta com base em entradas analógicas
* **Feedback em Tempo Real** : Feedback visual e sonoro imediato sobre estados cognitivos
* **Interface Visual** : Display OLED e matriz LED 5x5 para representação de estados
* **Modos de Operação** : Monitoramento, Configuração, Treinamento e Histórico
* **Personalização** : Parâmetros ajustáveis para limiares de atenção e relaxamento

## Hardware Necessário

* Raspberry Pi Pico
* Display OLED SSD1306 (via I2C)
* 2 potenciômetros (simulando sensores EEG/GSR)
* 3 botões (Avançar, Voltar, Configurar)
* 2 buzzers para feedback sonoro
* LED RGB para indicação visual
* Matriz WS2812 (5x5) para visualização de padrões

## Configuração dos Pinos

### GPIOs Utilizadas

* **OLED via I2C** :
* SDA = GPIO 14
* SCL = GPIO 15
* **Potenciômetros (ADC)** :
* Atenção (EEG simulado) = GPIO 27
* Relaxamento (GSR simulado) = GPIO 26
* **Botões** :
* NEXT (Avançar) = GPIO 5
* BACK (Retroceder) = GPIO 6
* SET (Configurar) = GPIO 22
* **Buzzers** :
* Principal (feedback) = GPIO 10
* Alertas (notificações) = GPIO 21
* **LED RGB** (PWM):
  * R (Vermelho) = GPIO 13
  * G (Verde) = GPIO 11
  * B (Azul) = GPIO 12
* **Matriz WS2812** :
* Pino de dados = GPIO 7

## Mapa Completo de Conexões

| Componente                | Função     | GPIO    | Observações             |
| ------------------------- | ------------ | ------- | ------------------------- |
| **OLED Display**    | SDA          | GPIO 14 | Comunicação I2C         |
|                           | SCL          | GPIO 15 | Comunicação I2C         |
| **Potenciômetros** | Atenção    | GPIO 27 | Entrada analógica (ADC1) |
|                           | Relaxamento  | GPIO 26 | Entrada analógica (ADC0) |
| **Botões**         | NEXT         | GPIO 5  | Com pull-up interno       |
|                           | BACK         | GPIO 6  | Com pull-up interno       |
|                           | SET          | GPIO 22 | Com pull-up interno       |
| **Buzzers**         | Principal    | GPIO 10 | Saída PWM para tons      |
|                           | Alertas      | GPIO 21 | Saída PWM para tons      |
| **LED RGB**         | R (Vermelho) | GPIO 13 | Saída PWM                |
|                           | G (Verde)    | GPIO 11 | Saída PWM                |
|                           | B (Azul)     | GPIO 12 | Saída PWM                |
| **Matriz WS2812**   | Dados        | GPIO 7  | Controlada via PIO        |

### Detalhes de Implementação

* Os potenciômetros são conectados aos pinos ADC e simulam sensores de atenção e relaxamento
* Os botões utilizam os pull-ups internos do Raspberry Pi Pico e são configurados como entrada
* Os buzzers são controlados via PWM para gerar diferentes frequências de tom
* O LED RGB utiliza PWM em cada canal para controle de intensidade de cor
* A matriz WS2812 (5x5) é controlada utilizando a capacidade de PIO do RP2040

### 1. Modo de Monitoramento

Exibe em tempo real os níveis de atenção e relaxamento, bem como o estado cognitivo atual. A interface visual mostra "carinhas" diferentes na matriz de LEDs e cores no LED RGB para indicar o estado:

* **Distraído** : Carinha triste, LED amarelo
* **Normal** : Carinha neutra, LED azul
* **Alta Concentração** : Carinha feliz, LED verde
* **Relaxamento Profundo** : Carinha neutra, LED ciano
* **Estado Flow** : Carinha feliz, LED verde-azulado
* **Ansiedade** : Carinha triste, LED vermelho

### 2. Modo de Configuração

Permite ajustar os parâmetros de limiar que determinam os estados cognitivos:

* Limiar Atenção Baixo
* Limiar Atenção Alto
* Limiar Relaxamento Baixo
* Limiar Relaxamento Alto

Use os botões NEXT e BACK para ajustar os valores e o botão SET para alternar entre os parâmetros.

### 3. Modo de Treinamento

Fornece exercícios direcionados para praticar estados cognitivos específicos:

* **Atenção** : Concentre-se para aumentar o nível de atenção
* **Relaxamento** : Relaxe para aumentar o nível de relaxamento
* **Estado Flow** : Combine alta atenção com alto relaxamento

O treinamento dura até 5 minutos e progride por 10 níveis. Se todos os níveis forem completados, o treinamento é considerado bem-sucedido.

### 4. Modo de Histórico

Apresenta estatísticas sobre o uso do sistema:

* Média de atenção e relaxamento
* Máximos de atenção e relaxamento alcançados
* Número de sessões concluídas
* Tempo total de uso

## Feedback Sonoro

* **Bipes curtos** : Confirmação de ações (navegação de menu, ajuste de valores)
* **Melodia ascendente** : Sucesso/Conclusão positiva (iniciar treinamento, completar nível)
* **Melodia descendente** : Erro/Conclusão negativa (falha no treinamento, tempo esgotado)

## Operação

1. Navegue entre os modos utilizando os botões NEXT e BACK
2. Use o botão SET para entrar em submenus ou confirmar ações
3. No modo de treinamento, use NEXT para selecionar o tipo de treinamento e SET para iniciar
4. No modo de configuração, use SET para entrar no modo de ajuste e depois para navegar entre parâmetros

## Estados Cognitivos

O sistema classifica o estado cognitivo do usuário em seis categorias:

* **Distraído** : Atenção abaixo do limiar baixo
* **Normal** : Estado neutro
* **Alta Concentração** : Atenção acima do limiar alto
* **Relaxamento Profundo** : Relaxamento acima do limiar alto
* **Estado Flow** : Alta atenção combinada com alto relaxamento
* **Ansiedade** : Alta atenção combinada com baixo relaxamento

## Limitações Conhecidas

* O tempo limite de treinamento é fixado em 5 minutos e não pode ser alterado pela interface
* O sistema é educacional e simulado, não representa medições reais de EEG
* Após configurar todos os parâmetros, pode aparecer "Parâmetro Desconhecido" se a variável `current_param` não for reiniciada

## Notas de Depuração

O sistema envia informações de depuração para o terminal serial, incluindo:

* Níveis atuais de atenção e relaxamento
* Valores simulados das ondas cerebrais
* Estado cognitivo atual

## Modificações Sugeridas

* Adicionar opção para configurar o tempo limite de treinamento
* Implementar armazenamento de configurações na memória Flash
* Adicionar mais padrões visuais para a matriz de LEDs
* Corrigir o bug de "Parâmetro Desconhecido" reiniciando `current_param` quando sair do modo de configuração

## Sobre a Plataforma

Este projeto utiliza a placa da BitDogLab, que é baseada no Raspberry Pi Pico. A placa integra diversos componentes periféricos utilizados no projeto, facilitando a prototipagem e o desenvolvimento de sistemas embarcados.

## Propósito Educacional

O NeuroSync foi desenvolvido como parte de uma atividade de revisão para a Residência do Embarcatech, um programa de formação em sistemas embarcados. O projeto demonstra conceitos de:

* Programação de microcontroladores
* Interfaces homem-máquina
* Processamento de sinais (simulados)
* Feedback multissensorial
* Design de sistemas embarcados interativos
