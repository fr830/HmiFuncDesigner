﻿#include "tcpserver.h"
#include "HmiRunTime.h"
#include "public.h"
#include "TimerTask.h"
#include "httpserver.h"
#include "VersionInfo.h"
#include "Log.h"
#include "ftpserver.h"
#include "Global.h"
#include "ConfigUtils.h"
#include "edncrypt.h"
#include "gSOAPServer.h"
#include "MainWindow.h"
#include "MemoryMessageService.h"
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <iostream>


#ifndef Q_OS_WIN
#include "eventdispatcher_libev/eventdispatcher_libev.h"
#endif

void customMessageHandler(QtMsgType type,
                          const QMessageLogContext &context,
                          const QString & msg) {
    QString txt;
    switch (type) {
    //调试信息提示
    case QtDebugMsg:
        //            txt = QString("%1: Debug: at:%2,%3 on %4; %5").arg(QTime::currentTime().toString("hh:mm:ss.zzz"))
        //                    .arg(context.file).arg(context.line).arg(context.function).arg(msg);
        txt = msg;
        break;

        //一般的warning提示
    case QtWarningMsg:
        txt = QString("%1:Warning: at:%2,%3 on %4; %5").arg(QTime::currentTime().toString("hh:mm:ss.zzz"))
                .arg(context.file)
                .arg(context.line)
                .arg(context.function)
                .arg(msg);
        break;
        //严重错误提示
    case QtCriticalMsg:
        txt = QString("%1:Critical: at:%2,%3 on %4; %5").arg(QTime::currentTime().toString("hh:mm:ss.zzz"))
                .arg(context.file)
                .arg(context.line)
                .arg(context.function)
                .arg(msg);
        break;
        //致命错误提示
    case QtFatalMsg:
        txt = QString("%1:Fatal: at:%2,%3 on %4; %5").arg(QTime::currentTime().toString("hh:mm:ss.zzz"))
                .arg(context.file)
                .arg(context.line)
                .arg(context.function)
                .arg(msg);
        abort();
    }
    QFile outFile(QString("%1/%2.txt").arg(QDir::currentPath())
                  .arg(QDate::currentDate().toString("yyyy-MM-dd")));
    outFile.open(QIODevice::WriteOnly | QIODevice::Append);
    QTextStream ts(&outFile);
    ts << txt << endl;
}

QString getSystemInfo() {

    QString s = "\n";
    s.append(QString("Build:%1 Version:%2\n").arg(GIT_HASH).arg(VER_FILE));
    s.append("System Info: \n");

    QTextStream out(&s);

    QSysInfo systemInfo;
#ifdef Q_OS_WIN
    out << " Windows Version:          " << systemInfo.windowsVersion()         << endl;
#endif
    out << " Build Cpu Architecture:   " << systemInfo.buildCpuArchitecture()   << endl;
    out << " Current Cpu Architecture: " << systemInfo.currentCpuArchitecture() << endl;
    out << " Kernel Type:              " << systemInfo.kernelType()             << endl;
    out << " Kernel Version:           " << systemInfo.kernelVersion()          << endl;
    out << " Machine Host Name:        " << systemInfo.machineHostName()        << endl;
    out << " Product Type:             " << systemInfo.productType()            << endl;
    out << " Product Version:          " << systemInfo.productVersion()         << endl;
    out << " Byte Order:               " << systemInfo.buildAbi()               << endl;
    out << " Pretty ProductName:       " << systemInfo.prettyProductName()      << endl;

    // root
    QStorageInfo storage = QStorageInfo::root();

    out << storage.rootPath()                                             << endl;
    out << "isReadOnly:" << storage.isReadOnly()                          << endl;
    out << "name:" << storage.name()                                      << endl;
    out << "fileSystemType:" << storage.fileSystemType()                  << endl;
    out << "size:" << storage.bytesTotal()/1000/1000 << "MB"              << endl;
    out << "availableSize:" << storage.bytesAvailable()/1000/1000 << "MB" << endl;

    // current volumn
    QStorageInfo storageCurrent(qApp->applicationDirPath());

    out << qApp->applicationDirPath()                                            << endl;
    out << storageCurrent.rootPath()                                             << endl;
    out << "isReadOnly:" << storageCurrent.isReadOnly()                          << endl;
    out << "name:" << storageCurrent.name()                                      << endl;
    out << "fileSystemType:" << storageCurrent.fileSystemType()                  << endl;
    out << "size:" << storageCurrent.bytesTotal()/1000/1000 << "MB"              << endl;
    out << "availableSize:" << storageCurrent.bytesAvailable()/1000/1000 << "MB" << endl;

    return s;
}

