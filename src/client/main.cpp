#include"json.hpp"
#include<iostream>
#include<thread>
#include<string>
#include<vector>
#include<chrono>
#include<ctime>
using namespace std;
using json = nlohmann::json;

#include<unistd.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>

#include"group.hpp"
#include"user.hpp"
#include"public.hpp"

//控制主菜单页面程序
bool isMainMenuRunning = false;

//记录当前系统登陆的用户信息
User g_currentUser;
//记录当前登录用户的好友信息
vector<User> g_currentUserFriendList;
//记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;

//显示当前登录成功的用户的基本信息
void showCurrentUserData();
//接收线程
void readTaskHandler(int clientfd);
//获取系统时间（聊天信息中添加时间信息）
string getCurrentTime();
//主聊天页面程序
void mainMenu(int clientfd);

//聊天客户端程序实现，main线程用作发送线程，子线程用作接收线程
int main(int argc,char** argv){
    if(argc < 3){
        cerr << "命令错误，示例：./ChatClient 127.0.0.1 6000"<<endl;
        exit(-1);
    }

    //解析命令行内容
    char* ip = argv[1];
    uint16_t port = atoi(argv[2]);

    //创建client的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if(clientfd == -1){
        cerr << "socket 创建错误" <<endl;
        exit(-1);
    }

    //填写client需要连接的server信息 ip+port
    sockaddr_in server;
    memset(&server,0,sizeof(sockaddr_in));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    //client 和 server进行连接
    if(connect(clientfd,(sockaddr*)&server,sizeof(sockaddr_in)) == -1){
        cerr << "connect server error" <<endl;
        close(clientfd);
        exit(-1);
    }

    //main线程用于接收用户输入，负责发送数据
    while(1){
        //显示首页菜单 登录、注册、退出
        cout<<"-----------------------"<<endl;
        cout<<"1. 登录"<<endl;
        cout<<"2. 注册"<<endl;
        cout<<"3. 退出"<<endl;
        cout<<"-----------------------"<<endl;
        cout<<"请选择: ";
        int choice = 0;
        cin>>choice;
        cin.get();//吃掉回车

        switch(choice){
        case 1: {//登录
            int id = 0;
            char pwd[50] = {0};
            cout<<"用户id: ";
            cin>>id;
            cin.get();
            cout<<"密码: ";
            cin.getline(pwd,50);

            json js;
            js["msgid"] = LOGIN_MSG;
            js["id"] = id;
            js["password"] = pwd;
            string request = js.dump();

            int len = send(clientfd,request.c_str(),strlen(request.c_str())+1,0);
            if(len == -1){
                cerr<<"发送登录消息错误"<<request<<endl;
            }else{
                char buffer[1024] = {0};
                len = recv(clientfd,buffer,1024,0);
                if(len == -1){
                    cerr<<"接收登录消息错误"<<endl;
                }else{
                    json respondjs = json::parse(buffer);
                    if(respondjs["errno"].get<int>() != 0){//登录失败
                        cerr << respondjs["errmsg"] <<endl;
                    }else{//登录成功
                        //记录当前用户 id 和name
                        g_currentUser.setId(respondjs["id"].get<int>());
                        g_currentUser.setName(respondjs["name"]);

                        //记录当前用户的好友列表消息
                        if(respondjs.contains("friend")){
                            //初始化
                            g_currentUserFriendList.clear();

                            vector<string> vec = respondjs["friend"];
                            for(string& str:vec){
                                json js = json::parse(str);
                                User user;
                                user.setId(js["id"]);
                                user.setName(js["name"]);
                                user.setState(js["state"]);
                                g_currentUserFriendList.push_back(user);
                            }
                        }

                        //记录当前用户的群组信息
                        if(respondjs.contains("group")){
                            //初始化
                            g_currentUserGroupList.clear();

                            vector<string> vec1 = respondjs["group"];
                            for(string& groupstr:vec1){
                                json groupjs = json::parse(groupstr);
                                Group group;
                                group.setId(groupjs["id"].get<int>());
                                group.setName(groupjs["groupname"]);
                                group.setDesc(groupjs["groupdesc"]);

                                vector<string> vec2 = groupjs["users"];
                                for(string& userstr:vec2){
                                    GroupUser user;
                                    json js = json::parse(userstr);
                                    user.setId(js["id"].get<int>());
                                    user.setName(js["name"]);
                                    user.setState(js["state"]);
                                    user.setRole(js["role"]);
                                    group.getUsers().push_back(user);
                                }
                                g_currentUserGroupList.push_back(group);
                            }
                        }
                        //显示用户的基本信息
                        showCurrentUserData();

                        //显示当前用户的离线消息 个人聊天信息或群组消息
                        if(respondjs.contains("offlinemsg")){
                            vector<string> vec = respondjs["offlinemsg"];
                            for(string& str : vec){
                                json js = json::parse(str);
                                if(js["msgid"] == ONE_CHAT_MSG){//私聊
                                    cout<<js["time"].get<string>()<<js["name"].get<string>()<<"["<<js["id"]<<"]"
                                    <<"说："<<js["msg"].get<string>()<<endl;
                                }else{//群聊
                                    cout<<"群消息["<< js["groupid"] << "]:" << js["time"].get<string>() << " [" 
                                        << js["id"] << "]" << js["name"].get<string>()
                                        << " 说: " << js["msg"].get<string>() << endl;
                                }
                                
                            }
                        }

                        //登录成功，启动接收线程负责接收数据
                        //该线程只启动一次
                        static int threadnum = 0;
                        if(threadnum == 0){
                            std::thread readTask(readTaskHandler,clientfd);//在Linux下相当于 pthread_create
                            //设置分离线程
                            readTask.detach();//pthread_detach
                            threadnum++;
                        }
                        //进入聊天主菜单页面
                        isMainMenuRunning = true;
                        mainMenu(clientfd);
                    }
                }
            }
        }
        break;
        case 2:{//注册
            char name[50] = {0};
            char pwd[50] = {0};
            cout<<"用户名：";
            cin.getline(name,50);
            cout<<"密码：";
            cin.getline(pwd,50);

            json js;
            js["msgid"] = REG_MSG;
            js["name"] = name;
            js["password"] = pwd;
            string request = js.dump();

            int len = send(clientfd,request.c_str(),strlen(request.c_str())+1,0);
            if(len == -1){
                cerr<<"发送注册消息错误"<<request<<endl;
            }else{
                char buffer[1024] = {0};
                len = recv(clientfd,buffer,1024,0);
                if(len == -1){
                    cerr<<"接收注册消息错误"<<endl;
                }else{
                    json respondjs = json::parse(buffer);
                    if(respondjs["errno"].get<int>() != 0){//注册失败
                        cerr << name <<"已经存在，注册失败"<<endl;
                    }else{//注册成功
                        cout<<name<<"注册成功, 用户id 是"<<respondjs["id"]<< "，请牢记"<<endl;
                    }
                }
            }
        }
        break;
        case 3:
            exit(0);
        default:
            cerr << "输入无效" <<endl;
            break;
        }

    }
    return 0;
}

