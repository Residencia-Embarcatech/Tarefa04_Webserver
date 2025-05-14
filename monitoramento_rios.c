#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/clocks.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"
#include "inc/ssd1306.h"
#include "inc/font.h"

#include "pico/stdlib.h"         // Biblioteca da Raspberry Pi Pico para funções padrão (GPIO, temporização, etc.)
#include "hardware/adc.h"        // Biblioteca da Raspberry Pi Pico para manipulação do conversor ADC
#include "pico/cyw43_arch.h"     // Biblioteca para arquitetura Wi-Fi da Pico com CYW43  

#include "lwip/pbuf.h"           // Lightweight IP stack - manipulação de buffers de pacotes de rede
#include "lwip/tcp.h"            // Lightweight IP stack - fornece funções e estruturas para trabalhar com o protocolo TCP
#include "lwip/netif.h"          // Lightweight IP stack - fornece funções e estruturas para trabalhar com interfaces de rede (netif)

/** ====================================== DEFINIÇÕES/FUNÇÕES DO WEBSERVER ====================================== */
#define WIFI_SSID "SEU SSID"
#define WIFI_PASSWORD "SUA SENHA"

#define LED 12
#define SEND_REPORT 0
#define SEND_STATUS 1
#define BUZZER_ALERT 2
#define LED_ALERT 3

int type_request = 0;

typedef struct  {
    int ID;
    float diff;
    float curr_river_l;
    float curr_rain_i;
    float last_river_l;
    char status[200];
} WebserverValues;
/** ============================================================================================================== */

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

/** ====================================== PROTÓTITPOS DE FUNÇÕES WEBSERVER ====================================== */
// Função de callback ao aceitar conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err);

// Função de callback para processar requisições HTTP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);

// Tratamento do request do usuário
void user_request(char **request);

int wifi_init();

/** ============================================================================================================== */
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
WebserverValues w; //Guarda valores de variaveis exibidas em requests
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
    char *message;
    static char html[200];

    printf("\nID %d\n", report_id++);
    printf("Nível: %.2f\n", current_river_level);
    (current_rain_intensity > 0) ? printf("Chuva: %.2f%%\n", current_rain_intensity) :  printf("Sem chuva.\n");

    switch (status)
    {
    case ATTENTION:
        printf("Status: ATENÇÃO\n");
        snprintf(html, sizeof(html), "Status: ATENÇÃO");
        break;
    case ALERT:
        printf("Status: ALERTA\n");
        snprintf(html, sizeof(html), "Status: ALERTA");
        break;
    case DANGER:
        printf("Status: PERIGO\n");
        snprintf(html, sizeof(html), "Status: PERIGO");
        break;
    case SAFE:
        printf("Status: SEGURO\n");
        snprintf(html, sizeof(html), "Status: SEGURO");
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

    w.ID = report_id;
    w.diff = diff_p;
    w.curr_rain_i = current_rain_intensity;
    w.curr_river_l = current_river_level;
    w.last_river_l = last_river_level;
    strcpy(w.status, html);

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
    //Faz as configurações e inicializações necessárias para conexão com o Wi-Fi
    wifi_init();

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

        cyw43_arch_poll(); // Mantém o Wi-Fi ativo
        /**
         * O intervalo é definido com tempo relativamente baixo, em uma implementação
         * real do sistema, os intervalos seriam maiores, para se encaixar melhor a necessidade
         * de checagem e verificação do nível do rio
         */  
        sleep_ms(1000);
    }

    cyw43_arch_deinit(); //Desliga a arquitetura CYW43
    return 0;
}

/** ====================================== IMPLEMENTAÇÃO DE FUNÇÕES WEBSERVER ====================================== */

