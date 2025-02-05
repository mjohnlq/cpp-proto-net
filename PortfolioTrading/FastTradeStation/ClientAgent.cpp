#include "ClientAgent.h"
#include "DiffStrategy.h"
#include "PortfolioOrderHelper.h"
#include "ScalperStrategy.h"
#include "globalmembers.h"

#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>

CClientAgent::CClientAgent(const string& clntId):
m_clientId(clntId),
m_pSession(NULL)
{
	m_orderProcessor.Initialize(this, &m_tradeAgent);
	m_quoteRepositry.Init(&m_quoteAgent);
	m_quoteAgent.SetCallbackHanlder(&m_quoteRepositry);
}

CClientAgent::~CClientAgent(void)
{
	m_tradeAgent.Close();
	m_quoteAgent.Close();
}

void CClientAgent::Add( entity::PortfolioItem* portfolioItem )
{
	// let CPortfolio wrap and hold entity::PortfolioItem instance
	CPortfolio* pPortf = CPortfolio::Create(this, portfolioItem); 
	m_portfolioMgr.Add(pPortf);
	pPortf->SubscribeQuotes(&m_quoteRepositry);
}

void CClientAgent::AddPortfolios( entity::AddPortfolioParam& addPortfParam )
{
	int count = addPortfParam.portfolioitems_size();
	for(int i = 0; i < count; ++i )
	{
		const entity::PortfolioItem& portfItem = addPortfParam.portfolioitems(i);
		entity::PortfolioItem* pClonedItem = new entity::PortfolioItem;
		pClonedItem->CopyFrom(portfItem);
		Add(pClonedItem);
	}
}

void CClientAgent::Remove( const string& pid )
{
	m_portfolioMgr.Remove(pid);
}

boost::tuple<bool, string> CClientAgent::TradeConnect( const std::string& address, const std::string& streamDir )
{
	m_tradeAddress = address;
	return m_tradeAgent.Open(m_tradeAddress, streamDir);
}

boost::tuple<bool, string> CClientAgent::TradeLogin( const string& brokerId, const string& userId, const string& password, const entity::AccountSettings* pAcctSettings )
{
	m_brokerId = brokerId;
	m_userId = userId;
	
	if(pAcctSettings != NULL)
	{
		m_orderProcessor.SetMaxOpenTimes(pAcctSettings->maxsubmit());
		m_orderProcessor.SetMaxCancelTimes(pAcctSettings->maxcancel());
	}

	return m_tradeAgent.Login(brokerId, userId, password);
}

void CClientAgent::TradeLogout()
{
	m_tradeAgent.Logout();
}

void CClientAgent::TradeDisconnect()
{
	m_tradeAgent.Logout();
	m_tradeAgent.Close();
}

boost::tuple<bool, string> CClientAgent::QuoteLogin( const string& brokerId, const string& userId, const string& password )
{
	return m_quoteAgent.Login(brokerId, userId, password);
}

void CClientAgent::QuoteLogout()
{
	m_quoteAgent.Logout();
}

boost::tuple<bool, string> CClientAgent::QuoteConnect( const std::string& address, const std::string& streamDir )
{
	m_quoteAddress = address;
	return m_quoteAgent.Open(m_quoteAddress, streamDir);
}

void CClientAgent::QuoteDisconnect()
{
	m_quoteAgent.Logout();
	m_quoteAgent.Close();
}

void CClientAgent::RegQuote( vector<string>& symbols )
{
}

void CClientAgent::OpenPosition( const string& pid, int quantity)
{
	CPortfolio* portf = m_portfolioMgr.Get(pid);
	OpenPosition(portf, quantity, trade::SR_Manual);
}

