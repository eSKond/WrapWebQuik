#ifndef WWQMAINWINDOW_H
#define WWQMAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include "wqconnector.h"
QT_BEGIN_NAMESPACE
namespace Ui { class WWQMainWindow; }
QT_END_NAMESPACE

class WWQMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    WWQMainWindow(QWidget *parent = nullptr);
    ~WWQMainWindow();

private slots:
    void startExchange();
    void exchangeStopped();
    void simpleLog(QString msg);
    void onServerMessage(QString msg);
    void tradeSessionOpen(QDate dt, quint64 flags, QString id);
    void fatalErrorDisconnect();

    void on_pbConnect_clicked();

private:
    Ui::WWQMainWindow *ui;
    QLabel *serverDt;
    QLabel *sessionId;
    WQConnector conn;
};
#endif // WWQMAINWINDOW_H
