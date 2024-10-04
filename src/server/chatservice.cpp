#include"chatservice.hpp"
#include"public.hpp"
#include<map>
#include<muduo/base/Logging.h>
#include<vector>
using namespace std;
using namespace muduo;

ChatService& ChatService::getinstance(){
    static ChatService instance;
    return instance;
}

//注册消息以及对应的Handler回调操作
ChatService:: ChatService(){
    _msgHandlerMap.insert({LOGIN_MSG,std::bind(&ChatService::login,this,_1,_2,_3)});
    _msgHandlerMap.insert({LOGOUT_MSG,std::bind(&ChatService::logout,this,_1,_2,_3)});
    _msgHandlerMap.insert({REG_MSG,std::bind(&ChatService::reg,this,_1,_2,_3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG,std::bind(&ChatService::onechat,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG,std::bind(&ChatService::addFriend,this,_1,_2,_3)});
    _msgHandlerMap.insert({CREATE_GROUP_MSG,std::bind(&ChatService::createGroup,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG,std::bind(&ChatService::addGroup,this,_1,_2,_3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG,std::bind(&ChatService::groupChat,this,_1,_2,_3)});

    if(_redis.connect()){
        //设置上报的消息回调
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage,this,_1,_2));
    }
}

//注册消息对应的处理器
MsgHandler ChatService:: getHandler(int msgid){
    //记录错误日志，msgid没有对应的事件处理回调
    auto it = _msgHandlerMap.find(msgid);
    if(it == _msgHandlerMap.end()){
        //返回一个默认的处理器，空操作
        return [=](const TcpConnectionPtr& conn,json js ,Timestamp ){
            LOG_ERROR << "msgid" << msgid << "can not find handler";
        };
    }else{
         return _msgHandlerMap[msgid];
    }
}

//处理登录业务 id password
void ChatService::login(const TcpConnectionPtr &conn,json &js,Timestamp time){
    int id = js["id"];
    string pwd = js["password"];
    
    User user = _userModel.query(id);
    if(user.getId()!= -1 && user.getPassword() == pwd){
        if(user.getState() == "online"){
            //在线用户不允许重复登录
            json respond;
            respond["msgid"] = LOGIN_MSG_ACK;
            respond["errno"] = 2;
            respond["errmsg"] = "该账号已登录";
            conn->send(respond.dump());
        }else{
            //记录连接
            //用{}括起，进入{加锁，出了}解锁 保证哈希容器的线程安全
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id,conn});
            }

            //idy用户登录成功后，向redis订阅channel（id）
            _redis.subscribe(id);
            
            //登录成功 更新用户在线状态为online
            user.setState("online");
            _userModel.updateState(user);

            json respond;
            respond["msgid"] = LOGIN_MSG_ACK;
            respond["errno"] = 0;
            respond["id"] = user.getId();
            respond["name"] = user.getName();

            //查询用户的离线消息
            vector<string>vec = _offlineMsgModel.query(id);
            if(!vec.empty()){
                respond["offlinemsg"] = vec;
                //读取后删除
                _offlineMsgModel.removeOfflineMsg(id);
            }
            //查询用户的好友并返回
            vector<User>userVec = _friendModel.query(id);
            if(!userVec.empty()){
                vector<string>vec2;
                for(User user:userVec){
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                respond["friend"] = vec2;
            }
            //查询群组消息返回
            vector<Group>groupVec = _groupModel.queryGroups(id);
            if(!groupVec.empty()){
                vector<string>vec2;
                for(Group group:groupVec){
                    json js;
                    js["id"] = group.getId();
                    js["groupname"] = group.getName();
                    js["groupdesc"] = group.getDesc();
                    vector<string>userv;
                    for(GroupUser user:group.getUsers()){
                        json js2;
                        js2["id"] = user.getId();
                        js2["name"] = user.getName();
                        js2["state"] = user.getState();
                        js2["role"] = user.getRole();
                        userv.push_back(js2.dump());
                    }
                    js["users"] = userv;
                    vec2.push_back(js.dump());
                }
                respond["group"] = vec2;
            }

            conn->send(respond.dump());
        }    
    }else{
        //登录失败  用户不存在  密码错误
        json respond;
        respond["msgid"] = LOGIN_MSG_ACK;
        respond["errno"] = 1;
        respond["errmsg"] = "用户名或密码错误";
        conn->send(respond.dump());
    }
}

