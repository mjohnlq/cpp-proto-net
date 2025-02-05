#pragma once

#include "Enums.h"
#include "entity/message.pb.h"
#include "LegUpdateItem.h"
#include "StrategyUpdateItem.h"

using namespace System;
using namespace System::Collections::Generic;
using namespace System::Runtime::InteropServices;

namespace PTEntity {

public ref class PortfolioUpdateItem
{
public:
	PortfolioUpdateItem(entity::PortfolioUpdateItem* pEntity);

	property String^ Id
	{
		String^ get()
		{
			return _id; 
		}
	}

	property List<LegUpdateItem^> ^Legs
	{
		List<LegUpdateItem^>^ get()
		{
			return _legs; 
		}
	}

	property int TotalOpenTimes
	{
		int get()
		{
			return _totalOpenTimes; 
		}
	}

	property int TotalCloseTimes
	{
		int get()
		{
			return _totalCloseTimes; 
		}
	}

	property int CurrentPosition
	{
		int get()
		{
			return _currentPosition; 
		}
	}

	property int CancelTimes
	{
		int get()
		{
			return _cancelTimes; 
		}
	}

	property double Profit
	{
		double get()
		{
			return _profit;
		}
	}

	property StrategyType Kind
	{
		StrategyType get()
		{
			return _strategyType; 
		}
	}

	property StrategyUpdateItem^ StrategyUpdate
	{
		StrategyUpdateItem^ get()
		{
			return _strategyUpdate; 
		}
	}

	property bool HasMsg
	{
		bool get(){ return _hasMsg; }
	}

	property String^ Message
	{
		String^ get(){ return _msg; }
	}

private:
	String ^_id;
	List<LegUpdateItem^> ^_legs;
	int _totalOpenTimes;
	int _totalCloseTimes;
	int _currentPosition;
	int _cancelTimes;
	double _profit;
	StrategyType _strategyType;
	StrategyUpdateItem ^_strategyUpdate;

	bool _hasMsg;
	String ^_msg;
};

}

