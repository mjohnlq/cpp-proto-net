#pragma once

#include "TradeAgentCallback.h"
#include "ThostTraderApi/ThostFtdcTraderApi.h"
#include "SyncRequest.h"
#include "TradeMessagePump.h"
#include "../Entity/gen/cpp/quote.pb.h"
#include "SymbolInfo.h"

#include <string>
#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/date_time.hpp>

using namespace std;

class CTradeAgent : public CThostFtdcTraderSpi
{
public:
	CTradeAgent(void);
	~CTradeAgent(void);

	boost::tuple<bool, string> Open( const string& address, const string& streamDir );
	void Close();

	boost::tuple<bool, string> Login(const string& brokerId, const string& userId, const string& password);
	void Logout();

	void SetCallbackHanlder(CTradeAgentCallback* pCallback);

	bool SubmitOrder(trade::InputOrder* pInputOrder);
	bool SubmitOrderAction(trade::InputOrderAction* pInputOrderAction);

	void ReqSettlementInfoConfirm();

	void QueryAccount();
	void QueryOrders(const std::string& symbol);
	void QueryPositions();
	void QueryPositionDetails(const std::string& symbol);

	bool QuerySymbol(const std::string& symbol, entity::Quote** ppQuote);
	bool QuerySymbolAsync(const std::string& symbol, int nReqestId);

	bool QuerySymbolInfo(const std::string& symbol, CSymbolInfo** ppSymbolInfo);
	bool QuerySymbolInfoAsync( CSymbolInfo* pSymbolInfo, int nReqestId );

	bool IsConnected(){ return m_bIsConnected; }
	bool IsDisconnected(){ return !m_bIsConnected; }

	const boost::gregorian::date& TradingDay(){ return m_tradingDay; }

	//////////////////////////////////////////////////////////////////////////
	// Response trading related api

	///当客户端与交易后台建立起通信连接时（还未登录前），该方法被调用。
	virtual void OnFrontConnected();

	///登录请求响应
	virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin,	CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	///登出请求响应
	virtual void OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	///投资者结算结果确认响应
	virtual void OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	///请求查询合约响应
	virtual void OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	///请求查询资金账户响应
	virtual void OnRspQryTradingAccount(CThostFtdcTradingAccountField *pTradingAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	///请求查询投资者持仓明细响应
	virtual void OnRspQryInvestorPositionDetail(CThostFtdcInvestorPositionDetailField *pInvestorPositionDetail, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	///报单录入请求响应
	virtual void OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	///报单操作请求响应
	virtual void OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	///错误应答
	virtual void OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	///当客户端与交易后台通信连接断开时，该方法被调用。当发生这个情况后，API会自动重新连接，客户端可不做处理。
	virtual void OnFrontDisconnected(int nReason);

	///心跳超时警告。当长时间未收到报文时，该方法被调用。
	virtual void OnHeartBeatWarning(int nTimeLapse);

	///报单通知
	virtual void OnRtnOrder(CThostFtdcOrderField *pOrder);

	///成交通知
	virtual void OnRtnTrade(CThostFtdcTradeField *pTrade);
	
	///请求查询行情响应
	virtual void OnRspQryDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);;

	//////////////////////////////////////////////////////////////////////////

private:
	int RequestIDIncrement();
	bool IsMyOrder(CThostFtdcOrderField *pOrder)
	{ 
		return pOrder->FrontID == FRONT_ID && pOrder->SessionID == SESSION_ID;
	}

	// 请求编号
	int m_iRequestID;
	boost::mutex m_mutReqIdInc;

	boost::condition_variable m_condLogin;
	boost::mutex m_mutLogin;
	string m_sLoginError;
	bool m_loginSuccess;

	CThostFtdcTraderApi* m_pUserApi;
	
	bool m_bIsConnected;
	bool m_bIsConnecting;

	boost::condition_variable m_condConnectDone;
	boost::mutex m_mutex;
	boost::thread m_thQuoting;

	string m_brokerID;
	string m_userID;

	// 会话参数
	TThostFtdcFrontIDType	FRONT_ID;	//前置编号
	TThostFtdcSessionIDType	SESSION_ID;	//会话编号
	int m_maxOrderRef;					//报单引用
	boost::gregorian::date m_tradingDay;

	CSyncRequestFactory<entity::Quote> m_requestFactory;
	CTradeMessagePump m_messagePump;

	CSyncRequestFactory<CSymbolInfo> m_symbInfoReqFactory;
};

