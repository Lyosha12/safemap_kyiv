#include <iostream>
#include <memory>
#include "Infrastructure.h"
#include "Establishments.h"
#include "Crowdsourcing.h"
#include "RoutePlanner.h"

using namespace std;

int main() {
    cout << "SafeMap Kyiv" << endl;

    Coordinates my_location(50.495, 30.345);
    Shelter big_shelter(100, true);

    auto silpo = make_shared<Supermarket>(1, my_location, "Silpo", big_shelter, true, true);
    auto nova_poshta = make_shared<PostOffice>(2, my_location, "NP 1", big_shelter, false, 30.0);

    RoutePlanner planner;
    planner.add_establishment(silpo);
    planner.add_establishment(nova_poshta);

    cout << "\n[TEST] Finding all nodes..." << endl;
    auto all_open = planner.find_safe_nodes([](shared_ptr<Establishment> node) {
        return true;
    });
    cout << "Open nodes in peace time: " << all_open.size() << endl;

    cout << "\n[TEST] Yellow Alert Activated!" << endl;
    planner.set_alert_level(AlertLevel::Yellow);

    cout << "Silpo access: " << silpo->is_accessible(AlertLevel::Yellow) << endl;
    cout << "Nova Poshta access: " << nova_poshta->is_accessible(AlertLevel::Yellow) << endl;

    cout << "\n[TEST] User sends report about Silpo generator failure..." << endl;
    User user_oleg("Oleg");
    auto report = make_shared<StatusReport>(user_oleg, false, 50);

    planner.process_user_report(report, silpo);

    cout << "Silpo access after report (no generator): " << silpo->is_accessible(AlertLevel::Yellow) << endl;

    return 0;
}