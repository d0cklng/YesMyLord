#include "mainwidget.h"
#include "ui_mainwidget.h"
#include "jinassert.h"
#include <QMessageBox>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include "jindatatime.h"
#include "jinendian.h"

/* 使用说明书.
 * create 创建,需要填KeyWord描述 Secret密码
 * save 保存,相当于修改当前PriKey的KeyWord和Secret
 * show/hide 显示密码
 * certificate 显示当前PriKey签的证书
 * verify 用当前PriKey验证对应证书是否通过
 * description 证书描述
 * time 证书有效时间
 * pubkey 目录下的公钥
 * sign 用当前PriKey签当前PubKey,产生刷新.
 * ??Time如果填了数字, 对sign有效.第一个代表生效时间,第二个代表持续时间
 *
 * */

MainWidget::MainWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MainWidget)
  , uiDataLoad_(false)
  , certDataLoad_(false)
  , currentKey_(NULL)
{
    ui->setupUi(this);
    QDir appdir = QApplication::applicationDirPath();
    appdir.mkdir("keys");
    appdir.cd("keys");
    keyDir_ = appdir.absolutePath().toUtf8();
    uiDataLoad(appdir);

}

MainWidget::~MainWidget()
{
    delete ui;
    if(currentKey_) delete currentKey_;
}

void MainWidget::uiDataLoad(QDir &dir)
{
    QStringList priNameFilter;
    priNameFilter << "*.pri";
    QFileInfoList priInfoList = dir.entryInfoList(priNameFilter,QDir::Files|QDir::NoDotAndDotDot);
    for(int i=0;i<priInfoList.size();i++)
    {
        ui->cbname2->insertItem(0,priInfoList[i].baseName());
    }
    ui->cbname2->setCurrentIndex(-1);
    masterKeyName_ = "";

    QStringList pubNameFilter;
    pubNameFilter << "*.pub";
    QFileInfoList pubInfoList = dir.entryInfoList(pubNameFilter,QDir::Files|QDir::NoDotAndDotDot);
    for(int i=0;i<pubInfoList.size();i++)
    {
        ui->cbname3->insertItem(0,pubInfoList[i].baseName());
    }

    uiDataLoad_ = true;
}

const char* MainWidget::SaveKeyToFile(JinPublicKey *pk, QByteArray &secret, QString &desc)
{
    JAssert(secret.length() > 0);
    const char* keyFileName = pk->saveToFile(keyDir_.constData(),JinBuffer(secret.constData()),desc.toUtf8().constData());
    return keyFileName;
}

void MainWidget::on_pbcreate2_clicked()
{
    QString desc = ui->ledesc2->text();
    QString pass = ui->lepass2->text();
    if(desc.length()==0){
        QMessageBox::critical(this,"Error!","Must enter desc!");
        return;
    }
    if(pass.length()<8){
        QMessageBox::critical(this,"Error!","Secret length must more than 8!");
        return;
    }

    if(currentKey_) delete currentKey_;
    currentKey_ = new JinPublicKey;
    bool ret = currentKey_->generate(256);
    if(!ret){
        QMessageBox::critical(this,"Error!","Key generate failed!");
        return;
    }

    const char* keyFileName = SaveKeyToFile(currentKey_,pass.toUtf8(),desc);
    if(keyFileName==NULL){
        QMessageBox::critical(this,"Error!","Save key file failed!");
        return;
    }

    ui->cbname3->insertItem(0,(char*)keyFileName);
    ui->cbname2->insertItem(0,(char*)keyFileName);
    ui->cbname2->setCurrentIndex(0);
}


void MainWidget::on_pbshow2_clicked()
{
    if(ui->pbshow2->text()=="Show")
    {
        ui->lepass2->setEchoMode(QLineEdit::Normal);
        ui->pbshow2->setText("Hide");
    }
    else
    {
        ui->lepass2->setEchoMode(QLineEdit::PasswordEchoOnEdit);
        ui->pbshow2->setText("Show");
    }
}

