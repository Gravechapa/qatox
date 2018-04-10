#include "toxmodel.h"

ToxModel::ToxModel()
{
    TOX_ERR_NEW err_new;
    tox = tox_new(NULL, &err_new);
    if (err_new != TOX_ERR_NEW_OK)
    {
        throw std::runtime_error("Toxcore");
    }
    _finalize = false;

    ToxCallbackHelper::registerModel(this);
}

ToxModel::~ToxModel()
{
    _finalize = true;
    if(_tox_main_loop.joinable())
        {
            _tox_main_loop.join();
        }
    tox_kill(tox);
}

typedef struct DHT_node {
    const char *ip;
    uint16_t port;
    const char key_hex[TOX_PUBLIC_KEY_SIZE*2 + 1]; // 1 for null terminator
    unsigned char key_bin[TOX_PUBLIC_KEY_SIZE];
} DHT_node;

DHT_node nodes[] =
{
    {"178.62.250.138",             33445, "788236D34978D1D5BD822F0A5BEBD2C53C64CC31CD3149350EE27D4D9A2F9B6B", {0}},
    {"2a03:b0c0:2:d0::16:1",       33445, "788236D34978D1D5BD822F0A5BEBD2C53C64CC31CD3149350EE27D4D9A2F9B6B", {0}},
    {"163.172.136.118",            33445, "2C289F9F37C20D09DA83565588BF496FAB3764853FA38141817A72E3F18ACA0B", {0}},
    {"2001:bc8:4400:2100::1c:50f", 33445, "2C289F9F37C20D09DA83565588BF496FAB3764853FA38141817A72E3F18ACA0B", {0}},
    {"128.199.199.197",            33445, "B05C8869DBB4EDDD308F43C1A974A20A725A36EACCA123862FDE9945BF9D3E09", {0}},
    {"2400:6180:0:d0::17a:a001",   33445, "B05C8869DBB4EDDD308F43C1A974A20A725A36EACCA123862FDE9945BF9D3E09", {0}},
    {"node.tox.biribiri.org",      33445, "F404ABAA1C99A9D37D61AB54898F56793E1DEF8BD46B1038B9D822E8460FAB67", {0}},
    {"tmux.ru",                    33445, "7467AFA626D3246343170B309BA5BDC975DF3924FC9D7A5917FBFA9F5CD5CD38", {0}},
    {"t0x-node1.weba.ru",          33445, "5A59705F86B9FC0671FDF72ED9BB5E55015FF20B349985543DDD4B0656CA1C63", {0}},
    {"d4rk4.ru",                   33445, "53737F6D47FA6BD2808F378E339AF45BF86F39B64E79D6D491C53A1D522E7039", {0}},
    {"tox.uplinklabs.net ",        33445, "1A56EA3EDF5DF4C0AEABBF3C2E4E603890F87E983CAC8A0D532A335F2C6E3E1F", {0}},
    {"tox.deadteam.org",           33445, "C7D284129E83877D63591F14B3F658D77FF9BA9BA7293AEB2BDFBFE1A803AF47", {0}}
};

void ToxModel::ToxCallbackHelper::friend_message_cb_helper(Tox *tox_c, uint32_t friend_number, TOX_MESSAGE_TYPE type, const uint8_t *message,
                                     size_t length, void *user_data)
{
    qDebug() << "Mesage received";
    _toxModel->_lastReceived = friend_number;
    const char *str = reinterpret_cast<const char*>(message);
    _toxModel->_receive_message_callback(friend_number, type, str, user_data);
}

void ToxModel::ToxCallbackHelper::friend_request_cb_helper(Tox *tox_c, const uint8_t *public_key, const uint8_t *message, size_t length, void *user_data)
{
    qDebug() << "Request received";
    tox_friend_add_norequest(tox_c, public_key, NULL);
}

void ToxModel::ToxCallbackHelper::self_connection_status_cb_helper(Tox *tox_c, TOX_CONNECTION connection_status, void *user_data)
{
    qDebug() << "Connection status";
    switch (connection_status)
        {
            case TOX_CONNECTION_NONE:
                _toxModel->_self_connection_status_callback("NONE");
                break;
            case TOX_CONNECTION_TCP:
                _toxModel->_self_connection_status_callback("TCP");
                break;
            case TOX_CONNECTION_UDP:
                _toxModel->_self_connection_status_callback("UDP");
                break;
        }

}

void ToxModel::authenticate(const std::string &username) //TODO: exception handling
{
    const uint8_t* name = reinterpret_cast<const uint8_t*>(username.c_str());
    if(!tox_self_set_name(tox, name, std::strlen(username.c_str()), NULL))
    {
        throw std::runtime_error("Cannot set user name");
    }

    const char *status_message = "Hello world";
    if (!tox_self_set_status_message(tox, reinterpret_cast<const uint8_t*>(status_message), strlen(status_message), NULL))
    {
        throw std::runtime_error("Cannot set status");
    }

    for (auto i = 0; i < sizeof(nodes)/sizeof(DHT_node); i++)
    {
        sodium_hex2bin(nodes[i].key_bin, sizeof(nodes[i].key_bin),
                       nodes[i].key_hex, sizeof(nodes[i].key_hex)-1, NULL, NULL, NULL);
        for (auto j = 0; j < 10 && !tox_bootstrap(tox, nodes[i].ip, nodes[i].port, nodes[i].key_bin, NULL); j++)
        {
            if (j == 9)
            {
                qDebug() << "Failed to connect to node: " << nodes[i].ip ;
            }
        }//TODO: handle false please
    }

    uint8_t tox_id_bin[TOX_ADDRESS_SIZE];
    tox_self_get_address(tox, tox_id_bin);

    char tox_id_hex[TOX_ADDRESS_SIZE*2 + 1];
    sodium_bin2hex(tox_id_hex, sizeof(tox_id_hex), tox_id_bin, sizeof(tox_id_bin));

    for (size_t i = 0; i < sizeof(tox_id_hex)-1; i ++) {
        tox_id_hex[i] = toupper(tox_id_hex[i]);
    }

    tox_callback_friend_request(tox, ToxCallbackHelper::friend_request_cb_helper);
    tox_callback_friend_message(tox, ToxCallbackHelper::friend_message_cb_helper);
    tox_callback_self_connection_status(tox, ToxCallbackHelper::self_connection_status_cb_helper);

    _tox_main_loop = std::thread(&ToxModel::_tox_loop, this);

    _userid = std::string(tox_id_hex);
    qDebug() << _userid.c_str();
}

void ToxModel::set_receive_message_callback(std::function<void(uint32_t, TOX_MESSAGE_TYPE, std::string, void *)> callback)
{
    _receive_message_callback = callback;
}

void ToxModel::set_self_connection_status_callback(std::function<void(std::string)> callback)
{
    _self_connection_status_callback = callback;
}

void ToxModel::send_message(std::string &msg)
{
    tox_friend_send_message(tox, _lastReceived, TOX_MESSAGE_TYPE_NORMAL, reinterpret_cast<const uint8_t*>(msg.c_str()), msg.size(), NULL);
}

std::string &ToxModel::getUserId()
{
    return _userid;
}

ToxModel *ToxModel::ToxCallbackHelper::_toxModel = nullptr;

void ToxModel::ToxCallbackHelper::registerModel(ToxModel *model)
{
    _toxModel = model;
}

void ToxModel::ToxCallbackHelper::unregisterModel()
{
    _toxModel = nullptr;
}

ToxModel TOX_MODEL;

ToxModel& getToxModel()
{
    return TOX_MODEL;
}