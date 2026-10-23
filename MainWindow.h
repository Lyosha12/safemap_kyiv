#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QLabel>
#include "RoutePlanner.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(RoutePlanner* planner, QWidget *parent = nullptr);
    ~MainWindow() = default;

private slots:
    void onAlertNoneSelected();
    void onAlertYellowSelected();
    void onAlertRedSelected();
    void onAddReportClicked();
    void onFindShelterClicked();
    void onTableCellDoubleClicked(int row, int column);
    void onRefreshClicked();
    void onOpenMapClicked();

private:
    RoutePlanner* planner;

    QTableWidget* tableWidget;
    QRadioButton* radioNone;
    QRadioButton* radioYellow;
    QRadioButton* radioRed;
    QPushButton* btnAddReport;
    QPushButton* btnFindShelter;
    QPushButton* btnRefresh;
    QLabel* statusLabel;
    QPushButton* btnOpenMap;

    void setupUI();
    void updateTable();
};

#endif
