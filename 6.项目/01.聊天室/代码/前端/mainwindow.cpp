#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QDataStream>

namespace {

constexpr quint32 kMagic = 0x48574600; // "HWF"协议头

enum MsgType : quint8 {
    MsgText            = 1,
    MsgFileUpload      = 2,
    MsgFileAnnounce    = 3,
    MsgFileDownloadReq = 4,
    MsgFileDownloadData= 5
};

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_socket(new QTcpSocket(this))
{
    ui->setupUi(this);

    // 默认测试参数
    ui->serverAddr->setText("127.0.0.1");
    ui->serverPort->setText("8080");
    ui->serverName->setText("User1");

    // 按钮 & 输入 事件
    connect(ui->swith, &QPushButton::clicked, this, &MainWindow::onConnectButtonClicked);
    connect(ui->btnSend, &QPushButton::clicked, this, &MainWindow::onSendButtonClicked);
    connect(ui->btnSendFile, &QPushButton::clicked, this, &MainWindow::onSendFileButtonClicked);
    connect(ui->lineEditMessage, &QLineEdit::returnPressed, this, &MainWindow::onSendButtonClicked);

    // socket 信号
    connect(m_socket, &QTcpSocket::connected, this, &MainWindow::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &MainWindow::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &MainWindow::onSocketReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &MainWindow::onSocketErrorOccurred);

    // 文件列表双击下载
    connect(ui->listWidgetFiles, &QListWidget::itemDoubleClicked, this, &MainWindow::onFileItemDoubleClicked);
}

MainWindow::~MainWindow()
{
    delete ui;
}

// 工具函数：系统消息
void MainWindow::appendSystemMessage(const QString &msg)
{
    ui->textBrowserChat->append(QString("[System] %1").arg(msg));
}

// 工具函数：普通聊天消息
void MainWindow::appendChatMessage(const QString &msg)
{
    ui->textBrowserChat->append(msg);
}

void MainWindow::onConnectButtonClicked()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        appendSystemMessage("Disconnecting from server...");
        m_socket->disconnectFromHost();
        return;
    }

    const QString host    = ui->serverAddr->text().trimmed();
    const QString portStr = ui->serverPort->text().trimmed();

    bool ok = false;
    quint16 port = portStr.toUShort(&ok);

    if (host.isEmpty() || !ok || port == 0) {
        QMessageBox::warning(this, tr("错误"),
                             tr("请输入正确的服务器地址和端口。"));
        return;
    }

    appendSystemMessage(
        QString("Connecting to %1:%2 ...").arg(host).arg(port));

    ui->swith->setEnabled(false);
    m_socket->connectToHost(host, port);
}

void MainWindow::onSocketConnected()
{
    ui->swith->setEnabled(true);
    ui->swith->setText(tr("断开"));
    appendSystemMessage("Connected to server.");

    // 连接成功后，按协议发送一个“加入聊天室”的提示
    const QString name = ui->serverName->text().trimmed();
    if (!name.isEmpty()) {
        QString hello = QString("%1 joined the chat.").arg(name);

        QByteArray payload = hello.toUtf8();

        QByteArray packet;
        packet.reserve(4 + 1 + 4 + payload.size());

        QDataStream ds(&packet, QIODevice::WriteOnly);
        ds.setByteOrder(QDataStream::BigEndian);

        quint32 magic  = kMagic;
        quint8  type   = MsgText;
        quint32 length = payload.size();

        ds << magic;
        ds << type;
        ds << length;

        packet.append(payload);
        m_socket->write(packet);
    }
}

void MainWindow::onSocketDisconnected()
{
    ui->swith->setEnabled(true);
    ui->swith->setText(tr("连接/断开"));
    appendSystemMessage("Disconnected from server.");

    m_recvBuffer.clear();
    m_remoteFiles.clear();
    ui->listWidgetFiles->clear();
}

void MainWindow::onSocketErrorOccurred(QAbstractSocket::SocketError)
{
    ui->swith->setEnabled(true);
    ui->swith->setText(tr("连接/断开"));
    appendSystemMessage("Socket error: " + m_socket->errorString());
}