void MainWidget::on_pbsave2_clicked()
{
    if(currentKey_ == NULL){
        QMessageBox::critical(this,"Error!","Current key not valid!");
        return;
    }
    if(!currentKey_->isPrivateKey()){
        QMessageBox::critical(this,"Error!","Key must a private key!");
        return;
    }
    QString desc = ui->ledesc2->text();
    QString pass = ui->lepass2->text();
    if(desc.length()==0){
        QMessageBox::critical(this,"Error!","Must enter desc!");
        return;
    }
    if(pass.length()<8){
        QMessageBox::critical(this,"Error!","Secret length must more than 8!");
        return;
    }

    const char* keyFileName = SaveKeyToFile(currentKey_,pass.toUtf8(),desc);
    if(keyFileName==NULL){
        QMessageBox::critical(this,"Error!","Save key file failed!");
        return;
    }
}

void MainWidget::LoadCertBySigner(QString signer)
{
    certDataLoad_ = false;
    ui->cbcert2->clear();
    ui->cbcert2->setCurrentIndex(-1);
    QDir dir( QString::fromUtf8(keyDir_) );
    QStringList certNameFilter;
    certNameFilter << signer+"_*.cert";
    QFileInfoList certFileList = dir.entryInfoList(certNameFilter,QDir::Files|QDir::NoDotAndDotDot);
    for(int i=0;i<certFileList.size();i++)
    {
        QString bn = certFileList[i].baseName();
        int idx = bn.indexOf('_');
        if(idx == 20)
        {
            bn = bn.mid(idx+1,20);
        }
        ui->cbcert2->insertItem(0,bn);
    }
    ui->cbcert2->setCurrentIndex(-1);
    certDataLoad_ = true;
}

void MainWidget::on_pbverify2_clicked()
{
    QString masterKey = ui->cbname2->currentText();
    QString slaveCert = ui->cbcert2->currentText();
    if(masterKey.length() == 0 || currentKey_ == 0){
        QMessageBox::critical(this,"Error!","Private key not set!");
        return;
    }
    if(slaveCert.length() == 0){
        QMessageBox::critical(this,"Error!","Cert not set!");
        return;
    }

    QString qCertPath = QString::fromUtf8(keyDir_).append(QDir::separator());
    qCertPath = qCertPath + masterKey + "_" + slaveCert + ".cert";
    QFile cf(qCertPath);
    if(!cf.open(QFile::ReadOnly))
    {
        QMessageBox::critical(this,"Error!","Open file for cert failed!");
        return;
    }

    QByteArray certf = cf.readAll();
    if(certf.length() == 0 || certf.length()<sizeof(CertDetailInfo))
    {
        QMessageBox::critical(this,"Error!","cert load failed!");
        return;
    }



    JinBuffer secret("jinzeyu.cn");
    JinPublicKey tobeSignKey;
    QString qfullPath = QString::fromUtf8(keyDir_).append(QDir::separator());
    qfullPath = qfullPath + slaveCert + ".pub";
    JinBuffer desc;
    if(!tobeSignKey.loadKey(secret,desc,qfullPath.toUtf8().constData()))
    {
        QMessageBox::critical(this,"Error!","Load public key failed!");
        return;
    }
    JinBuffer cert(certf.constData(),certf.length());
    bool result = currentKey_->verifyCert(tobeSignKey.exportKey(),cert);
    if(result)
    {
        QMessageBox::information(this,"OK!","Verify OK!");
        return;
    }
    else
    {
        QMessageBox::warning(this,"Warn!","Verify failed!");
        return;
    }
}

void MainWidget::on_cbcert2_currentTextChanged(const QString &arg1)
{
    if(!certDataLoad_) return;
    qDebug() << "cert:" << arg1;
    if(arg1.length() == 0)
    {
        ui->ledesc3->setText("");
        ui->letime3_1->setText("");
        ui->letime3_2->setText("");
        return;
    }
    QString masterKey = ui->cbname2->currentText();
    QString slaveCert = ui->cbcert2->currentText();
    if(masterKey.length() == 0){
        QMessageBox::critical(this,"Error!","Private key not set!");
        return;
    }
    if(slaveCert.length() == 0){
        QMessageBox::critical(this,"Error!","Cert not set!");
        return;
    }

    QString qCertPath = QString::fromUtf8(keyDir_).append(QDir::separator());
    qCertPath = qCertPath + masterKey + "_" + slaveCert + ".cert";
    QFile cf(qCertPath);
    if(!cf.open(QFile::ReadOnly))
    {
        //QMessageBox::critical(this,"Error!","Open file for cert failed!");
        return;
    }

    QByteArray cert = cf.readAll();
    if(cert.length() == 0 || cert.length()<sizeof(CertDetailInfo))
    {
        //QMessageBox::critical(this,"Error!","cert load failed!");
        return;
    }

    CertDetailInfo *info = (CertDetailInfo*)cert.constData();
    ui->ledesc3->setText(info->shortInfo);
    JinDateTime t1(fromLE(info->signTime));
    JinDateTime t2(fromLE(info->deadLine));
    ui->letime3_1->setText(t1.toString());
    ui->letime3_2->setText(t2.toString());
}

