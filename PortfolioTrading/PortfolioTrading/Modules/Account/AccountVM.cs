﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Microsoft.Practices.Prism.ViewModel;
using System.Collections.ObjectModel;
using System.Windows.Input;
using Microsoft.Practices.Prism.Commands;
using System.Xml.Linq;
using PortfolioTrading.Infrastructure;
using System.Threading;
using PortfolioTrading.Utils;
using System.Diagnostics;
using Microsoft.Practices.Prism.Events;
using Microsoft.Practices.ServiceLocation;
using PortfolioTrading.Events;
using PortfolioTrading.Modules.Portfolio.Strategy;
using Infragistics.Windows.DataPresenter;
using System.Windows;

namespace PortfolioTrading.Modules.Account
{
    public class AccountVM : NotificationObject
    {
        private ObservableCollection<PortfolioVM> _acctPortfolios = new ObservableCollection<PortfolioVM>();

        private Client _client;
        private NativeHost _host;

        private static int HostPortSeed = 16181;
        private static int MaxRetryConnectTimes = 3;

        private IEventAggregator EventAggregator { get; set; }
        private ServerAddressRepoVM AddressRepo { get; set; }

        public AccountVM()
        {
            AddPortfolioCommand = new DelegateCommand<AccountVM>(OnAddPortfolio);
            ConnectCommand = new DelegateCommand<AccountVM>(OnConnectHost);
            DisconnectCommand = new DelegateCommand<AccountVM>(OnDisconnectHost);
            RemovePortfolioCommand = new DelegateCommand<XamDataGrid>(OnRemovePortfolio);
            DetachCommand = new DelegateCommand<AccountVM>(OnDetachHost);

            _host = new NativeHost();

            EventAggregator = ServiceLocator.Current.GetInstance<IEventAggregator>();
            AddressRepo = ServiceLocator.Current.GetInstance<ServerAddressRepoVM>();
            
            _client = new Client();
            _client.OnError += new Action<string>(_client_OnError);
            _client.OnQuoteReceived += new Action<entity.Quote>(_client_OnQuoteReceived);
            _client.OnPortfolioItemUpdated += new Action<entity.PortfolioItem>(_client_OnPortfolioItemUpdated);
            _client.OnMultiLegOrderUpdated += new Action<trade.MultiLegOrder>(_client_OnMultiLegOrderUpdated);
            _client.OnLegOrderUpdated += new Action<string, string, string, trade.Order>(_client_OnLegOrderUpdated);
            _client.OnTradeUpdated += new Action<trade.Trade>(_client_OnTradeUpdated);
            _client.OnPositionDetialReturn += new Action<trade.PositionDetailInfo>(_client_OnPositionDetialReturn);
        }

        public string Id
        {
            get { return string.Format("{0}-{1}", _brokerId, _investorId); }
        }

        #region BrokerId
        private string _brokerId;

        public string BrokerId
        {
            get { return _brokerId; }
            set
            {
                if (_brokerId != value)
                {
                    _brokerId = value;
                    RaisePropertyChanged("BrokerId");
                }
            }
        }
        #endregion

        #region InvestorId
        private string _investorId;

        public string InvestorId
        {
            get { return _investorId; }
            set
            {
                if (_investorId != value)
                {
                    _investorId = value;
                    RaisePropertyChanged("InvestorId");
                }
            }
        }
        #endregion

        #region Password
        private string _password;

        public string Password
        {
            get { return _password; }
            set
            {
                if (_password != value)
                {
                    _password = value;
                    RaisePropertyChanged("Password");
                }
            }
        }
        #endregion

        #region MaxSubmit
        private int _maxSubmit = 450;

        public int MaxSubmit
        {
            get { return _maxSubmit; }
            set
            {
                if (_maxSubmit != value)
                {
                    _maxSubmit = value;
                    RaisePropertyChanged("MaxSubmit");
                }
            }
        }
        #endregion

        #region MaxCancel
        private int _maxCancel = 900;

        public int MaxCancel
        {
            get { return _maxCancel; }
            set
            {
                if (_maxCancel != value)
                {
                    _maxCancel = value;
                    RaisePropertyChanged("MaxCancel");
                }
            }
        }
        #endregion
        
        #region HostPort
        private int _hostPort;

        public int HostPort
        {
            get { return _hostPort; }
            set
            {
                if (_hostPort != value)
                {
                    _hostPort = value;
                    RaisePropertyChanged("HostPort");
                }
            }
        }
        #endregion

        #region Status
        private string _status = "未连接";

