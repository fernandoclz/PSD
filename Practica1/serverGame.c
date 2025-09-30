#include "serverGame.h"
#include <pthread.h>

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
	pthread_t threadID;				   /** Thread ID */
	int messageLength;
	int length = 0;
	unsigned int code;
	unsigned int stack;
	unsigned int bet;
	// Seed
	srand(time(0));

	// Check arguments
	if (argc != 2)
	{
		fprintf(stderr, "ERROR wrong number of arguments\n");
		fprintf(stderr, "Usage:\n$>%s port\n", argv[0]);
		exit(1);
	}

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

	// Listen fuera del bucle
	// While true, accept

	tSession partida;
	tPlayerData player;
	initSession(&partida);

	// Get length of client structure
	clientLength = sizeof(player1Address);

	// Accept!
	socketPlayer1 = accept(socketfd, (struct sockaddr *)&player1Address, &clientLength);
	player[player1].socket = socketPlayer1;
	// Check accept result
	if (socketPlayer1 < 0)
		showError("ERROR while accepting");

	// Init and read message
	memset(partida.player1Name, 0, STRING_LENGTH);
	messageLength = recv(socketPlayer1, &length, sizeof(int), 0);
	messageLength = recv(socketPlayer1, partida.player1Name, length, 0);
	length = 0;

	// Check read bytes
	if (messageLength < 0)
		showError("ERROR while reading from socket");

	// Show message
	printf("Player 1: %s\n", partida.player1Name);

	sendStringMessage(socketPlayer1, "Welcome %s!\n", partida.player1Name);
	sendStringMessage(socketPlayer1, "Waiting for the second player...\n");

	// Get length of client structure
	clientLength = sizeof(player2Address);

	socketPlayer2 = accept(socketfd, (struct sockaddr *)&player2Address, &clientLength);
	player[player2].socket = socketPlayer2;

	if (socketPlayer2 < 0)
		showError("ERROR while accepting");

	// Init and read message
	memset(partida.player2Name, 0, STRING_LENGTH);
	messageLength = recv(socketPlayer2, &length, sizeof(int), 0);
	messageLength = recv(socketPlayer2, partida.player2Name, length, 0);
	length = 0;
	// Check read bytes
	if (messageLength < 0)
		showError("ERROR while reading from socket");

	// Show message
	printf("Player 2: %s\n", partida.player2Name);

	sendStringMessage(socketPlayer2, "Welcome %s!\n", partida.player2Name);
	sendStringMessage(socketPlayer2, "Waiting for player1!\n");

	// Check bytes sent
	if (messageLength < 0)
		showError("ERROR while writing to socket");

	if (DEBUG_PRINT_GAMEDECK)
		printSession(&partida);
	// player1 4.b
	code = TURN_BET;
	while (code == TURN_BET)
	{
		messageLength = send(socketPlayer1, &code, sizeof(code), 0);
		stack = partida.player1Stack;
		messageLength = send(socketPlayer1, &stack, sizeof(stack), 0);
		messageLength = recv(socketPlayer1, &bet, sizeof(bet), 0);
		partida.player1Bet = bet;
		code = 0;
		if (bet > partida.player1Stack)
			code = TURN_BET;
		else
			code = TURN_BET_OK;
	}
	if (DEBUG_PRINT_GAMEDECK)
		printSession(&partida);
	// player2 4.b
	code = TURN_BET;
	while (code == TURN_BET)
	{
		messageLength = send(socketPlayer2, &code, sizeof(code), 0);
		stack = partida.player2Stack;
		messageLength = send(socketPlayer2, &stack, sizeof(stack), 0);
		messageLength = recv(socketPlayer2, &bet, sizeof(bet), 0);
		partida.player2Bet = bet;
		code = 0;
		if (bet > partida.player2Stack)
			code = TURN_BET;
		else
			code = TURN_BET_OK;
	}

	if (DEBUG_PRINT_GAMEDECK)
		printSession(&partida);

	while (1)
		;

	close(socketPlayer1);
	close(socketPlayer2);
	close(socketfd);

	return 0;
}
