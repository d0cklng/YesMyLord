#include <stdlib.h>
#include "xproxyassistant.h"
#include "jinlogger.h"
#include "jinplatformgap.h"   //linux itoa

bool XProxyAssistant::OnProxyRecv(const char *buf, uint32_t byteRecved,
                                  ProxyClientInfo *info)
{
    JAssert(info->proxyType>=0);
    if(info->proxyType == PTContinueRecv)  //都是第一次联络未完的.
    {
        JAssert(info->state < Larger_IS_AlreadySend_OpenTo);
        info->cached.cat(buf,byteRecved);
        info->proxyType = typeDetect(info->cached);
        if(info->proxyType < PTContinueRecv)
        {
            //LordProxyType type = LordProxyType(-info->proxyType);
            info->proxyType = LordProxyType(-info->proxyType);
            this->ReplySpecialCode(info,PRC_BAD_REQUEST);
            return false;
        }
        else if(info->proxyType == PTContinueRecv)
        {
            return true;
        }
        return HandleProxyHead(info);
    }
    if(info->state < Larger_IS_AlreadySend_OpenTo)
    {   //一条请求没完又发来消息,关闭!
        JELog("close more msg when first is not ok");
        this->ReplySpecialCode(info,PRC_BAD_REQUEST);
        return false;
    }
    if(info->state == WaitOpenTo)
    {   //一条请求没完又发来消息,关闭!
        //JELog("close more msg when first is not ok");
        //return false;
        //缓存起来..
        JDLog("more recv when WaitOpenTo %u",byteRecved);
        info->cached.cat(buf,byteRecved);
        return true;
    }
    if(info->state == WaitOpenToLargePost || info->state == ProxyToLargePost)
    {
        JAssert(info->contentLengthLeft>0);
        if(byteRecved > (uint32_t)info->contentLengthLeft)
        {
            JDLog("more recv pend large post %u",byteRecved - info->contentLengthLeft);
            info->cached.cat(buf + info->contentLengthLeft ,byteRecved - info->contentLengthLeft);
            byteRecved = info->contentLengthLeft;
        }
        info->contentLengthLeft -= byteRecved;
        cb_->ProxyDoSendData(info,buf,byteRecved);
        if(info->contentLengthLeft == 0)
        {  //转成正式状态.
            info->state = IncomeSockState(info->state+(info->state>Larget_IS_Already_OpenTo?1:-1));
        }
        JDLog("LargePost contentlength left=%d, state=%d",info->contentLengthLeft,info->state);
        return true;
    }
    JAssert(info->state > Larget_IS_Already_OpenTo);
    switch(info->proxyType)
    {
    case PTHTTP:
    {
        info->cached.cat(buf,byteRecved);
        return HandleProxyHead(info);
    }
        break;
    case PTHTTPS:
    case PTFORWARD:
    case PTSOCK4:
    case PTSOCK5:
    {
        cb_->ProxyDoSendData(info,buf,byteRecved);
        return true;
    }
        break;
    case PTContinueRecv:
    default:
    {
        JAssert(false);
    }
        break;
    }
    return false;
}

void XProxyAssistant::ReplySpecialCode(ProxyClientInfo *info,ProxyResponseCode code)
{
    int c = 0;
    JAssert(info->proxyType > PTContinueRecv);
    switch(info->proxyType)
    {
    case PTHTTP:
    case PTHTTPS:
        switch(code)
        {
        case PRC_OK: c=200; break;
        case PRC_BAD_REQUEST: c=400; break;
        case PRC_NOT_SUPPORT: c=501; break;
        case PRC_SERVER_ERROR: c=500; break;
        case PRC_BAD_GATEWAY: c=502; break;
        case PRC_GATEWAY_TIMEOUT: c=504; break;
        default: JAssert(false);
        }
        this->ReplyClient(info,PTHTTP,c);
        break;
    case PTSOCK4:
        switch(code)
        {
        case PRC_OK: c=90; break;
        case PRC_BAD_REQUEST: c=91; break;
        case PRC_NOT_SUPPORT: c=91; break;
        case PRC_SERVER_ERROR: c=92; break;
        case PRC_BAD_GATEWAY: c=92; break;
        case PRC_GATEWAY_TIMEOUT: c=92; break;
        default: JAssert(false);
        }
        this->ReplyClient(info,PTSOCK4,c);
        break;
    case PTSOCK5:
        switch(code)
        {
        case PRC_OK: c=0; break; // 0=ok succ.
        case PRC_BAD_REQUEST: c=1; break;
        case PRC_NOT_SUPPORT: c=7; break;
        case PRC_SERVER_ERROR: c=1; break;
        case PRC_BAD_GATEWAY: c=4; break;
        case PRC_GATEWAY_TIMEOUT: c=3; break;
        default: JAssert(false);
        }
        this->ReplyClient(info,PTSOCK5,c);
        break;
    case PTFORWARD:
        //do nothing
        break;
    default:
        JAssert(false);
    }
}
//返回true继续recv
bool XProxyAssistant::HandleProxyHead(ProxyClientInfo *info)
{
    JAssert(info->proxyType > PTContinueRecv);
    switch(info->proxyType)
    {
    case PTHTTP:
    case PTHTTPS:
        return HandleProxyHeadHTTP(info);
    case PTSOCK4:
        return HandleProxyHeadSOCK4(info);
    case PTSOCK5:
        return HandleProxyHeadSOCK5(info);
    default:
        break;
    }
    JAssert(false);
    return false;
}