//显示当前登录成功的用户的基本信息
void showCurrentUserData(){
    cout<<"------------登录用户-------------"<<endl;
    cout<<"当前用户 id: "<<g_currentUser.getId()<<"用户名: "<<g_currentUser.getName()<<endl;
    cout<<"------------好友列表-------------"<<endl;
    if(!g_currentUserFriendList.empty()){
        for(User &user : g_currentUserFriendList){
            cout<< user.getId()<<" "<<user.getName()<<" "<<user.getState()<<endl;
        }
    }
    cout<<"------------群组列表-------------"<<endl;
    if(!g_currentUserGroupList.empty()){
        for (Group &group : g_currentUserGroupList){
            cout << group.getId() << " " << group.getName() << " " << group.getDesc() << endl;
            for (GroupUser &user : group.getUsers()){
                cout << "| "<< user.getId() << " " << user.getName() << " " << user.getState()
                     << " " << user.getRole() << endl;
            }
        }
    }
    cout<<"--------------------------------"<<endl;
}

//接收线程
void readTaskHandler(int clientfd){
    while(1){
        char buffer[1024] = {0};
        int len = recv(clientfd,buffer,1024,0);
        if(len == -1 || len ==0){
            close(clientfd);
            exit(0);
        }

        //接收ChatServer转发的数据，反序列化生成json对象
        json js = json::parse(buffer);
        if(js["msgid"] == ONE_CHAT_MSG){
            cout<<js["time"].get<string>()<<"["<<js["id"]<<"]"<<js["name"].get<string>()
                <<"说："<<js["msg"].get<string>()<<endl;
            continue;
        }else if(js["msgid"] == GROUP_CHAT_MSG){
            cout<<"群消息: "<<'['<<js["groupid"]<<']'<<js["time"].get<string>()<<js["name"].get<string>()
                <<'['<<js["id"]<<"] 说: "<<js["msg"].get<string>()<<endl;
            continue;
        }
    }
}

