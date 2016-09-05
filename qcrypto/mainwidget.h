#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QWidget>
#include "jinbuffer.h"
#include "jinpublickey.h"
#include <QDir>
#include "secretrequestdialog.h"

namespace Ui {
class MainWidget;
}

class MainWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MainWidget(QWidget *parent = 0);
    ~MainWidget();
protected:
    void uiDataLoad(QDir &dir);
    //通过desc返回文件路径.
    const char* SaveKeyToFile(JinPublicKey *pk, QByteArray &secret, QString &desc);
    bool SaveKey(JinBuffer keyexp, QByteArray &secret, QString &desc, const QString path);

    void LoadCertBySigner(QString signer);
private slots:
    void on_pbcreate2_clicked();
    void on_pbshow2_clicked();
    void on_pbsave2_clicked();
    void on_pbverify2_clicked();
    void on_cbcert2_currentTextChanged(const QString &arg1);
    void on_cbname2_currentTextChanged(const QString &arg1);
    void on_pbsign3_clicked();

private:
    Ui::MainWidget *ui;
    bool uiDataLoad_;
    bool certDataLoad_;
    QByteArray keyDir_;
    QString masterKeyName_;
    //JinPublicKey masterKey_;
    JinPublicKey* currentKey_;
    SecretRequestDialog secretInputDialog_;
};

#endif // MAINWIDGET_H