bool XProxyAssistant::HandleProxyHeadHTTP(ProxyClientInfo *info)
{
    size_t ctidx=0;
    int contentlength = 0;
    if(info->cached.stricmp("POST",4)==0) //has contentlength
    {
        contentlength = GetHttpContentLength(info->cached,ctidx);
        if(contentlength < 0)
        {
            JELog("proxy may error recv:\n%s",info->cached.c_str());
            this->ReplySpecialCode(info,PRC_BAD_REQUEST);
            return false;
        }  //遇到content-length为0的情况.
        //if(contentlength == 0) return PTContinueRecv;
    }
    size_t endidx = info->cached.find("\r\n\r\n",ctidx);
    if(STRNPOS == endidx)
    {
        JDLog("not found \\r\\n\\r\\n, need more data! state=%d",
              info->state);
        if(info->state < Larger_IS_AlreadySend_OpenTo)
        {  //只有第一次未OpenTo的才改回ContinueRecv
            info->proxyType = PTContinueRecv;
        }
        return true;
    }
    if(info->cached.length() < endidx+4+contentlength)
    {  //没收齐的情况.一般是POST好多数据上传文件的时候.
        info->contentLengthLeft = contentlength + 4 + (int)(endidx - info->cached.length());
        JDLog("Large POST, contentlength left=%d",info->contentLengthLeft);
    }
    if(info->cached.length() > endidx+4+contentlength)
    {
        JDLog("proxy may error recv:\n%s",info->cached.c_str());
        this->ReplySpecialCode(info,PRC_BAD_REQUEST);
        return false;
    }
    JinString target;
    JinNetPort port;
    if(!GetHttpHeadHost(info->cached,info->proxyType,target,port))
    {
        this->ReplySpecialCode(info,PRC_BAD_REQUEST);
        return false;
    }
    JDLog("target=%s|%hu",target.c_str(),port.port());

    if(info->state < WaitOpenTo)
    {
        JAssert(info->target.length()==0);
        info->target = target;
        info->port = port;
        int retc = ChangeGETProxyHead(info->cached,info->target);
        if(retc < 0)
        {
            this->ReplySpecialCode(info,PRC_BAD_REQUEST);
            return false;
        }
        if(info->proxyType == PTHTTPS) //不要把CONNECT本体带出去.
        {
            info->cached.clear();
            cb_->ProxyDoOpenTo(info,false);
        }
        else
        {
            cb_->ProxyDoOpenTo(info,false);
            info->cached.clear();
        }
        if(info->contentLengthLeft>0) info->state = WaitOpenToLargePost;
        else info->state = WaitOpenTo;
        return true;
    }
    else
    {
        JAssert(info->state == ProxyTo);
        JAssert(info->proxyType == PTHTTP);
        if(info->target != target)  //目标发生改变.
        {
            JDLog("host change %s|%hu => %s|%hu info=%p",
                  info->target.c_str(),info->port.port(),
                  target.c_str(),port.port(),info);
            info->target = target;
            info->port = port;
            int retc = ChangeGETProxyHead(info->cached,info->target);
            if(retc < 0)
            {
                this->ReplySpecialCode(info,PRC_BAD_REQUEST);
                return false;
            }
            cb_->ProxyDoOpenTo(info,true);
            info->cached.clear();
            if(info->contentLengthLeft>0) info->state = WaitOpenToLargePost;
            else info->state = WaitOpenTo;
            return true;
        }
        else  //目标未改变,一样要changehead
        {
            int retc = ChangeGETProxyHead(info->cached,info->target);
            if(retc < 0)
            {
                this->ReplySpecialCode(info,PRC_BAD_REQUEST);
                return false;
            }
            cb_->ProxyDoSendData(info,info->cached.c_str(),(uint32_t)info->cached.length());
            info->cached.clear();
            if(info->contentLengthLeft>0)
            {
                JDLog("large post at ProxyTo state,contentLengthLeft=%d",info->contentLengthLeft);
                info->state = ProxyToLargePost;
            }
            return true;
        }
    }
    JAssert(false);
    return false;
}

