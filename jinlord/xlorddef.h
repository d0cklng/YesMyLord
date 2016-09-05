#ifndef XLORD_DEF_H
#define XLORD_DEF_H

//验证以下url来验证代理:
// 百度首页; https://www.baidu.com ;
// 登录百度; www.baidu.com
// 百度新闻; http://news.baidu.com/
// 百度糯米; http://www.nuomi.com/?cid=002540 看刷图
// 自主约考与登录; http://cgs1.stc.gov.cn:8088/kszzyy/id.html

static const char* const kLorsDefaultAddr = "qd.jinzeyu.cn";
static const unsigned short kLorsDefaultPort = 7913;
static const int kIdentityLength = 64;
/*  # outer contact (admit<==>finder)协议 #
 *  #abcdefghi jklmnopqrs tuvwxyz#
 *  #123456789 0123456789 0123456
 *
 *  Finder  ===>   Admit
 * 1.OpenTo somewhere, 如果有data,连接目的成功后直接发过去.
 *  param: MainID=F2A|AsstID=OpenTo(1520)|tunnelid(209)=1|target(20)='www.google.com'|port(16)=80|data
 *  return: MainID=A2F|AsstID=RtnOpenTo(8152)|tunnelid(209)=1|code(3)=0|msg(13)='OK'
 * 2.CloseTunnel
 *  param: MainID=F2A|AssID=CloseTunnel(320)|tunnelid(209)=1
 *  {no return}
 * 3.FlowControl  双向流量拥塞控制 先从finder->admit
 *  param: MainID=both|AsstID=FlowAck(601)|ack(1)=128 64bit
 * 4.Greeting 打招呼 协商一些东西，但不限定为必须动作. 总是由Finder先发起.
 *  param: MainID=both|AsstID=Greeting(7)|identity(9)|compress(306) 此处必须是string包含多种可选.
 *
 *  Admit ===> Finder
 * 1.ConnDone  后面加的地址和端口用于SOCK5
 *  param: MainID=A2F|AsstID=ConnDone(304)|tunnelid(209)=1|code(3)=0|msg(13)='OK'|ipv4|port
 *  {no return}
 * 2.ConnAbort
 *  param: MainID=A2F|AsstID=ConnAbort(301)|tunnelid(209)=1|msg(13)='OK'
 *  {no return}
 *
 *  双向 <==>
 * 1.DataTransfer 可靠传输可能使用加密，若加密会使用同一本字典.compress必须是8bit或无
 *  param:MainID|AsstID|tunnelid|compress|data
 * 2.Ack
 *  param:MainID|AsstID|kUkAck
 *
 */
enum XlorsMainID
{   // xx2xx
    kMainF2A = 6201,
    kMainA2F = 1206,

    kMainO2S = 15219,
    kMainS2O = 19215,
};

enum XlorsAsstID
{
    kAsstOpenTo = 1520,
    kAsstCloseTunnel = 320,
    kAsstFlowCtrl = 603,
    kAsstGreeting = 7,
    //kAsstRtnGreeting = 807,

    kAsstDataTransfer = 420,

    kAsstRtnOpenTo = 8152,
    kAsstConnDone = 304,
    kAsstConnAbort = 301,

    //outer <--> server
    kAsstLogin = 1209,
    kAsstRtnLogin = 8120,

    kAsstFind = 6,
    kAsstRtnFind = 806,
};

enum XlorsUserKey
{
    kUkTarget = 20,   //v
    kUkPort = 16,     //16bit
    kUkTunnelID = 209,//16bit
    kUkAck = 1,       //64bit
    kUkErrorCode = 3, //16bit
    kUkMessage = 13,  //v
    kUkData = 4,      //v
    kUkCompress = 35, //8bit char. l=7z/lzma z=zip q=quicklz r=rar

    // Outer <---> Server
    kUkCert = 5,      //v
    kUkPubKey = 17,   //v
    kUkIdentity = 9,  //v
    kUkNetID = 14,    //64bit
    kUkIPv4 = 10,     //32bit
};

enum XlorsErrorCode
{
    kEcSuccess = 0,
    kEcOutOfMemory = 1,
    kEcInternalError = 4,
    kEcReachMaximum = 10,
    kEcConnFailed = 11,
    kEcTunnidExist = 205,

    // Outer <---> Server
    kEcCertFailed = 36,  //验证失败.
    kEcIdentityExist = 5, //已存在 重复了.
    kEcIdentityInvalid = 9,  //通常是长度问题.
    kEcIdentityNotFound = 146,
};



/*  #outer finder/admit 与 server的通信 协议#
 *  #abcdefghi jklmnopqrs tuvwxyz#
 *  #123456789 0123456789 0123456
 *
 *    ===> server
 *  1.Login           服务器给的证书 自己公钥 名字
 *   param: O2S|Login|cert-of-cpk|c-pubkey|identity  #identity将被验证
 *   return: S2O|RtnLogin|ipv4|port|code(3)=0|msg(13)='OK'
 *
 *  2.Find
 *   param: O2S|Find|identity
 *   return: S2O|RtnFind|NetID|ipv4|port
 *   #return S2O|RtnFind|cert-of-cpk|c-pubkey|identity|code(3)=0|msg(13)='OK'
 */



#endif // XLORS_SHARE_H

