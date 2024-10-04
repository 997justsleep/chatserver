#include"offlinemessagemodel.hpp"
#include"db.hpp"

//存储离线消息
void OfflineMsgModel::insertOfflineMsg(int userid,string msg){
    //1 组装sql语句
    char sql[1024] = {0};
    sprintf(sql,"insert into offlinemessage values(%d,'%s')",userid,msg.c_str());
    //执行SQL语句
    MySQL mysql;
    if(mysql.connect()){
        mysql.query(sql);
    }
    return;
}

//删除已读信息
void OfflineMsgModel::removeOfflineMsg(int userid){
    //1 组装sql语句
    char sql[1024] = {0};
    sprintf(sql,"delete from offlinemessage where userid = %d",userid);
    //执行SQL语句
    MySQL mysql;
    if(mysql.connect()){
        mysql.query(sql);
    }
    return;
}

//查询用户的离线消息
vector<string> OfflineMsgModel::query(int userid){
    //1 组装sql语句
    char sql[1024] = {0};
    sprintf(sql,"select message from offlinemessage where userid = %d",userid);
    vector<string>vec;
    MySQL mysql;
    if(mysql.connect()){
         MYSQL_RES* res = mysql.query(sql);
        if(res != nullptr){
            MYSQL_ROW row;
            //只要还有消息就存到vector中
            while((row = mysql_fetch_row(res)) != nullptr){
                vec.push_back(row[0]);
            }
            mysql_free_result(res);
        }
    }
    return vec;
}