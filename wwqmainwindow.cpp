#include "wwqmainwindow.h"
#include "ui_wwqmainwindow.h"

WWQMainWindow::WWQMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::WWQMainWindow)
{
    ui->setupUi(this);
    serverDt = new QLabel(this);
    serverDt->setObjectName(QString::fromUtf8("serverDt"));
    ui->statusbar->addPermanentWidget(serverDt);
    sessionId = new QLabel(this);
    sessionId->setObjectName(QString::fromUtf8("sessionId"));
    ui->statusbar->addPermanentWidget(sessionId);

    connect(&conn, &WQConnector::startExchange, this, &WWQMainWindow::startExchange);
    connect(&conn, &WQConnector::exchangeStopped, this, &WWQMainWindow::exchangeStopped);
    connect(&conn, &WQConnector::simpleLog, this, &WWQMainWindow::simpleLog);
    connect(&conn, &WQConnector::serverMessage, this, &WWQMainWindow::onServerMessage);
    connect(&conn, &WQConnector::fatalError, this, &WWQMainWindow::fatalErrorDisconnect);
}

WWQMainWindow::~WWQMainWindow()
{
    delete ui;
}

void WWQMainWindow::startExchange()
{
    ui->cbServer->setEnabled(false);
    ui->leUser->setEnabled(false);
    ui->lePassword->setEnabled(false);
    ui->pbConnect->setText("Disconnect");
}

void WWQMainWindow::exchangeStopped()
{
    ui->cbServer->setEnabled(true);
    ui->leUser->setEnabled(true);
    ui->lePassword->setEnabled(true);
    ui->pbConnect->setText("Connect");
}

void WWQMainWindow::simpleLog(QString msg)
{
    ui->pteLog->appendPlainText(msg);
}

void WWQMainWindow::onServerMessage(QString msg)
{
    ui->pteServerMessages->appendPlainText(msg);
}

void WWQMainWindow::tradeSessionOpen(QDate dt, quint64 flags, QString id)
{
    Q_UNUSED(flags)
    serverDt->setText(dt.toString("dd.MM.yyyy"));
    sessionId->setText(id);
}

void WWQMainWindow::fatalErrorDisconnect()
{
    conn.wqDisconnect();
}

void WWQMainWindow::on_pbConnect_clicked()
{
    if(ui->pbConnect->text()=="Connect")
    {
        QString url=ui->cbServer->currentText();
        QString usr=ui->leUser->text();
        QString psq=ui->lePassword->text();
        conn.wqConnect(url, usr, psq);
        startExchange();
    }
    else
    {
        conn.wqDisconnect();
    }
}
