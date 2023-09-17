#include "cl_resources.hpp"
#include "cl_grid.hpp"
#include "cl_snake.hpp"
#include <SFML/Graphics.hpp>
#include "sh_constants.hpp"
#include "sh_protocol.hpp"
#include <thread>


const int windowWidth = cellSize * gridWidth;
const int windowHeight = cellSize * gridHeight;

void ConnectionToServer(SOCKET sock);
void game(SOCKET sock);
void tick(Grid& grid, Snake& snake, SOCKET sock, std::map<int, Snake>& enemySnakes);

int main()
{
	// Initialisation du g�n�rateur al�atoire
	// Note : en C++ moderne on dispose de meilleurs outils pour g�n�rer des nombres al�atoires,
	// mais ils sont aussi plus verbeux / complexes à utiliser, ce n'est pas tr�s int�ressant ici.
	std::srand(std::time(nullptr));

	WSADATA data;
	WSAStartup(MAKEWORD(2, 2), &data);

	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
	{
		std::cerr << "failed to open socket (" << WSAGetLastError() << ")\n";
		return EXIT_FAILURE;
	}

	/*
	// D�sactivation de l'algorithme de naggle sur la socket `sock`
	BOOL option = 1;
	if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&option), sizeof(option)) == SOCKET_ERROR)
	{
		std::cerr << "failed to disable Naggle's algorithm (" << WSAGetLastError() << ")\n";
		return EXIT_FAILURE;
	}
	*/
	ConnectionToServer(sock);
	game(sock);
}

void ConnectionToServer(SOCKET sock) 
{
	//Input ID port
	std::cout << "Input the ID port please : ";
	std::string ipAdress;
	std::getline(std::cin, ipAdress);

	//message -> "127.0.0.1"
	if (ipAdress.empty())
	{
		shutdown(sock, SD_BOTH);
		return;
	}

	sockaddr_in bindAddr;
	bindAddr.sin_family = AF_INET;
	bindAddr.sin_port = htons(14769);

	inet_pton(bindAddr.sin_family, ipAdress.data(), &bindAddr.sin_addr);

	if (connect(sock, reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr)) == SOCKET_ERROR)
	{
		std::cout << "failed to connect to server (" << WSAGetLastError() << ")";
		exit(1);
	}
	else std::cout << "\nConnected to server ! \n" << std::endl;	
}

