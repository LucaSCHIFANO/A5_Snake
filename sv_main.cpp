#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "sh_protocol.hpp"
#include "sh_constants.hpp"
#include <SFML/System/Clock.hpp> //< Gestion du temps avec la SFML
#include <cassert> //< assert
#include <iostream> //< std::cout/std::cerr
#include <string> //< std::string / std::string_view
#include <thread> //< std::thread
#include <vector>

// Sous Windows il faut linker ws2_32.lib (Propriétés du projet => �diteur de lien => Entr�e => D�pendances suppl�mentaires)
// Ce projet est �galement configur� en C++17 (ce n'est pas n�cessaire à winsock)

/*
//////
Squelette d'un serveur de Snake
//////
*/

using namespace std;

// On d�clare un prototype des fonctions que nous allons d�finir plus tard
// (en C++ avant d'appeler une fonction il faut dire au compilateur qu'elle existe, quitte à la d�finir apr�s)
int server(SOCKET sock);
void tick();

int main()
{
	// Initialisation de Winsock en version 2.2
	// Cette op�ration est obligatoire sous Windows avant d'utiliser les sockets
	WSADATA data;
	WSAStartup(MAKEWORD(2, 2), &data); //< MAKEWORD compose un entier 16bits à partir de deux entiers 8bits utilis�s par WSAStartup pour conna�tre la version à initialiser

	// La cr�ation d'une socket se fait à l'aide de la fonction `socket`, celle-ci prend la famille de sockets, le type de socket,
	// ainsi que le protocole d�sir� (0 est possible en troisi�me param�tre pour laisser le choix du protocole à la fonction).
	// Pour IPv4, on utilisera AF_INET et pour IPv6 AF_INET6
	// Ici on initialise donc une socket TCP
	// Sous POSIX la fonction renvoie un entier valant -1 en cas d'erreur
	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
	{
		// En cas d'erreur avec Winsock, la fonction WSAGetLastError() permet de r�cup�rer le dernier code d'erreur
		// Sous POSIX l'�quivalent est errno
		std::cerr << "failed to open socket (" << WSAGetLastError() << ")\n";
		return EXIT_FAILURE;
	}

	BOOL option = 1;
	if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&option), sizeof(option)) == SOCKET_ERROR)
	{
		std::cerr << "failed to disable Naggle's algorithm (" << WSAGetLastError() << ")\n";
		return EXIT_FAILURE;
	}

	int r = server(sock);	

	// Comme dans le premier code, on n'oublie pas de fermer les sockets d�s qu'on en a plus besoin
	closesocket(sock);

	// Et on arr�te l'application r�seau �galement.
	WSACleanup();

	return r; //< On retourne le code d'erreur de la fonction server / client
}

