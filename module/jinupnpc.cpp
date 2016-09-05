#include "jinupnpc.h"
#include "miniupnpc.h"
#include "upnpcommands.h"
#include "jinlogger.h"
#include "jinassert.h"

#define DEFAULT_UPNP_DESCNAME "byZeyu_"
#define DEFAULT_UPNP_OUTERPPORT_BASE (7638)
#define DEFAULT_UPNP_DISCOVER_WAIT (5000)

class UPNPDroppableData
{
public:
    volatile int ref_;
    volatile int state_;
    volatile bool exitFlag_; //退出标志
    JinNetAddr lanAddr_;   //获取得到自己的局域地址.
    JinNetAddr outerAddr_; //获得路由设备出口地址.
    JinNetPort outerPort_; //绑定成功的出口端口.
    JinNetPort innerSet_;  //需要设置,用于做映射的input
};

JinUPnPc::JinUPnPc()
    : handle_(0)
    , data_(NULL)
{
    data_ = new UPNPDroppableData;
    if(data_)
    {
        data_->ref_ = 1;
        data_->state_ = JUS_IDLE;
        data_->exitFlag_ = false;
    }
}

JinUPnPc::~JinUPnPc()
{
    if(data_)
    {
        if(--data_->ref_ == 0) delete data_;
        else data_->exitFlag_ = true;
    }
}

JinNetAddr JinUPnPc::GetLanAddr()
{
    return data_->lanAddr_;
}

JinNetAddr JinUPnPc::GetOuterAddr()
{
    return data_->outerAddr_;
}

JinNetPort JinUPnPc::GetOuterPort()
{
    return data_->outerPort_;
}

JinNetPort JinUPnPc::GetInnerPort()
{
    return data_->innerSet_;
}

void JinUPnPc::OpenPort(const JinNetPort &innerPort)
{
    JAssert(data_ && data_->state_ != JUS_DOING);
    JDLog("attempt open port:%hu",innerPort.port());
    if(data_ == NULL || data_->state_ == JUS_DOING) return;
    data_->innerSet_ = innerPort;
    data_->state_ = JUS_DOING;
    JAssert(handle_==0);
    ++ data_->ref_;
    if(0!=JinThread::create(OpenUPNPDroppable,(void*)data_,handle_))
    {
        JELog("Thread UPNPDroppable create failed.");
        -- data_->ref_;
        data_->state_ = JUS_FAILED;
        return;
    }
}

THREADFUNCRTN JinUPnPc::OpenUPNPDroppable(THREADFUNCPARAM)
{
    UPNPDroppableData* udata = (UPNPDroppableData*)arguments;
    struct UPNPDev * devlist = 0;
    struct UPNPUrls urls;
    struct IGDdatas data;
    int error = 0;
    char upnpBuffer[300];
    memset(upnpBuffer,0,300);
    char* iaddr = upnpBuffer + 0;
    char* iport = iaddr + 64;
    char* eaddr = iport + 10;
    char* eport = eaddr + 40;
    char* proto = eport + 10;
    char* desc = proto + 6;
    char* remoteHost = desc + 40;
    char* intClient = remoteHost + 40;
    char* intPort = intClient + 16;
    char* duration = intPort + 6;  //+16
    strcpy(proto,"UDP");
    unsigned short outerPort = DEFAULT_UPNP_OUTERPPORT_BASE;
    unsigned short innerPort = udata->innerSet_.port();

    if(!udata->exitFlag_)
    {
        devlist = upnpDiscover(DEFAULT_UPNP_DISCOVER_WAIT, NULL, NULL, 0, 0, 2, &error);
        if(!devlist){
            error += 10000;
            goto returnatlast2;
        }

        //这里返回1,2,3代表不同的含义,但是只要非0都继续
        if (0==UPNP_GetValidIGD(devlist, &urls, &data, iaddr, 64)){
            error = 20000;
            goto returnatlast1;
        }
        else
        {
            udata->lanAddr_ = JinNetAddr(iaddr);
            //根据自己的ip算一个合适的端口
            unsigned int p1,p2,p3=1,p4=1;
            sscanf(iaddr,"%u.%u.%u.%u",&p1,&p2,&p3,&p4);
            //outerPort = (unsigned short)((100+p3)*0x100+p4);
            outerPort += (p1*0x1010+p2*0x101+p3*0x100+p4)  % 8888;
        }

        if(0!=(error=UPNP_GetExternalIPAddress(urls.controlURL,data.first.servicetype,eaddr))
                || memcmp(eaddr,"0.0.0.0",7)==0) //实测中发现有返回外部ip 0.0.0.0的情况,算失败.应该是该路由没有出口
        {
            error += 30000;
            goto returnatlast;
        }
        udata->outerAddr_ = JinNetAddr(eaddr);
        //考虑我们每次端口不一样，upnp直接覆盖最好。这一点不同于以前的云4
        //实测发现已存在记录的情况会返回718错误码，覆盖失败
        sprintf(eport,"%u",outerPort);
        sprintf(iport,"%u",innerPort);
        error = UPNP_GetSpecificPortMappingEntry(urls.controlURL, data.first.servicetype, eport, proto, remoteHost,
                                                 intClient, intPort, NULL/*desc*/, NULL/*enabled*/, duration);
        if(error==UPNPCOMMAND_SUCCESS) //已经有记录 且不等的要抹掉.
        {
            if(strcmp(iport,intPort)!=0)
            {
                JSLog("UPNP try-delete %s|%s => %s|%hu",iaddr,intPort,eaddr,outerPort);
                error = UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, eport, proto, remoteHost);
                if(error!=UPNPCOMMAND_SUCCESS)
                {
                    JSLog("delete failed, ignore!");
                }
            }
            else
            {
                JSLog("UPNP using exist %s|%s => %s|%hu",iaddr,intPort,eaddr,outerPort);
                goto returnatlast;
            }
        }

        JSLog("UPNP try-open %s|%s => %s|%s",iaddr,iport,
              eaddr,eport);
        sprintf(desc,"%s%s",DEFAULT_UPNP_DESCNAME,iaddr);
        error = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,eport, iport, iaddr, desc, proto, 0, NULL);
        if(error!=UPNPCOMMAND_SUCCESS){
            error += 40000;
            goto returnatlast;
        }
        JSLog("Success Add UPNP-record %s:%s -> %s:%s %s\nDesc: %s ",eaddr,eport,iaddr,iport,proto,desc);
        udata->outerPort_ = JinNetPort(outerPort);
        if(!udata->exitFlag_)
        { //再确认一下
            int trys = UPNP_GetSpecificPortMappingEntry(urls.controlURL,data.first.servicetype,eport, proto, remoteHost, intClient, intPort, NULL/*desc*/,NULL/*enabled*/, duration);
            JSLog("UPNP-record confirm=%d",(trys==UPNPCOMMAND_SUCCESS)?1:0);
        }

returnatlast:
        FreeUPNPUrls(&urls);
returnatlast1:
        freeUPNPDevlist(devlist); devlist = 0;
returnatlast2:
        JSLog("UPNP-open error=%d,%d",error/10000,error%10000);
        udata->state_ = (error==0)?JUS_DONE:JUS_FAILED;
    }
    if(--udata->ref_ == 0)
    {
        delete udata;
    }
    else
    {
        udata->exitFlag_ = true;
    }

    return (THREADFUNCRTN)error;
}
