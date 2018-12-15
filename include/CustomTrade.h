/**
 * @file   CustomTrade.h
 * @author Ye Shiwei <yeshiwei.math@gmail.com>
 * @date   Sun May 20 22:53:50 2018
 * 
 * @brief  Custom CTP. 
 * 
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <ctime>
#include <map>
#include <set>
#include <sys/stat.h>
#include <sys/time.h>
#include <dirent.h>
#include <pthread.h>

#include "ThostFtdcMdApi.h"
#include "ThostFtdcTraderApi.h"
#include "util.h"
#include "Kline.h"
#include "KD.h"

using namespace std;

// 会员代码
TThostFtdcBrokerIDType g_chBrokerID="";
// 交易用户代码
TThostFtdcUserIDType g_chUserID="";
// 密码
TThostFtdcPasswordType Password;
//交易前置
char TradeFrontAddr[] ="tcp://180.168.146.187:10030";
//行情前置
char MarketFrontAddr[] ="tcp://180.168.146.187:10031";
//历史数据位置.
char HistoryLocation[100];
//加载历史数据天数.
int LOOKBACK_DATES = 6;
//延迟（秒）,接收到tick数据的时间差. 用于控制分钟线策略执行时间.
double DelaySeconds = 1;
//收盘时间，离前一天21:00:00距离(以秒计)。
int MARKET_CLOSE;
int PrintTick = 0;
int CloseAll = 0;
//持仓，订单初始化完成标志。
int StatusInit = 0;
//合约信息
map<string, CThostFtdcInstrumentField> InstrumentInfo;
//合约是否初始化
map<string, int> InstrumentInitialMark;
//交易的合约代码
set<string> TradingInstruments;
//合约对应最近一次执行策略的时间点.距离21:00:00的秒数.
map<string, int> LastManageTime;
//交易时间段，以按秒距离21点的距离计算。
vector<pair<int,int> > TradingIntervals;
double TradingLevel;
//盘口信息
map<string, CThostFtdcDepthMarketDataField> LastTick;
//持仓情况.
map<string, double> Portfolio;
//未成交订单
map<string, vector<CThostFtdcOrderField> > Order;
//
set<string> ModifyOrder;

TThostFtdcOrderRefType MaxOrderRef;
int iRequestID = 0 ;
int iOrderActionRef = 0;
void *start_trader(void*data);
void *minute_task(void*data);

bool IsTradingTime(int displace) {
  for (int i = 0; i < TradingIntervals.size(); i ++ ) {
    if ( displace < TradingIntervals[i].second && displace > TradingIntervals[i].first) {
      //cout <<displace <<" is in ("<< TradingIntervals[i].first << "," << TradingIntervals[i].second <<"]"<< endl;
      return true;
    }
  }
  return false;
}

void NextOrderRef() {
  MaxOrderRef[12] = 0;
  for ( int i = 0; i < 12; i ++ ) {
    if ( MaxOrderRef[i] < '0' ) MaxOrderRef[i] = '0';
  }
  int i = 12;
  MaxOrderRef[11] ++;
  while( --i>0) {
    if ( MaxOrderRef[i] > '9' ) {
      MaxOrderRef[i] = '0';
      MaxOrderRef[i-1] ++;
    } else {
      break;
    }
  }
}

class CSimpleHandler;

class TSimpleHandler : public CThostFtdcTraderSpi
{
public:

  /** 
   * 交易回报类构造函数，需要提供交易接口指针，和行情接口指针。
   * 
   * @param pUserApi 
   * @param pUserApim 行情接口用于收到合约信息后订阅行情。
   * 
   * @return 
   */
 TSimpleHandler(CThostFtdcTraderApi *pUserApiTrader, CThostFtdcMdApi *pUserApiMd,
		CSimpleHandler *p):
  m_pUserApiTrader(pUserApiTrader), m_pUserApiMd(pUserApiMd), p_C(p) {}
  ~TSimpleHandler() {}
  // 当客户端与交易托管系统建立起通信连接，客户端需要进行登录
  virtual void OnFrontConnected()
  {
    cout << "Trader OnFrontConnected." << endl;
    CThostFtdcReqUserLoginField reqUserLogin;
    // get BrokerID
    strcpy(reqUserLogin. BrokerID, g_chBrokerID);
    // get userid
    strcpy(reqUserLogin.UserID, g_chUserID);
    // get password
    strcpy(reqUserLogin.Password, Password);
    // 发出登陆请求
    m_pUserApiTrader -> ReqUserLogin(&reqUserLogin, 0);
  }
  // 当客户端与交易托管系统通信连接断开时，该方法被调用
  virtual void OnFrontDisconnected(int nReason)
  {
    // 当发生这个情况后，API会自动重新连接，客户端可不做处理
    printf("OnFrontDisconnected.\n");
  }
  // 当客户端发出登录请求之后，该方法会被调用，通知客户端登录是否成功
  virtual void OnRspUserLogin(CThostFtdcRspUserLoginField
			      *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool
			      bIsLast)
  {
    printf("Trade OnRspUserLogin:\n");
    strcpy(MaxOrderRef, pRspUserLogin -> MaxOrderRef);
    char Msg[200];
    code_convert(pRspInfo -> ErrorMsg, strlen(pRspInfo->ErrorMsg), Msg, 200);
    printf("ErrorCode=[%d], ErrorMsg=[%s]\n",
	   pRspInfo->ErrorID, Msg);
    printf("RequestID=[%d], Chain=[%d], MaxOrderRef=[%s]\n",
	   nRequestID, bIsLast, MaxOrderRef);

    if (pRspInfo->ErrorID != 0) {
      // 端登失败，客户端需进行错误处理
      printf("Failed to login, errorcode=%d errormsg=%s requestid=%d chain=%d", pRspInfo->ErrorID, pRspInfo->ErrorMsg,
	     nRequestID, bIsLast);
      exit(-1);
    }
    ///查询交易合约
    CThostFtdcQryInstrumentField req;
    memset(&req, 0, sizeof(req));
    int r = m_pUserApiTrader->ReqQryInstrument(&req, ++iRequestID);
    cout <<"ReqQryInstrument: " << r << endl;
  }
  virtual void OnRspQryInstrument(
				  CThostFtdcInstrumentField *pInstrument,
				  CThostFtdcRspInfoField *pRspInfo,
				  int nRequestID,
				  bool bIsLast) {
    if ( CloseAll == 1 ||
	 TradingInstruments.find( pInstrument -> InstrumentID ) !=
    	 TradingInstruments.end()) {
      char *Instrument[] = {pInstrument->InstrumentID};
      m_pUserApiMd -> SubscribeMarketData (Instrument,1);
      InstrumentInfo[ pInstrument -> InstrumentID] = *pInstrument;
    }
    if (bIsLast) {
      CThostFtdcQryInvestorPositionField QryInvestorPosition;
      memset(&QryInvestorPosition, 0, sizeof(QryInvestorPosition));
      sleep(1);
      int r = m_pUserApiTrader -> ReqQryInvestorPosition( &QryInvestorPosition,
				      ++iRequestID);
      cout << "QryInvestorPosition: "<< r << endl;
    }
  }
  virtual void OnRspQryInvestorPosition(
					CThostFtdcInvestorPositionField *pPosition,
					CThostFtdcRspInfoField *pRspInfo,
					int nRequestID,
					bool bIsLast) {
    //cout << "OnRspQryInvestorPosition" << pPosition << endl;
    if ( pPosition != NULL ) {
      if ( pPosition -> PosiDirection == THOST_FTDC_PD_Long ||
	   pPosition -> PosiDirection == THOST_FTDC_PD_Net ) {
	Portfolio[ pPosition -> InstrumentID ] += pPosition -> Position;
      } else {
	Portfolio[ pPosition -> InstrumentID ] -= pPosition -> Position;
      }
      cout << "Position of "<< pPosition -> InstrumentID << " is "
	   << Portfolio[ pPosition -> InstrumentID]
	   << endl;
    }
    if (bIsLast) {
      CThostFtdcQryOrderField QryOrder;
      memset(&QryOrder, 0, sizeof(QryOrder));
      sleep(1);
      int r=m_pUserApiTrader->ReqQryOrder(&QryOrder, ++iRequestID);
      cout <<"ReqQryOrder:"<< r << endl;
    }
  }

  // 报单录入应答
  virtual void OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder,
				CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool
				bIsLast)
  {
    // 输出报单录入结果
    char Msg[200];
    code_convert(pRspInfo -> ErrorMsg, strlen(pRspInfo->ErrorMsg), Msg, 200);
    printf("OnRspOrderInsert: InstrumentID[%s], ErrorCode=[%d], ErrorMsg=[%s]\n",
	   pInputOrder-> InstrumentID,pRspInfo->ErrorID, Msg);
  }
  ///报单操作请求响应
  virtual void OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction,
				CThostFtdcRspInfoField *pRspInfo,
				int nRequestID,
				bool bIsLast) {
    char Msg[200];
    code_convert(pRspInfo -> ErrorMsg, strlen(pRspInfo->ErrorMsg), Msg, 200);
    printf("OnRspOrderAction: InstrumentID[%s], ErrorCode=[%d], ErrorMsg=[%s]\n",
	   pInputOrderAction-> InstrumentID,pRspInfo->ErrorID, Msg);
  }

  virtual void OnRspQryOrder(CThostFtdcOrderField *pOrder,
			     CThostFtdcRspInfoField *pRspInfo,
			     int nRequestID,
			     bool bIsLast) {
    char Msg[200];
    //cout << "OnRspQryOrder"<<endl;
    if ( pRspInfo != NULL ) {
      code_convert(pRspInfo -> ErrorMsg, strlen(pRspInfo -> ErrorMsg), Msg, 200);
      cout << "OnRspQryOrder ErrorMsg:"
	   << Msg<<" ErrorID:" << pRspInfo -> ErrorID <<endl;
    }
    if ( pOrder != NULL) {
      if ( pOrder -> OrderStatus == THOST_FTDC_OST_PartTradedQueueing ||
      	   pOrder -> OrderStatus == THOST_FTDC_OST_PartTradedNotQueueing ||
      	   pOrder -> OrderStatus == THOST_FTDC_OST_NoTradeQueueing ||
      	   pOrder -> OrderStatus == THOST_FTDC_OST_NoTradeNotQueueing ||
      	   pOrder -> OrderStatus == THOST_FTDC_OST_NotTouched ) {
	Order[pOrder -> InstrumentID].push_back(*pOrder);
      	char direction[10];
	if (pOrder -> Direction == THOST_FTDC_D_Buy) {
	  strcpy(direction, "buy");
	} else {
	  strcpy(direction, "sell");
	}
	code_convert(pOrder -> StatusMsg, strlen(pOrder -> StatusMsg), Msg, 200);
	printf("Order: %s Instrument[%s] at LimitPrice[%lf]. Volume[%d], Remain[%d], Msg[%s]\n",
	       direction,
	       pOrder -> InstrumentID,
	       pOrder -> LimitPrice,
	       pOrder -> VolumeTotalOriginal,
	       pOrder -> VolumeTotal,
	       Msg);
      }
    }
    if (bIsLast) {
      pthread_t tid;
      StatusInit = 1;
      CThostFtdcSettlementInfoConfirmField SettlementInfoConfirm;
      memset(&SettlementInfoConfirm, 0, sizeof(SettlementInfoConfirm));
      strcpy(SettlementInfoConfirm.BrokerID, g_chBrokerID);
      strcpy(SettlementInfoConfirm.InvestorID, g_chUserID);
      struct timeval tv;
      gettimeofday(&tv,NULL);
      tm *ltm = localtime(&tv.tv_sec);
      sprintf(SettlementInfoConfirm.ConfirmDate,
	      "%d%02d%02d",
	      1970 + ltm -> tm_year,
	      1 + ltm -> tm_mon,
	      ltm -> tm_mday);
      sprintf(SettlementInfoConfirm.ConfirmTime,
	      "%02d:%02d:%02d",
	      ltm -> tm_hour,
	      ltm -> tm_min,
	      ltm -> tm_sec);
      int r =m_pUserApiTrader -> ReqSettlementInfoConfirm(&SettlementInfoConfirm,
						   ++iRequestID);
      cout << "ReqSettlementInfoConfirm: "<< r << endl;
      // 启动策略线程。
      pthread_create(&tid, NULL, minute_task, (void*) p_C);
    }
  }

  virtual void OnRtnTrade(CThostFtdcTradeField *pTrade) {
    cout << "OnRtnTrade ";
    if ( pTrade -> Direction == THOST_FTDC_D_Buy ) {
      Portfolio[ pTrade -> InstrumentID ] += pTrade -> Volume;
    } else {
      Portfolio[ pTrade -> InstrumentID ] -= pTrade -> Volume;
    }
    cout << "Trade Filled Position of " << pTrade -> InstrumentID
	 << " is now " << Portfolio[ pTrade -> InstrumentID ] << endl;
  }
  virtual void OnRtnOrder(CThostFtdcOrderField *pOrder);
 private:
  // 指向CThostFtdcMduserApi实例的指针
  CThostFtdcTraderApi *m_pUserApiTrader;
  // 指向CThostFtdcMdApi实例的指针
  CThostFtdcMdApi *m_pUserApiMd;
  CSimpleHandler *p_C;
};


