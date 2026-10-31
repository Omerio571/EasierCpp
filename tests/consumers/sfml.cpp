#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <SFML/Network.hpp>
#include <iostream>

int main()
{
    sf::InputSoundFile sound;
    sf::Packet packet;
    packet << 42;
    sf::RenderWindow window(sf::VideoMode({320, 200}), "CppLiveLab SFML integration");
    for (int frame = 0; frame < 90 && window.isOpen(); ++frame)
    {
        while (const auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
        }
        window.clear(sf::Color::Blue);
        window.display();
        sf::sleep(sf::milliseconds(16));
    }
    std::cout << "CPPLIVELAB_CONSUMER_OK\n";
}
