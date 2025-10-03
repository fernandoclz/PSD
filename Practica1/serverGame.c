#include "serverGame.h"
#include "signal.h"
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_HILOS 10
volatile sig_atomic_t apagar = 0;

void signal_handler(int sig)
{
	apagar = 1;
}

tPlayer getNextPlayer(tPlayer currentPlayer)
{

	tPlayer next;

	if (currentPlayer == player1)
		next = player2;
	else
		next = player1;

	return next;
}

void initDeck(tDeck *deck)
{

	deck->numCards = DECK_SIZE;

	for (int i = 0; i < DECK_SIZE; i++)
	{
		deck->cards[i] = i;
	}
}

void clearDeck(tDeck *deck)
{

	// Set number of cards
	deck->numCards = 0;

	for (int i = 0; i < DECK_SIZE; i++)
	{
		deck->cards[i] = UNSET_CARD;
	}
}

void printSession(tSession *session)
{

	printf("\n ------ Session state ------\n");

	// Player 1
	printf("%s [bet:%d; %d chips] Deck:", session->player1Name, session->player1Bet, session->player1Stack);
	printDeck(&(session->player1Deck));

	// Player 2
	printf("%s [bet:%d; %d chips] Deck:", session->player2Name, session->player2Bet, session->player2Stack);
	printDeck(&(session->player2Deck));

	// Current game deck
	if (DEBUG_PRINT_GAMEDECK)
	{
		printf("Game deck: ");
		printDeck(&(session->gameDeck));
	}
}

void initSession(tSession *session)
{

	clearDeck(&(session->player1Deck));
	session->player1Bet = 0;
	session->player1Stack = INITIAL_STACK;

	clearDeck(&(session->player2Deck));
	session->player2Bet = 0;
	session->player2Stack = INITIAL_STACK;

	initDeck(&(session->gameDeck));
}

unsigned int calculatePoints(tDeck *deck)
{

	unsigned int points;

	// Init...
	points = 0;

	for (int i = 0; i < deck->numCards; i++)
	{

		if (deck->cards[i] % SUIT_SIZE < 9)
			points += (deck->cards[i] % SUIT_SIZE) + 1;
		else
			points += FIGURE_VALUE;
	}

	return points;
}

unsigned int getRandomCard(tDeck *deck)
{

	unsigned int card, cardIndex, i;

	// Get a random card
	cardIndex = rand() % deck->numCards;
	card = deck->cards[cardIndex];

	// Remove the gap
	for (i = cardIndex; i < deck->numCards - 1; i++)
		deck->cards[i] = deck->cards[i + 1];

	// Update the number of cards in the deck
	deck->numCards--;
	deck->cards[deck->numCards] = UNSET_CARD;

	return card;
}

void addCard(tDeck *deck, tSession *partida)
{
	deck->cards[deck->numCards] = getRandomCard((&partida->gameDeck));
	deck->numCards++;
}

void sendDeck(int socket, tDeck *deck)
{
	int tam = sizeof(tDeck);
	send(socket, &tam, sizeof(tam), 0);
	send(socket, deck, tam, 0);
}

void sendStringMessage(int socket, const char *m)
{
	char buffer[STRING_LENGTH];
	strncpy(buffer, m, STRING_LENGTH - 1);
	buffer[STRING_LENGTH - 1] = '\0';

	int responseLength = strlen(buffer);
	int sent;
	sent = send(socket, &responseLength, sizeof(int), 0);
	if (sent < 0)
	{
		perror("envio de mensages\n");
	}
	sent = send(socket, buffer, responseLength, 0);
	if (sent < 0)
	{
		perror("envio de mensages\n");
	}
}

void recvStringMessage(int socket, char *m)
{
	int length = 0, messageLength;
	memset(m, 0, STRING_LENGTH);
	messageLength = recv(socket, &length, sizeof(int), 0);
	if (messageLength < 0)
		perror("recibo de mensajes\n");
	messageLength = recv(socket, m, length, 0);
	if (messageLength < 0)
		perror("recibo de mensajes\n");
}

int determinarGanador(unsigned int puntos1, unsigned int puntos2)
{
	if (puntos1 > 21 && puntos2 > 21)
		return 0; // Empate
	if (puntos1 > 21)
		return 2; // player2
	if (puntos2 > 21)
		return 1; // player1
	if (puntos1 > puntos2)
		return 1;
	if (puntos2 > puntos1)
		return 2;
	return 0; // Empate
}

