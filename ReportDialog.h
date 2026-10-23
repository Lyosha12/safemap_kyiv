#ifndef REPORTDIALOG_H
#define REPORTDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include "RoutePlanner.h"

class ReportDialog : public QDialog {
    Q_OBJECT

public:
    explicit ReportDialog(RoutePlanner* planner, QWidget *parent = nullptr);

signals:
    void reportSubmitted();

private slots:
    void onSubmitClicked();
    void onTypeChanged(int index);

private:
    RoutePlanner* planner;

    QComboBox* comboEstablishment;
    QComboBox* comboReportType;
    QCheckBox* checkGeneratorActive;
    QCheckBox* checkBlocked;
    QSpinBox* spinShelterCount;
    QLineEdit* editUserName;
    QPushButton* btnSubmit;
    QPushButton* btnCancel;
    QLabel* lblStatus;

    void setupUI();
};

#endif
