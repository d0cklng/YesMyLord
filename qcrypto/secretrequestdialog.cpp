#include "secretrequestdialog.h"
#include "ui_secretrequestdialog.h"

SecretRequestDialog::SecretRequestDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SecretRequestDialog)
{
    ui->setupUi(this);
}

SecretRequestDialog::~SecretRequestDialog()
{
    delete ui;
}

QString SecretRequestDialog::showAsModel()
{
    ui->leinput->clear();
    int rt = this->exec();
    if(rt>0)
        return ui->leinput->text();
    else
        return QString("");
}

QString SecretRequestDialog::getSecretInput()
{
    return ui->leinput->text();
}
