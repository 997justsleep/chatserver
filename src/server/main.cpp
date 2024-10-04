#include"chatserver.hpp"
#include"chatservice.hpp"
#include<iostream>
#include<signal.h>
using namespace std;

//处理服务器异常结束后，重置user的状态
void resetHandler(int){
    ChatService::getinstance().reset();
    exit(0);
}

int main(int argc,char** argv){
    if(argc < 3){
        cerr << "命令错误，示例：./ChatServer 127.0.0.1 6000"<<endl;
        exit(-1);
    }

    //解析通过命令行参数传递的ip和port
    char* ip = argv[1];
    uint16_t port = atoi(argv[2]); 

    signal(SIGINT,resetHandler);

    EventLoop loop;
    InetAddress addr(ip,port);
    ChatServer server(&loop,addr,"ChatServer");

    server.start();
    loop.loop();

}