void recogerApuesta(int playerSocket, unsigned int *stack, unsigned int *bet)
{
	unsigned int code = TURN_BET;
	while (code == TURN_BET)
	{
		send(playerSocket, &code, sizeof(code), 0);
		send(playerSocket, stack, sizeof(*stack), 0);
		recv(playerSocket, bet, sizeof(*bet), 0);
		code = 0;
		if (*bet > *stack || *bet < 1)
			code = TURN_BET;
		else
			code = TURN_BET_OK;
	}
}

void jugarPartida(tSession *partida, int socketAct, int socketSig, tPlayer jugadorActual)
{
	unsigned int code;
	tDeck *aux = NULL;
	unsigned int puntosAct = 0;

	code = TURN_PLAY_WAIT;
	if (SERVER_DEBUG)
	{
		printf("Enviando a sig\n");
		showCode(code);
	}
	send(socketSig, &code, sizeof(code), 0);

	for (int i = 0; i < 2; i++)
	{
		if (jugadorActual == player1)
			aux = &partida->player1Deck;
		else
			aux = &partida->player2Deck;
		code = TURN_PLAY;
		if (SERVER_DEBUG)
		{
			printf("Enviando a act\n");
			showCode(code);
		}
		send(socketAct, &code, sizeof(code), 0);

		clearDeck(aux);
		addCard(aux, partida);
		addCard(aux, partida);
		puntosAct = calculatePoints(aux);
		if (SERVER_DEBUG)
			printf("Enviando a act, los puntos %u\n", puntosAct);
		send(socketAct, &puntosAct, sizeof(unsigned int), 0);
		if (SERVER_DEBUG)
			printf("Enviando a sig, los puntos %u\n", puntosAct);
		send(socketSig, &puntosAct, sizeof(unsigned int), 0);
		if (SERVER_DEBUG)
			printf("Enviando a act, mazo.\n");
		sendDeck(socketAct, aux);
		if (SERVER_DEBUG)
			printf("Enviando a sig, mazo.\n");
		sendDeck(socketSig, aux);

		code = 0;
		recv(socketAct, &code, sizeof(code), 0);
		if (SERVER_DEBUG)
			showCode(code);

		while (code == TURN_PLAY_HIT && puntosAct <= 21)
		{
			addCard(aux, partida);
			puntosAct = calculatePoints(aux);
			int playOut = 0;

			if (puntosAct > 21)
			{
				playOut = 1;
				code = TURN_PLAY_OUT;
				if (SERVER_DEBUG)
				{
					printf("Enviando a act\n");
					showCode(code);
				}
				send(socketAct, &code, sizeof(code), 0);
				if (SERVER_DEBUG)
					printf("Enviando a act, los puntos %u\n", puntosAct);
				send(socketAct, &puntosAct, sizeof(unsigned int), 0);
				if (SERVER_DEBUG)
					printf("Enviando a act, mazo.\n");
				sendDeck(socketAct, aux);
			}
			else
			{
				code = TURN_PLAY;
				if (SERVER_DEBUG)
				{
					printf("Enviando a act\n");
					showCode(code);
				}
				send(socketAct, &code, sizeof(code), 0);
				code = TURN_PLAY_WAIT;
				if (SERVER_DEBUG)
				{
					printf("Enviando a sig\n");
					showCode(code);
				}
				send(socketSig, &code, sizeof(code), 0);
				if (SERVER_DEBUG)
					printf("Enviando a act, los puntos %u\n", puntosAct);
				send(socketAct, &puntosAct, sizeof(unsigned int), 0);
				if (SERVER_DEBUG)
					printf("Enviando a sig, los puntos %u\n", puntosAct);
				send(socketSig, &puntosAct, sizeof(unsigned int), 0);
				if (SERVER_DEBUG)
					printf("Enviando a act, mazo.\n");
				sendDeck(socketAct, aux);
				if (SERVER_DEBUG)
					printf("Enviando a sig, mazo.\n");
				sendDeck(socketSig, aux);
			}
			if (playOut == 0)
			{
				code = 0;
				recv(socketAct, &code, sizeof(code), 0);
			}
		}
		if (code == TURN_PLAY_STAND || code == TURN_PLAY_OUT)
		{
			code = TURN_PLAY_WAIT;
			if (SERVER_DEBUG)
			{
				printf("Enviando a act\n");
				showCode(code);
			}
			send(socketAct, &code, sizeof(code), 0);
			code = TURN_PLAY_RIVAL_DONE;
			if (SERVER_DEBUG)
			{
				printf("Enviando a sig\n");
				showCode(code);
			}
			send(socketSig, &code, sizeof(code), 0);
		}

		if (i == 0)
		{
			jugadorActual = getNextPlayer(jugadorActual);
			if (SERVER_DEBUG)
				printf("cambio de jugador\n");
			int tempSocket = socketAct;
			socketAct = socketSig;
			socketSig = tempSocket;
		}
	}
	// salir del wait
	unsigned int salida = 22;
	send(socketAct, &salida, sizeof(unsigned int), 0);
}

