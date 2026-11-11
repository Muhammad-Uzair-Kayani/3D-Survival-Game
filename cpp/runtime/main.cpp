#include <survival/game_session.hpp>

#ifdef SURVIVAL_ENABLE_PYTHON_PLUGIN
#include "python_policy.hpp"
#endif

#include <iostream>
#include <string>

namespace {

void Print(const survival::GameSnapshot& state) {
    std::cout << "\nTick " << state.tick << "\n";
    for (int y = 0; y < static_cast<int>(state.mapRows.size()); ++y) {
        for (int x = 0; x < static_cast<int>(state.mapRows[y].size()); ++x) {
            if (state.player.position.x == x && state.player.position.y == y)
                std::cout << 'P';
            else if (state.goal.x == x && state.goal.y == y)
                std::cout << 'X';
            else
                std::cout << state.mapRows[y][x];
        }
        std::cout << '\n';
    }
    std::cout << state.message << '\n';
}

survival::Action ParseAction(char key) {
    switch (key) {
    case 'w': case 'W': return survival::Action::MoveNorth;
    case 'd': case 'D': return survival::Action::MoveEast;
    case 's': case 'S': return survival::Action::MoveSouth;
    case 'a': case 'A': return survival::Action::MoveWest;
    default:            return survival::Action::Wait;
    }
}

} // namespace

int main() {
    survival::GameSession game;
#ifdef SURVIVAL_ENABLE_PYTHON_PLUGIN
    survival::client::PythonPolicyPlugin pythonPolicy;
#endif
    Print(game.Snapshot());

#ifdef SURVIVAL_ENABLE_PYTHON_PLUGIN
    std::cout << "Controls: W/A/S/D move, P asks Python policy, Enter waits, R resets, Q quits.\n";
#else
    std::cout << "Controls: W/A/S/D move, Enter waits, R resets, Q quits.\n";
#endif
    std::string line;
    while (std::cout << "> " && std::getline(std::cin, line)) {
        if (!line.empty() && (line[0] == 'q' || line[0] == 'Q'))
            break;
        if (!line.empty() && (line[0] == 'r' || line[0] == 'R')) {
            game.Reset();
            Print(game.Snapshot());
            continue;
        }

        auto action = line.empty() ? survival::Action::Wait : ParseAction(line[0]);
#ifdef SURVIVAL_ENABLE_PYTHON_PLUGIN
        if (!line.empty() && (line[0] == 'p' || line[0] == 'P'))
            action = pythonPolicy.ChooseAction(game.Snapshot());
#endif
        const auto state = game.Step(action);
        Print(state);
        if (state.player.arrived)
            std::cout << "Use R to start another run or Q to exit.\n";
    }

    return 0;
}
