#ifndef MAC_INFO_HELPER_H
#define MAC_INFO_HELPER_H

#include <stdint.h>

typedef struct {
    uint8_t  tx_data_rate;
    uint8_t  tx_data_rate_adr;
    int8_t   tx_power;
    uint8_t  nb_trans;
    uint8_t  nb_trans_cpt;
    uint8_t  nb_available_tx_channel;
    uint32_t tx_frequency;
    uint32_t rx1_frequency;
    uint8_t  rx2_data_rate;
    uint32_t rx2_frequency;
} mac_info_csv_t;

/**
 * @brief Récupère les infos MAC internes pour le logging CSV
 * @param stack_id  identifiant du stack (0)
 * @param info      pointeur vers la structure à remplir
 * @return 0 si OK, -1 si erreur
 */
int mac_info_csv_get( uint8_t stack_id, mac_info_csv_t* info );

#endif
