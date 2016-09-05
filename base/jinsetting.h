#ifndef JINSETTING_H
#define JINSETTING_H

#include "jinstring.h"

class JinSetting
{
public:
    //
    static JinString getConfigPath(const char* organization=NULL, const char* application=NULL);
private:
    JinSetting(); //未实现，实现后放到public
};

#endif // JINSETTING_H
