#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/rtc.h"
#include "pico/binary_info.h"

#include "lib/ssd1306.h"
#include "lib/push_button.h"
#include "lib/led.h"
#include "lib/pwm.h"

#include "ff.h"
#include "diskio.h"
#include "f_util.h"
#include "hw_config.h"
#include "my_debug.h"
#include "rtc.h"
#include "sd_card.h"

#define I2C0_PORT i2c0                 // i2c0 pinos 0 e 1, i2c1 pinos 2 e 3
#define I2C0_SDA 0                     // 0 ou 2
#define I2C0_SCL 1                     // 1 ou 3

#define BUZZER 21
#define PWM_DIVISER 20
#define PWM_WRAP 2000 // aprox 3.5kHz freq

/** global variables */
static char *status = "Init";
static uint32_t samples = 0;
static char *action = "N/A";
static volatile bool screen = false;

static volatile bool mounted = false; // true: mounted, false: unmounted
static volatile bool enabled = false; // true: enabled, false: disabled
static volatile bool action_A = false; // true: action A, false: no action A
static volatile bool action_B = false; // true: action B, false: no action B


static volatile uint32_t last_time = 0;

ssd1306_t ssd;

/*******************************************************************************/
/***** TRECHO DO MPU 6050 *****/
static int addr = 0x68;

/**
 * @brief Reset and initialize the MPU6050 sensor
 */
static void mpu6050_reset();

/**
 * @brief Read raw data from the MPU6050 sensor
 *
 * @param accel Array to store the accelerometer data (x, y, z)
 * @param gyro Array to store the gyroscope data (x, y, z)
 * @param temp Pointer to store the temperature data
 */
static void mpu6050_read_raw(int16_t accel[3], int16_t gyro[3], int16_t *temp);
/***** FIM DO TRECHO MPU 6050 *****/
/*******************************************************************************/

/*******************************************************************************/
/***** TRECHO DO FatFS_SPI *****/
static char filename[20] = "data.csv";

static sd_card_t *sd_get_by_name(const char *const name) {
    for (size_t i = 0; i < sd_get_num(); ++i)
        if (0 == strcmp(sd_get_by_num(i)->pcName, name))
            return sd_get_by_num(i);
    DBG_PRINTF("%s: unknown name %s\n", __func__, name);
    return NULL;
}
static FATFS *sd_get_fs_by_name(const char *name) {
    for (size_t i = 0; i < sd_get_num(); ++i)
        if (0 == strcmp(sd_get_by_num(i)->pcName, name))
            return &sd_get_by_num(i)->fatfs;
    DBG_PRINTF("%s: unknown name %s\n", __func__, name);
    return NULL;
}

static void run_setrtc() {
    const char *dateStr = strtok(NULL, " ");
    if (!dateStr) {
        printf("Missing argument\n");
        return;
    }
    int date = atoi(dateStr);

    const char *monthStr = strtok(NULL, " ");
    if (!monthStr) {
        printf("Missing argument\n");
        return;
    }
    int month = atoi(monthStr);

    const char *yearStr = strtok(NULL, " ");
    if (!yearStr) {
        printf("Missing argument\n");
        return;
    }
    int year = atoi(yearStr) + 2000;

    const char *hourStr = strtok(NULL, " ");
    if (!hourStr) {
        printf("Missing argument\n");
        return;
    }
    int hour = atoi(hourStr);

    const char *minStr = strtok(NULL, " ");
    if (!minStr) {
        printf("Missing argument\n");
        return;
    }
    int min = atoi(minStr);

    const char *secStr = strtok(NULL, " ");
    if (!secStr) {
        printf("Missing argument\n");
        return;
    }
    int sec = atoi(secStr);

    datetime_t t = {
        .year = (int16_t)year,
        .month = (int8_t)month,
        .day = (int8_t)date,
        .dotw = 0, // 0 is Sunday
        .hour = (int8_t)hour,
        .min = (int8_t)min,
        .sec = (int8_t)sec};
    rtc_set_datetime(&t);
}

