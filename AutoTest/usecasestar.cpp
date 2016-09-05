#include "usecasestar.h"
#include "jinassistthread.h"
#include "jinchart.h"
#include "jinchartbucket.h"
#include "jinchartbuckets.h"
#include "jinchartstore.h"
#include "jinchartping.h"
#include "jinchartrpc.h"
#include "jinstring.h"

const static int kChartWalkonRoleCount = 1;

CHAIN_USE_CASE(Star);

static bool gIsRun = true;

UseCaseStar::UseCaseStar()
    : engineFlag(false)
    , walkonStart(0)
    , chart(NULL)
    , walkon(NULL)
{
    errmsg_[0] = 0;
}

UseCaseStar::~UseCaseStar()
{

}

bool UseCaseStar::run()
{
    bool bRet = false;
    if(before())
    {
        bRet = doing();
    }
    after();

    return bRet;
}

void UseCaseStar::timeout(TimerID id)
{
    if(id == exitTimer)
    {
        exitTimer = 0;
        gIsRun = false;
    }
    else if(id == walkonTimer)
    {
        if(walkonStart < kChartWalkonRoleCount)
        {
            walkon[walkonStart].dbgIdx_ = walkonStart + 1;
            bool ret = walkon[walkonStart].init(0);
            if(!ret)
            {
                JELog("walkon chart %d init failed.",walkonStart);
                gIsRun = false;
            }
            ++walkonStart;
        }
        else
        {
            chart = new JinChart;
            JDLog("chart init start. chart=%p",chart);
            if(!chart || !chart->init(0))
            {
                JELog("chart %d init failed.",walkonStart);
                gIsRun = false;
            }
            JinMainThread::Get()->getTimer()->cancel(walkonTimer);
            walkonTimer = 0;
        }
    }
    else if(id == searchTimer)
    {  //4f373be946cc3dc739cc7198f2af030d653b55ad     4d487afb27aa1e260c9bfc6308dc37f13847bf74
        //JinString hash = JinString::fromReadableHex("46e8ad8e36de53bc6d6e092801748ccab14a159b",40);
        searchTimer = 0;
        JinString hash = JinString::fromReadableHex("8888888888888888888888888888888888888888",40);
        JinKey160 hashKey((const unsigned char*)hash.c_str());
        tranID = chart->search(hashKey, OnStarFound, this);
    }
}

bool UseCaseStar::OnStarFound(const JinKey160 &key, JinNetAddr addr, JinNetPort port, void *user)
{
    JDLog("*found* key=%s addr=%s|%hu user=%p",key.toString(),addr.toAddr(gDLogBufAddr),port.port(),user);
    return false;
}

bool UseCaseStar::before()
{
    if(!testBuckets())return false;
    if(!testStore())return false;
    if(!testKrpc())return false;

    bool bRet;
    bRet = initAsyncEngine();
    CHECK_ASSERT(bRet==true,"initAsyncEngine error");
    engineFlag = true;
    walkon = new JinChart[kChartWalkonRoleCount];
    return JinMainThread::Get()->init();
}

bool UseCaseStar::doing()
{
    chart = NULL;
    walkonStart = 0;
    walkonTimer = JinMainThread::Get()->getTimer()->
            start(3*1000,true,sTimeout,this);
    exitTimer = JinMainThread::Get()->getTimer()->
            start(120*1000,false,sTimeout,this);
    searchTimer = JinMainThread::Get()->getTimer()->
            start(60*1000,false,sTimeout,this);

    while(gIsRun)
    {
        while(driveAEngine()){}
        JinMainThread::msleep(10);
    }

    return true;
}

void UseCaseStar::after()
{
    if(walkonTimer) JinMainThread::Get()->getTimer()->cancel(walkonTimer);
    if(exitTimer) JinMainThread::Get()->getTimer()->cancel(exitTimer);
    if(searchTimer) JinMainThread::Get()->getTimer()->cancel(searchTimer);
    if(walkon) delete [] walkon;
    if(chart) delete chart;

    JinMainThread::Get()->uninit();
    if(engineFlag)
    {
        uninitAsyncEngine();
    }
}

void UseCaseStar::sTimeout(TimerID id, void *user, uint64_t user64)
{
    UseCaseStar *star = (UseCaseStar*)user;
    star->timeout(id);
}