class CSimpleHandler : public CThostFtdcMdSpi
{
public:
  // 构造函数，需要一个有效的指向CThostFtdcMdApi实例的指针
 CSimpleHandler(CThostFtdcTraderApi *pUserApiTrader, CThostFtdcMdApi *pUserApiMd):m_pUserApiTrader(pUserApiTrader),
    m_pUserApiMd(pUserApiMd) {
    }
  ~CSimpleHandler() {}
  // 当客户端与交易托管系统建立起通信连接，客户端需要进行登录
  virtual void OnFrontConnected()
  {
    CThostFtdcReqUserLoginField reqUserLogin;
    // get BrokerID
    //printf("BrokerID:");
    //scanf("%s",&g_chBrokerID);
    strcpy(reqUserLogin. BrokerID, g_chBrokerID);
    // get userid
    //printf("userid:");
    //scanf("%s", &g_chUserID);
    strcpy(reqUserLogin.UserID, g_chUserID);
    // get password
    //printf("password:");
    //scanf("%s", Password);
    strcpy(reqUserLogin.Password, Password);
    // 发出登陆请求
    m_pUserApiMd->ReqUserLogin(&reqUserLogin, 0);
  }
  // 当客户端与交易托管系统通信连接断开时，该方法被调用
  virtual void OnFrontDisconnected(int nReason)
  {
    // 当发生这个情况后，API会自动重新连接，客户端可不做处理
    printf("OnFrontDisconnected.\n");
  }
  // 当客户端发出登录请求之后，该方法会被调用，通知客户端登录是否成功
  virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin,
			      CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
  {
    printf("OnRspUserLogin:\n");
    char Msg[200];
    code_convert(pRspInfo -> ErrorMsg, strlen(pRspInfo->ErrorMsg), Msg, 200);

    printf("ErrorCode=[%d], ErrorMsg=[%s]\n", pRspInfo->ErrorID,
	   Msg);
    printf("RequestID=[%d], Chain=[%d]\n", nRequestID, bIsLast);
    if (pRspInfo->ErrorID != 0) {
      // 端登失败，客户端需进行错误处理
      printf("Failed to login, errorcode=%d errormsg=%s requestid=%d chain=%d",
	     pRspInfo->ErrorID, pRspInfo->ErrorMsg, nRequestID, bIsLast);
      exit(-1);
    }
    
    // 端登成功
    SetTradingInstruments();
    SetStrategyParm();
    LoadHistoryKline();
    //pthread 启动交易线程。
    pthread_t tid;
    pthread_create(&tid, NULL, start_trader, (void*) this);

  }

