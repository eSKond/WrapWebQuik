#include "wqconnector.h"
#include <QJsonObject>
#include <QStringList>
#include <QTimer>

WQConnector::WQConnector(QObject *parent)
    : QObject(parent), m_ws(nullptr)
{

}

bool WQConnector::isWqConnected()
{
    if(m_ws && m_ws->state()==QAbstractSocket::ConnectedState)
        return true;
    return false;
}

bool WQConnector::wqConnect(QString srv, QString usr, QString psw)
{
    if(m_ws)
        return false;
    QString origin=QString("https://%1").arg(srv);
    m_ws=new QWebSocket(origin);
    connect(m_ws, &QWebSocket::connected, this, &WQConnector::wqConnected);
    connect(m_ws, &QWebSocket::disconnected, this, &WQConnector::wqDisconnected);
    connect(m_ws, &QWebSocket::binaryFrameReceived, this, &WQConnector::wqBinaryFrameReceived);
    connect(m_ws, &QWebSocket::binaryMessageReceived, this, &WQConnector::wqBinaryMessageReceived);
    connect(m_ws, &QWebSocket::textFrameReceived, this, &WQConnector::wqTextFrameReceived);
    connect(m_ws, &QWebSocket::textMessageReceived, this, &WQConnector::wqTextMessageReceived);
    connect(m_ws, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(wqError(QAbstractSocket::SocketError)));
    connect(m_ws, &QWebSocket::sslErrors, this, &WQConnector::wqSslErrors);
    m_user=usr;
    m_password=psw;
    //wss://junior.webquik.ru:443/quik
    QUrl url(QString("wss://%1:443/quik").arg(srv));
    QNetworkRequest request{url};
    //request.setRawHeader("Sec-WebSocket-Protocol", "dumb-increment-protocol");
    m_ws->open(request);
    return true;
}

void WQConnector::wqDisconnect()
{
    if(m_ws)
    {
        m_ws->disconnect();
        m_ws->deleteLater();
        m_ws=nullptr;
        emit exchangeStopped();
    }
}

void WQConnector::sendLogin()
{
    if(!m_ws)
        return;
    QString sid=QString("12563a%1").arg(QDateTime::currentDateTime().toMSecsSinceEpoch(),0,16).right(6);
    QString localTime = QDateTime::currentDateTime().toString(Qt::SystemLocaleLongDate);
    QString msg=QString("{\"msgid\":10000,\"login\":\"%1\",\"password\":\"%2\",\"btc\":\"true\","
                "\"width\":\"1479\",\"height\":\"558\","
                "\"userAgent\":\"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_1) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/13.0.3 Safari/605.1.15\","
                "\"lang\":\"ru\",\"sid\":\"%4\",\"version\":\"7.0.1\","
                "\"device_desc\":\"SYSTEM=MacIntel%3OS-VERSION=%3LOCAL_TIME=%5:00%3USER_LOCAL_INFO=en-US\"}").
            arg(m_user).arg(m_password).arg(QChar('%')).arg(sid).arg(localTime);
    m_ws->sendTextMessage(msg);
}

void WQConnector::sendStatusCheck()
{
    if(!m_ws)
        return;
    QString msg=QString("{\"msgid\":10008}");
    m_ws->sendTextMessage(msg);
    QTimer::singleShot(3000, this, SLOT(restartStatusChecking()));
}

void WQConnector::sendPortfolioCalcParams()
{
    if(!m_ws)
        return;
    //{"msgid":10010,"params":[{"firmId":"NC0011100000","curr":"SUR","tag":"EQTV"}]}
    if(defaultFirmKey.isEmpty())
        return;
    QString msg=QString("{\"msgid\":10010,\"params\":[{\"firmId\":\"%1\",\"curr\":\"%2\",\"tag\":\"%3\"}]}").
            arg(firms[defaultFirmKey].firmId).arg(firms[defaultFirmKey].curr).arg(firms[defaultFirmKey].tag);
    m_ws->sendTextMessage(msg);
}

