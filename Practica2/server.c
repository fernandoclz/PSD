#include "server.h"

/** Shared array that contains all the games. */
tGame games[MAX_GAMES];
int gameIte = 0;

void initGame(tGame *game)
{

	// Init players' name
	memset(game->player1Name, 0, STRING_LENGTH);
	memset(game->player2Name, 0, STRING_LENGTH);

	// Alloc memory for the decks
	clearDeck(&(game->player1Deck));
	clearDeck(&(game->player2Deck));
	initDeck(&(game->gameDeck));

	// Bet and stack
	game->player1Bet = DEFAULT_BET;
	game->player2Bet = DEFAULT_BET;
	game->player1Stack = INITIAL_STACK;
	game->player2Stack = INITIAL_STACK;

	// Game status variables
	game->player1Stand = FALSE;
	game->player2Stand = FALSE;
	game->endOfGame = FALSE;
	game->status = gameEmpty;

	game->stacksUpdated = 0;
}

void resetGame(tGame *game)
{
	// Alloc memory for the decks
	clearDeck(&(game->player1Deck));
	clearDeck(&(game->player2Deck));
	initDeck(&(game->gameDeck));

	// Bet and stack
	game->player1Bet = DEFAULT_BET;
	game->player2Bet = DEFAULT_BET;
	game->player1Stack = INITIAL_STACK;
	game->player2Stack = INITIAL_STACK;

	// Game status variables
	game->player1Stand = FALSE;
	game->player2Stand = FALSE;
	game->endOfGame = FALSE;
	game->status = gameEmpty;

	game->stacksUpdated = 0;
}

void resetRound(tGame *game)
{
	clearDeck(&game->player1Deck);
	clearDeck(&game->player2Deck);
	initDeck(&game->gameDeck);
	game->player1Stand = FALSE;
	game->player2Stand = FALSE;
	game->endOfGame = FALSE;
	game->stacksUpdated = 0;
}

void initServerStructures(struct soap *soap)
{

	if (DEBUG_SERVER)
		printf("Initializing structures...\n");

	// Init seed
	srand(time(NULL));

	// Init each game (alloc memory and init)
	for (int i = 0; i < MAX_GAMES; i++)
	{
		games[i].player1Name = (xsd__string)soap_malloc(soap, STRING_LENGTH);
		games[i].player2Name = (xsd__string)soap_malloc(soap, STRING_LENGTH);
		allocDeck(soap, &(games[i].player1Deck));
		allocDeck(soap, &(games[i].player2Deck));
		allocDeck(soap, &(games[i].gameDeck));
		pthread_mutex_init(&games[i].mutex_status, NULL);
		pthread_mutex_init(&games[i].mutex_game, NULL);
		pthread_cond_init(&games[i].cond_player_turn, NULL);
		pthread_cond_init(&games[i].hasta_que_llegue_el_segundo, NULL);
		initGame(&(games[i]));
	}
}

void initDeck(blackJackns__tDeck *deck)
{

	deck->__size = DECK_SIZE;

	for (int i = 0; i < DECK_SIZE; i++)
		deck->cards[i] = i;
}

void clearDeck(blackJackns__tDeck *deck)
{

	// Set number of cards
	deck->__size = 0;
	for (int i = 0; i < DECK_SIZE; i++)
		deck->cards[i] = UNSET_CARD;
}

tPlayer calculateNextPlayer(tPlayer currentPlayer)
{
	return ((currentPlayer == player1) ? player2 : player1);
}

unsigned int getRandomCard(blackJackns__tDeck *deck)
{

	unsigned int card, cardIndex, i;

	// Get a random card
	cardIndex = rand() % deck->__size;
	card = deck->cards[cardIndex];

	// Remove the gap
	for (i = cardIndex; i < deck->__size - 1; i++)
		deck->cards[i] = deck->cards[i + 1];

	// Update the number of cards in the deck
	deck->__size--;
	deck->cards[deck->__size] = UNSET_CARD;

	return card;
}

unsigned int calculatePoints(blackJackns__tDeck *deck)
{

	unsigned int points = 0;

	for (int i = 0; i < deck->__size; i++)
	{

		if (deck->cards[i] % SUIT_SIZE < 9)
			points += (deck->cards[i] % SUIT_SIZE) + 1;
		else
			points += FIGURE_VALUE;
	}

	return points;
}