int wifi_init()
{
    //Inicializa a arquitetura do cyw43
    while (cyw43_arch_init())
    {
        printf("Falha ao inicializar Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }

    // Ativa o Wi-Fi no modo Station, de modo a que possam ser feitas ligações a outros pontos de acesso Wi-Fi.
    cyw43_arch_enable_sta_mode();

    // Conectar à rede WiFI - fazer um loop até que esteja conectado
    printf("Conectando ao Wi-Fi...\n");
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000))
    {
        printf("Falha ao conectar ao Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }
    printf("Conectado ao Wi-Fi\n");

    // Caso seja a interface de rede padrão - imprimir o IP do dispositivo.
    if (netif_default)
    {
        printf("IP do dispositivo: %s\n", ipaddr_ntoa(&netif_default->ip_addr));
    }

    // Configura o servidor TCP - cria novos PCBs TCP. É o primeiro passo para estabelecer uma conexão TCP.
    struct tcp_pcb *server = tcp_new();
    if (!server)
    {
        printf("Falha ao criar servidor TCP\n");
        return -1;
    }

    //vincula um PCB (Protocol Control Block) TCP a um endereço IP e porta específicos.
    if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK)
    {
        printf("Falha ao associar servidor TCP à porta 80\n");
        return -1;
    }

    // Coloca um PCB (Protocol Control Block) TCP em modo de escuta, permitindo que ele aceite conexões de entrada.
    server = tcp_listen(server);

    // Define uma função de callback para aceitar conexões TCP de entrada. É um passo importante na configuração de servidores TCP.
    tcp_accept(server, tcp_server_accept);
    printf("Servidor ouvindo na porta 80\n");

    return 0;
}

// Função de callback ao aceitar conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;
}

// Tratamento do request do usuário - digite aqui
void user_request(char **request){

    if (strstr(*request, "GET /send_report") != NULL)
    {
        type_request = SEND_REPORT;
    }
    else if (strstr(*request, "GET /update_status") != NULL)
    {
        /** @todo: Implementar alerta através de buzzer para parte 2 */
        // type_request = SEND_STATUS;
    }
    else if (strstr(*request, "GET /buzzer_alert") != NULL)
    {
        /** @todo: Implementar alerta através de buzzer para parte 2 */
        // start_buzzer_alert();
        // type_request = BUZZER_ALERT;
    }
    else if (strstr(*request, "GET /led_alert") != NULL)
    {
        /** @todo Implementar alerta através do LED RGB para parte 2 */
        // type_request = LED_ALERT;
    }
};

// Função de callback para processar requisições HTTP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (!p)
    {
        tcp_close(tpcb);
        tcp_recv(tpcb, NULL);
        return ERR_OK;
    }

    // Alocação do request na memória dinámica
    char *request = (char *)malloc(p->len + 1);
    memcpy(request, p->payload, p->len);
    request[p->len] = '\0';

    printf("Request: %s\n", request);

    // Tratamento de request - Controle dos LEDs
    user_request(&request);

    char html[2048];
        snprintf(html, sizeof(html), // Formatar uma string e armazená-la em um buffer de caracteres
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "<title> Monitoramento de Rios </title>\n"
        "<style>\n"
        "body { background-color: #b5e5fb; font-fam </button></form>\n"
        "<form action=\"./buzzer_alert\"><button>Alerta Sonoro</button></form>\n"
        "<form action=\"./led_alert\"><button>Alerta Visual</button></form>\n"
        "<p class=\"report_data\">ID: %d</p>\n"
        "<p class=\"report_data\">Nivel do Rio Anterior: %.2f</p>\n"
        "<p class=\"report_data\">Nivel Atual do Rio: %.2f</p>\n"
        "<p class=\"report_data\">Diff do nivel(%): %.2f </p>\n"
        "<p class=\"report_data\">Intensidade de Chuva: %.2f</p>\n"
        "<p class=\"report_data\">status: %s</p>\n"
        "</body>\n"
        "</html>\n",
        w.ID, w.last_river_l, w.curr_river_l, w.diff, w.curr_rain_i, w.status
    );
    
    
    // Escreve dados para envio (mas não os envia imediatamente).
    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);

    // Envia a mensagem
    tcp_output(tpcb);

    //libera memória alocada dinamicamente
    free(request);
    
    //libera um buffer de pacote (pbuf) que foi alocado anteriormente
    pbuf_free(p);

    return ERR_OK;
}
/** ============================================================================================================== */