void puntuaje(tSession *partida, int socketPlayer1, int socketPlayer2)
{
	unsigned int puntos1 = calculatePoints(&partida->player1Deck);
	if (SERVER_DEBUG)
		printf("%u\n", puntos1);
	unsigned int puntos2 = calculatePoints(&partida->player2Deck);
	if (SERVER_DEBUG)
		printf("%u\n", puntos2);
	int resultado = determinarGanador(puntos1, puntos2);
	unsigned int code;

	if (resultado == 1)
	{
		partida->player1Stack += partida->player2Bet;
		partida->player2Stack -= partida->player2Bet;
		printf("Player 1 wins!\n");
	}
	else if (resultado == 2)
	{
		partida->player2Stack += partida->player1Bet;
		partida->player1Stack -= partida->player1Bet;
		printf("Player 2 wins!\n");
	}
	else
	{
		printf("Draw\n");
	}

	if (partida->player1Stack == 0 || partida->player2Stack == 0)
	{
		printSession(partida);
		if (partida->player1Stack == 0)
		{
			code = TURN_GAME_LOSE;
			send(socketPlayer1, &code, sizeof(code), 0);
			code = TURN_GAME_WIN;
			send(socketPlayer2, &code, sizeof(code), 0);
		}
		else
		{
			code = TURN_GAME_LOSE;
			send(socketPlayer2, &code, sizeof(code), 0);
			code = TURN_GAME_WIN;
			send(socketPlayer1, &code, sizeof(code), 0);
		}
	}
}

void *hilo(void *args)
{
	tThreadArgs *threadArgs = (tThreadArgs *)args;
	int socketPlayer1 = threadArgs->socketPlayer1;
	int socketPlayer2 = threadArgs->socketPlayer2;

	free(threadArgs);
	tSession partida;
	initSession(&partida);
	// Nombre de jugador1
	recvStringMessage(socketPlayer1, partida.player1Name);
	printf("Player 1: %s\n", partida.player1Name);
	// Saludo
	sendStringMessage(socketPlayer1, "Welcome ");
	sendStringMessage(socketPlayer1, partida.player1Name);
	sendStringMessage(socketPlayer1, "Waiting to bet!\n");

	// Nombre de jugador2
	recvStringMessage(socketPlayer2, partida.player2Name);
	printf("Player 2: %s\n", partida.player2Name);
	// Saludo
	sendStringMessage(socketPlayer2, "Welcome ");
	sendStringMessage(socketPlayer2, partida.player2Name);
	sendStringMessage(socketPlayer2, "Waiting to bet!\n");

	if (DEBUG_PRINT_GAMEDECK)
		printSession(&partida);

	unsigned int hayGanador = 0;
	while (hayGanador == 0)
	{
		clearDeck(&(partida.player1Deck));
		clearDeck(&(partida.player2Deck));
		partida.player1Bet = 0;
		partida.player2Bet = 0;

		recogerApuesta(socketPlayer1, &partida.player1Stack, &partida.player1Bet);
		if (DEBUG_PRINT_GAMEDECK)
			printSession(&partida);
		recogerApuesta(socketPlayer2, &partida.player2Stack, &partida.player2Bet);
		if (DEBUG_PRINT_GAMEDECK)
			printSession(&partida);

		tPlayer jugadorActual;
		int socketAct, socketSig;
		if (rand() % 2 == 0)
		{
			jugadorActual = player1;
			socketAct = socketPlayer1;
			socketSig = socketPlayer2;
		}
		else
		{
			jugadorActual = player2;
			socketAct = socketPlayer2;
			socketSig = socketPlayer1;
		}

		jugarPartida(&partida, socketAct, socketSig, jugadorActual);
		puntuaje(&partida, socketPlayer1, socketPlayer2);
		if (partida.player1Stack == 0 || partida.player2Stack == 0)
			hayGanador = 1;
	}
	close(socketPlayer1);
	close(socketPlayer2);
	pthread_exit(NULL);
}

