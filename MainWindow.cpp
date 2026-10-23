#include "MainWindow.h"
#include "ReportDialog.h"
#include "DetailsDialog.h"
#include "RouteDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFile>
#include <QTextStream>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>

MainWindow::MainWindow(RoutePlanner* planner, QWidget *parent)
    : QMainWindow(parent), planner(planner) {
    setupUI();
    updateTable();
}

void MainWindow::setupUI() {
    setWindowTitle("SafeMap Kyiv - Панель диспетчера безпеки");
    resize(850, 500);

    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

    QHBoxLayout* topLayout = new QHBoxLayout();
    topLayout->addWidget(new QLabel("Оперативний стан у місті:"));

    radioNone = new QRadioButton("Відбій тривоги");
    radioYellow = new QRadioButton("Жовтий рівень (Підвищена)");
    radioRed = new QRadioButton("Червоний рівень (Загроза)");
    radioNone->setChecked(true);

    topLayout->addWidget(radioNone);
    topLayout->addWidget(radioYellow);
    topLayout->addWidget(radioRed);
    topLayout->addStretch();

    connect(radioNone, &QRadioButton::clicked, this, &MainWindow::onAlertNoneSelected);
    connect(radioYellow, &QRadioButton::clicked, this, &MainWindow::onAlertYellowSelected);
    connect(radioRed, &QRadioButton::clicked, this, &MainWindow::onAlertRedSelected);

    tableWidget = new QTableWidget(0, 5);
    tableWidget->setHorizontalHeaderLabels({"Назва", "Категорія", "Генератор", "Укриття (вільно)", "Доступність"});
    tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(tableWidget, &QTableWidget::cellDoubleClicked, this, &MainWindow::onTableCellDoubleClicked);

    QHBoxLayout* bottomLayout = new QHBoxLayout();
    btnAddReport = new QPushButton("Обробити звіт (Краудсорсинг)");
    btnFindShelter = new QPushButton("Навігатор укриттів");
    btnRefresh = new QPushButton("Оновити таблицю");

    btnOpenMap = new QPushButton("Відкрити мапу (Web)");
    btnOpenMap->setStyleSheet("background-color: #2e7d32; color: white; font-weight: bold;");

    bottomLayout->addWidget(btnAddReport);
    bottomLayout->addWidget(btnFindShelter);
    bottomLayout->addWidget(btnRefresh);
    bottomLayout->addWidget(btnOpenMap);

    statusLabel = new QLabel("Система функціонує в штатному режимі. Двічі клікніть по рядку для деталей.");

    connect(btnAddReport, &QPushButton::clicked, this, &MainWindow::onAddReportClicked);
    connect(btnFindShelter, &QPushButton::clicked, this, &MainWindow::onFindShelterClicked);
    connect(btnRefresh, &QPushButton::clicked, this, &MainWindow::onRefreshClicked);
    connect(btnOpenMap, &QPushButton::clicked, this, &MainWindow::onOpenMapClicked);

    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(tableWidget);
    mainLayout->addLayout(bottomLayout);
    mainLayout->addWidget(statusLabel);
}

void MainWindow::updateTable() {
    tableWidget->setRowCount(0);
    const auto& list = planner->get_establishments();
    auto currentAlert = planner->get_alert_level();

    for (size_t i = 0; i < list.size(); ++i) {
        tableWidget->insertRow(i);
        auto est = list[i];

        bool accessible = est->is_accessible(currentAlert);

        tableWidget->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(est->get_name())));
        tableWidget->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(est->get_type_name())));
        tableWidget->setItem(i, 2, new QTableWidgetItem(est->has_backup_power() ? "Так" : "Ні"));

        int freePlaces = est->get_shelter().get_capacity() - est->get_shelter().get_current_occupancy();
        tableWidget->setItem(i, 3, new QTableWidgetItem(QString::number(freePlaces > 0 ? freePlaces : 0)));
        tableWidget->setItem(i, 4, new QTableWidgetItem(accessible ? "Відчинено" : "Зачинено"));
    }
}

