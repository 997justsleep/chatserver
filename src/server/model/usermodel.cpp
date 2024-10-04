#include"usermodel.hpp"
#include"db.hpp"
#include"chatservice.hpp"
#include<iostream>
using namespace std;

//添加数据
bool UserModel::insert(User& user){
    //1 组装sql语句
    char sql[1024] = {0};
    sprintf(sql,"insert into user(name, password, state) values('%s', '%s', '%s')",
            user.getName().c_str(), user.getPassword().c_str(), user.getState().c_str());

    MySQL mysql;
    if(mysql.connect()){
        if(mysql.update(sql)){
            //获取插入成功的用户数据生成的主键id
            user.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
    return false;

}

//根据用户id返回用户信息
User UserModel::query(int id){
    char sql[1024] = {0};
    sprintf(sql,"select * from user where id = %d",id);
    MySQL mysql;
    if(mysql.connect()){
        //得到执行sql后的结果
        MYSQL_RES* res = mysql.query(sql);
        if(res != nullptr){
            //有结果就取第一行结果
            MYSQL_ROW row = mysql_fetch_row(res);
            if(row != nullptr){
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setPassword(row[2]);
                user.setState(row[3]);
                mysql_free_result(res);
                return user;
            }
        }
    }
    return User();
}

//更新在线状态
bool UserModel::updateState(User user){
    char sql[1024] = {0};
    sprintf(sql,"update user set state = '%s' where id = %d",user.getState().c_str(),user.getId());
    MySQL mysql;
    if(mysql.connect()){
        if(mysql.update(sql)){
            return true;
        }
    }
    return false;
}

//重置用户状态信息 用于服务器异常关闭
void UserModel::resetState(){
    char sql[1024] = {0};
    MySQL mysql;
    //只重置当前服务器上的用户在线状态
    for(auto it:ChatService::getinstance().getuserConnid()){
        sprintf(sql,"update user set state = 'offline' where id = %d",it);
        if(mysql.connect()){
        mysql.update(sql);  
        }
    }
}