void game(SOCKET sock)
{


	// On déclare le serpent du joueur qu'on fait apparaitre au milieu de la grille, pointant vers la droite
	// Note : les directions du serpent sont repr�sent�es par le d�calage en X ou en Y n�cessaire pour passer à la case suivante.
	// Ces valeurs doivent toujours �tre à 1 ou -1 et la valeur de l'autre axe doit �tre à z�ro (nos serpents ne peuvent pas se d�placer
	// en diagonale)
	Snake snake(sf::Vector2i(gridWidth / 2, gridHeight / 2), sf::Vector2i(1, 0), sf::Color::Green, 0);

	std::map<int, Snake> enemySnakes;

	// On déclare la grille des �l�ments statiques (murs, pommes)
	// Les �l�ments statiques sont ceux qui �voluent tr�s peu dans le jeu, comparativement aux serpents qui �voluent à chaque
	// tour de jeu ("tick")
	Grid grid(gridWidth, gridHeight);

	//Loop recv
	bool running = true;
	std::thread readThread([&]()
	{
		std::vector<std::uint8_t> pendingData;
		while (running)
		{
			// On attend que le serveur nous envoie un message (ou se déconnecte)
			std::vector<std::uint8_t> response = ReadData(sock);

			// Nous avons re�u un message de la part du server, affichons-le
			std::size_t oldSize = pendingData.size();
			pendingData.resize(oldSize + response.size());
			std::memcpy(&pendingData[oldSize], response.data(), response.size());


			while (pendingData.size() >= sizeof(std::uint16_t))
			{
				// -- Reception du message --

				// On deserialise la taille du message
				std::uint16_t messageSize;
				std::memcpy(&messageSize, &pendingData[0], sizeof(messageSize));
				messageSize = ntohs(messageSize);

				if (pendingData.size() - sizeof(messageSize) < messageSize)
					break;

				// On copie le contenu du message
				//switch(OPcode)

				std::uint8_t opcode;
				std::memcpy(&opcode, &pendingData[sizeof(messageSize)], sizeof(uint8_t));
				Opcode code = (Opcode)opcode;

				std::size_t handledSize = sizeof(messageSize) + messageSize;
				if (code == OpcodeConnection)
				{
					std::vector<std::uint8_t> receivedMessage(messageSize);
					std::memcpy(&receivedMessage[0], &pendingData[sizeof(messageSize) + sizeof(uint8_t)], messageSize - sizeof(uint8_t));

					// On retire la taille que nous de traiter des donnees en attente
					pendingData.erase(pendingData.begin(), pendingData.begin() + handledSize);

					int id = (int)receivedMessage[0];
					if ((int)receivedMessage[1] == 0)  //0 => remove player and snake
					{
						enemySnakes.erase(id);
						std::cout << "Snake#" << id << " left the game..." << std::endl;
					}
					else  //1 => create new player with snake
					{
						Snake clientSnake(sf::Vector2i(gridWidth / 2, gridHeight / 2), sf::Vector2i(1, 0), sf::Color::Red, id);
						enemySnakes.emplace(id, clientSnake);
						std::cout << "A new foe has appared! Snake#" << id << " join the battle!" << std::endl;
						std::vector<std::uint8_t> snakeBody = SerializeSnakeBodyToServer(id, snake.GetBody());
						SendData(sock, snakeBody.data(), snakeBody.size());
					}

				}
				if (code == OpcodeSnakePosition)
				{
					std::vector<std::uint8_t> receivedMessage(messageSize);

					std::memcpy(&receivedMessage[0], &pendingData[sizeof(messageSize) + sizeof(uint8_t)], messageSize - sizeof(uint8_t));

					// On retire la taille que nous de traiter des donnees en attente
					pendingData.erase(pendingData.begin(), pendingData.begin() + handledSize);

					enemySnakes[(int)receivedMessage[0]].Advance(sf::Vector2i((int)receivedMessage[1], (int)receivedMessage[2]));

				}
				if (code == OpcodeSnakeDeath)
				{
					std::vector<std::uint8_t> receivedMessage(messageSize);

					std::memcpy(&receivedMessage[0], &pendingData[sizeof(messageSize) + sizeof(uint8_t)], messageSize - sizeof(uint8_t));

					// On retire la taille que nous de traiter des donnees en attente
					pendingData.erase(pendingData.begin(), pendingData.begin() + handledSize);
				
					enemySnakes[(int)receivedMessage[0]].Respawn(sf::Vector2i(gridWidth / 2, gridHeight / 2), sf::Vector2i(1, 0));
				}
				else if (code == OpcodeApple)
				{
					std::vector<std::int8_t> receivedMessage(messageSize);
					std::memcpy(&receivedMessage[0], &pendingData[sizeof(messageSize) + sizeof(uint8_t)], messageSize - sizeof(uint8_t));

					// On retire la taille que nous de traiter des donnees en attente
					pendingData.erase(pendingData.begin(), pendingData.begin() + handledSize);
					grid.SetCell((int)receivedMessage[1], (int)receivedMessage[2], CellType::Apple);
					

				}
				else if (code == OpcodeEat)
				{
					std::vector<std::int8_t> receivedMessage(messageSize);
					std::memcpy(&receivedMessage[0], &pendingData[sizeof(messageSize) + sizeof(uint8_t)], messageSize - sizeof(uint8_t));

					// On retire la taille que nous de traiter des donnees en attente
					pendingData.erase(pendingData.begin(), pendingData.begin() + handledSize);
					enemySnakes[(int)receivedMessage[0]].Grow();
					grid.SetCell((int)receivedMessage[1], (int)receivedMessage[2], CellType::None);


				}
				else if (code == OpcodeSnakeBody) {
					std::vector<std::int8_t> receivedMessage(messageSize);
					std::memcpy(&receivedMessage[0], &pendingData[sizeof(messageSize) + sizeof(uint8_t)], messageSize - sizeof(uint8_t));

					int id = (int)receivedMessage[0];
					std::vector<sf::Vector2i> body;
					int currentIndex = 0;
					for (size_t i = 0; i < messageSize - sizeof(uint8_t); i++)
					{
						body.push_back(sf::Vector2i(receivedMessage[currentIndex], receivedMessage[currentIndex + 1]));
						currentIndex += 2;
						i++;
					}
					pendingData.erase(pendingData.begin(), pendingData.begin() + handledSize);
					enemySnakes[(int)receivedMessage[0]].SetBody(body);

				}
			}
		}
	});


	// Chargement des assets du jeu
	Resources resources;
	if (!LoadResources(resources))
		return;

	// Cr�ation et ouverture d'une fen�tre
	sf::RenderWindow window(sf::VideoMode(windowWidth, windowHeight), "Snake");
	window.setVerticalSyncEnabled(true);

	// �tant donn� que l'origine de tous les objets est au centre, il faut d�caler la cam�ra d'autant pour garder
	// une logique de grille à l'affichage
	sf::Vector2f viewSize(windowWidth, windowHeight);
	sf::Vector2f viewCenter = viewSize / 2.f - sf::Vector2f(cellSize, cellSize) / 2.f;
	window.setView(sf::View(viewCenter, viewSize));

	
	
	// On déclare quelques petits outils pour g�rer le temps
	sf::Clock clock;
	
	// Temps entre les ticks (tours de jeu)
	sf::Time tickInterval = sf::seconds(tickDelay);
	sf::Time nextTick = clock.getElapsedTime() + tickInterval;

	// Temps entre les apparitions de pommes
	sf::Time appleInterval = sf::seconds(5.f);
	sf::Time nextApple = clock.getElapsedTime() + appleInterval;

	while (window.isOpen())
	{
		// On traite les évènements fen�tre qui se sont produits depuis le dernier tour de boucles
		sf::Event event;
		while (window.pollEvent(event))
		{
			switch (event.type)
			{
				// L'utilisateur souhaite fermer la fen�tre, fermons-la
				case sf::Event::Closed:
					window.close();
					break;

				// Une touche a été enfonc�e par l'utilisateur
				case sf::Event::KeyPressed:
				{
					sf::Vector2i direction = sf::Vector2i(0, 0);

					// On met à jour la direction si la touche sur laquelle l'utilisateur a appuy�e est une fl�che directionnelle
					switch (event.key.code)
					{
						case sf::Keyboard::Up:
							direction = sf::Vector2i(0, -1);
							break;

						case sf::Keyboard::Down:
							direction = sf::Vector2i(0, 1);
							break;

						case sf::Keyboard::Left:
							direction = sf::Vector2i(-1, 0);
							break;

						case sf::Keyboard::Right:
							direction = sf::Vector2i(1, 0);
							break;

						default:
							break;
					}

					// On applique la direction, si modifi�e, au serpent
					if (direction != sf::Vector2i(0, 0))
					{
						// On interdit au serpent de faire un demi-tour complet
						if (direction != -snake.GetCurrentDirection())
							snake.SetFollowingDirection(direction);
					}
					break;
				}

				default:
					break;
			}
		}

		sf::Time now = clock.getElapsedTime();
		
		// On v�rifie si assez de temps s'est �coul� pour faire avancer la logique du jeu
		if (now >= nextTick)
		{
			// On met à jour la logique du jeu
			tick(grid, snake, sock, enemySnakes);

			// On pr�voit la prochaine mise à jour
			nextTick += tickInterval;

		}

		// On v�rifie si assez de temps s'est �coul� pour faire apparaitre une nouvelle pomme
		if (now >= nextApple)
		{
			// On �vite de remplacer un mur par une pomme ...
			int x = 1 + rand() % (gridWidth - 2);
			int y = 1 + rand() % (gridHeight - 2);

			// On �vite de faire apparaitre une pomme sur un serpent
			// si c'est le cas on retentera au prochain tour de boucle
			if (!snake.TestCollision(sf::Vector2i(x, y), true))
			{
				grid.SetCell(x, y, CellType::Apple);

				sf::Vector2i sendPommePos (x, y);

				std::vector<std::uint8_t> sendSnakeBuffer = SerializeAppleToServer(sendPommePos);

				SendData(sock, sendSnakeBuffer.data(), sendSnakeBuffer.size());


				// On pr�voit la prochaine apparition de pomme
				nextApple += appleInterval;
			}
		}

		// On remplit la sc�ne d'une couleur plus jolie pour les yeux
		window.clear(sf::Color(247, 230, 151));

		// On affiche les �l�ments statiques
		grid.Draw(window, resources);

		// On affiche le serpent
		snake.Draw(window, resources);
		auto it = enemySnakes.begin();
		for (it = enemySnakes.begin(); it != enemySnakes.end(); it++)
		{
			it->second.Draw(window, resources);
		}

		// On actualise l'affichage de la fen�tre
		window.display();
	}
}