int main(int argc, char *argv[])
{

	int socketfd;					   /** Socket descriptor */
	struct sockaddr_in serverAddress;  /** Server address structure */
	unsigned int port;				   /** Listening port */
	struct sockaddr_in player1Address; /** Client address structure for player 1 */
	struct sockaddr_in player2Address; /** Client address structure for player 2 */
	int socketPlayer1;				   /** Socket descriptor for player 1 */
	int socketPlayer2;				   /** Socket descriptor for player 2 */
	unsigned int clientLength;		   /** Length of client structure */
	tThreadArgs *threadArgs;		   /** Thread parameters */
	pthread_t threadID[MAX_HILOS];	   /** Thread ID */
	int numHilos = 0;
	// Seed
	srand(time(0));

	// Check arguments
	if (argc != 2)
	{
		fprintf(stderr, "ERROR wrong number of arguments\n");
		fprintf(stderr, "Usage:\n$>%s port\n", argv[0]);
		exit(1);
	}

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	// Create the socket
	socketfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// Check
	if (socketfd < 0)
		showError("ERROR while opening socket");

	// Init server structure
	memset(&serverAddress, 0, sizeof(serverAddress));

	// Get listening port
	port = atoi(argv[1]);

	// Fill server structure
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddress.sin_port = htons(port);

	// Bind
	if (bind(socketfd, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
		showError("ERROR while binding");

	// Listen
	listen(socketfd, 10);
	fcntl(socketfd, F_SETFL, O_NONBLOCK);
	while (!apagar)
	{
		clientLength = sizeof(player1Address);

		// Accept!
		socketPlayer1 = accept(socketfd, (struct sockaddr *)&player1Address, &clientLength);
		// Check accept result
		if (socketPlayer1 < 0)
		{
			if (errno == EAGAIN)
			{
				// Senial que se manda cuando se ha puesto el socket en NO BLOQUEANTE, indica que no se ha conseguido un valor para la variable
				usleep(100000); // Espero 100ms
				continue;
			}
			else if (errno == EINTR)
			{
				continue;
			}
			else
			{
				printf("ERROR while accepting\n");
				continue;
			}
		}

		// Get length of client structure
		clientLength = sizeof(player2Address);

		socketPlayer2 = accept(socketfd, (struct sockaddr *)&player2Address, &clientLength);

		if (socketPlayer2 < 0)
		{
			if (errno == EAGAIN)
			{
				// Senial que se manda cuando se ha puesto el socket en NO BLOQUEANTE, indica que no se ha conseguido un valor para la variable
				usleep(100000); // Espero 100ms
				continue;
			}
			else if (errno == EINTR)
			{
				continue;
			}
			else
			{
				printf("ERROR while accepting\n");
				continue;
			}
		}
		if (numHilos >= MAX_HILOS)
		{
			printf("Maximo de hilos alcanzado\n");
			continue;
		}
		threadArgs = (tThreadArgs *)malloc(sizeof(tThreadArgs));
		if (threadArgs == NULL)
		{
			perror("ERROR allocating thread arguments");
			close(socketPlayer1);
			close(socketPlayer2);
			continue;
		}

		threadArgs->socketPlayer1 = socketPlayer1;
		threadArgs->socketPlayer2 = socketPlayer2;

		if (pthread_create(&threadID[numHilos], NULL, hilo, (void *)threadArgs) != 0)
		{
			perror("ERROR creating thread");
			free(threadArgs);
			close(socketPlayer1);
			close(socketPlayer2);
			continue;
		}
		numHilos++;
	}
	for (int i = 0; i < numHilos; i++)
	{
		pthread_join(threadID[i], NULL);
	}
	close(socketfd);

	return 0;
}