        public string Status
        {
            get { return _status; }
            set
            {
                if (_status != value)
                {
                    _status = value;
                    RaisePropertyChanged("Status");
                    RaisePropertyChanged("IsConnected");
                }
            }
        }
        #endregion


        public IEnumerable<PortfolioVM> Portfolios
        {
            get { return _acctPortfolios; }
        }

        public PortfolioVM Get(string pid)
        {
            lock (_acctPortfolios)
            {
                return _acctPortfolios.FirstOrDefault(p => p.Id == pid);
            }
        }

        public void AddPorfolio(PortfolioVM porfVm)
        {
            lock (_acctPortfolios)
            {
                _acctPortfolios.Add(porfVm);
            }
        }

        public void RemovePortfolio(PortfolioVM portfVm)
        {
            lock (_acctPortfolios)
            {
                int idx = _acctPortfolios.IndexOf(portfVm);
                if(idx >= 0 && idx < _acctPortfolios.Count)
                    _acctPortfolios.RemoveAt(idx);
            }
        }

        public ICommand AddPortfolioCommand
        {
            get;
            private set;
        }

        public ICommand RemovePortfolioCommand
        {
            get;
            private set;
        }

        public ICommand ConnectCommand
        {
            get;
            private set;
        }

        public ICommand DisconnectCommand
        {
            get;
            private set;
        }

        public ICommand DetachCommand
        {
            get;
            private set;
        }

        public void QueryAccountInfo(Action<trade.AccountInfo> accountInfoCallback)
        {
            Func<trade.AccountInfo> funcQryAcctInfo = _client.QueryAccountInfo;
            funcQryAcctInfo.BeginInvoke(ar =>
            {
                trade.AccountInfo acctInfo = funcQryAcctInfo.EndInvoke(ar);
                if (accountInfoCallback != null)
                    accountInfoCallback(acctInfo);
            }, null);
        }

        public void QueryPositionDetails(string symbol)
        {
            _client.QueryPositionDetails(symbol);
        }

        public void ManualCloseOrder(string symbol, trade.TradeDirectionType direction,
            DateTime openDate, int quantity, Action<bool, string> closeDoneHandler)
        {
            Func<string, trade.TradeDirectionType, DateTime, int, OperationResult> closeFunc =
                new Func<string, trade.TradeDirectionType, DateTime, int, OperationResult>
                    (_client.ManualCloseOrder);
            closeFunc.BeginInvoke(symbol, direction, openDate, quantity, 
                delegate(IAsyncResult ar)
                {
                    try
                    {
                        var or = closeFunc.EndInvoke(ar);
                        if (closeDoneHandler != null)
                            closeDoneHandler(or.Success, or.ErrorMessage);
                    }
                    catch (System.Exception ex)
                    {
                        LogManager.Logger.ErrorFormat("Manual close order failed due to {0}", ex.Message);
                        if (closeDoneHandler != null)
                            closeDoneHandler(false, "手动平仓时发生未知错误");
                    }
                }, null);
        }

        public entity.SymbolInfo QuerySymbolInfo(string symbol)
        {
            if (_client.IsConnected)
                return _client.QuerySymbolInfo(symbol);
            else
                return null;
        }

        private void OnAddPortfolio(AccountVM acct)
        {
            PortfolioVM portf = new PortfolioVM(this);
            portf.Id = NextPortfId();
            
            EditPortfolioDlg dlg = new EditPortfolioDlg();
            dlg.Owner = System.Windows.Application.Current.MainWindow;
            dlg.Portfolio = portf;
            bool? res = dlg.ShowDialog();
            if (res ?? false)
            {
                entity.PortfolioItem portfolioItem = dlg.Portfolio.GetEntity();
                AddPorfolio(portf);
                if(_client.IsConnected)
                    _client.AddPortf(portfolioItem);

                PublishChanged();
            }
        }

        private string NextPortfId()
        {
            if (_acctPortfolios.Count > 0)
                return (int.Parse(_acctPortfolios.Last().Id) + 1).ToString();
            else
                return "1";
        }

        private void OnRemovePortfolio(XamDataGrid dataGrid)
        {
            if (dataGrid == null) return;

            DataRecord dr = dataGrid.ActiveRecord as DataRecord;
            if (dr != null)
            {
                PortfolioVM portfVm = dr.DataItem as PortfolioVM;
                if (portfVm != null)
                {
                    MessageBoxResult res = MessageBox.Show(Application.Current.MainWindow,
                        string.Format("删除帐户[{0}]的组合 {1}", portfVm.AccountId, portfVm.DisplayText),
                        "删除确认", MessageBoxButton.YesNo, MessageBoxImage.Question);
                    if (res == MessageBoxResult.Yes)
                    {
                        RemovePortfolio(portfVm);
                        if (_client.IsConnected)
                            _client.RemovePortf(portfVm.Id);
                        
                        PublishChanged();
                    }
                }
            }
        }

