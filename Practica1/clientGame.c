#include "clientGame.h"

unsigned int readBet()
{

	int isValid, bet = 0;
	tString enteredMove;

	// While player does not enter a correct bet...
	do
	{

		// Init...
		bzero(enteredMove, STRING_LENGTH);
		isValid = TRUE;

		printf("Enter a value:");
		fgets(enteredMove, STRING_LENGTH - 1, stdin);
		enteredMove[strlen(enteredMove) - 1] = 0;

		// Check if each character is a digit
		for (int i = 0; i < strlen(enteredMove) && isValid; i++)
			if (!isdigit(enteredMove[i]))
				isValid = FALSE;

		// Entered move is not a number
		if (!isValid)
			printf("Entered value is not correct. It must be a number greater than 0\n");
		else
			bet = atoi(enteredMove);

	} while (!isValid);

	printf("\n");

	return ((unsigned int)bet);
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

unsigned int readOption()
{

	unsigned int bet;

	do
	{
		printf("What is your move? Press %d to hit a card and %d to stand\n", TURN_PLAY_HIT, TURN_PLAY_STAND);
		bet = readBet();
		if ((bet != TURN_PLAY_HIT) && (bet != TURN_PLAY_STAND))
			printf("Wrong option!\n");
	} while ((bet != TURN_PLAY_HIT) && (bet != TURN_PLAY_STAND));

	return bet;
}

tDeck receiveDeck(int socket)
{
	tDeck deck;
	int tam;

	// Recibir el tamaño del deck
	recv(socket, &tam, sizeof(tam), 0);
	// Recibir la estructura del deck
	recv(socket, &deck, tam, 0);

	return deck;
}

void opponentTurn(int socketfd, unsigned int *puntosRival)
{
	tDeck suDeck;
	recv(socketfd, puntosRival, sizeof(*puntosRival), 0);
	if (*puntosRival == 22)
		return;
	suDeck = receiveDeck(socketfd);
	printf("Your opponent points: %u\n", *puntosRival);
	printf("Your opponent cards: ");
	printFancyDeck(&suDeck);
	printf("\n");
	printf("Waiting for your opponent to play...\n");
}

int main(int argc, char *argv[])
{

	int socketfd;					   /** Socket descriptor */
	unsigned int port;				   /** Port number (server) */
	struct sockaddr_in server_address; /** Server address structure */
	char *serverIP;					   /** Server IP */
	unsigned int endOfGame;			   /** Flag to control the end of the game */
	tString playerName, message;	   /** Name of the player */
	unsigned int code;				   /** Code */
	unsigned int stack;
	unsigned int bet;
	unsigned int puntos, puntosRival;
	tDeck miDeck;
	// Check arguments!
	if (argc != 3)
	{
		fprintf(stderr, "ERROR wrong number of arguments\n");
		fprintf(stderr, "Usage:\n$>%s serverIP port\n", argv[0]);
		exit(0);
	}

	// Get the server address
	serverIP = argv[1];

	// Get the port
	port = atoi(argv[2]);

	// Create socket
	socketfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// Check if the socket has been successfully created
	if (socketfd < 0)
		showError("ERROR while creating the socket");

	// Fill server address structure
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = inet_addr(serverIP);
	server_address.sin_port = htons(port);

	// Connect with server
	if (connect(socketfd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
		showError("ERROR while establishing connection");

	// Init and read the message
	printf("Introduce your username: ");
	fgets(playerName, STRING_LENGTH - 1, stdin);
	playerName[strcspn(playerName, "\n")] = 0;

	// Send message to the server side
	sendStringMessage(socketfd, playerName);

	// Init for reading incoming message
	recvStringMessage(socketfd, message);
	printf("%s", message);
	recvStringMessage(socketfd, message);
	printf("%s!\n", message);

	recvStringMessage(socketfd, message);

	// Show the returned message
	printf("%s\n", message);

	endOfGame = 0;
	while (endOfGame == 0)
	{
		code = 0;
		recv(socketfd, &code, sizeof(code), 0);
		if (DEBUG_CLIENT)
			showCode(code);

		switch (code)
		{
		case TURN_BET:
			stack = 0;
			recv(socketfd, &stack, sizeof(stack), 0);
			printf("Your stack is: %d\n", stack);
			printf("Your turn to bet:\n");
			bet = readBet();
			send(socketfd, &bet, sizeof(bet), 0);
			break;
		case TURN_PLAY:
			printf("Your turn to play\n");
			recv(socketfd, &puntos, sizeof(puntos), 0);
			miDeck = receiveDeck(socketfd);
			printf("Your points: %d\n", puntos);
			printf("Your cards: ");
			printFancyDeck(&miDeck);
			printf("\n");

			unsigned int option = readOption();
			send(socketfd, &option, sizeof(option), 0);
			break;
		case TURN_PLAY_WAIT:
			opponentTurn(socketfd, &puntosRival);
			break;
		case TURN_PLAY_RIVAL_DONE:
			printf("Your opponent has finished their turn.\n");
			printf("\n");
			break;
		case TURN_PLAY_OUT:
			recv(socketfd, &puntos, sizeof(puntos), 0);
			miDeck = receiveDeck(socketfd);
			printf("Your points: %d\n", puntos);
			printf("Your cards: ");
			printFancyDeck(&miDeck);
			printf("\n");
			printf("Your turn is over.\n");
			printf("\n");

			break;
		case TURN_GAME_WIN:
			printf("You won the game!\n");
			printf("\n");

			endOfGame = 1;
			break;
		case TURN_GAME_LOSE:
			printf("You lost the game!\n");
			printf("\n");

			endOfGame = 1;
			break;
		default:
			printf("Unkwon code: %d\n", code);
			break;
		}
	}
	close(socketfd);

	return 0;
}