void WQConnector::sendBookClientPortfolio()
{
    if(!m_ws)
        return;
    sendPortfolioCalcParams();
    QString msg=QString("{\"msgid\":11013}");
    m_ws->sendTextMessage(msg);
}

void WQConnector::sendUnbookClientPortfolio()
{
    if(!m_ws)
        return;
    QString msg=QString("{\"msgid\":11113}");
    m_ws->sendTextMessage(msg);
}

void WQConnector::sendBookBuySell()
{
    if(!m_ws)
        return;
    QString msg=QString("{\"msgid\":11012}");
    m_ws->sendTextMessage(msg);
}

void WQConnector::sendUnbookBuySell()
{
    if(!m_ws)
        return;
    QString msg=QString("{\"msgid\":11112}");
    m_ws->sendTextMessage(msg);
}

void WQConnector::processArrivedJson(QJsonDocument &jsonDoc)
{
    //qDebug() << "processArrivedJson:";
    //qDebug() << QString::fromLocal8Bit(jsonDoc.toJson());
    if(jsonDoc.isObject())
    {
        QVariantMap vmap=jsonDoc.object().toVariantMap();
        if(vmap.contains("msgid"))
        {
            int msgid=vmap.value("msgid").toInt();
            qDebug() << "msgid:" << msgid;
            switch(msgid)
            {
            case 20006:
                processConnectionEstablished(vmap);
                break;
            case 20014:
                processServerMessage(vmap);
                break;
            case 20000:
                processLogin(vmap);
                break;
            case 21000:
                processClassItem(vmap);
                break;
            //case 26000:   //что-то для завершения регистрации/изменения/восстановления пароля
            case 21024:
                processFxClassesKeys(vmap);
                break;
            case 21022:
                processTrdAccsListUpdate(vmap);
                break;
            case 21004:
                processCashLimitsUpdate(vmap);
                break;
            case 21063:
                qDebug() << QString::fromLocal8Bit(jsonDoc.toJson());
                break;
            case 21005:
                processDepoLimitsUpdate(vmap);
                break;
            case 21006:
                processFutClientLimitsUpdate(vmap);
                break;
            case 21011:
                processParamsUpdate(vmap);
                break;
            default:
                printToLog(QString::fromLocal8Bit(jsonDoc.toJson()));
            }
        }
        else
        {
            qDebug() << "Unknown message id";
            printToLog(QString::fromLocal8Bit(jsonDoc.toJson()));
        }
    }
    else
    {
        qDebug() << "Json doc is not object";
        printToLog(QString::fromLocal8Bit(jsonDoc.toJson()));
    }
}

void WQConnector::processConnectionEstablished(QVariantMap &msg)
{
    int resCode = -1;
    if(msg.contains("authMode"))
        qDebug() << "authMode: " << msg.value("authMode").toString();
    if(msg.contains("resultCode"))
    {
        resCode = msg.value("resultCode").toInt();
        qDebug() << "resultCode: " << msg.value("resultCode").toString();
    }
    if(msg.contains("serverMessage"))
        emit serverMessage(msg.value("serverMessage").toString());
    switch(resCode)
    {
    case 0:
        break;
    case 1:
        printToLog("Соединение с сервером QUIK недоступно. Обратитесь к брокеру.");
        break;
    case 2:
        printToLog("Неверные авторизационные данные");
        break;
    case 3:
        printToLog("Логин пользователя заблокирован. Обратитесь к брокеру.");
        break;
    case 5:
        printToLog("Соединение с сервером QUIK недоступно.");
        break;
    case 4:
    default:
        printToLog("Хрен знает в чём дело, но подключиться не шмогли");
        break;
    }
    if(resCode)
        emit fatalError();
}