        private bool CanRemovePortfolio(XamDataGrid dataGrid)
        {
            return (dataGrid != null && dataGrid.ActiveRecord != null && dataGrid.ActiveRecord.IsDataRecord);
        }

        private void SyncToHost()
        {
            List<entity.PortfolioItem> portfItems = new List<entity.PortfolioItem>();
            foreach (var portf in _acctPortfolios)
            {
                entity.PortfolioItem portfolioItem = portf.GetEntity();
                portfItems.Add(portfolioItem);
            }
            _client.AddPortfCollection(portfItems);
        }

        public bool IsConnected
        {
            get { return _status == "已连接"; }
        }

        #region IsBusy
        private bool _isBusy = false;

        public bool IsBusy
        {
            get { return _isBusy; }
            set
            {
                if (_isBusy != value)
                {
                    _isBusy = value;
                    RaisePropertyChanged("IsBusy");
                }
            }
        }
        #endregion


        #region IsExpanded
        private bool _isExpanded;

        public bool IsExpanded
        {
            get { return _isExpanded; }
            set
            {
                if (_isExpanded != value)
                {
                    _isExpanded = value;
                    RaisePropertyChanged("IsExpanded");
                }
            }
        }
        #endregion

        public void Close()
        {
            TradeStaionCutDown();
            _client.Disconnect();
            _host.Exit();
        }

        private int _connectTimes;

        void ChangeStatus(string statusTxt, bool begin)
        {
            Status = statusTxt;
            IsBusy = begin ? true : false;
        }

        private void OnConnectHost(AccountVM acct)
        {
            ChangeStatus("连接中...", true);

            SynchronizationContext uiContext = SynchronizationContext.Current;

            HostPort = ConfigurationHelper.GetAppSettingValue("tradeHostPort", 16181); ;
            
            EventLogger.Write(string.Format("正在为{0}建立交易终端...", acct.InvestorId));

            Func<int, bool, bool> funcLaunch = new Func<int, bool, bool>(_host.Startup);
            funcLaunch.BeginInvoke(HostPort, true, 
                delegate(IAsyncResult arLaunch)
                {
                    bool succ = funcLaunch.EndInvoke(arLaunch);

                    if(!succ)
                    {
                        LogManager.Logger.Warn("Launch trade station failed");
                        return;
                    }

                    LogManager.Logger.InfoFormat("TradeStaion started up.");

                    Action<int> actionLoopConnect = null;
                    _connectTimes = 1;

                    Action<bool, string, bool> actionClntConnectDone = null;
                    actionClntConnectDone = (b, t, attach) =>
                    {
                        string txt = string.Format("连接交易终端(第{0}次)", _connectTimes);
                        if (b)
                        {
                            txt += "成功";
                        }
                        else
                        {
                            txt += "失败 (" + t + ")";
                        }
                        EventLogger.Write(txt);

                        if (b)
                        {
                            if (attach)
                            {
                                uiContext.Send(o => ChangeStatus("已连接", false), null);
                                EventLogger.Write("恢复连接到交易终端");
                            }
                            else
                            {
                                Func<AccountVM, bool> actionReady = new Func<AccountVM, bool>(HaveTradeStationReady);
                                actionReady.BeginInvoke(
                                    acct,
                                    new AsyncCallback(
                                        delegate(IAsyncResult ar)
                                        {
                                            try
                                            {
                                                bool ok = actionReady.EndInvoke(ar);
                                                if (ok)
                                                {
                                                    uiContext.Send(o => ChangeStatus("已连接", false), null);
                                                    EventLogger.Write(string.Format("{0}准备就绪", acct.InvestorId));
                                                    SyncToHost();
                                                }
                                                else
                                                {
                                                    uiContext.Send(o => ChangeStatus("连接失败", false), null);
                                                    EventLogger.Write(string.Format("{0}发生错误", acct.InvestorId));
                                                    _client.Disconnect();
                                                    _host.Exit();
                                                }
                                            }
                                            catch (System.Exception ex)
                                            {
                                                EventLogger.Write("初始化交易终端发生错误");
                                                LogManager.Logger.Error(ex.Message);
                                                _host.Exit();
                                            }

                                        }),
                                    null);
                                LogManager.Logger.Info(txt);
                            }
                        }
                        else
                        {
                            LogManager.Logger.Warn(txt);
                            if ("交易终端拒绝连接" != t &&
                                _connectTimes < MaxRetryConnectTimes && 
                                actionLoopConnect != null)
                                actionLoopConnect.Invoke(++_connectTimes);
                            else
                            {
                                uiContext.Send(o => ChangeStatus("连接失败", false), null);
                                EventLogger.Write(string.Format("为{0}尝试{1}次连接均发生错误"
                                    , acct.InvestorId, _connectTimes));
                                _client.Disconnect();
                            }

                        }
                    };

                    actionLoopConnect = new Action<int>(delegate(int times)
                    {
                        if(times > 1)
                            Thread.Sleep(1000);

                        string effectiveTradeStation = AddressRepo.EffectiveTradeStation.Address;
                        EventLogger.Write("Connect to {0}", effectiveTradeStation);
                        LogManager.Logger.InfoFormat("Connect to {0}", effectiveTradeStation);
                        _client.AuthClientId = this.Id;
                        
                        try
                        {
                            _client.ConnectAsync(effectiveTradeStation, actionClntConnectDone);
                        }
                        catch (System.Exception ex)
                        {
                            LogManager.Logger.ErrorFormat("Error connecting host due to : {0}", ex.Message);
                            if (actionClntConnectDone != null)
                                actionClntConnectDone(false, "交易终端拒绝连接", false);
                        }
                    });

                    actionLoopConnect.Invoke(_connectTimes);
                }, null);

            //_host.Startup(HostPort);
        }

