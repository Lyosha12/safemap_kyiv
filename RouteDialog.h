#ifndef ROUTEDIALOG_H
#define ROUTEDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include "RoutePlanner.h"

class RouteDialog : public QDialog {
    Q_OBJECT

public:
    explicit RouteDialog(RoutePlanner* planner, QWidget *parent = nullptr);

private slots:
    void onCalculateClicked();
    void onClearClicked();

private:
    RoutePlanner* planner;

    QLineEdit* editLat;
    QLineEdit* editLon;
    QPushButton* btnCalculate;
    QPushButton* btnClear;
    QTextEdit* textResult;

    void setupUI();
};

#endif
