#include "RouteDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>

RouteDialog::RouteDialog(RoutePlanner* planner, QWidget *parent)
    : QDialog(parent), planner(planner) {
    setupUI();
}

void RouteDialog::setupUI() {
    setWindowTitle("Навігатор безпеки: Пошук найближчого укриття");
    resize(420, 350);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    QFormLayout* form = new QFormLayout();

    editLat = new QLineEdit("50.4501");
    editLon = new QLineEdit("30.5234");

    form->addRow("Ваша широта (Lat):", editLat);
    form->addRow("Ваша довгота (Lon):", editLon);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnCalculate = new QPushButton("Знайти найближче");
    btnClear = new QPushButton("Очистити");
    btnLayout->addWidget(btnCalculate);
    btnLayout->addWidget(btnClear);

    textResult = new QTextEdit();
    textResult->setReadOnly(true);

    mainLayout->addLayout(form);
    mainLayout->addLayout(btnLayout);
    mainLayout->addWidget(new QLabel("Результат розрахунку маршруту:"));
    mainLayout->addWidget(textResult);

    connect(btnCalculate, &QPushButton::clicked, this, &RouteDialog::onCalculateClicked);
    connect(btnClear, &QPushButton::clicked, this, &RouteDialog::onClearClicked);
}

void RouteDialog::onCalculateClicked() {
    bool okLat, okLon;
    double lat = editLat->text().toDouble(&okLat);
    double lon = editLon->text().toDouble(&okLon);

    if (!okLat || !okLon) {
        QMessageBox::warning(this, "Помилка вводу", "Введіть коректні числові координати!");
        return;
    }

    Coordinates userPos(lat, lon);
    auto nearest = planner->find_nearest_shelter(userPos);

    if (nearest) {
        double dist = userPos.distance_to(nearest->get_coordinates());
        textResult->setHtml(QString("<b>Знайдено безпечне укриття:</b><br>"
                                    "Заклад: %1<br>"
                                    "Тип: %2<br>"
                                    "Відстань: %3 км<br>"
                                    "Доступність: Відчинено для громадян")
                                .arg(QString::fromStdString(nearest->get_name()))
                                .arg(QString::fromStdString(nearest->get_type_name()))
                                .arg(QString::number(dist, 'f', 2)));
    } else {
        textResult->setPlainText("На жаль, поблизу немає вільних або доступних укриттів при поточному рівні тривоги.");
    }
}

void RouteDialog::onClearClicked() {
    editLat->clear();
    editLon->clear();
    textResult->clear();
}