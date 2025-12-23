#pragma once

#include "name_base.h"


// 子类注册函数
typedef void (*RegisterModNamesFunc)(NameFactory* factory);

// 中文姓名
class ChineseName : public Name {
public:
    ChineseName();
    ~ChineseName();

    static std::string GetId();
    virtual std::string GetType() const override;
    virtual std::string GetName() const override;

    virtual std::string GetSurname(std::string name) override;
    virtual std::string GenerateName(bool male = true, bool female = true, bool neutral = true) override;
    virtual std::string GenerateName(std::string surname, bool male = true, bool female = true, bool neutral = true) override;

private:
    std::vector<std::string> surnames;
    std::vector<std::string> maleNames;
    std::vector<std::string> femaleNames;
    std::vector<std::string> neutralNames;

    void InitializeSurnames();
    void InitializeNames();

};