static void run_format() {
    const char *arg1 = strtok(NULL, " ");
    if (!arg1)
        arg1 = sd_get_by_num(0)->pcName;
    FATFS *p_fs = sd_get_fs_by_name(arg1);
    if (!p_fs) {
        printf("Unknown logical drive number: \"%s\"\n", arg1);
        return;
    }
    /* Format the drive with default parameters */
    FRESULT fr = f_mkfs(arg1, 0, 0, FF_MAX_SS * 2);
    if (FR_OK != fr)
        printf("f_mkfs error: %s (%d)\n", FRESULT_str(fr), fr);
}
static void run_mount() {
    const char *arg1 = strtok(NULL, " ");
    if (!arg1)
        arg1 = sd_get_by_num(0)->pcName;
    FATFS *p_fs = sd_get_fs_by_name(arg1);
    if (!p_fs) {
        printf("Unknown logical drive number: \"%s\"\n", arg1);
        return;
    }
    FRESULT fr = f_mount(p_fs, arg1, 1);
    if (FR_OK != fr) {
        printf("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
        return;
    }
    sd_card_t *pSD = sd_get_by_name(arg1);
    myASSERT(pSD);
    pSD->mounted = true;
    printf("Processo de montagem do SD ( %s ) concluído\n", pSD->pcName);
}
static void run_unmount() {
    const char *arg1 = strtok(NULL, " ");
    if (!arg1)
        arg1 = sd_get_by_num(0)->pcName;
    FATFS *p_fs = sd_get_fs_by_name(arg1);
    if (!p_fs) {
        printf("Unknown logical drive number: \"%s\"\n", arg1);
        return;
    }
    FRESULT fr = f_unmount(arg1);
    if (FR_OK != fr) {
        printf("f_unmount error: %s (%d)\n", FRESULT_str(fr), fr);
        return;
    }
    sd_card_t *pSD = sd_get_by_name(arg1);
    myASSERT(pSD);
    pSD->mounted = false;
    pSD->m_Status |= STA_NOINIT; // in case medium is removed
    printf("SD ( %s ) desmontado\n", pSD->pcName);
}
static void run_getfree() {
    const char *arg1 = strtok(NULL, " ");
    if (!arg1)
        arg1 = sd_get_by_num(0)->pcName;
    DWORD fre_clust, fre_sect, tot_sect;
    FATFS *p_fs = sd_get_fs_by_name(arg1);
    if (!p_fs) {
        printf("Unknown logical drive number: \"%s\"\n", arg1);
        return;
    }
    FRESULT fr = f_getfree(arg1, &fre_clust, &p_fs);
    if (FR_OK != fr) {
        printf("f_getfree error: %s (%d)\n", FRESULT_str(fr), fr);
        return;
    }
    tot_sect = (p_fs->n_fatent - 2) * p_fs->csize;
    fre_sect = fre_clust * p_fs->csize;
    printf("%10lu KiB total drive space.\n%10lu KiB available.\n", tot_sect / 2, fre_sect / 2);
}
static void run_ls() {
    const char *arg1 = strtok(NULL, " ");
    if (!arg1)
        arg1 = "";
    char cwdbuf[FF_LFN_BUF] = {0};
    FRESULT fr;
    char const *p_dir;
    if (arg1[0]) {
        p_dir = arg1;
    } else {
        fr = f_getcwd(cwdbuf, sizeof cwdbuf);
        if (FR_OK != fr) {
            printf("f_getcwd error: %s (%d)\n", FRESULT_str(fr), fr);
            return;
        }
        p_dir = cwdbuf;
    }
    printf("Directory Listing: %s\n", p_dir);
    DIR dj;
    FILINFO fno;
    memset(&dj, 0, sizeof dj);
    memset(&fno, 0, sizeof fno);
    fr = f_findfirst(&dj, &fno, p_dir, "*");
    if (FR_OK != fr) {
        printf("f_findfirst error: %s (%d)\n", FRESULT_str(fr), fr);
        return;
    }
    while (fr == FR_OK && fno.fname[0]) {
        const char *pcWritableFile = "writable file",
                   *pcReadOnlyFile = "read only file",
                   *pcDirectory = "directory";
        const char *pcAttrib;
        if (fno.fattrib & AM_DIR)
            pcAttrib = pcDirectory;
        else if (fno.fattrib & AM_RDO)
            pcAttrib = pcReadOnlyFile;
        else
            pcAttrib = pcWritableFile;
        printf("%s [%s] [size=%llu]\n", fno.fname, pcAttrib, fno.fsize);

        fr = f_findnext(&dj, &fno);
    }
    f_closedir(&dj);
}
static void run_cat()
{
    char *arg1 = strtok(NULL, " ");
    if (!arg1) {
        printf("Missing argument\n");
        return;
    }
    FIL fil;
    FRESULT fr = f_open(&fil, arg1, FA_READ);
    if (FR_OK != fr) {
        printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
        return;
    }
    char buf[256];
    while (f_gets(buf, sizeof buf, &fil)) {
        printf("%s", buf);
    }
    fr = f_close(&fil);
    if (FR_OK != fr)
        printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
}

// Função para capturar dados do ADC e salvar no arquivo *.txt
void capture_data_and_save() {
    int16_t aceleracao[3], gyro[3], temp;

    printf("Capturando dados do ADC. Aguarde finalização...\n");
    FIL file;
    FRESULT res = f_open(&file, filename, FA_WRITE | FA_OPEN_APPEND);
    if (res != FR_OK) {
        printf("\n[ERRO] Não foi possível abrir o arquivo para escrita. Monte o Cartao.\n");
        return;
    }

    // Leitura dos dados de aceleração, giroscópio e temperatura
    mpu6050_read_raw(aceleracao, gyro, &temp);

    // --- Cálculos para Giroscópio ---
    float gx = (float)gyro[0];
    float gy = (float)gyro[1];
    float gz = (float)gyro[2];

    // Conversão para unidade de 'g'
    float ax = aceleracao[0] / 16384.0f;
    float ay = aceleracao[1] / 16384.0f;
    float az = aceleracao[2] / 16384.0f;

    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%f;%f;%f;%f;%f;%f\n", ax, ay, az, gx, gy, gz);
    UINT bw;
    res = f_write(&file, buffer, strlen(buffer), &bw);
    if (res != FR_OK) {
        printf("[ERRO] Não foi possível escrever no arquivo. Monte o Cartao.\n");
        f_close(&file);
        return;
    }
    samples++;
    f_close(&file);
    printf("Dados do ADC salvos no arquivo %s.\n\n", filename);
}

// Função para ler o conteúdo de um arquivo e exibir no terminal
void read_file(const char *filename) {
    FIL file;
    FRESULT res = f_open(&file, filename, FA_READ);
    if (res != FR_OK) {
        printf("[ERRO] Não foi possível abrir o arquivo para leitura. Verifique se o Cartão está montado ou se o arquivo existe.\n");
        return;
    }
    char buffer[128];
    UINT br;
    printf("Conteúdo do arquivo %s:\n", filename);
    while (f_read(&file, buffer, sizeof(buffer) - 1, &br) == FR_OK && br > 0) {
        buffer[br] = '\0';
        printf("%s", buffer);
    }
    f_close(&file);
    printf("\nLeitura do arquivo %s concluída.\n\n", filename);
}

static void run_help() {
    printf("\nComandos disponíveis:\n\n");
    printf("Digite 'a' para montar o cartão SD\n");
    printf("Digite 'b' para desmontar o cartão SD\n");
    printf("Digite 'c' para listar arquivos\n");
    printf("Digite 'd' para mostrar conteúdo do arquivo\n");
    printf("Digite 'e' para obter espaço livre no cartão SD\n");
    printf("Digite 'f' para capturar dados do ADC e salvar no arquivo\n");
    printf("Digite 'g' para formatar o cartão SD\n");
    printf("Digite 'h' para exibir os comandos disponíveis\n");
    printf("\nEscolha o comando:  ");
}

typedef void (*p_fn_t)();
typedef struct {
    char const *const command;
    p_fn_t const function;
    char const *const help;
} cmd_def_t;

static cmd_def_t cmds[] = {
    {"setrtc", run_setrtc, "setrtc <DD> <MM> <YY> <hh> <mm> <ss>: Set Real Time Clock"},
    {"format", run_format, "format [<drive#:>]: Formata o cartão SD"},
    {"mount", run_mount, "mount [<drive#:>]: Monta o cartão SD"},
    {"unmount", run_unmount, "unmount <drive#:>: Desmonta o cartão SD"},
    {"getfree", run_getfree, "getfree [<drive#:>]: Espaço livre"},
    {"ls", run_ls, "ls: Lista arquivos"},
    {"cat", run_cat, "cat <filename>: Mostra conteúdo do arquivo"},
    {"help", run_help, "help: Mostra comandos disponíveis"}};

static void process_stdio(int cRxedChar) {
    static char cmd[256];
    static size_t ix;

    if (!isprint(cRxedChar) && !isspace(cRxedChar) && '\r' != cRxedChar &&
        '\b' != cRxedChar && cRxedChar != (char)127)
        return;
    printf("%c", cRxedChar); // echo
    stdio_flush();
    if (cRxedChar == '\r') {
        printf("%c", '\n');
        stdio_flush();

        if (!strnlen(cmd, sizeof cmd)) {
            printf("> ");
            stdio_flush();
            return;
        }
        char *cmdn = strtok(cmd, " ");
        if (cmdn) {
            size_t i;
            for (i = 0; i < count_of(cmds); ++i) {
                if (0 == strcmp(cmds[i].command, cmdn)) {
                    (*cmds[i].function)();
                    break;
                }
            }
            if (count_of(cmds) == i)
                printf("Command \"%s\" not found\n", cmdn);
        }
        ix = 0;
        memset(cmd, 0, sizeof cmd);
        printf("\n> ");
        stdio_flush();
    } else {
        if (cRxedChar == '\b' || cRxedChar == (char)127) {
            if (ix > 0) {
                ix--;
                cmd[ix] = '\0';
            }
        } else {
            if (ix < sizeof cmd - 1) {
                cmd[ix] = cRxedChar;
                ix++;
            }
        }
    }
}
/***** FIM DO TRECHO FatFS_SPI *****/
/*******************************************************************************/

/**
 * @brief Initialize the SSD1306 display
 *
 */
void init_display(void);

/**
 * @brief Initialize the all GPIOs that will be used in project
 *      - I2C;
 *      - Blue LED;
 */
void init_gpio(void);

/**
 * @brief Update the display informations
 *
 * @param message the message that will be ploted in display OLED
 * @param x position on horizontal that the message will be ploted
 *      if x < 0, the message will be centered
 *      if x >= 0, the message will be ploted in x position
 * @param y position on vertical that the message will be ploted
 * @param clear if the display must be cleaned
 *
 */
void _update_display(char *message, int16_t x, uint8_t y, bool clear, bool update);

/**
 * @brief Update the display with the current status
 */
void update_display(void);

/**
 * @brief Handler function to interruption
 *
 * @param gpio GPIO that triggered the interruption
 * @param event The event which caused the interruption
 */
void gpio_irq_handler(uint gpio, uint32_t event) {
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    if (current_time - last_time >= 200) { // debounce
        if (gpio == PIN_BUTTON_A) {
            enabled = !enabled; // toggle action enable/disable
        } else if (gpio == PIN_BUTTON_B) {
            action_B = true; // toggle action mount/unmount
        } else if (gpio == PIN_BUTTON_J) {
            screen = !screen; // toggle screen state
        }
        last_time = current_time;
        printf("GPIO %d triggered an interrupt with event %d\n", gpio, event);
    }
}

int main() {
    //Inicializa todos os tipos de bibliotecas stdio padrão presentes que estão ligados ao binário.
    stdio_init_all();
    init_gpio();

    // get ws and ssd struct
    init_display();
    _update_display("DATALOGGER", -1, 28, true, false);
    _update_display("Iniciando...", -1, 38, false, true);
    sleep_ms(5000);
    
    // Inicializa o I2C0
    i2c_init(I2C0_PORT, 400 * 1000);
    gpio_set_function(I2C0_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C0_SDA);
    gpio_pull_up(I2C0_SCL);
    
    // Declara os pinos como I2C na Binary Info
    bi_decl(bi_2pins_with_func(I2C_SDA, I2C_SCL, GPIO_FUNC_I2C));
    mpu6050_reset();
    
    // configure interruptions handlers
    gpio_set_irq_enabled_with_callback(PIN_BUTTON_A, GPIO_IRQ_EDGE_FALL, 1, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(PIN_BUTTON_B, GPIO_IRQ_EDGE_FALL, 1, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(PIN_BUTTON_J, GPIO_IRQ_EDGE_FALL, 1, &gpio_irq_handler);

    while (true) {
        status = mounted ? "Mounted" : "Unmounted";
        if (action_B && !enabled) {
            action = mounted ? "Unmounting" : "Mounting";
            update_display();
            gpio_put(PIN_RED_LED, 1);
            gpio_put(PIN_GREEN_LED, 1);
            gpio_put(PIN_BLUE_LED, 0);
            sleep_ms(500);
            if (mounted) {
                printf("Desmontando o SD. Aguarde...\n");
                run_unmount();
                mounted = false;
            } else {
                printf("Montando o SD...\n");
                run_mount();
                mounted = true;
            }
            action_B = false;
        } else if (action_B && enabled) {
            printf("Please disable the data capture before mounting/unmounting.\n");
            _update_display("Disable capture", -1, 28, true, false);
            _update_display("to (un)mount", -1, 38, false, true);
            action_B = false;
            gpio_put(PIN_RED_LED, 1);
            gpio_put(PIN_GREEN_LED, 0);
            gpio_put(PIN_BLUE_LED, 0);
            pwm_set_gpio_level(BUZZER, PWM_WRAP / 4); // buzzer on
            sleep_ms(1000);
            pwm_set_gpio_level(BUZZER, 0); // buzzer off
            gpio_put(PIN_RED_LED, 0);
            gpio_put(PIN_GREEN_LED, 1);
            gpio_put(PIN_BLUE_LED, 0);
        }
        
        if (enabled && mounted) {
            gpio_put(PIN_RED_LED, 0);
            gpio_put(PIN_GREEN_LED, 0);
            gpio_put(PIN_BLUE_LED, 1);
            action = "Capturing";
            update_display();
            capture_data_and_save();
            gpio_put(PIN_RED_LED, 0);
            gpio_put(PIN_GREEN_LED, 0);
            gpio_put(PIN_BLUE_LED, 0);
        } else if (enabled && !mounted) {
            printf("SD not mounted. Please mount the SD card first.\n");
            _update_display("SD not mounted", -1, 28, true, true);
            enabled = false;
            gpio_put(PIN_RED_LED, 1);
            gpio_put(PIN_GREEN_LED, 0);
            gpio_put(PIN_BLUE_LED, 0);
            pwm_set_gpio_level(BUZZER, PWM_WRAP / 4); // buzzer on
            sleep_ms(1000);
            pwm_set_gpio_level(BUZZER, 0); // buzzer off
            gpio_put(PIN_RED_LED, 0);
            gpio_put(PIN_GREEN_LED, 1);
            gpio_put(PIN_BLUE_LED, 0);
        }else {
            action = "N/A";
            update_display();
            gpio_put(PIN_RED_LED, 0);
            gpio_put(PIN_GREEN_LED, 1);
            gpio_put(PIN_BLUE_LED, 0);
        }

        sleep_ms(500); // Aguarda 500ms antes de ler novamente
    }
}

// -------------------------------------- Functions --------------------------------- //

void init_display() {
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, I2C_ADDRESS, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);
}

void init_gpio() {
    /** initialize i2c communication */
    int baudrate = 400 * 1000; // 400kHz baud rate for i2c communication
    i2c_init(I2C_PORT, baudrate);

    // set GPIO pin function to I2C
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SCL);

    /** initialize RED, GREEN and BLUE LED */
    gpio_init(PIN_RED_LED);
    gpio_set_dir(PIN_RED_LED, GPIO_OUT);
    gpio_init(PIN_GREEN_LED);
    gpio_set_dir(PIN_GREEN_LED, GPIO_OUT);
    gpio_init(PIN_BLUE_LED);
    gpio_set_dir(PIN_BLUE_LED, GPIO_OUT);

    /** initialize buttons */
    init_push_button(PIN_BUTTON_A);
    init_push_button(PIN_BUTTON_B);
    init_push_button(PIN_BUTTON_J);

    /** initialize buzzer */
    pwm_init_(BUZZER);
    pwm_setup(BUZZER, PWM_DIVISER, PWM_WRAP);
    pwm_start(BUZZER, 0);
}

void update_display() {
    _update_display(NULL, -1, -1, true, true);
}

void _update_display(char *message, int16_t x, uint8_t y, bool clear, bool update) {
    if (clear)
        ssd1306_fill(&ssd, false);
    if (message == NULL) {
        _update_display("DATALOGGER MPU", -1, 4, true, false);
        if (screen) {
            _update_display("INFO", -1, 14, false, false);

            _update_display("J -> main/info", -1, 32, false, false);
            _update_display("A -> (dis)able", -1, 42, false, false);
            _update_display("B -> (un)mount", -1, 52, false, update);
        } else {
            char buffer[10];
            snprintf(buffer, sizeof(buffer), "%lu", samples);
            
            _update_display("press J->info", -1, 14, false, false);
            
            _update_display("STS: ", 4, 32, false, false);
            _update_display(status, 44, 32, false, false);
            
            _update_display("COUNT: ", 4, 42, false, false);
            _update_display(buffer, 60, 42, false, false);
    
            _update_display("ACT: ", 4, 52, false, false);
            _update_display(action, 44, 52, false, update);
        }
    } else {
        ssd1306_rect(&ssd, 0, 0, 128, 64, true, false);
        uint8_t x_pos;
        if (x < 0)
            x_pos = 64 - (strlen(message) * 4);
        else
            x_pos = x;
        ssd1306_draw_string(&ssd, message, x_pos, y);
        if (update)
            ssd1306_send_data(&ssd); // update display
    }
}

static void mpu6050_read_raw(int16_t accel[3], int16_t gyro[3], int16_t *temp) {
    uint8_t buffer[6];

    // Lê aceleração a partir do registrador 0x3B (6 bytes)
    uint8_t val = 0x3B;
    i2c_write_blocking(I2C_PORT, addr, &val, 1, true);
    i2c_read_blocking(I2C_PORT, addr, buffer, 6, false);

    for (int i = 0; i < 3; i++)
    {
        accel[i] = (buffer[i * 2] << 8) | buffer[(i * 2) + 1];
    }

    // Lê giroscópio a partir do registrador 0x43 (6 bytes)
    val = 0x43;
    i2c_write_blocking(I2C_PORT, addr, &val, 1, true);
    i2c_read_blocking(I2C_PORT, addr, buffer, 6, false);

    for (int i = 0; i < 3; i++)
    {
        gyro[i] = (buffer[i * 2] << 8) | buffer[(i * 2) + 1];
    }

    // Lê temperatura a partir do registrador 0x41 (2 bytes)
    val = 0x41;
    i2c_write_blocking(I2C_PORT, addr, &val, 1, true);
    i2c_read_blocking(I2C_PORT, addr, buffer, 2, false);

    *temp = (buffer[0] << 8) | buffer[1];
}

static void mpu6050_reset() {
    // Dois bytes para reset: primeiro o registrador, segundo o dado
    uint8_t buf[] = {0x6B, 0x80};
    i2c_write_blocking(I2C_PORT, addr, buf, 2, false);
    sleep_ms(100); // Aguarda reset e estabilização

    // Sai do modo sleep (registrador 0x6B, valor 0x00)
    buf[1] = 0x00;
    i2c_write_blocking(I2C_PORT, addr, buf, 2, false);
    sleep_ms(10); // Aguarda estabilização após acordar
}