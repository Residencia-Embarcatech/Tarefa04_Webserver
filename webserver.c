#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/clocks.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"
#include "inc/ssd1306.h"
#include "inc/font.h"

/** @brief Pino ligado ao joystick eixo X - Simula o sensor de chuva YL-83
 **/
#define YL_83 26 

/** @brief Pino ligado ao joystick eixo Y - Simula o sensor ultrassônico HC-SR04 
 **/
#define HC_SR04 27

/** 
 * @brief Pino ligado ao Botão A - Simula o recebimento de solicitações do usuário
 **/
#define BUTTON_A 5

/**
 * @brief Definições para I2C (display)
 */
#define I2C_PORT i2c1
#define I2C_SDA 14  
#define I2C_SCL 15 
#define address 0x3C 

/**
 * @brief Definições para uart
 */
#define UART_ID uart0
#define BAUD_RATE 115200

/**
 * @brief Intervalo de tempo entre os envios de relatórios
 */
#define REPORT_TIME 10000 

/**
 * @brief Tempo (ms) para tratamento de bouncing do botão 
 */
#define DEBOUNCE_TIME_MS 500 

//Variáveis Globais
ssd1306_t ssd;
uint32_t adc_x_value;
uint32_t adc_y_value;
uint32_t current_time;
uint32_t last_debounce_time = 0;
float river_level = 5.0;
float intense_rain = 100.0;
float current_river_level;
float last_river_level;
float current_rain_intensity;
int status;
enum level_status {ATTENTION, ALERT, DANGER, SAFE};
/**
 * @brief Procedimento para configurar e inicializar o Joystick
 */
void init_joystick()
{
    adc_init(); //Inicializa o ADC
    adc_gpio_init(YL_83);
    adc_gpio_init(HC_SR04);
}

/**
 * @brief Procedimento para configurar o I2C e Display ssd1306
 */
void init_i2c_display()
{
    //Configuração do I2C
    i2c_init(I2C_PORT, 400*1000);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    //Configuração do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, address, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd); // Configura o display
    ssd1306_send_data(&ssd); // Envia os dados para o display
    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
}

/**
 * @brief Procedimento para inicializar o botão A
 */
void init_button()
{
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);
}

/**
 * @brief Reúne informações de relatório e faz o envio para o usuário
 */
void send_report()
{
    static volatile uint report_id = 1;
    float diff_p = 0.0;

    printf("\nID %d\n", report_id++);
    printf("Nível: %.2f\n", current_river_level);
    (current_rain_intensity > 0) ? printf("Chuva: %.2f%%\n", current_rain_intensity) :  printf("Sem chuva.\n");

    switch (status)
    {
    case ATTENTION:
        printf("Status: ATENÇÃO\n");
        break;
    case ALERT:
        printf("Status: ALERTA\n");
        break;
    case DANGER:
        printf("Status: PERIGO\n");
        break;
    case SAFE:
        printf("Status: SEGURO\n");
        break;
    default:
        printf("Status: Erro\n");
        break;
    }

    if (last_river_level)
    {
        float diff = current_river_level - last_river_level;
        diff_p = (diff / last_river_level) * 100.0;
        printf("Dif:%.2f%%", diff);
    }

    last_river_level = current_river_level;
}

/**
 * @brief Gera um relatório e envia as informações para o usuário (quando solicitada)
 * Função de Callback para tratamento de interrupção acionada pelo Botão A
 */
static void gpio_irq_handler(uint gpio, uint32_t events)
{
    current_time = to_ms_since_boot(get_absolute_time());

    //Tratamento de bouncing
    if ((current_time - last_debounce_time) > DEBOUNCE_TIME_MS)
    {
        last_debounce_time = current_time;
        send_report();
    }
}

/**
 * @brief Função callback do alarme que faz o envio periódico do relatório
 */
bool repeating_timer_callback(struct repeating_timer *t)
{
    send_report();
    return true;
}

/**
 * @brief Envia uma notificação para o usuário (Exibição no display)
 */