void WQConnector::processLogin(QVariantMap &msg)
{
    QString uid("UID");
    if(msg.contains("uid"))
    {
        userId=msg.value("uid").toULongLong();
        uid=QString("%1").arg(userId);
    }
    if(msg.contains("tradeSession"))
    {
        QVariantMap ts=msg.value("tradeSession").toMap();
        //20191115
        QDate dt=QDate::fromString(ts.value("date").toString(),"yyyyMMdd");
        quint64 flgs=ts.value("flags").toULongLong();
        sessionId=ts.value("id").toString();
        QString sessid=QString("%1::%2").arg(uid).arg(sessionId);
        emit tradeSessionOpen(dt, flgs, sessid);
    }
    if(msg.contains("classList"))
        processClassItem(msg);
    if(msg.contains("userProperties"))
    {
        QVariantMap uprops=msg.value("userProperties").toMap();
        if(uprops.contains("portfolioCalcParams"))
        {
            QVariantMap allfirms = uprops.value("portfolioCalcParams").toMap();
            for(QVariantMap::const_iterator iter = allfirms.begin(); iter != allfirms.end(); ++iter)
            {
                QVariantMap fdata = iter.value().toMap();
                FirmData nfirm;
                if(fdata.contains("curr"))
                    nfirm.curr = fdata.value("curr").toString();
                if(fdata.contains("firmId"))
                    nfirm.firmId = fdata.value("firmId").toString();
                if(fdata.contains("tag"))
                    nfirm.tag = fdata.value("tag").toString();
                firms.insert(iter.key(), nfirm);
                if(defaultFirmKey.isEmpty())
                    defaultFirmKey = iter.key();
            }
            sendBookClientPortfolio();
            sendBookBuySell();
        }
        else
        {
            qDebug() << "No user propertioes was received, can't operate";
        }
    }
}

void WQConnector::processServerMessage(QVariantMap &msg)
{
    if(msg.contains("datetime"))
    {
        serverDateTime=QDateTime::fromString(msg.value("datetime").toString(),"yyyy-MM-dd HH:mm:ss");
        emit serverDateTimeChanged(serverDateTime);
    }
    if(msg.contains("text"))
    {
        emit serverMessage(msg.value("text").toString());
    }
}

void WQConnector::processDepoLimitsUpdate(QVariantMap &msg)
{
    qDebug() << "processDepoLimitsUpdate";
    printToLog(msg.keys().join("\n"));
}

void WQConnector::processFutClientLimitsUpdate(QVariantMap &msg)
{
    qDebug() << "processFutClientLimitsUpdate";
    printToLog(msg.keys().join("\n"));
}

void WQConnector::processCashLimitsUpdate(QVariantMap &msg)
{
    qDebug() << "processCashLimitsUpdate";
    QString ucode, firmid;
    if(msg.contains("ucode"))
        ucode = msg.value("ucode").toString();
    else
    {
        qDebug() << "No ucode";
        return;
    }
    if(msg.contains("firmid"))
        firmid = msg.value("firmid").toString();
    else
    {
        qDebug() << "No firmid";
        return;
    }
    ClientCode *cc;
    bool exists=false;
    if(clientCodes.contains(ucode))
    {
        cc = clientCodes.value(ucode);
        exists=true;
    }
    else
        cc = new ClientCode;
    if(!cc->initFromFirmId(ucode, firmid, tradeAccounts.values()))
        qDebug() << "Couldn't find corresponding trade account!";
    clientCodes.insert(ucode, cc);
}

void WQConnector::processParamsUpdate(QVariantMap &msg)
{
    qDebug() << "processParamsUpdate";
    printToLog(msg.keys().join("\n"));
}

void WQConnector::processTrdAccsListUpdate(QVariantMap &msg)
{
    qDebug() << "processTrdAccsListUpdate";
    TradeAccountInfo *ntacc = new TradeAccountInfo;
    if(ntacc->initFrom(msg))
    {
        QString key = QString("%1@%2").arg(ntacc->account).arg(ntacc->firmid);
        if(tradeAccounts.contains(key))
            delete tradeAccounts.take(key);
        tradeAccounts.insert(key, ntacc);
    }
    else
    {
        qDebug() << "Error during receiving trade account information";
        delete ntacc;
    }
}