bool UseCaseStar::testBuckets()
{
    int expectCount = 0,expectCount1 = 0;
    JinKey160 myself = JinKey160::random();
    JPLog("myself key:%s",myself.toString());
    JinChartBucket map(myself);
    JinChartBuckets buckets1;
    CHECK_ASSERT(buckets1.init(myself,1),"buckets1 init failed!");
    JinKey160 keys[600];
    int keyCurIdx = 0;
    for(int i=0;i<600;i++)
    {
        bool mapAddSucc = false;
        bool bucAddSucc = false;
        keys[keyCurIdx] = JinKey160::random();
        JinKey160 &key = keys[keyCurIdx];
        if(myself.samePrefixBit(key)>0)
        {
            mapAddSucc = map.add(&key);
            if(mapAddSucc){
                ++expectCount;
            }
        }
        bucAddSucc = buckets1.add(&key);
        if(mapAddSucc) //如果mapadd 成功，bucket一定成功.
        {
            CHECK_ASSERT(bucAddSucc,"buckets1 add failed.");
        }
        if(bucAddSucc)
        {
            ++expectCount1;
        }
        if(bucAddSucc || mapAddSucc) ++keyCurIdx;
    }
    CHECK_ASSERT(map.size() == expectCount,"count error!");
    CHECK_ASSERT(buckets1.size() == expectCount1,"count1 error!");

    for(int i=6;i<8;i++)
    {
        JinKey160 key = JinKey160::random(myself.rawBuf(),i);
        JPLog("find-key:%s",key.toString());
        const JinKey160* items[8];
        size_t sz = buckets1.getKeyNearby(key ,items,8);
        for(size_t j=0;j<sz;j++)
        {
            JPLog("[%02d]*key*:%s",(int)j,items[j]->toString());
        }
    }
    {
        unsigned char head = *myself.rawBuf();
        JinKey160 A = JinKey160::random(&head,1);
        head^=0x80;
        JinKey160 B = JinKey160::random(&head,1);
        const JinKey160* items[16];
        size_t sz = 0;
        JPLog("find-keyA:%s",A.toString());
        sz = buckets1.getKeyNearby(A ,items,16);
        CHECK_ASSERT(sz>8,"getKeyNearbyA not >8");
        for(size_t j=0;j<sz;j++)
        {
            JPLog("[%02d]*key*:%s",(int)j,items[j]->toString());
        }
        JPLog("find-keyB:%s",B.toString());
        sz = buckets1.getKeyNearby(B ,items,16);
        CHECK_ASSERT(sz>8,"getKeyNearbyB not >8");
        for(size_t j=0;j<sz;j++)
        {
            JPLog("[%d]*key*:%s",(int)j,items[j]->toString());
        }
    }

    printf("\n");
    for(int i = 0 ;i< keyCurIdx; i++)
    {
        bool mapRmvSucc = false;
        bool bucRmvSucc = false;
        printf("|");
        JinKey160& key = keys[i];
        if(myself.samePrefixBit(key)>0)
        {
            mapRmvSucc = map.remove(&key);
        }
        bucRmvSucc = buckets1.remove(&key);
        if(mapRmvSucc)
        {  //如果map移除成功，buckets一定移除成功.
            CHECK_ASSERT(bucRmvSucc,"buc remove failed.");
        }
    }
    printf("\n");
    CHECK_ASSERT(map.size() == 0, "map not empty");
    CHECK_ASSERT(buckets1.size() == 0,"buckets not empty");

    return true;
}

bool UseCaseStar::testStore()
{
    bool ret = false;
    JinChartStore store;
    JinKey160 hashKey((const unsigned char*)"hhhhhaaaaassssshhhhh");
    for(int i=0;i<60;i++)
    {
        char strbuf[12];
        memset(strbuf,0x30+i,6);
        JinBuffer buf(strbuf,6);
        ret = store.set(hashKey,buf);
        CHECK_ASSERT(ret,"store.set failed.");
    }
    JinBuffer bufJoind;
    ret = store.get(hashKey,bufJoind,50);
    CHECK_ASSERT(ret,"store.get failed1.");
    CHECK_ASSERT(bufJoind.length() == 50*6,"get return length error1.");
    for(int i=0;i<50;i++)
    {
        char strbuf[12];
        memset(strbuf,0x30+i,6);
        //CHECK_ASSERT(memcmp(bufJoind.buff()+i*6,strbuf,6)==0,"text error.");
        if(i>40)
        {
            JinBuffer buf(strbuf,6);
            ret = store.set(hashKey,buf); //虽然成功,但个数不应该增加.
            CHECK_ASSERT(ret,"store.set failed.");
        }
    }
    ret = store.get(hashKey,bufJoind,70);
    CHECK_ASSERT(ret,"store.get failed2.");
    CHECK_ASSERT(bufJoind.length() == 60*6,"get return length error2.");

    return true;
}