void MainWidget::on_cbname2_currentTextChanged(const QString &arg1)
{
    if(!uiDataLoad_) return;
    if(masterKeyName_==arg1)return;
    masterKeyName_ = arg1;
    //QString selectText = ui->cbname2->currentText();
    QString selectText = arg1;
    qDebug() << "master:" << selectText;
    if(selectText.length()==0) return;

    QString secretInput = secretInputDialog_.showAsModel();
    if(secretInput.length() > 0)
    {  //尝试打开,如果不行滚到下面.
        JinBuffer desc;
        if(currentKey_) delete currentKey_;
        currentKey_ = new JinPublicKey;
        JinBuffer secret(secretInput.toUtf8().constData());
        QString qfullPath = QString::fromUtf8(keyDir_).append(QDir::separator());
        qfullPath = qfullPath + selectText + ".pri";
        if(currentKey_->loadKey(secret,desc,qfullPath.toUtf8().constData()))
        {
            ui->ledesc2->setText(QString::fromUtf8(desc.cstr()));
            ui->lepass2->setText(secretInput);
            LoadCertBySigner(selectText);
            return;
        }
        else
        {
            QMessageBox::critical(this,"Error!","Load key failed!");
        }
    }

    delete currentKey_;
    currentKey_ = 0;
    ui->cbname2->setCurrentIndex(-1);
    masterKeyName_ = "";
    ui->ledesc2->setText("");
    ui->lepass2->setText("");

    ui->cbcert2->clear();
    ui->ledesc3->setText("");
    ui->letime3_1->setText("");
    ui->letime3_2->setText("");

}

void MainWidget::on_pbsign3_clicked()
{
    QString willSign = ui->cbname2->currentText();
    QString tobeSign = ui->cbname3->currentText();
    QString descSign = ui->ledesc3->text();
    if(willSign.length() == 0 || currentKey_==NULL){
        QMessageBox::critical(this,"Error!","Private key not set!");
        return;
    }
    if(tobeSign.length() == 0){
        QMessageBox::critical(this,"Error!","Public key not set!");
        return;
    }
    if(descSign.length()==0){
        QMessageBox::critical(this,"Error!","Must enter desc!");
        return;
    }

    JinBuffer secret("jinzeyu.cn");
    JinPublicKey tobeSignKey;
    QString qfullPath = QString::fromUtf8(keyDir_).append(QDir::separator());
    qfullPath = qfullPath + tobeSign + ".pub";
    JinBuffer desc;
    if(!tobeSignKey.loadKey(secret,desc,qfullPath.toUtf8().constData()))
    {
        QMessageBox::critical(this,"Error!","Load public key failed!");
        return;
    }
    JinBuffer cert = currentKey_->signToCert(tobeSignKey.exportKey(),10,descSign.toUtf8().constData());
    qDebug()<<"cert.length=" << cert.length();
    if(cert.length() == 0)
    {
        QMessageBox::critical(this,"Error!","Sign public key failed!");
        return;
    }

    QString qCertPath = QString::fromUtf8(keyDir_).append(QDir::separator());
    qCertPath = qCertPath + willSign + "_" + tobeSign + ".cert";
    QFile cf(qCertPath);
    if(!cf.open(QFile::ReadWrite|QFile::Truncate))
    {
        QMessageBox::critical(this,"Error!","Open file for cert failed!");
        return;
    }

    qint64 w = cf.write(cert.cstr(),cert.length());
    if(w != cert.length())
    {
        QMessageBox::critical(this,"Error!","Write cert failed!");
        return;
    }
    //ui->cbcert2->insertItem(0,tobeSign);
    //ui->cbcert2->setCurrentIndex(0);
    //ui->ledesc3->setText(descSign);
    int idx = ui->cbcert2->findText(tobeSign);
    if(idx == -1)
    {
        ui->cbcert2->addItem(tobeSign);
    }
    //ui->cbcert2->clear();
    //this->LoadCertBySigner(willSign);
    ui->cbcert2->setCurrentText(tobeSign);
}
