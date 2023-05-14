#include "chatgptmanager.h"
#include "runtimeinfo.h"
#include <QMessageBox>

ChatGPTManager::ChatGPTManager(QObject *parent) : QObject(parent)
{
}

void ChatGPTManager::setLiveService(LiveRoomService *service)
{
    this->liveService = service;
}

void ChatGPTManager::chat(qint64 uid, QString text, NetStringFunc func)
{
    /// 初始化ChatGPT
    ChatGPTUtil* chatgpt = new ChatGPTUtil(this);
    chatgpt->setStream(false);
    QString label = "ChatGPT弹幕版";

    connect(chatgpt, &ChatGPTUtil::signalResponseError, this, [=](const QByteArray& ba) {
        QJsonParseError error;
        QJsonDocument document = QJsonDocument::fromJson(ba, &error);
        if (error.error == QJsonParseError::NoError)
        {
            QJsonObject json = document.object();
            if (json.contains("error") && json.value("error").isObject())
                json = json.value("error").toObject();
            if (json.contains("message"))
            {
                int code = json.value("code").toInt();
                QString type = json.value("type").toString();
                QMessageBox::critical(rt->mainwindow, label, json.value("message").toString() + "\n\n错误码：" + snum(code) + "  " + type);
            }
            else
            {
                QMessageBox::critical(rt->mainwindow, label, QString(ba));
            }
        }
        else
        {
            QMessageBox::critical(rt->mainwindow, label, QString(ba));
        }
    });

    connect(chatgpt, &ChatGPTUtil::signalRequestStarted, this, [=]{

    });
    connect(chatgpt, &ChatGPTUtil::signalResponseFinished, this, [=]{

    });
    connect(chatgpt, &ChatGPTUtil::signalResponseText, this, [=](const QString& text) {
        chatgpt->stopAndDelete();
        func(text);
        if (!usersChats.contains(uid))
            usersChats.insert(uid, QList<ChatBean>());
        usersChats[uid].append(ChatBean("assistant", text));
    });
    connect(chatgpt, &ChatGPTUtil::signalStreamText, this, [=](const QString& text) {

    });

    /// 获取上下文
    QList<ChatBean>& userChats = usersChats[uid];
    userChats.append(ChatBean("user", text));

    QList<ChatBean> chats;
    if (!us->chatgpt_prompt.isEmpty())
    {
        QString rep = us->chatgpt_prompt;
        rep.replace("%danmu_longest%", snum(ac->danmuLongest));
        chats.append(ChatBean("system", rep));
    }

    int count = us->chatgpt_max_context_count;
    for (int i = userChats.size() - 1; i >= 0; i--)
    {
        const ChatBean& chat = userChats.at(i);
        if (chat.role.contains("error")) // 错误
            continue;
        if (us->chatgpt_history_input && chat.role != "user") // 仅包含输入
            continue;
        if (--count <= 0)
            break;
        // 判断token是否超出了

        // 添加记忆
        chats.append(userChats.at(i));
    }
    chatgpt->getResponse(chats);
}

void ChatGPTManager::clear()
{
    usersChats.clear();
}

void ChatGPTManager::localNotify(const QString &text)
{

}
