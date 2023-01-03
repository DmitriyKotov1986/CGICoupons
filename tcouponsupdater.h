#ifndef TCOUPONSUPDATER_H
#define TCOUPONSUPDATER_H

//QT
#include <QSettings>
#include <QtSql/QSqlDatabase>
#include <QDateTime>
#include <QString>

//My
#include "tconfig.h"

namespace CGICoupons
{

class TCouponsUpdater
{
private:
    struct TCouponData{
        QString Code; //Код талона
        uint8_t Active = 0;  //Текущий статус талона
        QDateTime UpdateDateTime = QDateTime::fromString("2000-01-01 00:00:00.001", "yyyy-MM-dd hh:mm:ss.zzz"); //время обновления
        QString AZSCode = ""; //код АЗС
    };

public:
    explicit TCouponsUpdater(CGICoupons::TConfig* cfg);
    ~TCouponsUpdater();

    int run(const QString& XMLText);
    QString errorString() const { return _errorString; }

private:
    QSqlDatabase _db;
    QString _errorString;

};

}; //namespace CGICoupons

#endif // TCOUPONSUPDATER_H