  /** 
   * 在一根K线填满时, 更新各个技术指标.
   * 
   * @param InstrumentID 
   */
  void FinishKline(string InstrumentID, int displace) {
    if ( KlineMap[ InstrumentID ].back().elements.size() % 10 == 9) {
      map<string, KD<double> >::iterator it = KDMap.find( InstrumentID );
      if ( it == KDMap.end() ) {
	KDMap.insert( pair<string, KD<double> >(InstrumentID, KD<double>(k_length, d_length)));
      }
      it = KDMap.find( InstrumentID );
      it -> second.push_back( KlineMap[ InstrumentID ].back().elements.back().ClosePrice );

      if ( it ->second.get_K() > 75 ) {
	kd_up[ InstrumentID ]  = 1;
      }
      if ( it ->second.get_K() < 25 ) {
	kd_down[ InstrumentID ] = 1;
      }

      if (kd_up[ InstrumentID ] == 1 && it -> second.get_D() < 75) {
	alpha[ InstrumentID ] = open_size;
	kd_up[ InstrumentID ] = 0;
	//if ( InstrumentID.compare("cu1808") == 0 ) 
	//cout << "-----" << InstrumentID << " long ------"<< endl;
      }

      if (kd_down[ InstrumentID ] == 1 && it -> second.get_D() > 25 ) {
	alpha[ InstrumentID ] = - open_size;
	kd_down[ InstrumentID ] = 0;
	//if ( InstrumentID.compare("cu1808") == 0 ) 
	//cout << "-----" << InstrumentID << " short ------"<< endl;
      }
      
    }
    if ( displace >= MARKET_CLOSE - 300) {
      alpha[ InstrumentID ] = 0;
    }
    if ( CloseAll == 1 ) {
      alpha[ InstrumentID ] = 0;
    }
  }