void dealInitialCards(tGame *game)
{
	// Deal 2 cards to player 1
	unsigned int card1 = getRandomCard(&game->gameDeck);
	unsigned int card2 = getRandomCard(&game->gameDeck);
	game->player1Deck.cards[game->player1Deck.__size++] = card1;
	game->player1Deck.cards[game->player1Deck.__size++] = card2;

	// Deal 2 cards to player 2
	card1 = getRandomCard(&game->gameDeck);
	card2 = getRandomCard(&game->gameDeck);
	game->player2Deck.cards[game->player2Deck.__size++] = card1;
	game->player2Deck.cards[game->player2Deck.__size++] = card2;
}

void copyGameStatusStructure(blackJackns__tBlock *status, char *message, blackJackns__tDeck *newDeck, int newCode)
{
	// Copy the message
	memset((status->msgStruct).msg, 0, STRING_LENGTH);
	strcpy((status->msgStruct).msg, message);
	(status->msgStruct).__size = strlen((status->msgStruct).msg);

	// Handle deck copying safely
	if (newDeck != NULL && newDeck->__size > 0)
	{
		memcpy((status->deck).cards, newDeck->cards, DECK_SIZE * sizeof(unsigned int));
		(status->deck).__size = newDeck->__size;
	}
	else
	{
		// Don't set cards to NULL - keep the allocated array but set size to 0
		(status->deck).__size = 0;
		// Clear the deck to avoid sending garbage data
		for (int i = 0; i < DECK_SIZE; i++)
		{
			(status->deck).cards[i] = UNSET_CARD;
		}
	}

	// Set the new code
	status->code = newCode;
}

/**
 * Determine game result when game ends
 */
void determineGameResult(tGame *game, int isPlayer1, char *msg, int *resultCode)
{
	unsigned int p1Points = calculatePoints(&game->player1Deck);
	unsigned int p2Points = calculatePoints(&game->player2Deck);

	printf("Determinando resultado\n");
	if (isPlayer1)
	{
		if (p1Points > GOAL_GAME)
		{
			strcpy(msg, "You busted! You lose!");
			*resultCode = GAME_LOSE;
		}
		else if (p2Points > GOAL_GAME)
		{
			strcpy(msg, "Opponent busted! You win!");
			*resultCode = GAME_WIN;
		}
		else if (p1Points > p2Points)
		{
			strcpy(msg, "You have more points! You win!");
			*resultCode = GAME_WIN;
		}
		else if (p2Points > p1Points)
		{
			strcpy(msg, "Opponent has more points! You lose!");
			*resultCode = GAME_LOSE;
		}
		else
		{
			strcpy(msg, "It's a tie!");
			*resultCode = GAME_TIE;
		}
	}
	else // Player 2
	{
		if (p2Points > GOAL_GAME)
		{
			strcpy(msg, "You busted! You lose!");
			*resultCode = GAME_LOSE;
		}
		else if (p1Points > GOAL_GAME)
		{
			strcpy(msg, "Opponent busted! You win!");
			*resultCode = GAME_WIN;
		}
		else if (p2Points > p1Points)
		{
			strcpy(msg, "You have more points! You win!");
			*resultCode = GAME_WIN;
		}
		else if (p1Points > p2Points)
		{
			strcpy(msg, "Opponent has more points! You lose!");
			*resultCode = GAME_LOSE;
		}
		else
		{
			strcpy(msg, "It's a tie!");
			*resultCode = GAME_TIE;
		}
	}
}

void updateStacks(tGame *game, int isPlayer1, int resultCode)
{
	if (resultCode == GAME_WIN)
	{
		if (isPlayer1)
		{
			game->player1Stack += game->player2Bet;
			game->player2Stack = (game->player2Stack < game->player2Bet) ? 0 : game->player2Stack - game->player2Bet;
		}
		else
		{
			game->player2Stack += game->player1Bet;
			game->player1Stack = (game->player1Stack < game->player1Bet) ? 0 : game->player1Stack - game->player1Bet;
		}
	}
	else if (resultCode == GAME_LOSE)
	{
		if (isPlayer1)
		{
			game->player1Stack = (game->player1Stack < game->player1Bet) ? 0 : game->player1Stack - game->player1Bet;
			game->player2Stack += game->player1Bet;
		}
		else
		{
			game->player2Stack = (game->player2Stack < game->player2Bet) ? 0 : game->player2Stack - game->player2Bet;
			game->player1Stack += game->player2Bet;
		}
	}
}