bool XProxyAssistant::HandleProxyHeadSOCK4(ProxyClientInfo *info)
{
    const char* fpack = info->cached.c_str();
    if(info->state == WaitTarget)
    { //SOCK4不握手
        uint32_t len = info->cached.length();
        if(len >= 9 && fpack[len-1]=='\0')
        {
            char shortBuf[16];
            if(fpack[1]!=1)
            {
                this->ReplySpecialCode(info,PRC_NOT_SUPPORT);
                return false;
            }
            JAssert(info->target.length()==0);
            if(fpack[4]==0 && fpack[5]==0 && fpack[6]==0 && fpack[7]!=0)
            {
                for(uint32_t i=8;i<len-1;i++)
                {
                    if(fpack[i]==0)
                    {
                        info->target = JinString(&fpack[i+1]);
                        JDLog("sock4a addr:%s",info->target.c_str());
                        break;
                    }
                }
            }
            else
            {
                info->target = JinNetAddr::fromRawIP(*(uint32_t*)&fpack[4]).toAddr(shortBuf);
                JDLog("sock4 addr:%s",info->target.c_str());
            }
            if(info->target.length()==0)
            {
                this->ReplySpecialCode(info,PRC_BAD_REQUEST);
                return false;
            }
            info->port = JinNetPort::fromRawPort(*(uint16_t*)&fpack[2]);
            info->cached.clear();
            cb_->ProxyDoOpenTo(info,false);
            info->state = WaitOpenTo;
            return true;
        }
        cb_->ProxyDoReplyClient(info,NULL,0,true);
        return false;
    }
    JAssert(false);
    return false;
}

bool XProxyAssistant::HandleProxyHeadSOCK5(ProxyClientInfo *info)
{
    const char* fpack = info->cached.c_str();
    if(info->state == WaitTarget)
    { //预期是握手 HandShakeOK
        if(info->cached.length()>=3 && fpack[1]+2 == info->cached.length())
        {
            info->cached.clear();
            ReplyClient(info,PTSOCK5,80);  //第一包返回 80是我们自己设定的.
            info->proxyType = PTContinueRecv;
            info->state = HandShakeOK;
            return true;
        }
        cb_->ProxyDoReplyClient(info,NULL,0,true);  //just close.
        return false;

    }
    else if(info->state == HandShakeOK)
    {  //握手完成后就看对方想去哪,我们增加一路forwarding.
        if(info->cached.length()>=10)
        {
            const uint8_t &cmd = fpack[1];
            char shortBuf[16];
            if(cmd == 1) //CONNECT
            {
                if(fpack[3]==1) //ipv4 addr
                {
                    info->target = JinNetAddr::fromRawIP(*(uint32_t*)&fpack[4]).toAddr(shortBuf);
                    info->port = JinNetPort::fromRawPort(*(uint16_t*)&fpack[8]);
                }
                else if(fpack[3]==3 && info->cached.length()==(10-4+1+fpack[4])) //domainname
                {
                    info->target = JinString(&fpack[5],fpack[4]);
                    info->port =  JinNetPort::fromRawPort(*(uint16_t*)(&fpack[5]+fpack[4]));
                }
                else //ipv6 or other.
                {
                    ReplyClient(info,PTSOCK5,8); //addr not support;
                    return false;
                }
                info->cached.clear();
                cb_->ProxyDoOpenTo(info,false);
                info->state = WaitOpenTo;
                return true;
            }
            else if(cmd == 2) //BIND
            {
                JDLog("not support BIND");
                ReplyClient(info,PTSOCK5,2); //connection not allowed by ruleset
                return false;
            }
            else if(cmd == 3) //udp associate
            {
                JAssert(false);
            }
        }
        ReplyClient(info,PTSOCK5,7);
        return false;
    }
    JAssert(false);
    return false;
}