  void Withdraw(CThostFtdcOrderField *pOrder) {
    CThostFtdcInputOrderActionField OrderAction;
    /// Set Parameters.
    memset(&OrderAction,0,sizeof(OrderAction));
    strcpy(OrderAction.BrokerID, g_chBrokerID);
    strcpy(OrderAction.InvestorID, g_chUserID);
    OrderAction.OrderActionRef = iOrderActionRef ++;
    strcpy(OrderAction.OrderRef, pOrder -> OrderRef);
    OrderAction.RequestID = ( ++ iRequestID);
    OrderAction.FrontID = pOrder -> FrontID;
    OrderAction.SessionID = pOrder -> SessionID;
    strcpy(OrderAction.ExchangeID, pOrder -> ExchangeID);
    strcpy(OrderAction.OrderSysID, pOrder -> OrderSysID);
    OrderAction.ActionFlag = THOST_FTDC_AF_Delete;
    strcpy(OrderAction.UserID, g_chUserID);
    strcpy(OrderAction.InstrumentID, pOrder -> InstrumentID);
    /// Call the function.
    m_pUserApiTrader -> ReqOrderAction(&OrderAction, iRequestID);
  }

  void WithdrawOrders(string InstrumentID) {
    for (vector<CThostFtdcOrderField>::iterator it = Order[InstrumentID].begin();
	 it < Order[InstrumentID].end(); it ++ ) {
      Withdraw(&(*it)); 
    }
    Order[InstrumentID].clear();
  }

  void FixOrder(CThostFtdcOrderField *pOrder) {
    if (pOrder -> LimitPrice ==
	KlineMap[pOrder -> InstrumentID].back().elements.back().ClosePrice) {
      return;
    }
    Withdraw(pOrder);
    ModifyOrder.insert(pOrder -> OrderRef);
    char direction[10];
    if (pOrder -> Direction == THOST_FTDC_D_Buy) {
      strcpy(direction, "buy");
    } else {
      strcpy(direction, "sell");
    }
    char offset[10];
    if (pOrder -> CombOffsetFlag[0] == THOST_FTDC_OF_Open) {
      strcpy(offset, "Open");
    } else if (pOrder -> CombOffsetFlag[0] == THOST_FTDC_OF_CloseToday) {
      strcpy(offset, "Close");
    }
    cout << "Modify Order:" << direction <<" " << offset << " "
	 << pOrder -> VolumeTotal << " "
	 << pOrder -> InstrumentID 
	 << " from " << pOrder -> LimitPrice 
	 << " to " << KlineMap[ pOrder ->InstrumentID ].back().elements.back().ClosePrice
	 << endl;
  }
  
  void FixOrderPrice(string InstrumentID) {
    for (vector<CThostFtdcOrderField>::iterator it = Order[InstrumentID].begin();
	 it < Order[InstrumentID].end(); it ++ ) {
      if ( it -> OrderStatus == THOST_FTDC_OST_PartTradedQueueing ||
      	   //it -> OrderStatus == THOST_FTDC_OST_PartTradedNotQueueing ||
      	   it -> OrderStatus == THOST_FTDC_OST_NoTradeQueueing ||
      	   //it -> OrderStatus == THOST_FTDC_OST_NoTradeNotQueueing ||
      	   it -> OrderStatus == THOST_FTDC_OST_NotTouched ) {
	FixOrder(&(*it));
      }
    }
  }

  void PlaceOrder(string InstrumentID,
		  TThostFtdcDirectionType Direction,
		  char CombOffsetFlag,
		  TThostFtdcVolumeType VolumeTotalOriginal) {
    CThostFtdcInputOrderField InputOrder;
    memset(&InputOrder, 0, sizeof(InputOrder));
    strcpy(InputOrder.BrokerID, g_chBrokerID);
    strcpy(InputOrder.InvestorID, g_chUserID);
    strcpy(InputOrder.InstrumentID, InstrumentID.c_str());
    NextOrderRef();
    strcpy(InputOrder.OrderRef, MaxOrderRef);
    strcpy(InputOrder.UserID, g_chUserID);
    InputOrder.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
    InputOrder.Direction = Direction;
    InputOrder.CombOffsetFlag[0] = CombOffsetFlag;
    InputOrder.CombHedgeFlag[0] = THOST_FTDC_CIDT_Speculation;
    InputOrder.LimitPrice = KlineMap[ InstrumentID ].back().elements.back().ClosePrice;
    InputOrder.VolumeTotalOriginal = VolumeTotalOriginal;
    InputOrder.TimeCondition = THOST_FTDC_TC_GFD;
    strcpy(InputOrder.GTDDate, "");
    InputOrder.VolumeCondition = THOST_FTDC_VC_AV;
    InputOrder.MinVolume = 0;
    InputOrder.ContingentCondition = THOST_FTDC_CC_Immediately;
    InputOrder.StopPrice = 0;
    InputOrder.ForceCloseReason =  THOST_FTDC_FCC_NotForceClose;
    InputOrder.IsAutoSuspend = 0;
    //strcpy(InputOrder.BusinessUnit, "what's this?")
    //InputOrder.RequestID = (++iRequestID);
    m_pUserApiTrader -> ReqOrderInsert(&InputOrder, ++iRequestID);

    char Dir[10], Flag[10];
    if ( Direction == THOST_FTDC_D_Buy) {
      strcpy(Dir, "buy");
    } else {
      strcpy(Dir, "sell");
    }
    if (CombOffsetFlag == THOST_FTDC_OF_Open ) {
      strcpy(Flag, "open");
    } else {
      strcpy(Flag, "close");
    }
    cout << "Placing Order: " << Dir <<" "<<Flag<<" "
	 << VolumeTotalOriginal << " "<< InstrumentID << " at "<< InputOrder.LimitPrice << endl;;
  }
  