void MainWindow::onAlertNoneSelected() {
    planner->set_alert_level(AlertLevel::None);
    statusLabel->setText("Статус: Загрози відсутні. Усі заклади працюють за графіком.");
    updateTable();
}

void MainWindow::onAlertYellowSelected() {
    planner->set_alert_level(AlertLevel::Yellow);
    statusLabel->setText("Увага! Жовтий рівень тривоги. Перевірено автономне живлення закладів.");
    updateTable();
}

void MainWindow::onAlertRedSelected() {
    planner->set_alert_level(AlertLevel::Red);
    statusLabel->setText("Небезпека! Червоний рівень. Відчинено лише локації з сертифікованими укриттями.");
    updateTable();
}

void MainWindow::onAddReportClicked() {
    ReportDialog dlg(planner, this);
    connect(&dlg, &ReportDialog::reportSubmitted, this, &MainWindow::updateTable);
    dlg.exec();
}

void MainWindow::onFindShelterClicked() {
    RouteDialog dlg(planner, this);
    dlg.exec();
}

void MainWindow::onTableCellDoubleClicked(int row, int column) {
    Q_UNUSED(column);
    const auto& list = planner->get_establishments();
    if (row >= 0 && row < static_cast<int>(list.size())) {
        DetailsDialog dlg(list[row].get(), this);
        dlg.exec();
    }
}

void MainWindow::onRefreshClicked() {
    updateTable();
    statusLabel->setText("Дані мапи оновлено вручну.");
}

void MainWindow::onOpenMapClicked() {
    QString filePath = QDir::currentPath() + "/safemap.html";
    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Помилка", "Не вдалося створити файл мапи!");
        return;
    }

    QTextStream out(&file);

    out << "<!DOCTYPE html>\n<html>\n<head>\n"
        << "<meta charset=\"utf-8\">\n"
        << "<title>SafeMap Kyiv - Interactive Map</title>\n"
        << "<link rel=\"stylesheet\" href=\"https://unpkg.com/leaflet@1.9.4/dist/leaflet.css\" />\n"
        << "<script src=\"https://unpkg.com/leaflet@1.9.4/dist/leaflet.js\"></script>\n"
        << "<style>body { margin: 0; } #map { height: 100vh; width: 100vw; }</style>\n"
        << "</head>\n<body>\n"
        << "<div id=\"map\"></div>\n"
        << "<script>\n"
        << "var map = L.map('map').setView([50.4501, 30.5234], 13);\n"
        << "L.tileLayer('https://server.arcgisonline.com/ArcGIS/rest/services/World_Street_Map/MapServer/tile/{z}/{y}/{x}', {\n"
        << "    attribution: 'Tiles &copy; Esri &mdash; SafeMap Kyiv',\n"
        << "    maxZoom: 18\n"
        << "}).addTo(map);\n";

    const auto& list = planner->get_establishments();
    for (const auto& est : list) {
        auto coords = est->get_coordinates();
        bool isAccessible = est->is_accessible(planner->get_alert_level());

        QString statusColor = isAccessible ? "green" : "red";
        QString statusText = isAccessible ? "Відчинено" : "Зачинено (Тривога)";

        out << "L.marker([" << coords.get_latitude() << ", " << coords.get_longitude() << "])"
            << ".addTo(map)"
            << ".bindPopup(\"<b>" << QString::fromStdString(est->get_name()) << "</b><br>"
            << "Статус: <span style='color:" << statusColor << ";'>" << statusText << "</span><br>"
            << "Місць в укритті: " << (est->get_shelter().get_capacity() - est->get_shelter().get_current_occupancy())
            << "\");\n";
    }

    out << "</script>\n</body>\n</html>\n";
    file.close();

    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
    statusLabel->setText("Мапа успішно згенерована та відкрита у браузері.");
}