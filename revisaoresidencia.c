/*
 * NeuroSync: Sistema de Biofeedback para Treinamento Cognitivo
 * 
 * Um sistema que utiliza o Raspberry Pi Pico para simular e demonstrar
 * princípios de neurofeedback e biofeedback para fins educacionais.
 * 
 * Hardware:
 * - Raspberry Pi Pico
 * - Display OLED SSD1306 via I2C
 * - 2 potenciômetros (joystick) para simular sensores EEG/GSR
 * - 3 botões: BUTTON_NEXT (pino 5), BUTTON_BACK (pino 6) e BUTTON_SET (pino 22)
 * - 2 buzzers para feedback sonoro
 * - LED RGB para indicação visual
 * - Matriz WS2812 (5x5) para visualização de padrões
 */

 #include "pico/stdlib.h"
 #include "hardware/i2c.h"
 #include "hardware/pwm.h"
 #include "hardware/adc.h"
 #include "hardware/pio.h"
 #include "hardware/timer.h"
 #include "hardware/clocks.h"
 #include "include/ssd1306.h"    // Display OLED
 #include "include/font.h"       // Fonte para o OLED
 #include "ws2812.pio.h"         // Matriz WS2812 via PIO
 #include <stdio.h>
 #include <string.h>
 #include <stdlib.h>
 #include <math.h>
 
 //===============================================
 // Configurações dos pinos
 //===============================================
 // OLED via I2C
 const uint8_t SDA = 14;
 const uint8_t SCL = 15;
 #define I2C_ADDR 0x3C
 #define SSD1306_WIDTH 128
 #define SSD1306_HEIGHT 64
 
 // Potenciômetros (simulando sensores)
 #define POT_ATENCAO_PIN 27    // Simula EEG (foco/atenção)
 #define POT_RELAXAMENTO_PIN 26 // Simula GSR (relaxamento)
 
 // Botões
 #define BUTTON_NEXT 5   // Avança (menu ou aumenta parâmetro)
 #define BUTTON_BACK 6   // Retrocede (menu ou diminui parâmetro)
 #define BUTTON_SET 22   // Alterna entre modos
 
 // Buzzers
 #define BUZZER1_PIN 10  // Feedback principal
 #define BUZZER2_PIN 21  // Alertas
 
 // LED RGB (PWM)
 #define R_LED_PIN 13
 #define G_LED_PIN 11
 #define B_LED_PIN 12
 #define PWM_WRAP 255
 
 // Matriz WS2812
 #define NUM_PIXELS 25
 #define WS2812_PIN 7
 #define IS_RGBW false
 
 //===============================================
 // Variáveis globais
 //===============================================
 // Cores para a matriz WS2812
 #define COR_WS2812_R 20
 #define COR_WS2812_G 20
 #define COR_WS2812_B 50
 
 // Matriz de pixels
 bool buffer_leds[NUM_PIXELS] = { false };
 
 // Gestão de modos e estados
 volatile int menu_index = 0;  // 0=Monitoramento, 1=Configuração, 2=Treinamento, 3=Histórico
 volatile bool in_set_mode = false;
 volatile int current_param = 0; // Parâmetro atual em configuração
 volatile uint32_t last_button_time = 0;
 const uint32_t DEBOUNCE_DELAY_MS = 200;
 
 // Estado cognitivo
 typedef struct {
     float atencao;      // 0-100%
     float relaxamento;  // 0-10
     float alpha;        // Ondas Alpha (8-12 Hz) - simulada
     float beta;         // Ondas Beta (12-30 Hz) - simulada
     float theta;        // Ondas Theta (4-8 Hz) - simulada
     float delta;        // Ondas Delta (0.5-4 Hz) - simulada
 } EstadoCognitivo;
 
 EstadoCognitivo estado_atual = {0};
 
 // Limiares e configurações
 volatile float limiar_atencao_baixo = 30.0f;
 volatile float limiar_atencao_alto = 70.0f;
 volatile float limiar_relaxamento_baixo = 3.0f;
 volatile float limiar_relaxamento_alto = 7.0f;
 
 // Estatísticas para histórico
 typedef struct {
     float soma_atencao;
     float soma_relaxamento;
     float max_atencao;
     float max_relaxamento;
     uint32_t amostras;
     uint32_t tempo_inicio;
     uint32_t tempo_ultimo_treino;
     uint8_t sessoes_concluidas;
 } Estatisticas;
 
 Estatisticas stats = {0};
 
 // Dados de treinamento
 typedef struct {
     uint8_t nivel_atual;    // Nível atual (1-10)
     uint8_t nivel_maximo;   // Nível máximo alcançado
     uint32_t duracao;       // Duração em segundos
     uint8_t objetivo;       // 0=Atenção, 1=Relaxamento, 2=Estado Flow
     uint8_t status;         // 0=Não iniciado, 1=Em andamento, 2=Concluído, 3=Falha
     uint32_t inicio;        // Tempo de início
     uint32_t pontuacao;     // Pontuação acumulada
 } DadosTreinamento;
 
 DadosTreinamento treinamento = {0};
 
 //===============================================
 // Padrões visuais para a matriz de LEDs (5x5)
 //===============================================
 // Índice 0: Carinha Neutra, 1: Carinha Feliz, 2: Carinha Triste
 const bool padroes_carinhas[3][5][5] = {
     { // Carinha Neutra
       {false, true, false, true, false},
       {false, true, false, true, false},
       {false, false, false, false, false},
       {true, false, false, false, true},
       {false, true, true, true, false}
     },
     { // Carinha Feliz
       {false, true, false, true, false},
       {false, true, false, true, false},
       {false, false, false, false, false},
       {true, true, true, true, true},
       {true, false, false, false, true}
     },
     { // Carinha Triste
       {false, true, false, true, false},
       {false, true, false, true, false},
       {false, false, false, false, false},
       {false, true, true, true, false},
       {true, false, false, false, true}
     }
 };
 
 // Padrão de ondas cerebrais (simulado)
 const bool padrao_ondas[5][5] = {
     {false, false, true, false, false},
     {false, true, true, true, false},
     {true, true, true, true, true},
     {false, true, true, true, false},
     {false, false, true, false, false}
 };
 
 // Padrão para foco alto
 const bool padrao_foco[5][5] = {
     {false, false, true, false, false},
     {false, true, true, true, false},
     {true, true, true, true, true},
     {false, true, true, true, false},
     {false, false, true, false, false}
 };
 
 // Padrão para relaxamento
 const bool padrao_relaxamento[5][5] = {
     {true, false, false, false, true},
     {false, true, false, true, false},
     {false, false, true, false, false},
     {false, true, false, true, false},
     {true, false, false, false, true}
 };
 
 //===============================================
 // Funções auxiliares
 //===============================================
 
 // Updated LED RGB initialization for NeuroSync