  void ManageInstrument(string InstrumentID, int displace) {
    /* if (InstrumentID.compare("cu1808") == 0 ) { */
    /*   cout << InstrumentID <<" "<<KlineMap[ InstrumentID ].back().elements.back().to_string() << endl; */
    /* } */
    // TODO 调整仓位.
    WithdrawOrders( InstrumentID );
    double LastPrice = KlineMap[ InstrumentID ].back().elements.back().ClosePrice;
    int HoldingPosition = Portfolio[ InstrumentID ], TargetPosition;
    TargetPosition = alpha[ InstrumentID ]
      /InstrumentInfo[InstrumentID].VolumeMultiple/LastPrice;
    /* cout << "Manage " << InstrumentID << " " */
    /*    	 << TargetPosition << " "  */
    /*    	 << HoldingPosition << " " << displace  */
    /*    	 << " " << MARKET_CLOSE  */
    /*    	 << " alpha " << alpha[InstrumentID]  */
    /*    	 << " " << InstrumentInfo[InstrumentID].VolumeMultiple  */
    /*    	 << " " <<KlineMap[ InstrumentID].back().elements.back().ClosePrice  */
    /*    	 << endl;  */
    
    if ( TargetPosition != HoldingPosition ) {
      if ( TargetPosition >= 0 && HoldingPosition >= 0) {
	if ( TargetPosition > HoldingPosition ) { /// Buy Open
	  PlaceOrder( InstrumentID, THOST_FTDC_D_Buy,
		      THOST_FTDC_OF_Open, TargetPosition - HoldingPosition);
	} else if ( TargetPosition < HoldingPosition ) {/// Sell Close
	  PlaceOrder( InstrumentID, THOST_FTDC_D_Sell,
		      THOST_FTDC_OF_CloseToday, HoldingPosition - TargetPosition);
	}
      } else if ( TargetPosition <= 0 && HoldingPosition <= 0 ) {
	if ( TargetPosition > HoldingPosition ) { /// Buy Close
	  PlaceOrder( InstrumentID, THOST_FTDC_D_Buy,
		      THOST_FTDC_OF_CloseToday, TargetPosition - HoldingPosition);
	} else if ( TargetPosition < HoldingPosition ) { /// Sell Open
	  PlaceOrder( InstrumentID, THOST_FTDC_D_Sell,
		      THOST_FTDC_OF_Open, HoldingPosition - TargetPosition);
	}
      } else if ( TargetPosition > 0 && HoldingPosition < 0 ) { /// Buy Close then Buy Open
	PlaceOrder( InstrumentID, THOST_FTDC_D_Buy,
		    THOST_FTDC_OF_CloseToday, -HoldingPosition);
	PlaceOrder( InstrumentID, THOST_FTDC_D_Buy,
		    THOST_FTDC_OF_Open, TargetPosition);
      } else if ( TargetPosition < 0 && HoldingPosition > 0 ) { /// Sell Close then Sell Open
	PlaceOrder( InstrumentID, THOST_FTDC_D_Sell,
		    THOST_FTDC_OF_CloseToday, HoldingPosition);
	PlaceOrder( InstrumentID, THOST_FTDC_D_Sell,
		    THOST_FTDC_OF_Open, -TargetPosition);
      }
    }
  }

  /** 
   * 添加一个tick到K线系统中，根据条件触发交易策略。
   * 
   * @param tick 
   */
  void addTick2Kline(CThostFtdcDepthMarketDataField *tick) {
    map<string, vector<Kline> >::iterator it = KlineMap.find(tick -> InstrumentID);
    
    if (it== KlineMap.end()) {
      vector<Kline> vec_kline;
      vec_kline.push_back(Kline(tick));
      KlineMap.insert(pair<string, vector<Kline> >(tick -> InstrumentID, vec_kline));
    } else if (strcmp(it -> second.back().elements.back().TradingDay, tick -> TradingDay) != 0 ) {
      it -> second.push_back( Kline(tick) );
    } else {
      it ->second.back().add_tick(tick);
      if (tick -> UpdateTime[6] == '0' && tick -> UpdateTime[7] == '0'
	  && tick -> UpdateMillisec == 0) {
	int displace = Time2Displace(tick -> UpdateTime);
	if (IsTradingTime( displace ) && StatusInit == 1 ) {
	  if ( LastManageTime[ tick -> InstrumentID ] < displace ) {
	    //cout << tick -> InstrumentID << " "<< it -> second.back().elements.back().to_string()
	    //	 << " " << displace << endl;
	    FinishKline( tick -> InstrumentID, displace );
	    ManageInstrument( tick -> InstrumentID, displace);
	    LastManageTime[ tick -> InstrumentID ] = displace;
	  }
	}
      }
    }
  }