int main(int argc, char *argv[])
{
    int ret = 0;
    //    qInstallMessageHandler(customMessageHandler);
#ifndef Q_OS_WIN
    //QCoreApplication::setEventDispatcher(new EventDispatcherLibEv());
#endif
    QApplication app(argc, argv);

    QString strInput = "";
    QString configPath = "";
#if 0
    //////////////////////////////////////////////////////////////////////////////
    /// connect database
    configPath = QApplication::applicationDirPath() + "/setting.ini";
    QString databaseName = ConfigUtils::getCfgStr(configPath, "Database", "DatabaseName", "runtimedb");
    strInput = ConfigUtils::getCfgStr(configPath, "Database", "UserName", "");
    QString mysqlUserName = EDncrypt::Dncrypt(strInput, AES, KEY_CODE);
    strInput = ConfigUtils::getCfgStr(configPath, "Database", "PassWord", "");
    QString mysqlPassword = EDncrypt::Dncrypt(strInput, AES, KEY_CODE);
    QString hostname = ConfigUtils::getCfgStr(configPath, "Database", "HostName", "127.0.0.1");
    int mysqlPort = ConfigUtils::getCfgInt(configPath, "Database", "Port", 3306);

    //////////////////////////////////////////////////////////////////////////////
#endif
    LogInfo(getSystemInfo());
    LogInfo("start SCADARunTime!");


    //////////////////////////////////////////////////////////////////////////////
    /// start http server
    HttpServer httpServer;
    httpServer.init(60000);


    //////////////////////////////////////////////////////////////////////////////
    /// start ftp server
    strInput = ConfigUtils::getCfgStr(configPath, "FtpServer", "UserName", "admin");
    const QString &userName = EDncrypt::Dncrypt(strInput, AES, KEY_CODE);
    strInput = ConfigUtils::getCfgStr(configPath, "FtpServer", "PassWord", "admin");
    const QString &password = EDncrypt::Dncrypt(strInput, AES, KEY_CODE);
    QStorageInfo storageRoot = QStorageInfo::root();
    const QString &rootPath = "/"; //storageRoot.rootPath();
    quint32 port = ConfigUtils::getCfgInt(configPath, "FtpServer", "Port", 60001);

    FtpServer ftpServer(&app, rootPath, port, userName, password, false, false);
    if (ftpServer.isListening()) {
        QString ftpServerInfo = QString("\nFtpServer Listening at %1:%2").arg(FtpServer::lanIp()).arg(port);
        ftpServerInfo += QString("\nFtpServer User: %1").arg(userName);
        ftpServerInfo += QString("\nFtpServer Password: %1").arg(password);
        LogInfo(ftpServerInfo);
    } else {
        LogInfo("Failed to start FtpServer.");
    }


    //////////////////////////////////////////////////////////////////////////////

    QString projPath = QCoreApplication::applicationDirPath() + "/RunProject";
    if(argc == 2) {
        projPath = argv[1];
    }

    QDir dir(projPath);
    if (!dir.exists())
    {
        dir.mkpath(projPath);
    }

    HmiRunTime runTime(projPath);
    runTime.Load(DATA_SAVE_FORMAT);
    // 添加脚本函数至脚本引擎
    HmiRunTime::scriptEngine_ = new QScriptEngine();
    addFuncToScriptEngine(HmiRunTime::scriptEngine_);
    runTime.Start();
    g_pHmiRunTime = &runTime;

    QApplication::processEvents();

    QString projSavePath = QCoreApplication::applicationDirPath() + "/Project";
    QDir projSaveDir(projSavePath);
    if(!projSaveDir.exists()) {
        projSaveDir.mkpath(projSavePath);
    }


    TcpServer ser(&runTime);
    ser.listen(QHostAddress::Any, 6000);

    ///////////////////////////////////////////////////////////////////////////
    /// 启动SOAP服务
#ifdef USE_SOAP_SERVICE
    SOAPServer gSOAPServer("0.0.0.0", 60002);
#endif


    ///////////////////////////////////////////////////////////////////////////
    /// 启动定时任务
    TimerTask *pTimerTask = new TimerTask();

    // 启动共享内存消息服务
    MemoryMessageService service;
    service.start();

    ret = app.exec();

#ifdef USE_SOAP_SERVICE
    gSOAPServer.exitService();
#endif

    delete pTimerTask;

    return ret;
}