LordProxyType XProxyAssistant::typeDetect(JinString &cached_const)
{
    char first = cached_const.c_str()[0];
    if(first < 10)
    {
        if(first == 4) return PTSOCK4;
        if(first == 5) return PTSOCK5;
        JELog("Bad sock request. first=%c",first);
        return PTErrorSOCK5;
    }
    else
    {
        if(cached_const.stricmp("GET ",4)==0 ||
                cached_const.stricmp("POST ",5)==0)
        {
            return PTHTTP;
        }
        else if(cached_const.stricmp("CONNECT ",8)==0)
        {
            return PTHTTPS;
        }
        else
        {
            JELog("Bad HTTP request.%s",cached_const.c_str());
            return PTErrorHTTP;
        }
    }
}

void XProxyAssistant::ReplyClient(ProxyClientInfo *info,
                                  LordProxyType type, int code)
{
    bool bClose = true;
    const char *resp = NULL;
    uint32_t respLen = 0;
    if(type == PTHTTP || type == PTHTTPS)
    {
        switch(code)
        {
        case 200:
            bClose = false;
            resp = "HTTP/1.1 200 Connection Established\r\n\r\n";
            break;
        case 400:
            resp = "HTTP/1.1 400 Bad Request\r\n\r\n";
            break;
        case 500:
            resp = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
            break;
        case 502:
            resp = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
            break;
        case 504:
            resp = "HTTP/1.1 504 Gateway Timeout\r\n\r\n";
            break;
        default:
            JAssert(false);
            return;
        }
        if(code == 200) JDLog("Reply %s",resp);
        else            JELog("Reply %s",resp);
        strcpy(responseBuff_,resp);
        respLen = (uint32_t)strlen(responseBuff_);
    }
    else if(type == PTSOCK4)
    {
        memset(responseBuff_,0,8);
        //responseBuff_[0] = 4;  //居然要的是0
        responseBuff_[1] = code;
        respLen = 8;
        if(code == 90)
        {
            bClose = false;
            JAssert(info->cached.length() == 6);
            memcpy(responseBuff_+4,info->cached.c_str(),4);
            memcpy(responseBuff_+2,info->cached.c_str()+4,2);
        }
    }
    else if(type == PTSOCK5)
    {  //0~9是协议定义的, 80以后为我定义的.
        memcpy(responseBuff_,"\x05\x00\x00\x01\xAA\xAA\xAA\xAA\xBB\xBB",10); //先占位.
        switch(code)
        {
        case 0:
            bClose = false;
            memcpy(responseBuff_+4,info->cached.c_str(),4);
            memcpy(responseBuff_+8,info->cached.c_str()+4,2);
            respLen = 10;
            break;
        case 80: //第一包返回
            bClose = false;  //"\x05\x00" 无密码.
            respLen = 2;
            break;
       // case -1: //无返回断开
       //    respLen = 0;    JAssert(bClose==true);
       //     break;
        default:
            JAssert(code < 80 && code !=0);
            responseBuff_[1] = (uint8_t)code;
            respLen = 3;
            break;
        }
    }
    else
    {
        JAssert(false);
        return;
    }

    //uint32_t len = (uint32_t)strlen(responseBuff_);
    cb_->ProxyDoReplyClient(info,responseBuff_,respLen,bClose);
}

int XProxyAssistant::GetHttpContentLength(const JinString &head, size_t &idx)
{
    JinString contentlength;
    size_t ctidx = head.find("\r\nContent-Length: ",0,true);
    if(STRNPOS == ctidx) return -1;  //不允许不提供content-length.
    ctidx += 18;  //18 is strlen "\r\nContent-Length: "
    idx = ctidx;
    size_t endidx = head.find("\r\n",ctidx);
    if(STRNPOS == endidx) return 0;
    contentlength = head.mid(ctidx,endidx-ctidx);
    if(contentlength==0)return -1;
    return atoi(contentlength.c_str());
}


