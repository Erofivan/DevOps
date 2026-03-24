#include <chrono>
#include <iostream>
#include <thread>

int main() {
    std::cout << "C++ Worker is successfully started!" << std::endl;

    for (std::size_t i = 0; i < 3; ++i) {
        std::cout << "Working processing data..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return EXIT_SUCCESS;
}