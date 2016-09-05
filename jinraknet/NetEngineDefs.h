#ifndef NETENGINE_DEFS_H
#define NETENGINE_DEFS_H

//#define NETWORKID_RESERVER					100		//小于这个数的NetID认为不是正常iD
#define DEFAULT_MAX_CONNECTIONS			100
#define CONNECTION_PASSWORD "??12aM(u<$z[%tV??13aM(u<$z[%tV-,p`%wDpm-K"
#define CONNECTION_PASSWORDLENGTH  (sizeof(CONNECTION_PASSWORD)-1)
#define COORDINATER_PASSWORD "?SVG  ?????XMI   12@#$%^&*(XMPP"

//#pragma pack(push,1)

//struct NetEngineConnection  //连接接受/失败/丢失时的数据包
//{
//    unsigned char PacketType0;
//    ConnID id;
//};
//#pragma pack(pop)

#endif // NETENGINE_DEFS_H