int server(SOCKET sock)
{
	// On compose une adresse IP (celle-ci sert à d�crire ce qui est autoris� à se connecter ainsi que le port d'�coute)
	// Cette adresse IP est associ�e à un port ainsi qu'� une famille (IPv4/IPv6)
	sockaddr_in bindAddr;
	bindAddr.sin_addr.s_addr = INADDR_ANY;
	bindAddr.sin_port = htons(port); //< Conversion du nombre en big endian (endianness r�seau)
	bindAddr.sin_family = AF_INET;

	// On associe notre socket à une adresse / port d'�coute
	if (bind(sock, reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr)) == SOCKET_ERROR)
	{
		std::cerr << "failed to bind socket (" << WSAGetLastError() << ")\n";
		return EXIT_FAILURE;
	}

	// On passe la socket en mode �coute, passant notre socket TCP en mode serveur, capable d'accepter des connexions externes
	// Le second argument de la fonction est le nombre de clients maximum pouvant �tre en attente
	if (listen(sock, 10) == SOCKET_ERROR)
	{
		std::cerr << "failed to put socket into listen mode (" << WSAGetLastError() << ")\n";
		return EXIT_FAILURE;
	}

	// On d�finit une structure pour Représenter une liste de client (avec un identifiant num�rique)
	unsigned int nextClientId = 1;

	struct Client
	{
		SOCKET socket;
		unsigned int id;
		std::vector<std::uint8_t> pendingData;
		std::string name;
	};

	std::vector<Client> clients;

	// On d�clare quelques petits outils pour g�rer le temps
	sf::Clock clock;

	// Temps entre les ticks (tours de jeu)
	sf::Time tickInterval = sf::seconds(tickDelay);
	sf::Time nextTick = clock.getElapsedTime() + tickInterval;

	// On déclare de quoi stoquer les pommes
	struct appleStock
	{
		int positionx;
		int positiony;
		appleStock(int x, int y) 
		{
			positionx = x;
			positiony = y;
		}
	};

	std::vector<appleStock> appleStorage;


	// Boucle infinie pour continuer d'accepter des clients
	for (;;)
	{
		// On construit une liste de descripteurs pour la fonctions WSAPoll, qui nous permet de surveiller plusieurs sockets simultan�ment
		// Ces descripteurs r�f�rencent les sockets à surveiller ainsi que les �v�nements à �couter (le plus souvent on surveillera l'entr�e,
		// à l'aide de POLLRDNORM). Ceci va d�tecter les donn�es re�ues en entr�e par nos sockets, mais aussi les �v�nements de d�connexion.
		// Dans le cas de la socket serveur, cela permet aussi de savoir lorsqu'un client est en attente d'acceptation (et donc que l'appel à accept ne va pas bloquer).

		// Note: on pourrait ne pas reconstruire le tableau à chaque fois, si vous voulez le faire en exercice ;o
		std::vector<WSAPOLLFD> pollDescriptors;
		{
			// La m�thode emplace_back construit un objet à l'int�rieur du vector et nous renvoie une r�f�rence dessus
			// alternativement nous pourrions �galement construire une variable de type WSAPOLLFD et l'ajouter au vector avec push_back 
			auto& serverDescriptor = pollDescriptors.emplace_back();
			serverDescriptor.fd = sock;
			serverDescriptor.events = POLLRDNORM;
			serverDescriptor.revents = 0;
		}

		// On rajoute un descripteur pour chacun de nos clients actuels
		for (Client& client : clients)
		{
			auto& clientDescriptor = pollDescriptors.emplace_back();
			clientDescriptor.fd = client.socket;
			clientDescriptor.events = POLLRDNORM;
			clientDescriptor.revents = 0;
		}

		// On appelle la fonction WSAPoll (�quivalent poll sous Linux) pour bloquer jusqu'� ce qu'un �v�nement se produise
		// au niveau d'une de nos sockets. Cette fonction attend un nombre d�fini de millisecondes (-1 pour une attente infinie) avant
		// de retourner le nombre de sockets actives.
		int activeSockets = WSAPoll(pollDescriptors.data(), pollDescriptors.size(), 1);
		if (activeSockets == SOCKET_ERROR)
		{
			std::cerr << "failed to poll sockets (" << WSAGetLastError() << ")\n";
			return EXIT_FAILURE;
		}

		// activeSockets peut avoir trois valeurs diff�rentes :
		// - SOCKET_ERROR en cas d'erreur (g�r� plus haut)
		// - 0 si aucune socket ne s'est activ�e avant la fin du d�lai
		// - > 0, avec le nombre de sockets activ�es
		// Dans notre cas, comme le d�lai est infini et l'erreur g�r�e plus haut, nous ne pouvons qu'avec un nombre positifs de sockets

		if (activeSockets > 0)
		{
			// WSAPoll modifie le champ revents des descripteurs pass�s en param�tre pour indiquer les �v�nements d�clench�s
			for (WSAPOLLFD& descriptor : pollDescriptors)
			{
				// Si ce descripteur n'a pas été actif, on passe au suivant
				if (descriptor.revents == 0)
					continue;

				// Ce descripteur a été d�clench�, et deux cas de figures sont possibles.
				// Soit il s'agit du descripteur de la socket serveur (celle permettant la connexion de clients), signifiant qu'un nouveau client est en attente
				// Soit une socket client est active, signifiant que nous avons re�u des donn�es (ou potentiellement que le client s'est d�connect�)
				if (descriptor.fd == sock)
				{
					// Nous sommes dans le cas du serveur, un nouveau client est donc disponible
					sockaddr_in clientAddr;
					int clientAddrSize = sizeof(clientAddr);

					SOCKET newClient = accept(sock, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrSize);
					if (newClient == INVALID_SOCKET)
					{
						std::cerr << "failed to accept new snake (" << WSAGetLastError() << ")\n";
						return EXIT_FAILURE;
					}

					// Rajoutons un client à notre tableau, avec son propre ID num�rique
					auto& client = clients.emplace_back();
					client.id = nextClientId++;
					client.socket = newClient;
					client.name = "";

					// Représente une adresse IP (celle du client venant de se connecter) sous forme textuelle
					char strAddr[INET_ADDRSTRLEN];
					inet_ntop(clientAddr.sin_family, &clientAddr.sin_addr, strAddr, INET_ADDRSTRLEN);

					std::cout << "snake#" << client.id << " connected from " << strAddr << std::endl;

					// On rajoute les pommes au join d'un nouveau joueur
					for (int i = 0; i < appleStorage.size(); i++)
					{
						std::vector<std::uint8_t> messageToSend = SerializeAppleToClient(sf::Vector2i(appleStorage[i].positionx, appleStorage[i].positiony));
						SendData(client.socket, messageToSend.data(), messageToSend.size());
					}

					// Ici nous pourrions envoyer un message à tous les clients pour indiquer la connexion d'un nouveau client

					std::vector<std::uint8_t> sendBuffer = SerializeConnection(client.id, true);

					for (Client& c : clients)
					{
						if (c.socket == client.socket)
							continue;

						SendData(c.socket, sendBuffer.data(), sendBuffer.size()); //inform other of new client
					}

				}
				else
				{
					// Nous sommes dans le cas où le descripteur Représente un client, tâchons de retrouver lequel
					auto clientIt = std::find_if(clients.begin(), clients.end(), [&](const Client& c)
						{
							return c.socket == descriptor.fd;
						});
					assert(clientIt != clients.end());

					Client& client = *clientIt;

					// La socket a été activée, tentons une lecture
					char buffer[1024];
					int byteRead = recv(client.socket, buffer, sizeof(buffer), 0);
					if (byteRead == SOCKET_ERROR || byteRead == 0)
					{
						// Une erreur s'est produite ou le nombre d'octets lus est de z�ro, indiquant une d�connexion
						// on adapte le message en fonction.
						if (byteRead == SOCKET_ERROR)
							std::cerr << "failed to read from snake#" << client.id << " (" << WSAGetLastError() << "), disconnecting..." << std::endl;
						else
							std::cout << "Snake#" << client.id << " disconnected" << std::endl;

						// Ici aussi nous pourrions envoyer un message à tous les clients pour notifier la d�connexion d'un client


						std::vector<std::uint8_t> sendBuffer = SerializeConnection(client.id, false);

						for (Client& c : clients)
						{
							if (c.socket == client.socket) continue;

							SendData(c.socket, sendBuffer.data(), sendBuffer.size());
						}

						// On oublie pas de fermer la socket avant de supprimer le client de la liste
						closesocket(client.socket);
						clients.erase(clientIt);
					}
					else
					{
						// Nous avons re�u un message de la part du client, affichons-le et renvoyons le aux autres clients
						std::size_t oldSize = client.pendingData.size();
						client.pendingData.resize(oldSize + byteRead);
						std::memcpy(&client.pendingData[oldSize], buffer, byteRead);

						while (client.pendingData.size() >= sizeof(std::uint16_t))
						{
							// -- R�ception du message --

							// On d�serialise la taille du message
							std::uint16_t messageSize;
							std::memcpy(&messageSize, &client.pendingData[0], sizeof(messageSize));
							messageSize = ntohs(messageSize);

							if (client.pendingData.size() - sizeof(messageSize) < messageSize)
								break;

							// On retire la taille que nous de traiter des donn�es en attente
							std::size_t handledSize = sizeof(messageSize) + messageSize;
							std::uint8_t opcode;
							std::memcpy(&opcode, &client.pendingData[sizeof(messageSize)], sizeof(uint8_t));
							Opcode code = (Opcode)opcode;

							std::vector<std::uint8_t> receivedMessage(messageSize);

							if (code == OpcodeSnakePosition) {

								std::memcpy(&receivedMessage[0], &client.pendingData[sizeof(messageSize) + sizeof(uint8_t)], messageSize - sizeof(uint8_t));

								// On retire la taille que nous de traiter des donnees en attente
								client.pendingData.erase(client.pendingData.begin(), client.pendingData.begin() + handledSize);
								std::vector<std::uint8_t> messageToSend = SerializeSnakeToClient(sf::Vector2i((int)receivedMessage[0], (int)receivedMessage[1]), client.id);
								
								for (Client& c : clients)
								{
									if (c.socket == client.socket) continue;

									SendData(c.socket, messageToSend.data(), messageToSend.size());
								}

								//std::cout << "-> Client #" << client.id << "'s direction :" << (int)receivedMessage[0] << ", " << (int)receivedMessage[1] << std::endl;
							}
							else if (code == OpcodeSnakeDeath) 
							{
								// On retire la taille que nous de traiter des donnees en attente
								client.pendingData.erase(client.pendingData.begin(), client.pendingData.begin() + handledSize);
								std::vector<std::uint8_t> messageToSend = SerializeDeathToClient(client.id);
							
								for (Client& c : clients)
								{
									if (c.socket == client.socket) continue;

									SendData(c.socket, messageToSend.data(), messageToSend.size());
								}
							}

							else if (code == OpcodeApple) {

								std::memcpy(&receivedMessage[0], &client.pendingData[sizeof(messageSize) + sizeof(uint8_t)], messageSize - sizeof(uint8_t));

								// On retire la taille que nous de traiter des donnees en attente
								client.pendingData.erase(client.pendingData.begin(), client.pendingData.begin() + handledSize);
								std::vector<std::uint8_t> messageToSend = SerializeAppleToClient(sf::Vector2i((int)receivedMessage[0], (int)receivedMessage[1]));

								appleStorage.push_back(appleStock((int)receivedMessage[0], (int)receivedMessage[1]));


								for (Client& c : clients)
								{
									if (c.socket == client.socket) continue;

									SendData(c.socket, messageToSend.data(), messageToSend.size());
								}
							}
							else if (code == OpcodeEat) {

								std::memcpy(&receivedMessage[0], &client.pendingData[sizeof(messageSize) + sizeof(uint8_t)], messageSize - sizeof(uint8_t));

								// On retire la taille que nous de traiter des donnees en attente
								client.pendingData.erase(client.pendingData.begin(), client.pendingData.begin() + handledSize);
								std::vector<std::uint8_t> messageToSend = SerializeEatToClient(sf::Vector2i((int)receivedMessage[0], (int)receivedMessage[1]), client.id);

								auto it = appleStorage.begin();
								for (it = appleStorage.begin(); it != appleStorage.end(); it++)
								{
									if (it->positionx == (int)receivedMessage[0] && it->positiony == (int)receivedMessage[1])
									{
										appleStorage.erase(it);
										break;
									}
								}

								for (Client& c : clients)
								{
									if (c.socket == client.socket) continue;

									SendData(c.socket, messageToSend.data(), messageToSend.size());
								}
							}
							else if (code == OpcodeChangeName) {

								std::memcpy(&receivedMessage[0], &client.pendingData[sizeof(messageSize) + sizeof(uint8_t)], messageSize - sizeof(uint8_t));
								client.pendingData.erase(client.pendingData.begin(), client.pendingData.begin() + handledSize);
								client.name = (char*)receivedMessage.data();

								std::vector<std::uint8_t> messageToSend = SerializeNameToClient((char*)receivedMessage.data(), client.id);
								std::vector<std::uint8_t> messageToReceive = SerializeNameToClient((char*)receivedMessage.data(), client.id);

								for (Client& c : clients)
								{
									if (c.socket == client.socket) continue;

									SendData(c.socket, messageToSend.data(), messageToSend.size());

									messageToReceive = SerializeNameToClient(c.name, c.id);
									SendData(client.socket, messageToReceive.data(), messageToReceive.size());
								}
							}
							if (code == OpcodeSnakeBody) {

								std::memcpy(&receivedMessage[0], &client.pendingData[sizeof(messageSize) + sizeof(uint8_t)], messageSize - sizeof(uint8_t));
								uint8_t toClientId = (int)receivedMessage[0];
								// On retire la taille que nous de traiter des donnees en attente

								std::vector<sf::Vector2i> _body;

								for (size_t i = 1; i < (int)messageSize - sizeof(uint8_t); i++)
								{
									_body.push_back(sf::Vector2i((int)receivedMessage[i], (int)receivedMessage[i + 1]));
									i++;

								}

								std::vector<std::uint8_t> body(messageSize - sizeof(uint8_t));
								std::memcpy(&body[0], &client.pendingData[sizeof(messageSize) + sizeof(uint8_t)], messageSize - sizeof(uint8_t));
								std::vector<std::uint8_t> messageToSend = SerializeSnakeBodyToClient(client.id, _body);

								for (Client& c : clients)
								{
									if (c.id != toClientId) continue;

									SendData(c.socket, messageToSend.data(), messageToSend.size());
								}
								client.pendingData.erase(client.pendingData.begin(), client.pendingData.begin() + handledSize);
							}
						}
					}
				}
			}
		}

		sf::Time now = clock.getElapsedTime();

		// On v�rifie si assez de temps s'est �coul� pour faire avancer la logique du jeu
		if (now >= nextTick)
		{
			// On met à jour la logique du jeu
			tick();

			// On pr�voit la prochaine mise à jour
			nextTick += tickInterval;
		}
	}

	return EXIT_SUCCESS;
}

void tick()
{

}