//"help"命令
void help(int i = 0,string str = "");
//"chat"命令
void chat(int,string);
//"addfriend"命令
void addfriend(int,string);
//"creategroup"命令
void creategroup(int,string);
//"addgroup"命令
void addgroup(int,string);
//"groupchat"命令
void groupchat(int,string);
//"loginout"命令
void loginout(int,string);

//命令列表
unordered_map<string,string> commandMap = {
    {"help","显示所有命令, 格式: help"},
    {"chat","一对一聊天, 格式: chat:friendid:message"},
    {"addfriend","添加好友, 格式: addfriend:friendid"},
    {"creategroup", "创建群组, 格式: creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组, 格式: addgroup:groupid"},
    {"groupchat", "群聊, 格式: groupchat:groupid:message"},
    {"logout", "注销, 格式: loginout"}
};

//注册系统支持的客户端命令
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"logout", loginout}
};

//获取系统时间（聊天信息中添加时间信息）
string getCurrentTime(){
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm* ptm = localtime(&tt);
    char date [60] = {0};
    sprintf(date,"%d-%02d-%02d %02d:%02d:%02d",
                (int)ptm->tm_year+1900,(int)ptm->tm_mon+1,(int)ptm->tm_mday,
                (int)ptm->tm_hour,(int)ptm->tm_min,(int)ptm->tm_sec);
    return string(date);
}

//主聊天页面程序
void mainMenu(int clientfd){
    help();

    char buffer[1024] = {0};
    while(isMainMenuRunning){
        cin.getline(buffer,1024);
        string commandstr(buffer);
        string command;
        int index = commandstr.find(':');
        if(index == -1){
            command = commandstr;
        }else{
            command = commandstr.substr(0,index);
        }
        auto it = commandHandlerMap.find(command);
        if(it == commandHandlerMap.end()){
            cerr << "命令无效" <<endl;
            continue;
        }

        //调用对应的命令事件回调，mainMenu对修改封闭，添加新功能不需要修改该函数
        //调用对应命令函数
        //传入命令函数的命令参数为去掉命令名称的命令，只剩下命令后的消息和目标
        it->second(clientfd,commandstr.substr(index+1));
    }

}

void help(int i,string str){
    cout<<"命令列表"<<endl;
    for(auto it:commandMap){
        cout<<it.first<<": "<<it.second<<endl;
    }
    cout<<endl;
}


//"chat"命令
void chat(int clientfd,string str){
    int index = str.find(':');
    if(index == -1){
        cerr<<"命令无效"<<endl;
        return;
    }
    int friendid = atoi(str.substr(0,index).c_str());
    string msg = str.substr(index+1);

    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["toid"] = friendid;
    js["msg"] = msg;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len == -1){
        cerr<<"发送消息错误"<<buffer<<endl;
    }
}

//"addfriend"命令
void addfriend(int clientfd,string str){
    int friendid = atoi(str.c_str());

    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();

    int len = send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len == -1){
        cerr<<"发送消息错误"<<buffer<<endl;
    }
}

//"creategroup"命令
void creategroup(int clientfd,string str){
    int index = str.find(':');
    if(index == -1){
        cerr<<"命令无效"<<endl;
        return;
    }

    string groupname = str.substr(0,index);
    string groupdesc = str.substr(index+1);

    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;
    string buffer = js.dump();

    int len = send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len == -1){
        cerr<<"发送消息错误"<<buffer<<endl;
    }
}

//"addgroup"命令
void addgroup(int clientfd,string str){
    int groupid = atoi(str.c_str());

    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;
    string buffer = js.dump();

    int len = send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len == -1){
        cerr<<"发送消息错误"<<buffer<<endl;
    }
}

//"groupchat"命令
void groupchat(int clientfd,string str){
    int index = str.find(':');
    if(index == -1){
        cerr<<"命令无效"<<endl;
        return;
    }
    int groupid = atoi(str.substr(0,index).c_str());
    string msg = str.substr(index+1);

    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["groupid"] = groupid;
    js["msg"] = msg;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len == -1){
        cerr<<"发送消息错误"<<buffer<<endl;
    }

}

//"loginout"命令
void loginout(int clientfd,string str){
    json js;
    js["msgid"] = LOGOUT_MSG;
    js["id"] = g_currentUser.getId();
    string buffer  = js.dump();

    int len = send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len == -1){
        cerr<<"发送消息错误"<<buffer<<endl;
        return;
    }
    isMainMenuRunning = false;
    exit(0);
}