void MainWindow::onSendButtonClicked()
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        QMessageBox::warning(this, tr("错误"),
                             tr("尚未连接到服务器。"));
        return;
    }

    QString text = ui->lineEditMessage->text();
    if (text.trimmed().isEmpty())
        return;

    const QString name = ui->serverName->text().trimmed();
    QString fullText = name.isEmpty()
            ? text
            : QString("%1: %2").arg(name, text);

    // 本地显示
    appendChatMessage(fullText);

    // 构造协议帧
    QByteArray payload = fullText.toUtf8();
    QByteArray packet;
    packet.reserve(4 + 1 + 4 + payload.size());

    QDataStream ds(&packet, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);

    quint32 magic  = kMagic;
    quint8  type   = MsgText;
    quint32 length = payload.size();

    ds << magic;
    ds << type;
    ds << length;

    packet.append(payload);

    m_socket->write(packet);

    ui->lineEditMessage->clear();
}

void MainWindow::onSendFileButtonClicked()
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        QMessageBox::warning(this, tr("错误"),
                             tr("尚未连接到服务器。"));
        return;
    }

    const QString filePath =
            QFileDialog::getOpenFileName(this, tr("选择要发送的文件"));
    if (filePath.isEmpty())
        return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("错误"),
                             tr("无法打开文件：%1").arg(file.errorString()));
        return;
    }

    QByteArray fileData = file.readAll();
    file.close();

    QString fileName      = QFileInfo(filePath).fileName();
    QByteArray nameBytes  = fileName.toUtf8();
    quint32 nameLen       = nameBytes.size();
    quint64 fileSize      = fileData.size();

    appendSystemMessage(
        tr("正在发送文件：%1，大小：%2 字节")
            .arg(fileName)
            .arg(fileSize));

    // payload: [u32 nameLen][name][u64 fileSize][fileData]
    QByteArray payload;
    {
        QDataStream ds(&payload, QIODevice::WriteOnly);
        ds.setByteOrder(QDataStream::BigEndian);

        // 1. 写入 nameLen
        ds << nameLen;

        // 2. 写入文件名原始字节
        ds.writeRawData(nameBytes.constData(), nameBytes.size());

        // 3. 写入 fileSize
        ds << fileSize;

        // 4. 写入文件内容
        ds.writeRawData(fileData.constData(), fileData.size());
    }

    QByteArray packet;
    packet.reserve(4 + 1 + 4 + payload.size());
    {
        QDataStream ds(&packet, QIODevice::WriteOnly);
        ds.setByteOrder(QDataStream::BigEndian);

        quint32 magic  = kMagic;
        quint8  type   = MsgFileUpload;
        quint32 length = payload.size();

        ds << magic;
        ds << type;
        ds << length;
    }
    packet.append(payload);

    m_socket->write(packet);
}