int checkGameEnd(tGame *game)
{
	unsigned int p1Points = calculatePoints(&game->player1Deck);
	unsigned int p2Points = calculatePoints(&game->player2Deck);

	printf("Checking game end\n");
	// Si ambos jugadores se han plantado
	if (game->player1Stand && game->player2Stand)
	{
		game->endOfGame = TRUE;
		return TRUE;
	}

	// Si un jugador se ha pasado de 21
	if (p1Points > GOAL_GAME || p2Points > GOAL_GAME)
	{
		game->endOfGame = TRUE;
		return TRUE;
	}

	return FALSE;
}

/* ===== WEB SERVICE OPERATIONS ===== */

/**
 * Get current game status for a player
 */
int blackJackns__getStatus(struct soap *soap,
						   blackJackns__tMessage playerName,
						   int gameId,
						   blackJackns__tBlock *result)
{
	allocClearBlock(soap, result);
	playerName.msg[playerName.__size] = 0;

	if (gameId < 0 || gameId >= MAX_GAMES)
		return copyGameStatusStructure(result, "Invalid game ID", NULL, ERROR_SERVER_FULL), SOAP_OK;

	tGame *game = &games[gameId];

	pthread_mutex_lock(&game->mutex_status);
	int isPlayer1 = strcmp(playerName.msg, game->player1Name) == 0;
	int isPlayer2 = strcmp(playerName.msg, game->player2Name) == 0;

	if (!isPlayer1 && !isPlayer2)
	{
		pthread_mutex_unlock(&game->mutex_status);
		return copyGameStatusStructure(result, "Player not found", NULL, ERROR_PLAYER_NOT_FOUND), SOAP_OK;
	}

	while (game->status == gameWaitingPlayer)
		pthread_cond_wait(&game->hasta_que_llegue_el_segundo, &game->mutex_status);
	pthread_mutex_unlock(&game->mutex_status);

	pthread_mutex_lock(&game->mutex_game);

	// ===== Fin de partida o ambos plantados =====
	if (game->endOfGame || (game->player1Stand && game->player2Stand))
	{
		int resultCode;
		char msg[STRING_LENGTH];

		if (!game->stacksUpdated)
		{
			game->stacksUpdated = 1;
			determineGameResult(game, isPlayer1, msg, &resultCode);

			// Actualización de stacks
			updateStacks(game, isPlayer1, resultCode);
			printf("Stacks -> P1:%u | P2:%u\n", game->player1Stack, game->player2Stack);

			if (game->player1Stack == 0 || game->player2Stack == 0)
			{
				int finalCode = (isPlayer1 ? (game->player1Stack ? GAME_WIN : GAME_LOSE)
										   : (game->player2Stack ? GAME_WIN : GAME_LOSE));
				char endMsg[STRING_LENGTH];
				snprintf(endMsg, STRING_LENGTH, "Game over! %s",
						 (finalCode == GAME_WIN) ? "You win!" : "You lose!");
				copyGameStatusStructure(result, endMsg, NULL, finalCode);

				game->endOfGame = TRUE;
				game->status = gameEmpty;
			}
			else
			{
				resetRound(game);
				dealInitialCards(game);
				game->status = gameReady;
				game->currentPlayer = (rand() % 2) ? player1 : player2;
				game->stacksUpdated = 0;

				unsigned int myStack = isPlayer1 ? game->player1Stack : game->player2Stack;
				blackJackns__tDeck *myDeck = isPlayer1 ? &game->player1Deck : &game->player2Deck;
				char newRoundMsg[STRING_LENGTH];
				snprintf(newRoundMsg, STRING_LENGTH, "%s New round! Stack: %u", msg, myStack);
				copyGameStatusStructure(result, newRoundMsg, myDeck,
										(game->currentPlayer == (isPlayer1 ? player1 : player2)) ? TURN_PLAY : TURN_WAIT);
			}
		}
		else
		{
			// Si stacks ya actualizados
			if (game->endOfGame)
			{
				int finalCode = (isPlayer1 ? (game->player1Stack ? GAME_WIN : GAME_LOSE)
										   : (game->player2Stack ? GAME_WIN : GAME_LOSE));
				char endMsg[STRING_LENGTH];
				snprintf(endMsg, STRING_LENGTH, "Game over! %s",
						 (finalCode == GAME_WIN) ? "You win!" : "You lose!");
				copyGameStatusStructure(result, endMsg, NULL, finalCode);
			}
			else
			{
				unsigned int myStack = isPlayer1 ? game->player1Stack : game->player2Stack;
				blackJackns__tDeck *myDeck = isPlayer1 ? &game->player1Deck : &game->player2Deck;
				char waitMsg[STRING_LENGTH];
				snprintf(waitMsg, STRING_LENGTH, "Waiting new round. Stack: %u", myStack);
				copyGameStatusStructure(result, waitMsg, myDeck, TURN_WAIT);
			}
		}
	}
	else
	{
		// ===== Turno del jugador =====
		if ((isPlayer1 && game->currentPlayer != player1) ||
			(isPlayer2 && game->currentPlayer != player2))
		{
			pthread_cond_wait(&game->cond_player_turn, &game->mutex_game);
			char waitMsg[STRING_LENGTH];
			snprintf(waitMsg, STRING_LENGTH, "Your opponent's move...");
			blackJackns__tDeck *opDeck = isPlayer1 ? &game->player2Deck : &game->player1Deck;
			copyGameStatusStructure(result, waitMsg, opDeck, TURN_WAIT);
		}
		else
		{
			blackJackns__tDeck *myDeck = isPlayer1 ? &game->player1Deck : &game->player2Deck;
			unsigned int myPoints = calculatePoints(myDeck);
			unsigned int stack = isPlayer1 ? game->player1Stack : game->player2Stack;
			char msg[STRING_LENGTH];
			snprintf(msg, STRING_LENGTH, "Your turn. %u points. Stack %u", myPoints, stack);
			copyGameStatusStructure(result, msg, myDeck, TURN_PLAY);
		}
	}
	pthread_mutex_unlock(&game->mutex_game);
	return SOAP_OK;
}

