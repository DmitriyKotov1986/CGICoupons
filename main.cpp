//Qt
#include <QCoreApplication>
#include <QTimer>
#include <QThread>
#include <QSettings>

//My
#include "Common/common.h"
#include "tconfig.h"
#include "tcouponsupdater.h"

using namespace Common;

using namespace CGICoupons;

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName("CGICoupons");
    QCoreApplication::setOrganizationName("OOO SA");
    QCoreApplication::setApplicationVersion(QString("Version:0.2a Build: %1 %2").arg(__DATE__).arg(__TIME__));

    setlocale(LC_CTYPE, ""); //настраиваем локаль

    QString configFileName = a.applicationDirPath() +"/CGICoupons.ini";

    CGICoupons::TConfig* cfg = TConfig::config(configFileName);

    if (cfg->isError()) {
        QString errorMsg = "Error load configuration: " + cfg->errorString();
        qCritical() << errorMsg;
        writeLogFile("Error load configuration", errorMsg);
        exit(EXIT_CODE::LOAD_CONFIG_ERR);
    }

    QString buf;
    QTextStream inputStream(stdin);
    while (1) {
        QString tmpStr = inputStream.readLine();
        if (tmpStr != "EOF") {
            buf += tmpStr + "\n";
        }
        else {
            break;
        }
    }

    CGICoupons::TCouponsUpdater couponsUpdater(cfg);

    int res = couponsUpdater.run(buf); //обрабатываем пришедшие данные

    if (res != 0 ) {
        writeLogFile("Error parse XML: " + couponsUpdater.errorString(), buf);
    }
    else {
        qDebug() << "OK";
    }

    return res;
}