  // 行情应答
  virtual void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField
				    *pDepthMarketData) {
    addTick2Kline(pDepthMarketData);
    LastTick[ pDepthMarketData -> InstrumentID ] = *pDepthMarketData;
    struct timeval tv;
    gettimeofday(&tv,NULL);
    tm *ltm = localtime(&tv.tv_sec);
    if (PrintTick == 1) {
      cout << pDepthMarketData -> TradingDay << " "
	   << pDepthMarketData -> UpdateTime << ":"<< pDepthMarketData -> UpdateMillisec << " "
	   << pDepthMarketData -> InstrumentID<<" "
	   << pDepthMarketData -> LastPrice << " LocalTime "
	   << ltm -> tm_hour <<":"<<ltm -> tm_min << ":"
	   << ltm -> tm_sec << ":" << tv.tv_usec
	   << endl;
    }
    double local_displace = tm2Displace(ltm) + tv.tv_usec * 1e-6;
    double server_displace = Time2Displace(pDepthMarketData -> UpdateTime) +
      pDepthMarketData -> UpdateMillisec * 1e-3;
    DelaySeconds = ceil(local_displace - server_displace + 0.1);
  }
  
  // 针对用户请求的出错通知
  virtual void OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID,
			  bool bIsLast) {
    printf("OnRspError:\n");
    char Msg[200];
    code_convert(pRspInfo->ErrorMsg, strlen(pRspInfo->ErrorMsg), Msg, 200);
    printf("ErrorCode=[%d], ErrorMsg=[%s]\n", pRspInfo->ErrorID,
	   Msg);
    printf("RequestID=[%d], Chain=[%d]\n", nRequestID, bIsLast);
    // 客户端需进行错误处理
    //{客户端的错误处理}
  }
  
  void SetStrategyParm() {
    ifstream File;
    File.open("strategy_config");
    string ParmName;
    while ( File >> ParmName ) {
      if ( ParmName.compare("k_length") == 0 ) {
	File >> k_length;
      } else if ( ParmName.compare("d_length") == 0 ) {
	File >> d_length;
      }
    }
    open_size = TradingLevel * 5/TradingInstruments.size();
  }

  void SetTradingInstruments() {
    ifstream File;
    File.open("instruments");
    string InstrumentID;
    while ( File >> InstrumentID ) {
      TradingInstruments.insert(InstrumentID);
    }
  }
  
  void LoadHistoryKline() {
    DIR *dir, *dir_date;
    struct dirent *ent, *ent_ins;
    char dir_date_name[100];
    char filename[100];
    vector <string> dates;
    ifstream File;
    if ((dir = opendir( HistoryLocation)) != NULL) {
      while ( (ent = readdir(dir)) != NULL) {
	if ( ent -> d_name[0] == '2' ) {
	  dates.push_back( ent -> d_name );
	}
      }
      sort(dates.begin(), dates.end());
      int start_date = 0;
      if ( dates.size() > LOOKBACK_DATES ) {
	start_date = dates.size() - LOOKBACK_DATES;
      }
      for ( int i = start_date; i < dates.size(); i ++ ) {
	sprintf(dir_date_name, "%s/%s/", HistoryLocation, dates[i].c_str());
	cout << "Loading " <<dir_date_name << endl;
	if ((dir_date = opendir( dir_date_name )) != NULL ) {
	  while ((ent_ins = readdir( dir_date )) != NULL ) {
	    string d_name(ent_ins -> d_name);
	    int s = d_name.find("-") + 1;
	    int e = d_name.find(".");
	    if ( s > 0 ) {
	      string InstrumentID = d_name.substr(s, e-s);
	      map<string, vector<Kline> >::iterator it = KlineMap.find( InstrumentID );
	      if ( it == KlineMap.end() ) {
		vector<Kline> vec_kline;
		vec_kline.push_back(Kline());
		KlineMap.insert(pair<string, vector<Kline> >(InstrumentID, vec_kline));
	      } else {
		it -> second.push_back(Kline());
	      }
	      it = KlineMap.find( InstrumentID );
	      sprintf(filename, "%s%s", dir_date_name, d_name.c_str());
	      //cout << filename << endl;
	      File.open(filename);
	      string line;
	      while (getline(File, line)) {
		stringstream lineStream(line), s;
		string TradingDay, UpdateTime, buf;
		double OpenPrice, HighestPrice, LowestPrice, ClosePrice;
		getline(lineStream, TradingDay, ',');
		getline(lineStream, UpdateTime, ',');
		getline(lineStream, buf,','); s.clear();s.str(buf);
		s >> OpenPrice;
		getline(lineStream, buf,','); s.clear();s.str(buf);
		s >> HighestPrice;
		getline(lineStream, buf,','); s.clear();s.str(buf);
		s >> LowestPrice;
		getline(lineStream, buf,','); s.clear();s.str(buf);
		s >> ClosePrice;
		it -> second.back().elements.push_back(KlineElement(TradingDay,
								    UpdateTime,
								    OpenPrice,
								    HighestPrice,
								    LowestPrice,
								    ClosePrice));
		int displace = Time2Displace(UpdateTime);
		if ( IsTradingTime(displace) ) {
		  FinishKline(InstrumentID, displace);
		}
	      }
	      File.close();
	    }
	  }
	} else {
	  cerr << "Can't open " << dir_date_name << endl;
	}
      }
    } else {
      cerr << HistoryLocation << " is not a directory." << endl;      
    }
  }
  friend void *start_trader(void*data);
  friend void *minute_task(void*data);
