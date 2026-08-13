#include "uart.h"
#include "timer.h"
#include "task.h"
#include "scheduler.h"
#include "mutex.h"
#include "kalman.h"

tcb_t tcb_low, tcb_medium, tcb_high, tcb_sensor, tcb_fusion;
uint32_t low_stack[1024];
uint32_t medium_stack[1024];
uint32_t high_stack[1024];
uint32_t sensor_stack[1024];
uint32_t fusion_stack[1024];

mutex_t shared_mutex;


volatile float sensor_raw_value = 0.0f;
volatile uint8_t sensor_fault = 0;



void low_priority_task(void) {
    uart_puts("[LOW] Started\n");
    while (1) {
        mutex_lock(&shared_mutex);
        uart_puts("[LOW] Got mutex, doing work...\n");
        for (volatile int i = 0; i < 300000; i++);
        uart_puts("[LOW] Releasing mutex\n");
        mutex_unlock(&shared_mutex);

        task_sleep(5);
    }
}

void medium_priority_task(void) {
    uart_puts("[MEDIUM] Started\n");
    while (1) {
        uart_puts("[MEDIUM] Running (no relation with mutex)...\n");
        for (volatile int i = 0; i < 200000; i++);
        task_sleep(3);
    }
}

void high_priority_task(void) {
    uart_puts("[HIGH] Started\n");
    task_sleep(2);

    while (1) {
        uart_puts("[HIGH] Trying to get mutex...\n");
        mutex_lock(&shared_mutex);
        uart_puts("[HIGH] Got mutex! (priority inheritance worked)\n");
        for (volatile int i = 0; i < 100000; i++);
        mutex_unlock(&shared_mutex);

        task_sleep(8);
    }
}



static uint32_t prng_seed = 12345;

static int32_t simple_rand(void) {
    prng_seed = prng_seed * 1103515245 + 12345;
    return (int32_t)((prng_seed >> 16) % 21) - 10;   // range: -10..+10
}

void sensor_task(void) {
    uart_puts("[SENSOR] Started\n");
    uint32_t cycle = 0;

    while (1) {
        cycle++;

        
        if (cycle >= 60) {
            cycle = 0;
        }

        int32_t noise = simple_rand();
        float base = 100.0f;
        float reading = base + (float)noise * 0.1f;

        if (cycle >= 40 && cycle < 60) {
            sensor_fault = 1;
        } else {
            sensor_fault = 0;
            sensor_raw_value = reading;
        }

        task_sleep(2);
    }
}

void fusion_task(void) {
    kalman_t kf;
    kalman_init(&kf, 100.0f, 0.5f, 4.0f);

    uart_puts("[FUSION] Started\n");

    uint8_t was_faulted = 0;

    while (1) {
        if (sensor_fault) {
            if (!was_faulted) {
                uart_puts("[FUSION] WARNING: sensor fault! Holding last good estimate: ");
                uart_print_float(kf.x, 2);
                uart_puts("\n");
                was_faulted = 1;
            }
        } else {
            if (was_faulted) {
                uart_puts("[FUSION] Sensor recovered, resuming fusion.\n");
                was_faulted = 0;
            }

            float fused = kalman_update(&kf, sensor_raw_value);

            uart_puts("[FUSION] Raw=");
            uart_print_float(sensor_raw_value, 2);
            uart_puts(" Fused=");
            uart_print_float(fused, 2);
            uart_puts("\n");
        }

        task_sleep(4);
    }
}


void kernel_main(void) {
    uart_init();
    uart_puts("Nova-RTOS: Sensor Fusion + Priority Inheritance Demo\n");

    timer_init(10);
    scheduler_init();
    mutex_init(&shared_mutex);

    task_create(&tcb_low, low_priority_task, low_stack, 1, "LOW");
    task_create(&tcb_medium, medium_priority_task, medium_stack, 2, "MEDIUM");
    task_create(&tcb_high, high_priority_task, high_stack, 3, "HIGH");
    task_create(&tcb_sensor, sensor_task, sensor_stack, 2, "SENSOR");
    task_create(&tcb_fusion, fusion_task, fusion_stack, 3, "FUSION");

    scheduler_add_task(&tcb_low);
    scheduler_add_task(&tcb_medium);
    scheduler_add_task(&tcb_high);
    scheduler_add_task(&tcb_sensor);
    scheduler_add_task(&tcb_fusion);

    scheduler_start();

    while (1);
}