bool UseCaseStar::testKrpc()
{
    ChartRpcPacket *kpack=0;
    JinChartRpc krpc;

    JinKey160 selfid((unsigned char*)"xxxxxxxxxxoooooooooo");
    krpc.setSelfID(selfid);
    ChartRpcBuf *kbuf;
    const char* buf;
    size_t size;

    ChartRpcBuf bTranID;
    //bTranID.buffer = NULL; bTranID.len = 0;
    uint16_t nTranID = JinRandom::rnd()%0xFFFF;
    bTranID.buffer = (const unsigned char*)&nTranID;
    bTranID.len = 2;

    kbuf = krpc.ping();
    CHECK_ASSERT(kbuf,"construct ping failed.");
    kpack = krpc.handle_packet(kbuf->buffer,kbuf->len);
    CHECK_ASSERT(kpack,"handle ping failed.");
    CHECK_ASSERT(kpack->isQuery == true,"ping is query.");
    CHECK_ASSERT(kpack->type == ChartRpcPacket::kPing,"ping type.");
    CHECK_ASSERT(kpack->tranID16() == krpc.lastTranID(),"tranID error.");
    kpack->paramBuf(PbkID,buf,size);
    CHECK_ASSERT(buf&&size==20&&memcmp(selfid.rawBuf(),buf,20)==0,"ID of ping");
    krpc.free_packet(kpack);

    kbuf = krpc.pong(&bTranID);
    CHECK_ASSERT(kbuf,"construct pong failed.");
    kpack = krpc.handle_packet(kbuf->buffer,kbuf->len);
    CHECK_ASSERT(kpack,"handle pong failed.");
    CHECK_ASSERT(kpack->isQuery == false,"pong not query.");
    CHECK_ASSERT(kpack->type == ChartRpcPacket::kPong,"pong type.");
    CHECK_ASSERT(kpack->tranID16() == ChartRpcPacket::tranID16(&bTranID),"tranID error.");
    kpack->paramBuf(PbkID,buf,size);
    CHECK_ASSERT(buf&&size==20&&memcmp(selfid.rawBuf(),buf,20)==0,"ID of pong");
    krpc.free_packet(kpack);

    JinKey160 target((unsigned char*)"===<<<***--***>>>===");
    size_t valuelen = 78;
    char value[78];  //26余6的公倍数78
    memset(value,'.',valuelen);

    kbuf = krpc.find_node(target);
    CHECK_ASSERT(kbuf,"construct find_node failed.");
    kpack = krpc.handle_packet(kbuf->buffer,kbuf->len);
    CHECK_ASSERT(kpack,"handle find_node failed.");
    CHECK_ASSERT(kpack->isQuery == true,"find_node is query.");
    CHECK_ASSERT(kpack->type == ChartRpcPacket::kFindNode,"find_node type.");
    CHECK_ASSERT(kpack->tranID16() == krpc.lastTranID(),"tranID error.");
    kpack->paramBuf(PbkID,buf,size);
    CHECK_ASSERT(buf&&size==20&&memcmp(selfid.rawBuf(),buf,20)==0,"ID of find_node");
    kpack->paramBuf(PbkTarget,buf,size);
    CHECK_ASSERT(buf&&size==20&&memcmp(target.rawBuf(),buf,20)==0,"target of find_node");
    krpc.free_packet(kpack);

    kbuf = krpc.found_node(value,valuelen,&bTranID);
    CHECK_ASSERT(kbuf,"construct found_node failed.");
    kpack = krpc.handle_packet(kbuf->buffer,kbuf->len);
    CHECK_ASSERT(kpack,"handle found_node failed.");
    CHECK_ASSERT(kpack->isQuery == false,"found_node not query.");
    CHECK_ASSERT(kpack->type == ChartRpcPacket::kFoundNode,"find_node type.");
    CHECK_ASSERT(kpack->tranID16() == ChartRpcPacket::tranID16(&bTranID),"tranID error.");
    kpack->paramBuf(PbkID,buf,size);
    CHECK_ASSERT(buf&&size==20&&memcmp(selfid.rawBuf(),buf,20)==0,"ID of find_node");
    kpack->paramBuf(PbkNodes,buf,size);
    CHECK_ASSERT(size==valuelen,"nodes of found_node");
    size_t valueExpect = valuelen;
    for(size_t i=0;i<valuelen;i++)
    {
        CHECK_ASSERT(buf[i] == '.',"nodes value");
        valueExpect --;
    }
    CHECK_ASSERT(valueExpect==0,"pack value count expect.");
    krpc.free_packet(kpack);

    JinKey160 hash((unsigned char*)"O0oO0oO0oO0oO0oO0oO0");
    kbuf = krpc.get_peers(hash);
    CHECK_ASSERT(kbuf,"construct get_peers failed.");
    kpack = krpc.handle_packet(kbuf->buffer,kbuf->len);
    CHECK_ASSERT(kpack,"handle get_peers failed.");
    CHECK_ASSERT(kpack->isQuery == true,"get_peers is query.");
    CHECK_ASSERT(kpack->type == ChartRpcPacket::kGetPeers,"get_peers type.");
    CHECK_ASSERT(kpack->tranID16() == krpc.lastTranID(),"tranID error.");
    kpack->paramBuf(PbkID,buf,size);
    CHECK_ASSERT(buf&&size==20&&memcmp(selfid.rawBuf(),buf,20)==0,"ID of get_peers");
    kpack->paramBuf(PbkInfoHash,buf,size);
    CHECK_ASSERT(buf&&size==20&&memcmp(hash.rawBuf(),buf,20)==0,"hash of get_peers");
    krpc.free_packet(kpack);

    unsigned char token[4];
    memset(token,'t',4);

    kbuf = krpc.got_peers(true,token,value,valuelen,&bTranID);
    CHECK_ASSERT(kbuf,"construct got_peers failed.");
    kpack = krpc.handle_packet(kbuf->buffer,kbuf->len);
    CHECK_ASSERT(kpack,"handle got_peers failed.");
    CHECK_ASSERT(kpack->isQuery == false,"got_peers not query.");
    CHECK_ASSERT(kpack->type == ChartRpcPacket::kGotPeerV,"got_peers type.");
    CHECK_ASSERT(kpack->tranID16() == ChartRpcPacket::tranID16(&bTranID),"tranID error.");
    kpack->paramBuf(PbkID,buf,size);
    CHECK_ASSERT(buf&&size==20&&memcmp(selfid.rawBuf(),buf,20)==0,"ID of got_peers");
    kpack->paramBuf(PbkToken,buf,size);
    CHECK_ASSERT(buf&&size==4&&memcmp(token,buf,4)==0,"token of got_peers");
    _BNode* node = kpack->paramValue(buf,size);
    while(node)
    {  //checkvalues...
        CHECK_ASSERT(size==6,"peer value size6");
        node = kpack->paramValueNext(node,buf,size);
    }
    krpc.free_packet(kpack);

    kbuf = krpc.got_peers(false,token,value,valuelen,&bTranID);
    CHECK_ASSERT(kbuf,"construct got_peers2 failed.");
    kpack = krpc.handle_packet(kbuf->buffer,kbuf->len);
    CHECK_ASSERT(kpack,"handle got_peers2 failed.");
    CHECK_ASSERT(kpack->isQuery == false,"got_peers2 not query.");
    CHECK_ASSERT(kpack->type == ChartRpcPacket::kGotPeerP,"got_peers2 type.");
    CHECK_ASSERT(kpack->tranID16() == ChartRpcPacket::tranID16(&bTranID),"tranID error.");
    kpack->paramBuf(PbkID,buf,size);
    CHECK_ASSERT(buf&&size==20&&memcmp(selfid.rawBuf(),buf,20)==0,"ID of got_peers2");
    kpack->paramBuf(PbkToken,buf,size);
    CHECK_ASSERT(buf&&size==4&&memcmp(token,buf,4)==0,"token of got_peers");
    kpack->paramBuf(PbkNodes,buf,size);
    CHECK_ASSERT(buf&&size==valuelen&&memcmp(value,buf,valuelen)==0,"buf nodes of got_peers");
    krpc.free_packet(kpack);

    kbuf = krpc.got_peers(false,token,NULL,0,&bTranID);
    CHECK_ASSERT(kbuf,"construct got_peers3 failed.");
    kpack = krpc.handle_packet(kbuf->buffer,kbuf->len);
    CHECK_ASSERT(kpack,"handle got_peers3 failed.");
    kpack->paramBuf(PbkNodes,buf,size);
    CHECK_ASSERT(buf&&size==0,"buf nodes of got_peers2");
    krpc.free_packet(kpack);

    int64_t num = 0;
    kbuf = krpc.announce_peer(hash,6881,token,4);
    CHECK_ASSERT(kbuf,"construct announce_peer failed.");
    kpack = krpc.handle_packet(kbuf->buffer,kbuf->len);
    CHECK_ASSERT(kpack,"handle announce_peer failed.");
    CHECK_ASSERT(kpack->isQuery == true,"announce_peer is query.");
    CHECK_ASSERT(kpack->type == ChartRpcPacket::kGetPeers,"announce_peer type.");
    CHECK_ASSERT(kpack->tranID16() == krpc.lastTranID(),"tranID error.");
    kpack->paramBuf(PbkID,buf,size);
    CHECK_ASSERT(buf&&size==20&&memcmp(selfid.rawBuf(),buf,20)==0,"ID of announce_peer");
    kpack->paramBuf(PbkInfoHash,buf,size);
    CHECK_ASSERT(buf&&size==20&&memcmp(hash.rawBuf(),buf,20)==0,"hash of announce_peer");
    kpack->paramBuf(PbkToken,buf,size);
    CHECK_ASSERT(buf&&size==4&&memcmp(token,buf,4)==0,"token of announce_peer");
    kpack->paramNum(PnkImpliedPort,num);
    CHECK_ASSERT(num==1,"announce_peer implied_port");
    kpack->paramNum(PnkPort,num);
    CHECK_ASSERT(num==6881,"announce_peer port");
    krpc.free_packet(kpack);

    const char* testPacket = "d2:ip6:???R??1:rd2:id20:?5?94????e????u???F?5:nodes0:e1:t2:O?1:v4:UT?j1:y1:re";
    kpack = krpc.handle_packet(testPacket,strlen(testPacket));
    CHECK_ASSERT(kpack,"handle testPacket failed.");
    krpc.free_packet(kpack);

    const char* testPacket2 = "d2:ip6:??????1:rd2:id20:xxxxxcccccvvvvvbbbbb5:nodes208:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef5:token20:xxxxxcccccvvvvvbbbbbe1:t2:I?1:v4:UT?j1:y1:re";
    kpack = krpc.handle_packet(testPacket2,strlen(testPacket2));
    CHECK_ASSERT(kpack,"handle testPacket2 failed.");
    krpc.free_packet(kpack);

//    const char* testPacket3 = "d1:q9:get_peers1:rd2:id20:,????@c??V???X???]t?5:token20:,????@c??V???X???]t?5:value208:,#?(??????G????,?n?mb???nM,?????????e}o?????^Z$??:2?,??^???j??????????sk^?#?#?,?????S???kW?'?70$?d???wN?,?D?^???c?r?G?????j?I5?q??,?m?y?}?C???|@?n?gZBi??V,?,?=?????????*????X??ZP???a,??E????g???;???????1????.e1:t2:??1:y1:re";
//    kpack = krpc.handle_packet(testPacket3,strlen(testPacket3));
//    CHECK_ASSERT(kpack,"handle testPacket3 failed.");
//    krpc.free_packet(kpack);

    kbuf = krpc.error(kProtocolError,&bTranID);
    CHECK_ASSERT(kbuf,"construct error failed.");
    kpack = krpc.handle_packet(kbuf->buffer,kbuf->len);
    CHECK_ASSERT(kpack,"handle error failed.");
    CHECK_ASSERT(kpack->isQuery == false,"error not query.");
    CHECK_ASSERT(kpack->type == ChartRpcPacket::kError,"error type.");
    CHECK_ASSERT(kpack->tranID16() == ChartRpcPacket::tranID16(&bTranID),"tranID error.");
    num = kpack->errCode(buf,size);
    CHECK_ASSERT(num==kProtocolError,"error code.");
    krpc.free_packet(kpack);

    kpack = NULL;

    return true;
}
