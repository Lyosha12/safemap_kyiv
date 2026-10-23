#include "DetailsDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>

DetailsDialog::DetailsDialog(Establishment* est, QWidget *parent)
    : QDialog(parent) {
    setupUI(est);
}

void DetailsDialog::setupUI(Establishment* est) {
    setWindowTitle("Детальна інформація про локацію");
    resize(360, 260);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    QFormLayout* form = new QFormLayout();

    lblName = new QLabel(QString::fromStdString(est->get_name()));
    lblType = new QLabel(QString::fromStdString(est->get_type_name()));
    
    auto coords = est->get_coordinates();
    lblCoordinates = new QLabel(QString("%1, %2").arg(coords.latitude).arg(coords.longitude));
    
    lblGenerator = new QLabel(est->has_backup_power() ? "В наявності (Працює)" : "Відсутній / Не працює");

    int current = est->get_shelter().get_current_occupancy();
    int maxCap = est->get_shelter().get_capacity();
    
    lblShelterStatus = new QLabel(QString("%1 / %2 місць").arg(current).arg(maxCap));

    progressCapacity = new QProgressBar();
    progressCapacity->setRange(0, maxCap > 0 ? maxCap : 1);
    progressCapacity->setValue(current);

    form->addRow("Назва об'єкта:", lblName);
    form->addRow("Категорія:", lblType);
    form->addRow("Геолокація:", lblCoordinates);
    form->addRow("Резервне живлення:", lblGenerator);
    form->addRow("Місць в укритті:", lblShelterStatus);
    form->addRow("Заповненість:", progressCapacity);

    btnClose = new QPushButton("Закрити");
    connect(btnClose, &QPushButton::clicked, this, &DetailsDialog::onCloseClicked);

    mainLayout->addLayout(form);
    mainLayout->addWidget(btnClose);
}

void DetailsDialog::onCloseClicked() {
    accept();
}