void send_notification(char *notification)
{
    char level[20];
    sprintf(level, "%.2f", current_river_level);

    ssd1306_fill(&ssd, false); // Limpa o display
    ssd1306_rect(&ssd, 3, 3, 122, 58, true, false); // Desenha um retângulo
    ssd1306_draw_string(&ssd, notification, 40, 20); // Desenha uma string
    ssd1306_draw_string(&ssd, "NIVEL: ", 10, 40);
    ssd1306_draw_string(&ssd, level, 60, 40);
    ssd1306_send_data(&ssd); // Atualiza o display
}

/**
 * @brief Faz a leitura dos sensores (Joystick eixo X e Y) e calcula o nivel do rio e intensidade da chuva 
 */
void verify_river_level()
{
    adc_select_input(0);
    adc_x_value = adc_read();

    adc_select_input(1);
    adc_y_value = adc_read();

    //Calcula o nível do rio com base nos valores fornecidos pelo adc do eixo Y
    if (adc_y_value > 2100){
        //Indica que o nível do rio subiu; calcula o valor atual (pode aumentar até 10.0 metros)
        current_river_level = river_level + (river_level * (adc_y_value - 2048) / 2047);
    }else if (adc_y_value < 1800){
        //Indica que o nível do rio desceu; calcula o valor atual (pode diminuir até 0 metros)
        current_river_level = river_level - (river_level * (2048 - adc_y_value) / 2047);
    }else {
        current_river_level = river_level;
    }

    //Calcula a intensidade da chuva com base nos valores do eixo X
    current_rain_intensity = (intense_rain * adc_x_value) / 4095.0;
}

/**
 * @brief Mapeia o status que deve ser enviado com base no nível do rio e na intensidade da chuva
 * 
 * Cada mensagem enviada indica um nível diferente de risco, como indicado abaixo:
 * 'ATENÇÃO': Quando o nível do rio está um pouco acima do normal ou há chuva forte (pouco risco de enchente) 
 * 'ALERTA': quando existe risco de enchente
 * 'PERIGO': risco de inundação
 * 'SEGURO': Quando o nível do rio está normal ou abaixo do normal, sem risco de enchente/inundação
 */
void set_river_status()
{
  
    char *message;

    if (current_river_level >= 9.0 || (current_river_level >= 7.0 && current_rain_intensity > 50.0))
    {
        status = DANGER;
        message = "PERIGO";
    }else if ((current_river_level >= 7.0 && current_rain_intensity > 50.0) || (current_river_level > river_level && current_rain_intensity > 50.0)){
        status = ALERT;
        message = "ALERTA";
    }else if ((current_river_level > river_level && current_rain_intensity <= 50.0) || (current_river_level <= river_level && current_rain_intensity > 70.0)){
        status = ATTENTION;
        message = "ATENCAO";
    }else {
        status = SAFE;
        message = "SEGURO";
    }

    send_notification(message);
}

int main()
{
    //Realiza as inicializações e configurações dos dispositivos
    stdio_init_all();
    init_joystick();
    init_button();
    init_i2c_display();
    uart_init(UART_ID, BAUD_RATE);

    /**
     * @brief Função de interrupção para tratamento de ação ao acionar o Botão A 
     * @see gpio_irq_handler()
     */
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    /**
     * @brief Aciona uma função de callback de maneira periódica para enviar relatórios
     * @see repeating_timer_callback()
     */
    repeating_timer_t timer;
    add_repeating_timer_ms(REPORT_TIME, repeating_timer_callback, NULL, &timer);

    /**
     * Faz a primeira leitura do nível do rio quando o sistema é iniciado
     * para garantir que as informações do relatório estejam corretas */
    verify_river_level();
    last_river_level = current_river_level;

    while (true) {
        verify_river_level();
        set_river_status();
        /**
         * O intervalo é definido com tempo relativamente baixo, em uma implementação
         * real do sistema, os intervalos seriam maiores, para se encaixar melhor a necessidade
         * de checagem e verificação do nível do rio
         */  
        sleep_ms(1000);
    }
}