void tick(Grid& grid, Snake& snake, SOCKET sock, std::map<int, Snake>& enemySnakes)
{
	snake.Advance();
	std::vector<std::uint8_t> sendSnakeBuffer = SerializeSnakeToServer(snake.GetHeadPosition());
	SendData(sock, sendSnakeBuffer.data(), sendSnakeBuffer.size());
	
	/*auto it = enemySnakes.begin();
	for (it = enemySnakes.begin(); it != enemySnakes.end(); it++)
	{
		it->second.Advance();
	}*/

	// On teste la collision de la t�te du serpent avec la grille
	sf::Vector2i headPos = snake.GetHeadPosition();
	switch (grid.GetCell(headPos.x, headPos.y))
	{
		case CellType::Apple:
		{
			// Le serpent mange une pomme, faisons-la disparaitre et faisons grandir le serpent !
			grid.SetCell(headPos.x, headPos.y, CellType::None);
			sf::Vector2i sendPommePos(headPos.x, headPos.y);

			std::vector<std::uint8_t> sendSnakeBuffer = SerializeEatToServer(sendPommePos);

			SendData(sock, sendSnakeBuffer.data(), sendSnakeBuffer.size());
			snake.Grow();
			break;
		}

		case CellType::Wall:
		{
			// Le serpent s'est pris un mur, on le fait r�apparaitre
			snake.Respawn(sf::Vector2i(gridWidth / 2, gridHeight / 2), sf::Vector2i(1, 0));
			std::vector<std::uint8_t> SnakeDeath = SerializeDeathToServer();
			SendData(sock, SnakeDeath.data(), SnakeDeath.size());
			break;
		}

		case CellType::None:
			break; //< rien à faire
	}

	// On v�rifie maintenant que le serpent n'est pas en collision avec lui-m�me
	if (snake.TestCollision(headPos, false))
	{
		// Le serpent s'est tu� tout seul, on le fait r�apparaitre
		snake.Respawn(sf::Vector2i(gridWidth / 2, gridHeight / 2), sf::Vector2i(1, 0));
		std::vector<std::uint8_t> SnakeDeath = SerializeDeathToServer();
		SendData(sock, SnakeDeath.data(), SnakeDeath.size());
	}
	//On check si on est pas en collision avec un autre serpent
	for (auto& snakes : enemySnakes) 
	{
		if (snakes.second.TestCollision(headPos, false))
		{
			// Le serpent s'est tu� tout seul, on le fait r�apparaitre
			snake.Respawn(sf::Vector2i(gridWidth / 2, gridHeight / 2), sf::Vector2i(1, 0));
			std::vector<std::uint8_t> SnakeDeath = SerializeDeathToServer();
			SendData(sock, SnakeDeath.data(), SnakeDeath.size());
		}
	}

	//if (snake.TestCollision(headPos, false))
	//{
	//	// Le serpent s'est tu� tout seul, on le fait r�apparaitre
	//	snake.Respawn(sf::Vector2i(gridWidth / 2, gridHeight / 2), sf::Vector2i(1, 0));
	//	std::vector<std::uint8_t> SnakeDeath = SerializeDeathToServer();
	//	SendData(sock, SnakeDeath.data(), SnakeDeath.size());
	//}
}