/**
 * Register a new player in a game
 */
int blackJackns__register(struct soap *soap, blackJackns__tMessage playerName, int *result)
{
	playerName.msg[playerName.__size] = 0;
	if (DEBUG_SERVER)
		printf("[Register] Player: %s\n", playerName.msg);

	int gameIndex = -1;
	for (int i = 0; i < MAX_GAMES && gameIndex == -1; i++)
	{
		pthread_mutex_lock(&games[i].mutex_status);
		tGame *g = &games[i];

		if (g->status == gameEmpty || g->endOfGame)
		{
			resetGame(g);
			strncpy(g->player1Name, playerName.msg, STRING_LENGTH - 1);
			g->status = gameWaitingPlayer;
			gameIndex = i;
		}
		else if (g->status == gameWaitingPlayer)
		{
			if (!strcmp(playerName.msg, g->player1Name))
			{
				pthread_mutex_unlock(&g->mutex_status);
				*result = ERROR_NAME_REPEATED;
				return SOAP_OK;
			}
			strncpy(g->player2Name, playerName.msg, STRING_LENGTH - 1);
			g->status = gameReady;
			gameIndex = i;
			dealInitialCards(g);
			g->currentPlayer = (rand() % 2) ? player1 : player2;
			pthread_cond_broadcast(&g->hasta_que_llegue_el_segundo);
			pthread_cond_broadcast(&g->cond_player_turn);
		}
		pthread_mutex_unlock(&g->mutex_status);
	}

	*result = (gameIndex == -1) ? ERROR_SERVER_FULL : gameIndex;

	if (DEBUG_SERVER)
		printf("[Register] %s -> Game %d\n", playerName.msg, *result);

	return SOAP_OK;
}

/**
 * Process a player's move (hit or stand)
 */