//CONNECT 从 CONNECT之后获取host，带端口的.
//GET 从 Host: 后获取host，带端口的.
bool XProxyAssistant::GetHttpHeadHost(const JinString &head, LordProxyType proxytype,
                                      JinString &target, JinNetPort &port)
{
    JinString fullTarget;
    JAssert(proxytype > PTContinueRecv);
    size_t hostidx,endidx;
    if(proxytype == PTHTTP)
    {
        hostidx = head.find("\r\nHost: ",0,true);  //host不是第一行,后面有空格,大小写不敏感
        if(STRNPOS == hostidx) return false;  //没有host 参数错误.
        hostidx += 8;  //8 is strlen "\r\nHost: "
        endidx = head.find("\r\n",hostidx);
        JAssert(endidx != STRNPOS);
        fullTarget = head.mid(hostidx,endidx-hostidx);
        port = JinNetPort(80);
    }
    else if(proxytype == PTHTTPS)
    {
        hostidx = head.find("CONNECT ",0,true);
        if(STRNPOS == hostidx || hostidx != 0) return false;
        hostidx += 8;  //8 is strlen "CONNECT "
        endidx = head.find(" ",hostidx);
        JAssert(endidx != STRNPOS);
        fullTarget = head.mid(hostidx,endidx-hostidx);
        port = JinNetPort(443);
    }
    else
    {
        JAssert(!"not implement.");
        return false;
    }

    endidx = fullTarget.find(":");
    if(STRNPOS != endidx)
    {
        uint16_t iport = atoi(fullTarget.c_str()+endidx+1);
        port = JinNetPort(iport);
        fullTarget = fullTarget.left(endidx);
    }
    target = fullTarget;
    return (target.length()>0);
}

int XProxyAssistant::ChangeGETProxyHead(JinString &head, const JinString &target)
{  //修改GET从绝对url变相对,去掉Proxy-Connection字段
    uint32_t line = 0;
    size_t startIdx = 0;
    JinString newHead;

    do
    {
        size_t endIdx = head.find("\r\n", startIdx);
        if(endIdx == STRNPOS) break;
        if(startIdx == endIdx){  //这是\r\n\r\n二连的时候.
            if(startIdx != 0){
                JDLog("newHead: \n%s",newHead.c_str());
                //newHead += (head.c_str()+startIdx);
                newHead.cat(head.c_str()+startIdx,head.length()-startIdx);
                head = newHead;
                return 1; //不够严格.
            }
            return -1;
        }
        //获取HTTP头每一行
        JinString lineTxt(head.c_str()+startIdx, endIdx-startIdx);
        //每一行的第一段
        size_t blankIdx = lineTxt.find(" ");
        if(blankIdx == STRNPOS) return -2;
        ++line;
        if(line == 1)
        {
            if(lineTxt.stricmp("http://",7,blankIdx+1)==0)
            {
                if(lineTxt.stricmp(target.c_str(),target.length(),blankIdx+1+7)==0)
                {
                    size_t sub = blankIdx+1+7+target.length();
                    newHead = lineTxt.left(blankIdx+1);
                    if(lineTxt.c_str()[sub]==':')
                    {
                        sub = lineTxt.find("/",sub);
                        JAssert(sub != STRNPOS);
                    }
                    JAssert(lineTxt.c_str()[sub]=='/');
                    //if(lineTxt.c_str()[sub]!='/') newHead.cat("/",1);
                    newHead += (lineTxt.c_str()+sub);
                    newHead += "\r\n";
                    startIdx += lineTxt.length()+2;  //+2 is \r\n
                    continue;
                }
            }
            //未进一步检查.
            newHead += lineTxt;
            newHead += "\r\n";
        }
        else
        {
            const char* ProxyConnMatch = "Proxy-Connection:";
            const size_t proxyconnmatchlen = 17;
            JAssert(proxyconnmatchlen == strlen( ProxyConnMatch ));
            if(blankIdx != proxyconnmatchlen ||
                    lineTxt.stricmp(ProxyConnMatch,proxyconnmatchlen)!=0)
            {
                newHead += lineTxt;
                newHead += "\r\n";
            }
        }
        startIdx += lineTxt.length()+2;  //+2 is \r\n
    }
    while(true);
    return -8;
}

