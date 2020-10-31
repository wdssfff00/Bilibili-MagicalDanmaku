#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 刷新间隔
    danmakuTimer = new QTimer(this);
    int interval = settings.value("danmaku/interval", 500).toInt();
    ui->refreshDanmakuIntervalSpin->setValue(interval);
    danmakuTimer->setInterval(interval);
    connect(danmakuTimer, SIGNAL(timeout()), this, SLOT(pullLiveDanmaku()));
    ui->refreshDanmakuCheck->setChecked(true);

    // 点歌自动复制
    diangeAutoCopy = settings.value("danmaku/diangeAutoCopy", true).toBool();
    ui->DiangeAutoCopyCheck->setChecked(diangeAutoCopy);
    connect(this, &MainWindow::signalNewDanmaku, this, [=](LiveDanmaku danmaku){
       if (!diangeAutoCopy)
           return ;
       QString text = danmaku.getText();
       if (!text.startsWith("点歌"))
           return ;
       text = text.replace(0, 2, "");
       if (QString(" :：，。").contains(text.left(1)))
           text.replace(0, 1, "");
       text = text.trimmed();
       QClipboard* clip = QApplication::clipboard();
       clip->setText(text);
       qDebug() << "【点歌自动复制】" << text;
       ui->DiangeAutoCopyCheck->setText("点歌自动复制（" + text + "）");
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (danmakuWindow)
        danmakuWindow->deleteLater();
}

void MainWindow::pullLiveDanmaku()
{
    QString roomid = ui->roomIdEdit->text();
    if (roomid.isEmpty())
        return ;
    QString url = "https://api.live.bilibili.com/ajax/msg";
    QStringList param{"roomid", roomid};
    connect(new NetUtil(url, param), &NetUtil::finished, this, [=](QString result){
        QJsonParseError error;
        QJsonDocument document = QJsonDocument::fromJson(result.toUtf8(), &error);
        if (error.error != QJsonParseError::NoError)
        {
            qDebug() << error.errorString();
            return ;
        }
        QJsonObject json = document.object();
        QJsonArray danmakus = json.value("data").toObject().value("room").toArray();
        QList<LiveDanmaku> lds;
        for (int i = 0; i < danmakus.size(); i++)
            lds.append(LiveDanmaku::fromJson(danmakus.at(i).toObject()));
        appendNewLiveDanmaku(lds);
    });
}


void MainWindow::on_refreshDanmakuIntervalSpin_valueChanged(int arg1)
{
    settings.setValue("danmaku/interval", arg1);
    danmakuTimer->setInterval(arg1);
}

void MainWindow::on_refreshDanmakuCheck_stateChanged(int arg1)
{
    if (arg1)
        danmakuTimer->start();
    else
        danmakuTimer->stop();
}

void MainWindow::appendNewLiveDanmaku(QList<LiveDanmaku> danmakus)
{
    // 去掉已经存在的弹幕
    QDateTime prevLastTime = roomDanmakus.size()
            ? roomDanmakus.last().getTimeline()
            : QDateTime::fromMSecsSinceEpoch(0);
    while (danmakus.size() && danmakus.first().getTimeline() < prevLastTime)
        danmakus.removeFirst();
    while (danmakus.size() && danmakus.first().isIn(roomDanmakus))
        danmakus.removeFirst();
    if (!danmakus.size())
        return ;

    // 不是第一次加载
    if (roomDanmakus.size())
    {
        // 发送信号给其他插件
        for (int i = 0; i < danmakus.size(); i++)
        {
            newLiveDanmakuAdded(danmakus.at(i));
        }
    }

    // 添加到队列
    roomDanmakus.append(danmakus);

    /*QStringList texts;
    for (int i = 0; i < roomDanmakus.size(); i++)
        texts.append(roomDanmakus.at(i).toString());
    qDebug() << "当前弹幕" << texts;*/
}

void MainWindow::newLiveDanmakuAdded(LiveDanmaku danmaku)
{
    qDebug() << "+++++新弹幕：" <<danmaku.toString();
    emit signalNewDanmaku(danmaku);
}

/**
 * 显示实时弹幕
 */
void MainWindow::on_showLiveDanmakuButton_clicked()
{
    bool hidding = (danmakuWindow == nullptr || danmakuWindow->isHidden());
    if (danmakuWindow == nullptr)
    {
        danmakuWindow = new LiveDanmakuWindow(this);
        connect(this, SIGNAL(signalNewDanmaku(LiveDanmaku)), danmakuWindow, SLOT(slotNewLiveDanmaku(LiveDanmaku)));
    }

    if (hidding)
    {
        danmakuWindow->show();
        settings.setValue("danmaku/liveWindow", true);
    }
    else
    {
        danmakuWindow->hide();
        settings.setValue("danmaku/liveWindow", false);
    }
}

void MainWindow::on_DiangeAutoCopyCheck_stateChanged(int)
{
    settings.setValue("danmaku/diangeAutoCopy", diangeAutoCopy = ui->DiangeAutoCopyCheck->isChecked());
}

void MainWindow::on_testDanmakuButton_clicked()
{
    QString text = ui->testDanmakuEdit->text();
    if (text.isEmpty())
        text = "测试弹幕";
    newLiveDanmakuAdded(
                LiveDanmaku("测试用户", text,
                            qrand() % 89999999 + 10000000,
                            QDateTime::currentDateTime()));

}
