#pragma once

#include "name_base.h"


// 子类注册函数
typedef void (*RegisterModNamesFunc)(NameFactory* factory);

// 主程序检测子类
class TestName : public Name {
public:
    TestName();
    ~TestName();

    static std::string GetId() { return "chinese"; }
    virtual std::string GetType() const override { return "chinese"; }
    virtual std::string GetName() const override { return "中文姓名"; }

    virtual std::string GetSurname(std::string name);
    virtual std::string GenerateName(bool male = true, bool female = true, bool neutral = true);
    virtual std::string GenerateName(std::string surname, bool male = true, bool female = true, bool neutral = true);

private:
    std::vector<std::string> surnames;
    std::vector<std::string> maleNames;
    std::vector<std::string> femaleNames;
    std::vector<std::string> neutralNames;

    void InitializeSurnames();
    void InitializeNames();

};