void CClientAgent::OpenPosition( CPortfolio* portf, int qty, trade::SubmitReason submitReason)
{
	bool autoTracking = portf->Strategy()->IsAutoTracking();
	bool enablePrefer = portf->Strategy()->EnablePrefer();
	entity::PosiDirectionType direction = portf->Strategy()->Direction();

	PlaceOrderContext placeOrderCtx;
	placeOrderCtx.quantity = qty;
	placeOrderCtx.brokerId = m_brokerId;
	placeOrderCtx.investorId = m_userId;
	placeOrderCtx.orderPriceType = trade::LIMIT_PRICE;
	placeOrderCtx.limitPriceType = entity::Opposite;

	boost::shared_ptr<trade::MultiLegOrder> multilegOrder(BuildOpenPosiOrder(portf, direction, &placeOrderCtx));
	multilegOrder->set_reason(submitReason);

	// send to order processor
	//m_orderProcessor.SubmitPortfOrder(portf, multilegOrder);
}

void CClientAgent::ClosePosition( const trade::MultiLegOrder& openMlOrd, const string& legOrdRef, trade::SubmitReason submitReason, string& msg)
{
	CPortfolio* portf = m_portfolioMgr.Get(openMlOrd.portfolioid());

	bool autoTracking = portf->Strategy()->IsAutoTracking();
	bool enablePrefer = portf->Strategy()->EnablePrefer();
	entity::PosiDirectionType portfDirection = portf->Strategy()->Direction();

	PlaceOrderContext placeOrderCtx;
	placeOrderCtx.brokerId = m_brokerId;
	placeOrderCtx.investorId = m_userId;
	placeOrderCtx.orderPriceType = trade::LIMIT_PRICE;
	placeOrderCtx.limitPriceType = entity::Opposite;

	boost::shared_ptr<trade::MultiLegOrder> multilegOrder(BuildClosePosiOrder(portf, portfDirection,
		&openMlOrd, openMlOrd.quantity(), &placeOrderCtx));
	multilegOrder->set_reason(submitReason);

	//m_orderProcessor.SubmitPortfOrder(portf, multilegOrder);
}

void CClientAgent::SimpleCloseOrderPosition(const string& portfolioId, trade::SubmitReason submitReason)
{
	CPortfolio* portf = m_portfolioMgr.Get(portfolioId);
	int qty = portf->Quantity();
	if(portf->HasPosition())
	{
		ClosePosition(portfolioId, qty, submitReason);
	}
}

void CClientAgent::ClosePosition(const string& portfolioId, int quantity, trade::SubmitReason submitReason)
{
	CPortfolio* portf = m_portfolioMgr.Get(portfolioId);

	bool autoTracking = portf->Strategy()->IsAutoTracking();
	bool enablePrefer = portf->Strategy()->EnablePrefer();
	entity::PosiDirectionType portfDirection = portf->Strategy()->Direction();

	PlaceOrderContext placeOrderCtx;
	placeOrderCtx.brokerId = m_brokerId;
	placeOrderCtx.investorId = m_userId;
	placeOrderCtx.orderPriceType = trade::LIMIT_PRICE;
	placeOrderCtx.limitPriceType = entity::Opposite;

	boost::shared_ptr<trade::MultiLegOrder> multilegOrder(BuildClosePosiOrder(portf, portfDirection,
		NULL, quantity, &placeOrderCtx));
	multilegOrder->set_reason(submitReason);

	//m_orderProcessor.SubmitPortfOrder(portf, multilegOrder);
}

void CClientAgent::ChangePosition(CPortfolio* portf, const string& closeSymbol, entity::PosiDirectionType existingPosition, int qty, trade::SubmitReason submitReason)
{
	PlaceOrderContext placeOrderCtx;
	placeOrderCtx.quantity = qty;
	placeOrderCtx.brokerId = m_brokerId;
	placeOrderCtx.investorId = m_userId;
	placeOrderCtx.orderPriceType = trade::LIMIT_PRICE;
	placeOrderCtx.limitPriceType = entity::Opposite;

	boost::shared_ptr<trade::MultiLegOrder> multilegOrder(BuildChangePosiOrder(portf,
		closeSymbol, existingPosition, &placeOrderCtx));
	bool autoTracking = portf->Strategy()->IsAutoTracking();
	multilegOrder->set_reason(submitReason);

	//m_orderProcessor.SubmitPortfOrder(portf, multilegOrder);
}

