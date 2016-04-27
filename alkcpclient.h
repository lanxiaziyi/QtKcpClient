#ifndef ALKCPCLIENT_H
#define ALKCPCLIENT_H
/*
 * 这个主要是github中asio_kcp的改写，改成Qt的架构
 * 改写的要求，尽量提供TcpSocket之类的接口
 *
 */


#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QUdpSocket>
#include <QElapsedTimer>
#include "ikcp.h"
#include "connect_packet.hpp"


#define MAX_MSG_SIZE 1024 * 10
#define KCP_UPDATE_INTERVAL 5 // milliseconds
#define KCP_RESEND_CONNECT_MSG_INTERVAL 500 // milliseconds
#define KCP_CONNECT_TIMEOUT_TIME 5000 // milliseconds

#define KCP_ERR_ALREADY_CONNECTED       -2001
#define KCP_ERR_ADDRESS_INVALID         -2002
#define KCP_ERR_CREATE_SOCKET_FAIL      -2003
#define KCP_ERR_SET_NON_BLOCK_FAIL      -2004

#define KCP_ERR_CONNECT_FUNC_FAIL       -2010
#define KCP_ERR_KCP_CONNECT_TIMEOUT     -2011

#define KCP_CONNECT_TIMEOUT_MSG "connect timeout"


enum eEventType
{
    eConnect,
    eConnectFailed,
    eDisconnect,
    eRcvMsg,

    eCountOfEventType
};

typedef void(client_event_callback_t)(kcp_conv_t /*conv*/, eEventType /*event_type*/, const std::string& /*msg*/, void* /*var*/);


class ALKcpClient : public QObject
{
    Q_OBJECT
public:
    explicit ALKcpClient(QObject *parent = 0);
    ~ALKcpClient();
    int connect_async(const QHostAddress &address = QHostAddress::Any, quint16 port = 0);

    void set_event_callback(const client_event_callback_t& event_callback_func, void* var);
    static void event_callback_func(kcp_conv_t conv, eEventType event_type, const std::string& msg, void* var);
    void handle_client_event_callback(kcp_conv_t conv, eEventType event_type, const std::string& msg);
private:
    void init_kcp(kcp_conv_t conv);

    int init_udp_connect(void);

    bool connect_timeout(quint64 cur_clock);
    bool need_send_connect_packet(uint64_t cur_clock) const;

private:

    static int udp_output(const char *buf, int len, ikcpcb *kcp, void *user);
    void send_udp_package(const char *buf,int len);
    void do_send_connect_packet(quint64 cur_clock);

    void do_recv_udp_packet_in_loop();



signals:
    void connected();
    void disconnected();


public slots:
    void update();
    void onReadUdpDatagrams();


private slots:
    void onUpdateTimer();
    void do_asio_kcp_connect(uint64_t cur_clock);


    void do_send_msg_in_queue(void);
    void do_recv_udp_packet_in_loop(void);


private:
    bool in_connect_stage_;//
    quint64 connect_start_time;
    quint64 last_send_connect_msg_time;
    bool connect_successed;


    ikcpcb *m_pKcp;
   // QElapsedTimer m_elapsedTimer;
    QTimer m_updateTimer;

    int server_udp_port;
    //QHostAddress server_host;

    int udp_socket_;

    QUdpSocket* m_pUdpSocket;

    client_event_callback_t* pevent_func_;
    void* event_callback_var_;
};

#endif // ALKCPCLIENT_H
