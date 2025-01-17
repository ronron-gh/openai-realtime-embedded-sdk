#include "main.h"
#include <esp_event.h>
#include <esp_log.h>
#include "driver/mcpwm_prelude.h"
static const char *TAG = "servo";

// Please consult the datasheet of your servo before changing the following parameters
#define SERVO_MIN_PULSEWIDTH_US 500  // Minimum pulse width in microsecond
#define SERVO_MAX_PULSEWIDTH_US 2500  // Maximum pulse width in microsecond
#define SERVO_MIN_DEGREE        -90   // Minimum angle
#define SERVO_MAX_DEGREE        90    // Maximum angle

#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000  // 1MHz, 1us per tick
#define SERVO_TIMEBASE_PERIOD        20000    // 20000 ticks, 20ms

static mcpwm_cmpr_handle_t comparator_pan = NULL;
static mcpwm_cmpr_handle_t comparator_tilt = NULL;

static int angle_pan_old = 0;
static int angle_tilt_old = 0;

static inline uint32_t angle_to_compare(int angle)
{
    return (angle - SERVO_MIN_DEGREE) * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) / (SERVO_MAX_DEGREE - SERVO_MIN_DEGREE) + SERVO_MIN_PULSEWIDTH_US;
}

static void servo_oper_init(mcpwm_cmpr_handle_t* cmpr, int32_t gpio_num, mcpwm_timer_handle_t* timer)
{
    mcpwm_oper_handle_t oper = NULL;
    mcpwm_operator_config_t operator_config = {
        .group_id = 0, // operator must be in the same group to the timer
        .intr_priority = 0,
        .flags = {false, false, false, false, false, false},
    };
    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper));

    ESP_LOGI(TAG, "Connect timer and operator");
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, *timer));

    ESP_LOGI(TAG, "Create comparator and generator from the operator");
    //mcpwm_cmpr_handle_t comparator = NULL;
    mcpwm_comparator_config_t comparator_config = {
        .intr_priority = 0,
        //.flags.update_cmp_on_tez = true,
        .flags = {true, false, false},
    };
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &comparator_config, cmpr));

    mcpwm_gen_handle_t generator = NULL;
    mcpwm_generator_config_t generator_config = {
        .gen_gpio_num = gpio_num,
    };
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &generator_config, &generator));

    // set the initial compare value, so that the servo will spin to the center position
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(*cmpr, angle_to_compare(0)));

    ESP_LOGI(TAG, "Set generator action on timer and compare event");
    // go high on counter empty
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator,
                                                              MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    // go low on compare threshold
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator,
                                                                MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, *cmpr, MCPWM_GEN_ACTION_LOW)));


}

void servo_init(void)
{
    ESP_LOGI(TAG, "Create timer and operator");
    mcpwm_timer_handle_t timer = NULL;
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = SERVO_TIMEBASE_RESOLUTION_HZ,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
        .period_ticks = SERVO_TIMEBASE_PERIOD,
        .intr_priority = 0,
        .flags = {false, false, false},
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    // Pan
    servo_oper_init(&comparator_pan, CONFIG_SERVO_PULSE_GPIO_PAN, &timer);
    // Tilt
    servo_oper_init(&comparator_tilt, CONFIG_SERVO_PULSE_GPIO_TILT, &timer);


    ESP_LOGI(TAG, "Enable and start timer");
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));

}

void servo_set_angle(int angle_pan, int angle_tilt)
{
    int step_pan, step_tilt;

    if ((angle_pan < CONFIG_SERVO_MIN_DEGREE_PAN) || (CONFIG_SERVO_MAX_DEGREE_PAN < angle_pan)) {
        angle_pan = angle_pan_old;
    }

    if ((angle_tilt < CONFIG_SERVO_MIN_DEGREE_TILT) || (CONFIG_SERVO_MAX_DEGREE_TILT < angle_tilt)) {
        angle_tilt = angle_tilt_old;
    }

    if(angle_pan - angle_pan_old > 0){
        step_pan = 1;
    }else{
        step_pan = -1;
    }

    if(angle_tilt - angle_tilt_old > 0){
        step_tilt = 1;
    }else{
        step_tilt = -1;
    }

    while(1){
        if((angle_pan == angle_pan_old) && (angle_tilt == angle_tilt_old)){
            break;
        }

        if(angle_pan != angle_pan_old){
            angle_pan_old += step_pan;
            ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator_pan, angle_to_compare(angle_pan_old)));
        }

        if(angle_tilt != angle_tilt_old){
            angle_tilt_old += step_tilt;
            ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator_tilt, angle_to_compare(angle_tilt_old)));
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }

    
}

void servo_test(void)
{

    int angle_pan = 0;
    int step_pan = 2;

    int angle_tilt = 0;
    int step_tilt = 2;
    while (1) {
        ESP_LOGI(TAG, "Angle of rotation (Pan): %d", angle_pan);
        ESP_LOGI(TAG, "Angle of rotation (Tilt): %d", angle_tilt);
        ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator_pan, angle_to_compare(angle_pan)));
        ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator_tilt, angle_to_compare(angle_tilt)));
        //Add delay, since it takes time for servo to rotate, usually 200ms/60degree rotation under 5V power supply
        vTaskDelay(pdMS_TO_TICKS(500));
        if ((angle_pan + step_pan) > 60 || (angle_pan + step_pan) < -60) {
            step_pan *= -1;
        }
        if ((angle_tilt + step_tilt) > 5 || (angle_tilt + step_tilt) < -20) {
            step_tilt *= -1;
        }
        angle_pan += step_pan;
        angle_tilt += step_tilt;
    }
}