        private bool HaveTradeStationReady(AccountVM acct)
        {
            EventLogger.Write("正在连接交易服务器: " + AddressRepo.EffectiveTrading.Name);
            OperationResult tradeConnResult = _client.TradeConnect(AddressRepo.EffectiveTrading.Address,
                                                          acct.InvestorId);

            if (tradeConnResult.Success)
            {
                EventLogger.Write("交易连接成功");
            }
            else
            {
                EventLogger.Write("交易连接失败 (" + tradeConnResult.ErrorMessage + ")");
                return false;
            }

            OperationResult tradeLoginResult = _client.TradeLogin(acct.BrokerId,
                acct.InvestorId, acct.Password, new entity.AccountSettings
                {
                    MaxSubmit = acct.MaxSubmit,
                    MaxCancel = acct.MaxCancel
                });

            if (tradeLoginResult.Success)
            {
                EventLogger.Write("交易登录成功");
            }
            else
            {
                EventLogger.Write("交易登录失败 (" + tradeLoginResult.ErrorMessage + ")");
                return false;
            }

            EventLogger.Write("正在连接行情服务器: " + AddressRepo.EffectiveMarket.Name);
            OperationResult quoteConnResult = _client.QuoteConnect(AddressRepo.EffectiveMarket.Address,
                                                          acct.InvestorId);
            if (quoteConnResult.Success)
            {
                EventLogger.Write("行情连接成功");
            }
            else
            {
                EventLogger.Write("行情连接失败 (" + quoteConnResult.ErrorMessage + ")");
                return false;
            }

            OperationResult quoteLoginResult = _client.QuoteLogin(acct.BrokerId,
                acct.InvestorId, acct.Password);

            if (quoteLoginResult.Success)
            {
                EventLogger.Write("行情登录成功");
            }
            else
            {
                EventLogger.Write("行情登录失败 (" + quoteLoginResult.ErrorMessage + ")");
                return false;
            }

            return true;
        }

        private void OnDisconnectHost(AccountVM acct)
        {
            EventLogger.Write(string.Format("正在将{0}关闭交易终端连接...", acct.InvestorId));
            Close();
            EventLogger.Write(string.Format("{0}已断开", acct.InvestorId));
            Status = "未连接";
        }

        private void OnDetachHost(AccountVM acct)
        {
            EventLogger.Write(string.Format("正在将{0}从交易终端断开连接...", acct.InvestorId));
            _client.Disconnect();
            EventLogger.Write(string.Format("{0}已断开", acct.InvestorId));
            Status = "未连接";
        }

        public void TradeStaionCutDown()
        {
            try
            {
                _client.QuoteLogout();
                _client.TradeLogout();
                _client.QuoteDisconnect();
                _client.TradeDisconnect();
            }
            catch (System.Exception ex)
            {
                LogManager.Logger.WarnFormat("Cutting down Trade station encountered error - {0}", ex.Message);            	
            }
        }

