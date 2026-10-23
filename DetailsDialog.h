#ifndef DETAILSDIALOG_H
#define DETAILSDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include "Establishments.h"

class DetailsDialog : public QDialog {
    Q_OBJECT

public:
    explicit DetailsDialog(Establishment* est, QWidget *parent = nullptr);

private slots:
    void onCloseClicked();

private:
    QLabel* lblName;
    QLabel* lblType;
    QLabel* lblCoordinates;
    QLabel* lblGenerator;
    QLabel* lblShelterStatus;
    QProgressBar* progressCapacity;
    QPushButton* btnClose;

    void setupUI(Establishment* est);
};

#endif