void WQConnector::processClassItem(QVariantMap &cls)
{
    qDebug() << "processClassItem";
    if(!cls.contains("classList"))
    {
        qDebug() << "classList was not sent in classList update";
        return;
    }
    QVariantList clist = cls.value("classList").toList();
    int i, j;
    for(i=0;i<clist.count();i++)
    {
        QVariantMap classDesc = clist.at(i).toMap();
        if(!classDesc.contains("ccode"))
            continue;
        if(!classDesc.contains("type"))
            continue;
        QString ccode = classDesc.value("ccode").toString().trimmed();
        SecClassSpec *updcspec;
        if(classList.contains(ccode))
            updcspec=classList.value(ccode);
        else
        {
            updcspec = new SecClassSpec;
            classList.insert(ccode, updcspec);
        }
        updcspec->classCode = ccode;
        int ctp = classDesc.value("type").toInt();
        updcspec->classType = (WQ_CLASS_TYPES)ctp;
        if(classDesc.contains("paramList"))
        {
            QVariantList plist = classDesc.value("paramList").toList();
            for(j=0;j<plist.count();j++)
            {
                ParamSpec *npsc = new ParamSpec;
                npsc->initFrom(plist.at(j).toMap());
                if(npsc->quikname.isEmpty())
                    delete npsc;
                else
                    updcspec->params.insert(npsc->quikname, npsc);
            }
        }
        if(classDesc.contains("cname"))
            updcspec->className = classDesc.value("cname").toString();
        if(classDesc.contains("enableQuotes"))
            updcspec->enableQuotes = classDesc.value("enableQuotes").toBool();
        if(classDesc.contains("enableStop"))
            updcspec->enableStop = classDesc.value("enableStop").toBool();
        if(classDesc.contains("enableStopPeriod"))
            updcspec->enableStopPeriod = classDesc.value("enableStopPeriod").toBool();
        if(classDesc.contains("enableTran"))
            updcspec->enableTran = classDesc.value("enableTran").toBool();
        if(classDesc.contains("firmid"))
            updcspec->firmid = classDesc.value("firmid").toString();
        if(classDesc.contains("isCurrency"))
            updcspec->isCurrency = classDesc.value("isCurrency").toBool();
        if(classDesc.contains("isFutures"))
            updcspec->isFutures = classDesc.value("isFutures").toBool();
        if(classDesc.contains("isNegot"))
            updcspec->isNegot = classDesc.value("isNegot").toBool();
        if(classDesc.contains("isObligation"))
            updcspec->isObligation = classDesc.value("isObligation").toBool();
        if(classDesc.contains("isRepo"))
            updcspec->isRepo = classDesc.value("isRepo").toBool();
        if(classDesc.contains("pars_hash"))
            updcspec->parsHash = classDesc.value("pars_hash").toString();
        if(classDesc.contains("perms"))
            updcspec->perms = classDesc.value("perms").toString();
        if(classDesc.contains("secs_hash"))
            updcspec->secsHash = classDesc.value("secs_hash").toString();
        if(classDesc.contains("subType"))
            updcspec->subType = classDesc.value("subType").toInt();
        if(classDesc.contains("tranAvailable"))
            updcspec->tranAvailable = classDesc.value("tranAvailable").toBool();
        if(classDesc.contains("secList"))
        {
            QVariantList slist = classDesc.value("secList").toList();
            for(j=0;j<slist.count();j++)
            {
                SecSpec *nss = new SecSpec;
                nss->initFrom(slist.at(j).toMap());
                if(nss->secCode.isEmpty())
                    delete nss;
                else
                    updcspec->securities.insert(nss->secCode, nss);
            }
        }
    }
}

