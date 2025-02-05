#include "StdAfx.h"
#include "PortfolioScalperOrderPlacer.h"
#include "Portfolio.h"
#include "OrderProcessor.h"
#include "ScalperStrategy.h"

#ifndef USE_FEMAS_API
#include "InputOrder.h"
#else
#include "InputOrder_FM.h"
#endif // !USE_FEMAS_API

#include <boost/date_time.hpp>

CPortfolioScalperOrderPlacer::CPortfolioScalperOrderPlacer(void)
{
}


CPortfolioScalperOrderPlacer::~CPortfolioScalperOrderPlacer(void)
{
}

void CPortfolioScalperOrderPlacer::BuildTemplateOrder()
{
	static trade::OffsetFlagType OFFSET_FLAGS[2] = 
				{trade::OF_OPEN, trade::OF_CLOSE_TODAY};

	boost::shared_ptr<trade::MultiLegOrder> pMultiLegOrder(new trade::MultiLegOrder);
	
	pMultiLegOrder->set_quantity(m_pPortf->Quantity());
	pMultiLegOrder->set_portfolioid(m_pPortf->ID());
	pMultiLegOrder->set_offset(trade::ML_OF_OTHER);
	pMultiLegOrder->set_haswarn(false);
	pMultiLegOrder->set_statusmsg("");

	boost::gregorian::date d = boost::gregorian::day_clock::local_day();
	pMultiLegOrder->set_opendate(boost::gregorian::to_iso_string(d));
	pMultiLegOrder->set_reason(trade::SR_Scalpe);

	assert(m_pPortf->Count() == 1);

	CLeg* leg = m_pPortf->GetLeg(1);

	for(int i = 0; i < 2; ++i)
	{
		trade::Order* order = pMultiLegOrder->add_legs();
		order->set_brokerid(m_pOrderProcessor->BrokerId());
		order->set_investorid(m_pOrderProcessor->InvestorId());
		order->set_instrumentid(leg->Symbol());
		order->set_orderref("");

		order->set_orderpricetype(trade::LIMIT_PRICE);

		char CombOffset[2] = { 0, 0 };
		CombOffset[0] = OFFSET_FLAGS[i];

		order->set_comboffsetflag(CombOffset);

		char CombHedgeFlag[] = { static_cast<char>(m_pPortf->HedgeFlag()), 0 };
		order->set_combhedgeflag(std::string(CombHedgeFlag));

		int qty = pMultiLegOrder->quantity() * leg->Ratio();
		order->set_volumetotaloriginal(qty);

		order->set_timecondition(trade::TC_GFD);
		
		order->set_volumecondition(trade::VC_AV);
		order->set_minvolume(1);

		order->set_contingentcondition(trade::IMMEDIATELY);
		order->set_forceclosereason(trade::NOT_FORCE_CLOSE);
		order->set_isautosuspend(false);
		order->set_userforceclose(false);

		order->set_ordersubmitstatus(trade::NOT_SUBMITTED);
		order->set_orderstatus(trade::STATUS_UNKNOWN);

		order->set_preferred(i == 0);
	}

	m_multiLegOrderTemplate = pMultiLegOrder;
}

void CPortfolioScalperOrderPlacer::OnAddingLegOrderPlacer( CLegOrderPlacer* pLegOrderPlacer )
{
	CScalperStrategy* scalperStrategy = dynamic_cast<CScalperStrategy*>(m_pPortf->Strategy());
	if(scalperStrategy != NULL)
	{
		double priceTick = scalperStrategy->PriceTick();
		pLegOrderPlacer->SetPriceTick(priceTick);
	}
}