//处理下线业务
void ChatService::logout(const TcpConnectionPtr &conn,json &js,Timestamp time){
    int userid = js["id"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if(it != _userConnMap.end()){
            _userConnMap.erase(it);
        }
    }

    //用户下线，redis取消订阅channel
    _redis.unsubscribe(userid);

    //用户下线，更新在线状态
    User user(userid,"","","offline");
    _userModel.updateState(user);
}

//处理注册业务 name password
void ChatService::reg(const TcpConnectionPtr &conn,json &js,Timestamp time){
     string name = js["name"];
     string pwd = js["password"];

     User user;
     user.setName(name);
     user.setPassword(pwd);
     bool state = _userModel.insert(user);
     if(state){
        //注册成功
        json respond;
        respond["msgid"] = REG_MSG_ACK;
        respond["errno"] = 0;
        respond["id"] = user.getId();
        conn->send(respond.dump());
     }else{
        //注册失败
        json respond;
        respond["msgid"] = REG_MSG_ACK;
        respond["errno"] = 1;
        conn->send(respond.dump());
     }
}

//处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr& conn){
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        //遍历存储连接的容器，删除断开的连接
        for(auto it = _userConnMap.begin(); it != _userConnMap.end() ; ++it){
            if(it->second == conn){
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }

    //用户注销，相当于下线，在redis中取消订阅channel
    _redis.unsubscribe(user.getId());

    //更新用户在线状态
    user.setState("offline");
    _userModel.updateState(user);
}

void ChatService::onechat(const TcpConnectionPtr &conn,json &js,Timestamp time){
    int toid = js["toid"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if(it != _userConnMap.end()){
            //目标在线，转发消息
            it->second->send(js.dump());
            return;
        }
    }

    //查询toid是否在线
    User user = _userModel.query(toid);
    if(user.getState() == "online"){
        _redis.publish(toid,js.dump());
        return;
    }

    //目标离线，转为离线消息
    _offlineMsgModel.insertOfflineMsg(toid,js.dump());

}

//服务器异常关闭的重置
void ChatService::reset(){
    //把所有用户状态改成 offline
    _userModel.resetState();
}

void ChatService::addFriend(const TcpConnectionPtr &conn,json &js,Timestamp time){
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    //存储好友信息
    _friendModel.insert(userid,friendid);

}

//创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn,json &js,Timestamp time){
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    //存储创建的群组信息
    Group group(-1,name,desc);
    if(_groupModel.createGroup(group)){
        //存储创建人信息
        _groupModel.addGroup(userid,group.getId(),"creator");
    }
    return;
}

//加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn,json &js,Timestamp time){
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid,groupid,"normal");
}

//群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn,json &js,Timestamp time){
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int>useridVec = _groupModel.queryGroupUsers(userid,groupid);

    lock_guard<mutex> lock(_connMutex);
    for(int id:useridVec){
        auto it = _userConnMap.find(id);
        if(it != _userConnMap.end()){
            //转发消息
            it->second->send(js.dump());
        }else{
            //查询toid是否在线
            User user = _userModel.query(id);
            if(user.getState() == "online"){
                _redis.publish(id,js.dump());
            }else{
                //存储为离线消息
                _offlineMsgModel.insertOfflineMsg(id,js.dump());
            }
        }
    }
    return;
}


//从redis消息队列中获取订阅的消息
void ChatService::handleRedisSubscribeMessage(int userid,string msg){
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if(it != _userConnMap.end()){
        it->second->send(msg);
        return;
    }

    //存储为用户的离线消息
    _offlineMsgModel.insertOfflineMsg(userid,msg);
}

//获取用户连接的id vector
vector<int>& ChatService::getuserConnid(){
    static vector<int> vec;
    //执行函数时创建，返回vec本身，避免拷贝构造
    for(auto it:_userConnMap){
        vec.push_back(it.first);
    }
    return vec;
}