void WQConnector::processFxClassesKeys(QVariantMap &fxcls)
{
    if(!fxcls.contains("classList"))
    {
        qDebug() << "classList was not sent to fxClassesKeys";
        return;
    }
    fxClassesKeys = fxcls.value("classList").toStringList();
}

void WQConnector::printToLog(QString msg)
{
    emit simpleLog(msg);
}

void WQConnector::restartStatusChecking()
{
    sendStatusCheck();
}

void WQConnector::wqConnected()
{
    sendLogin();
    emit startExchange();
}

void WQConnector::wqDisconnected()
{
    qDebug() << "disconnected";
}

void WQConnector::wqBinaryFrameReceived(const QByteArray &frame, bool isLastFrame)
{
    //qDebug() << "wqBinaryFrameReceived(frame, " << isLastFrame << ")";
    //qDebug() << QString::fromLocal8Bit(frame);
    framecollection.append(frame);
    if(isLastFrame)
    {
        wqBinaryMessageReceived(framecollection);
        framecollection.clear();
    }
}

void WQConnector::wqBinaryMessageReceived(const QByteArray &message)
{
    //qDebug() << "wqBinaryMessageReceived";
    //qDebug() << QString::fromLocal8Bit(message);
    QJsonDocument jsonDoc=QJsonDocument::fromJson(message);
    processArrivedJson(jsonDoc);
}

void WQConnector::wqTextFrameReceived(const QString &frame, bool isLastFrame)
{
    //qDebug() << "wqTextFrameReceived";
    //qDebug() << frame;
    stringframecollection.append(frame);
    if(isLastFrame)
    {
        wqTextMessageReceived(stringframecollection);
        stringframecollection.clear();
    }
}

void WQConnector::wqTextMessageReceived(const QString &message)
{
    //qDebug() << "wqTextFrameReceived";
    //qDebug() << message;
    QJsonDocument jsonDoc=QJsonDocument::fromJson(message.toUtf8());
    processArrivedJson(jsonDoc);
}

void WQConnector::wqError(QAbstractSocket::SocketError error)
{
    qDebug() << "wqError";
}

void WQConnector::wqSslErrors(const QList<QSslError> &errors)
{
    qDebug() << "wqSslErrors";
}

void ParamSpec::initFrom(const QVariantMap &vmap)
{
    if(vmap.contains("quikname"))
        quikname = vmap.value("quikname").toString();
    else
    {
        quikname = QString();
        return;
    }
    if(vmap.contains("name"))
        name = vmap.value("name").toString();
    if(vmap.contains("long_name"))
        longName = vmap.value("long_name").toString();
    if(vmap.contains("flags"))
        flags = vmap.value("flags").toInt();
    if(vmap.contains("type"))
        pType = vmap.value("type").toInt();
    if(vmap.contains("scale"))
        scale = vmap.value("scale").toInt();
    if(vmap.contains("enumDesc"))
    {
        QVariantList enud = vmap.value("enumDesc").toList();
        int i;
        QString res;
        for(i=0;i<enud.count();i++)
        {
            if(!res.isEmpty())
                res.append(",");
            res.append(enud.at(i).toString());
        }
        enumDesc = res;
    }
}

