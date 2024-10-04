#ifndef OFFLINEMESSAGEMODEL_H
#define OFFLINEMESSAGEMODEL_H

#include<string>
#include<vector>
using namespace std;

//提供离线消息表的操作接口
class OfflineMsgModel{
public:
    //存储离线消息
    void insertOfflineMsg(int userid,string msg);

    //删除已读信息
    void removeOfflineMsg(int userid);

    //查询用户的离线消息
    vector<string> query(int userid);
};

#endif