// Alternative approach: Drive green LED directly (not PWM)
void init_rgb_led() {
    // Configure RED and BLUE with PWM
    gpio_set_function(R_LED_PIN, GPIO_FUNC_PWM);
    gpio_set_function(B_LED_PIN, GPIO_FUNC_PWM);
    
    uint slice_r = pwm_gpio_to_slice_num(R_LED_PIN);
    uint slice_b = pwm_gpio_to_slice_num(B_LED_PIN);
    
    pwm_set_wrap(slice_r, PWM_WRAP);
    pwm_set_clkdiv(slice_r, 125.0f);
    pwm_set_enabled(slice_r, true);
    
    if (slice_b != slice_r) {
        pwm_set_wrap(slice_b, PWM_WRAP);
        pwm_set_clkdiv(slice_b, 125.0f);
        pwm_set_enabled(slice_b, true);
    }
    
    // Configure GREEN as direct digital output
    gpio_init(G_LED_PIN);
    gpio_set_dir(G_LED_PIN, GPIO_OUT);
}

// Then modify set_rgb_color to handle this change:
void set_rgb_color(uint8_t r, uint8_t g, uint8_t b) {
    pwm_set_chan_level(pwm_gpio_to_slice_num(R_LED_PIN), pwm_gpio_to_channel(R_LED_PIN), r);
    gpio_put(G_LED_PIN, g > 128); // Digital on/off based on green intensity
    pwm_set_chan_level(pwm_gpio_to_slice_num(B_LED_PIN), pwm_gpio_to_channel(B_LED_PIN), b);
}
 

 //===============================================
 // Funções da matriz de LEDs
 //===============================================
 
 // Atualiza o buffer com um padrão de carinha
 void atualizar_buffer_com_carinha(int tipo) {
     // tipo: 0 = neutra, 1 = feliz, 2 = triste
     for (int linha = 0; linha < 5; linha++) {
         for (int coluna = 0; coluna < 5; coluna++) {
             int indice = linha * 5 + coluna;
             buffer_leds[indice] = padroes_carinhas[tipo][linha][coluna];
         }
     }
 }
 
 // Atualiza o buffer com o padrão de ondas cerebrais
 void atualizar_buffer_com_ondas() {
     for (int linha = 0; linha < 5; linha++) {
         for (int coluna = 0; coluna < 5; coluna++) {
             int indice = linha * 5 + coluna;
             buffer_leds[indice] = padrao_ondas[linha][coluna];
         }
     }
 }
 
 // Função auxiliar para formatar cores para a matriz de LEDs
 static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
     return ((uint32_t)r << 8) | ((uint32_t)g << 16) | (uint32_t)b;
 }
 
 // Função auxiliar para enviar um pixel para a matriz
 static inline void enviar_pixel(uint32_t pixel_grb) {
     pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
 }
 
 // Define os LEDs da matriz com base no buffer
 void definir_leds(uint8_t r, uint8_t g, uint8_t b) {
     uint32_t cor = urgb_u32(r, g, b);
     for (int i = 0; i < NUM_PIXELS; i++) {
         if (buffer_leds[i])
             enviar_pixel(cor);
         else
             enviar_pixel(0);
     }
     sleep_us(60);
 }
 
 //===============================================
 // Funções para feedback sonoro
 //===============================================
 
 // Gera um tom no buzzer (bloqueante)
 void play_tone(uint gpio, int frequency, int duration_ms) {
     uint slice_num = pwm_gpio_to_slice_num(gpio);
     uint channel = pwm_gpio_to_channel(gpio);
     gpio_set_function(gpio, GPIO_FUNC_PWM);
     float divider = 1.0f;
     uint32_t wrap = (uint32_t)((125000000.0f / (divider * frequency)) - 1);
     pwm_set_clkdiv(slice_num, divider);
     pwm_set_wrap(slice_num, wrap);
     pwm_set_chan_level(slice_num, channel, (wrap + 1) / 2);
     pwm_set_enabled(slice_num, true);
     sleep_ms(duration_ms);
     pwm_set_enabled(slice_num, false);
     gpio_set_function(gpio, GPIO_FUNC_SIO);
     gpio_set_dir(gpio, GPIO_OUT);
     gpio_put(gpio, 0);
 }
 
 // Callback para parar o tom (não bloqueante)
 int64_t stop_tone_callback(alarm_id_t id, void *user_data) {
     uint gpio = *(uint*)user_data;
     uint slice_num = pwm_gpio_to_slice_num(gpio);
     pwm_set_enabled(slice_num, false);
     gpio_set_function(gpio, GPIO_FUNC_SIO);
     gpio_set_dir(gpio, GPIO_OUT);
     gpio_put(gpio, 0);
     free(user_data);
     return 0;
 }
 
 // Toca um tom de forma não bloqueante
 void play_tone_non_blocking(uint gpio, int frequency, int duration_ms) {
     uint slice_num = pwm_gpio_to_slice_num(gpio);
     uint channel = pwm_gpio_to_channel(gpio);
     gpio_set_function(gpio, GPIO_FUNC_PWM);
     float divider = 1.0f;
     uint32_t wrap = (uint32_t)((125000000.0f / (divider * frequency)) - 1);
     pwm_set_clkdiv(slice_num, divider);
     pwm_set_wrap(slice_num, wrap);
     pwm_set_chan_level(slice_num, channel, (wrap + 1) / 2);
     pwm_set_enabled(slice_num, true);
     uint *gpio_ptr = malloc(sizeof(uint));
     *gpio_ptr = gpio;
     add_alarm_in_ms(duration_ms, stop_tone_callback, gpio_ptr, false);
 }
 
 // Reproduz uma sequência de tons indicando sucesso
 void tocar_sucesso() {
     play_tone_non_blocking(BUZZER1_PIN, 523, 200); // Do
     sleep_ms(220);
     play_tone_non_blocking(BUZZER1_PIN, 659, 200); // Mi
     sleep_ms(220);
     play_tone_non_blocking(BUZZER1_PIN, 784, 400); // Sol
 }
 
 // Reproduz uma sequência de tons indicando erro
 void tocar_erro() {
     play_tone_non_blocking(BUZZER2_PIN, 440, 200); // Lá
     sleep_ms(250);
     play_tone_non_blocking(BUZZER2_PIN, 349, 400); // Fá
 }
 
 // Reproduz um bipe básico para indicação
 void beep() {
     play_tone_non_blocking(BUZZER2_PIN, 392, 100); // Sol
 }
 
 //===============================================
 // Funções para simulação de ondas cerebrais
 //===============================================
 
 // Obtém o nível de atenção simulado a partir do potenciômetro X
 float obter_nivel_atencao(uint16_t adc_valor) {
     // Adiciona pequena variação aleatória para simular flutuações naturais
     float ruido = ((float)rand() / RAND_MAX) * 5.0f - 2.5f; // ±2.5% de ruído
     float valor = (adc_valor / 4095.0f) * 100.0f + ruido;
     
     // Limita entre 0-100%
     if (valor < 0.0f) valor = 0.0f;
     if (valor > 100.0f) valor = 100.0f;
     
     return valor;
 }
 
 // Obtém o nível de relaxamento simulado a partir do potenciômetro Y
 float obter_nivel_relaxamento(uint16_t adc_valor) {
     // Adiciona pequena variação aleatória para simular flutuações naturais
     float ruido = ((float)rand() / RAND_MAX) * 0.5f - 0.25f; // ±0.25 de ruído
     float valor = (adc_valor / 4095.0f) * 10.0f + ruido;
     
     // Limita entre 0-10
     if (valor < 0.0f) valor = 0.0f;
     if (valor > 10.0f) valor = 10.0f;
     
     return valor;
 }
 
 // Simula as ondas cerebrais com base nos níveis de atenção e relaxamento
 void simular_ondas_cerebrais(EstadoCognitivo *estado) {
     // Aqui simulamos uma relação entre os valores dos potenciômetros e as ondas cerebrais
     // Estes são modelos simplificados para fins educativos
     
     // Atenção alta = mais ondas beta, menos theta
     estado->beta = 10.0f + (estado->atencao / 100.0f) * 20.0f;
     estado->theta = 20.0f - (estado->atencao / 100.0f) * 15.0f;
     
     // Relaxamento alto = mais ondas alpha, menos beta
     estado->alpha = 5.0f + (estado->relaxamento / 10.0f) * 10.0f;
     
     // Delta é mais associado ao sono profundo
     // Aumenta quando ambos atenção e relaxamento são baixos
     float media_ativacao = (estado->atencao/100.0f + estado->relaxamento/10.0f) / 2.0f;
     estado->delta = 20.0f - media_ativacao * 18.0f;
     
     // Garantir que todos os valores estejam em faixas razoáveis
     if (estado->beta < 0.0f) estado->beta = 0.0f;
     if (estado->alpha < 0.0f) estado->alpha = 0.0f;
     if (estado->theta < 0.0f) estado->theta = 0.0f;
     if (estado->delta < 0.0f) estado->delta = 0.0f;
 }
 
 // Determina o estado cognitivo global com base nos parâmetros
 int determinar_estado_cognitivo(EstadoCognitivo *estado) {
     /*
     Estados:
     0 = Atenção Baixa / Distraído
     1 = Atenção Normal
     2 = Alta Concentração
     3 = Relaxamento Profundo
     4 = Estado Flow (Atenção Alta + Relaxamento Alto)
     5 = Ansiedade (Atenção Alta + Relaxamento Muito Baixo)
     */
     
     if (estado->atencao >= limiar_atencao_alto && estado->relaxamento >= limiar_relaxamento_alto) {
         return 4; // Estado Flow
     } else if (estado->atencao >= limiar_atencao_alto && estado->relaxamento < limiar_relaxamento_baixo) {
         return 5; // Ansiedade
     } else if (estado->atencao >= limiar_atencao_alto) {
         return 2; // Alta Concentração
     } else if (estado->atencao < limiar_atencao_baixo) {
         return 0; // Distraído
     } else if (estado->relaxamento >= limiar_relaxamento_alto) {
         return 3; // Relaxamento Profundo
     } else {
         return 1; // Estado Normal
     }
 }
 
 //===============================================
 // Funções do Display OLED
 //===============================================
 
 // Atualiza o display no modo de monitoramento
 void atualizar_display_monitoramento(ssd1306_t *ssd, EstadoCognitivo *estado, int estado_cognitivo) {
     char linha1[32], linha2[32], linha3[32];
     
     // Determina o nome do estado cognitivo
     const char *estado_nome;
     switch (estado_cognitivo) {
         case 0: estado_nome = "Distraido"; break;
         case 1: estado_nome = "Normal"; break;
         case 2: estado_nome = "Concentrado"; break;
         case 3: estado_nome = "Relaxado"; break;
         case 4: estado_nome = "Estado Flow"; break;
         case 5: estado_nome = "Ansioso"; break;
         default: estado_nome = "Desconhecido"; break;
     }
     
     sprintf(linha1, "NeuroSync - Monitora");
     sprintf(linha2, "Atencao: %.1f%% Rel: %.1f", estado->atencao, estado->relaxamento);
     sprintf(linha3, "Estado: %s", estado_nome);
     
     ssd1306_fill(ssd, 0);
     ssd1306_draw_string(ssd, linha1, 0, 0);
     ssd1306_draw_string(ssd, linha2, 0, 20);
     ssd1306_draw_string(ssd, linha3, 0, 40);
     ssd1306_send_data(ssd);
 }
 
 // Atualiza o display no modo de configuração
 void atualizar_display_configuracao(ssd1306_t *ssd, int param_atual) {
     char linha1[32], linha2[32], linha3[32];
     
     sprintf(linha1, "NeuroSync - Config");
     
     switch (param_atual) {
         case 0:
             sprintf(linha2, "Limiar Atencao Baixo");
             sprintf(linha3, "Valor: %.1f%%", limiar_atencao_baixo);
             break;
         case 1:
             sprintf(linha2, "Limiar Atencao Alto");
             sprintf(linha3, "Valor: %.1f%%", limiar_atencao_alto);
             break;
         case 2:
             sprintf(linha2, "Limiar Relax Baixo");
             sprintf(linha3, "Valor: %.1f", limiar_relaxamento_baixo);
             break;
         case 3:
             sprintf(linha2, "Limiar Relax Alto");
             sprintf(linha3, "Valor: %.1f", limiar_relaxamento_alto);
             break;
         default:
             sprintf(linha2, "Parametro Desconhecido");
             sprintf(linha3, "Erro");
             break;
     }
     
     ssd1306_fill(ssd, 0);
     ssd1306_draw_string(ssd, linha1, 0, 0);
     ssd1306_draw_string(ssd, linha2, 0, 20);
     ssd1306_draw_string(ssd, linha3, 0, 40);
     ssd1306_send_data(ssd);
 }
 
 // Atualiza o display no modo de treinamento
 void atualizar_display_treinamento(ssd1306_t *ssd, DadosTreinamento *treino) {
     char linha1[32], linha2[32], linha3[32];
     
     sprintf(linha1, "NeuroSync - Treino");
     
     const char *objetivo_nome;
     switch (treino->objetivo) {
         case 0: objetivo_nome = "Atencao"; break;
         case 1: objetivo_nome = "Relaxamento"; break;
         case 2: objetivo_nome = "Estado Flow"; break;
         default: objetivo_nome = "Desconhecido"; break;
     }
     
     uint32_t tempo_decorrido = 0;
     if (treino->status == 1) { // Em andamento
         tempo_decorrido = time_us_32() / 1000000 - treino->inicio;
     } else if (treino->status == 2 || treino->status == 3) { // Concluído ou Falha
         tempo_decorrido = treino->duracao;
     }
     
     sprintf(linha2, "Objetivo: %s Niv:%d/%d", objetivo_nome, treino->nivel_atual, treino->nivel_maximo);
     sprintf(linha3, "Pontos: %d Tempo: %ds", treino->pontuacao, tempo_decorrido);
     
     ssd1306_fill(ssd, 0);
     ssd1306_draw_string(ssd, linha1, 0, 0);
     ssd1306_draw_string(ssd, linha2, 0, 20);
     ssd1306_draw_string(ssd, linha3, 0, 40);
     ssd1306_send_data(ssd);
 }
 
 // Atualiza o display no modo de histórico
 void atualizar_display_historico(ssd1306_t *ssd, Estatisticas *stats) {
     char linha1[32], linha2[32], linha3[32];
     
     sprintf(linha1, "NeuroSync - Historico");
     
     float media_atencao = 0.0f;
     float media_relaxamento = 0.0f;
     
     if (stats->amostras > 0) {
         media_atencao = stats->soma_atencao / stats->amostras;
         media_relaxamento = stats->soma_relaxamento / stats->amostras;
     }
     
     uint32_t tempo_total = time_us_32() / 1000000 - stats->tempo_inicio;
     uint32_t minutos = tempo_total / 60;
     uint32_t segundos = tempo_total % 60;
     
     sprintf(linha2, "At: %.1f%% Rx: %.1f", media_atencao, media_relaxamento);
     sprintf(linha3, "Sessoes: %d Tempo: %02dm%02ds", stats->sessoes_concluidas, minutos, segundos);
     
     ssd1306_fill(ssd, 0);
     ssd1306_draw_string(ssd, linha1, 0, 0);
     ssd1306_draw_string(ssd, linha2, 0, 20);
     ssd1306_draw_string(ssd, linha3, 0, 40);
     ssd1306_send_data(ssd);
 }
 
 //===============================================
 // Funções para modos de operação
 //===============================================
 
 // Modo de monitoramento (principal)
 void executar_modo_monitoramento(ssd1306_t *ssd) {
     // Lê os valores dos potenciômetros
     adc_select_input(POT_ATENCAO_PIN - 26);
     int adc_atencao = adc_read();
     estado_atual.atencao = obter_nivel_atencao(adc_atencao);
     
     adc_select_input(POT_RELAXAMENTO_PIN - 26);
     int adc_relaxamento = adc_read();
     estado_atual.relaxamento = obter_nivel_relaxamento(adc_relaxamento);
     
     // Simula as ondas cerebrais
     simular_ondas_cerebrais(&estado_atual);
     
     // Determina o estado cognitivo atual
     int estado_cognitivo = determinar_estado_cognitivo(&estado_atual);
     
     // Envia dados para o terminal serial para depuração
     printf("MONITOR - Atencao: %.2f, Relaxamento: %.2f, Estado: %d\n", 
            estado_atual.atencao, estado_atual.relaxamento, estado_cognitivo);
     printf("ONDAS - Alpha: %.2f, Beta: %.2f, Theta: %.2f, Delta: %.2f\n", 
            estado_atual.alpha, estado_atual.beta, estado_atual.theta, estado_atual.delta);
     
     // Atualiza o display
     atualizar_display_monitoramento(ssd, &estado_atual, estado_cognitivo);
     
     // Atualiza a matriz de LEDs conforme o estado
     switch (estado_cognitivo) {
         case 0: // Distraído
             atualizar_buffer_com_carinha(2); // Triste
             set_rgb_color(255, 255, 0); // Amarelo
             break;
         case 1: // Normal
             atualizar_buffer_com_carinha(0); // Neutra
             set_rgb_color(0, 0, 255); // Azul
             break;
         case 2: // Alta Concentração
             atualizar_buffer_com_carinha(1); // Feliz
             set_rgb_color(0, 255, 0); // Verde
             break;
         case 3: // Relaxamento Profundo
             atualizar_buffer_com_carinha(0); // Neutra
             set_rgb_color(0, 255, 255); // Ciano
             break;
         case 4: // Estado Flow
             atualizar_buffer_com_carinha(1); // Feliz
             set_rgb_color(0, 255, 128); // Verde-azulado
             break;
         case 5: // Ansiedade
             atualizar_buffer_com_carinha(2); // Triste
             set_rgb_color(255, 0, 0); // Vermelho
             break;
     }
     
     definir_leds(COR_WS2812_R, COR_WS2812_G, COR_WS2812_B);
     
     // Atualiza estatísticas
     stats.soma_atencao += estado_atual.atencao;
     stats.soma_relaxamento += estado_atual.relaxamento;
     stats.amostras++;
     
     if (estado_atual.atencao > stats.max_atencao) {
         stats.max_atencao = estado_atual.atencao;
     }
     
     if (estado_atual.relaxamento > stats.max_relaxamento) {
         stats.max_relaxamento = estado_atual.relaxamento;
     }
 }
 
 // Modo de configuração
 void executar_modo_configuracao(ssd1306_t *ssd) {
     // Atualiza o display com o parâmetro atual
     atualizar_display_configuracao(ssd, current_param);
     
     // Verifica os botões NEXT e BACK para ajustar o valor
     if (gpio_get(BUTTON_NEXT) == 0) { // Botão pressionado (pull-up)
         uint32_t current_time = time_us_32() / 1000;
         if (current_time - last_button_time > DEBOUNCE_DELAY_MS) {
             last_button_time = current_time;
             
             // Aumenta o valor do parâmetro atual
             switch (current_param) {
                 case 0: // Limiar Atenção Baixo
                     limiar_atencao_baixo += 5.0f;
                     if (limiar_atencao_baixo > limiar_atencao_alto - 5.0f) {
                         limiar_atencao_baixo = limiar_atencao_alto - 5.0f;
                     }
                     if (limiar_atencao_baixo > 95.0f) {
                         limiar_atencao_baixo = 95.0f;
                     }
                     break;
                 case 1: // Limiar Atenção Alto
                     limiar_atencao_alto += 5.0f;
                     if (limiar_atencao_alto > 100.0f) {
                         limiar_atencao_alto = 100.0f;
                     }
                     break;
                 case 2: // Limiar Relaxamento Baixo
                     limiar_relaxamento_baixo += 0.5f;
                     if (limiar_relaxamento_baixo > limiar_relaxamento_alto - 0.5f) {
                         limiar_relaxamento_baixo = limiar_relaxamento_alto - 0.5f;
                     }
                     if (limiar_relaxamento_baixo > 9.5f) {
                         limiar_relaxamento_baixo = 9.5f;
                     }
                     break;
                 case 3: // Limiar Relaxamento Alto
                     limiar_relaxamento_alto += 0.5f;
                     if (limiar_relaxamento_alto > 10.0f) {
                         limiar_relaxamento_alto = 10.0f;
                     }
                     break;
             }
             beep();
         }
     }
     
     if (gpio_get(BUTTON_BACK) == 0) { // Botão pressionado (pull-up)
         uint32_t current_time = time_us_32() / 1000;
         if (current_time - last_button_time > DEBOUNCE_DELAY_MS) {
             last_button_time = current_time;
             
             // Diminui o valor do parâmetro atual
             switch (current_param) {
                 case 0: // Limiar Atenção Baixo
                     limiar_atencao_baixo -= 5.0f;
                     if (limiar_atencao_baixo < 5.0f) {
                         limiar_atencao_baixo = 5.0f;
                     }
                     break;
                 case 1: // Limiar Atenção Alto
                     limiar_atencao_alto -= 5.0f;
                     if (limiar_atencao_alto < limiar_atencao_baixo + 5.0f) {
                         limiar_atencao_alto = limiar_atencao_baixo + 5.0f;
                     }
                     break;
                 case 2: // Limiar Relaxamento Baixo
                     limiar_relaxamento_baixo -= 0.5f;
                     if (limiar_relaxamento_baixo < 0.5f) {
                         limiar_relaxamento_baixo = 0.5f;
                     }
                     break;
                 case 3: // Limiar Relaxamento Alto
                     limiar_relaxamento_alto -= 0.5f;
                     if (limiar_relaxamento_alto < limiar_relaxamento_baixo + 0.5f) {
                         limiar_relaxamento_alto = limiar_relaxamento_baixo + 0.5f;
                     }
                     break;
             }
             beep();
         }
     }
     
     // Mostra números na matriz de LEDs para representar os valores
     // Matriz limpa
     for (int i = 0; i < NUM_PIXELS; i++) {
         buffer_leds[i] = false;
     }
     
     // Acende LEDs conforme o valor atual
     float valor_percentual = 0.0f;
     switch (current_param) {
         case 0: // Limiar Atenção Baixo
             valor_percentual = limiar_atencao_baixo / 100.0f;
             break;
         case 1: // Limiar Atenção Alto
             valor_percentual = limiar_atencao_alto / 100.0f;
             break;
         case 2: // Limiar Relaxamento Baixo
             valor_percentual = limiar_relaxamento_baixo / 10.0f;
             break;
         case 3: // Limiar Relaxamento Alto
             valor_percentual = limiar_relaxamento_alto / 10.0f;
             break;
     }
     
     // Acende LEDs proporcionalmente ao valor
     int leds_acesos = (int)(valor_percentual * NUM_PIXELS);
     for (int i = 0; i < leds_acesos; i++) {
         buffer_leds[i] = true;
     }
     
     // Cor do LED RGB conforme o parâmetro atual
     switch (current_param) {
         case 0: case 1: // Parâmetros de Atenção
             set_rgb_color(0, 0, 255); // Azul
             break;
         case 2: case 3: // Parâmetros de Relaxamento
             set_rgb_color(0, 255, 255); // Ciano
             break;
     }
     
     definir_leds(COR_WS2812_R, COR_WS2812_G, COR_WS2812_B);
 }
 
 // Modo de treinamento
 void executar_modo_treinamento(ssd1306_t *ssd) {
     // Lê os valores dos potenciômetros
     adc_select_input(POT_ATENCAO_PIN - 26);
     int adc_atencao = adc_read();
     estado_atual.atencao = obter_nivel_atencao(adc_atencao);
     
     adc_select_input(POT_RELAXAMENTO_PIN - 26);
     int adc_relaxamento = adc_read();
     estado_atual.relaxamento = obter_nivel_relaxamento(adc_relaxamento);
     
     // Simula as ondas cerebrais
     simular_ondas_cerebrais(&estado_atual);
     
     // Se o treinamento não foi iniciado, configura
     if (treinamento.status == 0) {
         // Usando o estado do botão NEXT como um chaveador do tipo de treinamento
         if (gpio_get(BUTTON_NEXT) == 0) { // Botão pressionado
             uint32_t current_time = time_us_32() / 1000;
             if (current_time - last_button_time > DEBOUNCE_DELAY_MS) {
                 last_button_time = current_time;
                 
                 // Alterna entre os tipos de treinamento
                 treinamento.objetivo = (treinamento.objetivo + 1) % 3;
                 beep();
             }
         }
         
         // Inicia o treinamento com o botão SET
         if (gpio_get(BUTTON_SET) == 0) { // Botão pressionado
             uint32_t current_time = time_us_32() / 1000;
             if (current_time - last_button_time > DEBOUNCE_DELAY_MS) {
                 last_button_time = current_time;
                 
                 // Inicia o treinamento
                 treinamento.status = 1; // Em andamento
                 treinamento.inicio = time_us_32() / 1000000;
                 treinamento.nivel_atual = 1;
                 treinamento.nivel_maximo = 10;
                 treinamento.pontuacao = 0;
                 
                 tocar_sucesso();
             }
         }
     }
     // Se o treinamento está em andamento
     else if (treinamento.status == 1) {
         // Verifica se atingiu o objetivo conforme o tipo de treinamento
         bool objetivo_atingido = false;
         
         switch (treinamento.objetivo) {
             case 0: // Atenção
                 objetivo_atingido = estado_atual.atencao >= limiar_atencao_alto;
                 break;
             case 1: // Relaxamento
                 objetivo_atingido = estado_atual.relaxamento >= limiar_relaxamento_alto;
                 break;
             case 2: // Estado Flow
                 objetivo_atingido = (estado_atual.atencao >= limiar_atencao_alto && 
                                      estado_atual.relaxamento >= limiar_relaxamento_alto);
                 break;
         }
         
         // Se atingiu o objetivo, aumenta a pontuação
         if (objetivo_atingido) {
             treinamento.pontuacao += 1;
             
             // A cada 50 pontos, aumenta o nível se não estiver no máximo
             if (treinamento.pontuacao % 50 == 0 && treinamento.nivel_atual < treinamento.nivel_maximo) {
                 treinamento.nivel_atual++;
                 tocar_sucesso();
                 
                 // Se chegou ao nível máximo, conclui com sucesso
                 if (treinamento.nivel_atual == treinamento.nivel_maximo) {
                     treinamento.status = 2; // Concluído
                     treinamento.duracao = time_us_32() / 1000000 - treinamento.inicio;
                     stats.sessoes_concluidas++;
                     stats.tempo_ultimo_treino = treinamento.duracao;
                 }
             }
         }
         
         // Verifica o tempo limite (5 minutos)
         uint32_t tempo_decorrido = time_us_32() / 1000000 - treinamento.inicio;
         if (tempo_decorrido >= 300) { // 5 minutos
             // Se não atingiu o nível máximo, considera como falha
             if (treinamento.nivel_atual < treinamento.nivel_maximo) {
                 treinamento.status = 3; // Falha
             } else {
                 treinamento.status = 2; // Concluído
             }
             treinamento.duracao = tempo_decorrido;
             stats.sessoes_concluidas++;
             stats.tempo_ultimo_treino = treinamento.duracao;
             
             tocar_erro();
         }
         
         // Botão SET cancelará o treinamento
         if (gpio_get(BUTTON_SET) == 0) { // Botão pressionado
             uint32_t current_time = time_us_32() / 1000;
             if (current_time - last_button_time > DEBOUNCE_DELAY_MS) {
                 last_button_time = current_time;
                 
                 // Cancela o treinamento
                 treinamento.status = 0; // Não iniciado
                 beep();
             }
         }
     }
     // Se o treinamento está concluído ou falhou
     else if (treinamento.status == 2 || treinamento.status == 3) {
         // Botão SET para reiniciar
         if (gpio_get(BUTTON_SET) == 0) { // Botão pressionado
             uint32_t current_time = time_us_32() / 1000;
             if (current_time - last_button_time > DEBOUNCE_DELAY_MS) {
                 last_button_time = current_time;
                 
                 // Reinicia o estado do treinamento
                 treinamento.status = 0; // Não iniciado
                 beep();
             }
         }
     }
     
     // Atualiza o display
     atualizar_display_treinamento(ssd, &treinamento);
     
     // Atualiza a matriz de LEDs conforme o objetivo e estado do treinamento
     for (int i = 0; i < NUM_PIXELS; i++) {
         buffer_leds[i] = false;
     }
     
     // Estados visuais conforme o objetivo e progresso
     if (treinamento.status == 0) { // Não iniciado
         switch (treinamento.objetivo) {
             case 0: // Atenção
                 for (int linha = 0; linha < 5; linha++) {
                     for (int coluna = 0; coluna < 5; coluna++) {
                         int indice = linha * 5 + coluna;
                         buffer_leds[indice] = padrao_foco[linha][coluna];
                     }
                 }
                 set_rgb_color(0, 0, 255); // Azul
                 break;
             case 1: // Relaxamento
                 for (int linha = 0; linha < 5; linha++) {
                     for (int coluna = 0; coluna < 5; coluna++) {
                         int indice = linha * 5 + coluna;
                         buffer_leds[indice] = padrao_relaxamento[linha][coluna];
                     }
                 }
                 set_rgb_color(0, 255, 255); // Ciano
                 break;
             case 2: // Estado Flow
                 atualizar_buffer_com_ondas();
                 set_rgb_color(0, 255, 0); // Verde
                 break;
         }
     }
     else if (treinamento.status == 1) { // Em andamento
         // Exibe o nível atual visualmente
         int leds_por_nivel = NUM_PIXELS / treinamento.nivel_maximo;
         int leds_acesos = leds_por_nivel * treinamento.nivel_atual;
         if (leds_acesos > NUM_PIXELS) leds_acesos = NUM_PIXELS;
         
         for (int i = 0; i < leds_acesos; i++) {
             buffer_leds[i] = true;
         }
         
         // Cor conforme o objetivo
         switch (treinamento.objetivo) {
             case 0: // Atenção
                 set_rgb_color(0, 0, 255); // Azul
                 break;
             case 1: // Relaxamento
                 set_rgb_color(0, 255, 255); // Ciano
                 break;
             case 2: // Estado Flow
                 set_rgb_color(0, 255, 0); // Verde
                 break;
         }
     }
     else if (treinamento.status == 2) { // Concluído com sucesso
         atualizar_buffer_com_carinha(1); // Feliz
         set_rgb_color(0, 255, 0); // Verde
     }
     else if (treinamento.status == 3) { // Falha
         atualizar_buffer_com_carinha(2); // Triste
         set_rgb_color(255, 0, 0); // Vermelho
     }
     
     definir_leds(COR_WS2812_R, COR_WS2812_G, COR_WS2812_B);
 }
 
 // Modo de histórico
 void executar_modo_historico(ssd1306_t *ssd) {
     // Atualiza o display com as estatísticas
     atualizar_display_historico(ssd, &stats);
     
     // Botão SET para limpar estatísticas
     if (gpio_get(BUTTON_SET) == 0) { // Botão pressionado
         uint32_t current_time = time_us_32() / 1000;
         if (current_time - last_button_time > DEBOUNCE_DELAY_MS) {
             last_button_time = current_time;
             
             // Confirma com NEXT para evitar limpeza acidental
             if (gpio_get(BUTTON_NEXT) == 0) {
                 // Limpa as estatísticas
                 stats.soma_atencao = 0.0f;
                 stats.soma_relaxamento = 0.0f;
                 stats.max_atencao = 0.0f;
                 stats.max_relaxamento = 0.0f;
                 stats.amostras = 0;
                 stats.tempo_inicio = time_us_32() / 1000000;
                 stats.sessoes_concluidas = 0;
                 
                 tocar_sucesso();
             }
         }
     }
     
     // Mostra as estatísticas visualmente
     float media_atencao = 0.0f;
     float media_relaxamento = 0.0f;
     
     if (stats.amostras > 0) {
         media_atencao = stats.soma_atencao / stats.amostras;
         media_relaxamento = stats.soma_relaxamento / stats.amostras;
     }
     
     // Matriz limpa
     for (int i = 0; i < NUM_PIXELS; i++) {
         buffer_leds[i] = false;
     }
     
     // Primeira linha (0-4): Representa média de atenção
     int leds_atencao = (int)(media_atencao / 100.0f * 5.0f);
     for (int i = 0; i < leds_atencao && i < 5; i++) {
         buffer_leds[i] = true;
     }
     
     // Segunda linha (5-9): Representa média de relaxamento
     int leds_relaxamento = (int)(media_relaxamento / 10.0f * 5.0f);
     for (int i = 0; i < leds_relaxamento && i < 5; i++) {
         buffer_leds[i + 5] = true;
     }
     
     // Restante da matriz: Representa sessões concluídas (até 15 sessões)
     int leds_sessoes = stats.sessoes_concluidas;
     if (leds_sessoes > 15) leds_sessoes = 15;
     for (int i = 0; i < leds_sessoes; i++) {
         buffer_leds[i + 10] = true;
     }
     
     // LED RGB roxo para modo de histórico
     set_rgb_color(128, 0, 128); // Roxo
     
     definir_leds(COR_WS2812_R, COR_WS2812_G, COR_WS2812_B);
 }
 
 //===============================================
 // Callback para os botões
 //===============================================
 void button_callback(uint gpio, uint32_t events) {
     uint32_t current_time = time_us_32() / 1000;
     if (current_time - last_button_time < DEBOUNCE_DELAY_MS) return;
     last_button_time = current_time;
     
     if (gpio == BUTTON_SET && (events & GPIO_IRQ_EDGE_FALL)) {
         // Em modo de treinamento, o botão SET já tem comportamento específico
        // então não alteramos o modo global
        if (menu_index != 2) {
            if (!in_set_mode) {
                // Entra no modo de configuração do parâmetro atual
                in_set_mode = true;
                current_param = 0;
            } else {
                // Avança para o próximo parâmetro ou sai do modo de configuração
                current_param++;
                if (current_param > 3) {
                    in_set_mode = false;
                    current_param = 0;  // ADICIONE ESTA LINHA AQUI
                }
            }
            beep();
         }
     } else if (gpio == BUTTON_NEXT && (events & GPIO_IRQ_EDGE_FALL)) {
         // Se não estiver em modo de configuração, avança para o próximo menu
         if (!in_set_mode) {
             menu_index = (menu_index + 1) % 4;
             beep();
         }
     } else if (gpio == BUTTON_BACK && (events & GPIO_IRQ_EDGE_FALL)) {
         // Se não estiver em modo de configuração, retorna ao menu anterior
         if (!in_set_mode) {
             menu_index = (menu_index + 3) % 4; // +3 é equivalente a -1 em módulo 4
             beep();
         }
     }
 }
 
 //===============================================
 // Tela de boas-vindas no OLED
 //===============================================
 void splash_screen(ssd1306_t *ssd) {
     ssd1306_fill(ssd, 0);
     
     // Desenha o título
     ssd1306_draw_string(ssd, "NeuroSync", 30, 10);
     ssd1306_draw_string(ssd, "Sistema de Biofeedback", 10, 30);
     ssd1306_draw_string(ssd, "Treinamento Cognitivo", 15, 45);
     
     ssd1306_send_data(ssd);
     
     // Efeito visual na matriz de LEDs
     for (int i = 0; i < NUM_PIXELS; i++) {
         buffer_leds[i] = true;
         definir_leds(COR_WS2812_R, COR_WS2812_G, COR_WS2812_B);
         sleep_ms(50);
     }
     
     for (int i = 0; i < NUM_PIXELS; i++) {
         buffer_leds[i] = false;
         definir_leds(COR_WS2812_R, COR_WS2812_G, COR_WS2812_B);
         sleep_ms(50);
     }
     
     // Sequência sonora de inicialização
     play_tone(BUZZER1_PIN, 523, 200); // Do
     sleep_ms(50);
     play_tone(BUZZER1_PIN, 659, 200); // Mi
     sleep_ms(50);
     play_tone(BUZZER1_PIN, 784, 200); // Sol
     sleep_ms(50);
     play_tone(BUZZER1_PIN, 1047, 400); // Do (oitava acima)
     
     sleep_ms(1000);
 }
 
 //===============================================
 // Função principal
 //===============================================
 int main() {
     stdio_init_all();
     
     // Inicializa o OLED via I2C
     i2c_init(i2c1, 400 * 1000);
     gpio_set_function(SDA, GPIO_FUNC_I2C);
     gpio_set_function(SCL, GPIO_FUNC_I2C);
     gpio_pull_up(SDA);
     gpio_pull_up(SCL);
     
     // Inicializa o ADC para os potenciômetros
     adc_init();
     adc_gpio_init(POT_ATENCAO_PIN);
     adc_gpio_init(POT_RELAXAMENTO_PIN);
     
     // Inicializa os botões
     gpio_init(BUTTON_NEXT);
     gpio_set_dir(BUTTON_NEXT, GPIO_IN);
     gpio_pull_up(BUTTON_NEXT);
     
     gpio_init(BUTTON_BACK);
     gpio_set_dir(BUTTON_BACK, GPIO_IN);
     gpio_pull_up(BUTTON_BACK);
     
     gpio_init(BUTTON_SET);
     gpio_set_dir(BUTTON_SET, GPIO_IN);
     gpio_pull_up(BUTTON_SET);
     
     // Configura a interrupção para os botões
     gpio_set_irq_enabled_with_callback(BUTTON_NEXT, GPIO_IRQ_EDGE_FALL, true, button_callback);
     gpio_set_irq_enabled(BUTTON_BACK, GPIO_IRQ_EDGE_FALL, true);
     gpio_set_irq_enabled(BUTTON_SET, GPIO_IRQ_EDGE_FALL, true);
     
     // Inicializa os buzzers
     gpio_init(BUZZER1_PIN);
     gpio_set_dir(BUZZER1_PIN, GPIO_OUT);
     gpio_put(BUZZER1_PIN, 0);
     
     gpio_init(BUZZER2_PIN);
     gpio_set_dir(BUZZER2_PIN, GPIO_OUT);
     gpio_put(BUZZER2_PIN, 0);
     
     // Inicializa o LED RGB
     init_rgb_led();
     
     // Inicializa o display OLED
     ssd1306_t ssd;
     ssd1306_init(&ssd, SSD1306_WIDTH, SSD1306_HEIGHT, false, I2C_ADDR, i2c1);
     ssd1306_config(&ssd);
     ssd1306_fill(&ssd, 0);
     ssd1306_send_data(&ssd);
     
     // Inicializa a matriz WS2812 via PIO
     PIO pio = pio0;
     uint sm = 0;
     uint offset = pio_add_program(pio, &ws2812_program);
     ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);
     
     // Inicializa as estatísticas
     stats.tempo_inicio = time_us_32() / 1000000;
     
     // Mostra a tela de boas-vindas
     splash_screen(&ssd);
     
     // Loop principal
     while (true) {
         // Verifica em qual modo estamos e executa a função correspondente
         if (in_set_mode) {
             executar_modo_configuracao(&ssd);
         } else {
             switch (menu_index) {
                 case 0: // Modo de monitoramento
                     executar_modo_monitoramento(&ssd);
                     break;
                 case 1: // Modo de configuração (sem estar em set_mode, mostra apenas informações)
                     executar_modo_configuracao(&ssd);
                     break;
                 case 2: // Modo de treinamento
                     executar_modo_treinamento(&ssd);
                     break;
                 case 3: // Modo de histórico
                     executar_modo_historico(&ssd);
                     break;
             }
         }
         
         // Pequeno delay para não sobrecarregar o processador
         sleep_ms(50);
     }
     
     return 0;
 }