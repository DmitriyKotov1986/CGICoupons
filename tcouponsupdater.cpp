#include "tcouponsupdater.h"

//Qt
#include <QSqlError>
#include <QSqlQuery>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QVector>

//My
#include "Common/common.h"

using namespace Common;

using namespace CGICoupons;

TCouponsUpdater::TCouponsUpdater(CGICoupons::TConfig* cfg)

{
    Q_ASSERT(cfg != nullptr);

    //настраиваем БД
    _db = QSqlDatabase::addDatabase(cfg->db_Driver(), "MainDB");
    _db.setDatabaseName(cfg->db_DBName());
    _db.setUserName(cfg->db_UserName());
    _db.setPassword(cfg->db_Password());
    _db.setConnectOptions(cfg->db_ConnectOptions());
    _db.setPort(cfg->db_Port());
    _db.setHostName(cfg->db_Host());
}

TCouponsUpdater::~TCouponsUpdater()
{
    if (_db.isOpen())
    {
        _db.close();
    }
}

int TCouponsUpdater::run(const QString &XMLText)
{
    writeDebugLogFile("REQUEST>", XMLText);

    if (XMLText.isEmpty())
    {
        return EXIT_CODE::XML_EMPTY;
    }

    if (!_db.open())
    {
        _errorString = "Cannot connet to DB. Error: " + _db.lastError().text();
        return EXIT_CODE::SQL_NOT_OPEN_DB;
    }

    QTextStream textStream(stderr);

    //парсим XML
    QXmlStreamReader XMLReader(XMLText);
    QString AZSCode = "n/a";
    QString clientVersion = "n/a";
    QString protocolVersion = "n/a";

    quint64 lastRequesterID = 0;
    quint64 lastUpdateLocal2ServerID = 0;
    quint64 maxCoupons = 100;

    qint64 couponsCount = -1;

    QList<TCouponData> newCoupons;

    while ((!XMLReader.atEnd()) && (!XMLReader.hasError()))
    {
        QXmlStreamReader::TokenType token = XMLReader.readNext();
        if (token == QXmlStreamReader::StartDocument) continue;
        else if (token == QXmlStreamReader::EndDocument) break;
        else if (token == QXmlStreamReader::StartElement)
        {
            //qDebug() << XMLReader.name().toString();
            if (XMLReader.name().toString()  == "Root")
            {
                while ((XMLReader.readNext() != QXmlStreamReader::EndElement) || XMLReader.atEnd() || XMLReader.hasError()) {
                    //qDebug() << "Root/" << XMLReader.name().toString();
                    if (XMLReader.name().toString().isEmpty())
                    {
                        continue;
                    }
                    if (XMLReader.name().toString()  == "AZSCode")
                    {
                        AZSCode = XMLReader.readElementText();
                    }
                    else if (XMLReader.name().toString()  == "ClientVersion")
                    {
                        clientVersion = XMLReader.readElementText();
                    }
                    else if (XMLReader.name().toString()  == "ProtocolVersion")
                    {
                        protocolVersion = XMLReader.readElementText();
                    }
                    else if (XMLReader.name().toString()  == "LastID")
                    {
                        bool ok = true;
                        lastRequesterID = lastUpdateLocal2ServerID = XMLReader.readElementText().toULongLong(&ok);
                        if (!ok)
                        {
                            _errorString = "Incorrect value tag (" + XMLReader.name().toString() + "). Must be unsigneg numeric.";
                            textStream << _errorString; //выводим сообщение об ошибке в cin для отправки клиенту
                            return EXIT_CODE::XML_PARSE_ERR;
                        }
                    }
                    else if (XMLReader.name().toString()  == "MaxCoupons") {
                        QString tmpStr = XMLReader.readElementText();
                        if (!tmpStr.isEmpty())
                        {
                            bool ok = true;
                            maxCoupons = tmpStr.toULongLong(&ok);
                            if (!ok)
                            {
                                _errorString = "Incorrect value tag (" + XMLReader.name().toString() + "). Must be unsigneg numeric. Value: " + tmpStr;
                                textStream << _errorString; //выводим сообщение об ошибке в cin для отправки клиенту
                                return EXIT_CODE::XML_PARSE_ERR;
                            }
                            maxCoupons = std::min(maxCoupons, quint64(10000));
                        }
                    }

                    else if (XMLReader.name().toString()  == "CouponsCount")
                    {
                        bool ok = true;
                        couponsCount = XMLReader.readElementText().toULongLong(&ok);
                        if (!ok)
                        {
                            _errorString = "Incorrect value tag (" + XMLReader.name().toString() + "). Must be numeric.";
                            textStream << _errorString; //выводим сообщение об ошибке в cin для отправки клиенту
                            return EXIT_CODE::XML_PARSE_ERR;
                        }
                    }

                    else if (XMLReader.name().toString()  == "Coupon")
                    {
                        TCouponData tmp;
                        while ((XMLReader.readNext() != QXmlStreamReader::EndElement) || XMLReader.atEnd() || XMLReader.hasError())
                        {
                            if (XMLReader.name().toString().isEmpty())
                            {
                                continue;
                            }
                            else if (XMLReader.name().toString()  == "Code")
                            {
                                tmp.Code = XMLReader.readElementText();
                                if (tmp.Code.length() > 10) {
                                    _errorString = QString("Length of code must be <= 10. Value: '%1'").arg(tmp.Code);
                                    textStream << _errorString; //выводим сообщение об ошибке в cin для отправки клиенту
                                    return EXIT_CODE::XML_INCORRECT_VALUE;
                                }

                            }
                            else if (XMLReader.name().toString()  == "Active")
                            {
                                bool ok = true;
                                tmp.Active = XMLReader.readElementText().toUInt(&ok);
                                if (!ok)
                                {
                                    _errorString = "Incorrect value tag (Coupons/Active" + XMLReader.name().toString() + "). Must be 0 or 1.";
                                    textStream << _errorString; //выводим сообщение об ошибке в cin для отправки клиенту
                                    return EXIT_CODE::XML_PARSE_ERR;
                                }
                                if ((AZSCode != "000")&&(tmp.Active == 1))
                                {
                                    _errorString = QString("The value Active=1 cannot come from AZS");
                                    textStream << _errorString; //выводим сообщение об ошибке в cin для отправки клиенту
                                    return EXIT_CODE::XML_INCORRECT_VALUE;
                                }

                            }
                            else if (XMLReader.name().toString()  == "UpdateDateTime")
                            {
                                tmp.UpdateDateTime = QDateTime::fromString(XMLReader.readElementText(), "yyyy-MM-dd hh:mm:ss.zzz");
                            }
                            else
                            {
                                _errorString = "Undefine tag in XML (Coupons/" + XMLReader.name().toString() + ")";
                                textStream << _errorString; //выводим сообщение об ошибке в cin для отправки клиенту
                                return EXIT_CODE::XML_UNDEFINE_TOCKEN;
                            }
                        }

                        if (tmp.Code != "") newCoupons.push_back(tmp);
                    }

                    else
                    {
                        _errorString = "Undefine tag in XML (Root/" + XMLReader.name().toString() + ")";
                        textStream << _errorString; //выводим сообщение об ошибке в cin для отправки клиенту
                        return EXIT_CODE::XML_UNDEFINE_TOCKEN;
                    }
                }
            }
        }
        else
        {
            _errorString = "Undefine token in XML";
            textStream << _errorString; //выводим сообщение об ошибке в cin для отправки клиенту
            return EXIT_CODE::XML_UNDEFINE_TOCKEN;
        }
    }

    if (XMLReader.hasError()) //неудалось распарсить пришедшую XML
    {
        _errorString = "Cannot parse XML query. Message: " + XMLReader.errorString();
        textStream << _errorString; //выводим сообщение об ошибке в cin для отправки клиенту
        return EXIT_CODE::XML_PARSE_ERR;
    }

    if ((couponsCount != -1) && (newCoupons.size() != couponsCount))
    {
        _errorString = QString("Incorrect value tag <CouponsCount>. Value:%1, coupons count: %2").arg(newCoupons.size()).arg(couponsCount);
        textStream << _errorString; //выводим сообщение об ошибке в cin для отправки клиенту
        return EXIT_CODE::XML_PARSE_ERR;
    }

    //если есть что добавлять - сохраняем в БД
    quint64 couponsOfInsert = 0;
    if (!newCoupons.isEmpty()) {
        //Вставляем новые записи
        for (const auto &item : newCoupons) {
            QString queryText = "INSERT INTO [Coupons] ([Code] , [Status], [AZSCode], [DateTime]) "
                                "VALUES ( "
                                "'" + item.Code + "', " +
                                QString::number(item.Active) + ", " +
                                "'" + AZSCode + "', " +
                                "CAST('" + item.UpdateDateTime.toString("yyyy-MM-dd hh:mm:ss.zzz") + "' AS DATETIME2))";

            DBQueryExecute(_db, queryText);

            ++couponsOfInsert;
        }
    }



    //подготавливаем ответ
    QSqlQuery query(_db);
    query.setForwardOnly(true);
    _db.transaction();

    QString queryText = "SELECT TOP (" + QString::number(maxCoupons) + ") [ID], [DateTime], [Code], [Status], [AZSCode] "
                "FROM [Coupons] "
                "WHERE ([ID] >= " + QString::number(lastUpdateLocal2ServerID) + ") AND ([AZSCode] <> '" + AZSCode + "') " +
                "ORDER BY [ID] ";

    writeDebugLogFile(QString("QUERY TO %1>").arg(_db.connectionName()), queryText);

    if (!query.exec(queryText)) {
        errorDBQuery(_db, query);
    }

    QString XMLAnswer;
    QXmlStreamWriter XMLWriter(&XMLAnswer);
    XMLWriter.setAutoFormatting(true);
    XMLWriter.writeStartDocument("1.0");
    XMLWriter.writeStartElement("Root");
    XMLWriter.writeTextElement("ProtocolVersion", "0.1");
    quint64 couponsOfSelect = 0;

    while (query.next())
    {
        XMLWriter.writeStartElement("Coupon");
        XMLWriter.writeTextElement("Code", query.value("Code").toString());
        XMLWriter.writeTextElement("Active", (query.value("Status").toUInt() == 0 ? "0" : "1"));
        XMLWriter.writeTextElement("UpdateDateTime", query.value("DateTime").toDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));
        XMLWriter.writeTextElement("AZSCode", query.value("AZSCode").toString());
        XMLWriter.writeEndElement();
        lastUpdateLocal2ServerID = query.value("ID").toULongLong();
        ++couponsOfSelect;
    }
    if (couponsCount != -1)
    {
        XMLWriter.writeTextElement("CouponsProcessed", QString::number(couponsOfInsert));
        XMLWriter.writeTextElement("CouponsCount", QString::number(couponsOfSelect));
    }
    XMLWriter.writeTextElement("LastID", QString::number(lastUpdateLocal2ServerID));

    XMLWriter.writeEndElement();
    XMLWriter.writeEndDocument();

    query.finish();

    DBCommit(_db);

    //Сохраняем сведения о последнем обновлении

    queryText = "UPDATE [AZSInfo] SET "
                "[LastSentCouponsID] = " + QString::number(lastUpdateLocal2ServerID) + ", "
                "[LastRequestedCouponsID] = "  + QString::number(lastRequesterID) + ", "
                "[LastCouponsDateTime] = CAST('" + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") + "' AS DATETIME2) "

                "WHERE ([AZSCode] = '" + AZSCode + "')";

    DBQueryExecute(_db, queryText);

    queryText = "INSERT INTO [CouponsInfo] ([AZSCode], [LastSentCouponsID], [LastRequestedCouponsID], [LastCouponsDateTime]) VALUES ( "
                "'" + AZSCode + "', " +
                QString::number(lastUpdateLocal2ServerID) + ", " +
                QString::number(lastRequesterID) + ", " +
                "CAST('" + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") + "' AS DATETIME2)) ";

    DBQueryExecute(_db, queryText);

    //отправляем ответ
    QTextStream answerStream(stdout);
    answerStream << XMLAnswer;

    writeDebugLogFile("ANSWER>", XMLAnswer);

    return 0;
}