int blackJackns__playerMove(struct soap *soap,
							blackJackns__tMessage playerName,
							int gameId,
							unsigned int move,
							blackJackns__tBlock *result)
{
	allocClearBlock(soap, result);
	playerName.msg[playerName.__size] = 0;

	if (gameId < 0 || gameId >= MAX_GAMES)
		return copyGameStatusStructure(result, "Invalid game ID", NULL, ERROR_SERVER_FULL), SOAP_OK;

	tGame *game = &games[gameId];
	pthread_mutex_lock(&game->mutex_status);
	int isPlayer1 = strcmp(playerName.msg, game->player1Name) == 0;
	int isPlayer2 = strcmp(playerName.msg, game->player2Name) == 0;
	if (!isPlayer1 && !isPlayer2)
		return pthread_mutex_unlock(&game->mutex_status),
			   copyGameStatusStructure(result, "Player not found", NULL, ERROR_PLAYER_NOT_FOUND),
			   SOAP_OK;
	pthread_mutex_unlock(&game->mutex_status);

	pthread_mutex_lock(&game->mutex_game);
	blackJackns__tDeck *myDeck = isPlayer1 ? &game->player1Deck : &game->player2Deck;
	unsigned int myPoints = calculatePoints(myDeck);

	char msg[STRING_LENGTH];
	if (move == PLAYER_HIT_CARD)
	{
		unsigned int card = getRandomCard(&game->gameDeck);
		myDeck->cards[myDeck->__size++] = card;
		myPoints = calculatePoints(myDeck);
		snprintf(msg, STRING_LENGTH, "You drew a card. %u points.", myPoints);
		if (myPoints > GOAL_GAME)
			game->endOfGame = TRUE;
		copyGameStatusStructure(result, msg, myDeck, TURN_PLAY);
	}
	else if (move == PLAYER_STAND)
	{
		if (isPlayer1)
			game->player1Stand = TRUE;
		else
			game->player2Stand = TRUE;
		snprintf(msg, STRING_LENGTH, "You stand with %u points.", myPoints);
		game->currentPlayer = calculateNextPlayer(game->currentPlayer);
		copyGameStatusStructure(result, msg, myDeck, TURN_WAIT);
	}
	else
	{
		copyGameStatusStructure(result, "Invalid move", NULL, TURN_PLAY);
	}

	pthread_cond_broadcast(&game->cond_player_turn);
	pthread_mutex_unlock(&game->mutex_game);
	return SOAP_OK;
}

// Agregar función de limpieza al finalizar el servidor
void cleanupServer()
{
	for (int i = 0; i < MAX_GAMES; i++)
	{
		pthread_mutex_destroy(&games[i].mutex_status);
		pthread_mutex_destroy(&games[i].mutex_game);
		pthread_cond_destroy(&games[i].cond_player_turn);
		pthread_cond_destroy(&games[i].hasta_que_llegue_el_segundo);
	}
}

/* ===== SERVER MANAGEMENT FUNCTIONS ===== */

/**
 * Process incoming SOAP requests in separate thread
 */
void *processRequest(void *soap)
{
	struct soap *soap_ = (struct soap *)soap;
	soap_serve(soap_);
	soap_destroy(soap_);
	soap_end(soap_);
	soap_done(soap_);
	free(soap_);
	return NULL;
}

/**
 * Main server function
 */
int main(int argc, char **argv)
{

	struct soap soap;
	struct soap *tsoap;
	pthread_t tid;
	int port;
	SOAP_SOCKET m, s;

	// Check arguments
	if (argc != 2)
	{
		printf("Usage: %s port\n", argv[0]);
		exit(0);
	}

	soap_init(&soap);

	// Configure timeouts
	soap.send_timeout = 60;		// 60 seconds
	soap.recv_timeout = 60;		// 60 seconds
	soap.accept_timeout = 3600; // server stops after 1 hour of inactivity
	soap.max_keep_alive = 100;	// max keep-alive sequence

	// Get listening port
	port = atoi(argv[1]);

	// Bind
	m = soap_bind(&soap, NULL, port, 100);

	if (!soap_valid_socket(m))
	{
		exit(1);
	}

	printf("Server is ON!\n");
	initServerStructures(&soap);

	while (TRUE)
	{

		// Accept a new connection
		s = soap_accept(&soap);

		// Socket is not valid :(
		if (!soap_valid_socket(s))
		{

			if (soap.errnum)
			{
				soap_print_fault(&soap, stderr);
				exit(1);
			}

			fprintf(stderr, "Time out!\n");
			break;
		}

		// Copy the SOAP environment
		tsoap = soap_copy(&soap);

		if (!tsoap)
		{
			printf("SOAP copy error!\n");
			break;
		}

		// Create a new thread to process the request
		pthread_create(&tid, NULL, (void *(*)(void *))processRequest, (void *)tsoap);
	}

	// Detach SOAP environment
	soap_done(&soap);

	cleanupServer();

	return 0;
}