        public static AccountVM Load(XElement xmlElement)
        {
            AccountVM acct = new AccountVM();

            XAttribute attrBrokerId = xmlElement.Attribute("brokerId");
            if (attrBrokerId != null)
                acct.BrokerId = attrBrokerId.Value;

            XAttribute attrInvestorId = xmlElement.Attribute("investorId");
            if (attrInvestorId != null)
                acct.InvestorId = attrInvestorId.Value;

            XAttribute attrPwd = xmlElement.Attribute("password");
            if (attrPwd != null)
                acct.Password = attrPwd.Value;

            XAttribute attrMaxSubmit = xmlElement.Attribute("maxSubmit");
            if (attrMaxSubmit != null)
                acct.MaxSubmit = int.Parse(attrMaxSubmit.Value);

            XAttribute attrMaxCancel = xmlElement.Attribute("maxCancel");
            if (attrMaxCancel != null)
                acct.MaxCancel = int.Parse(attrMaxCancel.Value);

            foreach(var portfElem in xmlElement.Element("portfolios").Elements("portfolio"))
            {
                PortfolioVM porfVm = PortfolioVM.Load(acct, portfElem);
                acct.AddPorfolio(porfVm);
            }

            return acct;
        }

        public XElement Persist()
        {
            XElement elem = new XElement("account");
            elem.Add(new XAttribute("brokerId", _brokerId));
            elem.Add(new XAttribute("investorId", _investorId));
            elem.Add(new XAttribute("password", _password));
            elem.Add(new XAttribute("maxSubmit", _maxSubmit));
            elem.Add(new XAttribute("maxCancel", _maxCancel));

            XElement elemPortfs = new XElement("portfolios");
            lock (_acctPortfolios)
            {
                foreach (var portf in Portfolios)
                {
                    elemPortfs.Add(portf.Persist());
                }
            }
            elem.Add(elemPortfs);

            return elem;
        }

        public event Action<trade.PositionDetailInfo> OnPositionDetailReturn;

        #region Client event handlers

        void _client_OnTradeUpdated(trade.Trade obj)
        {
            string info = string.Format("trade: {0}\t{1}\t{2}", obj.InstrumentID, obj.Price, obj.TradeTime);
            EventAggregator.GetEvent<TradeUpdatedEvent>().Publish(obj);
        }

        void _client_OnMultiLegOrderUpdated(trade.MultiLegOrder obj)
        {
            string info = string.Format("mlOrder: {0}\t{1}\t{2}", obj.OrderId, obj.PortfolioId, obj.Quantity);
            EventAggregator.GetEvent<MultiLegOrderUpdatedEvent>().Publish(
                new MultiLegOrderUpdateArgs{ AccountId = Id, MultiLegOrder = obj });
        }


        void _client_OnLegOrderUpdated(string portfId, string mlOrderId, string legOrdRef, trade.Order legOrder)
        {
            OrderUpdateArgs args = new OrderUpdateArgs
                                    {
                                        AccountId = Id,
                                        PortfolioId = portfId,
                                        MlOrderId = mlOrderId,
                                        LegOrderRef = legOrdRef,
                                        LegOrder = legOrder
                                    };
            EventAggregator.GetEvent<IndividualOrderUpdatedEvent>().Publish(args);
        }

        void _client_OnPortfolioItemUpdated(entity.PortfolioItem obj)
        {
            string info = string.Format("Porf: {0}\t{1}\t{2}", obj.ID, obj.Quantity, obj.Diff);
            Debug.WriteLine(info);
            var portf = Get(obj.ID);
            if(portf != null)
                DispatcherHelper.Current.BeginInvoke(new Action(() => portf.Update(obj)));
        }

        void _client_OnQuoteReceived(entity.Quote obj)
        {
            string info = (string.Format("{0}\t{1}\t{2}", obj.symbol, obj.last, obj.update_time));
        }

        void _client_OnPositionDetialReturn(trade.PositionDetailInfo obj)
        {
            if (OnPositionDetailReturn != null)
            {
                DispatcherHelper.Current.BeginInvoke(
                    new Action(() => OnPositionDetailReturn(obj)));
            }
        }

        void _client_OnError(string err)
        {
            
        }
        #endregion
        
        public void PublishChanged()
        {
            EventAggregator.GetEvent<AccountChangedEvent>().Publish(this);
        }

        internal Client Host
        {
            get { return _client; }
        }

        public bool VerifyStatus()
        {
            if (IsConnected)
                return true;
            else
            {
                MessageBox.Show( 
                    "请先连接帐户!", "帐户错误",
                    MessageBoxButton.OK, MessageBoxImage.Error);
                return false;
            }
        }
    }
}
