#ifndef USERMODEL_H
#define USERMODEL_H

#include"user.hpp"

//user表的数据操作
class UserModel{
public:
    //user表的增加
    bool insert(User& user);

    //根据用户id返回用户信息
    User query(int id);

    //更新用户状态信息
    bool updateState(User user);

    //重置用户状态信息
    void resetState();
};



#endif