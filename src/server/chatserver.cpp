#include "chatserver.hpp"
#include"json.hpp"
#include"chatservice.hpp"

#include<functional>
using namespace std;
using namespace placeholders;
using json = nlohmann::json;

ChatServer::ChatServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       const string &nameArg)
    : _server(loop, listenAddr, nameArg), _loop(loop)
{
    // 注册连接回调函数
    _server.setConnectionCallback(bind(&ChatServer::onConnection,this,_1));
    // 注册消息回调函数
    _server.setMessageCallback(bind(&ChatServer::onMessage, this, _1, _2, _3));
    // 设置线程数量
    _server.setThreadNum(4);
}
/**
 * mainLoop(mainThread) int listenfd = socket() listenfd --> 客户端的连接
 * setThreadNum(4) mainLoop + subLoop == 4 
 * 4 * subLoop(subThread) --> int connfd = accept() --> 监听已连接用户的读写事件
 */

// 启动服务
void ChatServer::start()
{
    _server.start();
}

// 上报连接信息的回调函数
void ChatServer::onConnection(const TcpConnectionPtr & conn)
{
    //客户端断开连接
    if(!conn->connected()){
        ChatService::getinstance().clientCloseException(conn);
        conn->shutdown();
    }
}

// 上报读写事件的回调函数
void ChatServer::onMessage(const TcpConnectionPtr &conn, Buffer *buffer, Timestamp time)
{
    string buf = buffer->retrieveAllAsString();
    //数据反序列化
    json js = json::parse(buf);
    //达到的目的：完全解耦网络模块的代码和业务模块的代码
    //通过js["msgid"] 获取 -> 业务handler -> conn js time
    auto msgHandler = ChatService::getinstance().getHandler(js["msgid"].get<int>());
    //回调消息绑定好的事件处理器，来执行相应的业务处理
    msgHandler(conn,js,time);
    
}