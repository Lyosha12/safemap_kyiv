#include <QApplication>
#include "MainWindow.h"
#include "RoutePlanner.h"
#include "Establishments.h"
#include "Infrastructure.h"
#include <memory>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    RoutePlanner planner;

    auto silpo = std::make_shared<Supermarket>(
        1,
        Coordinates(50.4385, 30.5234),
        "Сільпо (ТРЦ Gulliver)",
        Shelter(150, true),
        true,
        true
    );
    planner.add_establishment(silpo);

    auto novaPoshta = std::make_shared<PostOffice>(
        3,
        Coordinates(50.4511, 30.5211),
        "Нова Пошта №1",
        Shelter(0, false),
        false,
        30.0
    );
    planner.add_establishment(novaPoshta);

    MainWindow window(&planner);
    window.show();

    return app.exec();
}