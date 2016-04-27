#include "alkcpclient.h"

ALKcpClient::ALKcpClient(QObject *parent) : QObject(parent)
{

    m_pKcp = NULL;

    m_elapsedTimer.start();

    m_updateTimer.setSingleShot(true);
    connect(&m_updateTimer,&QTimer::timeout,this,&ALKcpClient::onUpdateTimer);
    m_updateTimer.start(100);

    this->set_event_callback(event_callback_func,this);

}

ALKcpClient::~ALKcpClient()
{

}

int ALKcpClient::connect_async(const QHostAddress &address, quint16 port)
{
    if (udp_socket_ != -1)
          return KCP_ERR_ALREADY_CONNECTED;

    server_host = address;
    server_udp_port = port;

    //初始化 init udp connect
    int ret = init_udp_connect();
    if(ret < 0)
    {
        return ret;
    }

    in_connect_stage_ = true;
    connect_start_time = QDateTime::currentMSecsSinceEpoch();


    return 0;
}

void ALKcpClient::set_event_callback(const client_event_callback_t &event_callback_func, void *var)
{
    pevent_func_ = &event_callback_func;
    event_callback_var_ = var;
}

void ALKcpClient::event_callback_func(kcp_conv_t conv, eEventType event_type, const std::string &msg, void *var)
{
    ((ALKcpClient*)var)->handle_client_event_callback(conv, event_type, msg);
}
void ALKcpClient::handle_client_event_callback(kcp_conv_t conv, eEventType event_type, const std::string &msg)
{
    switch (event_type)
    {
        case eConnect:
            connect_result_ = 0;
            if (pconnect_event_func_)
                (*pconnect_event_func_)(conv, event_type, msg, event_func_var_);
            break;
        case eConnectFailed:
            // if msg == KCP_CONNECT_TIMEOUT_MSG
            connect_result_ = KCP_ERR_KCP_CONNECT_TIMEOUT;
            if (pconnect_event_func_)
                (*pconnect_event_func_)(conv, event_type, msg, event_func_var_);
            break;
        case eRcvMsg:
        case eDisconnect:
            if (pevent_func_)
                (*pevent_func_)(conv, event_type, msg, event_func_var_);
            break;
        default:
            ; // do nothing
    }
}
void ALKcpClient::init_kcp(kcp_conv_t conv)
{
    m_pKcp = ikcp_create(conv,(void*)this);
    m_pKcp->output = &ALKcpClient::udp_output;

    ikcp_nodelay(m_pKcp, 1, 2, 1, 1); // 设置成1次ACK跨越直接重传, 这样反应速度会更快. 内部时钟5毫秒.
}

int ALKcpClient::init_udp_connect()
{
    m_pUdpSocket = new QUdpSocket(this);
    m_pUdpSocket->bind(server_host,server_udp_port);
    connect(m_pUdpSocket,&QUdpSocket::readyRead,this,&ALKcpClient::onReadUdpDatagrams);

    return 0;
}

bool ALKcpClient::connect_timeout(quint64 cur_clock)
{
    return (cur_clock - last_send_connect_msg_time > KCP_RESEND_CONNECT_MSG_INTERVAL);
}

bool ALKcpClient::need_send_connect_packet(uint64_t cur_clock) const
{
    return (cur_clock - last_send_connect_msg_time > KCP_RESEND_CONNECT_MSG_INTERVAL);
}



int ALKcpClient::udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    ((ALKcpClient*)user)->send_udp_package(buf,len);
    return 0;
}

void ALKcpClient::send_udp_package(const char *buf, int len)
{
    m_pUdpSocket->writeDatagram(buf,len,server_host,server_udp_port);
}

void ALKcpClient::do_send_connect_packet(quint64 cur_clock)
{
    last_send_connect_msg_time = cur_clock;

    //发送 一个连接命令
    std::string connect_msg = asio_kcp::making_connect_packet();
    std::cerr << "send connect packet" << std::endl;

    //m_pUdpSocket->writeData(connect_msg.c_str(),connect_msg.size());
    m_pUdpSocket->writeDatagram(connect_msg.c_str(),connect_msg.size(),server_host,server_udp_port);
}

void ALKcpClient::update()
{
    quint64 cur_clock = QDateTime::currentMSecsSinceEpoch();
    if(in_connect_stage_)
    {
        do_asio_kcp_connect(cur_clock);
        return;
    }
    if(connect_successed)
    {
        do_send_msg_in_queue();

        do_recv_udp_packet_in_loop();

        ikcp_update(m_pKcp,cur_clock);
    }



}

void ALKcpClient::onReadUdpDatagrams()
{
    while (m_udpSocket.hasPendingDatagrams())
    {
       QByteArray datagram;
       datagram.resize(m_udpSocket.pendingDatagramSize());
       QHostAddress sender;
       quint16 senderPort;

       m_udpSocket.readDatagram(datagram.data(), datagram.size(),
                               &sender, &senderPort);


       if(asio_kcp::is_send_back_conv_packet(datagram.data(),datagram.size()))
       {
           // save conv when recved connect back packet.
           kcp_conv_t conv = asio_kcp::grab_conv_from_send_back_conv_packet(recv_buf, ret_recv);
           // init p_kcp_
           init_kcp(conv);
           in_connect_stage_ = false;
           connect_succeed_ = true;
       }

       if(connect_successed)
       {

       }
    }
}

void ALKcpClient::onUpdateTimer()
{
    qint64 t_curTime = QDateTime::currentMSecsSinceEpoch();
    ikcp_update(m_pKcp,t_curTime);


    uint newMilliseconds = ikcp_check(m_pKcp,t_curTime);
    uint t_waitTime = newMilliseconds - (quint32)t_curTimeInt32;
    m_updateTimer.start(t_waitTime);
}

void ALKcpClient::do_asio_kcp_connect(uint64_t cur_clock)
{
    if (connect_timeout(cur_clock))
    {
        (*pevent_func_)(0, eConnectFailed, KCP_CONNECT_TIMEOUT_MSG, event_callback_var_);
        in_connect_stage_ = false;
        return;
    }

    if(need_send_connect_packet(cur_clock))
    {
        do_send_connect_packet(cur_clock);
    }

    try_recv_connect_back_packet();
}

void ALKcpClient::do_send_msg_in_queue()
{

}

void ALKcpClient::do_recv_udp_packet_in_loop()
{

}
