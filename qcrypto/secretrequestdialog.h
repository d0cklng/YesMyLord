#ifndef SECRETREQUESTDIALOG_H
#define SECRETREQUESTDIALOG_H

#include <QDialog>

namespace Ui {
class SecretRequestDialog;
}

class SecretRequestDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SecretRequestDialog(QWidget *parent = 0);
    ~SecretRequestDialog();

    QString showAsModel();
    QString getSecretInput();
private:
    Ui::SecretRequestDialog *ui;
};

#endif // SECRETREQUESTDIALOG_H
