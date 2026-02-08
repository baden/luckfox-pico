#ifndef NETWORK_MONITOR_H
#define NETWORK_MONITOR_H

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

// Конфігурація моніторингу
typedef struct {
    char eth_gateway[32];   // IP шлюзу для eth0 (напр. 192.168.1.1)
    char wg_gateway[32];    // IP шлюзу для wg0 (напр. 10.8.7.1)
    char operator_ip[32];   // IP оператора (напр. 10.8.7.101)
    
    // IP пристроїв у мережі 10.0.x.x
    char dev1_ip[32];
    char dev2_ip[32];
    char dev3_ip[32];
} network_config_t;

// Стан мережі для відображення
typedef struct {
    // 0 = Down/Відсутній, 1 = Link Up (кабель є), 2 = Ping OK
    int eth_status; 
    int wg_status;
    
    bool operator_ping; // Чи пінгується оператор
    
    bool dev1_ping;     // Чи пінгуються пристрої
    bool dev2_ping;
    bool dev3_ping;
    
    pthread_mutex_t mutex;
} network_state_t;

// Ініціалізація та запуск потоку
int network_monitor_init(network_config_t* config);

// Очищення ресурсів
void network_monitor_cleanup(void);

// Отримання поточного стану (thread-safe)
void network_monitor_get_state(network_state_t* out_state);

#endif // NETWORK_MONITOR_H