void CClientAgent::QuickScalpe( CPortfolio* portf, int quantity, trade::PosiDirectionType posiDirection, double precedence )
{
	PlaceOrderContext placeOrderCtx;
	placeOrderCtx.quantity = quantity;
	placeOrderCtx.brokerId = m_brokerId;
	placeOrderCtx.investorId = m_userId;
	placeOrderCtx.orderPriceType = trade::LIMIT_PRICE;
	placeOrderCtx.limitPriceType = entity::Opposite;

	boost::shared_ptr<trade::MultiLegOrder> multilegOrder(BuildScalperOrder(portf, posiDirection, precedence, &placeOrderCtx));
	multilegOrder->set_reason(trade::SR_Scalpe);

	//m_orderProcessor.SubmitPortfOrder(portf, multilegOrder);
}

void CClientAgent::QuickScalpe( const string& pid, int quantity )
{
	CPortfolio* portf = m_portfolioMgr.Get(pid);
	CScalperStrategy* pScalperStrategy = dynamic_cast<CScalperStrategy*>(portf->Strategy());
	if(pScalperStrategy != NULL)
	{
		CLeg* leg = portf->GetLeg(1);
		double lmtPx[2];
		double pxTick = pScalperStrategy->PriceTick();
		lmtPx[0] = leg->Bid() + pxTick;
		lmtPx[1] = leg->Ask() - pxTick;
		portf->OrderPlacer().Run(trade::LONG, lmtPx, 2);
	}
}

bool CClientAgent::QueryAccountInfo(string* serializedAcctInfo)
{
	bool succ = m_orderProcessor.QueryAccountInfo(serializedAcctInfo);
	return succ;
}

void CClientAgent::OnPortfolioUpdated(entity::PortfolioItem* portfolioItem)
{
	std::string callbackData;
	portfolioItem->SerializeToString(&callbackData);
	if(m_pSession != NULL)
		m_pSession->BeginCallback("PortfolioPush", callbackData);
}

void CClientAgent::OnMultiLegOrderUpdated( trade::MultiLegOrder* order )
{
	std::string callbackData;
	order->SerializeToString(&callbackData);
	ClientMsgPacketPtr packet(new CClientMessagePacket("MultiLegOrderPush", callbackData));
	m_clientMsgPump.Enqueue(packet);
	/*if(m_pSession != NULL)
		m_pSession->BeginCallback("MultiLegOrderPush", callbackData);*/
}

void CClientAgent::OnTradeUpdated( trade::Trade* trade )
{
	std::string callbackData;
	trade->SerializeToString(&callbackData);
	ClientMsgPacketPtr packet(new CClientMessagePacket("TradePush", callbackData));
	m_clientMsgPump.Enqueue(packet);
	//if(m_pSession != NULL)
	//	m_pSession->BeginCallback("TradePush", callbackData);
}

void CClientAgent::OnLegOrderUpdated( const string& portfId, const string& mlOrderId, trade::Order* legOrd )
{
	entity::LegOrderUpdateParam orderUpdate;
	orderUpdate.set_portfid(portfId);
	orderUpdate.set_multilegorderid(mlOrderId);
	orderUpdate.set_legorderref(legOrd->orderref());
	
	trade::Order* pOrd = orderUpdate.mutable_legorder();
	pOrd->CopyFrom(*legOrd);
	    
	std::string callbackData;
	orderUpdate.SerializeToString(&callbackData);
	ClientMsgPacketPtr packet(new CClientMessagePacket("LegOrderPush", callbackData));
	m_clientMsgPump.Enqueue(packet);
	//if(m_pSession != NULL)
	//	m_pSession->BeginCallback("LegOrderPush", callbackData);
}

void CClientAgent::ApplyStrategySetting( const entity::ModifyStrategyParam& settings )
{
	const string& portfId = settings.portfid();
	CPortfolio* portf = m_portfolioMgr.Get(portfId);
	if(portf != NULL)
	{
		portf->ApplyStrategySetting(settings.strategyname(), settings.strategydata());
	}
}

