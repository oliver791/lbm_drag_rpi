#include "mac_info_helper.h"
#include "lorawan_api.h"
#include "lr1_stack_mac_layer.h"

int mac_info_csv_get( uint8_t stack_id, mac_info_csv_t* info )
{
    lr1_stack_mac_t* mac = lorawan_api_stack_mac_get( stack_id );
    if( mac == NULL || info == NULL )
        return -1;

    info->tx_data_rate          = mac->tx_data_rate;
    info->tx_data_rate_adr      = mac->tx_data_rate_adr;
    info->tx_power              = mac->tx_power;
    info->nb_trans              = mac->nb_trans;
    info->nb_trans_cpt          = mac->nb_trans_cpt;
    info->nb_available_tx_channel = mac->nb_available_tx_channel;
    info->tx_frequency          = mac->tx_frequency;
    info->rx1_frequency         = mac->rx1_frequency;
    info->rx2_data_rate         = mac->rx2_data_rate;
    info->rx2_frequency         = mac->rx2_frequency;
    return 0;
}
