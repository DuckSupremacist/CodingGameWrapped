#include <iostream>
#include <string>

uint32_t getInitialHash()
{
    uint32_t hash = 0;
    for (int i = 1e8; i > 0; i /= 10)
    {
        int value;
        std::cin >> value;
        std::cin.ignore();
        hash += value * i;
    }
    return hash;
}

#ifdef BUILD_TESTS
int testMain()
#else

int main()
#endif
{
    int depth;
    std::cin >> depth;
    std::cin.ignore();
    std::cerr << "Depth: " << depth << std::endl;
    uint32_t initial_hash = getInitialHash();
    std::cerr << "Initial hash: " << initial_hash << std::endl;

    std::cout << 0 << std::endl;

    return 0;
}