void CClientAgent::EnableStrategy(const entity::ModifyRunningStatusParam& runningStatusParam)
{
	const string& portfId = runningStatusParam.portfid();
	CPortfolio* portf = m_portfolioMgr.Get(portfId);
	if(portf != NULL)
	{
		bool isEnabled = runningStatusParam.enabled();
		portf->EnableStrategy(isEnabled);
	}
}

void CClientAgent::TurnPortfSwitches( const entity::ModifyPortfolioSwitchParam& switchesParam )
{
	const string& portfId = switchesParam.portfid();
	CPortfolio* portf = m_portfolioMgr.Get(portfId);
	if(portf != NULL)
	{
		portf->TurnSwitches(switchesParam.autoopen(),
			switchesParam.autostopgain(), switchesParam.autostoploss(),
			switchesParam.autotracking(), switchesParam.enableprefer());
	}
}

void CClientAgent::SetPorfPreferredLeg( const entity::ModifyPortfolioPreferredLegParam& preferredLegParam )
{
	const string& portfId = preferredLegParam.portfid();
	CPortfolio* portf = m_portfolioMgr.Get(portfId);
	const string& legName = preferredLegParam.legsymbol();
	if(portf != NULL)
	{
		BOOST_FOREACH(const LegPtr& l, portf->Legs())
		{
			if(l->Symbol() == legName)
				l->IsPreferred(true);
			else
				l->IsPreferred(false);
		}
	}
}

void CClientAgent::CancelOrder( const entity::CancelOrderParam& cancelParam )
{
	m_orderProcessor.CancelOrder(cancelParam.orderref(), cancelParam.exchangeid(),
		cancelParam.ordsysid(), m_userId, cancelParam.symbol());
}

void CClientAgent::OnPostionDetailReturned( trade::PositionDetailInfo* posiDetailInfo )
{
	std::string callbackData;
	posiDetailInfo->SerializeToString(&callbackData);
	if(m_pSession != NULL)
		m_pSession->BeginCallback("PositionDetailReturn", callbackData);
}

void CClientAgent::QueryPositionDetails( const string& symbol )
{
	m_orderProcessor.QueryPositionDetails(symbol);
}

boost::tuple<bool, string> CClientAgent::ManualCloseOrder( const string& symbol, trade::TradeDirectionType direction, const string& openDate, int quantity )
{
	PlaceOrderContext placeOrderCtx;
	placeOrderCtx.quantity = quantity;
	placeOrderCtx.brokerId = m_brokerId;
	placeOrderCtx.investorId = m_userId;
	placeOrderCtx.orderPriceType = trade::LIMIT_PRICE;
	placeOrderCtx.limitPriceType = entity::Opposite;

	return m_orderProcessor.PlaceOrder(symbol, direction, openDate, &placeOrderCtx);
}

void CClientAgent::VirtualOpenPosition( const string& pid, int quantity )
{
	CPortfolio* portf = m_portfolioMgr.Get(pid);
	if(portf != NULL)
		portf->VirtualOpen(quantity);
}

void CClientAgent::VirtualClosePosition( const string& pid, int quantity )
{
	CPortfolio* portf = m_portfolioMgr.Get(pid);
	if(portf != NULL)
		portf->VirtualClose(quantity);
}

void CClientAgent::SetPortfolioQuantity( const string& pid, int qty, int maxQty )
{
	CPortfolio* portf = m_portfolioMgr.Get(pid);
	if(portf != NULL)
		portf->SetQuantity(qty, maxQty);
}

bool CClientAgent::QuerySymbolInfo( const string& symbol, entity::SymbolInfo** ppSymbInfo )
{
	CSymbolInfo* pMySymbolInfo = NULL;
	bool succ = m_tradeAgent.QuerySymbolInfo(symbol, &pMySymbolInfo);
	if(succ)
	{
		*ppSymbInfo = &(pMySymbolInfo->InnerItem());
	}
	return succ;
}



