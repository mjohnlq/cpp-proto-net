#pragma once

#include "ThostFtdcTraderApi.h"
#include "protobuf_gen/trade.pb.h"

#include <string>
#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>

class COrderManager;

class CTradeAgent : public CThostFtdcTraderSpi
{
public:
	CTradeAgent(COrderManager* pOrderMgr);
	~CTradeAgent(void);

	void Login(const std::string& brokerId, const std::string& userId, const std::string& password);
	void Logout();

	boost::tuple<std::string&, std::string&, std::string&> GetCurrentUserInfo()
	{
		return boost::make_tuple(boost::ref(m_brokerId), boost::ref(m_userId), boost::ref(m_password));
	}

	const std::string& GetUserId() { return m_userId; }

	void Buy();

	bool SubmitOrder( const std::vector< boost::shared_ptr<protoc::InputOrder> >& orders);
	bool SubmitOrder( protoc::InputOrder* pOrder);

	bool SubmitOrderAction (protoc::InputOrderAction* pOrderAction);

	void QueryAccount();

	int GetSessionID(){ return SESSION_ID; }
	int GetFrontID(){ return FRONT_ID; }
	int GetLastMaxOrderRef(){ return m_maxOrderRef; }

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

	///请求查询投资者持仓响应
	virtual void OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

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

	//////////////////////////////////////////////////////////////////////////

private:
	int RequestIDIncrement(){ return ++m_iRequestID; }
	bool IsMyOrder(CThostFtdcOrderField *pOrder);
	bool IsTradingOrder(CThostFtdcOrderField *pOrder);
	void ReqSettlementInfoConfirm();

	CThostFtdcTraderApi* m_pUserApi;
	boost::thread m_thTrading;

	std::string m_brokerId;
	std::string m_userId;
	std::string m_password;

	bool m_isConnected;
	// 请求编号
	int m_iRequestID;

	COrderManager* m_pOrderMgr;

	boost::mutex m_mutex;
	boost::condition_variable m_condLogout;

	// 会话参数
	TThostFtdcFrontIDType	FRONT_ID;	//前置编号
	TThostFtdcSessionIDType	SESSION_ID;	//会话编号
	TThostFtdcOrderRefType	ORDER_REF;	//报单引用

	int m_maxOrderRef;
};

