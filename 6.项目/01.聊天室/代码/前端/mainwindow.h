#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QVector>
#include <QListWidgetItem>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onConnectButtonClicked();
    void onSendButtonClicked();
    void onSendFileButtonClicked();

    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketReadyRead();
    void onSocketErrorOccurred(QAbstractSocket::SocketError);

    // 双击文件列表下载
    void onFileItemDoubleClicked(QListWidgetItem *item);

private:
    Ui::MainWindow *ui;
    QTcpSocket *m_socket;

    QByteArray m_recvBuffer;

    struct RemoteFile {
        quint32 id;
        QString name;
        quint64 size;
    };
    QVector<RemoteFile> m_remoteFiles;

    void appendSystemMessage(const QString &msg);
    void appendChatMessage(const QString &msg);
};

#endif // MAINWINDOW_H
