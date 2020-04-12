#ifndef WQCONNECTOR_H
#define WQCONNECTOR_H

#include <QObject>
#include <QWebSocket>
#include <QJsonDocument>
#include <QVariantMap>

enum WQ_CLASS_TYPES
{
    UNKNOWN_CLASS = 0,
    SHARE = 1,
    BONDS = 2,
    FUTURES = 3,
    OPTION = 4,
    SPREAD = 5,
    SWAP = 6,
    CURRENCY = 7,
    STRATEGY = 8,
    INDEX = 9,
    GPB = 10,
    REPORTS = 11,
    INSTRUCTIONS = 12,
    AUTOCHARTIST = 13,
    AUTOFOLLOW = 14,
    OMS = 15,
    ALGO_TRADING = 16,
    SMS = 17,
    CROSSRATE = 18,
    RFQ = 19,
    SOR = 20,
    EX_OFF = 21,
    METALS = 22
};

struct FirmData
{
    QString curr;
    QString firmId;
    QString tag;
};

struct SecSpec
{
    QString baseCCode;      //base_ccode
    QString baseSCode;      //base_scode
    QString faceUnit;       //face_unit
    int faceValue;          //face_value
    int firstCurrQtyScale;  //first_curr_qty_scale
    QString isin;           //isin
    QString longName;       //long_name
    int lot;                //lot
    int matDate;            //mat_date
    int qtyMult;            //qty_multiplier
    int qtyScale;           //qty_scale
    int scale;              //scale
    QString secCode;        //scode
    int secondCurrQtyScale; //second_curr_qty_scale
    QString secName;        //sname
    int specFlags;          //specFlags
    int step;               //step
    WQ_CLASS_TYPES secType; //type
    int subType;            //subType
    QString tradeCurrency;  //trade_currency
    SecSpec()
        : faceValue(0), firstCurrQtyScale(1), lot(1), matDate(0), qtyMult(1),
          qtyScale(1), scale(1), secondCurrQtyScale(1), specFlags(0), step(1),
          subType(0) {}
    void initFrom(const QVariantMap &vmap);
};

struct ParamSpec
{
    QString quikname;   //quikname
    QString name;       //name
    QString longName;   //long_name
    int flags;          //flags
    int pType;          //type
    int scale;         //scale
    QString enumDesc;   //enumDesc
    ParamSpec() : flags(0), pType(0), scale(1){}
    void initFrom(const QVariantMap &vmap);
};

struct SecClassSpec
{
    QString classCode;      //ccode
    QString className;      //cname
    bool enableQuotes;      //enableQuotes
    bool enableStop;        //enableStop
    bool enableStopPeriod;  //enableStopPeriod
    bool enableTran;        //enableTran
    QString firmid;         //firmid
    bool isCurrency;        //isCurrency
    bool isFutures;         //isFutures
    bool isNegot;           //isNegot
    bool isObligation;      //isObligation
    bool isRepo;            //isRepo
    QMap<QString, ParamSpec *> params; //paramList
    QString parsHash;       //pars_hash
    QString perms;          //perms
    QString secsHash;       //secs_hash
    int subType;            //subType
    bool tranAvailable;     //tranAvailable
    WQ_CLASS_TYPES classType;//type
    QMap<QString, SecSpec *> securities;    //secList
    SecClassSpec()
        : enableQuotes(false), enableStop(false), enableStopPeriod(false), enableTran(false),
          isCurrency(false), isFutures(false), isNegot(false), isObligation(false), isRepo(false),
          subType(0), tranAvailable(false), classType(UNKNOWN_CLASS) {}
};

struct TradeAccountInfo
{
    QString account;                //trdacc
    QString firmid;                 //firmid
    QStringList classList;          //classList
    QStringList mainMarginClassList;//mainMarginClasses
    int limitsInLots;               //limitsInLots
    QList<int> limitKinds;          //limitKinds
    bool isFutures(const QMap<QString, SecClassSpec *> &clList);
    bool isCurrency(const QMap<QString, SecClassSpec *> &clList);
    bool isForex(const QStringList &fxClassesKeys);
    bool isInstruction(const QMap<QString, SecClassSpec *> &clList);
    TradeAccountInfo() : limitsInLots(0) {}
    bool initFrom(const QVariantMap &vmap);
};

class WQConnector : public QObject
{
    Q_OBJECT
public:
    explicit WQConnector(QObject *parent = nullptr);
    QString getServerUrl(){return m_serverUrl;}
    QString getUser(){return m_user;}

    bool isWqConnected();
    bool wqConnect(QString srv, QString usr, QString psw);
    void wqDisconnect();

    //sending messages
    void sendLogin();
    void sendStatusCheck();
    void sendPortfolioCalcParams();
    void sendBookClientPortfolio();
    void sendUnbookClientPortfolio();
    void sendBookBuySell();
    void sendUnbookBuySell();

private:
    QString m_serverUrl;
    QString m_user;
    QString m_password;
    QWebSocket *m_ws;
    QByteArray framecollection;
    QString stringframecollection;
    QString sessionId;
    quint64 userId;
    QString defaultFirmKey;
    QMap<QString, FirmData> firms;
    QDateTime serverDateTime;
    QMap<QString, SecClassSpec *> classList;
    QStringList fxClassesKeys;
    QMap<QString, TradeAccountInfo*> tradeAccounts;

    void processArrivedJson(QJsonDocument &jsonDoc);
    //incomming messages
    void processConnectionEstablished(QVariantMap &msg);
    void processLogin(QVariantMap &msg);
    void processServerMessage(QVariantMap &msg);
    void processDepoLimitsUpdate(QVariantMap &msg);
    void processFutClientLimitsUpdate(QVariantMap &msg);
    void processCashLimitsUpdate(QVariantMap &msg);
    void processParamsUpdate(QVariantMap &msg);
    void processTrdAccsListUpdate(QVariantMap &msg);
    //message parts
    void processClassItem(QVariantMap &cls);
    void processFxClassesKeys(QVariantMap &fxcls);

    void printToLog(QString msg);
signals:
    void simpleLog(QString msg);
    void startExchange();
    void exchangeStopped();
    void serverMessage(QString msg);
    void tradeSessionOpen(QDate dt, quint64 flags, QString id);
    void serverDateTimeChanged(QDateTime sdt);

    void fatalError();

private slots:
    void restartStatusChecking();
public slots:
    void wqConnected();
    void wqDisconnected();
    void wqBinaryFrameReceived(const QByteArray &frame, bool isLastFrame);
    void wqBinaryMessageReceived(const QByteArray &message);
    void wqTextFrameReceived(const QString &frame, bool isLastFrame);
    void wqTextMessageReceived(const QString &message);
    void wqError(QAbstractSocket::SocketError error);
    void wqSslErrors(const QList<QSslError> &errors);
};

#endif // WQCONNECTOR_H