private:
  // 指向CThostFtdcTraderApi实例的指针
  CThostFtdcTraderApi *m_pUserApiTrader;
  // 指向CThostFtdcMdApi实例的指针
  CThostFtdcMdApi *m_pUserApiMd;
  int iRequestID;
  map<string, vector<Kline> > KlineMap; /**< K线数据，每个合约每天一个Kline. */
  map<string, KD<double> > KDMap;
  map<string, int> kd_up;
  map<string, int> kd_down;
  map<string, double> alpha;
  double open_size;
  size_t k_length;
  size_t d_length;
};

/** 
 * 启动交易接口线程的函数.
 * 
 * @param data 
 * 
 * @return 
 */
void *start_trader(void*data) {
  CSimpleHandler*p = (CSimpleHandler*)data;
  cout << "starting trader thread." << endl;
  // 产生一个事件处理的实例
  TSimpleHandler sh(p -> m_pUserApiTrader, p -> m_pUserApiMd, p);
  // 注册一事件处理的实例
  p -> m_pUserApiTrader->RegisterSpi(&sh);
  // 订阅私有流
  // TERT_RESTART:从本交易日开始重传
  // TERT_RESUME:从上次收到的续传
  // TERT_QUICK:只传送登录后私有流的内容
  p -> m_pUserApiTrader->SubscribePrivateTopic(THOST_TERT_RESUME);
  // 订阅公共流
  // TERT_RESTART:从本交易日开始重传
  // TERT_RESUME:从上次收到的续传
  // TERT_QUICK:只传送登录后公共流的内容
  p -> m_pUserApiTrader->SubscribePublicTopic(THOST_TERT_RESUME);
  // 设置交易托管系统服务的地址，可以注册多个地址备用
  p -> m_pUserApiTrader->RegisterFront(TradeFrontAddr);
  // 使客户端开始与后台服务建立连接
  p -> m_pUserApiTrader->Init();
  // 释放API实例
  p -> m_pUserApiTrader->Release();
}

void *minute_task(void*data) {
  CSimpleHandler*p = (CSimpleHandler*)data;
  struct timeval tv, tv0;
  while(1) {
    gettimeofday(&tv,NULL);
    tm *ltm = localtime(&tv.tv_sec);
    cout << "Minute Task: " << ltm -> tm_hour << ":" << ltm -> tm_min << ":" << ltm -> tm_sec;
    ltm -> tm_sec -= DelaySeconds;
    mktime(ltm);
    ltm -> tm_sec = 0;
    int displace = tm2Displace(ltm);

    cout << " Server Time:" <<  ltm -> tm_hour << ":" << ltm -> tm_min << ":" << ltm -> tm_sec
	 << endl;

    gettimeofday(&tv, NULL);
    ltm = localtime(&tv.tv_sec);
    ltm -> tm_sec -= DelaySeconds;///转换到服务器时间
    ltm -> tm_min += 1;//下一分钟
    mktime(ltm);
    ltm -> tm_sec = 0;
    ltm -> tm_sec += DelaySeconds;///转换回本地时间
    time_t next_minute = mktime(ltm);
    /// 计算下单时间
    int break_num = 20;
    int interval = 60/break_num;
    time_t breaks[break_num];
    gettimeofday(&tv0, NULL);
    for ( int n = 1; n < break_num; n ++ ) {
      tv = tv0;
      ltm = localtime(&tv.tv_sec);
      ltm -> tm_sec -= DelaySeconds;///转换到服务器时间
      mktime(ltm);
      ltm -> tm_sec = n * interval + DelaySeconds;
      breaks[n] = mktime(ltm);
    }      
    if ( IsTradingTime(displace)) {
      if ( CloseAll == 1 ) {//清仓模式下，对所有持仓合约操作。
	for ( map<string, double>::iterator it = Portfolio.begin();
	      it != Portfolio.end(); it ++ ) {
	  string InstrumentID = it -> first;
	  if ( LastManageTime[ InstrumentID ] < displace ) {
	    //cout << "Manage " << InstrumentID << endl;
	    p -> FinishKline( InstrumentID, displace);
	    p -> ManageInstrument( InstrumentID, displace);
	    LastManageTime[ InstrumentID ] = displace;
	  }
	}
      }else {//正常模式下仅对主力合约操作.
	for (set<string>::iterator it = TradingInstruments.begin();
	       it != TradingInstruments.end(); it ++ ) {
	  string InstrumentID = *it;
	  if ( LastManageTime[ InstrumentID ] < displace ) {
	    cout << "Manage " << InstrumentID << endl;
	    p -> FinishKline( InstrumentID, displace);
	    p -> ManageInstrument( InstrumentID, displace);
	    LastManageTime[ InstrumentID ] = displace;
	  }
	}
      } 
      cout << "Portfolio:" << endl;
      for ( map<string, double>::iterator it = Portfolio.begin();
	    it != Portfolio.end(); it ++ ) {
	if ( it -> second != 0 ) {
	  cout << it -> first << " " << it -> second << endl;
	}
      }
      //重新下单
      for (int n = 1; n < break_num; n++ ) {
	gettimeofday(&tv, NULL);
      	double sleep_time = (breaks[n] - tv.tv_sec) * 1e6 - tv.tv_usec;
	if ( sleep_time > 0 ) {
	  usleep( sleep_time );
	  cout << "Manage modify:" << n << endl;
	  for (set<string>::iterator it = TradingInstruments.begin();
		 it != TradingInstruments.end(); it ++ ) {
	    string InstrumentID = *it;
	    p -> FixOrderPrice(InstrumentID);
	  }
	} else {
	  cout << "time not enough for change order" << endl;
	}
      }
    }
    gettimeofday(&tv, NULL);
    double sleep_time = (next_minute - tv.tv_sec) * 1e6 - tv.tv_usec;
    cout << "Wait untill: " << ltm -> tm_hour << ":" << ltm -> tm_min << ":" << ltm -> tm_sec << endl;
    usleep( sleep_time );
  }
}

