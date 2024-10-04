#ifndef REDIS_H
#define REDIS_H

#include<hiredis/hiredis.h>
#include<thread>
#include<functional>
using namespace std;


class Redis{
public:
    Redis();
    ~Redis();

    //连接redis服务器
    bool connect();

    //向redis指定的通道channel发布消息
    bool publish(int ,string);

    //向redis指定的通道subscribe订阅消息
    bool subscribe(int);

    //向redis指定的通道unsubscribe取消消息订阅
    bool unsubscribe(int);

    //在独立线程中接收订阅通道消息中的消息
    void observer_channel_message();

    //初始化向业务层上报通道消息的回调对象
    void init_notify_handler(function<void(int,string)>);

private:
    //hireids同步上下文对象，负责publish消息
    redisContext* _publish_context;

    //hiredis同步上下文对象，负责subscribe消息
    redisContext* _subscribe_context;

    //回调操作，接收订阅到的消息，给service层汇报
    function<void(int,string)> _notify_message_handler;
};

#endif