void MainWindow::onSocketReadyRead()
{
    QByteArray data = m_socket->readAll();
    if (data.isEmpty())
        return;

    m_recvBuffer.append(data);

    const int headerSize = 4 + 1 + 4;

    while (true) {
        if (m_recvBuffer.size() < headerSize)
            return;

        // 解析头
        QDataStream ds(m_recvBuffer);
        ds.setByteOrder(QDataStream::BigEndian);

        quint32 magic;
        quint8  type;
        quint32 length;

        ds >> magic >> type >> length;

        if (magic != kMagic) {
            appendSystemMessage("协议错误：magic 不匹配，清空缓冲区。");
            m_recvBuffer.clear();
            return;
        }

        if (m_recvBuffer.size() < headerSize + int(length))
            return; // 数据未收全

        // 把 header 去掉，body 取出来
        m_recvBuffer.remove(0, headerSize);
        QByteArray body = m_recvBuffer.left(length);
        m_recvBuffer.remove(0, length);

        // 根据 type 处理
        if (type == MsgText) {

            QString text = QString::fromUtf8(body);
            appendChatMessage(text);

        } else if (type == MsgFileAnnounce) {

            // body: [u32 fileId][u32 nameLen][name][u64 fileSize]
            QDataStream bs(body);
            bs.setByteOrder(QDataStream::BigEndian);

            quint32 fileId;
            quint32 nameLen;
            bs >> fileId;
            bs >> nameLen;

            QByteArray nameBytes(nameLen, 0);
            if (bs.readRawData(nameBytes.data(), nameLen) != int(nameLen)) {
                appendSystemMessage("解析文件公告失败：文件名长度不匹配。");
                continue;
            }

            QString fileName = QString::fromUtf8(nameBytes);
            quint64 fileSize;
            bs >> fileSize;

            RemoteFile rf;
            rf.id   = fileId;
            rf.name = fileName;
            rf.size = fileSize;
            m_remoteFiles.push_back(rf);

            QString itemText =
                    QString("%1 (id=%2, %3 bytes)")
                    .arg(fileName)
                    .arg(fileId)
                    .arg(fileSize);

            ui->listWidgetFiles->addItem(itemText);
            appendSystemMessage(
                QString("收到文件公告：%1，大小 %2 字节。双击右侧列表可下载。")
                    .arg(fileName)
                    .arg(fileSize));

        } else if (type == MsgFileDownloadData) {

            // body: [u32 fileId][u32 nameLen][name][u64 fileSize][fileData]
            QDataStream bs(body);
            bs.setByteOrder(QDataStream::BigEndian);

            quint32 fileId;
            quint32 nameLen;
            quint64 fileSize;

            bs >> fileId;
            bs >> nameLen;

            QByteArray nameBytes(nameLen, 0);
            if (bs.readRawData(nameBytes.data(), nameLen) != int(nameLen)) {
                appendSystemMessage("解析文件数据失败：文件名长度不匹配。");
                continue;
            }
            QString fileName = QString::fromUtf8(nameBytes);

            bs >> fileSize;

            QByteArray fileData(fileSize, 0);
            if (bs.readRawData(fileData.data(), fileSize) != qint64(fileSize)) {
                appendSystemMessage("解析文件数据失败：文件内容长度不匹配。");
                continue;
            }

            QString savePath = QFileDialog::getSaveFileName(
                        this, tr("保存文件"), fileName);
            if (savePath.isEmpty()) {
                appendSystemMessage("已取消保存文件。");
                continue;
            }

            QFile out(savePath);
            if (!out.open(QIODevice::WriteOnly)) {
                QMessageBox::warning(this, tr("错误"),
                                     tr("保存失败：%1").arg(out.errorString()));
                continue;
            }
            out.write(fileData);
            out.close();

            appendSystemMessage(
                QString("文件已保存到：%1").arg(savePath));

        } else {
            appendSystemMessage(
                QString("收到未知类型消息 type=%1，length=%2")
                    .arg(type).arg(length));
        }
    }
}

void MainWindow::onFileItemDoubleClicked(QListWidgetItem *item)
{
    if (!item)
        return;

    int row = ui->listWidgetFiles->row(item);
    if (row < 0 || row >= m_remoteFiles.size())
        return;

    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        QMessageBox::warning(this, tr("错误"),
                             tr("尚未连接到服务器。"));
        return;
    }

    const RemoteFile &rf = m_remoteFiles[row];

    appendSystemMessage(
        QString("请求下载文件：%1 (id=%2)")
            .arg(rf.name)
            .arg(rf.id));

    // payload: [u32 fileId]
    QByteArray payload;
    {
        QDataStream ds(&payload, QIODevice::WriteOnly);
        ds.setByteOrder(QDataStream::BigEndian);
        ds << rf.id;
    }

    QByteArray packet;
    packet.reserve(4 + 1 + 4 + payload.size());
    {
        QDataStream ds(&packet, QIODevice::WriteOnly);
        ds.setByteOrder(QDataStream::BigEndian);

        quint32 magic  = kMagic;
        quint8  type   = MsgFileDownloadReq;
        quint32 length = payload.size();

        ds << magic;
        ds << type;
        ds << length;
    }
    packet.append(payload);

    m_socket->write(packet);
}
