#include "ReportDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>

ReportDialog::ReportDialog(RoutePlanner* planner, QWidget *parent)
    : QDialog(parent), planner(planner) {
    setupUI();
}

void ReportDialog::setupUI() {
    setWindowTitle("Подати звіт про стан локації");
    resize(380, 320);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    QFormLayout* formLayout = new QFormLayout();

    editUserName = new QLineEdit("Волонтер_1");
    comboEstablishment = new QComboBox();

    for (const auto& est : planner->get_establishments()) {
        comboEstablishment->addItem(QString::fromStdString(est->get_name()));
    }

    comboReportType = new QComboBox();
    comboReportType->addItem("Оновлення статусу (генератор/люди)");
    comboReportType->addItem("Повідомлення про небезпеку (блокування)");

    checkGeneratorActive = new QCheckBox("Генератор працює");
    checkGeneratorActive->setChecked(true);

    checkBlocked = new QCheckBox("Прохід / укриття заблоковано");
    checkBlocked->setChecked(false);
    checkBlocked->setEnabled(false);

    spinShelterCount = new QSpinBox();
    spinShelterCount->setRange(0, 500);
    spinShelterCount->setValue(10);

    formLayout->addRow("Ім'я користувача:", editUserName);
    formLayout->addRow("Заклад:", comboEstablishment);
    formLayout->addRow("Тип звіту:", comboReportType);
    formLayout->addRow("Живлення:", checkGeneratorActive);
    formLayout->addRow("Доступність:", checkBlocked);
    formLayout->addRow("Людей в укритті:", spinShelterCount);

    lblStatus = new QLabel("");

    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnSubmit = new QPushButton("Надіслати");
    btnCancel = new QPushButton("Скасувати");
    btnLayout->addWidget(btnSubmit);
    btnLayout->addWidget(btnCancel);

    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(lblStatus);
    mainLayout->addLayout(btnLayout);

    connect(comboReportType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ReportDialog::onTypeChanged);
    connect(btnSubmit, &QPushButton::clicked, this, &ReportDialog::onSubmitClicked);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

void ReportDialog::onTypeChanged(int index) {
    bool isHazard = (index == 1);
    checkBlocked->setEnabled(isHazard);
    checkGeneratorActive->setEnabled(!isHazard);
    spinShelterCount->setEnabled(!isHazard);
}

void ReportDialog::onSubmitClicked() {
    if (comboEstablishment->count() == 0) {
        QMessageBox::warning(this, "Помилка", "Список закладів порожній!");
        return;
    }

    int targetIdx = comboEstablishment->currentIndex();
    auto est = planner->get_establishments()[targetIdx];

    User user(editUserName->text().toStdString());

    if (comboReportType->currentIndex() == 0) {
        auto report = std::make_shared<StatusReport>(user, checkGeneratorActive->isChecked(), spinShelterCount->value());
        planner->process_user_report(report, est);
    } else {
        auto report = std::make_shared<HazardReport>(user, checkBlocked->isChecked());
        planner->process_user_report(report, est);
    }

    emit reportSubmitted();
    QMessageBox::information(this, "Успіх", "Звіт успішно враховано системою!");
    accept();
}