void SecSpec::initFrom(const QVariantMap &vmap)
{
    if(vmap.contains("scode"))
        secCode = vmap.value("scode").toString();
    else
    {
        secCode = QString();
        return;
    }
    if(vmap.contains("base_ccode"))
        baseCCode = vmap.value("base_ccode").toString();
    if(vmap.contains("base_scode"))
        baseSCode = vmap.value("base_scode").toString();
    if(vmap.contains("face_unit"))
        faceUnit = vmap.value("face_unit").toString();
    if(vmap.contains("isin"))
        isin = vmap.value("isin").toString();
    if(vmap.contains("long_name"))
        longName = vmap.value("long_name").toString();
    if(vmap.contains("sname"))
        secName = vmap.value("sname").toString();
    if(vmap.contains("trade_currency"))
        tradeCurrency = vmap.value("trade_currency").toString();
    if(vmap.contains("face_value"))
        faceValue = vmap.value("face_value").toInt();
    if(vmap.contains("first_curr_qty_scale"))
        firstCurrQtyScale = vmap.value("first_curr_qty_scale").toInt();
    if(vmap.contains("lot"))
        lot = vmap.value("lot").toInt();
    if(vmap.contains("mat_date"))
        matDate = vmap.value("mat_date").toInt();
    if(vmap.contains("qty_multiplier"))
        qtyMult = vmap.value("qty_multiplier").toInt();
    if(vmap.contains("qty_scale"))
        qtyScale = vmap.value("qty_scale").toInt();
    if(vmap.contains("scale"))
        scale = vmap.value("scale").toInt();
    if(vmap.contains("second_curr_qty_scale"))
        secondCurrQtyScale = vmap.value("second_curr_qty_scale").toInt();
    if(vmap.contains("specFlags"))
        specFlags = vmap.value("specFlags").toInt();
    if(vmap.contains("step"))
        step = vmap.value("step").toInt();
    if(vmap.contains("type"))
        secType = (WQ_CLASS_TYPES)vmap.value("type").toInt();
    if(vmap.contains("subType"))
        subType = vmap.value("subType").toInt();
}

bool TradeAccountInfo::isFutures(const QMap<QString, SecClassSpec *> &clList)
{
    int i;
    for(i=0;i<classList.count();i++)
    {
        if(!clList.contains(classList.at(i)))
            continue;
        if(clList.value(classList.at(i))->isFutures)
            return true;
    }
    return false;
}

bool TradeAccountInfo::isCurrency(const QMap<QString, SecClassSpec *> &clList)
{
    int i;
    for(i=0;i<classList.count();i++)
    {
        if(!clList.contains(classList.at(i)))
            continue;
        if(clList.value(classList.at(i))->isCurrency)
            return true;
    }
    return false;
}

bool TradeAccountInfo::isForex(const QStringList &fxClassesKeys)
{
    int i;
    for(i=0;i<classList.count();i++)
    {
        if(fxClassesKeys.contains(classList.at(i)))
            return true;
    }
    return false;
}

bool TradeAccountInfo::isInstruction(const QMap<QString, SecClassSpec *> &clList)
{
    int i;
    for(i=0;i<classList.count();i++)
    {
        if(!clList.contains(classList.at(i)))
            continue;
        if(clList.value(classList.at(i))->classType==INSTRUCTIONS)
            return true;
    }
    return false;
}

bool TradeAccountInfo::initFrom(const QVariantMap &vmap)
{
    if(vmap.contains("trdacc"))
        account = vmap.value("trdacc").toString();
    else
    {
        account = QString();
        return false;
    }
    if(vmap.contains("firmid"))
        firmid = vmap.value("firmid").toString();
    else
        return false;
    if(vmap.contains("limitsInLots"))
        limitsInLots = vmap.value("limitsInLots").toInt();
    if(vmap.contains("classList"))
        classList = vmap.value("classList").toStringList();
    if(vmap.contains("mainMarginClasses"))
        mainMarginClassList = vmap.value("mainMarginClasses").toStringList();
    if(vmap.contains("limitKinds"))
    {
        QVariantList lklist = vmap.value("limitKinds").toList();
        int i;
        for(i=0;i<lklist.count();i++)
        {
            bool ok=false;
            int lk = lklist.at(i).toInt(&ok);
            if(ok)
                limitKinds.append(lk);
        }
    }
    return true;
}

bool ClientCode::initFromFirmId(QString userCode, QString firmId, const QList<TradeAccountInfo *> &allAccs)
{
    int i;
    ucode=userCode;
    for(i=0;i<allAccs.count();i++)
    {
        if(allAccs.at(i)->firmid==firmId)
            accounts.append(allAccs.at(i));
    }
    if(accounts.count())
        return true;
    return false;
}