void parse_config(string config_file) {
  ifstream inFile;
  inFile.open(config_file.c_str());
  if (!inFile) {
    cout << "Unable to Open file:" << config_file << endl;
  }
  string ParmName, Parm;
  while (inFile >> ParmName) {
    if (ParmName.compare("BrokerID")==0) {
      inFile >> g_chBrokerID;;
    } else if (ParmName.compare("UserID")==0) {
      inFile >> g_chUserID;
    } else if (ParmName.compare("password")==0) {
      inFile >> Password;
    } else if (ParmName.compare("TradeFront")==0) {
      inFile >> TradeFrontAddr;
    } else if (ParmName.compare("MarketFront")==0) {
      inFile >> MarketFrontAddr;
    } else if (ParmName.compare("HistoryLocation")==0) {
      inFile >> HistoryLocation;
    } else if (ParmName.compare("Delay")==0) {
      inFile >> DelaySeconds;
    } else if (ParmName.compare("PrintTick")==0) {
      inFile >> PrintTick;
    } else if (ParmName.compare("CloseAll")==0) {
      inFile >> CloseAll;
    } else if (ParmName.compare("TradingIntervals")==0) {
      int count, start, end;
      string Time;
      inFile >> count;
      while ( count -- ) {
	inFile >> Time;
	start = Time2Displace(Time);
	inFile >> Time;
	end = Time2Displace(Time);
	MARKET_CLOSE = end;
	TradingIntervals.push_back(pair<int, int>(start, end));
	cout << "(" << start << "," << end <<"] ";
      }
      cout << endl;
    } else if (ParmName.compare("TradingLevel") == 0 ) {
      inFile >> TradingLevel;
    } else {
      string buf;
      inFile >> buf;
      cout << "unknow parameter name:" << ParmName << endl;
    }
  }
}


void TSimpleHandler::OnRtnOrder(CThostFtdcOrderField *pOrder) {
    char Msg[200];
    code_convert(pOrder->StatusMsg, strlen(pOrder -> StatusMsg), Msg, 200);
    cout << "OnRtnOrder: ";
    if ( pOrder -> Direction == THOST_FTDC_D_Buy ) {
      cout << "buy ";
    } else {
      cout << "sell ";
    }
    cout <<pOrder -> InstrumentID
	 << " at " << pOrder -> LimitPrice 
	 << " Volume[" << pOrder -> VolumeTotal
	 << "],Traded[" << pOrder -> VolumeTraded
	 << "],OrderRef[" << pOrder -> OrderRef
	 <<"] StatusMsg["<< Msg <<"]" 
	 <<" OrderStatus["<< pOrder -> OrderStatus <<"]"
	 << endl;
    vector<CThostFtdcOrderField> &orders = Order[pOrder -> InstrumentID];
    int flag = 0;
    for ( int i = 0; i < orders.size(); i ++ ) {
      if (orders[i].SessionID == pOrder -> SessionID &&
	  strcmp(orders[i].OrderRef, pOrder -> OrderRef) == 0 ) {
	if ( pOrder -> OrderStatus == THOST_FTDC_OST_PartTradedQueueing ||
	     //pOrder -> OrderStatus == THOST_FTDC_OST_PartTradedNotQueueing ||
	     pOrder -> OrderStatus == THOST_FTDC_OST_NoTradeQueueing ||
	     //pOrder -> OrderStatus == THOST_FTDC_OST_NoTradeNotQueueing ||
	     pOrder -> OrderStatus == THOST_FTDC_OST_NotTouched ||
	     pOrder -> OrderStatus == THOST_FTDC_OST_Unknown) {
	  orders[i] = *pOrder;
	  cout << "update order " << pOrder -> InstrumentID
	       << " OrderRef: " <<pOrder -> OrderRef << endl;
	} else {
	  if ( pOrder -> OrderStatus == THOST_FTDC_OST_Canceled &&
	       ModifyOrder.find( pOrder -> OrderRef ) != ModifyOrder.end()) {
	    p_C -> PlaceOrder(pOrder -> InstrumentID,
		       pOrder -> Direction,
		       pOrder -> CombOffsetFlag[0],
		       pOrder -> VolumeTotal);
	    ModifyOrder.erase( pOrder ->OrderRef);	    
	  }
	  orders.erase( orders.begin() + i);
	  cout << "erase order " << pOrder -> InstrumentID
	       << " OrderRef: " <<pOrder -> OrderRef << endl;
	}
	flag = 1;
	break;
      }
    }
    if (flag == 0 &&
	(pOrder -> OrderStatus == THOST_FTDC_OST_PartTradedQueueing ||
	 pOrder -> OrderStatus == THOST_FTDC_OST_PartTradedNotQueueing ||
	 pOrder -> OrderStatus == THOST_FTDC_OST_NoTradeQueueing ||
	 pOrder -> OrderStatus == THOST_FTDC_OST_NoTradeNotQueueing ||
	 pOrder -> OrderStatus == THOST_FTDC_OST_NotTouched ||
	 pOrder -> OrderStatus == THOST_FTDC_OST_Unknown)) {
      cout << "new order " << pOrder -> InstrumentID
	   << " OrderRef: " <<pOrder -> OrderRef << endl;
      orders.push_back(*pOrder);
    }
  }

